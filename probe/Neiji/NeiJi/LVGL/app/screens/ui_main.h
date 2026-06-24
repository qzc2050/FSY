#ifndef NEIJI_UI_MAIN_H
#define NEIJI_UI_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

extern lv_obj_t *ui_main_dose_label;
extern lv_obj_t *ui_main_status_label;

void ui_main_create(void);
void ui_main_set_dose(float dose_usv_h);
void ui_main_set_status(const char *text);

#ifdef __cplusplus
}
#endif

#endif
