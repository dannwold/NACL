#ifndef QUICKJS_EVENTFD_BRIDGE_H

#define QUICKJS_EVENTFD_BRIDGE_H



#include <stdint.h>

#include <stdbool.h>

#include <stdatomic.h>

#include "quickjs.h"



#ifdef __cplusplus

extern "C" {

#endif



// Max capacity of our lock-free SPSC queue (Must be a power of 2)

#define EVENT_QUEUE_CAPACITY 256

#define EVENT_QUEUE_MASK (EVENT_QUEUE_CAPACITY - 1)



// Unified Event structure representing hardware telemetry

typedef struct {

    uint16_t subsystem;

    uint16_t event_type;

    uint64_t timestamp_ns;

    float data[4];

} HardwareEvent;



// Single-Producer Single-Consumer Lock-Free Queue

typedef struct {

    HardwareEvent ring[EVENT_QUEUE_CAPACITY];

    _Atomic uint32_t head; // Read pointer index (Main QuickJS thread)

    _Atomic uint32_t tail; // Write pointer index (Background worker thread)

} SpscEventQueue;



// Core Event Loop Bridge Context

typedef struct {

    int event_fd;                 // POSIX eventfd handle

    SpscEventQueue queue;         // Thread-safe circular queue

    JSContext *js_ctx;            // QuickJS context

    JSValue js_callback;          // Persistent JS callback

    bool is_running;              // Lifecycle state tracking flag

} EventfdBridge;



// Initialize the event loop bridge

EventfdBridge* eventfd_bridge_create(JSContext *ctx, JSValue callback);



// Clean up and free bridge resources

void eventfd_bridge_destroy(EventfdBridge *bridge);



// Thread-safe: Post an event from any background worker thread

bool eventfd_bridge_post_event(EventfdBridge *bridge, const HardwareEvent *event);



// Main Thread Loop: Consume pending events and dispatch to QuickJS

void eventfd_bridge_dispatch_pending(EventfdBridge *bridge);



// Local TCP Loopback Socket Helpers (SELinux path bypass)

int start_tcp_loopback_server(int port);

int connect_tcp_loopback_client(int port);



#ifdef __cplusplus

}

#endif



#endif // QUICKJS_EVENTFD_BRIDGE_H