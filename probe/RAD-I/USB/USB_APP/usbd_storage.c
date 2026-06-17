/**
  ******************************************************************************
  * @file    USB_Device/MSC_Standalone/Src/usbd_storage.c
  * @author  MCD Application Team
  * @version V1.2.0
  * @date    29-December-2017
  * @brief   Memory management layer
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics International N.V.
  * All rights reserved.</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice,
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other
  *    contributors to this software may be used to endorse or promote products
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under
  *    this license is void and will automatically terminate your rights under
  *    this license.
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------ */
#include "usbd_storage.h"
//#include "sdmmc_sdcard.h"
#include "w25qxx.h"
#include <string.h>
//#include "nand.h"    
//#include "ftl.h"  

/* Private typedef ----------------------------------------------------------- */
/* Private define ------------------------------------------------------------ */
#define STORAGE_LUN_NBR                  1
/* 兼容 PC 端 U 盘识别/格式化：MSC 逻辑扇区使用 512B */
#define STORAGE_BLK_SIZ                  512U
#define STORAGE_BLK_NBR                  (W25QxJV_FLASH_SIZE / STORAGE_BLK_SIZ)

/* NOR 物理擦除单位为 4KB，MSC 512B 写入需做子扇区读改写 */
static uint8_t msc_subsector_buf[W25QxJV_SUBSECTOR_SIZE];
static uint8_t msc_verify_buf[W25QxJV_SUBSECTOR_SIZE];

////////////////////////////自己定义的一个标志USB状态的寄存器///////////////////
//bit0:表示电脑正在向SD卡写入数据
//bit1:表示电脑正从SD卡读出数据
//bit2:SD卡写数据错误标志位
//bit3:SD卡读数据错误标志位
//bit4:1,表示电脑有轮询操作(表明连接还保持着)
volatile uint8_t USB_STATUS_REG=0;
////////////////////////////////////////////////////////////////////////////////

/* Private macro ------------------------------------------------------------- */
/* Private variables --------------------------------------------------------- */
/* USB Mass storage Standard Inquiry Data */
int8_t STORAGE_Inquirydata[] = {  /* 36 */
    	/* LUN 0 */ 
	0x00,		
	0x80,		
	0x02,		
	0x02,
	(STANDARD_INQUIRY_DATA_LEN - 5),
	0x00,
	0x00,	
	0x00,
    /* Vendor Identification */
    'A', 'L', 'I', 'E', 'N', 'T', 'E', 'K',//8字节
    /* Product Identification */
    'S', 'P', 'I', ' ', 'F', 'l', 'a', 's', 'h',
    ' ', 'D', 'i', 's', 'k', ' ', ' ',//16字节
    /* Product Revision Level */	
    '1', '.', '0', ' ',							//4字节		
	
//	/* LUN 1 */
//	0x00,
//	0x80,		
//	0x02,		
//	0x02,
//	(STANDARD_INQUIRY_DATA_LEN - 5),
//	0x00,
//	0x00,	
//	0x00,
//	/* Vendor Identification */
//	'A', 'L', 'I', 'E', 'N', 'T', 'E', 'K',' ',	//9字节
//	/* Product Identification */				
//    'N', 'A', 'N', 'D', ' ', 'F', 'l', 'a', 's', 'h',//15字节
//	' ','D', 'i', 's', 'k', 
//    /* Product Revision Level */
//	'1', '.', '0' ,' ',                      	//4字节
	
//	/* LUN 2 */
//	0x00,
//	0x80,		
//	0x02,		
//	0x02,
//	(STANDARD_INQUIRY_DATA_LEN - 5),
//	0x00,
//	0x00,	
//	0x00,
//	/* Vendor Identification */
//	'A', 'L', 'I', 'E', 'N', 'T', 'E', 'K',' ',	//9字节
//	/* Product Identification */				
//	'S', 'D', ' ', 'F', 'l', 'a', 's', 'h', ' ',//15字节
//	'D', 'i', 's', 'k', ' ', ' ',  
//    /* Product Revision Level */
//	'1', '.', '0' ,' ',                      	//4字节
};

