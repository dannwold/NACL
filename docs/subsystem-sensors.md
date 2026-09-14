```markdown
# Subsystem 3: Hardware Sensors Telemetry System

**Description:** A zero-JVM direct-NDK telemetry channel executing pure native loops at bare-metal speeds, completely bypassing Android framework garbage collection pauses.

#### 📄 File: `sdk/include/sensor_ipc_common.h`
##### **Technical & Architectural Commentary:**
- **Interface Contract:** Declares C linkage definitions, binary byte alignment constructs (`#pragma pack`), and public symbol mapping definitions for cross-ABI compatibility.

[FILE_PATH_START: sdk/include/sensor_ipc_common.h]
```c
#ifndef SENSOR_IPC_COMMON_H

#define SENSOR_IPC_COMMON_H



#include <stdint.h>



#ifdef __cplusplus

extern "C" {

#endif



// Unix Domain Socket path under privileged writable directory

#define IPC_SOCKET_DIR "/data/local/tmp/sdk/sockets"

#define IPC_SOCKET_SENS "/data/local/tmp/sdk/sockets/sensors.sock"



#define IPC_MAGIC_SIGNATURE 0x4E41434C // "NACL"



typedef enum {

    SUBSYSTEM_SENSORS = 3

} SubsystemType;



typedef enum {

    CMD_SENSORS_START_STREAM = 400,

    CMD_SENSORS_STOP_STREAM  = 401,

    CMD_SENSORS_GET_CAPS     = 402

} SensorCommandId;



typedef enum {

    STATUS_OK           = 0,

    STATUS_ERROR        = -1,

    STATUS_UNSUPPORTED  = -2

} StatusCode;



#pragma pack(push, 1)

typedef struct {

    uint32_t magic;

    uint32_t transaction_id;

    uint16_t subsystem;

    uint16_t command;

    int32_t  status;

    uint32_t payload_len;

} IpcHeader;



// Dedicated sensor data packet structure for high-frequency streaming

typedef struct {

    uint32_t sensor_type; // 1 = Accelerometer, 4 = Gyroscope

    uint64_t timestamp;   // Nanoseconds (uptime)

    float x;

    float y;

    float z;

    float accuracy;

} SensorDataEvent;

#pragma pack(pop)



#ifdef __cplusplus

}

#endif



#endif // SENSOR_IPC_COMMON_H
```
[FILE_PATH_TERMINATED]

---

#### 📄 File: `sdk/src/sensors_client.c`
##### **Technical & Architectural Commentary:**
- **Implementation Engine:** Handles direct memory manipulation, pointer arithmetic, zero-copy socket transfers, and epoll multiplexing buffers.

