#include "android_core.h"

#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <dlfcn.h>

#include <pthread.h>

#include <sys/system_properties.h>



// Concrete definition of the opaque NaclContext structure

struct NaclContext {

    pthread_mutex_t mutex;

    char last_error[256];

    NaclModuleEntry modules[NACL_MODULE_COUNT];

    int android_sdk_level;

};



// Static registry of libraries

static const NaclModuleEntry s_module_templates[NACL_MODULE_COUNT] = {

    { NACL_MODULE_CORE,      "core",      "libandroid_core.so",                             NULL, false },

    { NACL_MODULE_BLUETOOTH, "bluetooth", "/data/local/tmp/sdk/lib/libbluetooth_client.so", NULL, false },

    { NACL_MODULE_WIFI,      "wifi",      "/data/local/tmp/sdk/lib/libwifi_client.so",      NULL, false },

    { NACL_MODULE_SENSORS,   "sensors",   "/data/local/tmp/sdk/lib/libsensors_client.so",   NULL, false },

    { NACL_MODULE_LOCATION,  "location",  "/data/local/tmp/sdk/lib/liblocation_client.so",  NULL, false },

    { NACL_MODULE_IPC,       "ipc",       "/data/local/tmp/sdk/lib/libipc_client.so",       NULL, false },

    { NACL_MODULE_SYSTEM,    "system",    "/data/local/tmp/sdk/lib/libsystem_client.so",    NULL, false }

};



// Internal function to extract SDK level from system properties (AOSP specific)

static int query_sdk_level() {

    char sdk_ver_str[PROP_VALUE_MAX] = {0};

    int len = __system_property_get("ro.build.version.sdk", sdk_ver_str);

    if (len > 0) {

        return atoi(sdk_ver_str);

    }

    return 0; // Unknown/Error

}



NACL_EXPORT NaclContext* nacl_core_initialize(NaclResult *out_res) {

    NaclContext *ctx = (NaclContext *)malloc(sizeof(NaclContext));

    if (!ctx) {

        if (out_res) *out_res = NACL_ERROR_NO_MEMORY;

        return NULL;

    }



    if (pthread_mutex_init(&ctx->mutex, NULL) != 0) {

        free(ctx);

        if (out_res) *out_res = NACL_ERROR_UNKNOWN;

        return NULL;

    }



    // Initialize modules with templates

    memcpy(ctx->modules, s_module_templates, sizeof(s_module_templates));

    ctx->last_error[0] = '\0';

    ctx->android_sdk_level = query_sdk_level();



    // Mark core module as self-loaded (it's this library itself)

    ctx->modules[NACL_MODULE_CORE].is_loaded = true;

    ctx->modules[NACL_MODULE_CORE].handle = RTLD_DEFAULT;



    if (out_res) *out_res = NACL_SUCCESS;

    return ctx;

}



NACL_EXPORT void nacl_core_shutdown(NaclContext *ctx) {

    if (!ctx) return;



    pthread_mutex_lock(&ctx->mutex);

    // Unload all modules (except core)

    for (int i = 1; i < NACL_MODULE_COUNT; ++i) {

        if (ctx->modules[i].is_loaded && ctx->modules[i].handle) {

            dlclose(ctx->modules[i].handle);

            ctx->modules[i].handle = NULL;

            ctx->modules[i].is_loaded = false;

        }

    }

    pthread_mutex_unlock(&ctx->mutex);



    pthread_mutex_destroy(&ctx->mutex);

    free(ctx);

}



NACL_EXPORT NaclVersion nacl_core_get_version() {

    NaclVersion ver = {

        NACL_CORE_VERSION_MAJOR,

        NACL_CORE_VERSION_MINOR,

        NACL_CORE_VERSION_PATCH,

        "STABLE-NACL"

    };

    return ver;

}



NACL_EXPORT int nacl_core_get_android_sdk_level() {

    return query_sdk_level();

}



NACL_EXPORT bool nacl_core_is_api_supported(int min_api_level) {

    return query_sdk_level() >= min_api_level;

}



