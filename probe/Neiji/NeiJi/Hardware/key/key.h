#ifndef NEIJI_KEY_H
#define NEIJI_KEY_H

#include <stdbool.h>
#include <stdint.h>

/**
 * 4-button GPIO
 *   HOME  PA4  KEY_ID_SET
 *   UP    PH7  KEY_ID_UP
 *   DOWN  PC5  KEY_ID_DOWN
 *   OK    PC4  KEY_ID_EN
 */
typedef enum {
    KEY_ID_EN = 0,
    KEY_ID_UP,
    KEY_ID_DOWN,
    KEY_ID_LEFT,   /* unused */
    KEY_ID_RIGHT,  /* unused */
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
