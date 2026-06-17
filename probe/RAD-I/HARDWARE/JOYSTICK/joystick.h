#ifndef __JOYSTICK_H
#define __JOYSTICK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>


typedef enum{
    KEY_STATE_NULL,
    KEY_STATE_NEXT,
    KEY_STATE_PREV,
    KEY_STATE_UP,
    KEY_STATE_DOWN,
    KEY_STATE_LEFT,
    KEY_STATE_RIGHT,
    KEY_STATE_ENTER,
    KEY_STATE_ESC,
}KEY_STATE;


void button_adc_val_init(void);
uint8_t get_button_state(void);


#ifdef __cplusplus
}
#endif

#endif /* __JOYSTICK_H */


