#ifndef NEIJI_UI_H
#define NEIJI_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

LV_FONT_DECLARE(ui_font_hansanbold24);
LV_FONT_DECLARE(ui_font_hansanbold32);
LV_FONT_DECLARE(ui_font_hansanbold48);
LV_FONT_DECLARE(ui_font_hansanbold64);

void ui_init(void);

#ifdef __cplusplus
}
#endif

#endif
