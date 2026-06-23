# -*- coding: utf-8 -*-
"""Modbus RTU CRC16，与 NeiJi Protocol/fsy_crc.c 一致。"""


def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def append_crc(data: bytes) -> bytes:
    c = crc16_modbus(data)
    return data + bytes([c & 0xFF, (c >> 8) & 0xFF])


def verify_crc(frame: bytes) -> bool:
    if len(frame) < 3:
        return False
    rx = frame[-2] | (frame[-1] << 8)
    return rx == crc16_modbus(frame[:-2])
