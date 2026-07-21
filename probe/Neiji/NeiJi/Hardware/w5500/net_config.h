#ifndef NET_CONFIG_H
#define NET_CONFIG_H

/* 0 = RST 未接 W5500（仅用 SPI 软件复位）；1 = PC6 接 W5500 RST */
#ifndef W5500_RST_PIN_CONNECTED
#define W5500_RST_PIN_CONNECTED         (0)
#endif

#define DEVICE_PRODUCT_MODEL            "FSY-I"
#ifndef DEVICE_CFG_SN_LEN
#define DEVICE_CFG_SN_LEN               (12U)
#endif
/* 出厂默认（Flash 无有效配置时使用） */
#define NEIJI_DEVICE_SN                 "2026FSYI0101"
#define NEIJI_PRODUCT_NAME              "\xE9\x9B\xB7\xE6\xB2\x83-\xE6\x8E\xA2\xE6\xB5\x8B\xE5\x99\xA8"
#define DEVICE_SOFTWARE_VERSION         "V1.1.1.20260721C"
#define DEVICE_PROTOCOL_TYPE_CODE       (2U)
#define DEVICE_UDP_DISCOVER_RESERVED    (0U)

/* 1=周期性 [NET]/PHY link 等调试打印；长时间挂机测试请保持 0 */
#ifndef NET_STATUS_LOG
#define NET_STATUS_LOG                  (0)
#endif

#endif
