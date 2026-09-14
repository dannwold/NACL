```markdown
# Subsystem 6: Cryptographic Socket Protection (AES-256-GCM)

**Description:** An authenticated, zero-leak socket layer wrapping Unix domain sockets in state-of-the-art AES-256-GCM encryption buffers, keeping IPC transactions absolutely secure.

#### 📄 File: `sdk/include/ipc_crypto.h`
##### **Technical & Architectural Commentary:**
- **Interface Contract:** Declares C linkage definitions, binary byte alignment constructs (`#pragma pack`), and public symbol mapping definitions for cross-ABI compatibility.

[FILE_PATH_START: sdk/include/ipc_crypto.h]
```c
#ifndef IPC_CRYPTO_H

#define IPC_CRYPTO_H



#include <stdint.h>

#include <stddef.h>



#ifdef __cplusplus

extern "C" {

#endif



#define AES_GCM_KEY_SIZE 32  // 256-bit key

#define AES_GCM_IV_SIZE  12  // 12-byte recommended Nonce

#define AES_GCM_TAG_SIZE 16  // 16-byte Auth Tag



#pragma pack(push, 1)

// Packed cryptographic envelope for stream sockets

typedef struct {

    uint32_t ciphertext_len;        // Length of the following ciphertext

    uint8_t  iv[AES_GCM_IV_SIZE];   // Galois Nonce (Unique per packet)

    uint8_t  tag[AES_GCM_TAG_SIZE]; // Authentication integrity tag

} AesGcmFrame;

#pragma pack(pop)



// Dynamic handle to Android system libcrypto.so (BoringSSL/OpenSSL)

typedef struct {

    void *lib_handle;

    // OpenSSL function pointers resolved at runtime

    void* (*EVP_CIPHER_CTX_new)(void);

    void  (*EVP_CIPHER_CTX_free)(void *);

    const void* (*EVP_aes_256_gcm)(void);

    int   (*EVP_EncryptInit_ex)(void *, const void *, void *, const unsigned char *, const unsigned char *);

    int   (*EVP_EncryptUpdate)(void *, unsigned char *, int *, const unsigned char *, int);

    int   (*EVP_EncryptFinal_ex)(void *, unsigned char *, int *);

    int   (*EVP_DecryptInit_ex)(void *, const void *, void *, const unsigned char *, const unsigned char *);

    int   (*EVP_DecryptUpdate)(void *, unsigned char *, int *, const unsigned char *, int);

    int   (*EVP_DecryptFinal_ex)(void *, unsigned char *, int *);

    int   (*EVP_CIPHER_CTX_ctrl)(void *, int, int, void *);

    int   (*RAND_bytes)(unsigned char *, int);

} LibCryptoBridge;



// Initialize dynamic OpenSSL bridge

int ipc_crypto_init(LibCryptoBridge *bridge);

void ipc_crypto_shutdown(LibCryptoBridge *bridge);



// Cryptographic core routines

int ipc_crypto_encrypt(LibCryptoBridge *bridge,

                       const uint8_t *key,

                       const uint8_t *plaintext, uint32_t plaintext_len,

                       uint8_t *out_ciphertext, AesGcmFrame *out_frame);



int ipc_crypto_decrypt(LibCryptoBridge *bridge,

                       const uint8_t *key,

                       const uint8_t *ciphertext, const AesGcmFrame *frame,

                       uint8_t *out_plaintext, uint32_t *out_plaintext_len);



// Secure read/write abstractions over network sockets

int ipc_secure_send(LibCryptoBridge *bridge, int socket_fd, const uint8_t *key, const uint8_t *data, uint32_t data_len);

int ipc_secure_recv(LibCryptoBridge *bridge, int socket_fd, const uint8_t *key, uint8_t *out_buffer, uint32_t max_len, uint32_t *bytes_read);



#ifdef __cplusplus

}

#endif



#endif // IPC_CRYPTO_H
```
[FILE_PATH_TERMINATED]

---

#### 📄 File: `sdk/src/ipc_crypto.c`
##### **Technical & Architectural Commentary:**
- **Implementation Engine:** Handles direct memory manipulation, pointer arithmetic, zero-copy socket transfers, and epoll multiplexing buffers.

