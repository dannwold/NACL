#include <jni.h>

#include <pthread.h>

#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <dlfcn.h>

#include <android/log.h>



#define LOG_TAG "HostJniBridge"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)



// Global state variables

static JavaVM* g_jvm = nullptr;

static jobject g_app_context_ref = nullptr;

static pthread_key_t g_thread_key;

static pthread_mutex_t g_lifecycle_mutex = PTHREAD_MUTEX_INITIALIZER;



// Caching structure for our registered dynamically loaded modules

typedef struct {

    void* handle;

    int initialized;

} NativeRuntimeContext;



static NativeRuntimeContext g_runtime = { nullptr, 0 };



// Thread-local cleanup function called when a background POSIX thread exits

static void detach_current_thread_cleanup(void* env) {

    if (g_jvm && env) {

        g_jvm->DetachCurrentThread();

        LOGI("[JNI Bridge] Safely detached background worker thread from JVM.");

    }

}



// System initialization called automatically when libandroid_core.so is loaded

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {

    g_jvm = vm;

    JNIEnv* env = nullptr;

    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {

        return JNI_ERR;

    }



    // Set up thread-local storage key to track attached background worker threads

    if (pthread_key_create(&g_thread_key, detach_current_thread_cleanup) != 0) {

        LOGE("[JNI Bridge] Failed to instantiate thread-local cleanup key!");

        return JNI_ERR;

    }



    LOGI("[JNI Bridge] JNI_OnLoad completed. Global JavaVM* cached successfully.");

    return JNI_VERSION_1_6;

}



// Utility to retrieve a thread-safe JNIEnv context from background worker threads [7]

JNIEnv* get_safe_jni_env() {

    JNIEnv* env = nullptr;

    if (!g_jvm) return nullptr;



    jint res = g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6);

    if (res == JNI_EDETACHED) {

        // Thread is native and not yet attached. Attach it to JVM registry safely.

        JavaVMAttachArgs args = { JNI_VERSION_1_6, "NACL_WorkerThread", nullptr };

        if (g_jvm->AttachCurrentThread(&env, &args) == JNI_OK) {

            // Set thread-local value to trigger auto-detachment on thread exit [7]

            pthread_setspecific(g_thread_key, env);

            LOGI("[JNI Bridge] Successfully attached new worker thread to JVM.");

        } else {

            LOGE("[JNI Bridge] Failed to attach worker thread!");

            return nullptr;

        }

    }

    return env;

}



extern "C" {



// Native JNI Interface: Initializes our C++ native loader core and maps directories

JNIEXPORT jboolean JNICALL

Java_com_your_app_bootstrap_NativeInterface_nativeInitializeRuntime(

        JNIEnv* env, jobject thiz, jobject context, jstring private_dir_path) {



    pthread_mutex_lock(&g_lifecycle_mutex);

    if (g_runtime.initialized) {

        pthread_mutex_unlock(&g_lifecycle_mutex);

        return JNI_TRUE;

    }



    // Cache a global reference to the host application context to query managers later [7]

    g_app_context_ref = env->NewGlobalRef(context);



    const char* path_chars = env->GetStringUTFChars(private_dir_path, nullptr);

    LOGI("[JNI Bridge] Initializing native runtime workspace at: %s", path_chars);



    // Resolve system paths and initialize internal dynamic token registries [2, 5]

    char core_lib_path[512];

    snprintf(core_lib_path, sizeof(core_lib_path), "%s/lib/libandroid_core.so", path_chars);



    g_runtime.handle = dlopen(core_lib_path, RTLD_NOW | RTLD_GLOBAL);

    if (!g_runtime.handle) {

        LOGE("[JNI Bridge] Failed to load core loader library: %s", dlerror());

        env->ReleaseStringUTFChars(private_dir_path, path_chars);

        pthread_mutex_unlock(&g_lifecycle_mutex);

        return JNI_FALSE;

    }



    // Resolve base bootstrap symbol from the core loader

    typedef int (*init_core_fn)(const char*);

    init_core_fn init_core = (init_core_fn)dlsym(g_runtime.handle, "initialize_core_registry");

    if (!init_core || init_core(path_chars) != 0) {

        LOGE("[JNI Bridge] Core registry initialization returned failure.");

        dlclose(g_runtime.handle);

        g_runtime.handle = nullptr;

        env->ReleaseStringUTFChars(private_dir_path, path_chars);

        pthread_mutex_unlock(&g_lifecycle_mutex);

        return JNI_FALSE;

    }



    env->ReleaseStringUTFChars(private_dir_path, path_chars);

    g_runtime.initialized = 1;

    pthread_mutex_unlock(&g_lifecycle_mutex);



    LOGI("[JNI Bridge] Host application bootstrapper hooked successfully.");

    return JNI_TRUE;

}



// Native JNI Interface: Shuts down core engines and releases caches

JNIEXPORT void JNICALL

Java_com_your_app_bootstrap_NativeInterface_nativeShutdownRuntime(JNIEnv* env, jobject thiz) {

    pthread_mutex_lock(&g_lifecycle_mutex);

    if (!g_runtime.initialized) {

        pthread_mutex_unlock(&g_lifecycle_mutex);

        return;

    }



    // Close dynamic handles gracefully [25]

    if (g_runtime.handle) {

        dlclose(g_runtime.handle);

        g_runtime.handle = nullptr;

    }



    if (g_app_context_ref) {

        env->DeleteGlobalRef(g_app_context_ref);

        g_app_context_ref = nullptr;

    }



    pthread_key_delete(g_thread_key);

    g_runtime.initialized = 0;

    pthread_mutex_unlock(&g_lifecycle_mutex);



    LOGI("[JNI Bridge] Native runtime shutdown complete. Resources swept.");

}



} // extern "C"