#include "reg_flash.h"

//THUMB 指令不支持汇编内联
//采用如下方法实现执行汇编指令 WFI  
void WFI_SET(void)
{
	__ASM volatile("wfi");		  
}

//关闭所有中断 (但是不包括 fault 和 NMI 中断)
void INTX_DISABLE(void)
{
//	__ASM volatile("cpsid i");
//    __ASM volatile("mov r0, #0x40");
//    __ASM volatile("msr basepri, r0");
    __disable_irq();
}

//开启所有中断
void INTX_ENABLE(void)
{
//	__ASM volatile("cpsie i");
//    __ASM volatile("mov r0, #0");
//    __ASM volatile("msr basepri, r0");
    __enable_irq();
}

//设置栈顶地址
//addr:栈顶地址
__asm void MSR_MSP(uint32_t addr) 
{
	MSR MSP, r0 			//set Main Stack value
	BX r14
}

//解锁 STM32 的 FLASH
void Reg_Flash_Unlock(uint8_t idx_bank)
{
    if(!idx_bank)
    {
        FLASH->KEYR1 = FLASH_KEY1;	//Bank1，写入解锁序列.
        FLASH->KEYR1 = FLASH_KEY2; 
    }
    else
    {
        FLASH->KEYR2 = FLASH_KEY1;
        FLASH->KEYR2 = FLASH_KEY2;
    }
}

//flash 上锁
void Reg_Flash_Lock(uint8_t idx_bank)
{
    if(!idx_bank)
        FLASH->CR1 |= 1<<0;	//Bank1，上锁 
    else
        FLASH->CR2 |= 1<<0;	//Bank2，上锁 
}

//得到 FLASH 的错误状态 
//返回值:
//0，无错误
//其他，错误编号
uint8_t Reg_Flash_GetErrorStatus(uint8_t idx_bank)
{	
	uint32_t res = 0;	
    if(!idx_bank)
        res = FLASH->SR1;
    else
        res = FLASH->SR2;
    
	if(res & (1<<17)) return 1;   	//WRPERR=1，写保护错误
	else if(res & (1<<18)) return 2;	//PGSERR=1，编程序列错误
	else if(res & (1<<19)) return 3;	//STRBERR=1，复写错误 
	else if(res & (1<<21)) return 4;	//INCERR=1，数据一致性错误
	else if(res & (1<<22)) return 5;	//OPERR=1，写/擦除错误 
	else if(res & (1<<23)) return 6;	//RDPERR=1，读保护错误
	else if(res & (1<<24)) return 7;	//RDSERR=1，非法访问加密区错误 
	else if(res & (1<<25)) return 8;	//SNECCERR=1，1bit ecc 校正错误 
	else if(res & (1<<26)) return 9;	//DBECCERR=1，2bit ecc 错误
	return 0;						//没有任何状态/操作完成.
} 

//等待操作完成 
//time:要延时的长短 
//返回值:
//0，完成      
//1~9，错误代码.
//0XFF，超时
uint8_t Reg_Flash_WaitDone(uint8_t idx_bank, uint32_t time)
{
	uint8_t res = 0;
	uint32_t tempreg = 0; 
    
    if(!idx_bank)
    {
        while(1)
        {
            tempreg = FLASH->SR1; 
            if((tempreg & 0X07) == 0) break;		//BSY=0,WBNE=0,QW=0，则操作完成 
            time--;
            if(time == 0)
                return 0XFF;
        }
        res = Reg_Flash_GetErrorStatus(idx_bank);
        if(res)
            FLASH->CCR1 = 0X07EE0000;		//清所有错误标志
        return res;
    }
    else
    {
        while(1)
        {
            tempreg = FLASH->SR2; 
            if((tempreg & 0X07) == 0)
                break;		//BSY=0,WBNE=0,QW=0，则操作完成 
            time--;
            if(time == 0)
                return 0XFF;
        }
        res = Reg_Flash_GetErrorStatus(idx_bank);
        if(res)
            FLASH->CCR2 = 0X07EE0000;		//清所有错误标志
        return res;
    }
}

