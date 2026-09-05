#include <stdlib.h>

#include <string.h>

#include <android/log.h>

#include "camera_subsystem.h"



#define LOG_TAG "NACL_Camera"



static CameraFrameCallback g_frame_cb = NULL;

static void *g_frame_user_data = NULL;



// Native callback triggered by AImageReader when a raw frame is ready

static void on_image_available(void *context, AImageReader *reader) {

    if (!g_frame_cb) return;



    AImage *image = NULL;

    if (AImageReader_acquireNextImage(reader, &image) != AMEDIA_OK || !image) {

        return;

    }



    CameraYuvFrame frame;

    memset(&frame, 0, sizeof(CameraYuvFrame));



    AImage_getWidth(image, &frame.width);

    AImage_getHeight(image, &frame.height);

    AImage_getTimestamp(image, (int64_t *)&frame.timestamp_ns);



    // Extract raw YUV buffers directly from NDK imageplanes

    int32_t plane_count = 0;

    AImage_getNumberOfPlanes(image, &plane_count);



    if (plane_count >= 3) {

        int y_len = 0, u_len = 0, v_len = 0;

        AImage_getPlaneData(image, 0, &frame.y_plane, &y_len);

        AImage_getPlaneData(image, 1, &frame.u_plane, &u_len);

        AImage_getPlaneData(image, 2, &frame.v_plane, &v_len);



        AImage_getPlaneRowStride(image, 0, &frame.y_stride);

        AImage_getPlaneRowStride(image, 1, &frame.uv_stride);

        AImage_getPlanePixelStride(image, 1, &frame.uv_pixel_stride);



        g_frame_cb(&frame, g_frame_user_data);

    }



    AImage_delete(image);

}



int camera_initialize(CameraContext *ctx) {

    if (!ctx) return CAM_ERR_INIT;

    memset(ctx, 0, sizeof(CameraContext));



    ctx->manager = ACameraManager_create();

    if (!ctx->manager) return CAM_ERR_INIT;



    return CAM_SUCCESS;

}



int camera_open_device(CameraContext *ctx, const char *camera_id) {

    if (!ctx || !ctx->manager) return CAM_ERR_INIT;



    ACameraDevice_StateCallbacks callbacks;

    memset(&callbacks, 0, sizeof(callbacks));

    // Internal ACameraDevice state mappings can be registered here



    camera_status_t res = ACameraManager_openCamera(ctx->manager, camera_id, &callbacks, &ctx->device);

    if (res != ACAMERA_OK) {

        return CAM_ERR_DEVICE;

    }



    return CAM_SUCCESS;

}



int camera_start_streaming(CameraContext *ctx, int32_t width, int32_t height, CameraFrameCallback cb, void *user_data) {

    if (!ctx || !ctx->device) return CAM_ERR_DEVICE;

    g_frame_cb = cb;

    g_frame_user_data = user_data;



    // Create dynamic high-performance AImageReader mapped in YUV 420 Format

    media_status_t img_res = AImageReader_new(width, height, AIMAGE_FORMAT_YUV_420_888, 4, &ctx->image_reader);

    if (img_res != AMEDIA_OK || !ctx->image_reader) {

        return CAM_ERR_INIT;

    }



    AImageReader_ImageListener listener;

    listener.context = ctx;

    listener.onImageAvailable = on_image_available;

    AImageReader_setImageListener(ctx->image_reader, &listener);



    AImageReader_getWindow(ctx->image_reader, &ctx->native_window);



    // Set up standard target outputs

    ANativeWindow_acquire(ctx->native_window);

    ACameraOutputTarget_create(ctx->native_window, &ctx->output_target);



    // Initialize raw Capture Request with standard preview configurations

    ACameraDevice_createCaptureRequest(ctx->device, TEMPLATE_PREVIEW, &ctx->capture_request);

    ACaptureRequest_addTarget(ctx->capture_request, ctx->output_target);



    // Create session target container

    ACaptureSessionOutputContainer *container = NULL;

    ACaptureSessionOutputContainer_create(&container);



    ACaptureSessionOutput *output = NULL;

    ACaptureSessionOutput_create(ctx->native_window, &output);

    ACaptureSessionOutputContainer_add(container, output);



    // Instantiate and trigger the Capture Session

    ACameraCaptureSession_stateCallbacks session_callbacks;

    memset(&session_callbacks, 0, sizeof(session_callbacks));



    camera_status_t session_res = ACameraDevice_createCaptureSession(

        ctx->device, container, &session_callbacks, &ctx->capture_session

    );



    if (session_res != ACAMERA_OK) {

        return CAM_ERR_SESSION;

    }



    // Begin infinite capturing pipeline loop

    ACameraCaptureSession_setRepeatingRequest(ctx->capture_session, NULL, 1, &ctx->capture_request, NULL);



    return CAM_SUCCESS;

}



void camera_stop_streaming(CameraContext *ctx) {

    if (!ctx) return;

    if (ctx->capture_session) {

        ACameraCaptureSession_stopRepeating(ctx->capture_session);

        ACameraCaptureSession_close(ctx->capture_session);

        ctx->capture_session = NULL;

    }

    g_frame_cb = NULL;

}



void camera_close_device(CameraContext *ctx) {

    if (!ctx) return;

    camera_stop_streaming(ctx);



    if (ctx->device) {

        ACameraDevice_close(ctx->device);

        ctx->device = NULL;

    }

    if (ctx->image_reader) {

        AImageReader_delete(ctx->image_reader);

        ctx->image_reader = NULL;

    }

    if (ctx->manager) {

        ACameraManager_delete(ctx->manager);

        ctx->manager = NULL;

    }

}