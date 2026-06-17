#ifndef __OLED_H
#define __OLED_H 

#include "stm32l0xx.h"

#define USE_HORIZONTAL 0  //设置显示方向 0：正向显示；1：旋转180度显示


//SCL=SCLK 
//SDA=MOSI
//RES=RES
//DC=DC
//CS=CS

//-----------------OLED端口定义---------------- 

//#define OLED_SCL_Clr() HAL_GPIO_WritePin(GPIOB,GPIO_PIN_3, GPIO_PIN_RESET)//CLK
//#define OLED_SCL_Set() HAL_GPIO_WritePin(GPIOB,GPIO_PIN_3, GPIO_PIN_SET)

//#define OLED_SDA_Clr() HAL_GPIO_WritePin(GPIOA,GPIO_PIN_12, GPIO_PIN_RESET)//DIN
//#define OLED_SDA_Set() HAL_GPIO_WritePin(GPIOA,GPIO_PIN_12, GPIO_PIN_SET)

#define OLED_RES_Clr() HAL_GPIO_WritePin(GPIOB,GPIO_PIN_7, GPIO_PIN_RESET)//RES
#define OLED_RES_Set() HAL_GPIO_WritePin(GPIOB,GPIO_PIN_7, GPIO_PIN_SET)

#define OLED_DC_Clr() HAL_GPIO_WritePin(GPIOB,GPIO_PIN_6, GPIO_PIN_RESET)//DC
#define OLED_DC_Set() HAL_GPIO_WritePin(GPIOB,GPIO_PIN_6, GPIO_PIN_SET)
 
#define OLED_CS_Clr()  HAL_GPIO_WritePin(GPIOB,GPIO_PIN_4, GPIO_PIN_RESET)//CS
#define OLED_CS_Set()  HAL_GPIO_WritePin(GPIOB,GPIO_PIN_4, GPIO_PIN_SET)

//显示屏功能函数
//void OLED_WR_REG(uint8_t reg);//写入一个指令
//void OLED_WR_Byte(uint8_t dat);//写入一个数据
//void OLED_AddressSet(uint8_t x,uint8_t y);//设置起始坐标函数
//void OLED_Clear(void);//清屏函数
////void OLED_Fill(uint16_t x1,uint8_t y1,uint16_t x2,uint8_t y2,uint8_t color);//填充函数
//void OLED_ShowChinese(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey,uint8_t mode);//显示汉字串
//void OLED_ShowChinese12x12(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey,uint8_t mode);
//void OLED_ShowChinese16x16(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey,uint8_t mode);//显示16x16汉字
////void OLED_ShowChinese24x24(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey,uint8_t mode);//显示24x24汉字
////void OLED_ShowChinese32x32(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey,uint8_t mode);//显示32x32汉字
//void OLED_ShowChar12x12(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey,uint8_t mode);
//void OLED_ShowChar16x16(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey,uint8_t mode);
//void OLED_ShowChar32x32(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey,uint8_t mode);
//void OLED_ShowChar(uint8_t x,uint8_t y,uint8_t chr,uint8_t sizey,uint8_t mode);//显示单个字符
//void OLED_ShowString(uint8_t x,uint8_t y,uint8_t *dp,uint8_t sizey,uint8_t mode);//显示字符串
//void OLED_ShowNum(uint8_t x,uint8_t y,uint32_t num,uint16_t len,uint8_t sizey,uint8_t mode);//显示整数变量
//void OLED_DrawBMP(uint8_t x,uint8_t y,uint16_t length,uint8_t width,const uint8_t BMP[],uint8_t mode);//显示灰度图片
//void OLED_DrawSingleBMP(uint8_t x,uint8_t y,uint16_t length,uint8_t width,const uint8_t BMP[],uint8_t mode);//显示单色图片
//void OLED_GPIO_Init(void);
//void OLED_Init(void);
//void OLED_Draw_Fill(uint8_t x1,uint8_t y1,uint8_t x_length,uint8_t y_length,uint8_t data,uint8_t mode);



void OLED_WR_REG(uint8_t reg);//写入一个指令
void OLED_WR_Byte(uint8_t dat);//写入一个数据
void OLED_AddressSet(uint8_t x,uint8_t y);//设置起始坐标函数
void OLED_Clear(void);//清屏函数
//void OLED_Fill(uint16_t x1,uint8_t y1,uint16_t x2,uint8_t y2,uint8_t color);//填充函数
void OLED_ShowChinese(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey);//显示汉字串
void OLED_ShowChinese12x12(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey);
void OLED_ShowChinese16x16(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey);//显示16x16汉字
//void OLED_ShowChinese24x24(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey,uint8_t mode);//显示24x24汉字
//void OLED_ShowChinese32x32(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey,uint8_t mode);//显示32x32汉字
void OLED_ShowChar12x12(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey);
void OLED_ShowChar16x16(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey);
void OLED_ShowChar32x32(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey);
void OLED_ShowChar(uint8_t x,uint8_t y,uint8_t chr,uint8_t sizey);//显示单个字符
void OLED_ShowString(uint8_t x,uint8_t y,uint8_t *dp,uint8_t sizey);//显示字符串
void OLED_ShowNum(uint8_t x,uint8_t y,uint32_t num,uint16_t len,uint8_t sizey);//显示整数变量
void OLED_DrawBMP(uint8_t x,uint8_t y,uint16_t length,uint8_t width,const uint8_t BMP[]);//显示灰度图片
void OLED_DrawSingleBMP(uint8_t x,uint8_t y,uint16_t length,uint8_t width,const uint8_t BMP[]);//显示单色图片
void OLED_GPIO_Init(void);
void OLED_Init(void);
void OLED_Draw_Fill(uint8_t x1,uint8_t y1,uint8_t x_length,uint8_t y_length,uint8_t data);
#endif

