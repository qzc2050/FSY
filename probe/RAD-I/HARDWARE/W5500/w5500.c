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
#include "portmacro.h"

extern SPI_HandleTypeDef hspi1;

/*
 * W5500 SPI 短临界：用 ulPortRaiseBASEPRI / 恢复，不调用 taskENTER_CRITICAL。
 * 否则与 vPortEnterCritical 共用 uxCriticalNesting，易与 printf/堆/驱动临界区
 * 交叉错位，触发 vPortExitCritical(port.c:450)。
 */
static uint32_t s_iinchip_spi_saved_basepri;

void iinchip_spi_enter(void)
{
    s_iinchip_spi_saved_basepri = ulPortRaiseBASEPRI();
}

void iinchip_spi_exit(void)
{
    vPortSetBASEPRI(s_iinchip_spi_saved_basepri);
}

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



   IINCHIP_ISR_DISABLE();
   IINCHIP_CSoff();                              // CS=0, SPI start
   IINCHIP_SpiSendData( (addrbsb & 0x00FF0000)>>16);// Address byte 1
   IINCHIP_SpiSendData( (addrbsb & 0x0000FF00)>> 8);// Address byte 2
   IINCHIP_SpiSendData( (addrbsb & 0x000000F8) + 4);    // Data write command and Write data length 1
   for(uint16_t idx = 0; idx < len; idx++)                // Write data in loop
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
  if (len == 0)
  {
    return 0;
  }

  IINCHIP_ISR_DISABLE();
  IINCHIP_CSoff();
  IINCHIP_SpiSendData( (addrbsb & 0x00FF0000)>>16);		// 通过SPI发送16位地址段给MCU
  IINCHIP_SpiSendData( (addrbsb & 0x0000FF00)>> 8);		// 
  IINCHIP_SpiSendData( (addrbsb & 0x000000F8));    		// 设置SPI为读操作
  for(idx = 0; idx < len; idx++)                    	// 将buf中的数据通过SPI发送给MCU
  {
    buf[idx] = IINCHIP_SpiSendData(0x00);
  }
  IINCHIP_CSon();
  IINCHIP_ISR_ENABLE();
  
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
bit 9-8 : memory size of channel #4 \n
bit 11-10 : memory size of channel #5 \n
bit 12-12 : memory size of channel #6 \n
bit 15-14 : memory size of channel #7 \n
Maximum memory size for Tx, Rx in the W5500 is 16K Bytes,\n
In the range of 16KBytes, the memory size could be allocated dynamically by each channel.\n
Be attentive to sum of memory size shouldn't exceed 8Kbytes\n
and to data transmission and receiption from non-allocated channel may cause some problems.\n
If the 16KBytes memory is already  assigned to centain channel, \n
other 3 channels couldn't be used, for there's no available memory.\n
If two 4KBytes memory are assigned to two each channels, \n
other 2 channels couldn't be used, for there's no available memory.\n
*/
void sysinit( uint8 * tx_size, uint8 * rx_size  )
{
  int16 i;
  int16 ssum,rsum;
#ifdef __DEF_IINCHIP_DBG__
  printf("sysinit()\r\n");
#endif
  ssum = 0;
  rsum = 0;

  for (i = 0 ; i < MAX_SOCK_NUM; i++)       // Set the size, masking and base address of Tx & Rx memory by each channel
  {
          IINCHIP_WRITE( (Sn_TXMEM_SIZE(i)), tx_size[i]);
          IINCHIP_WRITE( (Sn_RXMEM_SIZE(i)), rx_size[i]);
          
#ifdef __DEF_IINCHIP_DBG__
         printf("tx_size[%d]: %d, Sn_TXMEM_SIZE = %d\r\n",i, tx_size[i], IINCHIP_READ(Sn_TXMEM_SIZE(i)));
         printf("rx_size[%d]: %d, Sn_RXMEM_SIZE = %d\r\n",i, rx_size[i], IINCHIP_READ(Sn_RXMEM_SIZE(i)));
#endif
    SSIZE[i] = (int16)(0);
    RSIZE[i] = (int16)(0);

// W5500有8个Socket，每个Socket有对应独立的收发缓存区。
// 每个Socket的发送/接收缓存区都在一个16KB的物理发送内存中，初始化分配为2KB。
// 无论给每个Socket分配多大的收/发缓存，都必须在16KB以内。

    if (ssum <= 16384)										// 设置Socket发送缓存空间的大小
    {
       switch( tx_size[i] )
				{
					case 1:
						SSIZE[i] = (int16)(1024);			// i=1，tx_size=1KB
					break;
					case 2:
						SSIZE[i] = (int16)(2048);			// i=2，tx_size=2KB
					break;
					case 4:
						SSIZE[i] = (int16)(4096);			// i=4，tx_size=4KB
					break;
					case 8:
						SSIZE[i] = (int16)(8192);			// i=8，tx_size=8KB
					break;
					case 16:
						SSIZE[i] = (int16)(16384);		// i=16，tx_size=16KB
					break;
					default :
						RSIZE[i] = (int16)(2048);			// 默认i=2，tx_size=2KB
					break;
				}
		}

			if (rsum <= 16384)									// 设置Socket接收缓存空间的大小
			{
					switch( rx_size[i] )				
					{
						case 1:
							RSIZE[i] = (int16)(1024);		// i=1，rx_size=1KB
						break;
						case 2:
							RSIZE[i] = (int16)(2048);		// i=2，rx_size=2KB
						break;
						case 4:
							RSIZE[i] = (int16)(4096);		// i=4，rx_size=4KB
						break;
						case 8:
							RSIZE[i] = (int16)(8192);		// i=8，rx_size=8KB
						break;
						case 16:
							RSIZE[i] = (int16)(16384);	// i=16，rx_size=16KB
						break;
						default :
							RSIZE[i] = (int16)(2048);		// 默认i=2，rx_size=2K
						break;
					}
			}
    ssum += SSIZE[i];
    rsum += RSIZE[i];
  }
}

