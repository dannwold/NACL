#ifndef LIBAUDIO_H

#define LIBAUDIO_H



#include <stdint.h>

#include <stddef.h>



#ifdef __cplusplus

extern "C" {

#endif



typedef enum {

    AUDIO_FORMAT_PCM_16BIT = 1,

    AUDIO_FORMAT_PCM_FLOAT = 2

} AudioFormat;



typedef struct {

    uint32_t sample_rate;

    uint16_t channels;

    AudioFormat format;

    uint32_t buffer_frames;

} AudioConfig;



typedef void (*AudioCaptureCallback)(const void *data, size_t size_bytes, void *user_data);



int audio_init(void);

int audio_start_playback(const AudioConfig *config);

int audio_write_pcm(const void *data, size_t size_bytes);

int audio_start_capture(const AudioConfig *config, AudioCaptureCallback cb, void *user_data);

void audio_stop_playback(void);

void audio_stop_capture(void);

void audio_shutdown(void);



#ifdef __cplusplus

}

#endif



#endif // LIBAUDIO_H