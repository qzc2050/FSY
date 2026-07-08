#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

void Protocol_OnUart1Bytes(void);
void Protocol_OnUart2Bytes(void);
void Protocol_OnCanFrames(void);

#ifdef __cplusplus
}
#endif

#endif /* __PROTOCOL_H */

