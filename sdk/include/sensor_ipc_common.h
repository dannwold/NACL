#ifndef SENSOR_IPC_COMMON_H

#define SENSOR_IPC_COMMON_H



#include <stdint.h>



#ifdef __cplusplus

extern "C" {

#endif



// Unix Domain Socket path under privileged writable directory

#define IPC_SOCKET_DIR "/data/local/tmp/sdk/sockets"

#define IPC_SOCKET_SENS "/data/local/tmp/sdk/sockets/sensors.sock"



#define IPC_MAGIC_SIGNATURE 0x4E41434C // "NACL"



typedef enum {

    SUBSYSTEM_SENSORS = 3

} SubsystemType;



typedef enum {

    CMD_SENSORS_START_STREAM = 400,

    CMD_SENSORS_STOP_STREAM  = 401,

    CMD_SENSORS_GET_CAPS     = 402

} SensorCommandId;



typedef enum {

    STATUS_OK           = 0,

    STATUS_ERROR        = -1,

    STATUS_UNSUPPORTED  = -2

} StatusCode;



#pragma pack(push, 1)

typedef struct {

    uint32_t magic;

    uint32_t transaction_id;

    uint16_t subsystem;

    uint16_t command;

    int32_t  status;

    uint32_t payload_len;

} IpcHeader;



// Dedicated sensor data packet structure for high-frequency streaming

typedef struct {

    uint32_t sensor_type; // 1 = Accelerometer, 4 = Gyroscope

    uint64_t timestamp;   // Nanoseconds (uptime)

    float x;

    float y;

    float z;

    float accuracy;

} SensorDataEvent;

#pragma pack(pop)



#ifdef __cplusplus

}

#endif



#endif // SENSOR_IPC_COMMON_H