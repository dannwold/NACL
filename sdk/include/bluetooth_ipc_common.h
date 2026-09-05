#ifndef NATIVE_BLUETOOTH_IPC_COMMON_H

#define NATIVE_BLUETOOTH_IPC_COMMON_H



#include <stdint.h>



#ifdef __cplusplus

extern "C" {

#endif



// Unix Domain Socket Endpoint

#define IPC_SOCKET_BT "/data/local/tmp/sdk/sockets/bluetooth.sock"



// Binary packed command mapping

typedef enum {

    CMD_BT_START_LE_SCAN = 300,

    CMD_BT_STOP_LE_SCAN  = 301,

    CMD_BT_GET_DEVICES   = 302,

    CMD_BT_GATT_CONNECT  = 303,

    CMD_BT_GATT_DISCONNECT = 304,

    CMD_BT_GATT_READ_CHAR = 305,

    CMD_BT_GATT_WRITE_CHAR = 306

} BtCommandId;



#pragma pack(push, 1)



// Unified header for Bluetooth IPC packets

typedef struct {

    uint32_t magic;         // 0x4E414342 ("NACB")

    uint32_t transaction_id;

    uint16_t command;       // Maps to BtCommandId

    int32_t  status;        // Transaction response code

    uint32_t payload_len;   // Size of trailing payload buffer

} BtIpcHeader;



// Packed structure representing a discovered BLE Peripheral

typedef struct {

    char     mac_address[18];   // Formatted: "XX:XX:XX:XX:XX:XX"

    int32_t  rssi;              // Received Signal Strength Indicator (dBm)

    uint32_t device_class;      // Bluetooth Device Class

    uint8_t  address_type;      // Public, Random Static, Resolvable Private

    uint8_t  scan_record_len;   // Raw advertisement record length

    uint8_t  scan_record[62];   // Packed raw advertising bytes (EIR/LTV format)

} BleScanResult;



#pragma pack(pop)



#define BT_IPC_MAGIC 0x4E414342 // "NACB" (Native Android Capability Bluetooth)



#ifdef __cplusplus

}

#endif



#endif // NATIVE_BLUETOOTH_IPC_COMMON_H