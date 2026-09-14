#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include "automation_common.h"

#include "adb_client.h"



// Execute a shell command over the active on-device ADB session and return the raw output buffer

static int execute_adb_shell_cmd(AdbSession *session, const char *cmd, char *out_buf, uint32_t max_out_len) {

    if (!session || session->state != ADB_STATE_CONNECTED) {

        return AUTO_STATUS_ADB_DISCONNECTED;

    }



    // Open a dynamic shell channel specifically for this command

    char shell_cmd[512];

    snprintf(shell_cmd, sizeof(shell_cmd), "shell:%s", cmd);



    if (adb_open_shell_channel(session, shell_cmd) < 0) {

        fprintf(stderr, "[Automation] Failed to open shell channel for: %s\n", cmd);

        return AUTO_STATUS_ERROR;

    }



    uint32_t total_bytes = 0;

    uint8_t temp_buf[2048];

    uint32_t bytes_read = 0;



    // Read the output stream until the channel is closed or EOF is hit

    while (adb_read_shell_data(session, temp_buf, sizeof(temp_buf), &bytes_read) == 0 && bytes_read > 0) {

        if (out_buf && total_bytes < max_out_len - 1) {

            uint32_t copy_len = (bytes_read < (max_out_len - 1 - total_bytes)) ? bytes_read : (max_out_len - 1 - total_bytes);

            memcpy(out_buf + total_bytes, temp_buf, copy_len);

            total_bytes += copy_len;

        }

    }



    if (out_buf) {

        out_buf[total_bytes] = '\0';

    }



    return AUTO_STATUS_OK;

}



// Check Bluetooth service status and enable Bluetooth if currently disabled

__attribute__((visibility("default")))

int auto_ensure_bluetooth_enabled(AutoContext *ctx) {

    if (!ctx || !ctx->adb_session_ptr) return AUTO_STATUS_ERROR;

    AdbSession *session = (AdbSession *)ctx->adb_session_ptr;



    char out_buf[128];

    int res = execute_adb_shell_cmd(session, CMD_BT_IS_ENABLED, out_buf, sizeof(out_buf));

    if (res != AUTO_STATUS_OK) return res;



    if (strstr(out_buf, "true") != NULL) {

        printf("[Automation] Bluetooth is already enabled.\n");

        return AUTO_STATUS_OK;

    }



    printf("[Automation] Enabling Bluetooth via on-device UID 2000...\n");

    res = execute_adb_shell_cmd(session, CMD_BT_ENABLE, out_buf, sizeof(out_buf));

    if (res != AUTO_STATUS_OK) return res;



    // Fast loop to wait for state change

    for (int i = 0; i < 10; ++i) {

        usleep(500000); // Wait 500ms

        execute_adb_shell_cmd(session, CMD_BT_IS_ENABLED, out_buf, sizeof(out_buf));

        if (strstr(out_buf, "true") != NULL) {

            return AUTO_STATUS_OK;

        }

    }



    return AUTO_STATUS_TIMEOUT;

}



// Programmatically initiate Bluetooth GATT pairing without root or popups

__attribute__((visibility("default")))

