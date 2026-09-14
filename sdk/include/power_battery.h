#ifndef LIBPOWER_BATTERY_H

#define LIBPOWER_BATTERY_H



#include <stdint.h>

#include <stdbool.h>



#ifdef __cplusplus

extern "C" {

#endif



typedef struct {

    float voltage_v;

    float current_now_a;

    float capacity_pct;

    float temperature_c;

    bool is_charging;

} BatteryStats;



int power_init(void);

int power_get_battery_stats(BatteryStats *out_stats);

int power_acquire_wakelock(const char *lock_name);

int power_release_wakelock(const char *lock_name);

void power_shutdown(void);



#ifdef __cplusplus

}

#endif



#endif // LIBPOWER_BATTERY_H