// added







/**
@brief  This function sets up gateway IP address.
*/
void setGAR(
  uint8 * addr  /**< a pointer to a 4 -byte array responsible to set the Gateway IP address. */
  )
{
    wiz_write_buf(GAR0, addr, 4);
}
void getGWIP(uint8 * addr)
{
    wiz_read_buf(GAR0, addr, 4);
}

/**
@brief  It sets up SubnetMask address
*/
void setSUBR(uint8 * addr)
{   
    wiz_write_buf(SUBR0, addr, 4);
}
/**
@brief  This function sets up MAC address.
*/
void setSHAR(
  uint8 * addr  /**< a pointer to a 6 -byte array responsible to set the MAC address. */
  )
{
  wiz_write_buf(SHAR0, addr, 6);  
}

/**
@brief  This function sets up Source IP address.
*/
void setSIPR(
  uint8 * addr  /**< a pointer to a 4 -byte array responsible to set the Source IP address. */
  )
{
    wiz_write_buf(SIPR0, addr, 4);  
}

/**
@brief  W5500心跳检测程序，设置Socket在线时间寄存器Sn_KPALVTR，单位为5s
*/
void setkeepalive(SOCKET s)
{ 
  IINCHIP_WRITE(Sn_KPALVTR(s),0x02);
}

/**
@brief  This function sets up Source IP address.
*/
void getGAR(uint8 * addr)
{
    wiz_read_buf(GAR0, addr, 4);
}
void getSUBR(uint8 * addr)
{
    wiz_read_buf(SUBR0, addr, 4);
}
void getSHAR(uint8 * addr)
{
    wiz_read_buf(SHAR0, addr, 6);
}
void getSIPR(uint8 * addr)
{
    wiz_read_buf(SIPR0, addr, 4);
}

void setMR(uint8 val)
{
  IINCHIP_WRITE(MR,val);
}


uint8 getPHYCFGR( void )
{
   return IINCHIP_READ(PHYCFGR);
}

/**
@brief  This function gets Interrupt register in common register.
 */
uint8 getIR( void )
{
   return IINCHIP_READ(IR);
}

/**
@brief  This function sets up Retransmission time.

If there is no response from the peer or delay in response then retransmission
will be there as per RTR (Retry Time-value Register)setting
*/
void setRTR(uint16 timeout)
{
  IINCHIP_WRITE(RTR0,(uint8)((timeout & 0xff00) >> 8));
  IINCHIP_WRITE(RTR1,(uint8)(timeout & 0x00ff));
}

/**
@brief  This function set the number of Retransmission.

If there is no response from the peer or delay in response then recorded time
as per RTR & RCR register seeting then time out will occur.
*/
void setRCR(uint8 retry)
{
  IINCHIP_WRITE(WIZ_RCR,retry);
}

/**
@brief  This function set the interrupt mask Enable/Disable appropriate Interrupt. ('1' : interrupt enable)

If any bit in IMR is set as '0' then there is not interrupt signal though the bit is
set in IR register.
*/
void clearIR(uint8 mask)
{
  IINCHIP_WRITE(IR, ~mask | getIR() ); // must be setted 0x10.
}

/**
@brief  This sets the maximum segment size of TCP in Active Mode), while in Passive Mode this is set by peer
*/
void setSn_MSS(SOCKET s, uint16 Sn_MSSR)
{
  IINCHIP_WRITE( Sn_MSSR0(s), (uint8)((Sn_MSSR & 0xff00) >> 8));
  IINCHIP_WRITE( Sn_MSSR1(s), (uint8)(Sn_MSSR & 0x00ff));
}
/*
void setSn_TTL(SOCKET s, uint8 ttl)
{    
   IINCHIP_WRITE( Sn_TTL(s) , ttl);
}

*/

