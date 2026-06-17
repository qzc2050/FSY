#ifndef __REG_FLASH_H__
#define __REG_FLASH_H__

#include "main.h"
#include <stdbool.h>

//FLASH 起始地址
#define STM32_FLASH_BASE		 0x08000000		//STM32 FLASH 的起始地址
#define STM32_FLASH_SIZE 		   0x200000		//STM32 FLASH 总大小
#define BANK1_END_ADDR           0x08100000		//Bank1 结束地址

//FLASH 扇区的起始地址，H750xx 只有 BANK1 的扇区 0 有效，共 128KB
#define BANK1_FLASH_SECTOR_0     ((uint32_t)0x08000000) 	//Bank1 扇区 0 起始地址，128 Kbytes 

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
