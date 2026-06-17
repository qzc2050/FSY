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

#include "device.h"
#include "socket.h"
#include "config.h"
#include "network_cmd.h"

extern int printf(const char *fmt, ...);

static uint16_t local_port;

uint16_t socket_local_port[3] = {DATA_UPLOAD_SOCKET_PORT, SETTING_SOCKET_PORT, (DATA_UPLOAD_SOCKET_PORT + 2)};
/**
@brief   This Socket function initialize the channel in perticular mode, and set the port and wait for W5200 done it.
@return  1 for sucess else 0.
*/
uint8_t socket(SOCKET s, uint8_t protocol, uint16_t port, uint8_t flag)
{
   uint8_t ret;
   if (
        ((protocol&0x0F) == Sn_MR_TCP)    ||
        ((protocol&0x0F) == Sn_MR_UDP)    ||
        ((protocol&0x0F) == Sn_MR_IPRAW)  ||
        ((protocol&0x0F) == Sn_MR_MACRAW) ||
        ((protocol&0x0F) == Sn_MR_PPPOE)
      )
   {
      close(s);
		 
		  if((protocol&0x0F) == Sn_MR_TCP)  // 如果是TCP 模式，需要设置心跳   2019-02-11
			{	
				setkeepalive(s); 
			}	
      IINCHIP_WRITE(Sn_MR(s) ,protocol | flag);
      if (port != 0) {
         IINCHIP_WRITE( Sn_PORT0(s) ,(uint8_t)((port & 0xff00) >> 8));
         IINCHIP_WRITE( Sn_PORT1(s) ,(uint8_t)(port & 0x00ff));
      } else {
         local_port++; // if don't set the source port, set local_port number.
         IINCHIP_WRITE(Sn_PORT0(s) ,(uint8_t)((local_port & 0xff00) >> 8));
         IINCHIP_WRITE(Sn_PORT1(s) ,(uint8_t)(local_port & 0x00ff));
      }
      IINCHIP_WRITE( Sn_CR(s) ,Sn_CR_OPEN); // run sockinit Sn_CR

      /* wait to process the command... */
      while( IINCHIP_READ(Sn_CR(s)) )
         ;
      /* ------- */
      ret = 1;
   }
   else
   {
      ret = 0;
   }
   return ret;
}


/**
@brief   This function close the socket and parameter is "s" which represent the socket number
*/
void close(SOCKET s)
{

   IINCHIP_WRITE( Sn_CR(s) ,Sn_CR_CLOSE);

   /* wait to process the command... */
   while( IINCHIP_READ(Sn_CR(s) ) )
      ;
   /* ------- */
        /* all clear */
   IINCHIP_WRITE( Sn_IR(s) , 0xFF);
}


/**
@brief   This function established  the connection for the channel in passive (server) mode. This function waits for the request from the peer.
@return  1 for success else 0.
*/
uint8_t listen(SOCKET s)
{
   uint8_t ret;			// 定义一个监听标志位，若Sn_CR的LISTEN命令发送成功，其值为1，否则为0
   
	if (IINCHIP_READ( Sn_SR(s) ) == SOCK_INIT)		// 若Sn_SR处于初始化状态，进入循环
   {
      IINCHIP_WRITE( Sn_CR(s) ,Sn_CR_LISTEN);		// MCU配置W5500为监听状态

      while( IINCHIP_READ(Sn_CR(s) ) )					// 配置完成，Sn_CR自动清零
         ;
      ret = 1;																	// LISTEN命令发送成功，ret=1
   }
   else
   {
      ret = 0;																	// 否则，ret=0
   }
   return ret;
}


/**
@brief   This function established  the connection for the channel in Active (client) mode.
      This function waits for the untill the connection is established.

@return  1 for success else 0.
*/
uint8_t connect(SOCKET s, uint8_t * addr, uint16_t port)
{
    uint8_t ret;
    if
        (
            ((addr[0] == 0xFF) && (addr[1] == 0xFF) && (addr[2] == 0xFF) && (addr[3] == 0xFF)) ||
            ((addr[0] == 0x00) && (addr[1] == 0x00) && (addr[2] == 0x00) && (addr[3] == 0x00)) ||
            (port == 0x00)
        )
    {
      ret = 0;
    }
    else
    {
        ret = 1;
        // set destination IP
        IINCHIP_WRITE( Sn_DIPR0(s), addr[0]);
        IINCHIP_WRITE( Sn_DIPR1(s), addr[1]);
        IINCHIP_WRITE( Sn_DIPR2(s), addr[2]);
        IINCHIP_WRITE( Sn_DIPR3(s), addr[3]);
        IINCHIP_WRITE( Sn_DPORT0(s), (uint8_t)((port & 0xff00) >> 8));
        IINCHIP_WRITE( Sn_DPORT1(s), (uint8_t)(port & 0x00ff));
        IINCHIP_WRITE( Sn_CR(s) ,Sn_CR_CONNECT);
        /* wait for completion */
        while ( IINCHIP_READ(Sn_CR(s) ) ) ;

//        while ( IINCHIP_READ(Sn_SR(s)) != SOCK_SYNSENT )
//        {
//            if(IINCHIP_READ(Sn_SR(s)) == SOCK_ESTABLISHED)
//            {
//                break;
//            }
//            if (getSn_IR(s) & Sn_IR_TIMEOUT)
//            {
//                IINCHIP_WRITE(Sn_IR(s), (Sn_IR_TIMEOUT));  // clear TIMEOUT Interrupt
//                ret = 0;
//                break;
//            }
//        }
    }

   return ret;
}



