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