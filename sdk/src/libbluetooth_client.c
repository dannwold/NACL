#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <sys/socket.h>

#include <sys/un.h>

#include <errno.h>

#include "bluetooth_ipc_common.h"



static int connect_to_bt_daemon() {

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd == -1) {

        perror("[BT Client] Socket creation failed");

        return -1;

    }



    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, IPC_SOCKET_BT, sizeof(addr.sun_path) - 1);



    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {

        fprintf(stderr, "[BT Client] Socket connection failed: %s\n", strerror(errno));

        close(fd);

        return -1;

    }



    return fd;

}



// Stable Export Blocks with global visibility ELF flags [25]

__attribute__((visibility("default")))

int bt_start_le_scan() {

    int daemon_fd = connect_to_bt_daemon();

    if (daemon_fd < 0) return -1;



    static uint32_t tx_id = 0;

    BtIpcHeader req;

    req.magic = BT_IPC_MAGIC;

    req.transaction_id = ++tx_id;

    req.command = CMD_BT_START_LE_SCAN;

    req.status = 0;

    req.payload_len = 0;



    if (write(daemon_fd, &req, sizeof(BtIpcHeader)) != sizeof(BtIpcHeader)) {

        close(daemon_fd);

        return -1;

    }



    BtIpcHeader resp;

    if (read(daemon_fd, &resp, sizeof(BtIpcHeader)) != sizeof(BtIpcHeader)) {

        close(daemon_fd);

        return -1;

    }



    close(daemon_fd);

    return resp.status;

}



__attribute__((visibility("default")))

int bt_stop_le_scan() {

    int daemon_fd = connect_to_bt_daemon();

    if (daemon_fd < 0) return -1;



    static uint32_t tx_id = 0;

    BtIpcHeader req;

    req.magic = BT_IPC_MAGIC;

    req.transaction_id = ++tx_id;

    req.command = CMD_BT_STOP_LE_SCAN;

    req.status = 0;

    req.payload_len = 0;



    if (write(daemon_fd, &req, sizeof(BtIpcHeader)) != sizeof(BtIpcHeader)) {

        close(daemon_fd);

        return -1;

    }



    BtIpcHeader resp;

    if (read(daemon_fd, &resp, sizeof(BtIpcHeader)) != sizeof(BtIpcHeader)) {

        close(daemon_fd);

        return -1;

    }



    close(daemon_fd);

    return resp.status;

}



__attribute__((visibility("default")))

int bt_get_discovered_devices(BleScanResult *out_buffer, uint32_t max_count, uint32_t *out_count) {

    if (out_buffer == NULL || out_count == NULL) return -2;



    int daemon_fd = connect_to_bt_daemon();

    if (daemon_fd < 0) return -1;



    static uint32_t tx_id = 0;

    BtIpcHeader req;

    req.magic = BT_IPC_MAGIC;

    req.transaction_id = ++tx_id;

    req.command = CMD_BT_GET_DEVICES;

    req.status = 0;

    req.payload_len = 0;



    if (write(daemon_fd, &req, sizeof(BtIpcHeader)) != sizeof(BtIpcHeader)) {

        close(daemon_fd);

        return -1;

    }



    BtIpcHeader resp;

    if (read(daemon_fd, &resp, sizeof(BtIpcHeader)) != sizeof(BtIpcHeader)) {

        close(daemon_fd);

        return -1;

    }



    int status = resp.status;

    *out_count = 0;



    if (status == 0 && resp.payload_len > 0) {

        uint32_t bytes_to_read = resp.payload_len;

        uint32_t limit_bytes = max_count * sizeof(BleScanResult);

        uint32_t target_read = (bytes_to_read < limit_bytes) ? bytes_to_read : limit_bytes;



        uint8_t *temp_buffer = malloc(bytes_to_read);

        ssize_t total_read = 0;

        while (total_read < bytes_to_read) {

            ssize_t r = read(daemon_fd, temp_buffer + total_read, bytes_to_read - total_read);

            if (r <= 0) break;

            total_read += r;

        }



        if (total_read == bytes_to_read) {

            memcpy(out_buffer, temp_buffer, target_read);

            *out_count = target_read / sizeof(BleScanResult);

        } else {

            status = -3;

        }

        free(temp_buffer);

    }



    close(daemon_fd);

    return status;

}



__attribute__((visibility("default")))

const char* bt_get_client_version() {

    return "1.0.0-NACL-BT";

}