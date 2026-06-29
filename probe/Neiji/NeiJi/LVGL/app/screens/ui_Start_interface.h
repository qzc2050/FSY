#ifndef UI_START_INTERFACE_H
#define UI_START_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

extern lv_obj_t *ui_Start_interface;

void ui_Start_interface_screen_init(void);
void ui_Start_interface_screen_destroy(void);
void ui_Start_interface_screen_relocalize(void);

#ifdef __cplusplus
}
#endif

#endif
