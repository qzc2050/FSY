/***********************************************************************************
�ɶ���Ȼ�������޹�˾
WIZnet�ٷ������̣�09����һֱ������Ѵ����̣�Ϊ�ͻ��ṩ��������Ʒ���ۺ��ȫ��λ����
�绰��028-86127089     0755-86066647
���棺028-86127039
��ַ��http://www.hschip.com
���ڣ�2016-01

Ӳ��ƽ̨�� ��Ȼ����������  HS-EVBW5500 /STM32
W5500 ��������QQȺ�� 722479032
WIZnet��������QQȺ�� 290473222
										 
SPIģʽ1��SPIģʽ2ͨ��spi.c�ļ���
#define MODE_SPI  1     // ѡ��SPI1  
#define MODE_SPI  2     // ѡ��SPI2
������ѡ��
***********************************************************************************/

#ifndef _TYPE_H_
#define _TYPE_H_

#define	MAX_SOCK_NUM		8	/**< Maxmium number of socket  */
//#define	STM32F10X_CL
#define __DEF_IINCHIP_MAP_BASE__ 0x0000
 #define COMMON_BASE 0x0000
#define __DEF_IINCHIP_MAP_TXBUF__ (COMMON_BASE + 0x8000) /* Internal Tx buffer address of the iinchip */
#define __DEF_IINCHIP_MAP_RXBUF__ (COMMON_BASE + 0xC000) /* Internal Rx buffer address of the iinchip */
//#define __DEF_IINCHIP_PPP

#include "FreeRTOS.h"
#include "task.h"

/* 实现在 w5500.c：仅用 BASEPRI，不碰 uxCriticalNesting（见该文件注释） */
void iinchip_spi_enter(void);
void iinchip_spi_exit(void);

#define IINCHIP_ISR_DISABLE()   iinchip_spi_enter()
#define IINCHIP_ISR_ENABLE()  iinchip_spi_exit()

#ifndef NULL
#define NULL		((void *) 0)
#endif

//typedef enum { false, true } bool;

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif


typedef char int8;

typedef volatile char vint8;

typedef unsigned char uint8;

typedef volatile unsigned char vuint8_t;



typedef int int16;

typedef unsigned short uint16;

typedef long int32;

typedef unsigned long uint32;

typedef uint8			u_char;		/**< 8-bit value */
typedef uint8 			SOCKET;
typedef uint16			u_short;	/**< 16-bit value */
typedef uint16			u_int;		/**< 16-bit value */
typedef uint32			u_long;		/**< 32-bit value */

typedef union _un_l2cval 
{
	u_long	lVal;
	u_char	cVal[4];
}un_l2cval;

typedef union _un_i2cval 
{
	u_int	iVal;
	u_char	cVal[2];
}un_i2cval;

#endif		/* _TYPE_H_ */
