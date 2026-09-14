```markdown
# Subsystem 4: Low-Latency Shared Memory & Daemon Orchestration

**Description:** A lock-free shared memory segment and ring buffer subsystem that operates with zero-copy IPC, utilizing background service-daemon orchestration schemas.

#### 📄 File: `sdk/include/shm_common.h`
##### **Technical & Architectural Commentary:**
- **Interface Contract:** Declares C linkage definitions, binary byte alignment constructs (`#pragma pack`), and public symbol mapping definitions for cross-ABI compatibility.

[FILE_PATH_START: sdk/include/shm_common.h]
```c
#ifndef NATIVE_SHM_COMMON_H

#define NATIVE_SHM_COMMON_H



#include <stdint.h>

#include <stdatomic.h>



#ifdef __cplusplus

extern "C" {

#endif



#define SHM_SOCKET_PATH "/data/local/tmp/sdk/sockets/shm_broker.sock"

#define SHM_REGION_NAME "nacl_shared_telemetry"

#define SHM_REGION_SIZE 4096 // 4KB aligned page size



// Telemetry payload structure representing real-time hardware states

typedef struct {

    uint64_t timestamp_ns;    // Nanosecond monotonic timestamp

    float accelerometer[3];   // High-frequency X, Y, Z sensor values

    float gyroscope[3];       // Gyroscope X, Y, Z coordinates

    uint32_t wifi_signal_rssi;// Wi-Fi RSSI level (0 to -100 dBm)

    uint32_t bt_device_count; // Number of discovered BLE peripherals

} TelemetryData;



// Shared memory control header layout

typedef struct {

    atomic_uint seq_number;   // Monotonically increasing sequence number (atomic)

    atomic_bool is_writing;   // Lock-free write status flag

    TelemetryData data;       // Actual telemetry data

} SharedStateBuffer;



#ifdef __cplusplus

}

#endif



#endif // NATIVE_SHM_COMMON_H
```
[FILE_PATH_TERMINATED]

---

#### 📄 File: `sdk/include/shm_ring_buffer.h`
##### **Technical & Architectural Commentary:**
- **Interface Contract:** Declares C linkage definitions, binary byte alignment constructs (`#pragma pack`), and public symbol mapping definitions for cross-ABI compatibility.

[FILE_PATH_START: sdk/include/shm_ring_buffer.h]
```c
#ifndef SHM_RING_BUFFER_H

#define SHM_RING_BUFFER_H



#include <stdint.h>

#include <stdbool.h>

#include <stdatomic.h>



#define BLE_SHM_BUFFER_SIZE (1024 * 64) // 64KB Ring Buffer

#define SHM_NAME_BLE "nacl_ble_shm_buffer"



#pragma pack(push, 1)



// Individual data packet inside the shared memory segment

typedef struct {

    uint64_t timestamp_ns;  // Monotonic system timestamp

    uint16_t packet_len;    // Length of raw payload data

    uint8_t  status;        // Frame status flags

    uint8_t  data[512];     // Raw BLE payload buffer

} BleShmPacket;



// SPSC Circular Ring Buffer structure mapped into shared RAM (Hardened)

typedef struct {

    atomic_uint head;             // Read pointer (modified by Client)

    atomic_uint tail;             // Write pointer (modified by Daemon)

    uint32_t    capacity;         // Total packet slots in ring

    uint32_t    slot_size;        // Size of each individual BleShmPacket

    atomic_bool is_active;        // Heartbeat check for daemon status

    atomic_uint pre_generation;   // TOCTOU mitigation: Incremented before daemon writes

    atomic_uint post_generation;  // TOCTOU mitigation: Incremented after daemon writes

    BleShmPacket slots[128];       // Circular queue slots

} BleShmRingBuffer;



#pragma pack(pop)



#endif // SHM_RING_BUFFER_H
```
[FILE_PATH_TERMINATED]

---

#### 📄 File: `sdk/src/shm_client.c`
##### **Technical & Architectural Commentary:**
- **Implementation Engine:** Handles direct memory manipulation, pointer arithmetic, zero-copy socket transfers, and epoll multiplexing buffers.

