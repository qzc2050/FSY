/***********************************************************************************
成都浩然电子有限公司
WIZnet官方代理商，09年起一直蝉联最佳代理商，为客户提供技术、产品、售后等全方位服务
电话：028-86127089     0755-86066647
传真：028-86127039
网址：http://www.hschip.com
日期：2016-01

硬件平台： 浩然电子评估板  HS-EVBW5500 /STM32
W5500 技术交流QQ群： 722479032
WIZnet技术交流QQ群： 290473222

SPI模式1或SPI模式2通过spi.c文件的
#define MODE_SPI  1     // 选择SPI1  
#define MODE_SPI  2     // 选择SPI2
来进行选择
***********************************************************************************/

#ifndef _DEVICE_H_
#define _DEVICE_H_


#include "main.h"

//#include "stm32f10x.h"
//#include"stdio.h"
#define DEVICE_ID "W5500"

#define FW_VER_HIGH  	1
#define FW_VER_LOW    	0

#define W5500_RST_PIN             GPIO_PIN_6
#define W5500_RST_GPIO_PORT       GPIOC


#define   wlan_connect    HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1)

typedef  void (*pFunction)(void);
void w5500_rst_io_configuration(void);
void set_w5500_network(void);
void set_w5500_default(void);
void Reset_W5500(void);


#endif

