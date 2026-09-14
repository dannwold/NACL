#include "quickjs.h"

#include "ipc_crypto.h"

#include <string.h>



static LibCryptoBridge g_crypto_bridge;

static int g_crypto_initialized = 0;



static JSValue js_crypto_init(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    if (g_crypto_initialized) return JS_TRUE;



    int res = ipc_crypto_init(&g_crypto_bridge);

    if (res == 0) {

        g_crypto_initialized = 1;

        return JS_TRUE;

    }

    return JS_ThrowInternalError(ctx, "Failed to bind to system libcrypto.so: %d", res);

}



static JSValue js_secure_send(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    if (argc < 3) return JS_ThrowTypeError(ctx, "Required params: (fd, keyArrayBuffer, payloadString)");

    if (!g_crypto_initialized) return JS_ThrowInternalError(ctx, "Crypto wrapper is inactive.");



    int fd;

    if (JS_ToInt32(ctx, &fd, argv[0])) return JS_EXCEPTION;



    size_t key_len = 0;

    uint8_t *key_data = JS_GetArrayBuffer(ctx, &key_len, argv[1]);

    if (!key_data || key_len != AES_GCM_KEY_SIZE) {

        return JS_ThrowTypeError(ctx, "Key must be an exact 32-byte ArrayBuffer.");

    }



    size_t msg_len = 0;

    const char *msg_str = JS_ToCStringLen(ctx, &msg_len, argv[2]);

    if (!msg_str) return JS_EXCEPTION;



    int res = ipc_secure_send(&g_crypto_bridge, fd, key_data, (const uint8_t*)msg_str, (uint32_t)msg_len);

    JS_FreeCString(ctx, msg_str);



    if (res != 0) return JS_ThrowInternalError(ctx, "Secure send transaction failed: %d", res);

    return JS_TRUE;

}



static JSValue js_secure_recv(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    if (argc < 2) return JS_ThrowTypeError(ctx, "Required params: (fd, keyArrayBuffer)");

    if (!g_crypto_initialized) return JS_ThrowInternalError(ctx, "Crypto wrapper is inactive.");



    int fd;

    if (JS_ToInt32(ctx, &fd, argv[0])) return JS_EXCEPTION;



    size_t key_len = 0;

    uint8_t *key_data = JS_GetArrayBuffer(ctx, &key_len, argv[1]);

    if (!key_data || key_len != AES_GCM_KEY_SIZE) {

        return JS_ThrowTypeError(ctx, "Key must be an exact 32-byte ArrayBuffer.");

    }



    uint32_t max_buf_size = 65536;

    uint8_t *decrypted_buffer = malloc(max_buf_size);

    if (!decrypted_buffer) return JS_ThrowOutOfMemory(ctx);



    uint32_t bytes_read = 0;

    int res = ipc_secure_recv(&g_crypto_bridge, fd, key_data, decrypted_buffer, max_buf_size, &bytes_read);



    if (res != 0) {

        free(decrypted_buffer);

        if (res == -8) {

            return JS_ThrowInternalError(ctx, "AES-GCM Authenticity check failed: data compromised.");

        }

        return JS_ThrowInternalError(ctx, "Secure read transaction failed: %d", res);

    }



    JSValue result_str = JS_NewStringLen(ctx, (const char*)decrypted_buffer, bytes_read);

    free(decrypted_buffer);

    return result_str;

}



static JSValue js_crypto_shutdown(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    if (g_crypto_initialized) {

        ipc_crypto_shutdown(&g_crypto_bridge);

        g_crypto_initialized = 0;

    }

    return JS_UNDEFINED;

}



static const JSCFunctionListEntry js_crypto_funcs[] = {

    JS_CFUNC_DEF("init", 0, js_crypto_init),

    JS_CFUNC_DEF("secureSend", 3, js_secure_send),

    JS_CFUNC_DEF("secureRecv", 2, js_secure_recv),

    JS_CFUNC_DEF("shutdown", 0, js_crypto_shutdown),

};



static int js_crypto_module_init(JSContext *ctx, JSModuleDef *m) {

    return JS_SetModuleExportList(ctx, m, js_crypto_funcs, sizeof(js_crypto_funcs) / sizeof(js_crypto_funcs[0]));

}



JSModuleDef *js_init_crypto_module(JSContext *ctx, const char *module_name) {

    JSModuleDef *m = JS_NewCModule(ctx, module_name, js_crypto_module_init);

    if (!m) return NULL;

    JS_AddModuleExportList(ctx, m, js_crypto_funcs, sizeof(js_crypto_funcs) / sizeof(js_crypto_funcs[0]));

    return m;

}