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