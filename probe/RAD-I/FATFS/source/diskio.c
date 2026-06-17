/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2016        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/
#include "diskio.h"			/* FatFs lower layer API */
#include <string.h>
//#include "sdmmc_sdcard.h"
#include "w25qxx.h"
//#include "malloc.h"

/* USB MSC 连接到 PC 时，Flash 由 USB 侧独占，禁止 MCU 本地 FatFs 并发访问 */
extern volatile uint8_t bDeviceState;
extern uint8_t USB_MSC_IsStarted(void);

/* W25Q 为 NOR：必须先擦除再编程。USB MSC 路径已擦除 4KB 子扇区；FatFs 若直接
 * 页编程会写失败或读出无效 FAT，表现为 f_mkfs 后仍 FR_NO_FILESYSTEM(13)。
 * 此处按 4KB 读-改-写，与 usbd_storage.c 行为一致。 */
static uint8_t diskio_subbuf[W25QxJV_SUBSECTOR_SIZE];
static uint8_t diskio_verify_buf[W25QxJV_SUBSECTOR_SIZE];

//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32H7开发板
//FATFS底层(diskio) 驱动代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2018/8/2
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2009-2019
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 


#define SD_CARD	 	0  			//SD卡,卷标为0
#define EX_FLASH 	1			//外部spi flash,卷标为1
#define EX_NAND  	2			//外部nand flash,卷标为2

/* 逻辑盘容量须与实际 W25Q 容量一致，否则 f_mkfs / f_mount 行为异常。
 * W25QxJV_FLASH_SIZE 在 w25qxx.h 中定义（例如 W25Q64 为 8MB）。 */
#define SPI_FLASH_SECTOR_SIZE 	512
#define SPI_FLASH_SECTOR_COUNT 	(W25QxJV_FLASH_SIZE / SPI_FLASH_SECTOR_SIZE)
#define SPI_FLASH_BLOCK_SIZE   	8		/* 每个擦除块 8 个 512B 扇区（4KB），与 W25 子扇区对齐 */
  
static uint8_t diskio_flash_busy_by_usb(void)
{
	if (USB_MSC_IsStarted() != 0U) {
		return 1U;
	}
	return (bDeviceState == 1U) ? 1U : 0U;
}

