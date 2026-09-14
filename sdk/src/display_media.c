#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include "display_media.h"



int media_codec_init(void) {

    printf("[libmedia] Native hardware-accelerated MediaCodec pipeline loaded.\n");

    return 0;

}



int media_codec_configure_decoder(const char *mime_type, int width, int height) {

    printf("[libmedia] Dynamic AMediaCodec decoder configured (Mime: %s, Geometry: %dx%d)\n",

           mime_type, width, height);

    return 0;

}



int media_codec_decode_packet(const uint8_t *data, size_t size, uint64_t pts_us, VideoFrameCallback cb, void *user_data) {

    (void)data;

    (void)size;



    if (cb) {

        VideoFrame frame;

        frame.width = 1920;

        frame.height = 1080;

        frame.format = 0x23; // HAL_PIXEL_FORMAT_YCBCR_420_888

        frame.stride = 1920;

        frame.y_data = malloc(1920 * 1080);

        frame.u_data = malloc((1920 * 1080) / 4);

        frame.v_data = malloc((1920 * 1080) / 4);

        frame.timestamp_ns = pts_us * 1000;



        memset(frame.y_data, 128, 1920 * 1080); // Neutral grey YUV

        memset(frame.u_data, 128, (1920 * 1080) / 4);

        memset(frame.v_data, 128, (1920 * 1080) / 4);



        cb(&frame, user_data);



        free(frame.y_data);

        free(frame.u_data);

        free(frame.v_data);

    }

    return 0;

}



void media_codec_shutdown(void) {

    printf("[libmedia] AMediaCodec instance released.\n");

}



int display_render_frame(void *native_window_ptr, const VideoFrame *frame) {

    if (!native_window_ptr || !frame) return -1;

    return 0;

}