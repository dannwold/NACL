#include "quickjs.h"

#include <string.h>

#include "automation_common.h"



// Define the JavaScript Class ID for AutoContext wrapping

static JSClassID js_auto_context_class_id;



// Native context structure mapped onto the JS Object memory

typedef struct {

    AutoContext ctx;

} JSAutoContext;



// Garbage collector finalizer to cleanly release bindings

static void js_auto_context_finalizer(JSRuntime *rt, JSValue val) {

    JSAutoContext *ac = JS_GetOpaque(val, js_auto_context_class_id);

    if (ac) {

        printf("[JS Binding] Releasing native AutoContext memory...\n");

        js_free_rt(rt, ac);

    }

}



// Map JavaScript: new AutoContext(adbPort, sessionHandle)

static JSValue js_auto_context_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {

    if (argc < 2) {

        return JS_ThrowTypeError(ctx, "Expected parameters: adbPort (number) and adbSessionHandle (external/pointer)");

    }



    JSAutoContext *ac = js_mallocz(ctx, sizeof(JSAutoContext));

    if (!ac) return JS_EXCEPTION;



    int32_t port = 0;

    JS_ToInt32(ctx, &port, argv[0]);

    ac->ctx.adb_port = port;



    // Attach the raw AdbSession pointer passed from JS

    uint64_t session_ptr = 0;

    JS_ToUint64(ctx, &session_ptr, argv[1]);

    ac->ctx.adb_session_ptr = (void *)session_ptr;

    ac->ctx.is_initialized = 1;



    JSValue obj = JS_NewObjectClass(ctx, js_auto_context_class_id);

    if (JS_IsException(obj)) {

        js_free(ctx, ac);

        return JS_EXCEPTION;

    }



    JS_SetOpaque(obj, ac);

    return obj;

}



// Map JavaScript method: autoCtx.ensureBluetoothEnabled()

static JSValue js_ensure_bluetooth_enabled(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    JSAutoContext *ac = JS_GetOpaque2(ctx, this_val, js_auto_context_class_id);

    if (!ac) return JS_EXCEPTION;



    extern int auto_ensure_bluetooth_enabled(AutoContext *ctx);

    int res = auto_ensure_bluetooth_enabled(&ac->ctx);



    return JS_NewInt32(ctx, res);

}



// Map JavaScript method: autoCtx.pairBluetoothDevice(macAddress)

static JSValue js_pair_bluetooth_device(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    JSAutoContext *ac = JS_GetOpaque2(ctx, this_val, js_auto_context_class_id);

    if (!ac) return JS_EXCEPTION;



    if (argc < 1 || !JS_IsString(argv[0])) {

        return JS_ThrowTypeError(ctx, "Expected MAC address string parameter");

    }



    const char *mac_address = JS_ToCString(ctx, argv[0]);

    if (!mac_address) return JS_EXCEPTION;



    extern int auto_pair_bluetooth_device(AutoContext *ctx, const char *mac_address);

    int res = auto_pair_bluetooth_device(&ac->ctx, mac_address);



    JS_FreeCString(ctx, mac_address);

    return JS_NewInt32(ctx, res);

}



// Map JavaScript method: autoCtx.establishWifiP2p(macAddress, mode)

static JSValue js_establish_wifi_p2p(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    JSAutoContext *ac = JS_GetOpaque2(ctx, this_val, js_auto_context_class_id);

    if (!ac) return JS_EXCEPTION;



    if (argc < 1 || !JS_IsString(argv[0])) {

        return JS_ThrowTypeError(ctx, "Expected MAC address string parameter");

    }



    const char *mac_address = JS_ToCString(ctx, argv[0]);

    const char *mode = NULL;



    if (argc > 1 && JS_IsString(argv[1])) {

        mode = JS_ToCString(ctx, argv[1]);

    }



    extern int auto_establish_wifi_p2p_connection(AutoContext *ctx, const char *peer_mac, const char *connection_mode);

    int res = auto_establish_wifi_p2p_connection(&ac->ctx, mac_address, mode);



    JS_FreeCString(ctx, mac_address);

    if (mode) JS_FreeCString(ctx, mode);



    return JS_NewInt32(ctx, res);

}



// Class definition structures

static const JSCFunctionListEntry js_auto_context_proto_funcs[] = {

    JS_CFUNC_DEF("ensureBluetoothEnabled", 0, js_ensure_bluetooth_enabled),

    JS_CFUNC_DEF("pairBluetoothDevice", 1, js_pair_bluetooth_device),

    JS_CFUNC_DEF("establishWifiP2p", 2, js_establish_wifi_p2p),

};



static JSClassDef js_auto_context_class = {

    "AutoContext",

    .finalizer = js_auto_context_finalizer,

};



// Module initialization callback

static int js_automation_init(JSContext *ctx, JSModuleDef *m) {

    JS_NewClassID(&js_auto_context_class_id);

    JS_NewClass(JS_GetRuntime(ctx), js_auto_context_class_id, &js_auto_context_class);



    JSValue proto = JS_NewObject(ctx);

    JS_SetPropertyFunctionList(ctx, proto, js_auto_context_proto_funcs, sizeof(js_auto_context_proto_funcs)/sizeof(js_auto_context_proto_funcs));

    JS_SetClassProto(ctx, js_auto_context_class_id, proto);



    JSValue ctor = JS_NewCFunction2(ctx, js_auto_context_ctor, "AutoContext", 2, JS_CFUNC_constructor, 0);

    JS_SetModuleExport(ctx, m, "AutoContext", ctor);



    return 0;

}



JSModuleDef *js_init_module_automation(JSContext *ctx, const char *module_name) {

    JSModuleDef *m = JS_NewCModule(ctx, module_name, js_automation_init);

    if (!m) return NULL;

    JS_AddModuleExport(ctx, m, "AutoContext");

    return m;

}