/**
@brief   This function used for disconnect the socket and parameter is "s" which represent the socket number
@return  1 for success else 0.
*/
void disconnect(SOCKET s)
{
   IINCHIP_WRITE( Sn_CR(s) ,Sn_CR_DISCON);

   /* wait to process the command... */
   while( IINCHIP_READ(Sn_CR(s) ) )
      ;
   /* ------- */
}


/**
@brief   This function used to send the data in TCP mode
@return  1 for success else 0.
*/
uint16_t send(SOCKET s, const uint8_t * buf, uint16_t len)
{
  uint8_t status=0;
  uint16_t ret=0;
  uint16_t freesize=0;

  if (len > getIINCHIP_TxMAX(s)) ret = getIINCHIP_TxMAX(s);
  else ret = len;

  do
  {
    freesize = getSn_TX_FSR(s);
    status = IINCHIP_READ(Sn_SR(s));
    if ((status != SOCK_ESTABLISHED) && (status != SOCK_CLOSE_WAIT))
    {
      ret = 0;
      break;
    }
  } while (freesize < ret);

  send_data_processing(s, (uint8_t *)buf, ret);
  IINCHIP_WRITE( Sn_CR(s) ,Sn_CR_SEND);

  while( IINCHIP_READ(Sn_CR(s) ) );

  while ( (IINCHIP_READ(Sn_IR(s) ) & Sn_IR_SEND_OK) != Sn_IR_SEND_OK )
  {
    status = IINCHIP_READ(Sn_SR(s));
    if ((status != SOCK_ESTABLISHED) && (status != SOCK_CLOSE_WAIT) )
    {
      printf("SEND_OK Problem!!\r\n");
      close(s);
      return 0;
    }
  }
  IINCHIP_WRITE( Sn_IR(s) , Sn_IR_SEND_OK);

#ifdef __DEF_IINCHIP_INT__
   putISR(s, getISR(s) & (~Sn_IR_SEND_OK));
#else
   IINCHIP_WRITE( Sn_IR(s) , Sn_IR_SEND_OK);
#endif

   return ret;
}



/**
@brief   This function is an application I/F function which is used to receive the data in TCP mode.
      It continues to wait for data as much as the application wants to receive.

@return  received data size for success else -1.
*/
uint16_t recv(SOCKET s, uint8_t * buf, uint16_t len)
{
   uint16_t ret=0;

   if (len > 0)
   {
      recv_data_processing(s, buf, len);				// 数据接收进程：将通过Sockets的buf接受的长度为len的数据写入指针对应的MCU的缓存地址

	  IINCHIP_WRITE(Sn_CR(s) ,Sn_CR_RECV);			// MCU配置Sn_CR为RECV

      while(IINCHIP_READ(Sn_CR(s)));					// 配置完成，Sn_CR自动清零

      ret = len;								// 将接收数据长度值赋给ret
   }

   return ret;										// 返回ret的值。有返回值说明W5500有数据接收，并不断重复接收这一进程
}


