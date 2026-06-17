/**
  ******************************************************************************
  * 文件名程: stm_flash.c 
  * 作    者: 硬石嵌入式开发团队
  * 版    本: V1.0
  * 编写日期: 2015-10-04
  * 功    能: 内部Falsh读写实现
  ******************************************************************************
  * 说明：
  * 本例程配套硬石stm32开发板YS-F1Pro使用。
  * 
  * 淘宝：
  * 论坛：http://www.ing10bbs.com
  * 版权归硬石嵌入式开发团队所有，请勿商用。
  ******************************************************************************
  */
/* 包含头文件 ----------------------------------------------------------------*/
#include "control.h"
/* 私有类型定义 --------------------------------------------------------------*/
/* 私有宏定义 ----------------------------------------------------------------*/
#if (STM32_DATAEEPROM_SIZE < 256) || (STM32_FLASH_SIZE < 256)
	#define STM_SECTOR_SIZE  128 //字节
#else 
	#define STM_SECTOR_SIZE	 1024
#endif

//#define ALL_SECPOS_NUM (STM32_DATAEEPROM_SIZE * 1024 / STM_SECTOR_SIZE / 16 )   
#define STM_BUF_SIZE  (STM_SECTOR_SIZE / 4)


/* 私有变量 ------------------------------------------------------------------*/
#if STM32_DATAEEPROM_WREN	||STM32_FLASH_WREN //如果使能了写 
static uint32_t STM_BUF [ STM_BUF_SIZE ];//最多是1K字节
static FLASH_EraseInitTypeDef EraseInitStruct;
#endif

/* 扩展变量 ------------------------------------------------------------------*/
/* 私有函数原形 --------------------------------------------------------------*/
static inline void Flash_Wait(void);
/* 函数体 --------------------------------------------------------------------*/
/**
  * 函数功能: 读取指定地址的字(32位数据)
  * 输入参数: faddr:读地址(此地址必须为4的倍数!!)
  * 返 回 值: 返回值:对应数据.
  * 说    明：无
  */
uint32_t STMDATAEEPROM_ReadWord ( uint32_t faddr )
{
	return *(__IO uint32_t*)faddr; 
}

#if STM32_DATAEEPROM_WREN	//如果使能了写   
/**
  * 函数功能: 不检查的写入
  * 输入参数: WriteAddr:起始地址
  *           pBuffer:数据指针
  *           NumToWrite:半字(16位)数
  * 返 回 值: 无
  * 说    明：无
  */
void STMDATAEEPROM_Write_NoCheck ( uint32_t WriteAddr, uint32_t * pBuffer, uint16_t NumToWrite )   
{ 			 		 
	uint16_t i;
	
	for(i=0;i<NumToWrite;i++)
	{
		HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAM_WORD,WriteAddr,pBuffer[i]);
        WriteAddr+=4;                                    //地址增加4.
	}  
} 

/**
  * 函数功能: 从指定地址开始写入指定长度的数据
  * 输入参数: WriteAddr:起始地址(此地址必须为4的倍数!!)
  *           pBuffer:数据指针
  *           NumToWrite:字(32位)数(就是要写入的32位数据的个数.)
  * 返 回 值: 无
  * 说    明：无
  */
