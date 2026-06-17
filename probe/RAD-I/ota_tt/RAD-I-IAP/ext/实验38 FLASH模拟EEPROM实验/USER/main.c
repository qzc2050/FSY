#include "sys.h" 
#include "usart.h" 
#include "delay.h" 
#include "led.h"
#include "key.h"
#include "lcd.h"  
#include "sdram.h" 
#include "ltdc.h"
#include "mpu.h"
#include "usmart.h"
#include "w25qxx.h"

#include "stmflash.h" 
//ALIENTEK 北极星STM32H750/F750开发板 实验38
//FLASH模拟EEPROM 实验 
//技术支持：www.openedv.com
//广州市星翼电子科技有限公司  

//要写入到STM32 FLASH的字符串数组
const u8 TEXT_Buffer[]={"STM32 FLASH TEST"};
#define TEXT_LENTH sizeof(TEXT_Buffer)	 		  	//数组长度	
#define SIZE TEXT_LENTH/4+((TEXT_LENTH%4)?1:0)

#define FLASH_SAVE_ADDR  0X08008000 	//设置FLASH 保存地址(必须大于32KB地址范围,且为4的倍数. 

int main(void)
{
	u8 key=0;
	u16 i=0;
	u8 datatemp[SIZE];
	Cache_Enable();					//打开L1-Cache
	HAL_Init();				        //初始化HAL库
	Stm32_Clock_Init(160,5,2,4);	//设置时钟,400Mhz 
	delay_init(400);				//延时初始化
	uart_init(115200);				//串口初始化
	LED_Init();						//初始化LED时钟
	KEY_Init();
	MPU_Memory_Protection();		//保护相关存储区域
	SDRAM_Init();                   //初始化SDRAM
	LCD_Init();						//初始化LCD
	
	POINT_COLOR=RED;				//设置字体为红色 
	LCD_ShowString(30,50,200,16,16,"POLARIS H750/F750");
	LCD_ShowString(30,70,200,16,16,"FLASH EEPROM TEST");	
	LCD_ShowString(30,90,200,16,16,"ATOM@ALIENTEK");
	LCD_ShowString(30,110,200,16,16,"2019/5/9");	 		
	LCD_ShowString(30,130,200,16,16,"KEY1:Write  KEY0:Read");
	while(1)
	{
		key=KEY_Scan(0);
		if(key==KEY1_PRES)	//KEY1按下,写入STM32 FLASH
		{
			LCD_Fill(0,170,239,319,WHITE);//清除半屏    
 			LCD_ShowString(30,170,200,16,16,"Start Write FLASH....");
			STMFLASH_Write(FLASH_SAVE_ADDR,(u32*)TEXT_Buffer,SIZE);
			LCD_ShowString(30,170,200,16,16,"FLASH Write Finished!");//提示传送完成
		}
		if(key==KEY0_PRES)	//KEY0按下,读取字符串并显示
		{
 			LCD_ShowString(30,170,200,16,16,"Start Read FLASH.... ");
			STMFLASH_Read(FLASH_SAVE_ADDR,(u32*)datatemp,SIZE);
			LCD_ShowString(30,170,200,16,16,"The Data Readed Is:  ");//提示传送完成
			LCD_ShowString(30,190,200,16,16,datatemp);//显示读到的字符串
		}
		i++;
		delay_ms(10);  
		if(i==20)
		{
			LED0_Toggle;//提示系统正在运行	
			i=0;
		}		   
	} 
}