/**
@brief   This function is an application I/F function which is used to send the data for other then TCP mode.
      Unlike TCP transmission, The peer's destination address and the port is needed.

@return  This function return send data size for success else -1.
*/
uint16_t sendto(SOCKET s, const uint8_t * buf, uint16_t len, uint8_t * addr, uint16_t port)
{
   uint16_t ret=0;

   if (len > getIINCHIP_TxMAX(s)) 
   ret = getIINCHIP_TxMAX(s); // check size not to exceed MAX size.
   else ret = len;

   if( ((addr[0] == 0x00) && (addr[1] == 0x00) && (addr[2] == 0x00) && (addr[3] == 0x00)) || ((port == 0x00)) )//||(ret == 0) )
   {
      /* added return value */
      ret = 0;
   }
   else
   {
      IINCHIP_WRITE( Sn_DIPR0(s), addr[0]);
      IINCHIP_WRITE( Sn_DIPR1(s), addr[1]);
      IINCHIP_WRITE( Sn_DIPR2(s), addr[2]);
      IINCHIP_WRITE( Sn_DIPR3(s), addr[3]);
      IINCHIP_WRITE( Sn_DPORT0(s),(uint8_t)((port & 0xff00) >> 8));
      IINCHIP_WRITE( Sn_DPORT1(s),(uint8_t)(port & 0x00ff));
      // copy data
      send_data_processing(s, (uint8_t *)buf, ret);
      IINCHIP_WRITE( Sn_CR(s) ,Sn_CR_SEND);
      /* wait to process the command... */
      while( IINCHIP_READ( Sn_CR(s) ) )
         ;
      /* ------- */
     while( (IINCHIP_READ( Sn_IR(s) ) & Sn_IR_SEND_OK) != Sn_IR_SEND_OK )
     {
      if (IINCHIP_READ( Sn_IR(s) ) & Sn_IR_TIMEOUT)
      {
            /* clear interrupt */
         IINCHIP_WRITE( Sn_IR(s) , (Sn_IR_SEND_OK | Sn_IR_TIMEOUT)); /* clear SEND_OK & TIMEOUT */
         return 0;
      }
     }
      IINCHIP_WRITE( Sn_IR(s) , Sn_IR_SEND_OK);
   }
   return ret;
}


/*
@brief   This function is an application I/F function which is used to receive the data in other then
   TCP mode. This function is used to receive UDP, IP_RAW and MAC_RAW mode, and handle the header as well.

@return  This function return received data size for success else -1.
*/
uint16_t recvfrom(SOCKET s, uint8_t * buf, uint16_t len, uint8_t * addr, uint16_t *port)
{
   uint8_t head[8];
   uint16_t data_len=0;
   uint16_t ptr=0;
   uint32_t addrbsb =0;
   /* 无数据时禁止读 RX：否则 Sn_RX_RD/长度无意义，DHCP 等会解析到随机长度并崩溃 */
   if (getSn_RX_RSR(s) == 0)
   {
      return 0;
   }
   if ( len > 0 )
   {
      ptr     = IINCHIP_READ(Sn_RX_RD0(s) );
      ptr     = ((ptr & 0x00ff) << 8) + IINCHIP_READ(Sn_RX_RD1(s));
      addrbsb = (uint32_t)(ptr<<8) +  (s<<5) + 0x18;
      
      switch (IINCHIP_READ(Sn_MR(s) ) & 0x07)
      {
      case Sn_MR_UDP :
        wiz_read_buf(addrbsb, head, 0x08);        
        ptr += 8;
        // read peer's IP address, port number.
        if (addr != NULL)
        {
           addr[0]  = head[0];
           addr[1]  = head[1];
           addr[2]  = head[2];
           addr[3]  = head[3];
        }
        if (port != NULL)
        {
           *port    = head[4];
           *port    = (*port << 8) + head[5];
        }
        data_len = head[6];
        data_len = (data_len << 8) + head[7];

        /* 防止缓冲区溢出 - 限制读取长度 */
        if(data_len > len)
        {
            data_len = len;
        }
        
        addrbsb = (uint32_t)(ptr<<8) +  (s<<5) + 0x18;
        wiz_read_buf(addrbsb, buf, data_len);                
        ptr += data_len;

        IINCHIP_WRITE( Sn_RX_RD0(s), (uint8_t)((ptr & 0xff00) >> 8));
        IINCHIP_WRITE( Sn_RX_RD1(s), (uint8_t)(ptr & 0x00ff));
        break;

      case Sn_MR_IPRAW :
        wiz_read_buf(addrbsb, head, 0x06);        
        ptr += 6;
        if (addr != NULL)
        {
           addr[0]  = head[0];
           addr[1]  = head[1];
           addr[2]  = head[2];
           addr[3]  = head[3];
        }
        data_len = head[4];
        data_len = (data_len << 8) + head[5];

        /* 防止缓冲区溢出 - 限制读取长度 */
        if(data_len > len)
        {
            data_len = len;
        }
        
        addrbsb  = (uint32_t)(ptr<<8) +  (s<<5) + 0x18;
        wiz_read_buf(addrbsb, buf, data_len);        
        ptr += data_len;

        IINCHIP_WRITE( Sn_RX_RD0(s), (uint8_t)((ptr & 0xff00) >> 8));
        IINCHIP_WRITE( Sn_RX_RD1(s), (uint8_t)(ptr & 0x00ff));
        break;

      case Sn_MR_MACRAW :
        wiz_read_buf(addrbsb, head, 0x02);
        ptr+=2;
        data_len = head[0];
        data_len = (data_len<<8) + head[1] - 2;
        if(data_len > 1514)
        {
           data_len = 1514;
        }

        /* 防止缓冲区溢出 - 限制读取长度 */
        if(data_len > len)
        {
            data_len = len;
        }
        
        addrbsb  = (uint32_t)(ptr<<8) +  (s<<5) + 0x18;
        wiz_read_buf(addrbsb, buf, data_len);
        ptr += data_len;

        IINCHIP_WRITE( Sn_RX_RD0(s), (uint8_t)((ptr & 0xff00) >> 8));
        IINCHIP_WRITE( Sn_RX_RD1(s), (uint8_t)(ptr & 0x00ff));
        break;

      default :
            break;
      }
      IINCHIP_WRITE( Sn_CR(s) ,Sn_CR_RECV);

      /* wait to process the command... */
      while( IINCHIP_READ( Sn_CR(s)) ) ;
      /* ------- */
   }
   return data_len;
}

