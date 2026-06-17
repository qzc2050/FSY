#ifndef __FAN_H
#define __FAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void Fan_Set(uint8_t on);
uint8_t Fan_Get(void);

#ifdef __cplusplus
}
#endif

#endif /* __FAN_H */

