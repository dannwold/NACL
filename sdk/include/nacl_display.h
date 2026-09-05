/**

 * @file nacl_display.h

 * @brief Stable C ABI for low-overhead native rendering overlays.

 */



#ifndef NACL_DISPLAY_H

#define NACL_DISPLAY_H



#include <stdint.h>

#include <stddef.h>



#ifdef __cplusplus

extern "C" {

#endif



typedef enum {

    NACL_RENDER_API_EGL_GLES3 = 1,

    NACL_RENDER_API_VULKAN    = 2

} NaclRenderApi;



typedef struct {

    uint32_t width;

    uint32_t height;

    NaclRenderApi api;

    uint32_t preferred_fps;

    float    clear_color[4]; // RGBA normalized color array [0.0 - 1.0]

} NaclDisplayConfig;



typedef struct OpaqueNaclDisplayContext* NaclDisplayContext;



/**

 * @brief Initializes the low-level rendering context on an ANativeWindow.

 * @param window_handle Pointer to the ANativeWindow structure.

 * @param config Pointer to the runtime rendering configuration.

 * @return Opaque handle to the display context, or NULL on failure.

 */

NaclDisplayContext nacl_display_create(void* window_handle, const NaclDisplayConfig* config);



/**

 * @brief Submits a raw numeric or byte array data payload (like a PCM envelope) for rendering.

 * @param context The active display context handle.

 * @param data Float array representing normalized values to plot.

 * @param count Number of elements in the array.

 * @return 0 on success, or a negative status code on failure.

 */

int nacl_display_update_waveform_data(NaclDisplayContext context, const float* data, size_t count);



/**

 * @brief Triggers a frame rendering pass and swaps buffers to display the overlay.

 * @param context The active display context handle.

 */

void nacl_display_render_frame(NaclDisplayContext context);



/**

 * @brief Tears down EGL/Vulkan resources and detaches the native window.

 * @param context The display context handle to destroy.

 */

void nacl_display_destroy(NaclDisplayContext context);



#ifdef __cplusplus

}

#endif



#endif // NACL_DISPLAY_H