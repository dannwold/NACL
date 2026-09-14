#include "quickjs_eventfd_bridge.h"

#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <sys/eventfd.h>

#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <fcntl.h>

#include <errno.h>



EventfdBridge* eventfd_bridge_create(JSContext *ctx, JSValue callback) {

    EventfdBridge *bridge = (EventfdBridge*)malloc(sizeof(EventfdBridge));

    if (!bridge) return NULL;



    // Open a non-blocking POSIX eventfd descriptor

    bridge->event_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    if (bridge->event_fd == -1) {

        perror("[EventfdBridge] Failed to create eventfd");

        free(bridge);

        return NULL;

    }



    // Initialize atomic indexes

    atomic_init(&bridge->queue.head, 0);

    atomic_init(&bridge->queue.tail, 0);



    bridge->js_ctx = ctx;

    bridge->js_callback = JS_DupValue(ctx, callback);

    bridge->is_running = true;



    return bridge;

}



void eventfd_bridge_destroy(EventfdBridge *bridge) {

    if (!bridge) return;

    bridge->is_running = false;



    if (bridge->event_fd != -1) {

        close(bridge->event_fd);

    }



    JS_FreeValue(bridge->js_ctx, bridge->js_callback);

    free(bridge);

}



// Thread-Safe Push operation (Called by any native background thread)

bool eventfd_bridge_post_event(EventfdBridge *bridge, const HardwareEvent *event) {

    if (!bridge || !bridge->is_running) return false;



    uint32_t current_tail = atomic_load_explicit(&bridge->queue.tail, memory_order_relaxed);

    uint32_t current_head = atomic_load_explicit(&bridge->queue.head, memory_order_acquire);



    // Verify queue backpressure threshold

    if ((current_tail - current_head) >= EVENT_QUEUE_CAPACITY) {

        return false; // Queue full

    }



    // Write structure data to the next empty circular index

    uint32_t idx = current_tail & EVENT_QUEUE_MASK;

    bridge->queue.ring[idx] = *event;



    // Enforce write ordering: data is physically flushed to RAM before tail increments

    atomic_store_explicit(&bridge->queue.tail, current_tail + 1, memory_order_release);



    // Alert the main thread's event loop

    uint64_t wake_val = 1;

    ssize_t bytes_written = write(bridge->event_fd, &wake_val, sizeof(wake_val));

    if (bytes_written == -1 && errno != EAGAIN) {

        perror("[EventfdBridge] Failed writing to eventfd");

        return false;

    }



    return true;

}



// Thread-Safe Callback Consumer (Executed strictly on the Main Interpreter Thread)

void eventfd_bridge_dispatch_pending(EventfdBridge *bridge) {

    if (!bridge || !bridge->is_running) return;



    uint64_t signal_count = 0;

    // Clear and consume accumulative signals on the eventfd descriptor

    ssize_t bytes_read = read(bridge->event_fd, &signal_count, sizeof(signal_count));

    if (bytes_read <= 0) {

        return; // Signal already drained or read would block (EAGAIN)

    }



    // Safely drain the ring buffer on the main thread

    while (true) {

        uint32_t current_head = atomic_load_explicit(&bridge->queue.head, memory_order_relaxed);

        uint32_t current_tail = atomic_load_explicit(&bridge->queue.tail, memory_order_acquire);



        // Queue is completely empty

        if (current_head == current_tail) {

            break;

        }



        uint32_t idx = current_head & EVENT_QUEUE_MASK;

        HardwareEvent ev = bridge->queue.ring[idx];



        // Release queue slot to the producer background thread

        atomic_store_explicit(&bridge->queue.head, current_head + 1, memory_order_release);



        // Convert the C struct into a QuickJS object safely on the main thread

        JSValue js_ev = JS_NewObject(bridge->js_ctx);

        JS_SetPropertyStr(bridge->js_ctx, js_ev, "subsystem", JS_NewInt32(bridge->js_ctx, ev.subsystem));

        JS_SetPropertyStr(bridge->js_ctx, js_ev, "eventType", JS_NewInt32(bridge->js_ctx, ev.event_type));

        JS_SetPropertyStr(bridge->js_ctx, js_ev, "timestamp", JS_NewBigInt64(bridge->js_ctx, ev.timestamp_ns));



        JSValue js_data = JS_NewArray(bridge->js_ctx);

        for (int i = 0; i < 4; i++) {

            JS_SetPropertyUint32(bridge->js_ctx, js_data, i, JS_NewFloat64(bridge->js_ctx, ev.data[i]));

        }

        JS_SetPropertyStr(bridge->js_ctx, js_ev, "data", js_data);



        // Invoke JS callback inside the main thread's interpreter context

        JSValue global_obj = JS_GetGlobalObject(bridge->js_ctx);

        JSValue ret_val = JS_Call(bridge->js_ctx, bridge->js_callback, global_obj, 1, &js_ev);



        // Clean up heap references

        JS_FreeValue(bridge->js_ctx, ret_val);

        JS_FreeValue(bridge->js_ctx, js_ev);

        JS_FreeValue(bridge->js_ctx, global_obj);

    }

}



// Bypasses path-based SELinux domain checks via local IP Loopback binding

int start_tcp_loopback_server(int port) {

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1) return -1;



    int opt = 1;

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));



    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;

    addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Pure loopback

    addr.sin_port = htons(port);



    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {

        close(server_fd);

        return -1;

    }



    if (listen(server_fd, SOMAXCONN) == -1) {

        close(server_fd);

        return -1;

    }



    // Configure as non-blocking

    int flags = fcntl(server_fd, F_GETFL, 0);

    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);



    return server_fd;

}



int connect_tcp_loopback_client(int port) {

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (client_fd == -1) return -1;



    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;

    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    addr.sin_port = htons(port);



    if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {

        close(client_fd);

        return -1;

    }



    return client_fd;

}