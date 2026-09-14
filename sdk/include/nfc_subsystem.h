#ifndef NATIVE_NFC_SUBSYSTEM_H

#define NATIVE_NFC_SUBSYSTEM_H



#include <stdint.h>

#include <stddef.h>

#include <jni.h>



#ifdef __cplusplus

extern "C" {

#endif



#define NFC_SUCCESS         0

#define NFC_ERR_INIT       -1

#define NFC_ERR_NO_TAG     -2

#define NFC_ERR_TRANSCEIVE -3



#pragma pack(push, 1)



typedef struct {

    uint8_t  uid[32];        // Tag UID Buffer

    uint32_t uid_len;        // Tag UID length

    uint32_t tag_type;       // Standard identifier (NFC-A, ISO-DEP, etc.)

} NfcTagInfo;



typedef struct {

    JavaVM  *jvm;            // Cached Java Virtual Machine instance

    jobject  nfc_adapter;    // Global reference to android.nfc.NfcAdapter

    jobject  current_tag;    // Reference to active Tag object

} NfcContext;



#pragma pack(pop)



// Callback signature for async Tag discoveries

typedef void (*NfcTagCallback)(const NfcTagInfo *tag, void *user_data);



int nfc_initialize(NfcContext *ctx, JavaVM *jvm);

int nfc_start_reader_mode(NfcContext *ctx, NfcTagCallback cb, void *user_data);

int nfc_stop_reader_mode(NfcContext *ctx);



int nfc_transceive_apdu(NfcContext *ctx,

                        const uint8_t *apdu,

                        uint32_t apdu_len,

                        uint8_t *response,

                        uint32_t *resp_len);



#ifdef __cplusplus

}

#endif



#endif // NATIVE_NFC_SUBSYSTEM_H