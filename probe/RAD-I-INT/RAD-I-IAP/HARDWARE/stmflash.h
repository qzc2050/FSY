#ifndef __STMFLASH_H__
#define __STMFLASH_H__
 
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32H7开发板
//STM32内部FLASH读写 驱动代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2019/5/9
//版本：V1.1
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2014-2024
//All rights reserved				
//********************************************************************************
//修改说明 
//20190516  V1.1
//1,修改BOOT_FLASH_SIZE大小为8K,尽可能多的给APP程序预留内部FLASH空间
//2,修改STMFLASH_Write函数bug
////////////////////////////////////////////////////////////////////////////////// 

#include "main.h"
#include <stdbool.h>

//FLASH起始地址
#define STM32_FLASH_BASE		 0x08000000		//STM32 FLASH的起始地址
#define STM32_FLASH_SIZE 		 0x200000		//STM32 FLASH总大小
//#define BOOT_FLASH_SIZE 		 0x2000			//前8K FLASH用于保存BootLoader
#define BANK1_END_ADDR           0x08100000		//Bank1 结束地址

//中断控制函数
void INTX_DISABLE(void);		//关闭所有中断
void INTX_ENABLE(void);			//开启所有中断

void Reg_Flash_Unlock(uint8_t idx_bank);						//FLASH 解锁
void Reg_Flash_Lock(uint8_t idx_bank);				 		//FLASH 上锁
uint8_t Reg_Flash_GetErrorStatus(uint8_t idx_bank);			//获得状态
uint8_t Reg_Flash_WaitDone(uint8_t idx_bank, uint32_t time);	//等待操作结束
uint8_t Reg_Flash_EraseSector(uint32_t addr);			 		//擦除扇区
uint8_t Reg_Flash_WriteNBytes(uint32_t faddr, uint8_t* pdata, uint32_t nbytes);	//一次写入 N 个字节
uint32_t Reg_Flash_ReadWord(uint32_t faddr);					//读一个字
bool Reg_Flash_Write(uint32_t WriteAddr, uint32_t *pBuffer, uint32_t NumToWrite);	//指定地址开始写入指定长度的数据（优化跨 Bank，带验证）//从指定地址开始读出指定长度的数据
void Reg_Flash_Read(uint32_t ReadAddr, uint32_t *pBuffer, uint32_t NumToRead);

//测试写入
bool Reg_Flash_Test_Write(uint32_t WriteAddr, uint32_t WriteData);								   
#endif

















