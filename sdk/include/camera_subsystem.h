#ifndef NATIVE_CAMERA_SUBSYSTEM_H

#define NATIVE_CAMERA_SUBSYSTEM_H



#include <stdint.h>

#include <stddef.h>

#include <camera/NdkCameraManager.h>

#include <camera/NdkCameraDevice.h>

#include <camera/NdkCameraCaptureSession.h>

#include <media/NdkImageReader.h>



#ifdef __cplusplus

extern "C" {

#endif



#define CAM_SUCCESS         0

#define CAM_ERR_INIT       -1

#define CAM_ERR_DEVICE     -2

#define CAM_ERR_SESSION    -3



#pragma pack(push, 1)



typedef struct {

    uint8_t *y_plane;

    uint8_t *u_plane;

    uint8_t *v_plane;

    int32_t  y_stride;

    int32_t  uv_stride;

    int32_t  uv_pixel_stride;

    int32_t  width;

    int32_t  height;

    uint64_t timestamp_ns;

} CameraYuvFrame;



typedef struct {

    ACameraManager        *manager;

    ACameraDevice         *device;

    ACameraOutputTarget   *output_target;

    ACaptureRequest       *capture_request;

    ACameraCaptureSession *capture_session;

    AImageReader          *image_reader;

    ANativeWindow         *native_window;

} CameraContext;



#pragma pack(pop)



// Callback triggered whenever a raw YUV frame is successfully queued

typedef void (*CameraFrameCallback)(const CameraYuvFrame *frame, void *user_data);



int camera_initialize(CameraContext *ctx);

int camera_open_device(CameraContext *ctx, const char *camera_id);

int camera_start_streaming(CameraContext *ctx, int32_t width, int32_t height, CameraFrameCallback cb, void *user_data);

void camera_stop_streaming(CameraContext *ctx);

void camera_close_device(CameraContext *ctx);



#ifdef __cplusplus

}

#endif



#endif // NATIVE_CAMERA_SUBSYSTEM_H