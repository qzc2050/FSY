#ifndef __ENV_MONITOR_H
#define __ENV_MONITOR_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pcf8563.h"


#ifdef __cplusplus
extern "C" {
#endif


// 环境监测数据
typedef struct{
    float temperature;
    float humidity;
    float baro;
    // float doserate;
    
    uint16_t CO2;
    uint16_t PM2_5;

    DateTime_t dt;
}Environment_Data_t;

extern Environment_Data_t env_data;
extern uint8_t current_year;
extern uint8_t current_month;
extern uint8_t current_day;
extern bool update_sys_cfg;





extern void datetime_setup_init(void);
extern void update_day_roller_options(void);
extern void refresh_env_data(void);
extern void create_refresh_timer(void);


#ifdef __cplusplus
} /*extern "C"*/
#endif


#endif

