#ifndef __FREERTOS_APP_H
#define __FREERTOS_APP_H


#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdint.h>


/* 用户头文件 */
#include "core/dev_config.h"
#include "core/dev_protocol.h"
#include "net_raw/net_raw_protocol.h"
#include "net_raw/net_raw_app.h"
#include "net_raw/net_raw_bsp.h"

#include "tim.h"
#include "quadspi.h"

#include "cmd.h"
#include "key.h"
#include "beep.h"
#include "pm2_5.h"
#include "geiger.h"
#include "w25qxx.h"
#include "ws2812b.h"
#include "pcf8563.h"
#include "lcd_rgb.h"

#include "w5500.h"
#include "socket.h"
#include "device.h"
#include "config.h"
#include "w5500_dhcp.h"
#include "network_cmd.h"

#include "ENS160.h"
#include "BME280_app.h"
#include "AHT20.h"
#include "geiger.h"
#include "joystick.h"

#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lv_demo_stress.h"

#include "ui.h"


#include "usbd_msc.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_storage.h" 
#include "flash_fs_mutex.h"


void freertos_task(void);

/* USB MSC（U盘功能）开关控制接口 */
void USB_MSC_SetEnable(uint8_t enable);
uint8_t USB_MSC_GetEnable(void);
uint8_t USB_MSC_IsStarted(void);

#endif
