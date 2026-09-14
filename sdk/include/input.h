#ifndef LIBINPUT_H

#define LIBINPUT_H



#include <stdint.h>



#ifdef __cplusplus

extern "C" {

#endif



typedef enum {

    INPUT_EVENT_TOUCH_DOWN = 1,

    INPUT_EVENT_TOUCH_MOVE = 2,

    INPUT_EVENT_TOUCH_UP = 3,

    INPUT_EVENT_KEY_DOWN = 4,

    INPUT_EVENT_KEY_UP = 5

} InputEventType;



typedef struct {

    InputEventType type;

    int32_t x;

    int32_t y;

    int32_t key_code;

    uint64_t timestamp_ns;

} NativeInputEvent;



typedef void (*InputEventCallback)(const NativeInputEvent *event, void *user_data);



int input_init(void);

int input_inject_tap_adb(int local_adb_port, int32_t x, int32_t y);

int input_inject_swipe_adb(int local_adb_port, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t duration_ms);



int input_start_monitoring(InputEventCallback cb, void *user_data);

void input_stop_monitoring(void);

void input_shutdown(void);



#ifdef __cplusplus

}

#endif



#endif // LIBINPUT_H