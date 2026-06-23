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


#ifndef	_SOCKET_H_
#define	_SOCKET_H_

#include "main.h"
#include "device.h"
#include "w5500.h"


extern uint8_t socket(SOCKET s, uint8_t protocol, uint16_t port, uint8_t flag); // Opens a socket(TCP or UDP or IP_RAW mode)
extern void close(SOCKET s); // Close socket
extern uint8_t connect(SOCKET s, uint8_t * addr, uint16_t port); // Establish TCP connection (Active connection)
extern void disconnect(SOCKET s); // disconnect the connection
extern uint8_t listen(SOCKET s);	// Establish TCP connection (Passive connection)
extern uint16_t send(SOCKET s, const uint8_t * buf, uint16_t len); // Send data (TCP)
extern uint16_t recv(SOCKET s, uint8_t * buf, uint16_t len);	// Receive data (TCP)
extern uint16_t sendto(SOCKET s, const uint8_t * buf, uint16_t len, uint8_t * addr, uint16_t port); // Send data (UDP/IP RAW)
extern uint16_t recvfrom(SOCKET s, uint8_t * buf, uint16_t len, uint8_t * addr, uint16_t  *port); // Receive data (UDP/IP RAW)

extern void w5500_socket_send_data(uint8_t socket_num, uint8_t *s_data, uint32_t len);
extern uint32_t process_w5500_socket_recv_data(uint8_t socket_num, uint8_t *r_data);

extern void w5500_set_ip(uint8_t *p_ip);

#ifdef __MACRAW__
void macraw_open(void);
uint16_t macraw_send( const uint8_t * buf, uint16_t len ); //Send data (MACRAW)
uint16_t macraw_recv( uint8_t * buf, uint16_t len ); //Recv data (MACRAW)
#endif

#endif
/* _SOCKET_H_ */