#ifdef __MACRAW__
void macraw_open(void)
{
  uint8_t sock_num;
  uint16_t dummyPort = 0;
  uint8_t mFlag = 0;
  sock_num = 0;


  close(sock_num); // Close the 0-th socket
  socket(sock_num, Sn_MR_MACRAW, dummyPort,mFlag);  // OPen the 0-th socket with MACRAW mode
}


uint16_t macraw_send( const uint8_t * buf, uint16_t len )
{
   uint16_t ret=0;
   uint8_t sock_num;
   sock_num =0;


   if (len > getIINCHIP_TxMAX(sock_num)) ret = getIINCHIP_TxMAX(sock_num); // check size not to exceed MAX size.
   else ret = len;

   send_data_processing(sock_num, (uint8_t *)buf, len);

   //W5500 SEND COMMAND
   IINCHIP_WRITE(Sn_CR(sock_num),Sn_CR_SEND);
   while( IINCHIP_READ(Sn_CR(sock_num)) );
   while ( (IINCHIP_READ(Sn_IR(sock_num)) & Sn_IR_SEND_OK) != Sn_IR_SEND_OK );
   IINCHIP_WRITE(Sn_IR(sock_num), Sn_IR_SEND_OK);

   return ret;
}

uint16_t macraw_recv( uint8_t * buf, uint16_t len )
{
   uint8_t sock_num;
   uint16_t data_len=0;
   uint16_t dummyPort = 0;
   uint16_t ptr = 0;
   uint8_t mFlag = 0;
   sock_num = 0;

   if ( len > 0 )
   {

      data_len = 0;

      ptr = IINCHIP_READ(Sn_RX_RD0(sock_num));
      ptr = (uint16_t)((ptr & 0x00ff) << 8) + IINCHIP_READ( Sn_RX_RD1(sock_num) );
      //-- read_data(s, (uint8_t *)ptr, data, len); // read data
      data_len = IINCHIP_READ_RXBUF(0, ptr);
      ptr++;
      data_len = ((data_len<<8) + IINCHIP_READ_RXBUF(0, ptr)) - 2;
      ptr++;

      if(data_len > 1514)
      {
         printf("data_len over 1514\r\n");
         printf("\r\nptr: %X, data_len: %X", ptr, data_len);
         //while(1);
         /** recommand : close and open **/
         close(sock_num); // Close the 0-th socket
         socket(sock_num, Sn_MR_MACRAW, dummyPort,mFlag);  // OPen the 0-th socket with MACRAW mode
         return 0;
      }

      IINCHIP_READ_RXBUF_BURST(sock_num, ptr, data_len, (uint8_t*)(buf));
      ptr += data_len;

      IINCHIP_WRITE(Sn_RX_RD0(sock_num),(uint8_t)((ptr & 0xff00) >> 8));
      IINCHIP_WRITE(Sn_RX_RD1(sock_num),(uint8_t)(ptr & 0x00ff));
      IINCHIP_WRITE(Sn_CR(sock_num), Sn_CR_RECV);
      while( IINCHIP_READ(Sn_CR(sock_num)) ) ;
   }

   return data_len;
}
#endif


/*****************************************************************************************
 * 功能: 通过对应的套接字发送数据
 * 形参: socket_num：套接字端口	s_data：要发送的数据 len：要发送的数据长度
 * 返回: 无
 * 说明: 无 
 *****************************************************************************************/