int auto_pair_bluetooth_device(AutoContext *ctx, const char *mac_address) {

    if (!ctx || !ctx->adb_session_ptr || !mac_address) return AUTO_STATUS_ERROR;

    AdbSession *session = (AdbSession *)ctx->adb_session_ptr;



    printf("[Automation] Querying bonded devices before pairing...\n");

    char out_buf[1024];

    int res = execute_adb_shell_cmd(session, CMD_BT_GET_BONDED, out_buf, sizeof(out_buf));

    if (res == AUTO_STATUS_OK && strstr(out_buf, mac_address) != NULL) {

        printf("[Automation] Device %s is already bonded/paired.\n", mac_address);

        return AUTO_STATUS_OK;

    }



    printf("[Automation] Dispatching pair command to Shell Bluetooth System Service for MAC: %s\n", mac_address);

    char cmd[256];

    snprintf(cmd, sizeof(cmd), CMD_BT_PAIR_DEVICE, mac_address);



    char pair_res[512];

    res = execute_adb_shell_cmd(session, cmd, pair_res, sizeof(pair_res));

    if (res != AUTO_STATUS_OK) return res;



    // AOSP pairing shell tool output parsing

    if (strstr(pair_res, "Successful") != NULL || strstr(pair_res, "paired") != NULL || strstr(pair_res, "bond_bonded") != NULL) {

        printf("[Automation] Bluetooth pairing succeeded for %s.\n", mac_address);

        return AUTO_STATUS_OK;

    }



    // Unseen Issue Mitigation: Check if pairing is stuck in "ConsentDialog" mode.

    // If so, we can programmatically dispatch a keypress event via shell input to auto-confirm pairing!

    if (strstr(pair_res, "consent") != NULL || strstr(pair_res, "dialog") != NULL || strstr(pair_res, "user") != NULL) {

        printf("[Automation] Pairing requires user consent. Injecting automated programmatic confirmation key events...\n");



        // Simulates down arrow to highlight "Pair" button, then executes ENTER keypress

        execute_adb_shell_cmd(session, "input keyevent KEYCODE_DPAD_DOWN", NULL, 0);

        usleep(100000); // 100ms delay for system transitions

        execute_adb_shell_cmd(session, "input keyevent KEYCODE_DPAD_RIGHT", NULL, 0);

        usleep(100000);

        execute_adb_shell_cmd(session, "input keyevent KEYCODE_ENTER", NULL, 0);



        // Re-evaluate if bonding completed

        usleep(500000);

        execute_adb_shell_cmd(session, CMD_BT_GET_BONDED, out_buf, sizeof(out_buf));

        if (strstr(out_buf, mac_address) != NULL) {

            printf("[Automation] Multi-step device pairing completed successfully.\n");

            return AUTO_STATUS_OK;

        }

    }



    fprintf(stderr, "[Automation] Bluetooth pairing failed: %s\n", pair_res);

    return AUTO_STATUS_REJECTED;

}



// Programmatically configure and join a Wi-Fi Direct (P2P) network group

__attribute__((visibility("default")))

int auto_establish_wifi_p2p_connection(AutoContext *ctx, const char *peer_mac, const char *connection_mode) {

    if (!ctx || !ctx->adb_session_ptr || !peer_mac) return AUTO_STATUS_ERROR;

    AdbSession *session = (AdbSession *)ctx->adb_session_ptr;



    printf("[Automation] Ensuring Wi-Fi Direct (P2P) interface is initialized...\n");

    char out_buf[1024];

    int res = execute_adb_shell_cmd(session, CMD_WIFI_P2P_INIT, out_buf, sizeof(out_buf));

    if (res != AUTO_STATUS_OK) return res;



    printf("[Automation] Scanning for local Wi-Fi P2P peers...\n");

    execute_adb_shell_cmd(session, CMD_WIFI_P2P_DISCOVER, NULL, 0);

    usleep(1000000); // Wait 1 second for the hardware transceiver to discover channels



    printf("[Automation] Auditing discovered P2P peers...\n");

    res = execute_adb_shell_cmd(session, CMD_WIFI_P2P_PEERS, out_buf, sizeof(out_buf));

    if (res == AUTO_STATUS_OK && strstr(out_buf, peer_mac) == NULL) {

        fprintf(stderr, "[Automation] Peer %s was not discovered in the surrounding area.\n", peer_mac);

        return AUTO_STATUS_ERROR;

    }



    // Default connection mode to Push Button Configuration (PBC) if not specified

    const char *mode = (connection_mode) ? connection_mode : "pbc";

    printf("[Automation] Linking to peer %s using mode: %s...\n", peer_mac, mode);



    char cmd[256];

    snprintf(cmd, sizeof(cmd), CMD_WIFI_P2P_PEER_C, peer_mac, mode);



    char conn_res[512];

    res = execute_adb_shell_cmd(session, cmd, conn_res, sizeof(conn_res));

    if (res != AUTO_STATUS_OK) return res;



    if (strstr(conn_res, "Success") != NULL || strstr(conn_res, "initiated") != NULL) {

        printf("[Automation] Wi-Fi Direct (P2P) linkage successfully initiated.\n");

        return AUTO_STATUS_OK;

    }



    fprintf(stderr, "[Automation] Wi-Fi P2P Connection initiation failed: %s\n", conn_res);

    return AUTO_STATUS_REJECTED;

}