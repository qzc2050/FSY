#ifndef FSY_UPLOAD_H
#define FSY_UPLOAD_H

#include <stdint.h>

#ifndef FSY_UPLOAD_PERIOD_MS
#define FSY_UPLOAD_PERIOD_MS 1000U
#endif

#ifndef FSY_UPLOAD_SN_PERIOD_MS
#define FSY_UPLOAD_SN_PERIOD_MS 3000U
#endif

/** 同台 LoRa/CAN 多机时，按协议地址在 [0, period) 内均分相位；0=关闭 */
#ifndef FSY_UPLOAD_PHASE_ENABLE
#define FSY_UPLOAD_PHASE_ENABLE  1
#endif

#ifndef FSY_UPLOAD_PHASE_SLOTS
#define FSY_UPLOAD_PHASE_SLOTS   4U
#endif

#define FSY_SN_UPLOAD_PAYLOAD_BYTES  16U
#define FSY_SN_UPLOAD_FRAME_LEN      (5U + FSY_SN_UPLOAD_PAYLOAD_BYTES + 2U)

void Fsy_Upload_Init(void);

int Fsy_Upload_BuildFrame(uint8_t *frame, uint16_t frame_cap);

int Fsy_Upload_Send(int (*write_fn)(const uint8_t *data, uint16_t len));

/** 0x23 主动广播序列号：start reg 86 (0x0056)，16 字节 ASCII */
int Fsy_Upload_BuildSerialFrame(uint8_t *frame, uint16_t frame_cap);

int Fsy_Upload_SendSerial(int (*write_fn)(const uint8_t *data, uint16_t len));

/** 0x23 / SN 上报启动前延迟（ms），由 dev_addr 与 FSY_UPLOAD_PHASE_SLOTS 决定 */
uint32_t Fsy_Upload_PhaseOffsetMs(void);

/** 0x23 五分钟实时窗：start=0x001E，12B = data_time[8] + D5×100 */
int Fsy_Upload_Build5MinFrame(uint8_t *frame, uint16_t frame_cap,
                              const uint8_t dt8[8], uint32_t dose_x100);

int Fsy_Upload_Send5Min(const uint8_t dt8[8], uint32_t dose_x100,
                        int (*write_fn)(const uint8_t *data, uint16_t len));

#endif
