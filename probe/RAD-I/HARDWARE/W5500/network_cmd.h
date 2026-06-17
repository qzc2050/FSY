#ifndef _NETWORK_CMD_H_
#define _NETWORK_CMD_H_

#include "main.h"
#include "stdbool.h"

#ifdef _NETWORK_CMD_C_
#define _NETWORK_CMD_C_EXT_
#define _NETWORK_CMD_C_EXT_INT_ //
#else
#define _NETWORK_CMD_C_EXT_  extern
#define _NETWORK_CMD_C_EXT_INT_ extern
#endif


#define SOFTWARE_VER (10)


#define NETWORK_CONNECTION 	  (1)	// 网络已连？
#define NETWORK_DISCONNECTION (0)	// 网络已断开

/* ==================== 网络配置宏定义 ==================== */
/* DHCP 开关：1=动态 IP, 0=静态 IP 
 * 已集成 WIZnet 官方 DHCP 库，支持真正的 DHCP 功能
 */
#ifndef USE_DHCP
#define USE_DHCP    (1)    // 默认启用 DHCP
#endif

/* 默认 IP 地址（DHCP 获取前使用） */
#ifndef DEFAULT_IP_ADDR
#define DEFAULT_IP_ADDR     {0, 0, 0, 0}
#endif
#ifndef DEFAULT_SUBNET_MASK
#define DEFAULT_SUBNET_MASK {255, 255, 255, 0}
#endif
#ifndef DEFAULT_GATEWAY
#define DEFAULT_GATEWAY     {192, 168, 2, 1}
#endif

extern uint8_t collect_data_printf_flag;

extern int32_t cur_encoder_cnt;
extern int32_t y_home_step;

//数据上传套接字编�?
#define DATA_UPLOAD_SOCKET_NUM (0)
//上位机命令�?�接字编�?
#define SETTING_SOCKET_NUM (1)
//调试打印套接字编�?
#define DEBUG_SOCKET_NUM (2)

//端口号（可设为相同或不同）
// 异口：5000=数据发送(→上位机数据口)，5001=控制接收(←上位机控制口)
// 同口：只监听 SETTING_SOCKET，兼做数据发送与控制接收
#define DATA_UPLOAD_SOCKET_PORT 5001  /* W5500 Socket0：数据发送 */
#define SETTING_SOCKET_PORT     5001  /* W5500 Socket1：控制接收 */



#pragma pack(1)

struct network_send_cmd__
{
	uint8_t buf[3600];
	uint16_t len;
};

#pragma pack()    //取消�??定义字节对齐方�??

enum net_handle_task_event_table_
{
	NET_HANDLE_EVENT = 0,
};


enum net_mode_update_task_event_table_
{
	NET_MODE_UPDATE_EVENT = 0,
};




//网口打印字�?��??
#define NETWORK_PRINTF_STR(x) network_send_data(DEBUG_SOCKET_NUM, x, strlen(x));

_NETWORK_CMD_C_EXT_ struct network_send_cmd__ network_send_cmd;
_NETWORK_CMD_C_EXT_ uint16_t upload_sync_data_timer;
_NETWORK_CMD_C_EXT_ uint8_t sys_rst_state;

_NETWORK_CMD_C_EXT_ uint8_t process_socket1_cmd(uint8_t *p_data, uint32_t len);


_NETWORK_CMD_C_EXT_ void net_handle_task(void);
_NETWORK_CMD_C_EXT_ void net_mode_update_task(void);
_NETWORK_CMD_C_EXT_ void updata_communication_mode(void);
_NETWORK_CMD_C_EXT_ void init_communication_mode(void);


_NETWORK_CMD_C_EXT_ void network_ack_return_origin_api(uint8_t p_result);


_NETWORK_CMD_C_EXT_ void network_send_data(uint8_t link_no, uint8_t *p_data, uint32_t p_len);
_NETWORK_CMD_C_EXT_ void network_ack_return_bg_check(uint8_t p_result, float p_Q1, float p_I1, float p_Q2, float p_I2);
_NETWORK_CMD_C_EXT_ void network_ack_return_sync_measured_api(uint8_t p_result);
_NETWORK_CMD_C_EXT_ void network_ack_return_set_sampling_period_api(uint8_t p_result);
_NETWORK_CMD_C_EXT_ void network_ack_return_water_surface_detection(uint8_t p_result,
												int x1, int y1, int z1,
												int x2, int y2, int z2,
												int x3, int y3, int z3,
												int x4, int y4, int z4);
_NETWORK_CMD_C_EXT_ void network_ack_return_set_hv_api(uint8_t p_result);
_NETWORK_CMD_C_EXT_ void network_ack_return_get_master_info_api(void);
_NETWORK_CMD_C_EXT_ void network_ack_set_ip_api(uint8_t p_result);
_NETWORK_CMD_C_EXT_ void network_ack_seve_central_point(uint8_t p_result);
_NETWORK_CMD_C_EXT_ void network_ack_set_ionization_state(uint8_t p_result);
_NETWORK_CMD_C_EXT_ void network_ack_sys_info(void);
_NETWORK_CMD_C_EXT_ void upload_sync_data(void);

_NETWORK_CMD_C_EXT_ uint8_t socket1_add_to_queue_send(uint8_t *buf, uint16_t len);
_NETWORK_CMD_C_EXT_ void socket1_poll_send_data_task(void);
_NETWORK_CMD_C_EXT_ void updata_network_state(void);

_NETWORK_CMD_C_EXT_ void network_ack_return_temp_api(uint8_t p_result);
_NETWORK_CMD_C_EXT_ void network_ack_return_sync_measured_rc_api(uint8_t p_result);
_NETWORK_CMD_C_EXT_ void network_ack_seve_central_point_rc(uint8_t p_result);
_NETWORK_CMD_C_EXT_ void network_ack_return_origin_rc_api(uint8_t p_result);

void tcp_net_process(void);
void int_wlan_data_wirte (void);
void int_wlan_data_read (void);

#endif



