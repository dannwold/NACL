#ifndef ANDROID_CORE_H

#define ANDROID_CORE_H



#include <stdint.h>

#include <stdbool.h>



#ifdef __cplusplus

extern "C" {

#endif



// Visibility macros for stable ELF symbol exporting

#define NACL_EXPORT __attribute__((visibility("default")))

#define NACL_LOCAL  __attribute__((visibility("hidden")))



// Versioning Definitions

#define NACL_CORE_VERSION_MAJOR 1

#define NACL_CORE_VERSION_MINOR 0

#define NACL_CORE_VERSION_PATCH 0



typedef struct {

    uint8_t major;

    uint8_t minor;

    uint8_t patch;

    const char *build_meta;

} NaclVersion;



// Capability Status Codes

typedef enum {

    NACL_SUCCESS           = 0,

    NACL_ERROR_UNKNOWN     = -1,

    NACL_ERROR_NOT_FOUND   = -2,

    NACL_ERROR_INVALID_ARG = -3,

    NACL_ERROR_PERMISSION  = -4,

    NACL_ERROR_UNSUPPORTED = -5,

    NACL_ERROR_NO_MEMORY   = -6,

    NACL_ERROR_IO          = -7,

    NACL_ERROR_BUSY        = -8

} NaclResult;



// Core Runtime Context Structure (Opaque Handle Pattern)

typedef struct NaclContext NaclContext;



// Module Registry Types

typedef enum {

    NACL_MODULE_CORE      = 0,

    NACL_MODULE_BLUETOOTH = 1,

    NACL_MODULE_WIFI      = 2,

    NACL_MODULE_SENSORS   = 3,

    NACL_MODULE_LOCATION  = 4,

    NACL_MODULE_IPC       = 5,

    NACL_MODULE_SYSTEM    = 6,

    NACL_MODULE_COUNT

} NaclModuleType;



typedef struct {

    NaclModuleType type;

    const char *name;

    const char *so_path;

    void *handle; // Handle returned by dlopen

    bool is_loaded;

} NaclModuleEntry;



// --- Runtime Initialization & Lifecycle ---

NACL_EXPORT NaclContext* nacl_core_initialize(NaclResult *out_res);

NACL_EXPORT void nacl_core_shutdown(NaclContext *ctx);



// --- Versioning and Discovery ---

NACL_EXPORT NaclVersion nacl_core_get_version(void);

NACL_EXPORT bool nacl_core_is_api_supported(int min_api_level);

NACL_EXPORT int nacl_core_get_android_sdk_level(void);



// --- Dynamic Module Loading (libdl wrapper) ---

NACL_EXPORT NaclResult nacl_core_load_module(NaclContext *ctx, NaclModuleType module_type);

NACL_EXPORT NaclResult nacl_core_unload_module(NaclContext *ctx, NaclModuleType module_type);

NACL_EXPORT void* nacl_core_get_symbol(NaclContext *ctx, NaclModuleType module_type, const char *symbol_name);

NACL_EXPORT bool nacl_core_is_module_loaded(NaclContext *ctx, NaclModuleType module_type);



// --- Structured Error Subsystem ---

NACL_EXPORT const char* nacl_core_get_last_error(NaclContext *ctx);

NACL_EXPORT void nacl_core_set_error(NaclContext *ctx, const char *error_msg);



// --- System Properties Interface (Bionic Libc) ---

NACL_EXPORT int nacl_core_get_system_property(const char *prop_name, char *out_value, uint32_t max_len);



#ifdef __cplusplus

}

#endif



#endif // ANDROID_CORE_H