NACL_EXPORT NaclResult nacl_core_load_module(NaclContext *ctx, NaclModuleType module_type) {

    if (!ctx || module_type < 0 || module_type >= NACL_MODULE_COUNT) {

        return NACL_ERROR_INVALID_ARG;

    }



    pthread_mutex_lock(&ctx->mutex);



    if (ctx->modules[module_type].is_loaded) {

        pthread_mutex_unlock(&ctx->mutex);

        return NACL_SUCCESS; // Already loaded

    }



    const char *path = ctx->modules[module_type].so_path;

    // Load dynamically via dlopen

    void *handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);

    if (!handle) {

        const char *err = dlerror();

        snprintf(ctx->last_error, sizeof(ctx->last_error), "Failed to load %s: %s", path, err ? err : "unknown");

        pthread_mutex_unlock(&ctx->mutex);

        return NACL_ERROR_NOT_FOUND;

    }



    ctx->modules[module_type].handle = handle;

    ctx->modules[module_type].is_loaded = true;



    pthread_mutex_unlock(&ctx->mutex);

    return NACL_SUCCESS;

}



NACL_EXPORT NaclResult nacl_core_unload_module(NaclContext *ctx, NaclModuleType module_type) {

    if (!ctx || module_type <= 0 || module_type >= NACL_MODULE_COUNT) {

        return NACL_ERROR_INVALID_ARG; // Cannot unload core itself

    }



    pthread_mutex_lock(&ctx->mutex);



    if (!ctx->modules[module_type].is_loaded) {

        pthread_mutex_unlock(&ctx->mutex);

        return NACL_SUCCESS; // Already unloaded

    }



    if (ctx->modules[module_type].handle) {

        dlclose(ctx->modules[module_type].handle);

        ctx->modules[module_type].handle = NULL;

    }

    ctx->modules[module_type].is_loaded = false;



    pthread_mutex_unlock(&ctx->mutex);

    return NACL_SUCCESS;

}



NACL_EXPORT void* nacl_core_get_symbol(NaclContext *ctx, NaclModuleType module_type, const char *symbol_name) {

    if (!ctx || module_type < 0 || module_type >= NACL_MODULE_COUNT || !symbol_name) {

        return NULL;

    }



    pthread_mutex_lock(&ctx->mutex);



    if (!ctx->modules[module_type].is_loaded) {

        // Attempt automatic load

        pthread_mutex_unlock(&ctx->mutex);

        if (nacl_core_load_module(ctx, module_type) != NACL_SUCCESS) {

            return NULL;

        }

        pthread_mutex_lock(&ctx->mutex);

    }



    void *sym = dlsym(ctx->modules[module_type].handle, symbol_name);

    if (!sym) {

        snprintf(ctx->last_error, sizeof(ctx->last_error), "Symbol %s not found: %s", symbol_name, dlerror());

    }



    pthread_mutex_unlock(&ctx->mutex);

    return sym;

}



NACL_EXPORT bool nacl_core_is_module_loaded(NaclContext *ctx, NaclModuleType module_type) {

    if (!ctx || module_type < 0 || module_type >= NACL_MODULE_COUNT) {

        return false;

    }

    return ctx->modules[module_type].is_loaded;

}



NACL_EXPORT const char* nacl_core_get_last_error(NaclContext *ctx) {

    if (!ctx) return "Null Context Pointer";

    return ctx->last_error;

}



NACL_EXPORT void nacl_core_set_error(NaclContext *ctx, const char *error_msg) {

    if (!ctx || !error_msg) return;

    pthread_mutex_lock(&ctx->mutex);

    snprintf(ctx->last_error, sizeof(ctx->last_error), "%s", error_msg);

    pthread_mutex_unlock(&ctx->mutex);

}



NACL_EXPORT int nacl_core_get_system_property(const char *prop_name, char *out_value, uint32_t max_len) {

    if (!prop_name || !out_value) return -1;

    char temp[PROP_VALUE_MAX] = {0};

    int len = __system_property_get(prop_name, temp);

    if (len > 0) {

        snprintf(out_value, max_len, "%s", temp);

        return len;

    }

    return -1;

}