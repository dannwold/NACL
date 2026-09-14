#ifndef NATIVE_ADB_CLIENT_H

#define NATIVE_ADB_CLIENT_H



#include <stdint.h>

#include <stddef.h>



#ifdef __cplusplus

extern "C" {

#endif



// ADB Message commands represented as little-endian constants

#define A_SYNC 0x434e5953  // "SYNC"

#define A_CNXN 0x4e584e43  // "CNXN"

#define A_OPEN 0x4e45504f  // "OPEN"

#define A_OKAY 0x59414b4f  // "OKAY"

#define A_CLSE 0x45534c43  // "CLSE"

#define A_WRTE 0x45545257  // "WRTE"

#define A_AUTH 0x48545541  // "AUTH"



// ADB Auth Subtypes

#define ADB_AUTH_TOKEN        1

#define ADB_AUTH_SIGNATURE    2

#define ADB_AUTH_RSAPUBLICKEY 3



// ADB Protocol constants

#define ADB_VERSION      0x01000000

#define ADB_MAX_PACKET   1048576



// Packed ADB Message Header struct (24 bytes)

#pragma pack(push, 1)

typedef struct {

    uint32_t command;       // Command code constant (e.g. A_CNXN)

    uint32_t arg0;          // First argument (command-dependent)

    uint32_t arg1;          // Second argument (command-dependent)

    uint32_t data_length;   // Length of data payload (0 is valid)

    uint32_t data_check;    // Simple CRC-32 checksum of data payload

    uint32_t magic;         // bitwise complement of command (command ^ 0xFFFFFFFF)

} AdbHeader;

#pragma pack(pop)



// ADB Client session state machine

typedef enum {

    ADB_STATE_DISCONNECTED = 0,

    ADB_STATE_CONNECTING,

    ADB_STATE_AUTH_TOKEN_RECVD,

    ADB_STATE_AUTH_SENT,

    ADB_STATE_CONNECTED,

    ADB_STATE_SHELL_OPENING,

    ADB_STATE_SHELL_ACTIVE,

    ADB_STATE_ERROR

} AdbSessionState;



typedef struct {

    int socket_fd;

    AdbSessionState state;

    uint32_t local_id;

    uint32_t remote_id;

    char private_key_path[256];

    char public_key_path[256];

} AdbSession;



// Core functions

int adb_initialize_session(AdbSession *session, const char *private_key_path, const char *public_key_path);

int adb_connect_loopback(AdbSession *session, int local_port);

int adb_send_packet(int socket_fd, uint32_t command, uint32_t arg0, uint32_t arg1, const uint8_t *data, uint32_t data_len);

int adb_read_packet(int socket_fd, AdbHeader *out_header, uint8_t *out_payload, uint32_t max_payload_len);

int adb_handle_handshake(AdbSession *session);

int adb_open_shell_channel(AdbSession *session, const char *command);

int adb_write_shell_data(AdbSession *session, const uint8_t *data, uint32_t data_len);

int adb_read_shell_data(AdbSession *session, uint8_t *out_buffer, uint32_t max_len, uint32_t *bytes_read);

void adb_close_session(AdbSession *session);



#ifdef __cplusplus

}

#endif



#endif // NATIVE_ADB_CLIENT_H