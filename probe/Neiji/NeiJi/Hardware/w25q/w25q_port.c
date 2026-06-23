#include "w25q_port.h"
#include "ext_flash_layout.h"

#include "w25qxx.h"

#include <stdio.h>
#include <string.h>

#define W25Q_EXPECTED_JEDEC_ID   0xEF4017U
#define W25Q_SELFTEST_ADDR       EXT_FLASH_SELFTEST_ADDR

int W25Q_Port_Init(void)
{
    uint32_t id;

    printf("[W25Q] probe...\r\n");

    id = W25Qx_QSPI_FLASH_ReadID();
    printf("[W25Q] raw=0x%06lX\r\n", (unsigned long)id);
    if (id != W25Q_EXPECTED_JEDEC_ID) {
        if (W25Qx_QSPI_Init() != QSPI_OK) {
            printf("[W25Q] init fail\r\n");
            return -1;
        }
        id = W25Qx_QSPI_FLASH_ReadID();
    }
    if (id == W25Q_EXPECTED_JEDEC_ID) {
        printf("[W25Q] JEDEC=0x%06lX OK\r\n", (unsigned long)id);
        return 0;
    }
    if (id == 0U) {
        printf("[W25Q] JEDEC read error\r\n");
        return -2;
    }

    printf("[W25Q] JEDEC=0x%06lX expect 0xEF4017\r\n", (unsigned long)id);
    return -3;
}

int W25Q_Port_SelfTest(void)
{
    static uint8_t pattern[] = "NeiJi-W25Q-OK!";
    uint8_t readback[sizeof(pattern)];
    uint32_t id;

    id = W25Qx_QSPI_FLASH_ReadID();
    if (id != W25Q_EXPECTED_JEDEC_ID) {
        printf("[W25Q] selftest skip (no flash)\r\n");
        return -1;
    }
    if (W25Qx_QSPI_Erase_Block(W25Q_SELFTEST_ADDR) != QSPI_OK) {
        printf("[W25Q] erase fail\r\n");
        return -1;
    }
    if (W25Qx_QSPI_Write(pattern, W25Q_SELFTEST_ADDR, (uint32_t)sizeof(pattern)) != QSPI_OK) {
        printf("[W25Q] write fail\r\n");
        return -2;
    }
    if (W25Qx_QSPI_Read(readback, W25Q_SELFTEST_ADDR, (uint32_t)sizeof(readback)) != QSPI_OK) {
        printf("[W25Q] read fail\r\n");
        return -3;
    }
    if (memcmp(pattern, readback, sizeof(pattern)) != 0) {
        printf("[W25Q] verify fail\r\n");
        return -4;
    }

    printf("[W25Q] selftest PASS @0x%06lX\r\n", (unsigned long)W25Q_SELFTEST_ADDR);
    return 0;
}
