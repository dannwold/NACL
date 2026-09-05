#include "quickjs.h"

#include <string.h>

#include <stdio.h>

#include "sensor_ipc_common.h"



typedef struct {

    int socket_fd;

    pthread_t thread;

    volatile int is_running;

    void (*callback)(const SensorDataEvent *event);

} SensorClientSession;



extern SensorClientSession* start_sensor_stream(void (*callback)(const SensorDataEvent *event));

extern void stop_sensor_stream(SensorClientSession *session);



static JSContext *g_js_ctx = NULL;

static JSValue g_js_callback = {0};

static SensorClientSession *g_session = NULL;



static void native_sensor_cb(const SensorDataEvent *event) {

    if (!g_js_ctx || JS_IsUndefined(g_js_callback)) return;



    // Convert raw C binary structures to QuickJS objects inside registers

    JSValue obj = JS_NewObject(g_js_ctx);

    JS_SetPropertyStr(g_js_ctx, obj, "type", JS_NewInt32(g_js_ctx, event->sensor_type));

    JS_SetPropertyStr(g_js_ctx, obj, "timestamp", JS_NewBigInt64(g_js_ctx, event->timestamp));

    JS_SetPropertyStr(g_js_ctx, obj, "x", JS_NewFloat64(g_js_ctx, event->x));

    JS_SetPropertyStr(g_js_ctx, obj, "y", JS_NewFloat64(g_js_ctx, event->y));

    JS_SetPropertyStr(g_js_ctx, obj, "z", JS_NewFloat64(g_js_ctx, event->z));

    JS_SetPropertyStr(g_js_ctx, obj, "accuracy", JS_NewFloat64(g_js_ctx, event->accuracy));



    JSValue ret = JS_Call(g_js_ctx, g_js_callback, JS_UNDEFINED, 1, &obj);

    JS_FreeValue(g_js_ctx, obj);

    JS_FreeValue(g_js_ctx, ret);

}



static JSValue js_sensors_start(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {

        return JS_ThrowTypeError(ctx, "Callback parameter required");

    }



    if (g_session != NULL) {

        return JS_ThrowInternalError(ctx, "Sensor streaming already initialized");

    }



    g_js_ctx = ctx;

    g_js_callback = JS_DupValue(ctx, argv[0]);



    g_session = start_sensor_stream(native_sensor_cb);

    if (!g_session) {

        JS_FreeValue(ctx, g_js_callback);

        g_js_callback = JS_UNDEFINED;

        return JS_ThrowInternalError(ctx, "Failed connecting to local sensor daemon process");

    }



    return JS_UNDEFINED;

}



static JSValue js_sensors_stop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    if (!g_session) return JS_UNDEFINED;



    stop_sensor_stream(g_session);

    g_session = NULL;



    JS_FreeValue(ctx, g_js_callback);

    g_js_callback = JS_UNDEFINED;

    g_js_ctx = NULL;



    return JS_UNDEFINED;

}



static const JSCFunctionListEntry js_sensors_funcs[] = {

    JS_CFUNC_DEF("start", 1, js_sensors_start),

    JS_CFUNC_DEF("stop", 0, js_sensors_stop),

};



static int js_sensors_init(JSContext *ctx, JSModuleDef *m) {

    return JS_SetModuleExportList(ctx, m, js_sensors_funcs, sizeof(js_sensors_funcs)/sizeof(js_sensors_funcs));

}



JSModuleDef *js_init_module_sensors(JSContext *ctx, const char *module_name) {

    JSModuleDef *m = JS_NewCModule(ctx, module_name, js_sensors_init);

    if (!m) return NULL;

    JS_AddModuleExportList(ctx, m, js_sensors_funcs, sizeof(js_sensors_funcs)/sizeof(js_sensors_funcs));

    return m;

}