#include <sys/stat.h>
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