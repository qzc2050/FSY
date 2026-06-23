#ifndef FSY_DISPATCH_H

#define FSY_DISPATCH_H



#include <stdint.h>



#ifndef FSY_DEVICE_ADDR_DEFAULT

#define FSY_DEVICE_ADDR_DEFAULT   0x01U

#endif



uint8_t Fsy_Dispatch_GetDeviceAddr(void);

void Fsy_Dispatch_SetDeviceAddr(uint8_t addr);



int Fsy_Dispatch_Request(const uint8_t *req, uint16_t req_len,

                         uint8_t *resp, uint16_t resp_cap);



#endif

