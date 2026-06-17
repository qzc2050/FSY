#ifndef _PCF8563_H_
#define _PCF8563_H_

#ifdef _PCF8563_C_
#define _PCF8563_C_EXT_  
#define _PCF8563_C_EXT_INT_ //
#else
#define _PCF8563_C_EXT_  extern
#define _PCF8563_C_EXT_INT_ extern
#endif

#include <stdint.h>
#include <stdbool.h>

#define PCF8563_READ_ADDR    0xa3
#define PCF8563_WRITE_ADDR   0xa2


struct time_type__//BCD
{
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t week;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
};


_PCF8563_C_EXT_ void pcf8563_init(void);
_PCF8563_C_EXT_ void pcf8563_set_cur_time(struct time_type__ *tTime);
_PCF8563_C_EXT_ void pcf8563_get_cur_time(struct time_type__ *tTime);

uint32_t Get_Date_uint(void);
void cheak_date(uint8_t sta);
void DateTime_Refresh(bool upd);
void Time_CAL(void);
void usart_pcf8563_get_cur_time(void);
void pcf8563_set_cap(uint8_t val);
#endif 






