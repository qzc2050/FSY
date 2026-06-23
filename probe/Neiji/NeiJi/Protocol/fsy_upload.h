#ifndef FSY_UPLOAD_H
#define FSY_UPLOAD_H

#include <stdint.h>

#ifndef FSY_UPLOAD_PERIOD_MS
#define FSY_UPLOAD_PERIOD_MS 1000U
#endif

void Fsy_Upload_Init(void);

int Fsy_Upload_BuildFrame(uint8_t *frame, uint16_t frame_cap);

int Fsy_Upload_Send(int (*write_fn)(const uint8_t *data, uint16_t len));

#endif
