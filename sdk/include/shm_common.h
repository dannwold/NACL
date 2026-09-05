#ifndef NATIVE_SHM_COMMON_H

#define NATIVE_SHM_COMMON_H



#include <stdint.h>

#include <stdatomic.h>



#ifdef __cplusplus

extern "C" {

#endif



#define SHM_SOCKET_PATH "/data/local/tmp/sdk/sockets/shm_broker.sock"

#define SHM_REGION_NAME "nacl_shared_telemetry"

#define SHM_REGION_SIZE 4096 // 4KB aligned page size



// Telemetry payload structure representing real-time hardware states

typedef struct {

    uint64_t timestamp_ns;    // Nanosecond monotonic timestamp

    float accelerometer[3];   // High-frequency X, Y, Z sensor values

    float gyroscope[3];       // Gyroscope X, Y, Z coordinates

    uint32_t wifi_signal_rssi;// Wi-Fi RSSI level (0 to -100 dBm)

    uint32_t bt_device_count; // Number of discovered BLE peripherals

} TelemetryData;



// Shared memory control header layout

typedef struct {

    atomic_uint seq_number;   // Monotonically increasing sequence number (atomic)

    atomic_bool is_writing;   // Lock-free write status flag

    TelemetryData data;       // Actual telemetry data

} SharedStateBuffer;



#ifdef __cplusplus

}

#endif



#endif // NATIVE_SHM_COMMON_H