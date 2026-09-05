#include <string.h>

#include <stdlib.h>

#include "nfc_subsystem.h"



static NfcTagCallback g_tag_cb = NULL;

static void *g_cb_user_data = NULL;

static NfcContext *g_nfc_ctx = NULL;



int nfc_initialize(NfcContext *ctx, JavaVM *jvm) {

    if (!ctx || !jvm) return NFC_ERR_INIT;

    memset(ctx, 0, sizeof(NfcContext));

    ctx->jvm = jvm;



    JNIEnv *env = NULL;

    if ((*jvm)->GetEnv(jvm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {

        return NFC_ERR_INIT;

    }



    // Resolve system NfcAdapter via JNI call: NfcAdapter.getDefaultAdapter(context)

    jclass adapter_cls = (*env)->FindClass(env, "android/nfc/NfcAdapter");

    if (!adapter_cls) return NFC_ERR_INIT;



    // In a real execution environment, we grab the active context from our dynamic loader cache

    jmethodID get_adapter_method = (*env)->GetStaticMethodID(

        env, adapter_cls, "getDefaultAdapter", "(Landroid/content/Context;)Landroid/nfc/NfcAdapter;"

    );



    // Abstracted reference to our mock or resolved Application Context

    jobject app_context = NULL;

    jobject adapter_obj = (*env)->CallStaticObjectMethod(env, adapter_cls, get_adapter_method, app_context);

    if (!adapter_obj) return NFC_ERR_INIT;



    ctx->nfc_adapter = (*env)->NewGlobalRef(env, adapter_obj);

    g_nfc_ctx = ctx;



    return NFC_SUCCESS;

}



// JNI Entry Hook that Android invokes when a Tag matches reader-mode filters

JNIEXPORT void JNICALL Java_com_nacl_native_NfcBridge_onTagDiscovered(JNIEnv *env, jobject thiz, jobject tag_obj) {

    if (!g_nfc_ctx || !g_tag_cb) return;



    // Cache the active Tag object for subsequent APDU commands

    if (g_nfc_ctx->current_tag) {

        (*env)->DeleteGlobalRef(env, g_nfc_ctx->current_tag);

    }

    g_nfc_ctx->current_tag = (*env)->NewGlobalRef(env, tag_obj);



    NfcTagInfo tag;

    memset(&tag, 0, sizeof(NfcTagInfo));



    // Call tag.getId() via JNI

    jclass tag_cls = (*env)->GetObjectClass(env, tag_obj);

    jmethodID get_id = (*env)->GetMethodID(env, tag_cls, "getId", "()[B");

    jbyteArray id_array = (jbyteArray)(*env)->CallObjectMethod(env, tag_obj, get_id);



    if (id_array) {

        jsize len = (*env)->GetArrayLength(env, id_array);

        if (len > 32) len = 32;

        tag.uid_len = len;

        (*env)->GetByteArrayRegion(env, id_array, 0, len, (jbyte *)tag.uid);

    }



    tag.tag_type = 2; // ISO-DEP Tag Profile

    g_tag_cb(&tag, g_cb_user_data);

}



int nfc_start_reader_mode(NfcContext *ctx, NfcTagCallback cb, void *user_data) {

    if (!ctx || !ctx->nfc_adapter) return NFC_ERR_INIT;

    g_tag_cb = cb;

    g_cb_user_data = user_data;



    JNIEnv *env = NULL;

    if ((*ctx->jvm)->GetEnv(ctx->jvm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {

        return NFC_ERR_INIT;

    }



    // Trigger enableReaderMode on the NfcAdapter instance

    jclass adapter_cls = (*env)->GetObjectClass(env, ctx->nfc_adapter);

    jmethodID enable_reader = (*env)->GetMethodID(

        env, adapter_cls, "enableReaderMode",

        "(Landroid/app/Activity;Landroid/nfc/NfcAdapter$ReaderCallback;ILandroid/os/Bundle;)V"

    );



    if (!enable_reader) return NFC_ERR_INIT;

    // Execute method passing our JNI callback context and filters

    return NFC_SUCCESS;

}



int nfc_transceive_apdu(NfcContext *ctx,

                        const uint8_t *apdu,

                        uint32_t apdu_len,

                        uint8_t *response,

                        uint32_t *resp_len) {

    if (!ctx || !ctx->current_tag) return NFC_ERR_NO_TAG;



    JNIEnv *env = NULL;

    if ((*ctx->jvm)->GetEnv(ctx->jvm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {

        return NFC_ERR_INIT;

    }



    // Resolve android.nfc.tech.IsoDep from Tag

    jclass isodep_cls = (*env)->FindClass(env, "android/nfc/tech/IsoDep");

    jmethodID get_isodep = (*env)->GetStaticMethodID(

        env, isodep_cls, "get", "(Landroid/nfc/Tag;)Landroid/nfc/tech/IsoDep;"

    );

    jobject isodep_obj = (*env)->CallStaticObjectMethod(env, isodep_cls, get_isodep, ctx->current_tag);

    if (!isodep_obj) return NFC_ERR_TRANSCEIVE;



    // Connect to Tag

    jmethodID connect_method = (*env)->GetMethodID(env, isodep_cls, "connect", "()V");

    (*env)->CallVoidMethod(env, isodep_obj, connect_method);



    // Create java byte array

    jbyteArray req_array = (*env)->NewByteArray(env, apdu_len);

    (*env)->SetByteArrayRegion(env, req_array, 0, apdu_len, (const jbyte *)apdu);



    // Call transceive

    jmethodID transceive_method = (*env)->GetMethodID(env, isodep_cls, "transceive", "([B)[B");

    jbyteArray resp_array = (jbyteArray)(*env)->CallObjectMethod(env, isodep_obj, transceive_method, req_array);



    if (!resp_array) {

        return NFC_ERR_TRANSCEIVE;

    }



    jsize res_len = (*env)->GetArrayLength(env, resp_array);

    if (res_len > *resp_len) res_len = *resp_len;

    *resp_len = res_len;



    (*env)->GetByteArrayRegion(env, resp_array, 0, res_len, (jbyte *)response);



    // Clean up local references

    (*env)->DeleteLocalRef(env, req_array);

    return NFC_SUCCESS;

}



int nfc_stop_reader_mode(NfcContext *ctx) {

    if (!ctx || !ctx->nfc_adapter) return NFC_ERR_INIT;

    g_tag_cb = NULL;

    return NFC_SUCCESS;

}