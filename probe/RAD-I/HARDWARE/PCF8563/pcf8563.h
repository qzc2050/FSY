#ifndef _PCF8563_H_
#define _PCF8563_H_


#include <stdint.h>
#include <stdbool.h>

#define PCF8563_ADDRESS      0xA2
#define PCF8563_WRITE_ADDR   0xA2
#define PCF8563_READ_ADDR    0xA3

typedef struct time_type__
{
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t week;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
}DateTime_t;


extern struct time_type__ date_time;

extern void pcf8563_init(void);
extern void pcf8563_set_cur_time(struct time_type__ *tTime);
extern void pcf8563_get_cur_time(struct time_type__ *tTime);

uint32_t Get_Date_uint(void);
void cheak_date(uint8_t sta);
void DateTime_Refresh(bool upd);
void Time_CAL(void);
void usart_pcf8563_get_cur_time(void);
void pcf8563_set_cap(uint8_t val);
#endif 






