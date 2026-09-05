#include "quickjs.h"

#include <string.h>

#include <stdlib.h>

#include "bluetooth_ipc_common.h"



extern int bt_start_le_scan();

extern int bt_stop_le_scan();

extern int bt_get_discovered_devices(BleScanResult *out_buffer, uint32_t max_count, uint32_t *out_count);

extern const char* bt_get_client_version();



// JS Export: bluetooth.startLeScan()

static JSValue js_bt_start_le_scan(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    int res = bt_start_le_scan();

    if (res != 0) {

        return JS_ThrowInternalError(ctx, "Failed to start BLE scanning. Status: %d", res);

    }

    return JS_UNDEFINED;

}



// JS Export: bluetooth.stopLeScan()

static JSValue js_bt_stop_le_scan(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    int res = bt_stop_le_scan();

    if (res != 0) {

        return JS_ThrowInternalError(ctx, "Failed to stop BLE scanning. Status: %d", res);

    }

    return JS_UNDEFINED;

}



// JS Export: bluetooth.getDiscoveredDevices()

// Returns native list: [{ mac: string, rssi: number, deviceClass: number, scanRecord: ArrayBuffer }]

static JSValue js_bt_get_discovered_devices(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    BleScanResult buffer[128];

    uint32_t out_count = 0;



    int status = bt_get_discovered_devices(buffer, 128, &out_count);

    if (status != 0) {

        return JS_ThrowInternalError(ctx, "Failed to query BLE hardware cache. Status: %d", status);

    }



    JSValue list = JS_NewArray(ctx);

    if (JS_IsException(list)) return list;



    for (uint32_t i = 0; i < out_count; i++) {

        JSValue device_obj = JS_NewObject(ctx);

        if (JS_IsException(device_obj)) continue;



        JS_SetPropertyStr(ctx, device_obj, "mac", JS_NewString(ctx, buffer[i].mac_address));

        JS_SetPropertyStr(ctx, device_obj, "rssi", JS_NewInt32(ctx, buffer[i].rssi));

        JS_SetPropertyStr(ctx, device_obj, "deviceClass", JS_NewInt32(ctx, buffer[i].device_class));



        // Package raw scan record data straight into a JavaScript ArrayBuffer with custom alloc allocators

        if (buffer[i].scan_record_len > 0) {

            uint8_t *ab_buf = malloc(buffer[i].scan_record_len);

            memcpy(ab_buf, buffer[i].scan_record, buffer[i].scan_record_len);



            JSValue ab = JS_NewArrayBuffer(ctx, ab_buf, buffer[i].scan_record_len,

                                          [](JSRuntime *rt, void *opaque, void *ptr) { free(ptr); },

                                          NULL, FALSE);

            JS_SetPropertyStr(ctx, device_obj, "scanRecord", ab);

        }



        JS_SetPropertyUint32(ctx, list, i, device_obj);

    }



    return list;

}



// JS Export: bluetooth.version()

static JSValue js_bt_version(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    return JS_NewString(ctx, bt_get_client_version());

}



static const JSCFunctionListEntry js_bt_funcs[] = {

    JS_CFUNC_DEF("startLeScan", 0, js_bt_start_le_scan),

    JS_CFUNC_DEF("stopLeScan", 0, js_bt_stop_le_scan),

    JS_CFUNC_DEF("getDiscoveredDevices", 0, js_bt_get_discovered_devices),

    JS_CFUNC_DEF("version", 0, js_bt_version)

};



static int js_bt_init(JSContext *ctx, JSModuleDef *m) {

    return JS_SetModuleExportList(ctx, m, js_bt_funcs, sizeof(js_bt_funcs) / sizeof(js_bt_funcs[0]));

}



JSModuleDef *js_init_module_bluetooth(JSContext *ctx, const char *module_name) {

    JSModuleDef *m = JS_NewCModule(ctx, module_name, js_bt_init);

    if (!m) return NULL;

    JS_AddModuleExportList(ctx, m, js_bt_funcs, sizeof(js_bt_funcs) / sizeof(js_bt_funcs[0]));

    return m;

}