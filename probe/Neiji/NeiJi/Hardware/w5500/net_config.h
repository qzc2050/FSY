#ifndef NET_CONFIG_H
#define NET_CONFIG_H

/* 0 = RST 未接 W5500（仅用 SPI 软件复位）；1 = PC6 接 W5500 RST */
#ifndef W5500_RST_PIN_CONNECTED
#define W5500_RST_PIN_CONNECTED         (0)
#endif

/* 出厂默认内机型号；产线可按配置改写 Flash（RK100P / RK100D / RK100N） */
#define DEVICE_PRODUCT_MODEL            "RK100P"
#ifndef DEVICE_CFG_SN_LEN
#define DEVICE_CFG_SN_LEN               (12U)
#endif
/* 出厂默认（Flash 无有效配置时使用）
 * 产品名称 Flash/寄存器区仅 16 字节，存简称「瑞联」；
 * 关于本机界面全称见 language.c（中/英）。
 */
#define NEIJI_DEVICE_SN                 "2026FSYI0101"
#define NEIJI_PRODUCT_NAME              "\xE7\x91\x9E\xE8\x81\x94" /* 瑞联 */
#define DEVICE_SOFTWARE_VERSION         "V1.1.1.20260721E"
#define DEVICE_PROTOCOL_TYPE_CODE       (2U)
#define DEVICE_UDP_DISCOVER_RESERVED    (0U)

/* 1=周期性 [NET]/PHY link 等调试打印；长时间挂机测试请保持 0 */
#ifndef NET_STATUS_LOG
#define NET_STATUS_LOG                  (0)
#endif

#endif
