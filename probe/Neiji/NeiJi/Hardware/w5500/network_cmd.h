#ifndef _NETWORK_CMD_H_
#define _NETWORK_CMD_H_

#include "main.h"

#ifndef USE_DHCP
#define USE_DHCP                (1)
#endif

#ifndef DEFAULT_IP_ADDR
#define DEFAULT_IP_ADDR         {0, 0, 0, 0}
#endif
#ifndef DEFAULT_SUBNET_MASK
#define DEFAULT_SUBNET_MASK     {255, 255, 255, 0}
#endif
#ifndef DEFAULT_GATEWAY
#define DEFAULT_GATEWAY         {192, 168, 2, 1}
#endif

#define DATA_UPLOAD_SOCKET_NUM  (0U)
#define SETTING_SOCKET_NUM      (1U)
#define DATA_UPLOAD_SOCKET_PORT (5001U)
#define SETTING_SOCKET_PORT     (5001U)

#endif