/* Private function prototypes ----------------------------------------------- */
int8_t STORAGE_Init(uint8_t lun);
int8_t STORAGE_GetCapacity(uint8_t lun, uint32_t * block_num,
                           uint16_t * block_size);
int8_t STORAGE_IsReady(uint8_t lun);
int8_t STORAGE_IsWriteProtected(uint8_t lun);
int8_t STORAGE_Read(uint8_t lun, uint8_t * buf, uint32_t blk_addr,
                    uint16_t blk_len);
int8_t STORAGE_Write(uint8_t lun, uint8_t * buf, uint32_t blk_addr,
                     uint16_t blk_len);
int8_t STORAGE_GetMaxLun(void);

USBD_StorageTypeDef USBD_DISK_fops = {
  STORAGE_Init,
  STORAGE_GetCapacity,
  STORAGE_IsReady,
  STORAGE_IsWriteProtected,
  STORAGE_Read,
  STORAGE_Write,
  STORAGE_GetMaxLun,
  STORAGE_Inquirydata,
};

/* Private functions --------------------------------------------------------- */

/**
  * @brief  Initializes the storage unit (medium)       
  * @param  lun: Logical unit number
  * @retval Status (0 : OK / -1 : Error)
  */
int8_t STORAGE_Init(uint8_t lun)
{
	uint8_t res = 0;

	switch(lun)
	{
		case 0: // SPI FLASH 作为 U 盘介质
			/* 此处通常需要保证 QSPI 已经完成底层初始化
			   当前项目在 freertos_task 中已调用 W25Qx_QSPI_Init()
			   如果未来改动初始化流程，可在这里补充调用 */
			// W25Qx_QSPI_Init();
			break;
		default:
			break;
	} 
	return res; 
}

/**
  * @brief  Returns the medium capacity.      
  * @param  lun: Logical unit number
  * @param  block_num: Number of total block number
  * @param  block_size: Block size
  * @retval Status (0: OK / -1: Error)
  */
int8_t STORAGE_GetCapacity(uint8_t lun, uint32_t * block_num,
                           uint16_t * block_size)
{
//    HAL_SD_CardInfoTypeDef info;
//    int8_t ret = -1;

    switch(lun)
	{
		case 0://SPI FLASH（512B/扇区，与 FatFs LBA 一一对应）
			*block_size = STORAGE_BLK_SIZ;
			*block_num = STORAGE_BLK_NBR;
			break;
//		case 1://NAND FLASH
//			*block_size=512;
//			*block_num=nand_dev.valid_blocknum*nand_dev.block_pagenum*nand_dev.page_mainsize/512;
//  			break;
//		case 1://SD卡
//            HAL_SD_GetCardInfo(&SDCARD_Handler,&info);
//            *block_num = info.LogBlockNbr - 1;
//            *block_size = info.LogBlockSize;
//			break; 
	}  	
	return 0; 
}

/**
  * @brief  Checks whether the medium is ready.  
  * @param  lun: Logical unit number
  * @retval Status (0: OK / -1: Error)
  */
int8_t STORAGE_IsReady(uint8_t lun)
{

    int8_t ret = 0;   
    USB_STATUS_REG|=0X10;//标�?�轮�?
    return ret;
}

/**
  * @brief  Checks whether the medium is write protected.
  * @param  lun: Logical unit number
  * @retval Status (0: write enabled / -1: otherwise)
  */
int8_t STORAGE_IsWriteProtected(uint8_t lun)
{
    return 0;
}

/**
  * @brief  Reads data from the medium.
  * @param  lun: Logical unit number
  * @param  blk_addr: Logical block address
  * @param  blk_len: Blocks number
  * @retval Status (0: OK / -1: Error)
  */
