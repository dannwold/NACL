#ifndef NATIVE_TELEPHONY_COMMON_H

#define NATIVE_TELEPHONY_COMMON_H



#include <stdint.h>



#ifdef __cplusplus

extern "C" {

#endif



#define MAX_CARRIER_NAME_LEN 64

#define MAX_SIM_OPERATOR_LEN 16

#define MAX_IMEI_LEN         32

#define MAX_IMSI_LEN         32



// Cellular Radio Technologies

typedef enum {

    RADIO_TECH_UNKNOWN = 0,

    RADIO_TECH_GPRS,

    RADIO_TECH_EDGE,

    RADIO_TECH_UMTS,

    RADIO_TECH_HSDPA,

    RADIO_TECH_HSUPA,

    RADIO_TECH_HSPA,

    RADIO_TECH_CDMA,

    RADIO_TECH_EVDO_0,

    RADIO_TECH_EVDO_A,

    RADIO_TECH_EVDO_B,

    RADIO_TECH_1xRTT,

    RADIO_TECH_LTE,

    RADIO_TECH_EHRPD,

    RADIO_TECH_HSPAP,

    RADIO_TECH_GSM,

    RADIO_TECH_TD_SCDMA,

    RADIO_TECH_IWLAN,

    RADIO_TECH_LTE_CA,

    RADIO_TECH_NR // 5G New Radio

} CellularRadioTech;



// Cell Connection Status

typedef enum {

    CELL_CONN_NONE = 0,

    CELL_CONN_PRIMARY,

    CELL_CONN_SECONDARY

} CellConnStatus;



// Unified Cell Tower Metric Packet (Packed)

#pragma pack(push, 1)

typedef struct {

    uint8_t  type;           // Maps to CellularRadioTech

    uint8_t  status;         // Maps to CellConnStatus

    int32_t  dbm;            // General signal strength (RSSI) in dBm

    int32_t  rsrp;           // LTE/5G Reference Signal Received Power (dBm)

    int32_t  rsrq;           // LTE/5G Reference Signal Received Quality (dB)

    int32_t  rssnr;          // LTE/5G Signal-to-Noise Ratio (dB)

    int32_t  asu;            // Arbitrary Strength Unit



    // Identity Parameters

    int32_t  mcc;            // Mobile Country Code (2-3 digits)

    int32_t  mnc;            // Mobile Network Code (2-3 digits)

    int32_t  lac_or_tac;     // Location Area Code (GSM/UMTS) or Tracking Area Code (LTE/5G)

    int32_t  cid_or_ci;      // Cell Identity (GSM/UMTS, 16/28-bit) or Cell Identity (LTE 28-bit, 5G 36-bit)

    int32_t  pci_or_psc;     // Physical Cell ID (LTE/5G) or Primary Scrambling Code (UMTS)

    int32_t  earfcn_or_nrarfcn; // Absolute Radio Frequency Channel Number (LTE/5G)

} CellTowerMetric;



typedef struct {

    uint32_t active_subscription_count;

    char     carrier_name[MAX_CARRIER_NAME_LEN];

    char     sim_operator[MAX_SIM_OPERATOR_LEN];

    char     device_imei[MAX_IMEI_LEN];

    char     subscriber_imsi[MAX_IMSI_LEN];

    int32_t  data_state;     // 0 = Disconnected, 1 = Connecting, 2 = Connected, 3 = Suspended

    int32_t  sim_state;      // 0 = Unknown, 1 = Absent, 2 = Pin Required, 3 = Puk Required, 4 = Network Locked, 5 = Ready

} TelephonyState;

#pragma pack(pop)



#ifdef __cplusplus

}

#endif



#endif // NATIVE_TELEPHONY_COMMON_H