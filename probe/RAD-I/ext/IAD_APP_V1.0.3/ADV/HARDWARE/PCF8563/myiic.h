#ifndef __MYIIC_H
#define __MYIIC_H
#include "stm32L0xx.h"
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK战舰STM32开发板
//IIC驱动 代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//修改日期:2012/9/9
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2009-2019
//All rights reserved									  
//////////////////////////////////////////////////////////////////////////////////

#define I2C1_SCL_Pin GPIO_PIN_8
#define I2C1_SCL_GPIO_Port GPIOB
#define I2C1_SDA_Pin GPIO_PIN_9
#define I2C1_SDA_GPIO_Port GPIOB


////IO方向设置
#define SDA_IN()  {GPIOB->MODER&=0XFFF3FFFF;GPIOB->PUPDR&=0XFFF3FFFF;GPIOB->PUPDR|=0X0004000;}
#define SDA_OUT() {GPIOB->MODER&=0XFFF3FFFF;GPIOB->MODER|=0X00040000;\
                   GPIOB->OTYPER&=0XFFFFFDFF;}

//IO操作函数	 
#define IIC_SCL(n)    HAL_GPIO_WritePin(I2C1_SCL_GPIO_Port,I2C1_SCL_Pin,n)  //输出
#define IIC_SDA(n)    HAL_GPIO_WritePin(GPIOB,I2C1_SDA_Pin,n)  //输出	 
#define READ_SDA      HAL_GPIO_ReadPin(GPIOB,I2C1_SDA_Pin) //输入 

//IIC所有操作函数
void IIC_Init(void);        //初始化IIC的IO口				 
void IIC_Start(void);				//发送IIC开始信号
void IIC_Stop(void);	  		//发送IIC停止信号
uint8_t IIC_Send_Byte(uint8_t txd);			  //IIC发送一个字节
uint8_t IIC_Read_Byte(unsigned char ack); //IIC读取一个字节
uint8_t IIC_Wait_Ack(void); 			      	//IIC等待ACK信号
void IIC_Ack(void);				  //IIC发送ACK信号
void IIC_NAck(void);				//IIC不发送ACK信号

void IIC_Write_One_Byte(uint8_t daddr,uint8_t addr,uint8_t data);
uint8_t IIC_Read_One_Byte(uint8_t daddr,uint8_t addr);	  
#endif
















