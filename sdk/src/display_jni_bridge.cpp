#include <jni.h>

#include <android/native_window_jni.h>

#include "nacl_display.h"



extern "C" {



JNIEXPORT jlong JNICALL

Java_com_your_app_nacl_NaclOverlayService_nativeInitializeDisplay(JNIEnv* env, jobject thiz, jobject surface) {

    if (!surface) return 0;



    // Convert the JVM Surface object into an ABI-compatible raw native pointer

    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);

    if (!window) return 0;



    NaclDisplayConfig config = {

        .width = 0, // Auto config

        .height = 0,

        .api = NACL_RENDER_API_EGL_GLES3,

        .preferred_fps = 60,

        .clear_color = {0.0f, 0.0f, 0.0f, 0.0f} // Translucent clear color

    };



    NaclDisplayContext context = nacl_display_create(window, &config);

    return reinterpret_cast<jlong>(context);

}



JNIEXPORT void JNICALL

Java_com_your_app_nacl_NaclOverlayService_nativeUpdateDisplayWaveform(JNIEnv* env, jobject thiz, jlong context_handle, jfloatArray data) {

    NaclDisplayContext context = reinterpret_cast<NaclDisplayContext>(context_handle);

    if (!context || !data) return;



    jsize count = env->GetArrayLength(data);

    jfloat* body = env->GetFloatArrayElements(data, nullptr);



    nacl_display_update_waveform_data(context, body, count);



    env->ReleaseFloatArrayElements(data, body, JNI_ABORT);

}



JNIEXPORT void JNICALL

Java_com_your_app_nacl_NaclOverlayService_nativeRenderDisplayFrame(JNIEnv* env, jobject thiz, jlong context_handle) {

    NaclDisplayContext context = reinterpret_cast<NaclDisplayContext>(context_handle);

    if (context) {

        nacl_display_render_frame(context);

    }

}



JNIEXPORT void JNICALL

Java_com_your_app_nacl_NaclOverlayService_nativeReleaseDisplay(JNIEnv* env, jobject thiz, jlong context_handle) {

    NaclDisplayContext context = reinterpret_cast<NaclDisplayContext>(context_handle);

    if (context) {

        nacl_display_destroy(context);

    }

}



}