void w5500_socket_send_data(uint8_t socket_num, uint8_t *s_data, uint32_t len)
{
	switch(getSn_SR(socket_num))										// 获取socket0的状态
	{
		case SOCK_INIT:											      // Socket处于初始化完成(打开)状态
			listen(socket_num);											// 监听刚刚打开的本地端口，等待客户端连接
         break;

		case SOCK_ESTABLISHED:							      // Socket处于连接建立状态
			if(getSn_IR(socket_num) & Sn_IR_CON)			
			{
				setSn_IR(socket_num, Sn_IR_CON);				// Sn_IR的CON位置1，通知W5500连接已建立
			}
			send(socket_num, s_data, len);	            //发送数据

			break;

		case SOCK_CLOSE_WAIT:								// Socket处于等待关闭状态
			// 此状态仍可以处理收发事务       2019-02-11
			disconnect(socket_num);					// ，处理完收发后，发起断开连接命令，以满足4次挥手2019-02-11
         // Reset_W5500();
         break;

		case SOCK_CLOSED:										// Socket处于关闭状态
			socket(socket_num, Sn_MR_TCP, socket_local_port[socket_num], Sn_MR_ND);		// 打开Socket0，并配置为TCP无延时模式，打开一个本地端口  2019-02-11
			// Reset_W5500();                            //断开连接，重启，防止异常
         break;
	}
}

/*****************************************************************************************
 * 功能: 接收相应套接字的数据
 * 形参: socket_num：套接字端口	r_data：发送的缓冲
 * 返回: 无
 * 说明: 接收数据的长度
 *****************************************************************************************/
uint32_t process_w5500_socket_recv_data(uint8_t socket_num, uint8_t *r_data)
{
	int32_t r_len = -2;

	switch(getSn_SR(socket_num))										// 获取socket0的状态
	{
		case SOCK_INIT:											      // Socket处于初始化完成(打开)状态
			listen(socket_num);											// 监听刚刚打开的本地端口，等待客户端连接
			break;

		case SOCK_ESTABLISHED:							      // Socket处于连接建立状态
			if(getSn_IR(socket_num) & Sn_IR_CON)			
			{
				setSn_IR(socket_num, Sn_IR_CON);				// Sn_IR的CON位置1，通知W5500连接已建立
			}
			r_len = getSn_RX_RSR(socket_num);				// 读取W5500空闲接收缓存寄存器的值并赋给len，Sn_RX_RSR表示接收缓存中已接收和保存的数据大小
         
			if(r_len > 0)
			{
				r_len = recv(socket_num, r_data, r_len);	// W5500接收来自客户端的数据，并通过SPI发送给MCU		
			}
			break;

		case SOCK_CLOSE_WAIT:								   // Socket处于等待关闭状态
			// 此状态仍可以处理收发事务       2019-02-11
			disconnect(socket_num);							   // 处理完收发后，发起断开连接命令，以满足4次挥手2019-02-11
         // wlan_state_flag = 0;
			break;

		case SOCK_CLOSED:										// Socket处于关闭状态
			socket(socket_num, Sn_MR_TCP, socket_local_port[socket_num], Sn_MR_ND);		// 打开Socket0，并配置为TCP无延时模式，打开一个本地端口  2019-02-11
			break; 
	}

	return r_len;
}


/************************************************************
 * 功能: 设置w5500 ip
 * 形参: ip：要设置的ip指针
 * 返回: 无
 * 说明: 无 
 *************************************************************/
void w5500_set_ip(uint8_t *p_ip)
{
	uint8_t ip[4];

	ConfigMsg.lip[0] = p_ip[0];
	ConfigMsg.lip[1] = p_ip[1];
	ConfigMsg.lip[2] = p_ip[2];
	ConfigMsg.lip[3] = p_ip[3];

	//根据ip设置默认网关
	ConfigMsg.gw[0] = p_ip[0];
	ConfigMsg.gw[1] = p_ip[1];
	ConfigMsg.gw[2] = p_ip[2];
	ConfigMsg.gw[3] = 1;

	setGAR(ConfigMsg.gw);
	setSIPR(ConfigMsg.lip);

	setRTR(4000);					// 设置超时时间
	setRCR(3);

	getSIPR (ip);
	printf("IP : %d.%d.%d.%d\r\n", ip[0],ip[1],ip[2],ip[3]);
	getSUBR(ip);
	printf("SN : %d.%d.%d.%d\r\n", ip[0],ip[1],ip[2],ip[3]);
	getGAR(ip);
	printf("GW : %d.%d.%d.%d\r\n", ip[0],ip[1],ip[2],ip[3]); 
}