//擦除扇区
//返回值：执行情况
uint8_t Reg_Flash_EraseSector(uint32_t addr)
{
    #define MAX_ERASE_TIMES    3
    
    uint32_t addr_r = 0;
    uint32_t addr_m = 0;
    uint32_t addr_end = 0;
    uint32_t sectorx = 0;
    uint8_t times = MAX_ERASE_TIMES;
    uint8_t bankx = 0;
	uint8_t res = 0;
    
    if(addr < BANK1_END_ADDR)
    {
        bankx = 0;	//判断地址是在 bank0，还是在 bank1
        addr_end = STM32_FLASH_BASE;
        sectorx = (addr - STM32_FLASH_BASE) / FLASH_SECTOR_SIZE;
    }
	else
    {
        bankx = 1;
        addr_end = BANK1_END_ADDR;
        sectorx = (addr - BANK1_END_ADDR) / FLASH_SECTOR_SIZE;
    }
    addr_r = addr_end + sectorx * FLASH_SECTOR_SIZE;
    addr_end += (sectorx + 1) * FLASH_SECTOR_SIZE;
//    sectorx = addr / FLASH_SECTOR_SIZE;
//    addr_m = addr_r;
    
    while(times--)
    {
        addr_m = addr_r;
        res = Reg_Flash_WaitDone(bankx, 0XFFFFFFFF);	//等待上次操作结束
        if(res)
            return res;
        
        if(!bankx)
        {
            FLASH->CR1 &= ~(7<<8);	//SNB1[2:0]=0，清除原来的设置
            FLASH->CR1 &= ~(3<<4);	//PSIZE1[1:0]=0，清除原来的设置
            FLASH->CR1 |= (uint32_t)sectorx << 8;	//设置要擦除的扇区编号
            FLASH->CR1 |= 2<<4;		//设置为 32bit 宽
            FLASH->CR1 |= 1<<2;		//SER1=1，扇区擦除 
            FLASH->CR1 |= 1<<7;		//START1=1，开始擦除
            res = Reg_Flash_WaitDone(bankx, 0XFFFFFFFF);//等待操作结束
            FLASH->CR1 &= ~(1<<2);	//SER1=0，清除扇区擦除标志
        }
        else
        {
            FLASH->CR2 &= ~(7<<8);	//SNB2[2:0]=0，清除原来的设置
            FLASH->CR2 &= ~(3<<4);	//PSIZE2[1:0]=0，清除原来的设置
            FLASH->CR2 |= (uint32_t)sectorx << 8;	//设置要擦除的扇区编号
            FLASH->CR2 |= 2<<4;		//设置为 32bit 宽
            FLASH->CR2 |= 1<<2;		//SER2=1，扇区擦除 
            FLASH->CR2 |= 1<<7;		//START2=1，开始擦除 
            res = Reg_Flash_WaitDone(bankx, 0XFFFFFFFF);//等待操作结束
            FLASH->CR2 &= ~(1<<2);	//SER2=0，清除扇区擦除标志
        }
        
        if(!res)
        {
            while(addr_m < addr_end)
            {
                if(Reg_Flash_ReadWord(addr_m) != 0XFFFFFFFF)
                {
                    if(!times)
                        return 0xff;
                    break;
                }
                else
                    addr_m += 4;
            }
            if(addr_m >= addr_end)
                break;
        }
    }
	return res;
}

//读取指定地址的一个字 (32 位数据) 
//faddr:读地址 
//返回值：对应数据.
uint32_t Reg_Flash_ReadWord(uint32_t faddr)
{
	return *(__IO uint32_t *)faddr; 
}   

/* 等待 Flash 空闲；0=成功，非 0/0xFF=失败或超时 */
static bool Reg_Flash_WaitIdle(uint8_t idx_bank)
{
    return (Reg_Flash_WaitDone(idx_bank, 0xFFFFFFFFU) == 0U);
}

