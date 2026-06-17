#define _NETWORK_CMD_C_

#include "network_cmd.h"
#include "socket.h"
#include "string.h"
#include "stdio.h"


uint8_t Rx_Buffer[4096];
uint8_t Rx_Cache[4096];
uint16_t Rx_Cache_Len = 0;


/************************************************************
 * 功能: 网口发送数???
 * 形参:
 * 返回: ???
 * 说明: ???
 *************************************************************/
void network_send_data(uint8_t link_no, uint8_t *p_data, uint32_t p_len)
{
	w5500_socket_send_data(link_no, p_data, p_len);
}


void tcp_net_process(void)
{
	int r_len = 0;
	uint32_t i;
//	uint8_t flag = 0;

	r_len = process_w5500_socket_recv_data(DATA_UPLOAD_SOCKET_NUM, Rx_Buffer); // 5000端口
	if (r_len > 0)
	{
		printf("\r\nsocket %u recv len = %d:", DATA_UPLOAD_SOCKET_NUM, r_len);
		for (i = 0; i < r_len; i++)
		{
			printf("0x%02x ", Rx_Buffer[i]);
		}
		printf("网络端口:%u\r\n", DATA_UPLOAD_SOCKET_PORT);
		/* 按字节清零 - 避免对齐问题 */
		for(uint16_t i = 0; i < sizeof(Rx_Buffer); i++)
		{
			Rx_Buffer[i] = 0;
		}

	}
	r_len = process_w5500_socket_recv_data(SETTING_SOCKET_NUM, Rx_Buffer); // 5001端口
	if (r_len > 0)
	{
		printf("网络端口:5001\r\n");
		printf("接收到的数据");
		printf("r_len = %d  Rx_Cache_Len = %d\r\n", r_len,Rx_Cache_Len);
		for (i = 0; i < r_len; i++)
		{
			printf("   %02X",Rx_Buffer[i]);
		}
		printf("\r\n");
	}
	r_len = process_w5500_socket_recv_data(DEBUG_SOCKET_NUM, Rx_Buffer); // 5002端口
	// if (r_len > 0)
	// {
	// 	// printf("网络端口:5002\r\n");
	// 	// net_resolve_set_cmd(Rx_Buffer, r_len);
	// }
	if (r_len > 0)
	{
		printf("\r\nsocket %u recv len = %d:", DEBUG_SOCKET_NUM, r_len);
		for (i = 0; i < r_len; i++)
		{
			printf("0x%02x ", Rx_Buffer[i]);
		}
		printf("\r\n");
	}
}

/*************************************************************
 功能：网口数据处理任???
 形参???
 返回???
 详解???
 编写???
**************************************************************/
//void net_handle_task(void)
//{
//	switch (task_table[NET_HANDLE_TASK].event)
//	{
//	case NET_HANDLE_EVENT:
//		tcp_net_process();
//		break;
//	default:
//		break;
//	}
//}

/*************************************************************3
 功能：socket1轮询发送数???
 形参???
 返回???
 详解???
 编写???
**************************************************************/
// void socket1_poll_send_data_task(void)
// {
// 	struct si_queue_data_ *p_recv_data = NULL;
// 	uint8_t i = 0, j = 0;
// 	uint8_t *p;
// 	if(esp32_flag == 0)      //Wlan 模式
// 	{
// 		if (!si_queue_is_empty(&socket1_send_data_queue)) // 判断队列是否为空
// 		{
// 			p_recv_data = si_queue_pop(&socket1_send_data_queue); // 出队
// 			if (p_recv_data -> size != 0)
// 			{
// 				// printf("\r\nrx len = %d \r\n", p_recv_data->size);
// 				// for(i = 0; i < p_recv_data->size; i++)
// 				//{
// 				//	printf("%02x ",p_recv_data->buff[i]);
// 				// }
// 				network_send_data(SETTING_SOCKET_NUM, p_recv_data->buff, p_recv_data->size);
// 				p_recv_data -> size = 0; // 标记为改缓冲可用
// 			}
// 		}
// 	}
// 	else					 //Wifi  模式
// 	{
// 		if (!si_queue_is_empty(&esp32_recv_data_queue)) // 判断队列是否为空
// 		{
// 			p_recv_data = si_queue_pop(&esp32_recv_data_queue); // 出队
// 			if (p_recv_data -> size != 0)
// 			{
// 				network_send_data(SETTING_SOCKET_NUM, p_recv_data->buff, p_recv_data->size);
// 				p_recv_data -> size = 0; // 标记为改缓冲可用
// 			}
// 		}
// 	}
// }

