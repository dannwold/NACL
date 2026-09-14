#include "quickjs.h"

#include "adb_client.h"

#include <string.h>

#include <stdlib.h>



// Handle mapping for ADB Session Class

static JSClassID js_adb_session_class_id;



typedef struct {

    AdbSession session;

} JSAdbSession;



static void js_adb_session_finalizer(JSRuntime *rt, JSValue val) {

    JSAdbSession *s = JS_GetOpaque(val, js_adb_session_class_id);

    if (s) {

        adb_close_session(&s->session);

        free(s);

    }

}



// js: session.execute(command)

static JSValue js_adb_session_execute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    JSAdbSession *s = JS_GetOpaque2(ctx, this_val, js_adb_session_class_id);

    if (!s) return JS_EXCEPTION;



    const char *command = JS_ToCString(ctx, argv[0]);

    if (!command) return JS_EXCEPTION;



    if (adb_open_shell_channel(&s->session, command) < 0) {

        JS_FreeCString(ctx, command);

        return JS_ThrowInternalError(ctx, "Failed to establish command channel to adbd");

    }

    JS_FreeCString(ctx, command);



    // Read full output buffer

    uint8_t read_buf[4096];

    uint32_t bytes_read = 0;

    char *output_accum = malloc(1);

    output_accum[0] = '\0';

    size_t accum_size = 0;



    while (adb_read_shell_data(&s->session, read_buf, sizeof(read_buf) - 1, &bytes_read) == 0) {

        read_buf[bytes_read] = '\0';

        output_accum = realloc(output_accum, accum_size + bytes_read + 1);

        memcpy(output_accum + accum_size, read_buf, bytes_read);

        accum_size += bytes_read;

        output_accum[accum_size] = '\0';

    }



    JSValue result = JS_NewString(ctx, output_accum);

    free(output_accum);



    // Clean up channel state but keep connection intact for subsequent commands

    adb_send_packet(s->session.socket_fd, A_CLSE, s->session.local_id, s->session.remote_id, NULL, 0);

    s->session.state = ADB_STATE_CONNECTED;



    return result;

}



static const JSCFunctionListEntry js_adb_session_proto_funcs[] = {

    JS_CFUNC_DEF("execute", 1, js_adb_session_execute),

};



static JSClassDef js_adb_session_class = {

    "AdbSession",

    .finalizer = js_adb_session_finalizer,

};



// js: adb.connect(port, privateKeyPath, publicKeyPath)

static JSValue js_adb_connect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    int port;

    if (JS_ToInt32(ctx, &port, argv[0])) return JS_EXCEPTION;



    const char *priv_key = JS_ToCString(ctx, argv[1]);

    const char *pub_key = JS_ToCString(ctx, argv[2]);



    JSAdbSession *s = malloc(sizeof(JSAdbSession));

    if (adb_initialize_session(&s->session, priv_key, pub_key) < 0) {

        free(s);

        JS_FreeCString(ctx, priv_key);

        JS_FreeCString(ctx, pub_key);

        return JS_ThrowInternalError(ctx, "Failed to initialize ADB engine configuration");

    }



    JS_FreeCString(ctx, priv_key);

    JS_FreeCString(ctx, pub_key);



    if (adb_connect_loopback(&s->session, port) < 0) {

        free(s);

        return JS_ThrowInternalError(ctx, "Failed to open loopback socket connection to adbd");

    }



    if (adb_handle_handshake(&s->session) < 0) {

        adb_close_session(&s->session);

        free(s);

        return JS_ThrowInternalError(ctx, "Cryptographic adbd handshake authorization rejected");

    }



    JSValue obj = JS_NewObjectClass(ctx, js_adb_session_class_id);

    if (JS_IsException(obj)) {

        adb_close_session(&s->session);

        free(s);

        return JS_EXCEPTION;

    }



    JS_SetOpaque(obj, s);

    return obj;

}



static const JSCFunctionListEntry js_adb_funcs[] = {

    JS_CFUNC_DEF("connect", 3, js_adb_connect),

};



static int js_adb_init(JSContext *ctx, JSModuleDef *m) {

    JS_NewClassID(&js_adb_session_class_id);

    JS_NewClass(JS_GetRuntime(ctx), js_adb_session_class_id, &js_adb_session_class);



    JSValue proto = JS_NewObject(ctx);

    JS_SetPropertyFunctionList(ctx, proto, js_adb_session_proto_funcs, sizeof(js_adb_session_proto_funcs)/sizeof(js_adb_session_proto_funcs));

    JS_SetClassProto(ctx, js_adb_session_class_id, proto);



    return JS_SetModuleExportList(ctx, m, js_adb_funcs, sizeof(js_adb_funcs)/sizeof(js_adb_funcs));

}



JSModuleDef *js_init_module_adb(JSContext *ctx, const char *module_name) {

    JSModuleDef *m = JS_NewCModule(ctx, module_name, js_adb_init);

    if (!m) return NULL;

    JS_AddModuleExportList(ctx, m, js_adb_funcs, sizeof(js_adb_funcs)/sizeof(js_adb_funcs));

    return m;

}