#include "quickjs.h"

#include <sys/mman.h>

#include <unistd.h>

#include <fcntl.h>

#include "shm_common.h"



// Struct wrapping our mapped shared state context inside QuickJS

typedef struct {

    int shm_fd;

    SharedStateBuffer *state;

} QuickJSShmContext;



static void js_shm_finalizer(JSRuntime *rt, JSValue val) {

    QuickJSShmContext *ctx = JS_GetOpaque(val, 1); // Get opaque context class

    if (ctx) {

        if (ctx->state) {

            munmap(ctx->state, SHM_REGION_SIZE);

        }

        if (ctx->shm_fd >= 0) {

            close(ctx->shm_fd);

        }

        js_free_rt(rt, ctx);

    }

}



// Maps JavaScript: shm.readTelemetry() -> returns standard JS Object

static JSValue js_shm_read_telemetry(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    QuickJSShmContext *qjs_ctx = JS_GetOpaque2(ctx, this_val, 1);

    if (!qjs_ctx || !qjs_ctx->state) {

        return JS_ThrowInternalError(ctx, "Shared memory block is not mapped or initialized");

    }



    SharedStateBuffer *state = qjs_ctx->state;



    // Perform a lock-free read check

    if (atomic_load(&state->is_writing)) {

        return JS_NULL; // Busy, write in progress

    }



    JSValue obj = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, obj, "timestampNs", JS_NewInt64(ctx, state->data.timestamp_ns));

    JS_SetPropertyStr(ctx, obj, "wifiRssi", JS_NewInt32(ctx, state->data.wifi_signal_rssi));

    JS_SetPropertyStr(ctx, obj, "btCount", JS_NewInt32(ctx, state->data.bt_device_count));



    // Map Accelerometer array

    JSValue acc = JS_NewArray(ctx);

    for (int i = 0; i < 3; i++) {

        JS_SetPropertyUint32(ctx, acc, i, JS_NewFloat64(ctx, state->data.accelerometer[i]));

    }

    JS_SetPropertyStr(ctx, obj, "accelerometer", acc);



    // Map Gyroscope array

    JSValue gyro = JS_NewArray(ctx);

    for (int i = 0; i < 3; i++) {

        JS_SetPropertyUint32(ctx, gyro, i, JS_NewFloat64(ctx, state->data.gyroscope[i]));

    }

    JS_SetPropertyStr(ctx, obj, "gyroscope", gyro);



    return obj;

}



static const JSCFunctionListEntry js_shm_funcs[] = {

    JS_CFUNC_DEF("readTelemetry", 0, js_shm_read_telemetry),

};



// Initializer patterns for binding our library dynamically to QuickJS module registries

static int js_shm_init(JSContext *ctx, JSModuleDef *m) {

    return JS_SetModuleExportList(ctx, m, js_shm_funcs, sizeof(js_shm_funcs)/sizeof(js_shm_funcs));

}



JSModuleDef *js_init_module_shm(JSContext *ctx, const char *module_name) {

    JSModuleDef *m = JS_NewCModule(ctx, module_name, js_shm_init);

    if (!m) return NULL;

    JS_AddModuleExportList(ctx, m, js_shm_funcs, sizeof(js_shm_funcs)/sizeof(js_shm_funcs));

    return m;

}