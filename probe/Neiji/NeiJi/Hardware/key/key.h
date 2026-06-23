#ifndef NEIJI_KEY_H
#define NEIJI_KEY_H

#include <stdbool.h>
#include <stdint.h>

/**
 * 同板 RAD-I 按键硬件（电阻梯 + 独立 SET）：
 *   EN/按压  PA4  ADC2_INP18
 *   上/下    PC4  ADC2_INP4
 *   左/右    PC5  ADC2_INP8
 *   SET      PH7  GPIO 输入（低电平有效）
 */
typedef enum {
    KEY_ID_EN = 0,
    KEY_ID_UP,
    KEY_ID_DOWN,
    KEY_ID_LEFT,
    KEY_ID_RIGHT,
    KEY_ID_SET,
    KEY_ID_MAX
} KEY_ID_t;

void KEY_Init(void);
void KEY_Scan(void);

bool KEY_IsActive(KEY_ID_t id);
bool KEY_GetPressEvent(KEY_ID_t *id);
int32_t KEY_GetPressAdcDiff(void);
KEY_ID_t KEY_GetActiveKey(void);

const char *KEY_Name(KEY_ID_t id);

#endif /* NEIJI_KEY_H */
