#ifndef NET_CONFIG_H
#define NET_CONFIG_H

/* 0 = RST 未接 W5500（仅用 SPI 软件复位）；1 = PC6 接 W5500 RST */
#ifndef W5500_RST_PIN_CONNECTED
#define W5500_RST_PIN_CONNECTED         (0)
#endif

#define DEVICE_PRODUCT_MODEL            "FSY-I"
#define DEVICE_CFG_SN_LEN               (12U)
/* 出厂默认（Flash 无有效配置时使用） */
#define NEIJI_DEVICE_SN                 "2026FSYI0101"
#define DEVICE_PROTOCOL_TYPE_CODE       (2U)
#define DEVICE_UDP_DISCOVER_RESERVED    (0U)

#endif