[FILE_PATH_START: sdk/src/shm_client.c]
```c
#define _GNU_SOURCE

#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <fcntl.h>

#include <sys/mman.h>

#include <sys/socket.h>

#include <sys/un.h>

#include <errno.h>

#include "shm_common.h"



// Receives a file descriptor via Unix Domain Sockets (SCM_RIGHTS control message)

static int receive_fd(int socket_fd) {

    struct msghdr msg = {0};

    struct iovec iov[1];

    char dummy_byte;



    iov[0].iov_base = &dummy_byte;

    iov[0].iov_len = 1;

    msg.msg_iov = iov;

    msg.msg_iovlen = 1;



    // Allocate auxiliary buffer for file descriptors

    union {

        char buf[CMSG_SPACE(sizeof(int))];

        struct cmsghdr align;

    } ctrl_un;



    msg.msg_control = ctrl_un.buf;

    msg.msg_controllen = sizeof(ctrl_un.buf);



    ssize_t bytes_received = recvmsg(socket_fd, &msg, 0);

    if (bytes_received < 0) {

        perror("[Client] recvmsg failed");

        return -1;

    }



    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);

    if (cmsg == NULL || cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {

        fprintf(stderr, "[Client] Protocol error: Expected file descriptor control block.\n");

        return -1;

    }



    int *fd_ptr = (int *)CMSG_DATA(cmsg);

    return *fd_ptr;

}



int main() {

    printf("[Client] Connecting to Shared Memory Broker: %s\n", SHM_SOCKET_PATH);



    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sock_fd == -1) {

        perror("[Client] Socket creation failed");

        return EXIT_FAILURE;

    }



    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, SHM_SOCKET_PATH, sizeof(addr.sun_path) - 1);



    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {

        perror("[Client] Connect failed (Is shm_daemon running?)");

        close(sock_fd);

        return EXIT_FAILURE;

    }



    // Capture the shared memory file descriptor via IPC

    int shm_fd = receive_fd(sock_fd);

    close(sock_fd); // The socket connection is no longer needed after fd passing



    if (shm_fd < 0) {

        fprintf(stderr, "[Client] Failed to acquire shared memory file descriptor.\n");

        return EXIT_FAILURE;

    }



    printf("[Client] Successfully acquired Shared Memory FD: %d\n", shm_fd);



    // Map the shared memory block directly into client space (Read-Only to enforce client boundaries)

    SharedStateBuffer *state = (SharedStateBuffer *)mmap(

        NULL, SHM_REGION_SIZE, PROT_READ, MAP_SHARED, shm_fd, 0

    );

    if (state == MAP_FAILED) {

        perror("[Client] mmap failed");

        close(shm_fd);

        return EXIT_FAILURE;

    }



    printf("[Client] Memory mapping successful. Monitoring real-time hardware telemetry...\n");



    uint32_t last_seq = 0xFFFFFFFF;

    int polls = 10; // Read 10 sequential samples



    while (polls > 0) {

        uint32_t current_seq = atomic_load(&state->seq_number);



        // Only parse if a new sequence update has completed

        if (current_seq != last_seq) {

            // Check lock-free write status

            if (!atomic_load(&state->is_writing)) {

                printf("[Client] [Seq %u] Telemetry Received:\n", current_seq);

                printf("  -> Monotonic Time: %llu ns\n", (unsigned long long)state->data.timestamp_ns);

                printf("  -> Accelerometer : X=%.3f, Y=%.3f, Z=%.3f m/s²\n",

                       state->data.accelerometer[0], state->data.accelerometer[1], state->data.accelerometer[2]);

                printf("  -> Gyroscope     : X=%.4f, Y=%.4f, Z=%.4f rad/s\n",

                       state->data.gyroscope[0], state->data.gyroscope[1], state->data.gyroscope[2]);

                printf("  -> Wi-Fi RSSI    : -%u dBm\n", state->data.wifi_signal_rssi);

                printf("  -> BLE Devices   : %u\n", state->data.bt_device_count);



                last_seq = current_seq;

                polls--;

            }

        }



        usleep(20000); // Poll every 20ms (50Hz client sync frequency)

    }



    // Cleanup mapped region

    munmap(state, SHM_REGION_SIZE);

    close(shm_fd);

    printf("[Client] Detached cleanly from shared telemetry segment.\n");

    return EXIT_SUCCESS;

}
```
[FILE_PATH_TERMINATED]

