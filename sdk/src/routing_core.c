#include "routing_core.h"

#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <dlfcn.h>

#include <sys/system_properties.h>

#include <jni.h>

#include <pthread.h>



// Global route registry

static CapabilityRoute g_bluetooth_route = { "bluetooth_scan", 33, PATHWAY_UNINITIALIZED, nullptr };

static pthread_mutex_t g_routing_mutex = PTHREAD_MUTEX_INITIALIZER;

static int g_cached_api_level = -1;



// Thread-safe caching of device API Level via Bionic System Properties

static int get_android_api_level() {

    if (g_cached_api_level != -1) {

        return g_cached_api_level;

    }



    char sdk_ver_str[PROP_VALUE_MAX] = {0};

    int len = __system_property_get("ro.build.version.sdk", sdk_ver_str);

    if (len > 0) {

        g_cached_api_level = atoi(sdk_ver_str);

    } else {

        g_cached_api_level = 21; // Default to basic Android 5.0 Lollipop if query fails

    }

    return g_cached_api_level;

}



// ============================================================================

// IMPLEMENTATION AREA 1: RAW BINDER TRANSACTIONS (MAX SPEED / BYPASS)

// ============================================================================

static int execute_bluetooth_scan_via_binder(const uint8_t *payload, uint32_t payload_len, uint8_t *out_buffer, uint32_t *out_len) {

    printf("[Router] EXECUTING HIGH-SPEED BINDER INTERFACE (API Level >= 33)\n");



    // Dynamically open libbinder to avoid hard compilation linkages

    void *binder_lib = dlopen("libbinder.so", RTLD_NOW);

    if (!binder_lib) {

        fprintf(stderr, "[Router] Direct Binder access blocked or libbinder.so unresolvable\n");

        return -1;

    }



    // Resolve internal transaction parameters

    // In production, transaction IDs are retrieved from AOSP AIDL version offset indices

    printf("[Router] Sending raw Binder transactional payload to 'bluetooth' service manager...\n");



    // Simulated successful native transaction return

    const char *mock_response = "Binder Transaction Success: BLE Scanning Hardware Initiated Directly";

    uint32_t resp_len = strlen(mock_response);

    if (out_buffer && out_len && *out_len >= resp_len) {

        memcpy(out_buffer, mock_response, resp_len);

        *out_len = resp_len;

    }



    dlclose(binder_lib);

    return 0;

}



// ============================================================================

// IMPLEMENTATION AREA 2: JNI JVM ATTACHMENT LOOP (STABLE FALLBACK)

// ============================================================================

// Reference pointer to the running ART virtual machine instance

static JavaVM *g_jvm = nullptr;



static int execute_bluetooth_scan_via_jni(const uint8_t *payload, uint32_t payload_len, uint8_t *out_buffer, uint32_t *out_len) {

    printf("[Router] DETECTED API LEVEL < 33 OR COMPATIBILITY BLOCKS BINDER. CALLING JNI FALLBACK...\n");



    if (!g_jvm) {

        // Locate active ART JavaVM instances running inside the process space

        jsize vm_count = 0;

        if (JNI_GetCreatedJavaVMs(&g_jvm, 1, &vm_count) != JNI_OK || vm_count == 0) {

            fprintf(stderr, "[Router] Failed to attach: No JVM context found running in current process\n");

            return -1;

        }

    }



    JNIEnv *env = nullptr;

    bool thread_attached = false;

    int env_res = g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6);



    if (env_res == JNI_EDETACHED) {

        if (g_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) {

            fprintf(stderr, "[Router] Failed to register background thread to JVM context\n");

            return -1;

        }

        thread_attached = true;

    }



    // Standard JNI transaction sequence calling Java Framework services

    // 1. Locate BluetoothAdapter Java Class

    // 2. Call default adapter methods to start scans

    printf("[Router] Thread successfully attached to JVM. Resolving JNI signatures...\n");



    const char *fallback_response = "JNI Callback Success: Target framework BluetoothAdapter triggered";

    uint32_t fallback_len = strlen(fallback_response);

    if (out_buffer && out_len && *out_len >= fallback_len) {

        memcpy(out_buffer, fallback_response, fallback_len);

        *out_len = fallback_len;

    }



    // Detach context cleanly if thread was allocated dynamically during call

    if (thread_attached) {

        g_jvm->DetachCurrentThread();

    }



    return 0;

}



// ============================================================================

// CORE ROUTER LIFECYCLE COORDINATION

// ============================================================================

int initialize_routing_engine() {

    pthread_mutex_lock(&g_routing_mutex);



    int api_level = get_android_api_level();

    printf("[Router] Bootstrapping Routing Engine. Target Device Android API Level: %d\n", api_level);



    // Initialize Bluetooth Route adaptively

    if (api_level >= g_bluetooth_route.min_sdk_version) {

        g_bluetooth_route.active_pathway = PATHWAY_BINDER;

        g_bluetooth_route.execute = execute_bluetooth_scan_via_binder;

        printf("[Router] Registered optimal PATHWAY_BINDER for capability: %s\n", g_bluetooth_route.capability_name);

    } else {

        g_bluetooth_route.active_pathway = PATHWAY_JNI;

        g_bluetooth_route.execute = execute_bluetooth_scan_via_jni;

        printf("[Router] Registered JNI fallback PATHWAY_JNI for capability: %s\n", g_bluetooth_route.capability_name);

    }



    pthread_mutex_unlock(&g_routing_mutex);

    return 0;

}



ExecutionPathway resolve_capability_pathway(const char *capability) {

    if (strcmp(capability, g_bluetooth_route.capability_name) == 0) {

        return g_bluetooth_route.active_pathway;

    }

    return PATHWAY_UNSUPPORTED;

}



int dispatch_hardware_command(const char *capability, const uint8_t *payload, uint32_t payload_len, uint8_t *out_buffer, uint32_t *out_len) {

    HardwareCommandFunc execute_target = nullptr;



    pthread_mutex_lock(&g_routing_mutex);

    if (strcmp(capability, g_bluetooth_route.capability_name) == 0) {

        execute_target = g_bluetooth_route.execute;

    }

    pthread_mutex_unlock(&g_routing_mutex);



    if (!execute_target) {

        fprintf(stderr, "[Router] Error: Capability '%s' is not registered or supported on this system\n", capability);

        return -1;

    }



    // Jump directly to the registered function pointer (O(1) execution timing)

    return execute_target(payload, payload_len, out_buffer, out_len);

}