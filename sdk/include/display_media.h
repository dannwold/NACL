#ifndef LIBDISPLAY_MEDIA_H

#define LIBDISPLAY_MEDIA_H



#include <stdint.h>

#include <stddef.h>



#ifdef __cplusplus

extern "C" {

#endif



typedef struct {

    uint32_t width;

    uint32_t height;

    uint32_t format;

    uint32_t stride;

    uint8_t *y_data;

    uint8_t *u_data;

    uint8_t *v_data;

    uint64_t timestamp_ns;

} VideoFrame;



typedef void (*VideoFrameCallback)(const VideoFrame *frame, void *user_data);



int media_codec_init(void);

int media_codec_configure_decoder(const char *mime_type, int width, int height);

int media_codec_decode_packet(const uint8_t *data, size_t size, uint64_t pts_us, VideoFrameCallback cb, void *user_data);

void media_codec_shutdown(void);



int display_render_frame(void *native_window_ptr, const VideoFrame *frame);



#ifdef __cplusplus

}

#endif



#endif // LIBDISPLAY_MEDIA_H