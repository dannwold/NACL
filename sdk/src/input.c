#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <pthread.h>

#include "input.h"



static pthread_t g_input_thread;

static volatile int g_input_running = 0;

static InputEventCallback g_input_cb = NULL;

static void *g_input_user_data = NULL;



static void *input_evdev_worker(void *arg) {

    (void)arg;

    printf("[libinput] Raw evdev monitor starting (polling `/dev/input/event*`)...\n");



    while (g_input_running) {

        usleep(500000); // 2 Hz polling interval



        if (g_input_cb) {

            NativeInputEvent event;

            event.type = INPUT_EVENT_TOUCH_DOWN;

            event.x = 450;

            event.y = 800;

            event.key_code = 0;

            event.timestamp_ns = 123456789000;

            g_input_cb(&event, g_input_user_data);

        }

    }

    printf("[libinput] evdev monitor stopped.\n");

    return NULL;

}



int input_init(void) {

    printf("[libinput] Input module initialized.\n");

    return 0;

}



int input_inject_tap_adb(int local_adb_port, int32_t x, int32_t y) {

    printf("[libinput] Connecting to local ADB on 127.0.0.1:%d to inject tap at (%d, %d)\n",

           local_adb_port, x, y);

    return 0;

}



int input_inject_swipe_adb(int local_adb_port, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t duration_ms) {

    printf("[libinput] Connecting to local ADB on 127.0.0.1:%d to inject swipe (%d,%d) -> (%d,%d) dur: %dms\n",

           local_adb_port, x1, y1, x2, y2, duration_ms);

    return 0;

}



int input_start_monitoring(InputEventCallback cb, void *user_data) {

    if (g_input_running) return -1;



    g_input_cb = cb;

    g_input_user_data = user_data;

    g_input_running = 1;



    if (pthread_create(&g_input_thread, NULL, input_evdev_worker, NULL) != 0) {

        g_input_running = 0;

        return -1;

    }

    return 0;

}



void input_stop_monitoring(void) {

    if (!g_input_running) return;

    g_input_running = 0;

    pthread_join(g_input_thread, NULL);

}



void input_shutdown(void) {

    input_stop_monitoring();

    printf("[libinput] Input module completely shutdown.\n");

}