void STMDATAEEPROM_Write ( uint32_t WriteAddr, uint32_t * pBuffer, uint16_t NumToWrite )	
{
	uint16_t secoff;	   //扇区内偏移地址(16位字计算)
	uint16_t secremain; //扇区内剩余地址(16位字计算)	   
 	uint16_t i;    
	uint32_t secpos;	   //扇区地址
	uint32_t offaddr;   //去掉0X08000000后的地址
	
	if(WriteAddr<DATA_EEPROM_BASE||(WriteAddr>=(DATA_EEPROM_BASE+1024*STM32_DATAEEPROM_SIZE)))return;//非法地址
	
	HAL_FLASHEx_DATAEEPROM_Unlock();						//解锁
	
	offaddr=WriteAddr-DATA_EEPROM_BASE;		//实际偏移地址.
	secpos=offaddr/STM_SECTOR_SIZE;			//扇区地址  0~63 for STM32L051C8T6
	secoff=(offaddr%STM_SECTOR_SIZE)/4;		//在扇区内的偏移(4个字节为基本单位.)
	secremain=STM_SECTOR_SIZE/4-secoff;		//扇区剩余空间大小
	if(NumToWrite<=secremain)secremain=NumToWrite;//不大于该扇区范围
	
	while(1) 
	{
		/*****写入几个数据就直接擦除4*n个字节*****/
		for(i=0;i<NumToWrite;i++){ 
			HAL_FLASHEx_DATAEEPROM_Erase(WriteAddr+4*i);   //清除要写入地址的数据
		}
		/*****写入几个数据就直接擦除4*n个字节*****/
		STMDATAEEPROM_Read(WriteAddr,STM_BUF,NumToWrite);//读出4*n个字节的内容
		for(i=0;i<NumToWrite;i++)//校验数据
		{
			if(STM_BUF[secoff+i]!=0X0)
				break;//需要重新擦除  	  
		}
        STMDATAEEPROM_Write_NoCheck(WriteAddr,pBuffer,secremain);//写已经擦除了的,直接写入扇区剩余区间.
        
        if(NumToWrite==secremain)
			break;//写入结束了
        
		else//写入未结束
		{
			secpos++;				//扇区地址增1
			secoff=0;				//偏移位置为0 	 
            pBuffer+=secremain;  	//指针偏移
			WriteAddr+=secremain*4;	//写地址偏移	   
            NumToWrite-=secremain;	//字节(16位)数递减
			if(NumToWrite>(STM_SECTOR_SIZE/4))secremain=STM_SECTOR_SIZE/4;//下一个扇区还是写不完
			else secremain=NumToWrite;//下一个扇区可以写完了
		}
	};
	HAL_FLASHEx_DATAEEPROM_Lock();//上锁
}
#endif

/**
  * 函数功能: 从指定地址开始读出指定长度的数据
  * 输入参数: ReadAddr:起始地址
  *           pBuffer:数据指针
  *           NumToRead:字(16位)数
  * 返 回 值: 无
  * 说    明：无
  */
void STMDATAEEPROM_Read ( uint32_t ReadAddr, uint32_t *pBuffer, uint16_t NumToRead )   	
{
	uint16_t i;
	
	for(i=0;i<NumToRead;i++)
	{
		pBuffer[i]=STMDATAEEPROM_ReadWord(ReadAddr);//读取4个字节.
		ReadAddr+=4;//偏移4个字节.
	}
}

/**
  * 函数功能: 清除数据部分的EEPROM
  * 输入参数: ReadAddr:起始地址
  *           NumToClear:字节(32位)数
  * 返 回 值: 无
  * 说    明：无
  */
void STMDATAEEPROM_Clear_Part ( uint32_t baseAddr, uint16_t NumToClear )   	
{
	 uint16_t i;
	 uint32_t buf = 0;

	 HAL_FLASHEx_DATAEEPROM_Unlock();                   //解锁
	 for(i=0;i<NumToClear;i++)
		 HAL_FLASHEx_DATAEEPROM_Erase(baseAddr+4*i);    //清除要写入地址的数据
    
	 Flash_Wait();
	 for(i=0;i<NumToClear;i++)
	 { 
		 STMDATAEEPROM_Read(baseAddr+4*i,&buf,1);       //清除要写入地址的数据

		 if(buf)
		 {
			 printf("addr:%#x - %d earse err!\r\n",baseAddr+4*i,buf);
			 break;
		 }
	 }
	 HAL_FLASHEx_DATAEEPROM_Lock();//上锁
}

/**
  * 函数功能: 无检测擦除整个flash
  * 输入参数: 无
  * 返 回 值: 无进行检测是否成功清除flash，默认为成功
  * 说    明：无
  */
//HAL_StatusTypeDef STMDATAEEPROM_Clear(void)
//{
//	float data[REAL_DATA_GROUP] = {0.000};
//	
//	STMDATAEEPROM_Write(REAL_RATE_OFFSET_ADD,(uint32_t *)(&data),REAL_DATA_GROUP);
//	return HAL_OK;
//}

/**
  * 函数功能: 读取指定地址的半字(16位数据)
  * 输入参数: faddr:读地址(此地址必须为2的倍数!!)
  * 返 回 值: 返回值:对应数据.
  * 说    明：无
  */
uint32_t STMFLASH_ReadWord ( uint32_t faddr )
{
	return *(__IO uint32_t*)faddr; 
}

