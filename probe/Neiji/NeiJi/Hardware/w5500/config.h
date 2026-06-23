#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "types.h"

#define MAX_BUF_SIZE          1460U
#define NORMAL_STATE          0U

typedef struct __attribute__((packed)) _CONFIG_MSG
{
    uint8 op[4];
    uint8 mac[6];
    uint8 sw_ver[2];
    uint8 lip[4];
    uint8 sub[4];
    uint8 gw[4];
    uint8 dns[4];
    uint8 dhcp;
    uint8 debug;
    uint16 fw_len;
    uint8 state;
} CONFIG_MSG;

extern CONFIG_MSG ConfigMsg;

#endif
