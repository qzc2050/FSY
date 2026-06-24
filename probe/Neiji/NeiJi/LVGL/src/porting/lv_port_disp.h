#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void lv_port_disp_init(void);

void disp_enable_update(void);
void disp_disable_update(void);

/** refresh 前设置，用于 flush 时统计 label 区域像素 */
void lv_port_disp_set_probe_area(const lv_area_t *area);

#ifdef __cplusplus
}
#endif

#endif