//从指定地址开始写入指定长度的数据（优化跨 Bank 写入）
//WriteAddr:起始地址 (此地址必须为 32 的倍数!!)
//pBuffer:数据指针
//NumToWrite:字 (32 位) 数
bool Reg_Flash_Write(uint32_t WriteAddr, uint32_t *pBuffer, uint32_t NumToWrite)	
{ 
    #define MAX_ONCE_WRITE_WORD  8
    
    uint32_t i;
    uint32_t addrx;
    uint32_t bank1_words, bank2_words;
    uint32_t endaddr = WriteAddr + NumToWrite * 4;
    
    uint8_t status = 0;
    uint8_t crt_write = 0;

    // 地址合法性检查
    if(WriteAddr > (STM32_FLASH_BASE + STM32_FLASH_SIZE))
        return false;
    if(WriteAddr % 32)
        return false;
    if((NumToWrite == 0U) || (pBuffer == NULL))
        return false;
    
    // 计算 Bank1 和 Bank2 各需要写多少字
    if(WriteAddr < BANK1_END_ADDR)
    {
        // 起始地址在 Bank1
        if(endaddr <= BANK1_END_ADDR)
        {
            // 全部在 Bank1
            bank1_words = NumToWrite;
            bank2_words = 0;
        }
        else
        {
            // 跨 Bank
            bank1_words = (BANK1_END_ADDR - WriteAddr) / 4;
            bank2_words = NumToWrite - bank1_words;
        }
    }
    else
    {
        // 全部在 Bank2
        bank1_words = 0;
        bank2_words = NumToWrite;
    }
    
    if(!Reg_Flash_WaitIdle(0) || !Reg_Flash_WaitIdle(1))
        return false;
    
    if(bank1_words > 0)    // 写 Bank1 部分（如果有）
    {
        addrx = WriteAddr;

        INTX_DISABLE();
        Reg_Flash_Unlock(0);
        
        SCB_CleanInvalidateDCache();
        
        // 擦除 Bank1 扇区（仅覆盖本次写入范围）
        while(addrx < BANK1_END_ADDR)
        {
            if(Reg_Flash_ReadWord(addrx) != 0XFFFFFFFF)
            {
                status = Reg_Flash_EraseSector(addrx);
                if(status) {
                    Reg_Flash_Lock(0);
                    INTX_ENABLE();
                    return false;
                }
                if(!Reg_Flash_WaitIdle(0))
                {
                    Reg_Flash_Lock(0);
                    INTX_ENABLE();
                    return false;
                }
            }
            else 
                addrx += 4;
        }
        
        // 批量写入 Bank1
        if(status == 0)
        {
            while(bank1_words)
            {
                if(!Reg_Flash_WaitIdle(0))
                {
                    Reg_Flash_Lock(0);
                    INTX_ENABLE();
                    return false;
                }
                
                if(bank1_words > MAX_ONCE_WRITE_WORD)
                    crt_write = MAX_ONCE_WRITE_WORD;
                else
                    crt_write = bank1_words;
                bank1_words -= crt_write;
                
                FLASH->CR1 &= ~(3<<4);
                FLASH->CR1 |= 2<<4;
                FLASH->CR1 |= 1<<1;
            
                for(i = 0; i < crt_write; i++)
                {
                    *(__IO uint32_t *)WriteAddr = *pBuffer;
                    WriteAddr += 4;
                    pBuffer++;
                }
                
                if(crt_write < 8)
                {
                    for(; i < 8; i++)
                    {
                        *(__IO uint32_t *)WriteAddr = 0U;
                        WriteAddr += 4;
                    }
                }
                
                __DSB();
                if(!Reg_Flash_WaitIdle(0))
                {
                    FLASH->CR1 &= ~(1<<1);
                    Reg_Flash_Lock(0);
                    INTX_ENABLE();
                    return false;
                }
                FLASH->CR1 &= ~(1<<1);
            }
        }
        Reg_Flash_Lock(0);
        INTX_ENABLE();
    }
    
    // 写 Bank2 部分（如果有）
    if(bank2_words > 0)
    {
        addrx = (bank1_words > 0) ? BANK1_END_ADDR : WriteAddr;
        
        INTX_DISABLE();
        Reg_Flash_Unlock(1);
        
        /* 擦除判断前清 D-Cache，避免读到旧缓存误判并触发整扇区误擦 */
        SCB_CleanInvalidateDCache();
        
        // 擦除 Bank2 扇区
        while(addrx < endaddr)
        {
            if(Reg_Flash_ReadWord(addrx) != 0XFFFFFFFF)
            { 
                status = Reg_Flash_EraseSector(addrx);
                if(status)
                {
                    Reg_Flash_Lock(1);
                    INTX_ENABLE();
                    return false;
                }
                if(!Reg_Flash_WaitIdle(1))
                {
                    Reg_Flash_Lock(1);
                    INTX_ENABLE();
                    return false;
                }
            }
            else 
                addrx += 4;
        }
        
        // 批量写入 Bank2
        if(status == 0)
        {
            while(bank2_words)
            {
                if(!Reg_Flash_WaitIdle(1))
                {
                    Reg_Flash_Lock(1);
                    INTX_ENABLE();
                    return false;
                }
                
                if(bank2_words > MAX_ONCE_WRITE_WORD)
                    crt_write = MAX_ONCE_WRITE_WORD;
                else
                    crt_write = bank2_words;
                bank2_words -= crt_write;
                
                FLASH->CR2 &= ~(3<<4);
                FLASH->CR2 |= 2<<4;
                FLASH->CR2 |= 1<<1;
                
                for(i = 0; i < crt_write; i++)
                {
                    *(__IO uint32_t *)WriteAddr = *pBuffer;
                    WriteAddr += 4;
                    pBuffer++;
                }
                
                if(crt_write < 8)
                {
                    for(; i < 8; i++)
                    {
                        *(__IO uint32_t *)WriteAddr = 0U;
                        WriteAddr += 4;
                    }
                }
                
                __DSB();
                if(!Reg_Flash_WaitIdle(1))
                {
                    FLASH->CR2 &= ~(1<<1);
                    Reg_Flash_Lock(1);
                    INTX_ENABLE();
                    return false;
                }
                FLASH->CR2 &= ~(1<<1);
            }
        }
        Reg_Flash_Lock(1);
        INTX_ENABLE();
    }
    return true;
}

//从指定地址开始读出指定长度的数据
//ReadAddr:起始地址
//pBuffer:数据指针
//NumToRead:字 (32 位) 数
void Reg_Flash_Read(uint32_t ReadAddr, uint32_t *pBuffer, uint32_t NumToRead)   	
{
	uint32_t i;
	for(i = 0; i < NumToRead; i++)
	{
		pBuffer[i] = Reg_Flash_ReadWord(ReadAddr);
		ReadAddr += 4;
	}
}

//测试写入
bool Reg_Flash_Test_Write(uint32_t WriteAddr, uint32_t WriteData)   	
{
	return Reg_Flash_Write(WriteAddr, &WriteData, 1);
}
