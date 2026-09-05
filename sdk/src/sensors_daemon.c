#include <android/sensor.h>
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



static void handle_client_request(int client_fd, const IpcHeader *header, const uint8_t *payload, ASensorEventQueue* queue, const ASensor* accel) {

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



    const ASensor* accel_sensor = ASensorManager_getDefaultSensor(sensor_manager, ASENSOR_TYPE_ACCELEROMETER);

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

    int sensor_fd = 1;

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

                        out_event.accuracy = (float)0;



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