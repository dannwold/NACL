#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <errno.h>

#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <fcntl.h>

#include <dlfcn.h>

#include "adb_client.h"



// Simple checksum algorithm required by original ADB protocol (sum of all bytes)

static uint32_t calculate_checksum(const uint8_t *data, uint32_t len) {

    uint32_t sum = 0;

    for (uint32_t i = 0; i < len; ++i) {

        sum += data[i];

    }

    return sum;

}



int adb_initialize_session(AdbSession *session, const char *private_key_path, const char *public_key_path) {

    if (!session) return -1;

    memset(session, 0, sizeof(AdbSession));

    session->socket_fd = -1;

    session->state = ADB_STATE_DISCONNECTED;

    session->local_id = 1; // Local channel ID (e.g. 1)



    if (private_key_path) {

        strncpy(session->private_key_path, private_key_path, sizeof(session->private_key_path) - 1);

    }

    if (public_key_path) {

        strncpy(session->public_key_path, public_key_path, sizeof(session->public_key_path) - 1);

    }

    return 0;

}



int adb_connect_loopback(AdbSession *session, int local_port) {

    if (!session) return -1;



    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1) {

        perror("[ADB Client] Failed to create socket");

        session->state = ADB_STATE_ERROR;

        return -1;

    }



    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;

    server_addr.sin_port = htons(local_port);

    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");



    printf("[ADB Client] Connecting to wireless debugging on 127.0.0.1:%d...\n", local_port);

    if (connect(fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {

        fprintf(stderr, "[ADB Client] Connect failed: %s\n", strerror(errno));

        close(fd);

        session->state = ADB_STATE_ERROR;

        return -1;

    }



    session->socket_fd = fd;

    session->state = ADB_STATE_CONNECTING;

    return 0;

}



int adb_send_packet(int socket_fd, uint32_t command, uint32_t arg0, uint32_t arg1, const uint8_t *data, uint32_t data_len) {

    AdbHeader header;

    header.command = command;

    header.arg0 = arg0;

    header.arg1 = arg1;

    header.data_length = data_len;

    header.data_check = data ? calculate_checksum(data, data_len) : 0;

    header.magic = command ^ 0xFFFFFFFF;



    // Send Header

    if (write(socket_fd, &header, sizeof(AdbHeader)) != sizeof(AdbHeader)) {

        perror("[ADB Client] Socket write header failed");

        return -1;

    }



    // Send Payload

    if (data_len > 0 && data != NULL) {

        if (write(socket_fd, data, data_len) != (ssize_t)data_len) {

            perror("[ADB Client] Socket write payload failed");

            return -1;

        }

    }



    return 0;

}



int adb_read_packet(int socket_fd, AdbHeader *out_header, uint8_t *out_payload, uint32_t max_payload_len) {

    ssize_t r = read(socket_fd, out_header, sizeof(AdbHeader));

    if (r <= 0) {

        return -1;

    }

    if (r != sizeof(AdbHeader)) {

        fprintf(stderr, "[ADB Client] Read incomplete header (got %zd bytes)\n", r);

        return -1;

    }



    // Check magic signature to verify frame integrity

    if ((out_header->command ^ 0xFFFFFFFF) != out_header->magic) {

        fprintf(stderr, "[ADB Client] Magic check failed: cmd 0x%x magic 0x%x\n",

                out_header->command, out_header->magic);

        return -1;

    }



    if (out_header->data_length > 0) {

        if (out_header->data_length > max_payload_len) {

            fprintf(stderr, "[ADB Client] Payload exceeds max length (%u > %u)\n",

                    out_header->data_length, max_payload_len);

            return -1;

        }



        uint32_t read_bytes = 0;

        while (read_bytes < out_header->data_length) {

            ssize_t chunk = read(socket_fd, out_payload + read_bytes, out_header->data_length - read_bytes);

            if (chunk <= 0) {

                fprintf(stderr, "[ADB Client] Connection closed during payload read\n");

                return -1;

            }

            read_bytes += chunk;

        }



        // Verify checksum

        uint32_t checksum = calculate_checksum(out_payload, out_header->data_length);

        if (checksum != out_header->data_check) {

            fprintf(stderr, "[ADB Client] Payload checksum mismatch! Header: 0x%x, Got: 0x%x\n",

                    out_header->data_check, checksum);

            return -1;

        }

    }



    return 0;

}



// Function pointers to dynamically load OpenSSL from Android's libcrypto.so

typedef void* (*PEM_read_bio_PrivateKey_t)(void*, void**, void*, void*);

typedef void* (*BIO_new_mem_buf_t)(const void*, int);

typedef void (*BIO_free_t)(void*);

typedef void (*EVP_PKEY_free_t)(void*);

typedef void* (*EVP_MD_CTX_new_t)();

typedef void (*EVP_MD_CTX_free_t)(void*);

typedef const void* (*EVP_sha256_t)();

typedef int (*EVP_SignInit_ex_t)(void*, const void*, void*);

typedef int (*EVP_SignUpdate_t)(void*, const void*, size_t);

typedef int (*EVP_SignFinal_t)(void*, unsigned char*, unsigned int*, void*);



static int sign_token_with_openssl(const uint8_t *token, uint32_t token_len,

                                   const char *key_file, uint8_t *out_sig, uint32_t *out_sig_len) {

    // Open system libcrypto.so dynamically to keep client binary fully portable without NDK compile-time dependencies

    void *libcrypto = dlopen("libcrypto.so", RTLD_NOW);

    if (!libcrypto) {

        fprintf(stderr, "[ADB Client] Cannot load system libcrypto.so: %s\n", dlerror());

        return -1;

    }



    // Load OpenSSL functions

    BIO_new_mem_buf_t BIO_new_mem_buf_fn = (BIO_new_mem_buf_t)dlsym(libcrypto, "BIO_new_mem_buf");

    PEM_read_bio_PrivateKey_t PEM_read_bio_PrivateKey_fn = (PEM_read_bio_PrivateKey_t)dlsym(libcrypto, "PEM_read_bio_PrivateKey");

    BIO_free_t BIO_free_fn = (BIO_free_t)dlsym(libcrypto, "BIO_free");

    EVP_PKEY_free_t EVP_PKEY_free_fn = (EVP_PKEY_free_t)dlsym(libcrypto, "EVP_PKEY_free");

    EVP_MD_CTX_new_t EVP_MD_CTX_new_fn = (EVP_MD_CTX_new_t)dlsym(libcrypto, "EVP_MD_CTX_new");

    EVP_MD_CTX_free_t EVP_MD_CTX_free_fn = (EVP_MD_CTX_free_t)dlsym(libcrypto, "EVP_MD_CTX_free");

    EVP_sha256_t EVP_sha256_fn = (EVP_sha256_t)dlsym(libcrypto, "EVP_sha256");

    EVP_SignInit_ex_t EVP_SignInit_ex_fn = (EVP_SignInit_ex_t)dlsym(libcrypto, "EVP_SignInit_ex");

    EVP_SignUpdate_t EVP_SignUpdate_fn = (EVP_SignUpdate_t)dlsym(libcrypto, "EVP_SignUpdate");

    EVP_SignFinal_t EVP_SignFinal_fn = (EVP_SignFinal_t)dlsym(libcrypto, "EVP_SignFinal");



    if (!BIO_new_mem_buf_fn || !PEM_read_bio_PrivateKey_fn || !BIO_free_fn ||

        !EVP_PKEY_free_fn || !EVP_MD_CTX_new_fn || !EVP_MD_CTX_free_fn ||

        !EVP_sha256_fn || !EVP_SignInit_ex_fn || !EVP_SignUpdate_fn || !EVP_SignFinal_fn) {

        fprintf(stderr, "[ADB Client] Missing core crypto symbols in libcrypto.so\n");

        dlclose(libcrypto);

        return -1;

    }



    // Read the private key file

    FILE *f = fopen(key_file, "r");

    if (!f) {

        perror("[ADB Client] Failed to open private key file");

        dlclose(libcrypto);

        return -1;

    }

    fseek(f, 0, SEEK_END);

    long key_len = ftell(f);

    fseek(f, 0, SEEK_SET);

    char *key_data = malloc(key_len + 1);

    fread(key_data, 1, key_len, f);

    key_data[key_len] = '\0';

    fclose(f);



    void *bio = BIO_new_mem_buf_fn(key_data, key_len);

    void *pkey = PEM_read_bio_PrivateKey_fn(bio, NULL, NULL, NULL);

    BIO_free_fn(bio);

    free(key_data);



    if (!pkey) {

        fprintf(stderr, "[ADB Client] Failed to parse PEM private key\n");

        dlclose(libcrypto);

        return -1;

    }



    void *ctx = EVP_MD_CTX_new_fn();

    unsigned int sig_len = 0;

    int success = 0;



    if (EVP_SignInit_ex_fn(ctx, EVP_sha256_fn(), NULL) &&

        EVP_SignUpdate_fn(ctx, token, token_len) &&

        EVP_SignFinal_fn(ctx, out_sig, &sig_len, pkey)) {

        *out_sig_len = sig_len;

        success = 1;

    }



    EVP_MD_CTX_free_fn(ctx);

    EVP_PKEY_free_fn(pkey);

    dlclose(libcrypto);



    return success ? 0 : -1;

}



int adb_handle_handshake(AdbSession *session) {

    if (!session || session->socket_fd < 0) return -1;



    // Send connection initialization request packet

    const char *identity = "host::NativeAndroidCapabilityLibrary";

    if (adb_send_packet(session->socket_fd, A_CNXN, ADB_VERSION, ADB_MAX_PACKET,

                        (const uint8_t *)identity, strlen(identity)) < 0) {

        return -1;

    }



    AdbHeader header;

    static uint8_t payload[8192];



    while (1) {

        if (adb_read_packet(session->socket_fd, &header, payload, sizeof(payload)) < 0) {

            session->state = ADB_STATE_ERROR;

            return -1;

        }



        if (header.command == A_CNXN) {

            session->state = ADB_STATE_CONNECTED;

            printf("[ADB Client] Successfully connected without auth!\n");

            return 0;

        }



        if (header.command == A_AUTH && header.arg0 == ADB_AUTH_TOKEN) {

            session->state = ADB_STATE_AUTH_TOKEN_RECVD;

            printf("[ADB Client] Received authentication token from adbd (Length: %u bytes)\n", header.data_length);



            // Attempt to sign token

            uint8_t signature[2048];

            uint32_t sig_len = 0;

            if (sign_token_with_openssl(payload, header.data_length, session->private_key_path,

                                         signature, &sig_len) == 0) {

                printf("[ADB Client] Signing token with local private key successful.\n");

                adb_send_packet(session->socket_fd, A_AUTH, ADB_AUTH_SIGNATURE, 0, signature, sig_len);

                session->state = ADB_STATE_AUTH_SENT;

            } else {

                // If private key signing fails (e.g. no key found), send raw public key to trigger authorization popup

                printf("[ADB Client] Token signing failed or key not found. Sending public key to trigger dialog...\n");

                FILE *pf = fopen(session->public_key_path, "r");

                if (!pf) {

                    fprintf(stderr, "[ADB Client] Public key file %s missing.\n", session->public_key_path);

                    return -1;

                }

                fseek(pf, 0, SEEK_END);

                long pub_len = ftell(pf);

                fseek(pf, 0, SEEK_SET);

                uint8_t *pub_data = malloc(pub_len + 1);

                fread(pub_data, 1, pub_len, pf);

                pub_data[pub_len] = '\0';

                fclose(pf);



                adb_send_packet(session->socket_fd, A_AUTH, ADB_AUTH_RSAPUBLICKEY, 0, pub_data, pub_len + 1);

                free(pub_data);

                session->state = ADB_STATE_AUTH_SENT;

            }

            continue;

        }



        if (header.command == A_CNXN && session->state == ADB_STATE_AUTH_SENT) {

            session->state = ADB_STATE_CONNECTED;

            printf("[ADB Client] Successfully authenticated and connected to adbd!\n");

            return 0;

        }



        if (header.command == A_CLSE) {

            fprintf(stderr, "[ADB Client] Handshake rejected by adbd (closed connection)\n");

            session->state = ADB_STATE_ERROR;

            return -1;

        }

    }

}



int adb_open_shell_channel(AdbSession *session, const char *command) {

    if (!session || session->state != ADB_STATE_CONNECTED) return -1;



    char destination[512];

    snprintf(destination, sizeof(destination), "shell:%s", command);



    session->state = ADB_STATE_SHELL_OPENING;

    // Open standard ADB channel. Arg0 is local channel ID, Arg1 is 0

    if (adb_send_packet(session->socket_fd, A_OPEN, session->local_id, 0,

                        (const uint8_t *)destination, strlen(destination) + 1) < 0) {

        return -1;

    }



    AdbHeader header;

    static uint8_t payload[4096];



    while (1) {

        if (adb_read_packet(session->socket_fd, &header, payload, sizeof(payload)) < 0) {

            session->state = ADB_STATE_ERROR;

            return -1;

        }



        if (header.command == A_OKAY && header.arg1 == session->local_id) {

            session->remote_id = header.arg0; // Grab remote's channel ID

            session->state = ADB_STATE_SHELL_ACTIVE;

            printf("[ADB Client] Shell channel active. Local ID: %u, Remote ID: %u\n",

                   session->local_id, session->remote_id);

            return 0;

        }



        if (header.command == A_CLSE) {

            fprintf(stderr, "[ADB Client] Open shell channel rejected by remote daemon\n");

            session->state = ADB_STATE_ERROR;

            return -1;

        }

    }

}



int adb_write_shell_data(AdbSession *session, const uint8_t *data, uint32_t data_len) {

    if (!session || session->state != ADB_STATE_SHELL_ACTIVE) return -1;

    return adb_send_packet(session->socket_fd, A_WRTE, session->local_id, session->remote_id, data, data_len);

}



int adb_read_shell_data(AdbSession *session, uint8_t *out_buffer, uint32_t max_len, uint32_t *bytes_read) {

    if (!session || session->state != ADB_STATE_SHELL_ACTIVE) return -1;



    AdbHeader header;

    if (adb_read_packet(session->socket_fd, &header, out_buffer, max_len) < 0) {

        return -1;

    }



    if (header.command == A_WRTE && header.arg1 == session->local_id) {

        if (bytes_read) *bytes_read = header.data_length;

        // Acknowledge receipt immediately with OKAY packet

        adb_send_packet(session->socket_fd, A_OKAY, session->local_id, session->remote_id, NULL, 0);

        return 0;

    }



    if (header.command == A_CLSE) {

        printf("[ADB Client] Remote closed shell session channel.\n");

        session->state = ADB_STATE_CONNECTED;

        return -1;

    }



    return 0;

}



void adb_close_session(AdbSession *session) {

    if (!session) return;

    if (session->socket_fd >= 0) {

        if (session->state == ADB_STATE_SHELL_ACTIVE) {

            adb_send_packet(session->socket_fd, A_CLSE, session->local_id, session->remote_id, NULL, 0);

        }

        close(session->socket_fd);

        session->socket_fd = -1;

    }

    session->state = ADB_STATE_DISCONNECTED;

    printf("[ADB Client] Session cleanly closed.\n");

}