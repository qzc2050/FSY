#ifndef _SPI_H_
#define _SPI_H_
#include "main.h"
#ifdef _SPI3_C_
#define _SPI3_C_EXT_  
#define _SPI3_C_EXT_INT_ //
#else
#define _SPI3_C_EXT_  extern
#define _SPI3_C_EXT_INT_ extern
#endif

extern SPI_HandleTypeDef SPI1_Handler;  //SPI句柄
extern SPI_HandleTypeDef SPI4_Handler;  //SPI句柄

// SPI总线速度设置 
//#define SPI_SPEED_2   		0
//#define SPI_SPEED_4   		1
//#define SPI_SPEED_8   		2
//#define SPI_SPEED_16  		3
//#define SPI_SPEED_32 		4
//#define SPI_SPEED_64 		5
//#define SPI_SPEED_128 		6
//#define SPI_SPEED_256 		7



//_SPI3_C_EXT_ void spi1_init(void);
//_SPI3_C_EXT_ void spi1_set_speed(u8 SpeedSet);
_SPI3_C_EXT_ uint8_t spi1_read_write_byte(uint8_t TxData);

//_SPI3_C_EXT_ void spi4_init(void);
//_SPI3_C_EXT_ void spi4_set_speed(uint8_t SpeedSet);
_SPI3_C_EXT_ uint8_t spi4_read_write_byte(uint8_t TxData);


#endif