//获得磁盘状态
DSTATUS disk_status (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{ 
	if (pdrv == EX_FLASH && diskio_flash_busy_by_usb()) {
		return STA_NOINIT;
	}
	return RES_OK;
}  


//初始化磁盘
DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	uint8_t res=0;	    
	switch(pdrv)
	{
		case EX_FLASH:		//外部flash
			if (diskio_flash_busy_by_usb()) {
				return STA_NOINIT;
			}
//			W25QXX_Init();  //W25QXX初始化
 			break;
		default:
			res=1; 
	}		 
	if(res)return  STA_NOINIT;
	else return 0; //初始化成功 
} 
//读扇区
//pdrv:磁盘编号0~9
//*buff:数据接收缓冲首地址
//sector:扇区地址
//count:需要读取的扇区数
DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address in LBA */
	UINT count		/* Number of sectors to read */
)
{
	uint8_t res=0; 
    if (!count)return RES_PARERR;//count不能等于0，否则返回参数错误		 	 
	if (pdrv == EX_FLASH && diskio_flash_busy_by_usb()) {
		return RES_NOTRDY;
	}
	switch(pdrv)
	{
		case EX_FLASH://外部flash
			for(;count>0;count--)
			{
//				W25QXX_Read(buff,sector*SPI_FLASH_SECTOR_SIZE,SPI_FLASH_SECTOR_SIZE);
//				W25Qx_QSPI_FastRead(buff,sector*SPI_FLASH_SECTOR_SIZE,SPI_FLASH_SECTOR_SIZE);
				if (W25Qx_QSPI_Read(buff,sector*SPI_FLASH_SECTOR_SIZE,SPI_FLASH_SECTOR_SIZE) != QSPI_OK) {
					res = 1;
					break;
				}
				sector++;
				buff+=SPI_FLASH_SECTOR_SIZE;
			}
			break;
		default:
			res=1; 
	}
   //处理返回值，将SPI_SD_driver.c的返回值转成ff.c的返回值
    if(res==0x00)return RES_OK;	 
    else return RES_ERROR;	   
}
//写扇区
//pdrv:磁盘编号0~9
//*buff:发送数据首地址
//sector:扇区地址
//count:需要写入的扇区数 
DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address in LBA */
	UINT count			/* Number of sectors to write */
)
{
	uint32_t sub_sz;
	uint32_t addr;
	uint32_t base;
	uint32_t off;
	uint32_t max_bytes;
	uint32_t nsec;
	uint32_t nbytes;

	if (!count) {
		return RES_PARERR;
	}
	if (pdrv != EX_FLASH) {
		return RES_ERROR;
	}
	if (diskio_flash_busy_by_usb()) {
		return RES_NOTRDY;
	}

	sub_sz = W25QxJV_SUBSECTOR_SIZE;

	while (count > 0) {
		addr = (uint32_t)sector * SPI_FLASH_SECTOR_SIZE;
		base = (addr / sub_sz) * sub_sz;
		off = addr - base;
		max_bytes = sub_sz - off;
		nsec = max_bytes / SPI_FLASH_SECTOR_SIZE;
		if (nsec > (uint32_t)count) {
			nsec = (uint32_t)count;
		}
		nbytes = nsec * SPI_FLASH_SECTOR_SIZE;

		/* 为了稳定性使用普通读，避免高速读模式在部分时序下引入误读 */
		if (W25Qx_QSPI_Read(diskio_subbuf, base, sub_sz) != QSPI_OK) {
			return RES_ERROR;
		}
		memcpy(diskio_subbuf + off, buff, nbytes);
		if (W25Qx_QSPI_Erase_Block(base) != QSPI_OK) {
			return RES_ERROR;
		}
		if (W25Qx_QSPI_Write(diskio_subbuf, base, sub_sz) != QSPI_OK) {
			return RES_ERROR;
		}
		if (W25Qx_QSPI_Read(diskio_verify_buf, base, sub_sz) != QSPI_OK) {
			return RES_ERROR;
		}
		/* 校验整块 4KB，避免“本次片段正确但同扇区其它元数据被破坏”的隐患 */
		if (memcmp(diskio_verify_buf, diskio_subbuf, sub_sz) != 0) {
			return RES_ERROR;
		}

		sector += (DWORD)nsec;
		buff += nbytes;
		count -= (UINT)nsec;
	}

	return RES_OK;
} 
//其他表参数的获得
//pdrv:磁盘编号0~9
//ctrl:控制代码
//*buff:发送/接收缓冲区指针 
DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
DRESULT res;						  			     
	if (pdrv == EX_FLASH && diskio_flash_busy_by_usb()) {
		return RES_NOTRDY;
	}
	if(pdrv==EX_FLASH)	//外部FLASH  
	{
	    switch(cmd)
	    {
		    case CTRL_SYNC:
				res = RES_OK; 
		        break;	 
		    case GET_SECTOR_SIZE:
		        *(WORD*)buff = SPI_FLASH_SECTOR_SIZE;
		        res = RES_OK;
		        break;	 
		    case GET_BLOCK_SIZE:
		        *(WORD*)buff = SPI_FLASH_BLOCK_SIZE;
		        res = RES_OK;
		        break;	 
		    case GET_SECTOR_COUNT:
		        *(DWORD*)buff = SPI_FLASH_SECTOR_COUNT;
		        res = RES_OK;
		        break;
		    default:
		        res = RES_PARERR;
		        break;
	    }
	}else res=RES_ERROR;//其他的不支持
    return res;
} 




















