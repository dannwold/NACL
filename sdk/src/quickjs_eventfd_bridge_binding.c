#include "quickjs.h"

#include "quickjs_eventfd_bridge.h"



static JSClassID js_eventfd_bridge_class_id;



// GC Finalizer to avoid native pointer leaks

static void js_eventfd_bridge_finalizer(JSRuntime *rt, JSValue val) {

    EventfdBridge *bridge = (EventfdBridge*)JS_GetOpaque(val, js_eventfd_bridge_class_id);

    if (bridge) {

        eventfd_bridge_destroy(bridge);

    }

}



static JSClassDef js_eventfd_bridge_class = {

    "EventfdBridge",

    .finalizer = js_eventfd_bridge_finalizer,

};



// JS: const bridge = new EventfdBridge(callback);

static JSValue js_eventfd_bridge_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {

    if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {

        return JS_ThrowTypeError(ctx, "Expected callback function as parameter 0");

    }



    JSValue obj = JS_NewObjectClass(ctx, js_eventfd_bridge_class_id);

    if (JS_IsException(obj)) return obj;



    EventfdBridge *bridge = eventfd_bridge_create(ctx, argv[0]);

    if (!bridge) {

        JS_FreeValue(ctx, obj);

        return JS_ThrowInternalError(ctx, "Failed to initialize native EventfdBridge context");

    }



    JS_SetOpaque(obj, bridge);

    return obj;

}



// JS: bridge.dispatch();

static JSValue js_eventfd_bridge_dispatch(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    EventfdBridge *bridge = (EventfdBridge*)JS_GetOpaque2(ctx, this_val, js_eventfd_bridge_class_id);

    if (!bridge) return JS_EXCEPTION;



    eventfd_bridge_dispatch_pending(bridge);

    return JS_UNDEFINED;

}



// JS: bridge.getFd();

static JSValue js_eventfd_bridge_get_fd(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    EventfdBridge *bridge = (EventfdBridge*)JS_GetOpaque2(ctx, this_val, js_eventfd_bridge_class_id);

    if (!bridge) return JS_EXCEPTION;



    return JS_NewInt32(ctx, bridge->event_fd);

}



static const JSCFunctionListEntry js_eventfd_bridge_proto_funcs[] = {

    JS_CFUNC_DEF("dispatch", 0, js_eventfd_bridge_dispatch),

    JS_CFUNC_DEF("getFd", 0, js_eventfd_bridge_get_fd),

};



static int js_eventfd_init(JSContext *ctx, JSModuleDef *m) {

    JS_NewClassID(&js_eventfd_bridge_class_id);

    JS_NewClass(JS_GetRuntime(ctx), js_eventfd_bridge_class_id, &js_eventfd_bridge_class);



    JSValue proto = JS_NewObject(ctx);

    JS_SetPropertyFunctionList(ctx, proto, js_eventfd_bridge_proto_funcs, sizeof(js_eventfd_bridge_proto_funcs)/sizeof(js_eventfd_bridge_proto_funcs[0]));

    JS_SetClassProto(ctx, js_eventfd_bridge_class_id, proto);



    JSValue ctor = JS_NewCFunction2(ctx, js_eventfd_bridge_constructor, "EventfdBridge", 1, JS_CFUNC_constructor, 0);

    JS_SetModuleExport(ctx, m, "EventfdBridge", ctor);



    return 0;

}



JSModuleDef *js_init_module_eventfd(JSContext *ctx, const char *module_name) {

    JSModuleDef *m = JS_NewCModule(ctx, module_name, js_eventfd_init);

    if (!m) return NULL;

    JS_AddModuleExport(ctx, m, "EventfdBridge");

    return m;

}