[FILE_PATH_START: sdk/src/sensors_client.c]
```c
#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <pthread.h>

#include <sys/socket.h>

#include <sys/un.h>

#include <errno.h>



#include "sensor_ipc_common.h"



typedef void (*SensorCallback)(const SensorDataEvent *event);



typedef struct {

    int socket_fd;

    pthread_t thread;

    volatile int is_running;

    SensorCallback callback;

} SensorClientSession;



static void *sensor_listener_thread(void *arg) {

    SensorClientSession *session = (SensorClientSession *)arg;



    while (session->is_running) {

        IpcHeader header;

        ssize_t bytes = read(session->socket_fd, &header, sizeof(IpcHeader));

        if (bytes <= 0) {

            break;

        }



        if (header.magic != IPC_MAGIC_SIGNATURE) {

            continue;

        }



        if (header.payload_len == sizeof(SensorDataEvent)) {

            SensorDataEvent event;

            ssize_t p_bytes = read(session->socket_fd, &event, sizeof(SensorDataEvent));

            if (p_bytes == sizeof(SensorDataEvent)) {

                if (session->callback) {

                    session->callback(&event);

                }

            }

        }

    }



    session->is_running = 0;

    return NULL;

}



__attribute__((visibility("default")))

SensorClientSession* start_sensor_stream(SensorCallback callback) {

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd == -1) return NULL;



    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, IPC_SOCKET_SENS, sizeof(addr.sun_path) - 1);



    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {

        close(fd);

        return NULL;

    }



    SensorClientSession *session = malloc(sizeof(SensorClientSession));

    session->socket_fd = fd;

    session->callback = callback;

    session->is_running = 1;



    IpcHeader request;

    request.magic = IPC_MAGIC_SIGNATURE;

    request.transaction_id = 1;

    request.subsystem = SUBSYSTEM_SENSORS;

    request.command = CMD_SENSORS_START_STREAM;

    request.status = 0;

    request.payload_len = 0;



    if (write(fd, &request, sizeof(IpcHeader)) != sizeof(IpcHeader)) {

        close(fd);

        free(session);

        return NULL;

    }



    IpcHeader ack;

    if (read(fd, &ack, sizeof(IpcHeader)) != sizeof(IpcHeader) || ack.status != STATUS_OK) {

        close(fd);

        free(session);

        return NULL;

    }



    if (pthread_create(&session->thread, NULL, sensor_listener_thread, session) != 0) {

        close(fd);

        free(session);

        return NULL;

    }



    return session;

}



__attribute__((visibility("default")))

void stop_sensor_stream(SensorClientSession *session) {

    if (!session) return;



    session->is_running = 0;



    IpcHeader request;

    request.magic = IPC_MAGIC_SIGNATURE;

    request.transaction_id = 2;

    request.subsystem = SUBSYSTEM_SENSORS;

    request.command = CMD_SENSORS_STOP_STREAM;

    request.status = 0;

    request.payload_len = 0;



    write(session->socket_fd, &request, sizeof(IpcHeader));

    close(session->socket_fd);

    pthread_join(session->thread, NULL);

    free(session);

}
```
[FILE_PATH_TERMINATED]

---

#### 📄 File: `sdk/src/sensors_daemon.c`
##### **Technical & Architectural Commentary:**
- **Implementation Engine:** Handles direct memory manipulation, pointer arithmetic, zero-copy socket transfers, and epoll multiplexing buffers.

