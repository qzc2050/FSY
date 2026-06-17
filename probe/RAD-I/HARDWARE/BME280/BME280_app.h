#ifndef __BME280_APP_H
#define __BME280_APP_H

#include <stdio.h>
#include <stdbool.h>


#define FILTER_BUFFER_SIZE    10


void bme280_app_init(void);
void bme280_get_real_data(float* temperature_out, float* humidity_out, float* baro_out);

#endif




