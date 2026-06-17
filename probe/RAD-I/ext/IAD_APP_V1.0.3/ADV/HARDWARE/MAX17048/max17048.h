#ifndef __MAX17048_H
#define __MAX17048_H

#include "main.h"

////IO方向设置
//#define POWER_CTR_SDA_IN()  {GPIOB->MODER&=0XFFF3FFFF;GPIOB->PUPDR&=0XFFF3FFFF;GPIOB->PUPDR|=0X0004000;}
//#define POWER_CTR_SDA_OUT() {GPIOB->MODER&=0XFFF3FFFF;GPIOB->MODER|=0X00040000;\
//                             GPIOB->OTYPER&=0XFFFFFDFF;}

////IO操作函数	 
//#define POWER_CTR_IIC_SCL(n)    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_8,n)  //输出 
//#define POWER_CTR_IIC_SDA(n)    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_9,n)  //输出
//#define POWER_CTR_READ_SDA      HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_9)     //输入 

//读写命令+地址
#define MAX17048_WRITE_ADDRESS  (0x6C)
#define MAX17048_READ_ADDRESS   (0x6D)

#ifdef _POWER_CTR_C_
#define _MAX17048_C_EXT_  
#define _MAX17048_C_EXT_INT_ //
#else
#define _MAX17048_C_EXT_  extern
#define _MAX17048_C_EXT_INT_ extern
#endif


typedef enum
{
    REGISTER_VCELL = 0x02,
    REGISTER_SOC = 0x04,
    REGISTER_MODE = 0x06,
    REGISTER_VERSION = 0x08,
    REGISTER_HIBRT = 0x0A,
    REGISTER_CONFIG = 0x0C,
    REGISTER_VALRT = 0x14,
    REGISTER_CRATE = 0x16,
    REGISTER_VRESET_ID = 0x18,
    REGISTER_STATUS = 0x1A,
    REGISTER_TABLE_START = 0x40,
    REGISTER_CMD = 0xFE
    
} max_register_t;

typedef enum
{
	BIT1 = 0x0001,
	BIT2 = 0x0002,
	BIT3 = 0x0004,
	BIT4 = 0x0008,
	BIT5 = 0x0010,
	BIT6 = 0x0020,
	BIT7 = 0x0040,
	BIT8 = 0x0080,
	BIT9 = 0x0100,
	BIT10 = 0x0200,
	BIT11 = 0x0400,
	BIT12 = 0x0800,
	BIT13 = 0x1000,
	BIT14 = 0x2000,
	BIT15 = 0x4000,
	BIT16 = 0x8000 
} uint16_bit_t;

typedef enum
{
	MAX17048_NULL_CTR = 0,
  MAX17048_CHARGE_STA,
	MAX17048_ATHD_STA
    
} MAX17048_cmd_t;

#define MAX17048_RCOMP0	                    (0x97u)

_MAX17048_C_EXT_ uint8_t max17048_write_rag(max_register_t reg, uint8_t *p_data, uint8_t len);
_MAX17048_C_EXT_ void max17048_read_rag(max_register_t reg, uint8_t *p_data, uint8_t len);
_MAX17048_C_EXT_ uint16_t max17048_get_verison(void);
_MAX17048_C_EXT_ void max17048_get_percent(uint8_t *perc);
_MAX17048_C_EXT_ void max17048_get_millivolt(float *milv);
_MAX17048_C_EXT_ uint16_t max17048_get_config(void);
_MAX17048_C_EXT_ uint16_t max17048_get_status(void);
_MAX17048_C_EXT_ void MAX17048_SET_ATHD(uint8_t ATHD);
_MAX17048_C_EXT_ void MAX17048_EN_SOC_ALERT(void);
//_MAX17048_C_EXT_ uint16_t MAX17048_DEAL_STATUS(void);
//_MAX17048_C_EXT_ void MAX17048_STATUS_INIT(void);
//_MAX17048_C_EXT_ void MAX17048_CLR_ALERT(uint16_t bit);

_MAX17048_C_EXT_ void MAX17048_POR(void);
_MAX17048_C_EXT_ void MAX17048_Compensation(uint8_t tem);
_MAX17048_C_EXT_ void MAX17048_SleepEnable(void);
_MAX17048_C_EXT_ void MAX17048_Sleep(uint8_t On_Off);
_MAX17048_C_EXT_ void MAX17048_QStart(void);



//_MAX17048_C_EXT_ void MAX17048_SET_HIBRT(void);

_MAX17048_C_EXT_ float max17048G_T10_compensate_capacity(float cap, float v_meas, uint16_t v_divider, float nominal_voltage, float initial_capacity);
//IIC所有操作函数
//void POWER_CTR_IIC_Init(void);        //初始化IIC的IO口				 
//void POWER_CTR_IIC_Start(void);				//发送IIC开始信号
//void POWER_CTR_IIC_Stop(void);	  		//发送IIC停止信号
//uint8_t POWER_CTR_IIC_Send_Byte(uint8_t txd);			 //IIC发送一个字节
//uint8_t POWER_CTR_IIC_Read_Byte(unsigned char ack);//IIC读取一个字节
//uint8_t POWER_CTR_IIC_Wait_Ack(void); 				     //IIC等待ACK信号
//void POWER_CTR_IIC_Ack(void);					//IIC发送ACK信号
//void POWER_CTR_IIC_NAck(void);				//IIC不发送ACK信号

//void POWER_CTR_IIC_Write_One_Byte(uint8_t daddr,uint8_t addr,uint8_t data);
//uint8_t POWER_CTR_IIC_Read_One_Byte(uint8_t daddr,uint8_t addr);	

#endif