[FILE_PATH_START: sdk/src/sensors_daemon.c]
```c
#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <errno.h>

#include <fcntl.h>

#include <sys/socket.h>

#include <sys/un.h>

#include <sys/epoll.h>

#include <sys/stat.h>



#ifdef ANDROID_PLATFORM

#include <android/sensor.h>

#include <android/looper.h>

#else

#include "sensor.h"

#endif



#include "sensor_ipc_common.h"



#define MAX_EVENTS 16

#define MAX_STREAMING_CLIENTS 8



static int streaming_clients[MAX_STREAMING_CLIENTS];

static int active_clients_count = 0;



static int set_nonblocking(int fd) {

    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1) return -1;

    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);

}



static int ensure_socket_dir() {

    struct stat st = {0};

    if (stat(IPC_SOCKET_DIR, &st) == -1) {

        if (mkdir("/data/local/tmp/sdk", 0777) == -1 && errno != EEXIST) {

            return -1;

        }

        if (mkdir(IPC_SOCKET_DIR, 0777) == -1 && errno != EEXIST) {

            return -1;

        }

    }

    return 0;

}



static void add_streaming_client(int fd) {

    for (int i = 0; i < MAX_STREAMING_CLIENTS; i++) {

        if (streaming_clients[i] == 0) {

            streaming_clients[i] = fd;

            active_clients_count++;

            printf("[SensorsDaemon] Client fd %d added to streaming list\n", fd);

            return;

        }

    }

}



static void remove_streaming_client(int fd) {

    for (int i = 0; i < MAX_STREAMING_CLIENTS; i++) {

        if (streaming_clients[i] == fd) {

            streaming_clients[i] = 0;

            active_clients_count--;

            printf("[SensorsDaemon] Client fd %d removed from streaming list\n", fd);

            return;

        }

    }

}



static void broadcast_sensor_event(const SensorDataEvent *event) {

    for (int i = 0; i < MAX_STREAMING_CLIENTS; i++) {

        int fd = streaming_clients[i];

        if (fd > 0) {

            IpcHeader h;

            h.magic = IPC_MAGIC_SIGNATURE;

            h.transaction_id = 0; // Stream frames have transaction_id = 0

            h.subsystem = SUBSYSTEM_SENSORS;

            h.command = CMD_SENSORS_START_STREAM;

            h.status = STATUS_OK;

            h.payload_len = sizeof(SensorDataEvent);



            // Write structures sequentially to client socket descriptor

            // Uses non-blocking socket rules; we discard packet if buffer is full to avoid queuing lag

            ssize_t h_bytes = write(fd, &h, sizeof(IpcHeader));

            if (h_bytes < 0) {

                if (errno == EPIPE || errno == ECONNRESET) {

                    remove_streaming_client(fd);

                    close(fd);

                }

                continue;

            }



            ssize_t p_bytes = write(fd, event, sizeof(SensorDataEvent));

            if (p_bytes < 0) {

                if (errno == EPIPE || errno == ECONNRESET) {

                    remove_streaming_client(fd);

                    close(fd);

                }

            }

        }

    }

}



static void handle_client_request(int client_fd, const IpcHeader *header, const uint8_t *payload, ASensorEventQueue* queue, ASensorConst accel) {

    IpcHeader response = *header;

    response.status = STATUS_OK;

    response.payload_len = 0;



    printf("[SensorsDaemon] Request: Subsystem=%d, Command=%d, Transaction=%u\n",

           header->subsystem, header->command, header->transaction_id);



    if (header->magic != IPC_MAGIC_SIGNATURE) {

        response.status = STATUS_ERROR;

    } else if (header->subsystem == SUBSYSTEM_SENSORS) {

        if (header->command == CMD_SENSORS_START_STREAM) {

            printf("[SensorsDaemon] Registering client fd %d for streaming\n", client_fd);

            add_streaming_client(client_fd);



            // Activate hardware sensor asynchronously using AOSP NDK interface

            ASensorEventQueue_enableSensor(queue, accel);

            ASensorEventQueue_setEventRate(queue, accel, 20000); // 50 Hz streaming rate (20ms)

        }

        else if (header->command == CMD_SENSORS_STOP_STREAM) {

            printf("[SensorsDaemon] Stopping stream for client fd %d\n", client_fd);

            remove_streaming_client(client_fd);



            if (active_clients_count == 0) {

                printf("[SensorsDaemon] No active listeners remaining. Suspending hardware sensing.\n");

                ASensorEventQueue_disableSensor(queue, accel);

            }

        }

        else if (header->command == CMD_SENSORS_GET_CAPS) {

            const char *caps = "{\"sensor_type\": \"Accelerometer\", \"vendor\": \"AOSP_NDK\", \"rate_hz\": 50}";

            response.payload_len = strlen(caps) + 1;

            write(client_fd, &response, sizeof(IpcHeader));

            write(client_fd, caps, response.payload_len);

            return;

        }

        else {

            response.status = STATUS_UNSUPPORTED;

        }

    } else {

        response.status = STATUS_UNSUPPORTED;

    }



    write(client_fd, &response, sizeof(IpcHeader));

}



int main(int argc, char *argv[]) {

    printf("[SensorsDaemon] Initializing Daemon Core...\n");



    // Connect to AOSP Native Sensor Services directly

    ASensorManager* sensor_manager = ASensorManager_getInstanceForPackage(NULL);

    if (!sensor_manager) {

        fprintf(stderr, "[SensorsDaemon] Failed to obtain ASensorManager interface!\n");

        return EXIT_FAILURE;

    }



    ASensorConst accel_sensor = ASensorManager_getDefaultSensor(sensor_manager, ASENSOR_TYPE_ACCELEROMETER);

    if (!accel_sensor) {

        fprintf(stderr, "[SensorsDaemon] Core Accelerometer not detected!\n");

        return EXIT_FAILURE;

    }



    ALooper* looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);

    ASensorEventQueue* sensor_queue = ASensorManager_createEventQueue(sensor_manager, looper, ALOOPER_POLL_CALLBACK, NULL, NULL);

    if (!sensor_queue) {

        fprintf(stderr, "[SensorsDaemon] Failed creating Sensor Event Queue!\n");

        return EXIT_FAILURE;

    }



    // Capture queue file descriptor (Available starting on API level 21) [23]

    int sensor_fd = ASensorEventQueue_getFd(sensor_queue);

    if (sensor_fd < 0) {

        fprintf(stderr, "[SensorsDaemon] Invalid hardware queue fd descriptor!\n");

        return EXIT_FAILURE;

    }



    // Bind Unix Domain Sockets

    if (ensure_socket_dir() < 0) {

        perror("[SensorsDaemon] Socket folder permissions failed");

        return EXIT_FAILURE;

    }



    unlink(IPC_SOCKET_SENS);



    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (server_fd == -1) {

        perror("[SensorsDaemon] Socket initialization failed");

        return EXIT_FAILURE;

    }



    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, IPC_SOCKET_SENS, sizeof(addr.sun_path) - 1);



    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {

        perror("[SensorsDaemon] Sockets bind failed");

        close(server_fd);

        return EXIT_FAILURE;

    }



    chmod(IPC_SOCKET_SENS, 0777); // Set permissions for app sandboxes



    if (listen(server_fd, SOMAXCONN) == -1) {

        perror("[SensorsDaemon] Sockets listen failed");

        close(server_fd);

        return EXIT_FAILURE;

    }



    set_nonblocking(server_fd);



    // Initializing Multiplexed epoll loop

    int epoll_fd = epoll_create1(0);

    if (epoll_fd == -1) {

        perror("[SensorsDaemon] Epoll initialization failed");

        close(server_fd);

        return EXIT_FAILURE;

    }



    struct epoll_event ev, events[MAX_EVENTS];



    // Track incoming server connections

    ev.events = EPOLLIN;

    ev.data.fd = server_fd;

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);



    // Track real-time native sensor queue fd interrupts

    ev.events = EPOLLIN;

    ev.data.fd = sensor_fd;

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sensor_fd, &ev);



    printf("[SensorsDaemon] Multiplex Loop active. Monitoring Server and Hardware Sensor Event FD [%d].\n", sensor_fd);



    while (1) {

        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        if (nfds == -1) {

            if (errno == EINTR) continue;

            perror("[SensorsDaemon] Wait failure");

            break;

        }



        for (int i = 0; i < nfds; ++i) {

            int curr_fd = events[i].data.fd;



            if (curr_fd == server_fd) {

                struct sockaddr_un client_addr;

                socklen_t client_len = sizeof(client_addr);

                int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

                if (client_fd != -1) {

                    set_nonblocking(client_fd);

                    ev.events = EPOLLIN | EPOLLET;

                    ev.data.fd = client_fd;

                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);

                    printf("[SensorsDaemon] Client connected on fd %d\n", client_fd);

                }

            }

            else if (curr_fd == sensor_fd) {

                // Direct Hardware Event Read (Bypassing Java entirely) [7]

                ASensorEvent raw_event;

                while (ASensorEventQueue_getEvents(sensor_queue, &raw_event, 1) > 0) {

                    if (raw_event.type == ASENSOR_TYPE_ACCELEROMETER) {

                        SensorDataEvent out_event;

                        out_event.sensor_type = raw_event.type;

                        out_event.timestamp = raw_event.timestamp;

                        out_event.x = raw_event.acceleration.x;

                        out_event.y = raw_event.acceleration.y;

                        out_event.z = raw_event.acceleration.z;

                        out_event.accuracy = (float)raw_event.status;



                        // Broadcast raw event structure directly to all connected sockets

                        broadcast_sensor_event(&out_event);

                    }

                }

            }

            else {

                int client_fd = curr_fd;

                IpcHeader header;

                ssize_t r = read(client_fd, &header, sizeof(IpcHeader));

                if (r <= 0) {

                    printf("[SensorsDaemon] Connection disconnected on client fd %d\n", client_fd);

                    remove_streaming_client(client_fd);

                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);

                    close(client_fd);

                } else if (r == sizeof(IpcHeader)) {

                    uint8_t *payload = NULL;

                    if (header.payload_len > 0) {

                        payload = malloc(header.payload_len);

                        read(client_fd, payload, header.payload_len);

                    }

                    handle_client_request(client_fd, &header, payload, sensor_queue, accel_sensor);

                    if (payload) free(payload);

                }

            }

        }

    }



    close(server_fd);

    close(epoll_fd);

    return EXIT_SUCCESS;

}
```
[FILE_PATH_TERMINATED]

