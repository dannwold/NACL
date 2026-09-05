#ifndef NATIVE_ROUTING_CORE_H

#define NATIVE_ROUTING_CORE_H



#include <stdint.h>

#include <stdbool.h>



#ifdef __cplusplus

extern "C" {

#endif



// Define supported execution pathways

typedef enum {

    PATHWAY_UNINITIALIZED = 0,

    PATHWAY_BINDER        = 1, // Raw libbinder transact

    PATHWAY_JNI           = 2, // Java Native Interface framework callback

    PATHWAY_UNSUPPORTED   = -1

} ExecutionPathway;



// Generic function pointer definition for subsystem control

typedef int (*HardwareCommandFunc)(const uint8_t *payload, uint32_t payload_len, uint8_t *out_buffer, uint32_t *out_len);



// Dynamic Route Registry mapping a capability to its optimal version-specific implementation

typedef struct {

    const char *capability_name;

    int min_sdk_version;

    ExecutionPathway active_pathway;

    HardwareCommandFunc execute;

} CapabilityRoute;



// Core initialization and routing methods

int initialize_routing_engine();

ExecutionPathway resolve_capability_pathway(const char *capability);

int dispatch_hardware_command(const char *capability, const uint8_t *payload, uint32_t payload_len, uint8_t *out_buffer, uint32_t *out_len);



#ifdef __cplusplus

}

#endif



#endif // NATIVE_ROUTING_CORE_H