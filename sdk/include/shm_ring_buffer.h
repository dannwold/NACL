#ifndef SHM_RING_BUFFER_H

#define SHM_RING_BUFFER_H



#include <stdint.h>

#include <stdbool.h>

#include <stdatomic.h>



#define BLE_SHM_BUFFER_SIZE (1024 * 64) // 64KB Ring Buffer

#define SHM_NAME_BLE "nacl_ble_shm_buffer"



#pragma pack(push, 1)



// Individual data packet inside the shared memory segment

typedef struct {

    uint64_t timestamp_ns;  // Monotonic system timestamp

    uint16_t packet_len;    // Length of raw payload data

    uint8_t  status;        // Frame status flags

    uint8_t  data[512];     // Raw BLE payload buffer

} BleShmPacket;



// SPSC Circular Ring Buffer structure mapped into shared RAM (Hardened)

typedef struct {

    atomic_uint head;             // Read pointer (modified by Client)

    atomic_uint tail;             // Write pointer (modified by Daemon)

    uint32_t    capacity;         // Total packet slots in ring

    uint32_t    slot_size;        // Size of each individual BleShmPacket

    atomic_bool is_active;        // Heartbeat check for daemon status

    atomic_uint pre_generation;   // TOCTOU mitigation: Incremented before daemon writes

    atomic_uint post_generation;  // TOCTOU mitigation: Incremented after daemon writes

    BleShmPacket slots[128];       // Circular queue slots

} BleShmRingBuffer;



#pragma pack(pop)



#endif // SHM_RING_BUFFER_H