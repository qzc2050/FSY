#include "fsy_dispatch.h"

#include "fsy_crc.h"

#include "fsy_frame.h"

#include "fsy_regmap.h"

#include <stddef.h>

#include <string.h>



static uint8_t s_device_addr = FSY_DEVICE_ADDR_DEFAULT;



uint8_t Fsy_Dispatch_GetDeviceAddr(void)

{

    return s_device_addr;

}



void Fsy_Dispatch_SetDeviceAddr(uint8_t addr)

{

    if (addr == 0U) {

        addr = FSY_DEVICE_ADDR_DEFAULT;

    }

    s_device_addr = addr;

}



static uint16_t load_u16_le(const uint8_t *p)

{

    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);

}



static void store_u16_le(uint8_t *p, uint16_t v)

{

    p[0] = (uint8_t)(v & 0xFFU);

    p[1] = (uint8_t)(v >> 8);

}



static int build_error_resp(uint8_t fc_err, uint16_t reg, uint16_t err_code,

                            uint8_t *resp, uint16_t resp_cap)

{

    if (resp_cap < 8U) {

        return -1;

    }



    resp[0] = Fsy_Dispatch_GetDeviceAddr();

    resp[1] = fc_err;

    store_u16_le(&resp[2], reg);

    store_u16_le(&resp[4], err_code);

    Fsy_AppendCrc(resp, 6U);

    return 8;

}



static int read_reg_block(uint16_t start_reg, uint16_t reg_count,

                          uint8_t *out, uint16_t out_cap)

{

    return Fsy_Regmap_ReadBlock(start_reg, reg_count, out, out_cap);

}



static int handle_read_holding(const uint8_t *req, uint16_t req_len,

                               uint8_t *resp, uint16_t resp_cap)

{

    uint16_t start_reg;

    uint16_t reg_count;

    int data_len;

    uint16_t total;



    if (req_len < 8U) {

        return -1;

    }



    start_reg = load_u16_le(&req[2]);

    reg_count = load_u16_le(&req[4]);

    if (reg_count == 0U) {

        return build_error_resp(0x83U, start_reg, 0x0003U, resp, resp_cap);

    }



    total = (uint16_t)(7U + reg_count * 2U);

    if (resp_cap < total) {

        return -1;

    }



    resp[0] = Fsy_Dispatch_GetDeviceAddr();

    resp[1] = FSY_FC_READ_HOLDING_RESP;

    resp[2] = (uint8_t)(reg_count * 2U);

    store_u16_le(&resp[3], start_reg);



    data_len = read_reg_block(start_reg, reg_count, &resp[5], (uint16_t)(resp_cap - 7U));

    if (data_len < 0) {

        return build_error_resp(0x83U, start_reg, 0x0002U, resp, resp_cap);

    }



    Fsy_AppendCrc(resp, (uint16_t)(5U + (uint16_t)data_len));

    return (int)(7U + (uint16_t)data_len);

}



static int handle_write_single(const uint8_t *req, uint16_t req_len,

                               uint8_t *resp, uint16_t resp_cap)

{

    uint16_t reg;

    uint8_t data[2];



    if (req_len < 8U) {

        return -1;

    }

    if (resp_cap < 8U) {

        return -1;

    }



    reg = load_u16_le(&req[2]);

    data[0] = req[4];

    data[1] = req[5];



    if (Fsy_Regmap_WriteBlock(reg, data, 2U) != 0) {

        return build_error_resp(0x86U, reg, 0x0002U, resp, resp_cap);

    }



    resp[0] = Fsy_Dispatch_GetDeviceAddr();

    resp[1] = FSY_FC_WRITE_SINGLE_RESP;

    resp[2] = req[2];

    resp[3] = req[3];

    resp[4] = req[4];

    resp[5] = req[5];

    Fsy_AppendCrc(resp, 6U);

    return 8;

}



static int handle_write_multi(const uint8_t *req, uint16_t req_len,

                              uint8_t *resp, uint16_t resp_cap)

{

    uint16_t start_reg;

    uint16_t reg_count;

    uint8_t byte_count;



    if (req_len < 9U) {

        return -1;

    }

    if (resp_cap < 8U) {

        return -1;

    }



    start_reg = load_u16_le(&req[2]);

    reg_count = load_u16_le(&req[4]);

    byte_count = req[6];

    if ((reg_count == 0U) || (byte_count != (uint8_t)(reg_count * 2U))) {

        return build_error_resp(0x90U, start_reg, 0x0003U, resp, resp_cap);

    }

    if (req_len < (uint16_t)(9U + byte_count)) {

        return -1;

    }



    if (Fsy_Regmap_WriteBlock(start_reg, &req[7], byte_count) != 0) {

        return build_error_resp(0x90U, start_reg, 0x0002U, resp, resp_cap);

    }



    resp[0] = Fsy_Dispatch_GetDeviceAddr();

    resp[1] = FSY_FC_WRITE_MULTI_RESP;

    resp[2] = req[2];

    resp[3] = req[3];

    resp[4] = req[4];

    resp[5] = req[5];

    Fsy_AppendCrc(resp, 6U);

    return 8;

}



int Fsy_Dispatch_Request(const uint8_t *req, uint16_t req_len,

                         uint8_t *resp, uint16_t resp_cap)

{

    if ((req == NULL) || (resp == NULL) || (req_len < 4U)) {

        return -1;

    }



    if (req[0] != Fsy_Dispatch_GetDeviceAddr()) {

        return 0;

    }



    if (!Fsy_Frame_CrcOk(req, req_len) || !Fsy_Frame_FormatOk(req, req_len)) {

        return -1;

    }



    switch (req[1]) {

    case FSY_FC_READ_HOLDING_REQ:

        return handle_read_holding(req, req_len, resp, resp_cap);



    case FSY_FC_WRITE_SINGLE_REQ:

        return handle_write_single(req, req_len, resp, resp_cap);



    case FSY_FC_WRITE_MULTI_REQ:

        return handle_write_multi(req, req_len, resp, resp_cap);



    default:

        return build_error_resp((uint8_t)(req[1] | 0x80U), load_u16_le(&req[2]),

                                0x0002U, resp, resp_cap);

    }

}