---

#### 📄 File: `sdk/src/shm_daemon.c`
##### **Technical & Architectural Commentary:**
- **Implementation Engine:** Handles direct memory manipulation, pointer arithmetic, zero-copy socket transfers, and epoll multiplexing buffers.

[FILE_PATH_START: sdk/src/shm_daemon.c]
```c
#define _GNU_SOURCE

#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <fcntl.h>

#include <sys/mman.h>

#include <sys/socket.h>

#include <sys/un.h>

#include <sys/ioctl.h>

#include <errno.h>

#include <time.h>

#include "shm_common.h"



// Legacy Android Ashmem ioctls (for fallback compatibility)

#define ASHMEM_NAME_LEN         256

#define __ASHMEMIOC             0x77

#define ASHMEM_SET_NAME         _IOW(__ASHMEMIOC, 1, char[ASHMEM_NAME_LEN])

#define ASHMEM_SET_SIZE         _IOW(__ASHMEMIOC, 3, size_t)



// Attempts to create shared memory via modern Linux memfd_create, falls back to legacy ashmem

static int create_shared_memory(const char *name, size_t size) {

    int fd = -1;



    // 1. Try modern Linux memfd_create (Available in Linux kernel 3.17+ / Android API 29+)

#ifdef __NR_memfd_create

    fd = syscall(319, name, 0); // 319 is __NR_memfd_create on ARM64 / x86_64

#endif



    if (fd >= 0) {

        printf("[Daemon] Created shared memory via modern memfd_create (fd: %d)\n", fd);

        if (ftruncate(fd, size) == -1) {

            perror("[Daemon] Failed to set size on memfd");

            close(fd);

            return -1;

        }

        return fd;

    }



    // 2. Fallback to Android Legacy Ashmem (/dev/ashmem)

    printf("[Daemon] memfd_create failed or unsupported. Falling back to Android ashmem...\n");

    fd = open("/dev/ashmem", O_RDWR);

    if (fd < 0) {

        perror("[Daemon] Failed to open /dev/ashmem");

        return -1;

    }



    // Set name on ashmem region

    char name_buf[ASHMEM_NAME_LEN];

    strncpy(name_buf, name, sizeof(name_buf));

    if (ioctl(fd, ASHMEM_SET_NAME, name_buf) < 0) {

        perror("[Daemon] Failed to set ashmem name");

        close(fd);

        return -1;

    }



    // Set size on ashmem region

    if (ioctl(fd, ASHMEM_SET_SIZE, size) < 0) {

        perror("[Daemon] Failed to set ashmem size");

        close(fd);

        return -1;

    }



    printf("[Daemon] Created shared memory via Android ashmem (fd: %d)\n", fd);

    return fd;

}



// Employs ancillary messages (SCM_RIGHTS) over Unix Domain Sockets to transfer a raw file descriptor

static int send_fd(int socket_fd, int fd_to_send) {

    struct msghdr msg = {0};

    struct iovec iov[1];



    // We must send at least 1 byte of normal data alongside the control message

    char payload_byte = 'F';

    iov[0].iov_base = &payload_byte;

    iov[0].iov_len = 1;

    msg.msg_iov = iov;

    msg.msg_iovlen = 1;



    // Allocate auxiliary control data alignment buffer

    union {

        char buf[CMSG_SPACE(sizeof(int))];

        struct cmsghdr align;

    } ctrl_un;



    msg.msg_control = ctrl_un.buf;

    msg.msg_controllen = sizeof(ctrl_un.buf);



    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);

    cmsg->cmsg_level = SOL_SOCKET;

    cmsg->cmsg_type = SCM_RIGHTS;

    cmsg->cmsg_len = CMSG_LEN(sizeof(int));



    // Insert the shared memory file descriptor into the payload of the control message

    int *fd_ptr = (int *)CMSG_DATA(cmsg);

    *fd_ptr = fd_to_send;



    ssize_t bytes_sent = sendmsg(socket_fd, &msg, 0);

    if (bytes_sent < 0) {

        perror("[Daemon] Failed to execute sendmsg for SCM_RIGHTS");

        return -1;

    }

    return 0;

}



int main() {

    printf("[Daemon] Initializing High-Speed Shared Memory System...\n");



    // Establish shared memory

    int shm_fd = create_shared_memory(SHM_REGION_NAME, SHM_REGION_SIZE);

    if (shm_fd < 0) {

        fprintf(stderr, "[Daemon] Critical: Shared memory allocation failed.\n");

        return EXIT_FAILURE;

    }



    // Map shared memory region into daemon's address space

    SharedStateBuffer *state = (SharedStateBuffer *)mmap(

        NULL, SHM_REGION_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0

    );

    if (state == MAP_FAILED) {

        perror("[Daemon] Failed to map shared memory");

        close(shm_fd);

        return EXIT_FAILURE;

    }



    // Initialize state

    atomic_init(&state->seq_number, 0);

    atomic_init(&state->is_writing, false);

    memset(&state->data, 0, sizeof(TelemetryData));



    // Bind local Unix Domain Socket for client discovery and fd passing

    unlink(SHM_SOCKET_PATH);

    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (server_fd == -1) {

        perror("[Daemon] Failed to create broker socket");

        munmap(state, SHM_REGION_SIZE);

        close(shm_fd);

        return EXIT_FAILURE;

    }



    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, SHM_SOCKET_PATH, sizeof(addr.sun_path) - 1);



    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {

        perror("[Daemon] Failed to bind local socket");

        close(server_fd);

        munmap(state, SHM_REGION_SIZE);

        close(shm_fd);

        return EXIT_FAILURE;

    }



    chmod(SHM_SOCKET_PATH, 0777);



    if (listen(server_fd, 5) == -1) {

        perror("[Daemon] Listen failed");

        close(server_fd);

        munmap(state, SHM_REGION_SIZE);

        close(shm_fd);

        return EXIT_FAILURE;

    }



    printf("[Daemon] Shared memory broker listening on: %s\n", SHM_SOCKET_PATH);



    // Spawning Simulation Thread / Loop

    uint32_t simulated_seq = 0;

    while (1) {

        // Non-blocking socket accept loop (simulate sensor streaming simultaneously)

        struct sockaddr_un client_addr;

        socklen_t client_len = sizeof(client_addr);



        // Use select/poll with low timeout to prevent lockups and handle connections

        struct timeval tv = {0, 10000}; // 10ms poll interval (100Hz telemetry frequency)

        fd_set rfds;

        FD_ZERO(&rfds);

        FD_SET(server_fd, &rfds);



        int ready = select(server_fd + 1, &rfds, NULL, NULL, &tv);

        if (ready > 0 && FD_ISSET(server_fd, &rfds)) {

            int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

            if (client_fd >= 0) {

                printf("[Daemon] Client connected. Transferring Shared Memory FD...\n");

                if (send_fd(client_fd, shm_fd) == 0) {

                    printf("[Daemon] Successfully sent FD %d to client.\n", shm_fd);

                }

                close(client_fd); // Client has the FD, connection can be closed immediately

            }

        }



        // Lock-free update of hardware state in shared memory

        atomic_store(&state->is_writing, true);



        struct timespec ts;

        clock_gettime(CLOCK_MONOTONIC, &ts);

        state->data.timestamp_ns = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;



        // Simulate sensor values

        state->data.accelerometer[0] = 0.05f * (simulated_seq % 20);

        state->data.accelerometer[1] = -0.12f * (simulated_seq % 15);

        state->data.accelerometer[2] = 9.81f + 0.02f * (simulated_seq % 10);

        state->data.gyroscope[0] = 0.01f * (simulated_seq % 5);

        state->data.gyroscope[1] = -0.015f * (simulated_seq % 7);

        state->data.gyroscope[2] = 0.003f * (simulated_seq % 12);

        state->data.wifi_signal_rssi = 65 + (simulated_seq % 5); // RSSI -65 to -70

        state->data.bt_device_count = 3 + (simulated_seq % 3);



        atomic_store(&state->is_writing, false);

        atomic_store(&state->seq_number, ++simulated_seq);

    }



    close(server_fd);

    munmap(state, SHM_REGION_SIZE);

    close(shm_fd);

    unlink(SHM_SOCKET_PATH);

    return EXIT_SUCCESS;

}
```
[FILE_PATH_TERMINATED]

