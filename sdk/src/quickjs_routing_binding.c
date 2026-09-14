#include "quickjs.h"

#include "routing_core.h"

#include <string.h>



// JavaScript Interface Map: router.execute("bluetooth_scan")

static JSValue js_router_execute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    if (argc < 1) {

        return JS_ThrowTypeError(ctx, "Expected at least 1 argument: capability name");

    }



    const char *capability = JS_ToCString(ctx, argv[0]);

    if (!capability) {

        return JS_EXCEPTION;

    }



    uint8_t out_buf[1024];

    uint32_t out_len = sizeof(out_buf);



    // Call our C runtime routing pipeline

    int status = dispatch_hardware_command(capability, NULL, 0, out_buf, &out_len);

    JS_FreeCString(ctx, capability);



    if (status != 0) {

        return JS_ThrowInternalError(ctx, "Dynamic routing dispatch execution failed");

    }



    return JS_NewStringLen(ctx, (char *)out_buf, out_len);

}



// JavaScript Interface Map: router.getPathway("bluetooth_scan")

static JSValue js_router_get_pathway(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    if (argc < 1) {

        return JS_ThrowTypeError(ctx, "Expected 1 argument: capability name");

    }



    const char *capability = JS_ToCString(ctx, argv[0]);

    if (!capability) {

        return JS_EXCEPTION;

    }



    ExecutionPathway path = resolve_capability_pathway(capability);

    JS_FreeCString(ctx, capability);



    switch (path) {

        case PATHWAY_BINDER:

            return JS_NewString(ctx, "BINDER");

        case PATHWAY_JNI:

            return JS_NewString(ctx, "JNI_FALLBACK");

        case PATHWAY_UNSUPPORTED:

            return JS_NewString(ctx, "UNSUPPORTED");

        default:

            return JS_NewString(ctx, "UNINITIALIZED");

    }

}



static const JSCFunctionListEntry js_router_funcs[] = {

    JS_CFUNC_DEF("execute", 1, js_router_execute),

    JS_CFUNC_DEF("getPathway", 1, js_router_get_pathway),

};



static int js_router_init(JSContext *ctx, JSModuleDef *m) {

    return JS_SetModuleExportList(ctx, m, js_router_funcs, sizeof(js_router_funcs)/sizeof(js_router_funcs));

}



JSModuleDef *js_init_module_router(JSContext *ctx, const char *module_name) {

    JSModuleDef *m = JS_NewCModule(ctx, module_name, js_router_init);

    if (!m) return NULL;

    JS_AddModuleExportList(ctx, m, js_router_funcs, sizeof(js_router_funcs)/sizeof(js_router_funcs));

    return m;

}