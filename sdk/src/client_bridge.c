#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <sys/socket.h>

#include <sys/un.h>

#include <errno.h>

#include "ipc_common.h"



static int connect_to_daemon(const char *socket_path) {

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd == -1) {

        perror("[Client] Failed to create socket");

        return -1;

    }



    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);



    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {

        fprintf(stderr, "[Client] Connection failed to %s: %s\n", socket_path, strerror(errno));

        close(fd);

        return -1;

    }



    return fd;

}



// Stable C ABI: Sends an asynchronous or synchronous hardware call through our IPC broker

__attribute__((visibility("default")))

int execute_hardware_command(int subsystem, int command, const uint8_t *payload, uint32_t payload_len, uint8_t *out_buffer, uint32_t *out_len) {

    const char *socket_path = NULL;

    switch (subsystem) {

        case SUBSYSTEM_WIFI:

            socket_path = IPC_SOCKET_WIFI;

            break;

        case SUBSYSTEM_BLUETOOTH:

            socket_path = IPC_SOCKET_BT;

            break;

        case SUBSYSTEM_SENSORS:

            socket_path = IPC_SOCKET_SENS;

            break;

        default:

            return STATUS_UNSUPPORTED;

    }



    int daemon_fd = connect_to_daemon(socket_path);

    if (daemon_fd < 0) {

        return STATUS_ERROR;

    }



    static uint32_t global_tx_id = 0;

    IpcHeader request;

    request.magic = IPC_MAGIC_SIGNATURE;

    request.transaction_id = ++global_tx_id;

    request.subsystem = (uint16_t)subsystem;

    request.command = (uint16_t)command;

    request.status = 0;

    request.payload_len = payload_len;



    if (write(daemon_fd, &request, sizeof(IpcHeader)) != sizeof(IpcHeader)) {

        close(daemon_fd);

        return STATUS_ERROR;

    }



    if (payload_len > 0 && payload != NULL) {

        if (write(daemon_fd, payload, payload_len) != payload_len) {

            close(daemon_fd);

            return STATUS_ERROR;

        }

    }



    IpcHeader response;

    ssize_t bytes_read = read(daemon_fd, &response, sizeof(IpcHeader));

    if (bytes_read != sizeof(IpcHeader)) {

        fprintf(stderr, "[Client] Failed reading response header\n");

        close(daemon_fd);

        return STATUS_ERROR;

    }



    if (response.magic != IPC_MAGIC_SIGNATURE) {

        fprintf(stderr, "[Client] Response header signature verification failed\n");

        close(daemon_fd);

        return STATUS_ERROR;

    }



    int status = response.status;

    if (status == STATUS_OK && response.payload_len > 0) {

        if (out_buffer != NULL && out_len != NULL) {

            uint32_t limit = *out_len;

            uint32_t read_size = (response.payload_len < limit) ? response.payload_len : limit;



            ssize_t read_bytes = read(daemon_fd, out_buffer, response.payload_len);

            if (read_bytes > 0) {

                *out_len = read_bytes;

            }

        }

    } else if (out_len != NULL) {

        *out_len = 0;

    }



    close(daemon_fd);

    return status;

}



__attribute__((visibility("default")))

const char *get_client_library_version() {

    return "1.0.0-NACL-IPC";

}