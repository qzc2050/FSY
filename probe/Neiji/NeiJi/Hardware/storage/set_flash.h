#ifndef SET_FLASH_H
#define SET_FLASH_H

#include <stdint.h>

int SetFlash_Read(uint32_t offset, void *buf, uint32_t len);

int SetFlash_WriteRegion(uint32_t offset, const void *data, uint32_t len);

/** 一次擦写更新多个区域（共享同一次扇区擦除）。regions 为 offset 数组，最后一项须为 0xFFFFFFFF。 */
int SetFlash_WriteRegions(const uint32_t *offsets,
                          const void *const *datas,
                          const uint32_t *lens,
                          uint32_t count);

uint32_t SetFlash_GetLastError(void);

#endif