int8_t STORAGE_Read(uint8_t lun, uint8_t * buf, uint32_t blk_addr,
                    uint16_t blk_len)
{
    int8_t res=0;
    uint32_t start_addr;
    uint32_t total_size;
	USB_STATUS_REG|=0X02;//标�?��?�在读数�?

    if ((uint32_t)blk_addr + (uint32_t)blk_len > STORAGE_BLK_NBR)
    {
        USB_STATUS_REG |= 0X08;
        return -1;
    }

    start_addr = blk_addr * STORAGE_BLK_SIZ;
    total_size = (uint32_t)blk_len * STORAGE_BLK_SIZ;

	switch(lun)
	{
		case 0://SPI FLASH
			if (W25Qx_QSPI_Read(buf, start_addr, total_size) != QSPI_OK)
            {
                res = -1;
            }
			break;
//		case 1://NAND FLASH
//			res=FTL_ReadSectors(buf,blk_addr,512,blk_len);
//			break;
//		case 1://SD卡
//			res=SD_ReadDisk(buf,blk_addr,blk_len);
//			break; 
	} 
	if(res)
	{
		USB_STATUS_REG|=0X08;//读错�?! 
	} 
	return res;
}

/**
  * @brief  Writes data into the medium.
  * @param  lun: Logical unit number
  * @param  blk_addr: Logical block address
  * @param  blk_len: Blocks number
  * @retval Status (0 : OK / -1 : Error)
  */
int8_t STORAGE_Write(uint8_t lun, uint8_t * buf, uint32_t blk_addr,
                     uint16_t blk_len)
{
    int8_t res = 0;
    uint32_t start_addr;
    uint32_t total_size;
    uint32_t curr_addr;
    uint32_t remain_size;
	USB_STATUS_REG |= 0X01; // 标�?��?�在写数�?

    if ((uint32_t)blk_addr + (uint32_t)blk_len > STORAGE_BLK_NBR)
    {
        USB_STATUS_REG |= 0X04;
        return -1;
    }

    start_addr = blk_addr * STORAGE_BLK_SIZ;
    total_size = (uint32_t)blk_len * STORAGE_BLK_SIZ;

	switch(lun)
	{
		case 0: // SPI FLASH（512B LBA + 4KB 子扇区 RMW）
		{
            curr_addr = start_addr;
            remain_size = total_size;

            while ((remain_size > 0U) && (res == 0))
            {
                uint32_t sub_base = (curr_addr / W25QxJV_SUBSECTOR_SIZE) * W25QxJV_SUBSECTOR_SIZE;
                uint32_t sub_off = curr_addr - sub_base;
                uint32_t wr_size = W25QxJV_SUBSECTOR_SIZE - sub_off;
                if (wr_size > remain_size)
                {
                    wr_size = remain_size;
                }

                if (W25Qx_QSPI_Read(msc_subsector_buf, sub_base, W25QxJV_SUBSECTOR_SIZE) != QSPI_OK)
                {
                    res = -1;
                    break;
                }

                memcpy(msc_subsector_buf + sub_off, buf, wr_size);

                if (W25Qx_QSPI_Erase_Block(sub_base) != QSPI_OK)
                {
                    res = -1;
                    break;
                }

                if (W25Qx_QSPI_Write(msc_subsector_buf, sub_base, W25QxJV_SUBSECTOR_SIZE) != QSPI_OK)
                {
                    res = -1;
                    break;
                }

                if (W25Qx_QSPI_Read(msc_verify_buf, sub_base, W25QxJV_SUBSECTOR_SIZE) != QSPI_OK)
                {
                    res = -1;
                    break;
                }
                if (memcmp(msc_verify_buf, msc_subsector_buf, W25QxJV_SUBSECTOR_SIZE) != 0)
                {
                    res = -1;
                    break;
                }

                curr_addr += wr_size;
                buf += wr_size;
                remain_size -= wr_size;
            }
		}
		break;
//		case 1://NAND FLASH
//			res=FTL_WriteSectors(buf,blk_addr,512,blk_len);
//			break;
//		case 1://SD卡
//			res=SD_WriteDisk(buf,blk_addr,blk_len);
//			break; 
	}  

	if (res)
	{
		USB_STATUS_REG |= 0X04; // 写错�?
	}

	return res;
}

/**
  * @brief  Returns the Max Supported LUNs.   
  * @param  None
  * @retval Lun(s) number
  */
int8_t STORAGE_GetMaxLun(void)
{
//    HAL_SD_CardInfoTypeDef info;
//    HAL_SD_GetCardInfo(&SDCARD_Handler,&info);
//    
//    if(info.LogBlockNbr)return STORAGE_LUN_NBR-1;
//	else return STORAGE_LUN_NBR-2;
    return 0;
}


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