#if STM32_FLASH_WREN	//如果使能了写   
/**
  * 函数功能: 不检查的写入
  * 输入参数: WriteAddr:起始地址
  *           pBuffer:数据指针
  *           NumToWrite:半字(16位)数
  * 返 回 值: 无
  * 说    明：无
  */
void STMFLASH_Write_NoCheck ( uint32_t WriteAddr, uint32_t * pBuffer, uint16_t NumToWrite )   
{ 			 		 
	uint16_t i;	
//	uint32_t temp = 0;
	for(i=0;i<NumToWrite;i++)
	{
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,WriteAddr,pBuffer[i]);
//		STMFLASH_Read(WriteAddr,&temp,1);
		WriteAddr+=4;	                  //地址增加4.
	}  
} 

/**
  * 函数功能: 从指定地址开始写入指定长度的数据
  * 输入参数: WriteAddr:起始地址(此地址必须为4的倍数!!)
  *           pBuffer:数据指针
  *           NumToWrite:字(32位)数(就是要写入的32位数据的个数.)
  * 返 回 值: 无
  * 说    明：无
  */
void STMFLASH_Write(uint32_t WriteAddr, uint32_t * pBuffer, uint16_t NumToWrite )	
{
 	uint16_t i;
    uint16_t k = 0;
	uint16_t secoff;        //扇区内偏移地址(16位字计算)
	uint16_t secremain;     //扇区内剩余地址(16位字计算)	 
    uint32_t SECTORError = 0;
	uint32_t secpos;        //扇区地址
	uint32_t offaddr;       //去掉0X08000000后的地址
	
	if(WriteAddr<FLASH_BASE||(WriteAddr>=(FLASH_BASE+1024*STM32_FLASH_SIZE)))return;//非法地址
	
	HAL_FLASH_Unlock();						//解锁
	
	offaddr=WriteAddr-FLASH_BASE;           //实际偏移地址.
	secpos=offaddr/STM_SECTOR_SIZE;			//扇区地址  0~127 for STM32L051C8T6
	secoff=(offaddr%STM_SECTOR_SIZE)/4;		//在扇区内的偏移(4个字节为基本单位.)
	secremain=STM_SECTOR_SIZE/4-secoff;		//扇区剩余空间大小
	if(NumToWrite<=secremain)
	{
		k = secremain - NumToWrite;
		secremain=NumToWrite;               //不大于该扇区范围
	}
	
	while(1)
	{
		STMFLASH_Read(secpos*STM_SECTOR_SIZE+FLASH_BASE,STM_BUF,STM_SECTOR_SIZE/4);//读出整个扇区的内容
//		for(i=0;i<STM_SECTOR_SIZE/4;i++)//复制
//		{
//			printf("%d,",STM_BUF[i]);	  //一页擦除后不保存写入数据后，之前原有的数据
//		}
		
		for(i=0;i<secremain;i++)//校验数据
		{
			if(STM_BUF[secoff+i]!=0X0000)  
				break;//需要擦除 
		}
		if(i<secremain)//需要擦除
		{
//			again:
            //擦除这个扇区
            /* Fill EraseInit structure*/
            EraseInitStruct.TypeErase     = FLASH_TYPEERASE_PAGES;
            EraseInitStruct.PageAddress   = secpos*STM_SECTOR_SIZE+FLASH_BASE;
            EraseInitStruct.NbPages       = 1;
            HAL_FLASHEx_Erase(&EraseInitStruct, &SECTORError);
//			printf("\r\n我擦除啦！\r\n");
//			STMFLASH_Read(secpos*STM_SECTOR_SIZE+FLASH_BASE,STM_BUF,STM_SECTOR_SIZE/4);//读出整个扇区的内容
//			for(i=0;i<secremain;i++)//校验数据
//			{
////				printf("1111:%d\r\n",STM_BUF[secoff+i]);
//				if(STM_BUF[secoff+i]!=0X0000)
//				{
//					printf("\r\n需要重新擦除！\r\n");
//					goto again;
////					break;//需要擦除
//				} 
//			}
			
			for(i=0;i<secremain;i++)//复制
				STM_BUF[i+secoff]=pBuffer[i];	  
			
			if(k != 0)
			{
				for(i=0;i<k;i++)//复制
					STM_BUF[i+secoff+secremain]=0X0000;	  //一页擦除后不保存写入数据后之前原有的数据
				k = 0;
			}
			STMFLASH_Write_NoCheck(secpos*STM_SECTOR_SIZE+FLASH_BASE,STM_BUF,STM_SECTOR_SIZE/4);//写入整个扇区 	
		}
    else
      STMFLASH_Write_NoCheck(WriteAddr,pBuffer,secremain);//写已经擦除了的,直接写入扇区剩余区间.

    if(NumToWrite==secremain)break;//写入结束了
		else//写入未结束
		{
			secpos++;				//扇区地址增1
			secoff=0;				//偏移位置为0 	 
		   	pBuffer+=secremain;  	//指针偏移
			WriteAddr+=secremain*4;	//写地址偏移	   
		   	NumToWrite-=secremain;	//字节(16位)数递减
			if(NumToWrite>(STM_SECTOR_SIZE/4))
				secremain=STM_SECTOR_SIZE/4;//下一个扇区还是写不完
			else 
			{
				k = secremain - NumToWrite;
				secremain=NumToWrite;//下一个扇区可以写完了
			}
		}	 
	};	
	HAL_FLASH_Lock();//上锁
}
#endif

