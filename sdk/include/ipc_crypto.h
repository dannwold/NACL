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