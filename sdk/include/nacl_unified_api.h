/**

 * @file nacl_unified_api.h

 * @brief Unified C ABI Gateway for the Android Native Capability Library (NACL).

 */



#ifndef ANDROID_NATIVE_CAPABILITY_LIBRARY_UNIFIED_API_H

#define ANDROID_NATIVE_CAPABILITY_LIBRARY_UNIFIED_API_H



#include <stdint.h>

#include <stddef.h>



#if defined(__GNUC__) || defined(__clang__)

    #define NACL_EXPORT __attribute__((visibility("default")))

    #define NACL_IMPORT __attribute__((visibility("default")))

#else

    #define NACL_EXPORT

    #define NACL_IMPORT

#endif



#ifdef __cplusplus

extern "C" {

#endif



/**

 * @brief Identifiers for all 21 modular native capability libraries [2].

 */

typedef enum {

    NACL_MODULE_CORE            = 1,   /* libandroid_core.so       */

    NACL_MODULE_BLUETOOTH       = 2,   /* libbluetooth.so          */

    NACL_MODULE_WIFI            = 3,   /* libwifi.so               */

    NACL_MODULE_NFC             = 4,   /* libnfc.so                */

    NACL_MODULE_USB             = 5,   /* libusb.so                */

    NACL_MODULE_CAMERA          = 6,   /* libcamera.so             */

    NACL_MODULE_LOCATION        = 7,   /* liblocation.so           */

    NACL_MODULE_SENSORS         = 8,   /* libsensors.so            */

    NACL_MODULE_AUDIO           = 9,   /* libaudio.so              */

    NACL_MODULE_DISPLAY         = 10,  /* libdisplay.so            */

    NACL_MODULE_INPUT           = 11,  /* libinput.so              */

    NACL_MODULE_STORAGE         = 12,  /* libstorage.so            */

    NACL_MODULE_NETWORK         = 13,  /* libnetwork.so            */

    NACL_MODULE_PROCESS         = 14,  /* libprocess.so            */

    NACL_MODULE_IPC             = 15,  /* libipc.so                */

    NACL_MODULE_SYSTEM          = 16,  /* libsystem.so             */

    NACL_MODULE_POWER           = 17,  /* libpower.so              */

    NACL_MODULE_BATTERY         = 18,  /* libbattery.so            */

    NACL_MODULE_TELEPHONY       = 19,  /* libtelephony.so          */

    NACL_MODULE_MEDIA           = 20,  /* libmedia.so              */

    NACL_MODULE_SECURITY        = 21   /* libsecurity.so           */

} NaclModuleId;



/**

 * @brief Android Security and Boundary Status Codes [8, 23].

 */

typedef enum {

    NACL_STATUS_OK                   = 0,     /* Operation completed successfully */



    /* Dynamic Loader & Treble Errors [5, 26] */

    NACL_ERROR_MODULE_NOT_FOUND      = -101,  /* Target .so library not found */

    NACL_ERROR_TREBLE_LINKER_LIMIT   = -102,  /* Blocked by Project Treble linker policy */

    NACL_ERROR_SYMBOL_NOT_FOUND      = -103,  /* Missing dynamic function entry symbol */

    NACL_ERROR_OUT_OF_MEMORY         = -104,  /* Local heap allocation failure */



    /* Security & Privilege Boundary Violations [8] */

    NACL_ERROR_SELINUX_DENIED        = -201,  /* Blocked by Kernel SELinux MAC policies */

    NACL_ERROR_DAC_PERMISSION        = -202,  /* Blocked by Linux DAC (Missing GID/UID) */

    NACL_ERROR_SECCOMP_RESTRICTED    = -203,  /* Blocked by active app seccomp-bpf filter */

    NACL_ERROR_MISSING_RUNTIME_PERM  = -204,  /* Missing dynamic framework permission */

    NACL_ERROR_SIGNATURE_REQUIRED    = -205,  /* Restricted to Platform/Signature packages */



    /* Hardware & Operational Degradations [21] */

    NACL_ERROR_HARDWARE_ABSENT       = -301,  /* Physical peripheral not present */

    NACL_ERROR_EMULATOR_UNSUPPORTED  = -302,  /* Executing on emulator without mocks */

    NACL_ERROR_HAL_DISCONNECTED      = -303,  /* System service/HAL daemon not running */

    NACL_ERROR_IOCTL_FAILED          = -304,  /* Kernel driver IOCTL transaction failed */

    NACL_ERROR_TIMEOUT               = -305,  /* Subsystem interface timeout */



    /* Threading & IPC Faults [15] */

    NACL_ERROR_IPC_DISCONNECTED      = -401,  /* Unix Socket or Binder transaction broken */

    NACL_ERROR_SHM_QUEUE_FULL        = -402,  /* Shared memory circular queue saturated */

    NACL_ERROR_THREAD_ATTACH_FAILED  = -403,  /* Background thread JVM attach failure */

    NACL_ERROR_DEGRADED_MOCK_ACTIVE  = -501   /* Fell back to synthetic simulation mode */

} NaclStatus;



/**

 * @brief Subsystem Capability Flags.

 */

typedef enum {

    NACL_CAP_UNSUPPORTED    = 0,      /* Subsystem unavailable */

    NACL_CAP_SUPPORTED      = 1 << 0, /* Subsystem active and working */

    NACL_CAP_SANDBOXED      = 1 << 1, /* Bound within standard app sandbox */

    NACL_CAP_PRIVILEGED     = 1 << 2, /* Active via UID 2000 loopback client */

    NACL_CAP_DEGRADED_MOCK  = 1 << 3  /* Supported via simulation telemetry */

} NaclCapFlags;



/**

 * @brief Unified Event Types.

 */

typedef enum {

    NACL_EVENT_CORE_INIT            = 0x1000,

    NACL_EVENT_SENSOR_ACCEL         = 0x2001,

    NACL_EVENT_SENSOR_GYRO          = 0x2002,

    NACL_EVENT_SENSOR_MAG           = 0x2003,

    NACL_EVENT_BT_DISCOVERED        = 0x3001,

    NACL_EVENT_BT_GATT_READ         = 0x3002,

    NACL_EVENT_WIFI_P2P_PEER        = 0x4001,

    NACL_EVENT_NFC_TAG_DETECTED     = 0x5001,

    NACL_EVENT_USB_DEVICE_ATTACHED  = 0x6001,

    NACL_EVENT_AUDIO_BUFFER_READY   = 0x7001,

    NACL_EVENT_GPS_LOCATION         = 0x8001,

    NACL_EVENT_TELEPHONY_CELL_INFO  = 0x9001,

    NACL_EVENT_BATTERY_UPDATE       = 0xA001,

    NACL_EVENT_IPC_SOCKET_ERR       = 0xE001,

    NACL_EVENT_SECURITY_VIOLATION   = 0xF001

} NaclEventType;



/**

 * @brief Metadata payload for a subsystem module.

 */

typedef struct {

    uint32_t module_id;         /* Value from NaclModuleId */

    uint32_t cap_flags;         /* Value from NaclCapFlags */

    uint32_t api_level;         /* Detected AOSP SDK API Level */

    char name[32];              /* Friendly name of the subsystem */

    char library_path[128];     /* Resolved dynamic library load location */

} NaclSubsystemMeta;



/**

 * @brief Unified Asynchronous Event Frame [25].

 */

typedef struct {

    uint32_t module_id;         /* Originating NaclModuleId */

    uint32_t event_type;        /* NaclEventType identifier */

    uint64_t timestamp_ns;      /* Monotonic timestamp in nanoseconds */

    size_t payload_size;        /* Bytes contained within payload */

    const void* payload;        /* Thread-safe immutable pointer to data */

} NaclEventFrame;



/**

 * @brief Callback signature for dispatching background events to the RAD layer [25].

 */

typedef void (*NaclEventCallback)(const NaclEventFrame* frame);



/* --- Lifecycle & Orchestration APIs --- */



NACL_EXPORT NaclStatus nacl_init(const char* private_dir_path);

NACL_EXPORT NaclStatus nacl_shutdown(void);

NACL_EXPORT NaclStatus nacl_get_subsystem_meta(NaclModuleId module_id, NaclSubsystemMeta* out_meta);

NACL_EXPORT NaclStatus nacl_register_event_callback(NaclEventCallback callback);

NACL_EXPORT NaclStatus nacl_set_subsystem_mock_mode(NaclModuleId module_id, uint8_t enabled);

NACL_EXPORT int32_t    nacl_dispatch_command(NaclModuleId module_id, uint32_t cmd_id, const void* payload, size_t size);



#ifdef __cplusplus

}

#endif



#endif // ANDROID_NATIVE_CAPABILITY_LIBRARY_UNIFIED_API_H