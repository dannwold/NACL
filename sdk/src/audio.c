#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <pthread.h>

#include "audio.h"



static pthread_t g_capture_thread;

static volatile int g_capture_running = 0;

static AudioCaptureCallback g_capture_cb = NULL;

static void *g_capture_user_data = NULL;

static AudioConfig g_audio_config;



static void *audio_capture_worker(void *arg) {

    (void)arg;

    printf("[libaudio] AAudio capture stream starting...\n");



    size_t frame_size = (g_audio_config.format == AUDIO_FORMAT_PCM_FLOAT) ? sizeof(float) : sizeof(int16_t);

    size_t buffer_size_bytes = g_audio_config.buffer_frames * g_audio_config.channels * frame_size;

    uint8_t *simulated_buffer = (uint8_t *)malloc(buffer_size_bytes);

    memset(simulated_buffer, 0, buffer_size_bytes);



    while (g_capture_running) {

        uint32_t sleep_us = (uint32_t)(((double)g_audio_config.buffer_frames / g_audio_config.sample_rate) * 1000000.0);

        usleep(sleep_us);



        if (g_capture_cb) {

            g_capture_cb(simulated_buffer, buffer_size_bytes, g_capture_user_data);

        }

    }



    free(simulated_buffer);

    printf("[libaudio] AAudio capture stream stopped.\n");

    return NULL;

}



int audio_init(void) {

    printf("[libaudio] Audio subsystem initialized.\n");

    return 0;

}



int audio_start_playback(const AudioConfig *config) {

    if (!config) return -1;

    printf("[libaudio] AAudio playback stream started (Rate: %u, Ch: %u, Format: %d)\n",

           config->sample_rate, config->channels, config->format);

    return 0;

}



int audio_write_pcm(const void *data, size_t size_bytes) {

    (void)data;

    return (int)size_bytes;

}



int audio_start_capture(const AudioConfig *config, AudioCaptureCallback cb, void *user_data) {

    if (g_capture_running) return -1;

    if (!config || !cb) return -1;



    g_audio_config = *config;

    g_capture_cb = cb;

    g_capture_user_data = user_data;

    g_capture_running = 1;



    if (pthread_create(&g_capture_thread, NULL, audio_capture_worker, NULL) != 0) {

        g_capture_running = 0;

        return -1;

    }

    return 0;

}



void audio_stop_playback(void) {

    printf("[libaudio] AAudio playback stream stopped.\n");

}



void audio_stop_capture(void) {

    if (!g_capture_running) return;

    g_capture_running = 0;

    pthread_join(g_capture_thread, NULL);

}



void audio_shutdown(void) {

    audio_stop_playback();

    audio_stop_capture();

    printf("[libaudio] Audio subsystems completely offline.\n");

}