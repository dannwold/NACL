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

#include "ipc_common.h"



#define MAX_EVENTS 16



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



static void handle_request(int client_fd, const IpcHeader *header, const uint8_t *payload) {

    IpcHeader response = *header;

    response.status = STATUS_OK;

    uint8_t *response_payload = NULL;

    uint32_t resp_len = 0;



    printf("[Daemon] Processing Transaction ID: %u, Subsystem: %d, Command: %d\n",

           header->transaction_id, header->subsystem, header->command);



    if (header->magic != IPC_MAGIC_SIGNATURE) {

        response.status = STATUS_ERROR;

        printf("[Daemon] Invalid magic signature: 0x%X\n", header->magic);

    } else {

        switch (header->subsystem) {

            case SUBSYSTEM_WIFI:

                if (header->command == CMD_WIFI_START_SCAN) {

                    printf("[Daemon] Executing low-level hardware call: Wi-Fi scanning...\n");

                    const char *mock_scan_ack = "WiFi Scan Triggered Asynchronously";

                    resp_len = strlen(mock_scan_ack) + 1;

                    response_payload = (uint8_t *)strdup(mock_scan_ack);

                } else {

                    response.status = STATUS_UNSUPPORTED;

                }

                break;



            case SUBSYSTEM_BLUETOOTH:

                if (header->command == CMD_BT_START_SCAN) {

                    printf("[Daemon] Executing low-level hardware call: BLE scanning...\n");

                    const char *mock_bt_ack = "BLE Discovery Started";

                    resp_len = strlen(mock_bt_ack) + 1;

                    response_payload = (uint8_t *)strdup(mock_bt_ack);

                } else {

                    response.status = STATUS_UNSUPPORTED;

                }

                break;



            default:

                response.status = STATUS_UNSUPPORTED;

                break;

        }

    }



    response.payload_len = resp_len;



    // Write header and payload back sequentially

    write(client_fd, &response, sizeof(IpcHeader));

    if (resp_len > 0 && response_payload != NULL) {

        write(client_fd, response_payload, resp_len);

        free(response_payload);

    }

}



int main(int argc, char *argv[]) {

    const char *socket_path = IPC_SOCKET_WIFI; // Defaults to WiFi

    if (argc > 1) {

        if (strcmp(argv[1], "bluetooth") == 0) {

            socket_path = IPC_SOCKET_BT;

        } else if (strcmp(argv[1], "sensors") == 0) {

            socket_path = IPC_SOCKET_SENS;

        }

    }



    printf("[Daemon] Starting native service process targeting socket: %s\n", socket_path);



    if (ensure_socket_dir() < 0) {

        perror("[Daemon] Failed to establish SDK socket directory");

        return EXIT_FAILURE;

    }



    unlink(socket_path);



    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (server_fd == -1) {

        perror("[Daemon] Failed to create POSIX Unix Socket");

        return EXIT_FAILURE;

    }



    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);



    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {

        perror("[Daemon] Failed to bind local socket");

        close(server_fd);

        return EXIT_FAILURE;

    }



    // Grant read/write access to socket for app sandbox processes

    chmod(socket_path, 0777);



    if (listen(server_fd, SOMAXCONN) == -1) {

        perror("[Daemon] Socket listen failed");

        close(server_fd);

        return EXIT_FAILURE;

    }



    if (set_nonblocking(server_fd) == -1) {

        perror("[Daemon] Nonblocking configuration failed");

        close(server_fd);

        return EXIT_FAILURE;

    }



    int epoll_fd = epoll_create1(0);

    if (epoll_fd == -1) {

        perror("[Daemon] Epoll initialization failed");

        close(server_fd);

        return EXIT_FAILURE;

    }



    struct epoll_event ev, events[MAX_EVENTS];

    ev.events = EPOLLIN;

    ev.data.fd = server_fd;



    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {

        perror("[Daemon] Failed adding server fd to epoll loop");

        close(epoll_fd);

        close(server_fd);

        return EXIT_FAILURE;

    }



    printf("[Daemon] Running multiplexed event loop on process ID %d...\n", getpid());



    while (1) {

        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        if (nfds == -1) {

            if (errno == EINTR) continue;

            perror("[Daemon] Epoll wait encountered an error");

            break;

        }



        for (int i = 0; i < nfds; ++i) {

            if (events[i].data.fd == server_fd) {

                struct sockaddr_un client_addr;

                socklen_t client_len = sizeof(client_addr);

                int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

                if (client_fd == -1) {

                    if (errno != EAGAIN && errno != EWOULDBLOCK) {

                        perror("[Daemon] Connection accept failed");

                    }

                    continue;

                }



                if (set_nonblocking(client_fd) == -1) {

                    perror("[Daemon] Client nonblocking configuration failed");

                    close(client_fd);

                    continue;

                }



                ev.events = EPOLLIN | EPOLLET;

                ev.data.fd = client_fd;

                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {

                    perror("[Daemon] Failed adding client fd to epoll loop");

                    close(client_fd);

                } else {

                    printf("[Daemon] Established connection on client fd: %d\n", client_fd);

                }

            } else {

                int client_fd = events[i].data.fd;

                IpcHeader header;

                ssize_t bytes_read = read(client_fd, &header, sizeof(IpcHeader));



                if (bytes_read <= 0) {

                    printf("[Daemon] Client disconnected on fd: %d\n", client_fd);

                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);

                    close(client_fd);

                } else if (bytes_read == sizeof(IpcHeader)) {

                    uint8_t *payload = NULL;

                    if (header.payload_len > 0) {

                        payload = malloc(header.payload_len);

                        ssize_t payload_bytes = read(client_fd, payload, header.payload_len);

                        if (payload_bytes != header.payload_len) {

                            printf("[Daemon] Incomplete payload read on fd %d\n", client_fd);

                        }

                    }



                    handle_request(client_fd, &header, payload);



                    if (payload != NULL) {

                        free(payload);

                    }

                } else {

                    printf("[Daemon] Malformed packet header on fd %d\n", client_fd);

                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);

                    close(client_fd);

                }

            }

        }

    }



    close(server_fd);

    close(epoll_fd);

    unlink(socket_path);

    return EXIT_SUCCESS;

}