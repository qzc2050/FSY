#ifndef BL_OTA_H
#define BL_OTA_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 若 Set 区 OTA Flag 为 PENDING：校验 Download CRC → 擦 App → 拷贝 → 清 Flag。
 * @return 0=已成功搬运或无需搬运；-1=Flag/CRC/擦写失败（此时勿盲目跳转坏 App）
 *
 * 无 PENDING / Flag 无效时返回 0，由调用方继续校验现有 App 向量并跳转。
 */
int BL_OtaTryUpdate(void);

#ifdef __cplusplus
}
#endif

#endif /* BL_OTA_H */
