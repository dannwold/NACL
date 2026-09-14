#ifndef LIBLOCATION_H

#define LIBLOCATION_H



#include <stdint.h>



#ifdef __cplusplus

extern "C" {

#endif



typedef struct {

    double latitude;

    double longitude;

    double altitude;

    float accuracy;

    float speed;

    uint64_t timestamp_ms;

} GnssLocation;



typedef void (*LocationCallback)(const GnssLocation *location, void *user_data);

typedef void (*NmeaCallback)(const char *nmea_sentence, uint64_t timestamp_ms, void *user_data);



int location_init(void);

int location_start_updates(LocationCallback loc_cb, NmeaCallback nmea_cb, void *user_data);

void location_stop_updates(void);

void location_shutdown(void);



#ifdef __cplusplus

}

#endif



#endif // LIBLOCATION_H