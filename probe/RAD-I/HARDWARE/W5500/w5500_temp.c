/***********************************************************************************
成都浩然电子有限公司
WIZnet官方代理商，09年起一直蝉联最佳代理商，为客户提供技术、产品、售后等全方位服务
电话：028-86127089     0755-86066647
传真：028-86127039
网址：http://www.hschip.com
日期：2016-01

硬件平台： 浩然电子评估板  HS-EVBW5500 /STM32
W5500 技术交流QQ群： 722479032
WIZnet技术交流QQ群： 290473222
										 
SPI模式1或SPI模式2通过w5500.h 文件的
#define MODE_SPI  1     // 选择SPI1  
#define MODE_SPI  2     // 选择SPI2
来进行选择
***********************************************************************************/


#include "w5500.h"
#include "types.h"
#include "spi.h"
#include "main.h"
extern SPI_HandleTypeDef hspi1;

extern int printf(const char *fmt, ...);

#ifdef __DEF_IINCHIP_PPP__
   #include "md5.h"
#endif

static uint16 SSIZE[MAX_SOCK_NUM]; /**< Max Tx buffer size by each channel */
static uint16 RSIZE[MAX_SOCK_NUM]; /**< Max Rx buffer size by each channel */


uint16 getIINCHIP_RxMAX(uint8 s)
{
   return RSIZE[s];
}

uint16 getIINCHIP_TxMAX(uint8 s)
{
   return SSIZE[s];
}

void IINCHIP_CSoff(void)
{
  HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_RESET);
}

void IINCHIP_CSon(void)
{
   HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_SET);
}

uint8_t  IINCHIP_SpiSendData(uint8 dat)
{
    uint8_t read_data = 0;
    
    HAL_SPI_TransmitReceive(&hspi1, &dat, &read_data, 1, 0x0100);
   return read_data;
//   return(spi1_read_write_byte(dat));
}

void IINCHIP_WRITE( uint32 addrbsb,  uint8 data)
{
   IINCHIP_ISR_DISABLE();                        // Interrupt Service Routine Disable
   IINCHIP_CSoff();                              // CS=0, SPI start
   IINCHIP_SpiSendData( (addrbsb & 0x00FF0000)>>16);// Address byte 1
   IINCHIP_SpiSendData( (addrbsb & 0x0000FF00)>> 8);// Address byte 2
   IINCHIP_SpiSendData( (addrbsb & 0x000000F8) + 4);    // Data write command and Write data length 1
   IINCHIP_SpiSendData(data);                    // Data write (write 1byte data)
   IINCHIP_CSon();                               // CS=1,  SPI end
   IINCHIP_ISR_ENABLE();                         // Interrupt Service Routine Enable
}

uint8 IINCHIP_READ(uint32 addrbsb)
{
   uint8 data = 0;
   IINCHIP_ISR_DISABLE();                        // Interrupt Service Routine Disable
   IINCHIP_CSoff();                              // CS=0, SPI start
   IINCHIP_SpiSendData( (addrbsb & 0x00FF0000)>>16);// Address byte 1
   IINCHIP_SpiSendData( (addrbsb & 0x0000FF00)>> 8);// Address byte 2
   IINCHIP_SpiSendData( (addrbsb & 0x000000F8))    ;// Data read command and Read data length 1
   data = IINCHIP_SpiSendData(0x00);             // Data read (read 1byte data)
   IINCHIP_CSon();                               // CS=1,  SPI end
   IINCHIP_ISR_ENABLE();                         // Interrupt Service Routine Enable
   return data;    
}

uint16 wiz_write_buf(uint32 addrbsb,uint8* buf,uint16 len)
{
   uint16 idx = 0;
   if(len == 0) printf("Unexpected2 length 0\r\n");

   IINCHIP_ISR_DISABLE();
   IINCHIP_CSoff();                              // CS=0, SPI start
   IINCHIP_SpiSendData( (addrbsb & 0x00FF0000)>>16);// Address byte 1
   IINCHIP_SpiSendData( (addrbsb & 0x0000FF00)>> 8);// Address byte 2
   IINCHIP_SpiSendData( (addrbsb & 0x000000F8) + 4);    // Data write command and Write data length 1
   for(idx = 0; idx < len; idx++)                // Write data in loop
   {
     IINCHIP_SpiSendData(buf[idx]);
   }
   IINCHIP_CSon();                               // CS=1, SPI end
   IINCHIP_ISR_ENABLE();                         // Interrupt Service Routine Enable    

   return len;  
}

uint16 wiz_read_buf(uint32 addrbsb, uint8* buf,uint16 len)
{
  uint16 idx = 0;
  if(len == 0)				// 若接收到的数据长度为0，则串口打印“Unexpected2 length 0”
  {
    printf("Unexpected2 length 0\r\n");
  }

  IINCHIP_CSoff();                                  	// CS=0, SPI开启
  IINCHIP_SpiSendData( (addrbsb & 0x00FF0000)>>16);		// 通过SPI发送16位地址段给MCU
  IINCHIP_SpiSendData( (addrbsb & 0x0000FF00)>> 8);		// 
  IINCHIP_SpiSendData( (addrbsb & 0x000000F8));    		// 设置SPI为读操作
  for(idx = 0; idx < len; idx++)                    	// 将buf中的数据通过SPI发送给MCU
  {
    buf[idx] = IINCHIP_SpiSendData(0x00);
  }
  IINCHIP_CSon();                                   	// CS=1, SPI关闭
  
  return len;																					// 返回已接收数据的长度值
}


/**
@brief  This function is for resetting of the iinchip. Initializes the iinchip to work in whether DIRECT or INDIRECT mode
*/
void iinchip_init(void)
{
  setMR( MR_RST );
#ifdef __DEF_IINCHIP_DBG__
  printf("MR value is %02x \r\n",IINCHIP_READ_COMMON(MR));
#endif
}

/**
@brief  This function set the transmit & receive buffer size as per the channels is used
Note for TMSR and RMSR bits are as follows\n
bit 1-0 : memory size of channel #0 \n
bit 3-2 : memory size of channel #1 \n
bit 5-4 : memory size of channel #2 \n
bit 7-6 : memory size of channel #3 \n
