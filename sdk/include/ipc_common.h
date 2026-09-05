#ifndef NATIVE_IPC_COMMON_H

#define NATIVE_IPC_COMMON_H



#include <stdint.h>



#ifdef __cplusplus

extern "C" {

#endif



// Defined Unix Domain Socket paths

// UID 2000 (Shell) has full read/write permissions under /data/local/tmp/

#define IPC_SOCKET_DIR "/data/local/tmp/sdk/sockets"

#define IPC_SOCKET_WIFI "/data/local/tmp/sdk/sockets/wifi.sock"

#define IPC_SOCKET_BT   "/data/local/tmp/sdk/sockets/bluetooth.sock"

#define IPC_SOCKET_SENS "/data/local/tmp/sdk/sockets/sensors.sock"



// Subsystem classifications

typedef enum {

    SUBSYSTEM_CORE      = 0,

    SUBSYSTEM_WIFI      = 1,

    SUBSYSTEM_BLUETOOTH = 2,

    SUBSYSTEM_SENSORS   = 3,

    SUBSYSTEM_LOCATION  = 4

} SubsystemType;



// Command ID maps

typedef enum {

    // Core commands

    CMD_CORE_PING       = 100,

    CMD_CORE_GET_VER    = 101,



    // Wi-Fi commands

    CMD_WIFI_START_SCAN = 200,

    CMD_WIFI_GET_RESULTS= 201,

    CMD_WIFI_SET_STATE  = 202,



    // Bluetooth commands

    CMD_BT_START_SCAN   = 300,

    CMD_BT_STOP_SCAN    = 301,

    CMD_BT_CONNECT      = 302

} CommandId;



// Response status codes

typedef enum {

    STATUS_OK           = 0,

    STATUS_ERROR        = -1,

    STATUS_UNSUPPORTED  = -2,

    STATUS_PERMISSION_DENIED = -3,

    STATUS_BUSY         = -4

} StatusCode;



// Standard header for IPC messages (Explicitly packed binary layout)

#pragma pack(push, 1)

typedef struct {

    uint32_t magic;         // Magic signature validation (0x4E41434C)

    uint32_t transaction_id;// Transaction multiplexing ID

    uint16_t subsystem;     // Target Subsystem (SubsystemType)

    uint16_t command;       // Specific Command Code (CommandId)

    int32_t  status;        // Transaction status

    uint32_t payload_len;   // Payload length immediately following the header

} IpcHeader;

#pragma pack(pop)



#define IPC_MAGIC_SIGNATURE 0x4E41434C // "NACL" (Native Android Capability Library)



#ifdef __cplusplus

}

#endif



#endif // NATIVE_IPC_COMMON_H