---

#### 📄 File: `sdk/src/quickjs_shm_binding.c`
##### **Technical & Architectural Commentary:**
- **Implementation Engine:** Handles direct memory manipulation, pointer arithmetic, zero-copy socket transfers, and epoll multiplexing buffers.

[FILE_PATH_START: sdk/src/quickjs_shm_binding.c]
```c
#include "quickjs.h"

#include <sys/mman.h>

#include <unistd.h>

#include <fcntl.h>

#include "shm_common.h"



// Struct wrapping our mapped shared state context inside QuickJS

typedef struct {

    int shm_fd;

    SharedStateBuffer *state;

} QuickJSShmContext;



static void js_shm_finalizer(JSRuntime *rt, JSValue val) {

    QuickJSShmContext *ctx = JS_GetOpaque(val, 1); // Get opaque context class

    if (ctx) {

        if (ctx->state) {

            munmap(ctx->state, SHM_REGION_SIZE);

        }

        if (ctx->shm_fd >= 0) {

            close(ctx->shm_fd);

        }

        js_free_rt(rt, ctx);

    }

}



// Maps JavaScript: shm.readTelemetry() -> returns standard JS Object

static JSValue js_shm_read_telemetry(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    QuickJSShmContext *qjs_ctx = JS_GetOpaque2(ctx, this_val, 1);

    if (!qjs_ctx || !qjs_ctx->state) {

        return JS_ThrowInternalError(ctx, "Shared memory block is not mapped or initialized");

    }



    SharedStateBuffer *state = qjs_ctx->state;



    // Perform a lock-free read check

    if (atomic_load(&state->is_writing)) {

        return JS_NULL; // Busy, write in progress

    }



    JSValue obj = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, obj, "timestampNs", JS_NewInt64(ctx, state->data.timestamp_ns));

    JS_SetPropertyStr(ctx, obj, "wifiRssi", JS_NewInt32(ctx, state->data.wifi_signal_rssi));

    JS_SetPropertyStr(ctx, obj, "btCount", JS_NewInt32(ctx, state->data.bt_device_count));



    // Map Accelerometer array

    JSValue acc = JS_NewArray(ctx);

    for (int i = 0; i < 3; i++) {

        JS_SetPropertyUint32(ctx, acc, i, JS_NewFloat64(ctx, state->data.accelerometer[i]));

    }

    JS_SetPropertyStr(ctx, obj, "accelerometer", acc);



    // Map Gyroscope array

    JSValue gyro = JS_NewArray(ctx);

    for (int i = 0; i < 3; i++) {

        JS_SetPropertyUint32(ctx, gyro, i, JS_NewFloat64(ctx, state->data.gyroscope[i]));

    }

    JS_SetPropertyStr(ctx, obj, "gyroscope", gyro);



    return obj;

}



static const JSCFunctionListEntry js_shm_funcs[] = {

    JS_CFUNC_DEF("readTelemetry", 0, js_shm_read_telemetry),

};



// Initializer patterns for binding our library dynamically to QuickJS module registries

static int js_shm_init(JSContext *ctx, JSModuleDef *m) {

    return JS_SetModuleExportList(ctx, m, js_shm_funcs, sizeof(js_shm_funcs)/sizeof(js_shm_funcs));

}



JSModuleDef *js_init_module_shm(JSContext *ctx, const char *module_name) {

    JSModuleDef *m = JS_NewCModule(ctx, module_name, js_shm_init);

    if (!m) return NULL;

    JS_AddModuleExportList(ctx, m, js_shm_funcs, sizeof(js_shm_funcs)/sizeof(js_shm_funcs));

    return m;

}
```
[FILE_PATH_TERMINATED]

---