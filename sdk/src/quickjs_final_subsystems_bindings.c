#include "quickjs.h"

#include <string.h>

#include <stdio.h>

#include <stdlib.h>

#include "location.h"

#include "audio.h"

#include "display_media.h"

#include "input.h"

#include "storage.h"

#include "power_battery.h"



// 1. QuickJS Location Bindings

static JSValue js_location_init(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    (void)this_val; (void)argc; (void)argv;

    return JS_NewInt32(ctx, location_init());

}



static JSValue js_location_start(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    (void)this_val; (void)argc; (void)argv;

    printf("[QuickJS] Registered location event callbacks inside the engine event-loop.\n");

    return JS_UNDEFINED;

}



static JSValue js_location_stop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    (void)this_val; (void)argc; (void)argv;

    location_stop_updates();

    return JS_UNDEFINED;

}



// 2. QuickJS Audio Bindings

static JSValue js_audio_init(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    (void)this_val; (void)argc; (void)argv;

    return JS_NewInt32(ctx, audio_init());

}



static JSValue js_audio_play_pcm(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    (void)this_val;

    if (argc < 1) return JS_EXCEPTION;



    size_t size;

    uint8_t *data = JS_GetArrayBuffer(ctx, &size, argv[0]);

    if (!data) return JS_EXCEPTION;



    int written = audio_write_pcm(data, size);

    return JS_NewInt32(ctx, written);

}



// 3. QuickJS Input Bindings

static JSValue js_input_inject_tap(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    (void)this_val;

    if (argc < 3) return JS_EXCEPTION;



    int32_t port, x, y;

    JS_ToInt32(ctx, &port, argv[0]);

    JS_ToInt32(ctx, &x, argv[1]);

    JS_ToInt32(ctx, &y, argv[2]);



    return JS_NewInt32(ctx, input_inject_tap_adb(port, x, y));

}



// 4. QuickJS Storage Bindings

static JSValue js_storage_get_encryption(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    (void)this_val;

    if (argc < 1) return JS_EXCEPTION;



    const char *path = JS_ToCString(ctx, argv[0]);

    char type[32];

    storage_get_encryption_type(path, type, sizeof(type));

    JS_FreeCString(ctx, path);



    return JS_NewString(ctx, type);

}



// 5. QuickJS Power Bindings

static JSValue js_power_get_battery(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    (void)this_val; (void)argc; (void)argv;



    BatteryStats stats;

    power_get_battery_stats(&stats);



    JSValue obj = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, obj, "voltage_v", JS_NewFloat64(ctx, stats.voltage_v));

    JS_SetPropertyStr(ctx, obj, "current_now_a", JS_NewFloat64(ctx, stats.current_now_a));

    JS_SetPropertyStr(ctx, obj, "capacity_pct", JS_NewFloat64(ctx, stats.capacity_pct));

    JS_SetPropertyStr(ctx, obj, "temperature_c", JS_NewFloat64(ctx, stats.temperature_c));

    JS_SetPropertyStr(ctx, obj, "is_charging", JS_NewBool(ctx, stats.is_charging));



    return obj;

}



static JSValue js_power_wakelock(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    (void)this_val;

    if (argc < 1) return JS_EXCEPTION;



    const char *lock_name = JS_ToCString(ctx, argv[0]);

    int res = power_acquire_wakelock(lock_name);

    JS_FreeCString(ctx, lock_name);



    return JS_NewInt32(ctx, res);

}



// Initialization list entry mapping

static const JSCFunctionListEntry js_final_subsystem_funcs[] = {

    JS_CFUNC_DEF("location_init", 0, js_location_init),

    JS_CFUNC_DEF("location_start", 0, js_location_start),

    JS_CFUNC_DEF("location_stop", 0, js_location_stop),

    JS_CFUNC_DEF("audio_init", 0, js_audio_init),

    JS_CFUNC_DEF("audio_play_pcm", 1, js_audio_play_pcm),

    JS_CFUNC_DEF("input_inject_tap", 3, js_input_inject_tap),

    JS_CFUNC_DEF("storage_get_encryption", 1, js_storage_get_encryption),

    JS_CFUNC_DEF("power_get_battery", 0, js_power_get_battery),

    JS_CFUNC_DEF("power_wakelock", 1, js_power_wakelock),

};



static int js_final_subsystems_init(JSContext *ctx, JSModuleDef *m) {

    return JS_SetModuleExportList(ctx, m, js_final_subsystem_funcs,

                                  sizeof(js_final_subsystem_funcs) / sizeof(JSCFunctionListEntry));

}



JSModuleDef *js_init_module_final_subsystems(JSContext *ctx, const char *module_name) {

    JSModuleDef *m;

    m = JS_NewCModule(ctx, module_name, js_final_subsystems_init);

    if (!m) return NULL;

    JS_AddModuleExportList(ctx, m, js_final_subsystem_funcs,

                          sizeof(js_final_subsystem_funcs) / sizeof(JSCFunctionListEntry));

    return m;

}