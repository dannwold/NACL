#ifndef NATIVE_AUTOMATION_COMMON_H

#define NATIVE_AUTOMATION_COMMON_H



#include <stdint.h>



#ifdef __cplusplus

extern "C" {

#endif



// Status codes for zero-root automation module

typedef enum {

    AUTO_STATUS_OK = 0,

    AUTO_STATUS_ERROR = -1,

    AUTO_STATUS_ADB_DISCONNECTED = -2,

    AUTO_STATUS_TIMEOUT = -3,

    AUTO_STATUS_SERVICE_MISSING = -4,

    AUTO_STATUS_REJECTED = -5

} AutoStatus;



// Mapped shell commands for Bluetooth automation

#define CMD_BT_ENABLE       "cmd bluetooth_manager enable"

#define CMD_BT_DISABLE      "cmd bluetooth_manager disable"

#define CMD_BT_IS_ENABLED   "cmd bluetooth_manager is-enabled"

#define CMD_BT_PAIR_DEVICE  "cmd bluetooth pair %s"

#define CMD_BT_UNPAIR_DEVICE "cmd bluetooth unpair %s"

#define CMD_BT_GET_BONDED   "cmd bluetooth get-bonded-devices"



// Mapped shell commands for Wi-Fi Direct (P2P) automation

#define CMD_WIFI_P2P_INIT   "cmd wifi p2p-init"

#define CMD_WIFI_P2P_PEER_C "cmd wifi p2p-connect-peer %s %s" // MAC address and PIN/WPS (PBC, PIN, KEYPAD)

#define CMD_WIFI_P2P_STOP   "cmd wifi p2p-cancel-connect"

#define CMD_WIFI_P2P_STATUS "cmd wifi p2p-status"

#define CMD_WIFI_P2P_DISCOVER "cmd wifi p2p-find"

#define CMD_WIFI_P2P_PEERS   "cmd wifi p2p-peers"



// Automation context wrapping ADB session

typedef struct {

    int adb_port;

    void *adb_session_ptr; // Points to active AdbSession

    uint8_t is_initialized;

} AutoContext;



#ifdef __cplusplus

}

#endif



#endif // NATIVE_AUTOMATION_COMMON_H