/**
  * 函数功能: 从指定地址开始读出指定长度的数据
  * 输入参数: ReadAddr:起始地址
  *           pBuffer:数据指针
  *           NumToRead:半字(16位)数
  * 返 回 值: 无
  * 说    明：无
  */
void STMFLASH_Read ( uint32_t ReadAddr, uint32_t *pBuffer, uint16_t NumToRead )   	
{
	uint16_t i;
	
	for(i=0;i<NumToRead;i++)
	{
		pBuffer[i]=STMFLASH_ReadWord(ReadAddr);//读取4个字节.
		ReadAddr+=4;//偏移4个字节.	
	}
}



/******************* (C) COPYRIGHT 2015-2020 硬石嵌入式开发团队 *****END OF FILE****/


// 定义更新标志位的值
#define UPDATE_FLAG_VALUE       0xAB1011AB

// 定义 IAP 程序地址和大小
#define IAP_START_ADDRESS       0x08000000
#define IAP_SIZE                0x1000
#define UPDATE_FLAG_ADDRESS     (IAP_START_ADDRESS + IAP_SIZE - 4) // 更新标志位地址


// Flash 解锁
static inline void Flash_Unlock(void) {
    if (FLASH->PECR & FLASH_PECR_PELOCK) { // 检查是否已锁定
        FLASH->PEKEYR = 0x89ABCDEF;      // 写入 PEKEY1
        FLASH->PEKEYR = 0x02030405;      // 写入 PEKEY2
    }
    if (FLASH->PECR & FLASH_PECR_PRGLOCK) { // 检查是否已锁定
        FLASH->PRGKEYR = 0x8C9DAEBF;     // 写入 PRGKEY1
        FLASH->PRGKEYR = 0x13141516;     // 写入 PRGKEY2
    }
}

// Flash 锁定
static inline void Flash_Lock(void) {
    FLASH->PECR |= FLASH_PECR_PRGLOCK; // 锁定 Flash
    FLASH->PECR |= FLASH_PECR_PELOCK;  // 锁定 Flash
}

// Flash 等待操作完成
static inline void Flash_Wait(void) {
    while (FLASH->SR & FLASH_SR_BSY); // 等待 Flash 操作完成
}

// 检查是否需要擦除
static bool Flash_NeedErase(uint32_t address, uint16_t length) {
    for(uint16_t i = 0; i < length; i++) {
        if(*(__IO uint8_t*)(address + i) != 0x00)
            return true;
//        if ((*(uint8_t *)(address + i) & data[i]) != 0x00) {
//            // 如果目标地址的位不是 0x00，则需要擦除
//            return true;
//        }
    }
    return false;
}

