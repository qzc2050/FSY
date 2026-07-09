#ifndef HIST_5MIN_QUERY_H
#define HIST_5MIN_QUERY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 主机写满 reg108+112 后调用，启动按时间区间回传
 * @param start_dt8 / end_dt8 协议 8B 时间
 */
void Hist5Min_Query_Start(const uint8_t start_dt8[8], const uint8_t end_dt8[8]);

/** 在 upload 任务中周期调用：逐条发 0x23 start=0x0024 */
void Hist5Min_Query_Pump(void);

#ifdef __cplusplus
}
#endif

#endif /* HIST_5MIN_QUERY_H */