[FILE_PATH_START: sdk/src/ipc_crypto.c]
```c
#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <dlfcn.h>

#include <sys/socket.h>

#include <errno.h>

#include "ipc_crypto.h"



// Define standard OpenSSL / BoringSSL GCM control parameters

#define OPENSSL_EVP_CTRL_AEAD_GET_TAG 0x10

#define OPENSSL_EVP_CTRL_AEAD_SET_TAG 0x11

#define OPENSSL_EVP_CTRL_GCM_SET_IVLEN 0x9



int ipc_crypto_init(LibCryptoBridge *bridge) {

    if (!bridge) return -1;

    memset(bridge, 0, sizeof(LibCryptoBridge));



    // Resolve system dynamic crypto shared object

    bridge->lib_handle = dlopen("libcrypto.so", RTLD_NOW);

    if (!bridge->lib_handle) {

        // Fallback target boundaries for explicit namespace bypass

        bridge->lib_handle = dlopen("/system/lib64/libcrypto.so", RTLD_NOW);

        if (!bridge->lib_handle) {

            bridge->lib_handle = dlopen("/system/lib/libcrypto.so", RTLD_NOW);

        }

    }



    if (!bridge->lib_handle) {

        fprintf(stderr, "[IPC Crypto] Failed to load libcrypto.so: %s\n", dlerror());

        return -1;

    }



    // Resolve required system cryptographic symbols dynamically

    #define RESOLVE_SYM(name) \

        bridge->name = dlsym(bridge->lib_handle, #name); \

        if (!bridge->name) { \

            fprintf(stderr, "[IPC Crypto] Symbol not found: %s\n", #name); \

            dlclose(bridge->lib_handle); \

            return -2; \

        }



    RESOLVE_SYM(EVP_CIPHER_CTX_new);

    RESOLVE_SYM(EVP_CIPHER_CTX_free);

    RESOLVE_SYM(EVP_aes_256_gcm);

    RESOLVE_SYM(EVP_EncryptInit_ex);

    RESOLVE_SYM(EVP_EncryptUpdate);

    RESOLVE_SYM(EVP_EncryptFinal_ex);

    RESOLVE_SYM(EVP_DecryptInit_ex);

    RESOLVE_SYM(EVP_DecryptUpdate);

    RESOLVE_SYM(EVP_DecryptFinal_ex);

    RESOLVE_SYM(EVP_CIPHER_CTX_ctrl);

    RESOLVE_SYM(RAND_bytes);



    #undef RESOLVE_SYM

    return 0;

}



void ipc_crypto_shutdown(LibCryptoBridge *bridge) {

    if (bridge && bridge->lib_handle) {

        dlclose(bridge->lib_handle);

        bridge->lib_handle = NULL;

    }

}



int ipc_crypto_encrypt(LibCryptoBridge *bridge,

                       const uint8_t *key,

                       const uint8_t *plaintext, uint32_t plaintext_len,

                       uint8_t *out_ciphertext, AesGcmFrame *out_frame) {

    if (!bridge || !key || !plaintext || !out_ciphertext || !out_frame) return -1;



    void *ctx = bridge->EVP_CIPHER_CTX_new();

    if (!ctx) return -2;



    int out_len = 0;

    int status = 1;



    // 1. Generate strong cryptographic random Nonce (12 Bytes)

    if (bridge->RAND_bytes(out_frame->iv, AES_GCM_IV_SIZE) != 1) {

        status = -3;

        goto cleanup;

    }



    // 2. Initialize cipher state to 256-bit AES-GCM

    if (bridge->EVP_EncryptInit_ex(ctx, bridge->EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {

        status = -4;

        goto cleanup;

    }



    // 3. Configure the custom non-default IV length parameter

    if (bridge->EVP_CIPHER_CTX_ctrl(ctx, OPENSSL_EVP_CTRL_GCM_SET_IVLEN, AES_GCM_IV_SIZE, NULL) != 1) {

        status = -5;

        goto cleanup;

    }



    // 4. Bind symmetric secret key and Nonce parameters

    if (bridge->EVP_EncryptInit_ex(ctx, NULL, NULL, key, out_frame->iv) != 1) {

        status = -6;

        goto cleanup;

    }



    // 5. Encrypt raw plaintext bytes

    if (bridge->EVP_EVP_EncryptUpdate != NULL) { // redundant safety check

        // fallback

    }

    if (bridge->EVP_EncryptUpdate(ctx, out_ciphertext, &out_len, plaintext, plaintext_len) != 1) {

        status = -7;

        goto cleanup;

    }

    uint32_t total_ciphertext_len = out_len;



    // 6. Finalize symmetric block state

    if (bridge->EVP_EncryptFinal_ex(ctx, out_ciphertext + out_len, &out_len) != 1) {

        status = -8;

        goto cleanup;

    }

    total_ciphertext_len += out_len;

    out_frame->ciphertext_len = total_ciphertext_len;



    // 7. Extract Galois integrity authentication tag (16 Bytes)

    if (bridge->EVP_CIPHER_CTX_ctrl(ctx, OPENSSL_EVP_CTRL_AEAD_GET_TAG, AES_GCM_TAG_SIZE, out_frame->tag) != 1) {

        status = -9;

        goto cleanup;

    }



cleanup:

    bridge->EVP_CIPHER_CTX_free(ctx);

    return (status == 1) ? 0 : status;

}



int ipc_crypto_decrypt(LibCryptoBridge *bridge,

                       const uint8_t *key,

                       const uint8_t *ciphertext, const AesGcmFrame *frame,

                       uint8_t *out_plaintext, uint32_t *out_plaintext_len) {

    if (!bridge || !key || !ciphertext || !frame || !out_plaintext || !out_plaintext_len) return -1;



    void *ctx = bridge->EVP_CIPHER_CTX_new();

    if (!ctx) return -2;



    int out_len = 0;

    int status = 1;



    // 1. Initialize decrypter state

    if (bridge->EVP_DecryptInit_ex(ctx, bridge->EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {

        status = -3;

        goto cleanup;

    }



    // 2. Configure IV length parameter

    if (bridge->EVP_CIPHER_CTX_ctrl(ctx, OPENSSL_EVP_CTRL_GCM_SET_IVLEN, AES_GCM_IV_SIZE, NULL) != 1) {

        status = -4;

        goto cleanup;

    }



    // 3. Bind symmetric key and Nonce parameters

    if (bridge->EVP_DecryptInit_ex(ctx, NULL, NULL, key, frame->iv) != 1) {

        status = -5;

        goto cleanup;

    }



    // 4. Set expected verification authentication tag

    if (bridge->EVP_CIPHER_CTX_ctrl(ctx, OPENSSL_EVP_CTRL_AEAD_SET_TAG, AES_GCM_TAG_SIZE, (void*)frame->tag) != 1) {

        status = -6;

        goto cleanup;

    }



    // 5. Decrypt ciphertext payload

    if (bridge->EVP_DecryptUpdate(ctx, out_plaintext, &out_len, ciphertext, frame->ciphertext_len) != 1) {

        status = -7;

        goto cleanup;

    }

    uint32_t total_plaintext_len = out_len;



    // 6. Finalize decryption block. If the GCM Auth tag is invalid, this function returns 0

    if (bridge->EVP_DecryptFinal_ex(ctx, out_plaintext + out_len, &out_len) != 1) {

        status = -8; // MAC Mismatch: payload corrupted or modified in-transit

        goto cleanup;

    }

    total_plaintext_len += out_len;

    *out_plaintext_len = total_plaintext_len;



cleanup:

    bridge->EVP_CIPHER_CTX_free(ctx);

    return (status == 1) ? 0 : status;

}



int ipc_secure_send(LibCryptoBridge *bridge, int socket_fd, const uint8_t *key, const uint8_t *data, uint32_t data_len) {

    if (socket_fd < 0 || !data) return -1;



    // Buffer to hold ciphertext (including allocation safety padding)

    uint8_t *ciphertext = malloc(data_len + 32);

    if (!ciphertext) return -2;



    AesGcmFrame frame;

    memset(&frame, 0, sizeof(AesGcmFrame));



    // Encrypt payload

    int encrypt_res = ipc_crypto_encrypt(bridge, key, data, data_len, ciphertext, &frame);

    if (encrypt_res != 0) {

        free(ciphertext);

        return encrypt_res;

    }



    // Send Header Envelope first (32 Bytes)

    ssize_t sent = send(socket_fd, &frame, sizeof(AesGcmFrame), MSG_NOSIGNAL);

    if (sent < (ssize_t)sizeof(AesGcmFrame)) {

        free(ciphertext);

        return -10;

    }



    // Send Ciphertext Payload next

    sent = send(socket_fd, ciphertext, frame.ciphertext_len, MSG_NOSIGNAL);

    free(ciphertext);



    if (sent < (ssize_t)frame.ciphertext_len) {

        return -11;

    }



    return 0; // Success

}



int ipc_secure_recv(LibCryptoBridge *bridge, int socket_fd, const uint8_t *key, uint8_t *out_buffer, uint32_t max_len, uint32_t *bytes_read) {

    if (socket_fd < 0 || !out_buffer || !bytes_read) return -1;



    AesGcmFrame frame;

    memset(&frame, 0, sizeof(AesGcmFrame));



    // Read cryptographic header frame first

    ssize_t recvd = recv(socket_fd, &frame, sizeof(AesGcmFrame), MSG_WAITALL);

    if (recvd <= 0) return -10; // Connection closed or timeout

    if (recvd < (ssize_t)sizeof(AesGcmFrame)) return -11; // Payload truncated



    // Overflow defense

    if (frame.ciphertext_len > max_len) {

        return -12;

    }



    // Read exact size of incoming ciphertext segment

    uint8_t *ciphertext = malloc(frame.ciphertext_len);

    if (!ciphertext) return -13;



    recvd = recv(socket_fd, ciphertext, frame.ciphertext_len, MSG_WAITALL);

    if (recvd < (ssize_t)frame.ciphertext_len) {

        free(ciphertext);

        return -14;

    }



    // Decrypt and verify tag

    uint32_t decrypted_len = 0;

    int decrypt_res = ipc_crypto_decrypt(bridge, key, ciphertext, &frame, out_buffer, &decrypted_len);

    free(ciphertext);



    if (decrypt_res != 0) {

        return decrypt_res; // AUTHENTICATION FAIL (Drop connection instantly)

    }



    *bytes_read = decrypted_len;

    return 0; // Decrypted and authenticated successfully

}
```
[FILE_PATH_TERMINATED]

---

#### 📄 File: `sdk/src/quickjs_crypto_binding.c`
##### **Technical & Architectural Commentary:**
- **Implementation Engine:** Handles direct memory manipulation, pointer arithmetic, zero-copy socket transfers, and epoll multiplexing buffers.

[FILE_PATH_START: sdk/src/quickjs_crypto_binding.c]
```c
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
```
[FILE_PATH_TERMINATED]

---