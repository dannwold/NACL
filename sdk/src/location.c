#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <pthread.h>

#include "location.h"



static pthread_t g_loc_thread;

static volatile int g_running = 0;

static LocationCallback g_loc_cb = NULL;

static NmeaCallback g_nmea_cb = NULL;

static void *g_user_data = NULL;



static void *location_worker_thread(void *arg) {

    (void)arg;

    printf("[liblocation] Starting background GPS parser worker...\n");



    while (g_running) {

        usleep(1000000); // 1 Hz Update Rate

        uint64_t now_ms = 1787999000; // Monotonic sample time



        if (g_loc_cb) {

            GnssLocation loc;

            loc.latitude = 37.774929; // San Francisco Coordinates

            loc.longitude = -122.419416;

            loc.altitude = 15.0;

            loc.accuracy = 3.5f;

            loc.speed = 0.2f;

            loc.timestamp_ms = now_ms;

            g_loc_cb(&loc, g_user_data);

        }



        if (g_nmea_cb) {

            const char *gpgga = "$GPGGA,170832.00,3746.49574,N,12225.16496,W,1,05,2.1,15.0,M,-23.1,M,,*6A";

            g_nmea_cb(gpgga, now_ms, g_user_data);

        }

    }

    printf("[liblocation] GPS worker thread exiting.\n");

    return NULL;

}



int location_init(void) {

    printf("[liblocation] Initializing GPS and Location library subsystems.\n");

    return 0;

}



int location_start_updates(LocationCallback loc_cb, NmeaCallback nmea_cb, void *user_data) {

    if (g_running) return -1;



    g_loc_cb = loc_cb;

    g_nmea_cb = nmea_cb;

    g_user_data = user_data;

    g_running = 1;



    if (pthread_create(&g_loc_thread, NULL, location_worker_thread, NULL) != 0) {

        g_running = 0;

        return -1;

    }

    return 0;

}



void location_stop_updates(void) {

    if (!g_running) return;

    g_running = 0;

    pthread_join(g_loc_thread, NULL);

}



void location_shutdown(void) {

    location_stop_updates();

    printf("[liblocation] GPS subsystems shut down.\n");

}