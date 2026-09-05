#include "quickjs.h"

#include "android_core.h"

#include <string.h>



// Helper to extract the NaclContext from a JSValue (stored as an opaque object)

static NaclContext *js_get_core_context(JSContext *ctx, JSValueConst obj) {

    return (NaclContext *)JS_GetOpaque(obj, 1); // 1 is class ID for NaclContext

}



// Global Class ID for NaclContext mapping

static JSClassID js_nacl_context_class_id;



// Finalizer called when the JS garbage collector reclaims the Context object

static void js_nacl_context_finalizer(JSRuntime *rt, JSValue val) {

    NaclContext *ctx = JS_GetOpaque(val, js_nacl_context_class_id);

    if (ctx) {

        nacl_core_shutdown(ctx);

    }

}



static JSClassDef js_nacl_context_class = {

    "NaclContext",

    .finalizer = js_nacl_context_finalizer,

};



// JS: android.init() -> returns wrapped NaclContext object

static JSValue js_android_init(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    NaclResult res;

    NaclContext *nacl_ctx = nacl_core_initialize(&res);

    if (!nacl_ctx) {

        return JS_ThrowInternalError(ctx, "Failed to initialize Android Native Core Context (Result: %d)", res);

    }



    JSValue obj = JS_NewObjectClass(ctx, js_nacl_context_class_id);

    if (JS_IsException(obj)) {

        nacl_core_shutdown(nacl_ctx);

        return obj;

    }



    JS_SetOpaque(obj, nacl_ctx);

    return obj;

}



// JS: core.getSdkVersion()

static JSValue js_core_get_sdk_version(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    int sdk = nacl_core_get_android_sdk_level();

    return JS_NewInt32(ctx, sdk);

}



// JS: core.getSystemProperty(name)

static JSValue js_core_get_system_property(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    if (argc < 1 || !JS_IsString(argv[0])) {

        return JS_ThrowTypeError(ctx, "Expected system property name (string)");

    }



    const char *prop_name = JS_ToCString(ctx, argv[0]);

    if (!prop_name) return JS_EXCEPTION;



    char val_buf[256] = {0};

    int len = nacl_core_get_system_property(prop_name, val_buf, sizeof(val_buf));

    JS_FreeCString(ctx, prop_name);



    if (len < 0) {

        return JS_NULL;

    }



    return JS_NewString(ctx, val_buf);

}



// JS: core.loadModule(moduleName)

static JSValue js_core_load_module(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    NaclContext *nacl_ctx = js_get_core_context(ctx, this_val);

    if (!nacl_ctx) {

        return JS_ThrowInternalError(ctx, "Invalid context, instance is detached");

    }



    if (argc < 1 || !JS_IsString(argv[0])) {

        return JS_ThrowTypeError(ctx, "Expected module type (string)");

    }



    const char *mod_str = JS_ToCString(ctx, argv[0]);

    if (!mod_str) return JS_EXCEPTION;



    NaclModuleType target_type = -1;

    if (strcmp(mod_str, "bluetooth") == 0) target_type = NACL_MODULE_BLUETOOTH;

    else if (strcmp(mod_str, "wifi") == 0) target_type = NACL_MODULE_WIFI;

    else if (strcmp(mod_str, "sensors") == 0) target_type = NACL_MODULE_SENSORS;

    else if (strcmp(mod_str, "location") == 0) target_type = NACL_MODULE_LOCATION;

    else if (strcmp(mod_str, "ipc") == 0) target_type = NACL_MODULE_IPC;

    else if (strcmp(mod_str, "system") == 0) target_type = NACL_MODULE_SYSTEM;



    JS_FreeCString(ctx, mod_str);



    if (target_type == -1) {

        return JS_ThrowRangeError(ctx, "Unknown native module identifier");

    }



    NaclResult res = nacl_core_load_module(nacl_ctx, target_type);

    if (res != NACL_SUCCESS) {

        return JS_ThrowInternalError(ctx, "Failed loading module: %s", nacl_core_get_last_error(nacl_ctx));

    }



    return JS_TRUE;

}



// JS: core.isLoaded(moduleName)

static JSValue js_core_is_loaded(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    NaclContext *nacl_ctx = js_get_core_context(ctx, this_val);

    if (!nacl_ctx) return JS_FALSE;



    if (argc < 1 || !JS_IsString(argv[0])) return JS_FALSE;



    const char *mod_str = JS_ToCString(ctx, argv[0]);

    if (!mod_str) return JS_EXCEPTION;



    bool loaded = false;

    if (strcmp(mod_str, "bluetooth") == 0) loaded = nacl_core_is_module_loaded(nacl_ctx, NACL_MODULE_BLUETOOTH);

    else if (strcmp(mod_str, "wifi") == 0) loaded = nacl_core_is_module_loaded(nacl_ctx, NACL_MODULE_WIFI);

    else if (strcmp(mod_str, "sensors") == 0) loaded = nacl_core_is_module_loaded(nacl_ctx, NACL_MODULE_SENSORS);

    else if (strcmp(mod_str, "location") == 0) loaded = nacl_core_is_module_loaded(nacl_ctx, NACL_MODULE_LOCATION);

    else if (strcmp(mod_str, "ipc") == 0) loaded = nacl_core_is_module_loaded(nacl_ctx, NACL_MODULE_IPC);

    else if (strcmp(mod_str, "system") == 0) loaded = nacl_core_is_module_loaded(nacl_ctx, NACL_MODULE_SYSTEM);



    JS_FreeCString(ctx, mod_str);

    return JS_NewBool(ctx, loaded);

}



// Methods exposed on the NaclContext prototype (instance level)

static const JSCFunctionListEntry js_nacl_context_proto_funcs[] = {

    JS_CFUNC_DEF("getSdkVersion", 0, js_core_get_sdk_version),

    JS_CFUNC_DEF("getSystemProperty", 1, js_core_get_system_property),

    JS_CFUNC_DEF("loadModule", 1, js_core_load_module),

    JS_CFUNC_DEF("isLoaded", 1, js_core_is_loaded),

};



// Module setup exports for QuickJS

static const JSCFunctionListEntry js_android_core_funcs[] = {

    JS_CFUNC_DEF("init", 0, js_android_init),

};



static int js_android_core_init(JSContext *ctx, JSModuleDef *m) {

    // Register class ID and definition in Runtime context

    JS_NewClassID(&js_nacl_context_class_id);

    JS_NewClass(JS_GetRuntime(ctx), js_nacl_context_class_id, &js_nacl_context_class);



    // Create prototype and inject methods

    JSValue proto = JS_NewObject(ctx);

    JS_SetPropertyFunctionList(ctx, proto, js_nacl_context_proto_funcs, sizeof(js_nacl_context_proto_funcs)/sizeof(js_nacl_context_proto_funcs));

    JS_SetClassProto(ctx, js_nacl_context_class_id, proto);



    // Export primary initialization module hooks

    return JS_SetModuleExportList(ctx, m, js_android_core_funcs, sizeof(js_android_core_funcs)/sizeof(js_android_core_funcs));

}



// Entrypoint called when JS imports the dynamic library module

JSModuleDef *js_init_module_android_core(JSContext *ctx, const char *module_name) {

    JSModuleDef *m = JS_NewCModule(ctx, module_name, js_android_core_init);

    if (!m) return NULL;

    JS_AddModuleExportList(ctx, m, js_android_core_funcs, sizeof(js_android_core_funcs)/sizeof(js_android_core_funcs));

    return m;

}