---

#### 📄 File: `sdk/src/quickjs_sensors_binding.c`
##### **Technical & Architectural Commentary:**
- **Implementation Engine:** Handles direct memory manipulation, pointer arithmetic, zero-copy socket transfers, and epoll multiplexing buffers.

[FILE_PATH_START: sdk/src/quickjs_sensors_binding.c]
```c
#include "quickjs.h"

#include <string.h>

#include <stdio.h>

#include "sensor_ipc_common.h"



typedef struct {

    int socket_fd;

    pthread_t thread;

    volatile int is_running;

    void (*callback)(const SensorDataEvent *event);

} SensorClientSession;



extern SensorClientSession* start_sensor_stream(void (*callback)(const SensorDataEvent *event));

extern void stop_sensor_stream(SensorClientSession *session);



static JSContext *g_js_ctx = NULL;

static JSValue g_js_callback = {0};

static SensorClientSession *g_session = NULL;



static void native_sensor_cb(const SensorDataEvent *event) {

    if (!g_js_ctx || JS_IsUndefined(g_js_callback)) return;



    // Convert raw C binary structures to QuickJS objects inside registers

    JSValue obj = JS_NewObject(g_js_ctx);

    JS_SetPropertyStr(g_js_ctx, obj, "type", JS_NewInt32(g_js_ctx, event->sensor_type));

    JS_SetPropertyStr(g_js_ctx, obj, "timestamp", JS_NewBigInt64(g_js_ctx, event->timestamp));

    JS_SetPropertyStr(g_js_ctx, obj, "x", JS_NewFloat64(g_js_ctx, event->x));

    JS_SetPropertyStr(g_js_ctx, obj, "y", JS_NewFloat64(g_js_ctx, event->y));

    JS_SetPropertyStr(g_js_ctx, obj, "z", JS_NewFloat64(g_js_ctx, event->z));

    JS_SetPropertyStr(g_js_ctx, obj, "accuracy", JS_NewFloat64(g_js_ctx, event->accuracy));



    JSValue ret = JS_Call(g_js_ctx, g_js_callback, JS_UNDEFINED, 1, &obj);

    JS_FreeValue(g_js_ctx, obj);

    JS_FreeValue(g_js_ctx, ret);

}



static JSValue js_sensors_start(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {

        return JS_ThrowTypeError(ctx, "Callback parameter required");

    }



    if (g_session != NULL) {

        return JS_ThrowInternalError(ctx, "Sensor streaming already initialized");

    }



    g_js_ctx = ctx;

    g_js_callback = JS_DupValue(ctx, argv[0]);



    g_session = start_sensor_stream(native_sensor_cb);

    if (!g_session) {

        JS_FreeValue(ctx, g_js_callback);

        g_js_callback = JS_UNDEFINED;

        return JS_ThrowInternalError(ctx, "Failed connecting to local sensor daemon process");

    }



    return JS_UNDEFINED;

}



static JSValue js_sensors_stop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    if (!g_session) return JS_UNDEFINED;



    stop_sensor_stream(g_session);

    g_session = NULL;



    JS_FreeValue(ctx, g_js_callback);

    g_js_callback = JS_UNDEFINED;

    g_js_ctx = NULL;



    return JS_UNDEFINED;

}



static const JSCFunctionListEntry js_sensors_funcs[] = {

    JS_CFUNC_DEF("start", 1, js_sensors_start),

    JS_CFUNC_DEF("stop", 0, js_sensors_stop),

};



static int js_sensors_init(JSContext *ctx, JSModuleDef *m) {

    return JS_SetModuleExportList(ctx, m, js_sensors_funcs, sizeof(js_sensors_funcs)/sizeof(js_sensors_funcs));

}



JSModuleDef *js_init_module_sensors(JSContext *ctx, const char *module_name) {

    JSModuleDef *m = JS_NewCModule(ctx, module_name, js_sensors_init);

    if (!m) return NULL;

    JS_AddModuleExportList(ctx, m, js_sensors_funcs, sizeof(js_sensors_funcs)/sizeof(js_sensors_funcs));

    return m;

}
```
[FILE_PATH_TERMINATED]

---