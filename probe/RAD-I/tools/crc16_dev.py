# -*- coding: utf-8 -*-
"""
与 dev_protocol/net_raw/net_raw_protocol.c 中 Net_Raw_CRC16_Calc 一致：
标准 Modbus RTU CRC16（多项式 0xA001，初值 0xFFFF）。

注意：与 core/dev_protocol.c 中 Dev_Calculate_CRC（0x8005）不同，后者仍用于 SPI 等其它模块。
"""


def dev_calculate_crc(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def dev_append_crc(data: bytes) -> bytes:
    c = dev_calculate_crc(data)
    return data + bytes([c & 0xFF, (c >> 8) & 0xFF])


def dev_verify_crc(frame: bytes) -> bool:
    if len(frame) < 3:
        return False
    rx = frame[-2] | (frame[-1] << 8)
    cc = dev_calculate_crc(frame[:-2])
    return rx == cc