// 擦除 Flash 页
static bool Flash_ErasePage(uint32_t page_address) {
    // 检查地址是否对齐到页边界
    if (page_address & (FLASH_PAGE_SIZE - 1))
        return false; // 地址不对齐

    // 解锁 Flash
    Flash_Unlock();

    // 设置擦除模式
    FLASH->PECR |= FLASH_PECR_ERASE;

    // 启动擦除
    FLASH->PECR |= FLASH_PECR_PROG;

    // 写入任意数据到目标页地址以触发擦除
    *(__IO uint32_t *)page_address = 0x00000000;

    // 等待擦除完成
    Flash_Wait();

    // 清除擦除模式
    FLASH->PECR &= ~FLASH_PECR_PROG;
    FLASH->PECR &= ~FLASH_PECR_ERASE;

    // 锁定 Flash
    Flash_Lock();
    return true;
}

// 写入 32 位数据到 Flash
static void Flash_ProgramWord(uint32_t address, uint32_t data) {
    // 写入数据到目标地址
    *(uint32_t *)address = data;

    // 等待写入完成
    Flash_Wait();

    // 检查是否写入成功
    if (FLASH->SR & FLASH_SR_EOP)
        FLASH->SR |= FLASH_SR_EOP; // 清除操作完成标志
    else{
        // 写入失败，处理错误
//        printf("Flash write failed!\r\n");
    }
}

// Flash 写入函数（带擦除检测）
bool Flash_Write(uint32_t address, uint8_t *data, uint16_t length) {
     // 禁用全局中断
    __disable_irq();
    
    // 检查目标地址对齐
    if ((address % 4) != 0)
        return false; // 目标地址未对齐

    // 检查数据长度对齐
    if ((length % 4) != 0)
        return false; // 数据长度不是4的倍数

    // 检查地址范围
    if (address < FLASH_BASE || (address + length) > (FLASH_BASE + FLASH_SIZE))
        return false; // 地址超出范围

    // 检查是否需要擦除
    if (Flash_NeedErase(address, length)) {
        // 计算需要擦除的页地址
        uint32_t page_address = address & ~(FLASH_PAGE_SIZE - 1); // 对齐到页起始地址
        if (!Flash_ErasePage(page_address)) {
            return false; // 擦除失败
        }
    }

    // 解锁 Flash
    Flash_Unlock();

    // 设置编程模式
    FLASH->PECR |= FLASH_PECR_FPRG;

    // 写入数据
    for (uint16_t i = 0; i < length; i += 4) {
        // 从 uint8_t 数组中提取32位数据
        uint32_t word_data = (uint32_t)data[i] |
                             ((uint32_t)data[i + 1] << 8) |
                             ((uint32_t)data[i + 2] << 16) |
                             ((uint32_t)data[i + 3] << 24);

        // 写入Flash
        Flash_ProgramWord(address + i, word_data);
    }

    // 清除编程模式
    FLASH->PECR &= ~FLASH_PECR_FPRG;

    // 锁定 Flash
    Flash_Lock();
    
    // 启用全局中断
    __enable_irq();
    return true;
}

/********************************************************************************************
* 函数名：Clr_Program_Update
* 描  述：清除程序更新标志
********************************************************************************************/
void Clr_Program_Update(void)
{
    uint32_t data = UPDATE_FLAG_VALUE;
    
    if((*(__IO uint32_t *)UPDATE_FLAG_ADDRESS) != UPDATE_FLAG_VALUE)
        Flash_Write(UPDATE_FLAG_ADDRESS,(uint8_t *)&data,4);
}

/********************************************************************************************
* 函数名：Req_Program_Update
* 描  述：请求程序更新标志
********************************************************************************************/
void Req_Program_Update(void)
{
     // 禁用全局中断
    __disable_irq();
    
    // 解锁 Flash
    Flash_Unlock();
    
    Flash_Wait();
    
    /* Set the ERASE bit */
    SET_BIT(FLASH->PECR, FLASH_PECR_ERASE);

    /* Set PROG bit */
    SET_BIT(FLASH->PECR, FLASH_PECR_PROG);

    /* Write 00000000h to the first word of the program page to erase */
    *(__IO uint32_t *)(uint32_t)(UPDATE_FLAG_ADDRESS & ~(FLASH_PAGE_SIZE - 1)) = 0x00000000;

    Flash_Wait();
    CLEAR_BIT(FLASH->PECR, FLASH_PECR_PROG);
    CLEAR_BIT(FLASH->PECR, FLASH_PECR_ERASE);
    
    // 锁定 Flash
    Flash_Lock();
    
    // 启用全局中断
    __enable_irq();
}






