#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

void Protocol_OnUart1Bytes(void);
void Protocol_OnUart2Bytes(void);
void Protocol_OnCanFrames(void);
/** 周期调用：向总线发 ZJB CAN 心跳（约 2s） */
void Protocol_CanHeartbeatPoll(void);

#ifdef __cplusplus
}
#endif

#endif /* __PROTOCOL_H */