/**
@brief  get socket interrupt status

These below functions are used to read the Interrupt & Soket Status register
*/
uint8 getSn_IR(SOCKET s)
{
   return IINCHIP_READ(Sn_IR(s));
}


/**
@brief   get socket status
*/
uint8 getSn_SR(SOCKET s)
{
   return IINCHIP_READ(Sn_SR(s));
}


/**
@brief  get socket TX free buf size

This gives free buffer size of transmit buffer. This is the data size that user can transmit.
User shuold check this value first and control the size of transmitting data
*/
uint16 getSn_TX_FSR(SOCKET s)
{
  uint16 val=0,val1=0;
  do
  {
    val1 = IINCHIP_READ(Sn_TX_FSR0(s));
    val1 = (val1 << 8) + IINCHIP_READ(Sn_TX_FSR1(s));
      if (val1 != 0)
    {
        val = IINCHIP_READ(Sn_TX_FSR0(s));
        val = (val << 8) + IINCHIP_READ(Sn_TX_FSR1(s));
    }
  } while (val != val1);
   return val;
}


/**
@brief   get socket RX recv buf size

This gives size of received data in receive buffer.
*/
uint16 getSn_RX_RSR(SOCKET s)														// 获取空闲接收缓存寄存器的值
{
  uint16 val=0,val1=0;
  do
  {
    val1 = IINCHIP_READ(Sn_RX_RSR0(s));									// MCU读Sn_RX_RSR的低8位，并赋给val1
    val1 = (val1 << 8) + IINCHIP_READ(Sn_RX_RSR1(s));		// 读高8位，并与低8位相加赋给val1
    if(val1 != 0)																				// 若Sn_RX_RSR的值不为0，将其赋给val
    {
        val = IINCHIP_READ(Sn_RX_RSR0(s));
        val = (val << 8) + IINCHIP_READ(Sn_RX_RSR1(s));
    }
  } while (val != val1);																// 判断val与val1是否相等，若不等，重新返回do循环，若相等，跳出循环
   return val;																					// 将val的值返回给getSn_RX_RSR
}


/**
@brief   This function is being called by send() and sendto() function also.

This function read the Tx write pointer register and after copy the data in buffer update the Tx write pointer
register. User should read upper byte first and lower byte later to get proper value.
*/
void send_data_processing(SOCKET s, uint8 *data, uint16 len)
{
  uint16 ptr =0;
  uint32 addrbsb =0;
  if(len == 0)
  {
    //printf("CH: %d Unexpected1 length 0\r\n", s);
    return;
  }

 
  ptr = IINCHIP_READ( Sn_TX_WR0(s) );
  ptr = ((ptr & 0x00ff) << 8) + IINCHIP_READ(Sn_TX_WR1(s));

  addrbsb = (uint32)(ptr<<8) + (s<<5) + 0x10;
  wiz_write_buf(addrbsb, data, len);
  
  ptr += len;
  IINCHIP_WRITE( Sn_TX_WR0(s) ,(uint8)((ptr & 0xff00) >> 8));
  IINCHIP_WRITE( Sn_TX_WR1(s),(uint8)(ptr & 0x00ff));
}


/**
@brief  This function is being called by recv() also.

This function read the Rx read pointer register
and after copy the data from receive buffer update the Rx write pointer register.
User should read upper byte first and lower byte later to get proper value.
*/
void recv_data_processing(SOCKET s, uint8 *data, uint16 len)
{
  uint16 ptr = 0;
  uint32 addrbsb = 0;
  
  
  
  
  
  
  
	// MCU读取Sn_RX_RD接收写指针寄存器的值，并赋给ptr
	// Sn_RX_RD保存接收缓存中数据的首地址，若有数据接收，则接收完后该寄存器值要更新
  ptr = IINCHIP_READ( Sn_RX_RD0(s) );
  ptr = ((ptr & 0x00ff) << 8) + IINCHIP_READ( Sn_RX_RD1(s) );
	
  addrbsb = (uint32)(ptr<<8) + (s<<5) + 0x18;		// 获取接收到的数据的绝对地址
  wiz_read_buf(addrbsb, data, len);							// 通过绝对地址，将接收到的数据发给MCU
  
	// 更新Sn_RX_RD寄存器的值
	ptr += len;														// 
  IINCHIP_WRITE( Sn_RX_RD0(s), (uint8)((ptr & 0xff00) >> 8));
  IINCHIP_WRITE( Sn_RX_RD1(s), (uint8)(ptr & 0x00ff));
}


void setSn_IR(uint8 s, uint8 val)
{
    IINCHIP_WRITE(Sn_IR(s), val);
}





