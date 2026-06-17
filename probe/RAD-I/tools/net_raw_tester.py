#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import os
import sys
import re
import subprocess
import serial
import serial.tools.list_ports
import struct
import time
import threading
import socket
import queue
import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
from datetime import datetime
from typing import Optional, List, Tuple, Callable

_tools_dir = os.path.dirname(os.path.abspath(__file__))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)
try:
    import net_raw_pack_support as _nps
    _nps.early_init()
except ImportError:
    pass

from serial_link import (
    SerialConnectionService,
    format_port_combo_values,
    normalize_port,
    _set_serial_idle_modem_lines,
)
from crc16_dev import dev_append_crc, dev_calculate_crc, dev_verify_crc


# 窗口标题
APP_TITLE = "RWD-I - 上位机 - V1.0.20260603"

# TCP 默认端口（与 network_cmd.h 一致；两端口相同时为单连接收发）
DEFAULT_TCP_CTRL_PORT = 5001
DEFAULT_TCP_DATA_PORT = 5001
LOCAL_IP_CACHE_TTL = 5.0  # 本地 IP 缓存秒数，避免频繁 ipconfig 卡顿
_SUBPROCESS_NO_WINDOW = getattr(subprocess, "CREATE_NO_WINDOW", 0x08000000) if sys.platform == "win32" else 0


def _subprocess_check_output(cmd, **kwargs):
    """运行子进程并捕获输出；Windows 下隐藏控制台，避免扫描时闪黑框"""
    if sys.platform == "win32":
        kwargs.setdefault("creationflags", _SUBPROCESS_NO_WINDOW)
    return subprocess.check_output(cmd, **kwargs)


def _set_tcp_keepalive(sock: socket.socket, idle_ms: int = 10000, interval_ms: int = 3000) -> None:
    """开启 TCP keepalive，便于检测对端异常断开（如单片机重启）"""
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        if hasattr(socket, 'SIO_KEEPALIVE_VALS'):
            sock.ioctl(socket.SIO_KEEPALIVE_VALS, (1, idle_ms, interval_ms))
        elif hasattr(socket, 'TCP_KEEPIDLE'):
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE, max(1, idle_ms // 1000))
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, max(1, interval_ms // 1000))
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPCNT, 3)
    except OSError:
        pass


class CRC16:
    """CRC16：与单片机 Net_Raw_CRC16_Calc 一致（标准 Modbus RTU / 0xA001）"""

    calculate = staticmethod(dev_calculate_crc)

    @staticmethod
    def append(data: bytes) -> bytes:
        return dev_append_crc(data)

    verify = staticmethod(dev_verify_crc)


class NetRawProtocol:
    """Net Raw 协议类"""
    
    # 功能码
    FC_WRITE_SINGLE_REQ = 0x06
    FC_WRITE_SINGLE_ACK = 0x16
    FC_WRITE_SINGLE_ERR = 0x86
    FC_WRITE_MULTI_REQ = 0x10
    FC_WRITE_MULTI_ACK = 0x20
    FC_WRITE_MULTI_ERR = 0x90
    FC_READ_SINGLE_REQ = 0x05
    FC_READ_SINGLE_ACK = 0x15
    FC_READ_SINGLE_ACTIVE = 0x25
    FC_READ_SINGLE_ERR = 0x85
    FC_READ_MULTI_REQ = 0x03
    FC_READ_MULTI_ACK = 0x13
    FC_READ_MULTI_ACTIVE = 0x23
    FC_READ_MULTI_ERR = 0x83
    
    # 寄存器地址（只读 - 传感器数据）
    REG_DOSE_RATE = 0x0001
    REG_TEMP = 0x0003
    REG_PRESS = 0x0005
    REG_HUM = 0x0007
    REG_CO2 = 0x0009
    REG_PM25 = 0x000B
    REG_ALARM_BIT = 0x000D
    REG_STATUS_BIT = 0x000F
    
    # 寄存器地址（读写 - 配置参数）
    REG_THR_DOSE_HI = 0x0032
    REG_THR_DOSE_LO = 0x0034
    REG_THR_TEMP_HI = 0x0036
    REG_THR_TEMP_LO = 0x0038
    REG_THR_PRESS_HI = 0x003A
    REG_THR_PRESS_LO = 0x003C
    REG_THR_HUM_HI = 0x003E
    REG_THR_HUM_LO = 0x0040
    REG_THR_CO2_HI = 0x0042
    REG_THR_CO2_LO = 0x0044
    REG_THR_PM25_HI = 0x0046
    REG_THR_PM25_LO = 0x0048
    REG_ALARM_BITEN = 0x0052
    REG_ALARM_BITEN_DOSE_HI_BIT = 0
    REG_ALARM_BITEN_DOSE_LO_BIT = 1
    REG_CTRL2_SOUND_BIT = 0
    REG_CTRL2_LIGHT_BIT = 1
    REG_CTRL2_DISPLAY_BIT = 2
    REG_SERIALNUM = 0x0056
    REG_SW_VERSION = 0x0062
    REG_REBOOT = 0x0078
    REG_DEV_ADDR = 0x0079
    REG_ALARM_VOLUME = 0x007A
    REG_CONTROL_BIT2 = 0x007B
    
    # OTA 寄存器地址
    REG_OTA_FILE_SIZE = 0x00C8    # 200 - OTA 文件大小
    REG_OTA_CRC32 = 0x00CA        # 202 - OTA 文件 CRC32（结束指令）
    REG_OTA_STATE = 0x00CC        # 204 - OTA 状态 + 已写入字节数
    REG_OTA_DATA = 0x00D0         # 208 - OTA 数据寄存器
    
    # 5 分钟记录寄存器（主动上传 0x23，起始地址 30，共 6 寄存器 / 12 字节）
    REG_DATA_TIME = 0x001E        # 30 - 时间戳（年月日时分秒，4 寄存器）
    REG_DOSE_5MIN = 0x0022        # 34 - 5 分钟累计剂量（2 寄存器）
    REG_DATA_TIME_CFG = 0x005E    # 94 - 系统时间同步（4 寄存器 / 8 字节）
    REG_DATA_TIME_START = 0x006C  # 108 - 历史查询开始时间（4 寄存器 / 8 字节）
    REG_DATA_TIME_END = 0x0070    # 112 - 历史查询结束时间（4 寄存器 / 8 字节）
    DATA_5_MIN_LABEL = "5min"
    
    # 串口指令
    UART_CMD_PREFIX = 0xAA
    UART_CMD_SEND_RAW = 0x01
    UART_CMD_SEND_REG = 0x02
    UART_CMD_SEND_CTRL = 0x03
    UART_FRAME_END = 0x55
    
    @staticmethod
    def build_read_single(addr: int, reg: int, qty: int = 1) -> bytes:
        """构建读单寄存器指令（功能码为单字节 0x05）；寄存器地址/数量为小端序（低位在前）"""
        frame = struct.pack('<BB', addr, NetRawProtocol.FC_READ_SINGLE_REQ) + struct.pack('<HH', reg, qty)
        return CRC16.append(frame)

    @staticmethod
    def build_read_multi(addr: int, reg: int, qty: int) -> bytes:
        """构建读多寄存器指令（功能码为单字节 0x03）；寄存器地址/数量为小端序（低位在前）"""
        frame = struct.pack('<BB', addr, NetRawProtocol.FC_READ_MULTI_REQ) + struct.pack('<HH', reg, qty)
        return CRC16.append(frame)

    @staticmethod
    def build_write_single(addr: int, reg: int, val: int) -> bytes:
        """构建写单寄存器指令（功能码为单字节 0x06）；寄存器地址/值为小端序（低位在前）"""
        frame = struct.pack('<BB', addr, NetRawProtocol.FC_WRITE_SINGLE_REQ) + struct.pack('<HH', reg, val)
        return CRC16.append(frame)

    @staticmethod
    def build_write_multi(addr: int, reg: int, values: List[int]) -> bytes:
        """构建写多寄存器指令 0x10；寄存器地址/数量/值均为小端序（低位在前）"""
        reg_qty = len(values)
        payload = b''.join(struct.pack('<H', v & 0xFFFF) for v in values)
        byte_count = len(payload)
        frame = struct.pack('<BB', addr, NetRawProtocol.FC_WRITE_MULTI_REQ) + struct.pack('<HH', reg, reg_qty)
        frame += bytes([byte_count])
        frame += payload
        return CRC16.append(frame)

    @staticmethod
    def pack_hist_query_time_regs(dt: datetime) -> List[int]:
        """
        历史查询时间（108/112）：8 字节 [年%100,月,日,时,分,秒,0,0] -> 4 寄存器（小端）
        与固件 net_reg_write_bytes / Net_Sync_5MinRecord 一致
        """
        b = (
            dt.year % 100,
            dt.month,
            dt.day,
            dt.hour,
            dt.minute,
            dt.second,
            0,
            0,
        )
        return [
            b[0] | (b[1] << 8),
            b[2] | (b[3] << 8),
            b[4] | (b[5] << 8),
            b[6] | (b[7] << 8),
        ]

    @staticmethod
    def build_time_sync_frame(addr: int, dt: datetime = None) -> bytes:
        """写寄存器 94：同步 PC 当前时间到设备 RTC（4 寄存器 / 8 字节）"""
        if dt is None:
            dt = datetime.now()
        return NetRawProtocol.build_write_multi(
            addr, NetRawProtocol.REG_DATA_TIME_CFG,
            NetRawProtocol.pack_hist_query_time_regs(dt),
        )

    @staticmethod
    def build_hist_query_frames(addr: int, start: datetime, end: datetime) -> Tuple[bytes, bytes]:
        """写 108 起始时间 + 112 结束时间（第二次写入后设备开始上传历史 5 分钟记录）"""
        f_start = NetRawProtocol.build_write_multi(
            addr, NetRawProtocol.REG_DATA_TIME_START,
            NetRawProtocol.pack_hist_query_time_regs(start),
        )
        f_end = NetRawProtocol.build_write_multi(
            addr, NetRawProtocol.REG_DATA_TIME_END,
            NetRawProtocol.pack_hist_query_time_regs(end),
        )
        return f_start, f_end

    @staticmethod
    def build_write_multi_ack(addr: int, reg: int, reg_qty: int) -> bytes:
        """构建写多应答 0x20（确认 0x23 主动上传）"""
        frame = struct.pack('<BB', addr, NetRawProtocol.FC_WRITE_MULTI_ACK) + struct.pack('<HH', reg, reg_qty)
        return CRC16.append(frame)

    @staticmethod
    def build_write_single_ack(addr: int, reg: int, val: int) -> bytes:
        """构建写单应答 0x16（确认 0x25 主动上传）"""
        frame = struct.pack('<BB', addr, NetRawProtocol.FC_WRITE_SINGLE_ACK) + struct.pack('<HH', reg, val)
        return CRC16.append(frame)

    @staticmethod
    def build_active_upload_ack(fi: dict) -> Optional[bytes]:
        """
        对从机主动上传回写应答（与 Net_IsRespFcMatch 一致）：
        - 0x23 -> 0x20（起始寄存器 + 寄存器个数）
        - 0x25 -> 0x16（寄存器地址 + 寄存器值）
        """
        if not fi:
            return None

        func = fi.get('func', 0)
        addr = fi.get('addr', 1)
        byte_count = fi.get('byte_count', 0)
        reg_addr = fi.get('reg_addr', 0)

        if func == NetRawProtocol.FC_READ_MULTI_ACTIVE:
            if byte_count == 0 or (byte_count % 2) != 0:
                return None
            reg_qty = byte_count // 2
            return NetRawProtocol.build_write_multi_ack(addr, reg_addr, reg_qty)

        if func == NetRawProtocol.FC_READ_SINGLE_ACTIVE:
            payload = fi.get('payload', b'')
            if len(payload) < 2:
                return None
            reg_val = payload[0] | (payload[1] << 8)
            return NetRawProtocol.build_write_single_ack(addr, reg_addr, reg_val)

        return None

    @staticmethod
    def build_active_upload_ack_from_frame(frame: bytes) -> Optional[bytes]:
        """从已校验 CRC 的原始帧快速组 ACK，避免完整 parse 阻塞读线程"""
        if len(frame) < 4:
            return None

        addr = frame[0]
        func = frame[1]

        if func == NetRawProtocol.FC_READ_MULTI_ACTIVE:
            if len(frame) < 7:
                return None
            byte_count = frame[2]
            if byte_count == 0 or (byte_count % 2) != 0:
                return None
            reg = frame[3] | (frame[4] << 8)
            reg_qty = byte_count // 2
            return NetRawProtocol.build_write_multi_ack(addr, reg, reg_qty)

        if func == NetRawProtocol.FC_READ_SINGLE_ACTIVE:
            if len(frame) < 8:
                return None
            reg = frame[2] | (frame[3] << 8)
            val = frame[4] | (frame[5] << 8)
            return NetRawProtocol.build_write_single_ack(addr, reg, val)

        return None
    
    @staticmethod
    def build_modbus_ack_from_request(frame: bytes) -> Optional[bytes]:
        """
        根据 Modbus 请求指令构建应答帧（上位机作为从机）
        支持所有标准 Modbus 功能码的应答
        """
        if len(frame) < 4:
            return None
        
        addr = frame[0]
        func = frame[1]
        
        # 读多寄存器请求 0x03 -> 读多应答 0x13
        if func == NetRawProtocol.FC_READ_MULTI_REQ:
            if len(frame) < 8:
                return None
            # 解析请求：起始地址 + 寄存器数量
            start_addr = frame[2] | (frame[3] << 8)
            reg_qty = frame[4] | (frame[5] << 8)
            # 构建应答：功能码 + 字节数 + 数据（这里返回最小应答，实际数据需要上位机填充）
            # 注意：实际使用时需要根据寄存器表填充真实数据
            byte_count = reg_qty * 2
            ack_frame = struct.pack('<BB', addr, NetRawProtocol.FC_READ_MULTI_ACK)
            ack_frame += struct.pack('<B', byte_count)
            # 填充 0x00 作为示例数据（实际应由寄存器表提供）
            ack_frame += b'\x00' * byte_count
            return CRC16.append(ack_frame)
        
        # 读单寄存器请求 0x05 -> 读单应答 0x15
        elif func == NetRawProtocol.FC_READ_SINGLE_REQ:
            if len(frame) < 8:
                return None
            # 解析请求：寄存器地址
            reg_addr = frame[2] | (frame[3] << 8)
            # 构建应答：功能码 + 寄存器地址 + 寄存器值（示例值为 0）
            reg_val = 0  # 实际应从寄存器表读取
            ack_frame = struct.pack('<BBHH', addr, NetRawProtocol.FC_READ_SINGLE_ACK, reg_addr, reg_val)
            return CRC16.append(ack_frame)
        
        # 写单寄存器请求 0x06 -> 写单应答 0x16
        elif func == NetRawProtocol.FC_WRITE_SINGLE_REQ:
            if len(frame) < 8:
                return None
            # 解析请求：寄存器地址 + 寄存器值
            reg_addr = frame[2] | (frame[3] << 8)
            reg_val = frame[4] | (frame[5] << 8)
            # 构建应答：原样返回（Modbus 标准）
            ack_frame = struct.pack('<BBHH', addr, NetRawProtocol.FC_WRITE_SINGLE_ACK, reg_addr, reg_val)
            return CRC16.append(ack_frame)
        
        # 写多寄存器请求 0x10 -> 写多应答 0x20
        elif func == NetRawProtocol.FC_WRITE_MULTI_REQ:
            if len(frame) < 11:
                return None
            # 解析请求：起始地址 + 寄存器数量 + 字节数
            start_addr = frame[2] | (frame[3] << 8)
            reg_qty = frame[4] | (frame[5] << 8)
            # 构建应答：功能码 + 起始地址 + 寄存器数量
            ack_frame = struct.pack('<BB', addr, NetRawProtocol.FC_WRITE_MULTI_ACK)
            ack_frame += struct.pack('<HH', start_addr, reg_qty)
            return CRC16.append(ack_frame)
        
        # 主动上传 0x23/0x25 -> 保持原有应答逻辑
        elif func == NetRawProtocol.FC_READ_MULTI_ACTIVE:
            return NetRawProtocol.build_active_upload_ack_from_frame(frame)
        elif func == NetRawProtocol.FC_READ_SINGLE_ACTIVE:
            return NetRawProtocol.build_active_upload_ack_from_frame(frame)
        
        # 不支持的功能码，返回 None（不应答）
        return None
    
    @staticmethod
    def build_uart_cmd(cmd_type: int, addr: int, func: int, reg: int = 0, 
                       val: int = 0, qty: int = 1, data: bytes = b'') -> bytes:
        """构建串口指令"""
        frame = bytes([NetRawProtocol.UART_CMD_PREFIX, cmd_type])
        
        if cmd_type == NetRawProtocol.UART_CMD_SEND_RAW:
            frame += addr.to_bytes(1, 'little')
            frame += data
        elif cmd_type == NetRawProtocol.UART_CMD_SEND_REG:
            frame += bytes([addr, func])
            frame += struct.pack('<HH', reg, val)
            if qty > 1:
                frame += struct.pack('<H', qty)
        elif cmd_type == NetRawProtocol.UART_CMD_SEND_CTRL:
            frame += bytes([addr, func])
            frame += data
        
        frame += bytes([NetRawProtocol.UART_FRAME_END])
        return frame
    
    @staticmethod
    def parse_frame(frame: bytes) -> dict:
        """解析协议帧（PDU 内 16 位字段为 Modbus 大端；CRC 低字节在前）"""
        if len(frame) < 4:
            return None
        
        if not CRC16.verify(frame):
            return None
        
        addr = frame[0]
        func = frame[1]
        
        result = {
            'addr': addr,
            'func': func,
            'raw': frame
        }
        
        if func in [NetRawProtocol.FC_READ_SINGLE_REQ, NetRawProtocol.FC_READ_MULTI_REQ]:
            if len(frame) >= 8:
                reg_addr, reg_qty = struct.unpack('<HH', frame[2:6])
                result['reg_addr'] = reg_addr
                result['reg_qty'] = reg_qty
        elif func in [NetRawProtocol.FC_WRITE_SINGLE_REQ, NetRawProtocol.FC_WRITE_SINGLE_ACK,
                      NetRawProtocol.FC_WRITE_SINGLE_ERR, NetRawProtocol.FC_READ_SINGLE_ACK,
                      NetRawProtocol.FC_READ_SINGLE_ACTIVE, NetRawProtocol.FC_READ_SINGLE_ERR,
                      NetRawProtocol.FC_READ_MULTI_ERR, NetRawProtocol.FC_WRITE_MULTI_ERR]:
            if len(frame) >= 8:
                reg_addr, reg_val = struct.unpack('<HH', frame[2:6])
                result['reg_addr'] = reg_addr
                result['reg_val'] = reg_val
        elif func == NetRawProtocol.FC_WRITE_MULTI_ACK:
            if len(frame) >= 8:
                reg_addr, reg_qty = struct.unpack('<HH', frame[2:6])
                result['reg_addr'] = reg_addr
                result['reg_qty'] = reg_qty
        elif func in [NetRawProtocol.FC_READ_MULTI_ACK, NetRawProtocol.FC_READ_MULTI_ACTIVE]:
            if len(frame) >= 5:
                byte_count = frame[2]
                result['byte_count'] = byte_count
                reg_addr = struct.unpack('<H', frame[3:5])[0]
                result['reg_addr'] = reg_addr
                # print(f"[帧解析] 功能码 0x{func:02X}, 字节数={byte_count}, 起始地址={reg_addr}, 帧长度={len(frame)}")
                if len(frame) >= 5 + byte_count:
                    payload = frame[5:5+byte_count]
                    result['payload'] = payload
                    # print(f"[帧解析] payload 长度={len(payload)}, 数据={payload.hex()}")
                else:
                    # print(f"[帧解析] 帧长度不足，期望{5+byte_count}，实际{len(frame)}")
                    pass
        
        elif func == NetRawProtocol.FC_WRITE_MULTI_REQ:
            if len(frame) >= 7:
                reg_addr, reg_qty = struct.unpack('<HH', frame[2:6])
                byte_count = frame[6]
                result['reg_addr'] = reg_addr
                result['reg_qty'] = reg_qty
        
        return result
    
    @staticmethod
    def frame_len_format_ok(frame_data, frame_len: int) -> bool:
        """
        帧格式校验（严格参照单片机 Net_FrameLen_FormatOk）
        CRC 命中后按功能码校验帧长，避免短帧/伪帧误判
        """
        if not frame_data or frame_len < 4:
            return False
        
        func = frame_data[1]
        
        # 异常功能码 (bit7=1) → 固定 8 字节
        if (func & 0x80) != 0:
            return frame_len == 8
        
        # 根据功能码校验帧长度
        if func in [0x03, 0x05, 0x06, 0x10, 0x13, 0x15, 0x16, 0x20]:
            # 固定长度帧 → 必须 8 字节
            return frame_len == 8
        
        elif func == 0x10:
            # 写多请求 → 可变长度，严格校验字节数字段
            if frame_len < 9:
                return False
            byte_count = frame_data[6]
            reg_qty = frame_data[4] | (frame_data[5] << 8)
            if byte_count != reg_qty * 2:
                return False
            return frame_len == 9 + byte_count
        
        elif func in [0x13, 0x23]:
            # 读多应答/主动上传 → 可变长度，校验字节数字段
            if frame_len < 7:
                return False
            byte_count = frame_data[2]
            return frame_len == 7 + byte_count
        
        else:
            # 不支持的功能码
            return False

    @staticmethod
    def format_alarm_biten(value: int) -> str:
        """解析 reg82 报警使能标志"""
        return f"0x{value:08X}"

    @staticmethod
    def format_dev_addr(value: int) -> str:
        """解析 reg121 设备地址"""
        return f"0x{value:02X}"

    @staticmethod
    def pack_u32_as_reg_pair(value: int) -> List[int]:
        """32 位数值拆成 2 个 16 位寄存器（小端）"""
        value &= 0xFFFFFFFF
        return [value & 0xFFFF, (value >> 16) & 0xFFFF]

    @staticmethod
    def dose_display_to_usv(value: float, unit: str) -> float:
        """界面剂量率阈值 -> uSv/h"""
        if unit == "mSv/h":
            return value * 1000.0
        return value

    @staticmethod
    def build_dose_threshold_write(addr: int, reg: int, value_usv: float) -> bytes:
        """写剂量率阈值 reg50/52（2 寄存器 / uSv/h×100）"""
        if value_usv < 0.0:
            value_usv = 0.0
        raw = int(round(value_usv * 100.0))
        if raw < 0:
            raw = 0
        if raw > 0xFFFFFFFF:
            raw = 0xFFFFFFFF
        return NetRawProtocol.build_write_multi(
            addr, reg, NetRawProtocol.pack_u32_as_reg_pair(raw))

    @staticmethod
    def build_alarm_biten_write(addr: int, biten: int) -> bytes:
        """写 reg82 报警禁止掩码（2 寄存器，bit=1 禁止）"""
        return NetRawProtocol.build_write_multi(
            addr, NetRawProtocol.REG_ALARM_BITEN, NetRawProtocol.pack_u32_as_reg_pair(biten))

    @staticmethod
    def build_control_bit2_write(addr: int, ctrl: int) -> bytes:
        """写 reg123 声/光/屏控制位"""
        return NetRawProtocol.build_write_single(
            addr, NetRawProtocol.REG_CONTROL_BIT2, ctrl & 0xFFFF)

    @staticmethod
    def format_dose_rate_reg(raw_x100: int) -> str:
        """
        剂量率寄存器值（uSv/h×100）格式化显示。
        用于实时剂量率寄存器（reg1-2）和阈值寄存器（reg50/52）。
        转换规则：
          - ≤ 999.99 μSv/h: 显示为 "X.XX μSv/h"
          - > 999.99 μSv/h: 显示为 "X.XX mSv/h"
        注：寄存器值只有数值部分（uSv/h×100），没有单位编码位。
        """
        dose_uSv = raw_x100 / 100.0
        if dose_uSv < 0.0:
            dose_uSv = 0.0
        # >999.99 μSv/h 转换为 mSv/h
        if dose_uSv > 999.99:
            return f"{dose_uSv / 1000.0:.2f} mSv/h"
        return f"{dose_uSv:.2f} μSv/h"
    
    @staticmethod
    def format_dose_threshold_reg(raw_x100: int) -> str:
        """
        剂量率阈值寄存器值（uSv/h×100）格式化显示。
        用于阈值寄存器（reg50/52），转换规则同 format_dose_rate_reg。
        注：阈值寄存器只有数值部分（uSv/h×100），没有单位编码位。
        """
        # 直接调用 format_dose_rate_reg，逻辑完全一致
        return NetRawProtocol.format_dose_rate_reg(raw_x100)

    @staticmethod
    def format_5min_dose_str(dose_uSv: float) -> str:
        """与 geiger.c format_dose_value_str 一致：>999.99 μSv/h 转换为 mSv/h"""
        if dose_uSv < 0.0:
            dose_uSv = 0.0
        if dose_uSv > 999.99:
            return f"{dose_uSv / 1000.0:05.2f}mSv"
        return f"{dose_uSv:04.2f}uSv"

    @staticmethod
    def decode_5min_dose_word(word: int) -> float:
        """解码协议剂量字：低 14 位 = 数值×100，bit15:14 = 单位(00 uSv, 01 mSv, 10 Sv)"""
        unit = (word >> 14) & 0x3
        value = (word & 0x3FFF) / 100.0
        if unit == 0:
            return value
        if unit == 1:
            return value * 1000.0
        return value * 1000000.0

    @staticmethod
    def format_5min_record_line(payload: bytes) -> Optional[str]:
        """
        解析 5 分钟值主动上传 payload（寄存器 30 起 12 字节），
        格式与单片机 Geiger_Dose_Periodic_Save 串口打印一致：
        5min | YYYYMMDD,HHMMSS,XX.XXuSv
        """
        if len(payload) < 12:
            return None
        year = 2000 + payload[0]
        month, day = payload[1], payload[2]
        hour, minute, second = payload[3], payload[4], payload[5]
        datetime_str = f"{year:04d}{month:02d}{day:02d},{hour:02d}{minute:02d}{second:02d}"
        dose_word = payload[8] | (payload[9] << 8)
        dose_uSv = NetRawProtocol.decode_5min_dose_word(dose_word)
        dose_str = NetRawProtocol.format_5min_dose_str(dose_uSv)
        return f"{NetRawProtocol.DATA_5_MIN_LABEL} | {datetime_str},{dose_str}"


def _rx_segment_is_printable_text(part: bytes) -> bool:
    """判断一段是否为可读的调试文本（UTF-8，含中文、空格、换行）。"""
    if not part:
        return True
    try:
        s = part.decode('utf-8')
    except UnicodeDecodeError:
        return False
    ok = 0
    for c in s:
        if c.isprintable() or c in '\n\t\r':
            ok += 1
        elif '\u4e00' <= c <= '\u9fff':
            ok += 1
        elif c == '\ufffd':
            return False
    return ok * 10 >= len(s) * 8


def _rx_hex_chunk_lines(seg: bytes, chunk_bytes: int = 16) -> List[str]:
    """将二进制段按每行 16 字节展开为 HEX 字符串行。"""
    lines: List[str] = []
    for i in range(0, len(seg), chunk_bytes):
        chunk = seg[i : i + chunk_bytes]
        lines.append(' '.join(f'{b:02X}' for b in chunk))
    return lines if lines else ['']


def format_rx_log_lines(data: bytes) -> List[str]:
    """
    仅展开串口收到的字节：按 \r\n 分段；可打印段为原文；否则为分行 HEX。
    不附加「RX 字节数」、协议诊断等上位机提示。
    """
    lines: List[str] = []
    if not data:
        return lines

    for seg in data.split(b"\r\n"):
        if not seg:
            continue
        if _rx_segment_is_printable_text(seg):
            text = seg.decode("utf-8", errors="replace").replace("\r", " ")
            lines.append(text)
        else:
            lines.extend(_rx_hex_chunk_lines(seg))

    if not lines:
        if _rx_segment_is_printable_text(data):
            lines.append(data.decode("utf-8", errors="replace").replace("\r", " "))
        else:
            lines.extend(_rx_hex_chunk_lines(data))
    return lines


def extract_rx_write_multi_ack_lines(data: bytes) -> List[str]:
    """
    在 RX 字节流中滑动查找 CRC 有效的 8 字节帧，识别写多应答 0x20 / 写多异常 0x90，
    用于判断单片机是否解析并应答上位机发出的写多指令。
    """
    lines: List[str] = []
    if len(data) < 8:
        return lines
    seen: set[str] = set()
    for i in range(len(data) - 7):
        chunk = data[i : i + 8]
        if not CRC16.verify(chunk):
            continue
        fi = NetRawProtocol.parse_frame(chunk)
        if not fi:
            continue
        key = chunk.hex()
        if key in seen:
            continue
        seen.add(key)
        fc = fi["func"]
        if fc == NetRawProtocol.FC_WRITE_MULTI_ACK:
            ra = fi.get("reg_addr", 0)
            rq = fi.get("reg_qty", 0)
            lines.append(
                f"[设备解析] 写多应答成功 0x20 | 起始寄存器=0x{ra:04X}({ra}) | 已写入寄存器个数={rq}"
            )
        elif fc == NetRawProtocol.FC_WRITE_MULTI_ERR:
            ra = fi.get("reg_addr", 0)
            rv = fi.get("reg_val", 0)
            lines.append(
                f"[设备解析] 写多异常 0x90 | 寄存器=0x{ra:04X}({ra}) | 异常信息字=0x{rv:04X}"
            )
    return lines


class TCPClient:
    """TCP 客户端类（支持异步发送队列）"""
    
    def __init__(self, enable_reader: bool = True):
        self.enable_reader = enable_reader
        self.socket: Optional[socket.socket] = None
        self.is_reading = False
        self.read_thread: Optional[threading.Thread] = None
        self.on_data_received = None
        self.on_data_sent = None  # 发送成功后的回调
        self.on_data_received_raw = None  # 收到原始数据的回调（立即显示）
        self.lock = threading.Lock()
        self.connected = False
        self.read_error = False
        
        # 异步发送队列
        self.send_queue = queue.Queue()
        self.send_thread: Optional[threading.Thread] = None
        self.send_thread_running = False
        self.last_send_error = None
        
        # 发送统计（用于验证数据是否真的发送）
        self.send_count = 0  # 成功发送的包数
        self.send_bytes = 0  # 成功发送的字节数
        self.send_fail_count = 0  # 发送失败次数
    
    def connect(self, ip: str, port: int) -> bool:
        """连接 TCP 服务器"""
        # 确保先完全断开旧连接
        if self.socket or self.connected:
            self.disconnect()
        
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(2.0)
            self.socket.connect((ip, port))
            
            # 禁用 Nagle 算法，让数据包立即发送（减少延迟）
            self.socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            
            _set_tcp_keepalive(self.socket)
            self.socket.settimeout(0.5)
            self.connected = True
            self.read_error = False
            
            # 启动发送线程
            self.send_thread_running = True
            self.send_thread = threading.Thread(target=self._send_worker, daemon=True)
            self.send_thread.start()
            
            if self.enable_reader:
                self.is_reading = True
                self.read_thread = threading.Thread(target=self._read_loop, daemon=True)
                self.read_thread.start()
            else:
                self.is_reading = False
                self.read_thread = None
            return True
        except Exception as e:
            print(f"TCP 连接失败：{e}")
            return False
    
    def disconnect(self):
        """断开连接"""
        # 停止发送线程
        self.send_thread_running = False
        # 清空队列，避免阻塞
        try:
            while not self.send_queue.empty():
                self.send_queue.get_nowait()
        except queue.Empty:
            pass
        # 发送 None 唤醒发送线程
        try:
            self.send_queue.put_nowait(None)
        except queue.Full:
            pass
        if self.send_thread:
            self.send_thread.join(timeout=1.0)
            self.send_thread = None
        
        with self.lock:
            self.is_reading = False
            self.connected = False
            self.read_error = False
            sock = self.socket
            self.socket = None
        if sock:
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            try:
                sock.close()
            except OSError:
                pass
        if self.read_thread:
            self.read_thread.join(timeout=1.0)
            self.read_thread = None
    
    def _mark_disconnected(self, reason: str = "") -> None:
        """读线程或发送失败时标记连接已失效"""
        if reason:
            print(f"TCP 连接断开：{reason}")
        with self.lock:
            self.is_reading = False
            self.connected = False
            self.read_error = True
    
    def _read_loop(self):
        """TCP 读取循环（recv 不持锁，避免阻塞 GUI 发送）"""
        while True:
            with self.lock:
                if not self.is_reading:
                    break
                sock = self.socket
            if sock is None:
                break
            try:
                sock.settimeout(0.5)
                data = sock.recv(4096)
            except socket.timeout:
                continue
            except BlockingIOError:
                continue
            except (ConnectionResetError, ConnectionAbortedError, BrokenPipeError) as e:
                self._mark_disconnected(str(e))
                break
            except OSError as e:
                with self.lock:
                    still_reading = self.is_reading
                if still_reading:
                    self._mark_disconnected(str(e))
                break
            except Exception as e:
                print(f"TCP 读取错误：{e}")
                self._mark_disconnected(str(e))
                break
            
            # 收到数据后立即显示原始数据（在接收线程中）
            if data:
                # 立即显示接收到的原始数据
                if self.on_data_received_raw:
                    try:
                        self.on_data_received_raw(data)
                    except Exception as e:
                        print(f"接收原始数据显示失败：{e}")
                
                # 然后继续处理数据（解析、应答等）
                if self.on_data_received:
                    try:
                        self.on_data_received(data)
                    except RuntimeError:
                        break
            else:
                self._mark_disconnected("对端关闭连接")
                break

        with self.lock:
            still_reading = self.is_reading
        if still_reading:
            self._mark_disconnected("读线程退出")
    
    def _send_worker(self):
        """后台发送线程工作函数（高优先级，有数据时立即发送，发送成功后显示）"""
        # 设置线程优先级（如果平台支持）
        try:
            import psutil
            psutil.Process().nice(psutil.IOPRIO_CLASS_RT)
        except Exception as e:
            # psutil 可能未安装，忽略错误
            pass
        
        while self.send_thread_running:
            try:
                # 先尝试非阻塞获取（有数据时立即发送）
                try:
                    data = self.send_queue.get_nowait()
                except queue.Empty:
                    # 队列为空，短暂等待
                    try:
                        data = self.send_queue.get(timeout=0.05)
                    except queue.Empty:
                        continue
                
                # 收到 None 表示退出信号
                if data is None:
                    break
                
                # 检查连接状态（在锁内）
                with self.lock:
                    if not self.connected or not self.socket:
                        self.last_send_error = "未连接"
                        # 连接断开时，清空队列避免阻塞
                        try:
                            while not self.send_queue.empty():
                                self.send_queue.get_nowait()
                        except:
                            pass
                        continue
                    sock = self.socket
                
                # 发送数据（阻塞操作）
                try:
                    # 记录发送前的时间戳
                    import time
                    send_start = time.time()
                    
                    sock.sendall(data)
                    
                    # 记录发送完成的时间
                    send_end = time.time()
                    send_duration_ms = (send_end - send_start) * 1000
                    
                    # 更新发送统计
                    self.send_count += 1
                    self.send_bytes += len(data)
                    
                    # 发送成功后，回调显示函数（在发送线程中直接显示）
                    if self.on_data_sent:
                        try:
                            self.on_data_sent(data)
                        except Exception as e:
                            print(f"发送显示回调失败：{e}")
                    
                except (ConnectionResetError, ConnectionAbortedError, BrokenPipeError, OSError) as e:
                    print(f"TCP 发送失败：{e}")
                    self.last_send_error = str(e)
                    self.send_fail_count += 1
                    self._mark_disconnected(str(e))
                    
            except Exception as e:
                print(f"发送线程异常：{e}")
                continue

    def send_sync(self, data: bytes, max_retry=3, retry_delay_ms=10) -> bool:
        """同步发送数据（带重试机制，确保数据成功传输到网络）
        
        Args:
            data: 要发送的数据
            max_retry: 最大重试次数，默认 3 次
            retry_delay_ms: 重试延时（毫秒），默认 10ms
        
        Returns:
            bool: 发送是否成功
        """
        # 先检查连接是否真的有效
        if not self.check_alive():
            return False
        
        # 检查连接状态（在锁内）
        with self.lock:
            if not self.connected or not self.socket:
                self.last_send_error = "未连接"
                return False
            sock = self.socket
        
        # 重试机制：最多尝试 max_retry 次
        for attempt in range(1, max_retry + 1):
            try:
                # 发送数据（阻塞操作）
                send_start = time.time()
                sock.sendall(data)
                send_end = time.time()
                send_duration_ms = (send_end - send_start) * 1000
                
                # 更新发送统计
                self.send_count += 1
                self.send_bytes += len(data)
                
                # 发送成功后，回调显示函数
                if self.on_data_sent:
                    try:
                        self.on_data_sent(data)
                    except Exception as e:
                        print(f"发送显示回调失败：{e}")
                
                # 打印发送成功信息（调试用）
                # print(f"[TCP 同步发送] 成功发送 {len(data)} 字节，耗时：{send_duration_ms:.2f}ms，尝试次数：{attempt}")
                
                return True  # 发送成功，直接返回
                
            except (ConnectionResetError, ConnectionAbortedError, BrokenPipeError, OSError) as e:
                self.last_send_error = str(e)
                self.send_fail_count += 1
                
                if attempt < max_retry:
                    # 发送失败，准备重试
                    print(f"TCP 同步发送失败（第{attempt}次）：{e}，准备重试...")
                    # 短暂延时后重试（避免立即重试失败）
                    if retry_delay_ms > 0:
                        time.sleep(retry_delay_ms / 1000.0)  # 转换为秒
                else:
                    # 达到最大重试次数
                    print(f"TCP 同步发送失败（已重试{max_retry}次）：{e}")
                    self._mark_disconnected(str(e))
                    return False
                    
            except Exception as e:
                print(f"TCP 同步发送异常：{e}")
                return False
        
        return False
    
    def send(self, data: bytes) -> bool:
        """同步发送数据（带重试机制，默认 3 次重试，10ms 延时）"""
        return self.send_sync(data, max_retry=3, retry_delay_ms=10)

    def check_alive(self) -> bool:
        """检测 TCP 连接是否仍有效"""
        if not self.socket or not self.connected:
            return False
        if self.read_error:
            return False
        if not self.enable_reader:
            return True
        if self.read_thread and not self.read_thread.is_alive():
            return False
        return True
    
    def get_send_queue_size(self) -> int:
        """获取当前发送队列大小"""
        return self.send_queue.qsize()
    
    def wait_send_empty(self, timeout: float = 5.0) -> bool:
        """等待发送队列为空（用于 OTA 等场景）"""
        start_time = time.time()
        while time.time() - start_time < timeout:
            if self.send_queue.empty():
                return True
            time.sleep(0.01)
        return False
    
    def check_send_status(self) -> dict:
        """检查发送状态（用于调试）"""
        return {
            'queue_size': self.send_queue.qsize(),
            'connected': self.connected,
            'thread_running': self.send_thread_running,
            'last_error': self.last_send_error,
            'send_count': self.send_count,
            'send_bytes': self.send_bytes,
            'fail_count': self.send_fail_count,
        }
    
    def verify_send_to_network(self, timeout_sec: float = 2.0) -> bool:
        """
        验证数据是否真的发送到网络（通过等待队列清空和检查连接状态）
        
        返回 True 表示数据已发送到网络，False 表示可能未发送
        
        注意：这是一个间接验证，因为 Python 无法直接知道网卡何时发送数据
        """
        # 1. 等待发送队列清空
        if not self.wait_send_empty(timeout=timeout_sec):
            print(f"[验证] 超时：队列还有 {self.send_queue.qsize()} 个包未发送")
            return False
        
        # 2. 检查连接状态
        if not self.connected:
            print("[验证] 失败：连接已断开")
            return False
        
        # 3. 检查是否有发送错误
        if self.last_send_error:
            print(f"[验证] 失败：{self.last_send_error}")
            return False
        
        # 4. 如果队列清空且连接正常，认为数据已发送到网络
        return True
    
    def reset_send_stats(self):
        """重置发送统计"""
        self.send_count = 0
        self.send_bytes = 0
        self.send_fail_count = 0


class TCPDualChannel:
    """
    TCP 连接管理：默认同端口单连接收发；控制/数据端口不同时分别建立两条连接。
    默认端口见 DEFAULT_TCP_CTRL_PORT / DEFAULT_TCP_DATA_PORT。
    """

    def __init__(self):
        self.ctrl = TCPClient(enable_reader=False)
        self.data = TCPClient(enable_reader=True)
        self.single = TCPClient(enable_reader=True)
        self._single_port_mode = False
        self._on_data_received = None
        self._on_data_sent = None  # 发送回调
        self._on_data_received_raw = None  # 原始数据接收回调
        self.ctrl_connected = False
        self.data_connected = False

    @property
    def single_port_mode(self) -> bool:
        return self._single_port_mode

    def _send_client(self) -> TCPClient:
        return self.single if self._single_port_mode else self.ctrl

    def _recv_client(self) -> TCPClient:
        return self.single if self._single_port_mode else self.data

    def set_on_data_received(self, callback: Optional[Callable]) -> None:
        """设置数据接收回调"""
        self._on_data_received = callback
        self._apply_data_callback()
    
    def set_on_data_sent(self, callback: Optional[Callable]) -> None:
        """设置数据发送回调"""
        self._on_data_sent = callback
        self._apply_sent_callback()
    
    def set_on_data_received_raw(self, callback: Optional[Callable]) -> None:
        """设置原始数据接收回调（收到数据立即显示，不等待解析）"""
        self._on_data_received_raw = callback
        self._apply_raw_received_callback()
    
    def _apply_raw_received_callback(self) -> None:
        if self._on_data_received_raw is None:
            return
        if self._single_port_mode:
            self.single.on_data_received_raw = self._on_data_received_raw
            self.data.on_data_received_raw = None
            self.ctrl.on_data_received_raw = None
        else:
            self.data.on_data_received_raw = self._on_data_received_raw
            self.single.on_data_received_raw = None
            self.ctrl.on_data_received_raw = None
    
    def _apply_sent_callback(self) -> None:
        if self._on_data_sent is None:
            return
        if self._single_port_mode:
            self.single.on_data_sent = self._on_data_sent
            self.data.on_data_sent = None
            self.ctrl.on_data_sent = None
        else:
            self.ctrl.on_data_sent = self._on_data_sent
            self.single.on_data_sent = None
            self.data.on_data_sent = None

    def _apply_data_callback(self) -> None:
        if self._on_data_received is None:
            return
        if self._single_port_mode:
            self.single.on_data_received = self._on_data_received
            self.data.on_data_received = None
            self.ctrl.on_data_received = None
        else:
            self.data.on_data_received = self._on_data_received
            self.single.on_data_received = None
            self.ctrl.on_data_received = None

    def connect(self, ip: str, ctrl_port: int, data_port: int) -> Tuple[bool, bool]:
        """连接 TCP；同端口只连一次，不同端口分别连控制口与数据口"""
        self.disconnect()

        if ctrl_port == data_port:
            self._single_port_mode = True
            ok = self.single.connect(ip, ctrl_port)
            self.ctrl_connected = ok
            self.data_connected = ok
            if ok:
                self._apply_data_callback()
            return ok, ok

        self._single_port_mode = False
        results = {'ctrl': False, 'data': False}

        def _connect_ctrl():
            results['ctrl'] = self.ctrl.connect(ip, ctrl_port)

        def _connect_data():
            results['data'] = self.data.connect(ip, data_port)

        t_ctrl = threading.Thread(target=_connect_ctrl, daemon=True)
        t_data = threading.Thread(target=_connect_data, daemon=True)
        t_ctrl.start()
        t_data.start()
        t_ctrl.join(timeout=5.0)
        t_data.join(timeout=5.0)

        ok_ctrl = results['ctrl']
        ok_data = results['data']
        if not (ok_ctrl and ok_data):
            self.disconnect()
            ok_ctrl = False
            ok_data = False
        self.ctrl_connected = ok_ctrl
        self.data_connected = ok_data
        if ok_ctrl and ok_data:
            self._apply_data_callback()
        return ok_ctrl, ok_data

    def disconnect(self) -> None:
        if self._single_port_mode:
            self.single.disconnect()
        else:
            self.ctrl.disconnect()
            self.data.disconnect()
        self.ctrl_connected = False
        self.data_connected = False
        self._single_port_mode = False

    def send(self, data: bytes) -> bool:
        """经控制口（或单端口连接）发送"""
        return self._send_client().send(data)

    def sync_connection_state(self) -> None:
        """根据实际 socket 状态同步连接标志"""
        if self._single_port_mode:
            if self.ctrl_connected and not self.single.check_alive():
                self.ctrl_connected = False
                self.data_connected = False
            return
        if self.ctrl_connected and not self.ctrl.check_alive():
            self.ctrl_connected = False
        if self.data_connected and not self.data.check_alive():
            self.data_connected = False

    def is_fully_connected(self) -> bool:
        self.sync_connection_state()
        if self._single_port_mode:
            return self.ctrl_connected
        return self.ctrl_connected and self.data_connected

    def check_ctrl_alive(self) -> bool:
        return self._send_client().check_alive()

    def check_data_alive(self) -> bool:
        return self._recv_client().check_alive()


class UDPServer:
    """UDP 服务器类"""
    
    def __init__(self):
        self.socket: Optional[socket.socket] = None
        self.is_reading = False
        self.read_thread: Optional[threading.Thread] = None
        self.on_data_received = None
        self.lock = threading.Lock()
        self.bound = False
        self.bind_local_ip = "0.0.0.0"
        self.bind_port = 0
        self.bind_multicast = "236.2.3.6"
        self.read_error = False
    
    def bind(self, local_ip: str, port: int, multicast_group: str = "236.2.3.6") -> bool:
        """绑定 UDP 端口并加入组播组"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            
            # 固定绑定 0.0.0.0，Windows 上接收组播最可靠
            self.socket.bind(("0.0.0.0", port))
            self.socket.settimeout(0.5)
            
            group = socket.inet_aton(multicast_group)
            if local_ip and local_ip not in ("0.0.0.0", ""):
                # 指定网卡加入组播
                mreq = struct.pack("4s4s", group, socket.inet_aton(local_ip))
                bind_desc = f"0.0.0.0:{port} (网卡 {local_ip})"
            else:
                mreq = struct.pack("4sI", group, socket.INADDR_ANY)
                bind_desc = f"0.0.0.0:{port} (全部网卡)"
            
            self.socket.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
            
            print(f"[UDP] 绑定 {bind_desc}，组播 {multicast_group}:{port}")
            
            self.bind_local_ip = local_ip or "0.0.0.0"
            self.bind_port = port
            self.bind_multicast = multicast_group
            self.read_error = False
            self.bound = True
            self.is_reading = True
            self.read_thread = threading.Thread(target=self._read_loop, daemon=True)
            self.read_thread.start()
            return True
        except Exception as e:
            print(f"UDP 绑定失败：{e}")
            import traceback
            traceback.print_exc()
            return False
    
    def unbind(self):
        """解除绑定"""
        self.is_reading = False
        self.bound = False
        self.read_error = False
        if self.read_thread:
            self.read_thread.join(timeout=1.0)
            self.read_thread = None
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
    
    def _read_loop(self):
        """UDP 读取循环"""
        while self.is_reading and self.socket:
            try:
                data, addr = self.socket.recvfrom(4096)
                if data and self.on_data_received:
                    self.on_data_received(data, addr)
            except socket.timeout:
                continue
            except OSError as e:
                if self.is_reading:
                    self.read_error = True
                    print(f"UDP 读取 OSError：{e}")
                    break
            except Exception as e:
                print(f"UDP 读取错误：{e}")
                break
            time.sleep(0.01)
    
    def send(self, data: bytes, addr: tuple = None) -> bool:
        """发送数据"""
        if self.socket and self.bound:
            try:
                with self.lock:
                    if addr:
                        self.socket.sendto(data, addr)
                    else:
                        self.socket.sendto(data, ('255.255.255.255', 2468))
                return True
            except Exception as e:
                print(f"UDP 发送失败：{e}")
                return False
        return False


class NetRawTesterGUI:
    """Net Raw 协议测试上位机 GUI"""

    _TCP_ADDR_COLOR_AUTO = "#333333"
    _TCP_ADDR_COLOR_IDENTIFIED = "#0099CC"  # 识别后浅青色
    
    def __init__(self, root):
        self.root = root
        self.root.title(APP_TITLE)
        self.root.geometry("1395x955")  # 宽度 1395，高度增加 15 像素（从 940 改为 955）
        
        self.serial_monitor = SerialConnectionService()
        self.tcp_dual = TCPDualChannel()
        self.udp_server = UDPServer()
        
        # 设置 TCP 发送回调（发送成功后立即显示）
        self.tcp_dual.set_on_data_sent(self._on_tcp_data_sent)
        # 设置 TCP 原始数据接收回调（收到数据立即显示，不等待解析）
        self.tcp_dual.set_on_data_received_raw(self._on_tcp_data_received_raw)
        
        self.connected = False
        self.current_mode = "tcp"  # 默认 TCP 模式
        self.current_port = None
        
        # 每个模式独立的连接状态
        self.serial_connected = False  # 串口连接状态
        self.tcp_connected = False     # TCP 连接状态
        self.udp_connected = False     # UDP 连接状态
        
        # 连接监控相关 - 每个模式独立的监控线程
        self.tcp_monitor_thread = None  # TCP 监控线程
        self.udp_monitor_thread = None  # UDP 监控线程
        self.serial_is_monitoring = False  # 串口是否正在监控
        self.tcp_is_monitoring = False  # TCP 是否正在监控
        self.udp_is_monitoring = False  # UDP 是否正在监控
        self._tcp_disconnect_detected_time = None
        self._udp_disconnect_detected_time = None
        self.reconnect_timeout = 20.0  # 重连超时时间（秒）
        self.is_reconnecting = False  # 是否正在重连（监控线程内部使用）
        self._serial_manual_disconnect = False
        self._tcp_manual_disconnect = False
        self._udp_manual_disconnect = False
        self._udp_nic_waiting = False  # UDP 模式：本地网卡曾断开，等待恢复中
        self._udp_rebind_grace_until = 0.0  # 重绑后短暂宽限，避免误判再次重绑
        self._local_ip_cache: Optional[List[str]] = None
        self._local_ip_cache_ts = 0.0
        self._local_ip_scanning = False
        self._tcp_link_grace_until = 0.0  # TCP 连接/重连后宽限期，避免误判断线
        
        # 记住连接时的串口号
        self._connected_serial_port = None  # 记住连接时的串口号
        self._syncing_port_combo = False  # 程序同步下拉框时抑制自动重连
        
        # 寄存器数据显示字典
        self.register_values = {}
        self._reg_raw_cache = {}
        
        # 累积接收缓冲区（解决 TCP 分包问题）
        self.rx_buffer = bytearray()
        self.rx_lock = threading.Lock()
        self._tcp_parse_expected_addr = -1
        self._tcp_identified_addr = None
        self._ui_mode_cache = "tcp"  # 主线程写入；读线程只读，用于按 UI 模式过滤显示
        self._app_closing = False
        # 串口文本行缓冲（避免 UTF-8/串口分包导致一行拆成两帧显示）
        self.serial_line_buffer = bytearray()
        
        # 颜色循环列表（用于区分不同帧）
        self.rx_color_index = 0
        self.tx_color_index = 0
        self.debug_color_index = 0  # 调试信息框的颜色索引
        self.history_color_index = 0  # 历史记录框的颜色索引
        # 接收窗和发送窗使用相同的高对比度颜色循环
        self.frame_colors = [
            "#FF6600",  # 橙色
            "#0066FF",  # 蓝色
            "#00AA00",  # 绿色
            "#AA00AA",  # 紫色
            "#FF9900",  # 橙黄色
            "#00AAAA",  # 青蓝色
            "#FF0066",  # 玫红色
        ]
        self.rx_colors = self.frame_colors
        self.tx_colors = self.frame_colors
        
        self.setup_ui()
        
        # 配置接收窗的颜色标签
        self.rx_text.tag_config('black', foreground='#000000')
        for color in self.rx_colors:
            self.rx_text.tag_config('color_' + color, foreground=color)
        
        # 配置发送窗的颜色标签
        self.tx_text.tag_config('black', foreground='#000000')
        for color in self.tx_colors:
            self.tx_text.tag_config('color_' + color, foreground=color)
        
        self._bind_serial_service_callbacks()
        self.tcp_dual.set_on_data_received(lambda data: self._dispatch_received(data, "tcp"))
        self.udp_server.on_data_received = lambda data, addr=None: self._dispatch_received(data, "udp")
        
        self.update_port_list()

    def _bind_serial_service_callbacks(self) -> None:
        """串口服务回调（看门狗在后台线程，UI 更新走 root.after）"""
        self.serial_monitor.on_data_received = lambda data: self._dispatch_received(data, "serial")
        self.serial_monitor.on_log = lambda msg: self._safe_ui_after(
            0, lambda m=msg: self.log_receive(m)
        )
        self.serial_monitor.on_reconnecting = lambda: self.root.after(
            0, self._on_serial_watchdog_reconnecting
        )
        self.serial_monitor.on_connected = lambda port, baud: self.root.after(
            0, lambda p=port, b=baud: self._on_serial_watchdog_connected(p, b)
        )
        self.serial_monitor.on_reconnect_failed = lambda: self.root.after(
            0, self._on_serial_watchdog_gave_up
        )

    def _on_serial_watchdog_reconnecting(self) -> None:
        self.serial_connected = False
        self.is_reconnecting = True
        self._update_serial_status_ui()

    def _on_serial_watchdog_connected(self, port: str, baudrate: int) -> None:
        self.serial_connected = True
        self._connected_serial_port = port
        self.connected = (
            self.serial_connected or self.tcp_connected or self.udp_connected
        )
        self.is_reconnecting = False
        self._refresh_serial_port_combo(prefer_port=port)
        self._update_serial_status_ui()
        if not self.serial_monitor.watchdog_running:
            self.serial_is_monitoring = True
            self.serial_monitor.start_watchdog()

    def _on_serial_watchdog_gave_up(self) -> None:
        self.serial_connected = False
        self.is_reconnecting = False
        self.serial_is_monitoring = False
        self._update_serial_status_ui()
        if hasattr(self, "port_combo"):
            self.port_combo.config(state=tk.NORMAL)
        if hasattr(self, "baud_combo"):
            self.baud_combo.config(state=tk.NORMAL)
    
    def _on_tcp_data_sent(self, data: bytes) -> None:
        """TCP 发送成功后的回调（在发送线程中调用）"""
        # 需要在主线程中更新 UI
        self.root.after(0, lambda: self._display_tcp_sent_data(data))
    
    def _on_tcp_data_received_raw(self, data: bytes) -> None:
        """TCP 收到原始数据的回调（在接收线程中调用，立即显示）"""
        # 需要在主线程中更新 UI
        self.root.after(0, lambda: self._display_tcp_received_raw(data))
    
    def _display_tcp_received_raw(self, data: bytes) -> None:
        """在接收框显示 TCP 接收的原始数据（在主线程中执行）"""
        # 检查是否应该显示 TCP 接收数据
        mode = self.mode_var.get().split(' - ')[0].lower()
        show_rx = (mode != "tcp") or getattr(self, '_tcp_log_rx_enabled', True)
        
        if not show_rx:
            return
        
        # 显示时间戳（黑色）
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        self.rx_text.insert(tk.END, f"[{timestamp}]\n", 'black')
        
        # 一帧一种颜色（所有行使用相同颜色）
        color = self.rx_colors[self.rx_color_index % len(self.rx_colors)]
        self.rx_color_index += 1
        
        # TCP 模式：显示 16 进制格式
        # 格式化帧数据（每行 16 字节）
        hex_str = " ".join(f"{b:02X}" for b in data)
        for i in range(0, len(hex_str.split()), 16):
            line = ' '.join(hex_str.split()[i:i+16])
            self.rx_text.insert(tk.END, line + "\n", ('color_' + color))
        
        # 自动滚动到底部
        self.rx_text.see(tk.END)
    
    def _display_tcp_sent_data(self, data: bytes) -> None:
        """在发送框显示 TCP 发送的数据（在主线程中执行）"""
        # 检查是否应该显示 TCP 发送数据
        mode = self.mode_var.get().split(' - ')[0].lower()
        show_tx = (mode != "tcp") or getattr(self, '_tcp_log_tx_enabled', True)
        
        if not show_tx:
            return
        
        # 显示时间戳（黑色）
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        self.tx_text.insert(tk.END, f"[{timestamp}]\n", 'black')
        
        # 一帧一种颜色（所有行使用相同颜色）
        color = self.tx_colors[self.tx_color_index % len(self.tx_colors)]
        self.tx_color_index += 1
        
        # TCP/UDP模式：显示 16 进制格式
        # 格式化帧数据（每行 16 字节）
        hex_str = " ".join(f"{b:02X}" for b in data)
        for i in range(0, len(hex_str.split()), 16):
            line = ' '.join(hex_str.split()[i:i+16])
            self.tx_text.insert(tk.END, line + "\n", ('color_' + color))
        
        # 自动滚动到底部
        self.tx_text.see(tk.END)
    
    def setup_ui(self):
        """设置界面"""
        main_paned = tk.PanedWindow(self.root, orient=tk.VERTICAL, sashrelief=tk.RAISED, sashwidth=5)
        main_paned.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        top_paned = tk.PanedWindow(main_paned, orient=tk.HORIZONTAL, sashrelief=tk.RAISED, sashwidth=5)
        main_paned.add(top_paned)
        
        # 右侧留白，避免 place(relwidth=1.0) 的 LabelFrame 边框被 PanedWindow 分隔条遮挡
        self._left_sash_pad = 6
        left_frame = ttk.Frame(top_paned, padding=(0, 0, self._left_sash_pad, 0))
        top_paned.add(left_frame, width=882)  # 增加 88 像素（794 的九分之一）
        
        self.right_frame = ttk.Frame(top_paned)
        top_paned.add(self.right_frame, width=432)
        
        conn_frame = ttk.LabelFrame(left_frame, text="连接设置", padding=0)
        conn_frame.place(x=0, y=0, relwidth=1.0, height=60)
        
        # 使用坐标定位所有部件，不再使用 pack
        # 模式选择（x=5, y=5）
        ttk.Label(conn_frame, text="模式:", width=5).place(x=5, y=5)
        self.mode_var = tk.StringVar(value="tcp")  # 默认 TCP 模式
        self.mode_combo = ttk.Combobox(conn_frame, textvariable=self.mode_var, width=10, state="readonly")
        self.mode_combo['values'] = ['Serial', 'TCP', 'UDP']
        self.mode_combo.current(1)  # 默认选择 TCP（索引 1）
        self.mode_combo.place(x=50, y=5)
        self.mode_combo.bind('<<ComboboxSelected>>', self.on_mode_changed)
        
        # 串口设置框（x=148, y=5）
        self.serial_frame = ttk.Frame(conn_frame)
        self.serial_frame.place(x=148, y=5)
        ttk.Label(self.serial_frame, text="端口:", width=5).pack(side=tk.LEFT, padx=2)
        self.port_combo = ttk.Combobox(self.serial_frame, width=14, state="readonly")
        self.port_combo.pack(side=tk.LEFT, padx=2)
        # 填充串口列表
        self._populate_serial_ports()
        # 绑定事件：端口变化时自动重连
        self.port_combo.bind('<<ComboboxSelected>>', self.on_serial_param_changed)
        ttk.Label(self.serial_frame, text="波特率:", width=6).pack(side=tk.LEFT, padx=(10,2))
        self.baud_combo = ttk.Combobox(self.serial_frame, width=7, values=[
            '9600', '19200', '38400', '57600', '115200', 
            '230400', '460800', '921600'
        ])
        self.baud_combo.set('921600')
        self.baud_combo.pack(side=tk.LEFT, padx=(10,2))
        # 绑定事件：波特率变化时自动重连
        self.baud_combo.bind('<<ComboboxSelected>>', self.on_serial_param_changed)
        
        # TCP 设置框（初始隐藏）（x=148, y=5）
        self.tcp_frame = ttk.Frame(conn_frame)
        self.tcp_frame.place(x=148, y=5)
        ttk.Label(self.tcp_frame, text="IP:", width=3).pack(side=tk.LEFT, padx=2)
        self.tcp_ip_entry = ttk.Entry(self.tcp_frame, width=12)
        self.tcp_ip_entry.insert(0, "192.168.2.2")
        self.tcp_ip_entry.pack(side=tk.LEFT, padx=2)
        ttk.Label(self.tcp_frame, text="控制端口:", width=8).pack(side=tk.LEFT, padx=(6,2))
        self.tcp_ctrl_port_entry = ttk.Entry(self.tcp_frame, width=4)
        self.tcp_ctrl_port_entry.insert(0, str(DEFAULT_TCP_CTRL_PORT))
        self.tcp_ctrl_port_entry.pack(side=tk.LEFT, padx=2)
        ttk.Label(self.tcp_frame, text="数据端口:", width=8).pack(side=tk.LEFT, padx=(4,2))
        self.tcp_data_port_entry = ttk.Entry(self.tcp_frame, width=4)
        self.tcp_data_port_entry.insert(0, str(DEFAULT_TCP_DATA_PORT))
        self.tcp_data_port_entry.pack(side=tk.LEFT, padx=2)
        ttk.Label(self.tcp_frame, text="设备地址:", width=8).pack(side=tk.LEFT, padx=(5,2))
        self.tcp_addr_var = tk.StringVar(value="Auto")
        self.tcp_addr_label = ttk.Label(
            self.tcp_frame, textvariable=self.tcp_addr_var, width=5,
            foreground=self._TCP_ADDR_COLOR_AUTO)
        self.tcp_addr_label.pack(side=tk.LEFT, padx=2)
        
        # UDP 设置框（初始隐藏）（x=148, y=5）
        self.udp_frame = ttk.Frame(conn_frame)
        self.udp_frame.place(x=148, y=5)
        ttk.Label(self.udp_frame, text="本地 IP:", width=7).pack(side=tk.LEFT, padx=2)
        self.udp_local_ip_combo = ttk.Combobox(self.udp_frame, width=14, state="readonly")
        self.udp_local_ip_combo.pack(side=tk.LEFT, padx=2)
        self.udp_local_ip_combo.bind('<Button-1>', self._on_udp_local_ip_combo_click, add='+')
        self.udp_local_ip_combo['values'] = ["0.0.0.0"]
        self.udp_local_ip_combo.current(0)
        
        ttk.Label(self.udp_frame, text="远端 IP:", width=7).pack(side=tk.LEFT, padx=(10,2))
        self.udp_ip_entry = ttk.Entry(self.udp_frame, width=11)
        self.udp_ip_entry.insert(0, "236.2.3.6")  # 远程设备 IP（组播地址）
        self.udp_ip_entry.pack(side=tk.LEFT, padx=2)
        ttk.Label(self.udp_frame, text="端口:", width=4).pack(side=tk.LEFT, padx=(10,2))
        self.udp_port_entry = ttk.Entry(self.udp_frame, width=5)
        self.udp_port_entry.insert(0, "2468")
        self.udp_port_entry.pack(side=tk.LEFT, padx=2)
        # UDP 模式不需要设备地址输入框
        
        # 按钮区域（右对齐）
        btn_frame = ttk.Frame(conn_frame)
        # 连接设置框高度是60，按钮区域高度25，垂直居中：y=(60-25)/2=17.5
        btn_frame.place(relx=1.0, y=17, anchor="e", width=290, height=28)

        # 为每个模式创建独立的连接按钮和状态标签
        # 串口模式
        self.serial_status_label = ttk.Label(btn_frame, text="未连接", foreground="red", width=18)
        self.serial_connect_btn = ttk.Button(btn_frame, text="连接", command=self.toggle_connection, width=8)
        # TCP 模式
        self.tcp_status_label = ttk.Label(btn_frame, text="未连接", foreground="red", width=18)
        self.tcp_connect_btn = ttk.Button(btn_frame, text="连接", command=self.toggle_connection, width=8)
        # UDP 模式
        self.udp_status_label = ttk.Label(btn_frame, text="未连接", foreground="red", width=18)
        self.udp_connect_btn = ttk.Button(btn_frame, text="连接", command=self.toggle_connection, width=8)

        # 刷新按钮（所有模式共用）
        self.refresh_btn = ttk.Button(btn_frame, text="刷新", command=self.update_port_list, width=8)
        self.refresh_btn.place(x=217, y=0)  # 最右边

        # 当前显示的按钮和标签（根据模式切换）
        self.current_connect_btn = self.serial_connect_btn
        self.current_status_label = self.serial_status_label
        # 初始化时显示串口模式的按钮和标签（使用place布局）
        # 顺序：状态标签(x=0) → 连接按钮(x=250) → 刷新按钮(x=280)
        # self.serial_status_label.place(x=10, y=2)
        # self.serial_connect_btn.place(x=150, y=0)
        
        # 初始化串口自动扫描变量
        self.auto_scan_active = True
        self.last_port_count = 0
        
        # 初始化串口列表（在程序启动时）
        self.update_port_list()
        
        # 启动串口自动扫描（未连接时）
        self.start_auto_port_scan()
        
        # ==================== 串口模式专用框（使用 place 布局） ====================
        _info_lbl_w = 9
        _info_lbl_padx = (0, 4)
        _info_entry_padx = (0, 3)
        _device_info_h = 115
        _param_config_y = 60 + _device_info_h + 10
        # 设备信息框（在连接设置框下方，y=100 开始）
        self.device_info_frame = ttk.LabelFrame(left_frame, text="设备信息", padding=2)
        self.device_info_frame.place(x=0, y=60, relwidth=1.0, height=_device_info_h)
        self.device_info_frame.place_forget()  # 默认隐藏
        
        # 第一行：SN (y=5)
        sn_frame = ttk.Frame(self.device_info_frame)
        sn_frame.place(x=2, y=0, relwidth=1.0, width=-2, height=30)
        ttk.Label(sn_frame, text="SN:", width=_info_lbl_w, anchor="e", font=("Microsoft YaHei", 9)).pack(side=tk.LEFT, padx=_info_lbl_padx)
        self.device_sn_var = tk.StringVar()
        self.device_sn_entry = ttk.Entry(sn_frame, width=30, textvariable=self.device_sn_var)
        self.device_sn_entry.pack(side=tk.LEFT, padx=_info_entry_padx)
        ttk.Button(sn_frame, text="设置", command=self.on_set_device_sn, width=8).pack(side=tk.LEFT, padx=3)
        
        # 第二行：硬件版本 (y=40)
        hw_ver_frame = ttk.Frame(self.device_info_frame)
        hw_ver_frame.place(x=2, y=30, relwidth=1.0, width=-2, height=30)
        ttk.Label(hw_ver_frame, text="硬件版本:", width=_info_lbl_w, anchor="e", font=("Microsoft YaHei", 9)).pack(side=tk.LEFT, padx=_info_lbl_padx)
        self.device_hw_ver_var = tk.StringVar()
        self.device_hw_ver_entry = ttk.Entry(hw_ver_frame, width=30, textvariable=self.device_hw_ver_var)
        self.device_hw_ver_entry.pack(side=tk.LEFT, padx=_info_entry_padx)
        ttk.Button(hw_ver_frame, text="设置", command=self.on_set_device_hw_ver, width=8).pack(side=tk.LEFT, padx=3)
        ttk.Button(hw_ver_frame, text="获取", command=self.on_get_device_info, width=8).pack(side=tk.LEFT, padx=3)
        
        # 第三行：软件版本 / 软件日期 / IP / LORA (y=60)，软件版本值与硬件版本 Entry 左对齐
        sw_ver_frame = ttk.Frame(self.device_info_frame)
        sw_ver_frame.place(x=2, y=60, relwidth=1.0, width=-2, height=30)
        _sw_row_y = 5
        _sw_cw = 7
        _sw_grp = 5
        _x = 0
        ttk.Label(sw_ver_frame, text="软件版本:", width=_info_lbl_w, anchor="e", font=("Microsoft YaHei", 9)).place(x=_x, y=_sw_row_y)
        _x = _info_lbl_w * _sw_cw + _info_lbl_padx[1]
        self.device_sw_ver_label = ttk.Label(sw_ver_frame, text="--", width=18, anchor="w", foreground="#009999")
        self.device_sw_ver_label.place(x=_x, y=_sw_row_y)
        _x += 15 * _sw_cw + _sw_grp
        ttk.Label(sw_ver_frame, text="软件日期:", width=8, anchor="e", font=("Microsoft YaHei", 9)).place(x=_x, y=_sw_row_y)
        _x += 8 * _sw_cw + 1
        self.device_sw_date_label = ttk.Label(sw_ver_frame, text="--", width=22, anchor="w", foreground="#CC6600")
        self.device_sw_date_label.place(x=_x, y=_sw_row_y)
        _x += 18 * _sw_cw + _sw_grp
        ttk.Label(sw_ver_frame, text="IP:", width=3, anchor="e", font=("Microsoft YaHei", 9)).place(x=_x, y=_sw_row_y)
        _x += 3 * _sw_cw + 1
        self.device_ip_label = ttk.Label(sw_ver_frame, text="--", width=36, anchor="w", foreground="#9933CC")
        self.device_ip_label.place(x=_x, y=_sw_row_y)
        _x += 36 * _sw_cw + _sw_grp
        ttk.Label(sw_ver_frame, text="LORA:", width=5, anchor="e", font=("Microsoft YaHei", 9)).place(x=_x, y=_sw_row_y)
        _x += 5 * _sw_cw + 1
        self.device_lora_label = ttk.Label(sw_ver_frame, text="--", width=28, anchor="w", foreground="#26C6DA")
        self.device_lora_label.place(x=_x, y=_sw_row_y)
        
        # 参数配置框（在设备信息框下方）
        self.param_config_frame = ttk.LabelFrame(left_frame, text="参数配置", padding=2)
        self.param_config_frame.place(x=0, y=_param_config_y, relwidth=1.0, height=120)
        self.param_config_frame.place_forget()  # 默认隐藏
        
        # 第一行：设备地址 (y=5)
        addr_frame = ttk.Frame(self.param_config_frame)
        addr_frame.place(x=2, y=0, relwidth=1.0, width=-2, height=30)
        ttk.Label(addr_frame, text="设备地址:", width=_info_lbl_w, anchor="e", font=("Microsoft YaHei", 9)).pack(side=tk.LEFT, padx=_info_lbl_padx)
        self.param_addr_var = tk.StringVar()
        self.param_addr_entry = ttk.Entry(addr_frame, width=30, textvariable=self.param_addr_var)
        self.param_addr_entry.pack(side=tk.LEFT, padx=_info_entry_padx)
        ttk.Button(addr_frame, text="设置", command=self.on_set_param_addr, width=8).pack(side=tk.LEFT, padx=3)
        
        # 第二行：灵敏度 (y=40)
        gm_sens_frame = ttk.Frame(self.param_config_frame)
        gm_sens_frame.place(x=2, y=30, relwidth=1.0, width=-2, height=30)
        ttk.Label(gm_sens_frame, text="灵敏度:", width=_info_lbl_w, anchor="e", font=("Microsoft YaHei", 9)).pack(side=tk.LEFT, padx=_info_lbl_padx)
        self.param_gm_sens_var = tk.StringVar()
        self.param_gm_sens_entry = ttk.Entry(gm_sens_frame, width=19, textvariable=self.param_gm_sens_var)
        self.param_gm_sens_entry.pack(side=tk.LEFT, padx=_info_entry_padx)
        ttk.Label(gm_sens_frame, text="cpm/μSv/h", width=10, font=("Microsoft YaHei", 9)).pack(side=tk.LEFT, padx=3)
        ttk.Button(gm_sens_frame, text="设置", command=self.on_set_param_gm_sens, width=8).pack(side=tk.LEFT, padx=0)
        ttk.Button(gm_sens_frame, text="获取", command=self.on_get_param_info, width=8).pack(side=tk.LEFT, padx=3)
        
        # 第三行：功能按钮 (y=75)
        btn_row_frame = ttk.Frame(self.param_config_frame)
        btn_row_frame.place(x=2, y=60, relwidth=1.0, width=-2, height=30)
        ttk.Button(btn_row_frame, text="时间同步", command=self.on_time_sync, width=12).pack(side=tk.LEFT, padx=4, pady=2)
        ttk.Button(btn_row_frame, text="中文界面", command=self.on_set_lang_zh, width=12).pack(side=tk.LEFT, padx=4, pady=2)
        ttk.Button(btn_row_frame, text="英文界面", command=self.on_set_lang_en, width=12).pack(side=tk.LEFT, padx=4, pady=2)
        ttk.Button(btn_row_frame, text="声光测试", command=self.on_ht_test, width=12).pack(side=tk.LEFT, padx=4, pady=2)
        ttk.Button(btn_row_frame, text="模拟数据", command=self.on_simulate_data, width=12).pack(side=tk.LEFT, padx=4, pady=2)
        ttk.Button(btn_row_frame, text="打印数据", command=self.on_print_data, width=12).pack(side=tk.LEFT, padx=4, pady=2)
        ttk.Button(btn_row_frame, text="清空数据", command=self.on_clear_data, width=12).pack(side=tk.LEFT, padx=4, pady=2)
        ttk.Button(btn_row_frame, text="恢复出厂设置", command=self.on_factory_reset, width=12).pack(side=tk.LEFT, padx=4, pady=2)
        
        # 协议指令框（含快捷指令按钮，y=60）
        _CMD_FRAME_Y = 60
        _CMD_ROW_GAP = 3   # 各行之间额外行间距（像素）
        _CMD_FRAME_H = 147 + _CMD_ROW_GAP * 3
        _CUSTOM_FRAME_Y = _CMD_FRAME_Y + _CMD_FRAME_H
        _REG_DISPLAY_Y = _CUSTOM_FRAME_Y + 78 + 1
        self._cmd_frame_height = _CMD_FRAME_H
        self._custom_frame_y = _CUSTOM_FRAME_Y
        self._reg_display_y = _REG_DISPLAY_Y

        self.cmd_frame = ttk.LabelFrame(left_frame, text="协议指令", padding=1)
        self.cmd_frame.place(x=0, y=_CMD_FRAME_Y, relwidth=1.0, height=_CMD_FRAME_H)
        
        _CMD_LEFT_PADX = 5
        
        ack_row = ttk.Frame(self.cmd_frame)
        ack_row.pack(fill=tk.X, side=tk.TOP, anchor='w', pady=(1, 2 + _CMD_ROW_GAP))
        self.auto_ack_var = tk.BooleanVar(value=True)
        self._auto_ack_enabled = True
        self._ota_auto_ack_restore = None
        self.auto_ack_checkbtn = ttk.Checkbutton(
            ack_row,
            text="指令应答",
            variable=self.auto_ack_var,
            command=self._on_auto_ack_toggle,
        )
        self.auto_ack_checkbtn.pack(side=tk.LEFT, padx=(_CMD_LEFT_PADX, 0), anchor='w')

        self.tcp_log_debug_var = tk.BooleanVar(value=True)
        self.tcp_log_tx_var = tk.BooleanVar(value=True)
        self.tcp_log_rx_var = tk.BooleanVar(value=True)
        self._tcp_log_debug_enabled = True
        self._tcp_log_tx_enabled = True
        self._tcp_log_rx_enabled = True
        ttk.Checkbutton(
            ack_row,
            text="调试信息",
            variable=self.tcp_log_debug_var,
            command=self._on_tcp_log_toggle,
        ).pack(side=tk.LEFT, padx=(8, 0), anchor='w')
        ttk.Checkbutton(
            ack_row,
            text="发送信息",
            variable=self.tcp_log_tx_var,
            command=self._on_tcp_log_toggle,
        ).pack(side=tk.LEFT, padx=(6, 0), anchor='w')
        ttk.Checkbutton(
            ack_row,
            text="接收信息",
            variable=self.tcp_log_rx_var,
            command=self._on_tcp_log_toggle,
        ).pack(side=tk.LEFT, padx=(6, 0), anchor='w')

        ttk.Label(ack_row, text="单帧最大字节:", font=("Microsoft YaHei", 9)).pack(
            side=tk.LEFT, padx=(12, 4))
        self.ota_packet_max_var = tk.StringVar(value="224")
        self.ota_packet_max_128_radio = ttk.Radiobutton(
            ack_row, text="128", variable=self.ota_packet_max_var, value="128")
        self.ota_packet_max_128_radio.pack(side=tk.LEFT, padx=2)
        self.ota_packet_max_224_radio = ttk.Radiobutton(
            ack_row, text="224", variable=self.ota_packet_max_var, value="224")
        self.ota_packet_max_224_radio.pack(side=tk.LEFT, padx=2)
        self.ota_packet_max_widgets = [
            self.ota_packet_max_128_radio,
            self.ota_packet_max_224_radio,
        ]
        
        quick_btn_padx = 8
        quick_rows = [
            [("时间同步", "timesync"), ("获取记录", "hist5min"), ("设备重启", "reboot")],
        ]
        self.quick_buttons = []
        for row_idx, row_items in enumerate(quick_rows):
            quick_row = ttk.Frame(self.cmd_frame)
            quick_row.pack(fill=tk.X, side=tk.TOP, anchor='w', pady=(3 if row_idx == 0 else 1, 1 + _CMD_ROW_GAP))
            for col_idx, (text, cmd_type) in enumerate(row_items):
                btn = ttk.Button(
                    quick_row, text=text, width=10,
                    command=lambda t=cmd_type: self.send_quick_cmd(t),
                )
                btn.pack(
                    side=tk.LEFT,
                    padx=(_CMD_LEFT_PADX if col_idx == 0 else quick_btn_padx, quick_btn_padx),
                    pady=1, anchor='w',
                )
                self.quick_buttons.append(btn)

        cmd_inner = ttk.Frame(self.cmd_frame)
        cmd_inner.pack(fill=tk.X, side=tk.TOP, anchor='w', pady=(1, 1 + _CMD_ROW_GAP))

        _CMD_DEV_ADDR_GAP = 7
        ttk.Label(cmd_inner, text="设备地址:", font=("Microsoft YaHei", 9)).pack(side=tk.LEFT, padx=(29, _CMD_DEV_ADDR_GAP))
        self.cmd_dev_addr_entry = ttk.Entry(cmd_inner, width=5, font=("Microsoft YaHei", 9))
        self.cmd_dev_addr_entry.insert(0, "0x01")
        self.cmd_dev_addr_entry.pack(side=tk.LEFT, padx=_CMD_DEV_ADDR_GAP)
        cmd_dev_addr_btn = ttk.Button(cmd_inner, text="设置", width=6, command=self.send_cmd_dev_addr)
        cmd_dev_addr_btn.pack(side=tk.LEFT, padx=3)
        self.quick_buttons.append(cmd_dev_addr_btn)

        ttk.Label(cmd_inner, text="报警音量:", font=("Microsoft YaHei", 9)).pack(side=tk.LEFT, padx=(102, _CMD_DEV_ADDR_GAP))
        self.cmd_alarm_volume_entry = ttk.Entry(cmd_inner, width=5, font=("Microsoft YaHei", 9))
        self.cmd_alarm_volume_entry.insert(0, "80")
        self.cmd_alarm_volume_entry.pack(side=tk.LEFT, padx=_CMD_DEV_ADDR_GAP)
        ttk.Label(cmd_inner, text="%", font=("Microsoft YaHei", 9)).pack(side=tk.LEFT, padx=(0, 3))
        cmd_alarm_volume_btn = ttk.Button(cmd_inner, text="设置", width=6, command=self.send_cmd_alarm_volume)
        cmd_alarm_volume_btn.pack(side=tk.LEFT, padx=3)
        self.quick_buttons.append(cmd_alarm_volume_btn)

        self.cmd_dev_addr_widgets = [self.cmd_dev_addr_entry, self.cmd_alarm_volume_entry]
        
        self.dose_thr_widgets = []
        dose_thr_row = ttk.Frame(self.cmd_frame)
        dose_thr_row.pack(fill=tk.X, side=tk.TOP, anchor='w', pady=(2, 0))

        _DOSE_THR_GAP = 5

        ttk.Label(dose_thr_row, text="剂量率上阈值:", font=("Microsoft YaHei", 9)).pack(side=tk.LEFT, padx=(_CMD_LEFT_PADX, _DOSE_THR_GAP))
        self.dose_hi_entry = ttk.Entry(dose_thr_row, width=6, font=("Microsoft YaHei", 9))
        self.dose_hi_entry.insert(0, "2.50")
        self.dose_hi_entry.pack(side=tk.LEFT, padx=_DOSE_THR_GAP)
        self.dose_hi_unit = ttk.Combobox(
            dose_thr_row, width=5, values=["μSv/h", "mSv/h"], state="readonly", font=("Microsoft YaHei", 9),
        )
        self.dose_hi_unit.set("μSv/h")
        self.dose_hi_unit.pack(side=tk.LEFT, padx=_DOSE_THR_GAP)
        dose_hi_btn = ttk.Button(dose_thr_row, text="设置", width=6, command=self.send_dose_hi_threshold)
        dose_hi_btn.pack(side=tk.LEFT, padx=(_DOSE_THR_GAP, _DOSE_THR_GAP + 5))
        self.quick_buttons.append(dose_hi_btn)

        ttk.Label(dose_thr_row, text="剂量率下阈值:", font=("Microsoft YaHei", 9)).pack(side=tk.LEFT, padx=(0, _DOSE_THR_GAP))
        self.dose_lo_entry = ttk.Entry(dose_thr_row, width=6, font=("Microsoft YaHei", 9))
        self.dose_lo_entry.insert(0, "0.00")
        self.dose_lo_entry.pack(side=tk.LEFT, padx=_DOSE_THR_GAP)
        self.dose_lo_unit = ttk.Combobox(
            dose_thr_row, width=5, values=["μSv/h", "mSv/h"], state="readonly", font=("Microsoft YaHei", 9),
        )
        self.dose_lo_unit.set("μSv/h")
        self.dose_lo_unit.pack(side=tk.LEFT, padx=_DOSE_THR_GAP)
        dose_lo_btn = ttk.Button(dose_thr_row, text="设置", width=6, command=self.send_dose_lo_threshold)
        dose_lo_btn.pack(side=tk.LEFT, padx=_DOSE_THR_GAP)
        self.quick_buttons.append(dose_lo_btn)

        ttk.Label(dose_thr_row, text="报警:", font=("Microsoft YaHei", 9)).pack(side=tk.LEFT, padx=(_DOSE_THR_GAP + 5, _DOSE_THR_GAP))
        self.alarm_type_combo = ttk.Combobox(
            dose_thr_row,
            width=11,
            values=["屏幕", "声报警", "光报警", "剂量率上阈值", "剂量率下阈值"],
            state="readonly",
            font=("Microsoft YaHei", 9),
        )
        self.alarm_type_combo.set("声报警")
        self.alarm_type_combo.pack(side=tk.LEFT, padx=_DOSE_THR_GAP)
        self.alarm_state_combo = ttk.Combobox(
            dose_thr_row, width=4, values=["打开", "关闭"], state="readonly", font=("Microsoft YaHei", 9),
        )
        self.alarm_state_combo.set("打开")
        self.alarm_state_combo.pack(side=tk.LEFT, padx=_DOSE_THR_GAP)
        alarm_set_btn = ttk.Button(dose_thr_row, text="设置", width=6, command=self.send_alarm_setting)
        alarm_set_btn.pack(side=tk.LEFT, padx=_DOSE_THR_GAP)
        self.quick_buttons.append(alarm_set_btn)

        self.dose_thr_widgets = [
            self.dose_hi_entry, self.dose_hi_unit, self.dose_lo_entry, self.dose_lo_unit,
        ]
        self.alarm_setting_widgets = [
            self.alarm_type_combo, self.alarm_state_combo,
        ]
        
        # 自定义指令框（紧接协议指令框下方）
        self.custom_frame = ttk.LabelFrame(left_frame, text="自定义指令", padding=2)
        self.custom_frame.place(x=0, y=_CUSTOM_FRAME_Y, relwidth=1.0, height=78)
        
        custom_inner = ttk.Frame(self.custom_frame)
        custom_inner.pack(fill=tk.X, side=tk.LEFT, anchor='w')

        ttk.Label(custom_inner, text="功能码:", font=("Microsoft YaHei", 9)).pack(side=tk.LEFT, padx=(2, 5), anchor='w')
        self.func_combo = ttk.Combobox(custom_inner, width=15, values=[
            '0x03 - 读多寄存器',
            '0x05 - 读单寄存器',
            '0x06 - 写单寄存器',
            '0x10 - 写多寄存器',
            '0x13 - 读多应答',
            '0x15 - 读单应答',
            '0x23 - 多字节上传',
            '0x25 - 单字节上传',
        ])
        self.func_combo.set('0x03 - 读多寄存器')
        self.func_combo.pack(side=tk.LEFT, padx=5, anchor='w')

        ttk.Label(custom_inner, text="寄存器地址:", font=("Microsoft YaHei", 9)).pack(side=tk.LEFT, padx=(5, 5), anchor='w')
        self.reg_addr_entry = ttk.Entry(custom_inner, width=5)
        self.reg_addr_entry.insert(0, "1")
        self.reg_addr_entry.pack(side=tk.LEFT, padx=5, anchor='w')

        ttk.Label(custom_inner, text="值/数量:", font=("Microsoft YaHei", 9)).pack(side=tk.LEFT, padx=(5, 5), anchor='w')
        self.reg_val_entry = ttk.Entry(custom_inner, width=5)
        self.reg_val_entry.insert(0, "0")
        self.reg_val_entry.pack(side=tk.LEFT, padx=5, anchor='w')

        self.cmd_send_button = ttk.Button(custom_inner, text="发送", command=self.send_custom_cmd, width=6)
        self.cmd_send_button.pack(side=tk.LEFT, padx=(3, 15), anchor='w')
        
        ttk.Label(custom_inner, text="HEX:").pack(side=tk.LEFT, padx=2, anchor='w')
        self.hex_entry = ttk.Entry(custom_inner, width=36)
        self.hex_entry.insert(0, "01 03 01 00 02 00 45 56")
        self.hex_entry.pack(side=tk.LEFT, padx=2, fill=tk.X, expand=True, anchor='w')
        
        ttk.Button(custom_inner, text="发送", 
                    command=self.send_raw_frame, width=6).pack(side=tk.LEFT, padx=3, anchor='w')
        
        # 保存自定义指令按钮引用
        self.custom_send_button = custom_inner.winfo_children()[-1]
        
        # 寄存器表数据显示框（固定布局，不可上下滚动）
        self.reg_display_frame = ttk.LabelFrame(left_frame, text="寄存器表数据", padding=2)
        self.reg_display_frame.place(x=0, y=_REG_DISPLAY_Y, relwidth=1.0, height=215)

        self.reg_scrollable_frame = ttk.Frame(self.reg_display_frame)
        self.reg_scrollable_frame.pack(fill="both", expand=True)
        
        # 初始化寄存器表（每列 4 个，自动分多列）
        self.init_register_table()
        
        # 报警状态框（使用 place 布局，y=830 开始）
        self.alarm_status_frame = ttk.LabelFrame(left_frame, text="报警状态", padding=2)
        self.alarm_status_frame.place(x=0, y=_REG_DISPLAY_Y + 215, relwidth=1.0, height=215)
        
        # 报警状态显示（使用 Frame 容器 + 标签，8 列显示，每列 4 个）
        # 计算高度：每列 4 个参数框，每个约 42 像素（边框 18 + 字体 20 + padding 4）
        # 4 × 42 = 168 像素，加上 Canvas 边距和滚动空间，设置为 190 像素
        alarm_canvas = tk.Canvas(self.alarm_status_frame, bg=self.root.cget('bg'), highlightthickness=0, height=190)
        self.alarm_scrollable_frame = tk.Frame(alarm_canvas, bg=self.root.cget('bg'))
        
        self.alarm_scrollable_frame.bind(
            "<Configure>",
            lambda e: alarm_canvas.configure(scrollregion=alarm_canvas.bbox("all"))
        )
        
        alarm_canvas.create_window((0, 0), window=self.alarm_scrollable_frame, anchor="nw")
        
        alarm_canvas.pack(fill="both", expand=True)
        
        # 初始化报警状态标签字典
        self.alarm_labels = {}
        
        # 自定义数据框（UDP 模式专用）
        self.custom_data_frame = ttk.LabelFrame(left_frame, text="自定义数据", padding=2)
        # 初始化时不显示，由 on_mode_changed() 控制显示
        # self.custom_data_frame.place(x=0, y=216, relwidth=1.0, height=78)
        
        custom_data_inner = ttk.Frame(self.custom_data_frame)
        custom_data_inner.pack(fill=tk.X, side=tk.LEFT, anchor='w')
        
        ttk.Label(custom_data_inner, text="DATA:").pack(side=tk.LEFT, padx=2, anchor='w')
        self.udp_data_entry = ttk.Entry(custom_data_inner, width=80)
        self.udp_data_entry.insert(0, "RAW-I-V2026,1904RAW0203,192.168.2.28,5001,5000,4,0,0")
        self.udp_data_entry.pack(side=tk.LEFT, padx=2, fill=tk.X, expand=True, anchor='w')
        
        # 发送按钮（OTA 后台传输时仍应保持可用）
        self.udp_send_btn = ttk.Button(custom_data_inner, text="发送",
                                       command=self.send_udp_data)
        self.udp_send_btn.pack(side=tk.LEFT, padx=3, anchor='w')
        
        # 模拟设备按钮
        self.udp_simulate_btn = ttk.Button(custom_data_inner, text="模拟设备",
                                           command=self.simulate_device)
        self.udp_simulate_btn.pack(side=tk.LEFT, padx=3, anchor='w')
        
        # 设备状态框（使用 place 布局，y=1030 开始）
        self.device_status_frame = ttk.LabelFrame(left_frame, text="设备状态", padding=2)
        self.device_status_frame.place(x=0, y=_REG_DISPLAY_Y + 215 + 215, relwidth=1.0, height=125)
        
        # 设备状态显示（使用 Frame 容器 + 标签，8 列显示，每列 2 个）
        # 计算高度：每列 2 个参数框，每个约 42 像素（边框 18 + 字体 20 + padding 4）
        # 2 × 42 = 84 像素，加上 Canvas 边距和滚动空间，设置为 100 像素
        device_canvas = tk.Canvas(self.device_status_frame, bg=self.root.cget('bg'), highlightthickness=0, height=100)
        self.device_scrollable_frame = tk.Frame(device_canvas, bg=self.root.cget('bg'))
        
        self.device_scrollable_frame.bind(
            "<Configure>",
            lambda e: device_canvas.configure(scrollregion=device_canvas.bbox("all"))
        )
        
        device_canvas.create_window((0, 0), window=self.device_scrollable_frame, anchor="nw")
        
        device_canvas.pack(fill="both", expand=True)
        
        # 初始化设备状态标签字典
        self.device_labels = {}
        
        # 初始化显示报警状态和设备状态（初始值为"-"）
        self._init_alarm_display()
        self._alarm_initialized = True
        self._init_device_display()
        self._device_initialized = True
        
        # 固件更新框（使用 place 布局，y=1135 开始）
        self.firmware_frame = ttk.LabelFrame(left_frame, text="固件更新", padding=0)
        self.firmware_frame.place(x=0, y=_REG_DISPLAY_Y + 215 + 215 + 125, relwidth=1.0, height=90)
        
        # 第一行：IAP 文件 + APP 文件 + 浏览按钮 + 模式选项
        file_frame = ttk.Frame(self.firmware_frame)
        file_frame.pack(fill=tk.X, pady=2)
        
        # IAP 文件选择
        ttk.Label(file_frame, text="IAP 文件:", width=8).pack(side=tk.LEFT, padx=2)
        self.iap_path_var = tk.StringVar()
        self.iap_path_entry = ttk.Entry(file_frame, textvariable=self.iap_path_var, justify=tk.RIGHT)
        self.iap_path_entry.pack(side=tk.LEFT, padx=2, fill=tk.X, expand=True)
        
        self.iap_browse_btn = ttk.Button(file_frame, text="···", command=self.browse_iap_file, width=3)
        self.iap_browse_btn.pack(side=tk.LEFT, padx=5)
        
        # APP 文件选择
        ttk.Label(file_frame, text="APP 文件:", width=8).pack(side=tk.LEFT, padx=2)
        self.app_path_var = tk.StringVar()
        self.app_path_entry = ttk.Entry(file_frame, textvariable=self.app_path_var, justify=tk.RIGHT)
        self.app_path_entry.pack(side=tk.LEFT, padx=2, fill=tk.X, expand=True)
        
        self.app_browse_btn = ttk.Button(file_frame, text="···", command=self.browse_app_file, width=3)
        self.app_browse_btn.pack(side=tk.LEFT, padx=5)
        
        # TCP/BootLoader 选项（RadioButton）
        ttk.Label(file_frame, text="模式:").pack(side=tk.LEFT, padx=5)
        self.ota_mode_var = tk.StringVar(value="TCP")
        tcp_radio = ttk.Radiobutton(file_frame, text="TCP", variable=self.ota_mode_var, value="TCP")
        tcp_radio.pack(side=tk.LEFT, padx=2)
        bootloader_radio = ttk.Radiobutton(file_frame, text="BootLoader", variable=self.ota_mode_var, value="BootLoader")
        bootloader_radio.pack(side=tk.LEFT, padx=2)
        
        # 第二行：进度条 + 三个更新按钮
        progress_frame = ttk.Frame(self.firmware_frame)
        progress_frame.pack(fill=tk.X, pady=2)
        
        ttk.Label(progress_frame, text="进度:", width=5).pack(side=tk.LEFT, padx=2)
        self.firmware_progress = ttk.Progressbar(progress_frame, mode='determinate', length=490)
        self.firmware_progress.pack(side=tk.LEFT, padx=2, fill=tk.X, expand=True)
        
        self.progress_label = ttk.Label(progress_frame, text="0.0%", width=6)
        self.progress_label.pack(side=tk.LEFT, padx=5)
        
        # IAP 更新、APP 更新、停止更新按钮
        self.iap_update_btn = ttk.Button(progress_frame, text="IAP 更新", command=self.start_iap_update, width=10)
        self.iap_update_btn.pack(side=tk.LEFT, padx=5)
        
        self.app_update_btn = ttk.Button(progress_frame, text="APP 更新", command=self.start_app_update, width=10)
        self.app_update_btn.pack(side=tk.LEFT, padx=5)
        
        self.stop_update_btn = ttk.Button(progress_frame, text="停止更新", command=self.user_stop_update, width=10)
        self.stop_update_btn.pack(side=tk.LEFT, padx=5)
        
        # 根据当前模式设置按钮状态
        self._update_ota_buttons_state()
        
        # 固件更新状态
        self.firmware_update_active = False
        self.iap_update_active = False
        self.app_update_active = False
        self._ota_device_addr = None
        self._ota_reconnect_grace_until = 0.0
        self._ota_resume_after_reconnect = False
        
        # BootLoader 模式 DFU 更新相关常量
        self.BOOTLOADER_IAP_START_ADDR = 0x08000000  # IAP 更新起始地址
        self.BOOTLOADER_APP_START_ADDR = 0x08020000  # APP 更新起始地址
        self.BOOTLOADER_BLOCK_SIZE = 1024  # 写入块大小
        self.BOOTLOADER_CHUNK_SIZE = 64    # 串口传输块大小
        self.BOOTLOADER_FLASH_PAGE_SIZE = 0x20000  # Flash 页大小（128KB）

        # TCP OTA 状态等待（无设备上报时的超时）
        self.OTA_STATUS_TIMEOUT_S = 40.0
        self.OTA_START_WAIT_S = 40.0
        self.OTA_RECONNECT_GRACE_S = 40.0
        
        # BootLoader 命令定义
        self.BL_CMD_GET = 0x00
        self.BL_CMD_GO = 0x21
        self.BL_CMD_WRITE = 0x31
        self.BL_CMD_ERASE_EXT = 0x44
        self.BL_CMD_ACK = 0x79
        self.BL_CMD_NACK = 0x1F
        
        # BootLoader 更新状态
        self.bootloader_update_thread = None
        self.bootloader_update_aborted = False
        self.BOOTLOADER_MAX_ATTEMPTS = 3
        self._BL_CANCEL = "__bl_cancel__"
        self._BL_SERIAL_LOST = "__bl_serial_lost__"
        
        # USB DFU 设备检测相关
        self.use_usb_dfu = False  # 是否使用 USB DFU 模式
        self.usb_handle = None
        self.libusb_context = None
        
        # 绑定模式变化事件
        self.ota_mode_var.trace('w', lambda *args: self._update_ota_buttons_state())
        
        # 设置右侧日志区域
        self.setup_right_panel()
    
    def _update_ota_buttons_state(self):
        """根据当前模式更新 OTA 按钮状态"""
        mode = self.ota_mode_var.get()
        if mode == "TCP":
            # TCP 模式：禁用 IAP 更新按钮，启用 APP 更新按钮
            self.iap_update_btn.config(state=tk.DISABLED)
            self.app_update_btn.config(state=tk.NORMAL)
        else:  # BootLoader
            # BootLoader 模式：启用所有按钮
            self.iap_update_btn.config(state=tk.NORMAL)
            self.app_update_btn.config(state=tk.NORMAL)
    
    def start_auto_port_scan(self):
        """启动串口自动扫描（未连接时）"""
        self._scan_ports_if_needed()
    
    def _scan_ports_if_needed(self):
        """扫描串口并在需要时更新下拉列表"""
        if not self.auto_scan_active:
            return
        
        # 只在串口模式下扫描
        mode = self.mode_var.get().split(' - ')[0].lower()
        if mode == "serial":
            try:
                ports = self.serial_monitor.scan_ports()
                current_count = len(ports)
                
                # 如果端口数量变化，更新下拉列表
                if current_count != self.last_port_count:
                    self.update_port_list()
                    self.last_port_count = current_count
            except Exception as e:
                print(f"自动扫描串口失败：{e}")
        
        # 100ms 后再次扫描
        self.root.after(100, self._scan_ports_if_needed)
    
    def stop_auto_port_scan(self):
        """停止串口自动扫描"""
        self.auto_scan_active = False
    
    def setup_right_panel(self):
        """设置右侧日志区域"""
        # 右侧日志区域（调试信息/接收/发送）
        log_paned = tk.PanedWindow(self.right_frame, orient=tk.VERTICAL, sashrelief=tk.RAISED, sashwidth=4)
        log_paned.pack(fill=tk.BOTH, expand=True)
        
        # 调试信息框（顶部）
        debug_container = ttk.Frame(log_paned)
        log_paned.add(debug_container, height=200)
        
        debug_frame = ttk.LabelFrame(debug_container, text="调试信息", padding=3)
        debug_frame.pack(fill=tk.BOTH, expand=True)
        self.debug_text = scrolledtext.ScrolledText(
            debug_frame,
            wrap=tk.WORD,
            font=("Consolas", 9),
            foreground="#0000AA",
            insertbackground="#0000AA",
        )
        self.debug_text.pack(fill=tk.BOTH, expand=True)
        
        # 接收框
        rx_container = ttk.Frame(log_paned)
        log_paned.add(rx_container, height=300)  # 增加高度从 250 到 300
        
        rx_frame = ttk.LabelFrame(rx_container, text="接收（设备 → 本机）", padding=3)
        rx_frame.pack(fill=tk.BOTH, expand=True)
        self.rx_text = scrolledtext.ScrolledText(
            rx_frame,
            wrap=tk.WORD,
            font=("Consolas", 9),
            foreground="#1B5E20",
            insertbackground="#1B5E20",
        )
        self.rx_text.pack(fill=tk.BOTH, expand=True)
        
        # 发送框
        tx_container = ttk.Frame(log_paned)
        log_paned.add(tx_container, height=150)  # 减少高度从 200 到 150
        
        tx_frame = ttk.LabelFrame(tx_container, text="发送（本机 → 设备）", padding=3)
        tx_frame.pack(fill=tk.BOTH, expand=True)
        self.tx_text = scrolledtext.ScrolledText(
            tx_frame,
            wrap=tk.WORD,
            font=("Consolas", 9),
            foreground="#BF360C",
            insertbackground="#BF360C",
        )
        self.tx_text.pack(fill=tk.BOTH, expand=True)
        
        # 历史记录框（发送框下方）
        history_container = ttk.Frame(log_paned)
        log_paned.add(history_container, height=72)
        
        history_btn_frame = ttk.Frame(history_container)
        history_btn_frame.pack(side=tk.BOTTOM, fill=tk.X, pady=(2, 0))
        ttk.Button(history_btn_frame, text="清空调试", command=self.clear_debug).pack(side=tk.LEFT, padx=3)
        ttk.Button(history_btn_frame, text="清空接收", command=self.clear_rx).pack(side=tk.LEFT, padx=3)
        ttk.Button(history_btn_frame, text="清空发送", command=self.clear_tx).pack(side=tk.LEFT, padx=3)
        ttk.Button(history_btn_frame, text="清空历史", command=self.clear_history).pack(side=tk.LEFT, padx=3)
        
        history_frame = ttk.LabelFrame(history_container, text="历史记录", padding=3)
        history_frame.pack(fill=tk.BOTH, expand=True)
        self.history_text = scrolledtext.ScrolledText(
            history_frame,
            height=3,
            wrap=tk.NONE,
            font=("Consolas", 9),
            foreground="#000000",
            insertbackground="#000000",
        )
        self.history_text.pack(fill=tk.BOTH, expand=True)
        
        # 配置调试文本颜色标签（每帧不同颜色）
        self.debug_text.tag_config('black', foreground='#000000')
        for color in self.rx_colors:
            self.debug_text.tag_config('debug_color_' + color, foreground=color)
            self.history_text.tag_config('history_color_' + color, foreground=color)

        # 所有控件创建完成后再应用默认模式（TCP）布局
        self.on_mode_changed()
    
    def init_register_table(self):
        """初始化寄存器表 - 每列固定 4 个，自动分多列"""
        # 定义所有寄存器（地址，显示名称，转换函数）- 按照协议地址排序
        self.registers = [
            # 只读寄存器（传感器数据）- 地址 1-21（32 位数据，每个占用 2 个寄存器地址）
            (1, '实时剂量率', NetRawProtocol.format_dose_rate_reg),  # uint32, uSv/h*100
            (3, '实时温度', lambda v: f"{v/10.0:.1f} ℃"),  # int32, ℃*10
            (5, '实时气压', lambda v: f"{v/100.0:.1f} hPa"),  # uint32, Pa (转换为 hPa 显示)
            (7, '实时湿度', lambda v: f"{v:.1f} %"),  # uint32, % (直接上传，不需要缩放)
            (9, '实时 CO2', lambda v: f"{v:.0f} ppm"),  # uint32, ppm (直接上传，不需要缩放)
            (11, '实时 PM2.5', lambda v: f"{v/10.0:.1f} μg/m³"),  # uint32, μg/m³*10
            (13, '报警状态', lambda v: f"0x{v:08X}"),  # uint32, 32 位标志
            (15, '设备状态', lambda v: f"0x{v:08X}"),  # uint32, 32 位标志
            
            # 配置参数 - 辐射剂量率阈值 - 地址 50-53（32 位数据，单位固定为 uSv/h*100）
            (50, '剂量率上阈值', NetRawProtocol.format_dose_threshold_reg),  # uint32, uSv/h*100
            (52, '剂量率下阈值', NetRawProtocol.format_dose_threshold_reg),  # uint32, uSv/h*100
            
            # 配置参数 - 温度阈值 - 地址 54-57（32 位数据）
            (54, '温度上阈值', lambda v: f"{v/10.0:.1f} ℃"),  # uint32, ℃*10
            (56, '温度下阈值', lambda v: f"{v/10.0:.1f} ℃"),  # uint32, ℃*10
            
            # 配置参数 - 气压阈值 - 地址 58-61（32 位数据）
            (58, '气压上阈值', lambda v: f"{v/100.0:.1f} hPa"),  # uint32, Pa (转换为 hPa 显示)
            (60, '气压下阈值', lambda v: f"{v/100.0:.1f} hPa"),  # uint32, Pa (转换为 hPa 显示)
            
            # 配置参数 - 湿度阈值 - 地址 62-65（32 位数据）
            (62, '湿度上阈值', lambda v: f"{v/10.0:.1f} %"),  # uint32, %*10
            (64, '湿度下阈值', lambda v: f"{v/10.0:.1f} %"),  # uint32, %*10
            
            # 配置参数 - CO2 阈值 - 地址 66-69（32 位数据）
            (66, 'CO2 上阈值', lambda v: f"{v/100.0:.0f} ppm"),  # uint32, ppm*100
            (68, 'CO2 下阈值', lambda v: f"{v/100.0:.0f} ppm"),  # uint32, ppm*100
            
            # 配置参数 - PM2.5 阈值 - 地址 70-73（32 位数据）
            (70, 'PM2.5 上阈值', lambda v: f"{v/100.0:.0f} μg/m³"),  # uint32, ug/m³*100
            (72, 'PM2.5 下阈值', lambda v: f"{v/100.0:.0f} μg/m³"),  # uint32, ug/m³*100
            
            # 配置参数 - 报警使能 - 地址 82-83（32 位数据）
            (82, '报警使能', NetRawProtocol.format_alarm_biten),  # uint32, 32 位标志
            
            # 配置参数 - 设备控制 - 地址 121-122（16 位数据）
            (121, '设备地址', NetRawProtocol.format_dev_addr),  # uint16
            (122, '报警音量', lambda v: f"{v}%"),  # uint16, 0-100
        ]
        
        self._threshold_zero_alarms = set()
        self._last_alarm_status = 0
        
        # 清空之前的显示
        for widget in self.reg_scrollable_frame.winfo_children():
            widget.destroy()
        
        # 每列固定 4 个，自动分多列
        items_per_col = 4
        total_items = len(self.registers)
        num_cols = (total_items + items_per_col - 1) // items_per_col
        
        # 创建多列
        for col_idx in range(num_cols):
            col_frame = ttk.Frame(self.reg_scrollable_frame)
            col_frame.grid(row=0, column=col_idx, padx=1, pady=1, sticky="n")
            
            start_idx = col_idx * items_per_col
            end_idx = min(start_idx + items_per_col, total_items)
            
            for i in range(start_idx, end_idx):
                addr, name, convert_fn = self.registers[i]
                label_frame = ttk.LabelFrame(col_frame, text=name, padding=2)
                label_frame.pack(fill=tk.X, pady=0, ipadx=1, ipady=0, anchor="center")
                
                value_label = ttk.Label(label_frame, text="-", font=("Microsoft YaHei", 9), foreground="#000000")
                value_label.pack(anchor=tk.W, padx=2, pady=0)
                
                # 保存标签引用
                self.register_values[addr] = {
                    'label': value_label,
                    'convert': convert_fn,
                    'name': name,
                    'default_color': "#000000",  # 默认黑色（无数据）
                    'data_color': "#FF8800"      # 浅橙色（有数据）
                }
        
        # 自动调整高度
        self.reg_scrollable_frame.update_idletasks()
    
    def _reset_tcp_addr_display(self) -> None:
        """断开连接或未识别时显示 Auto"""
        self._tcp_identified_addr = None
        if hasattr(self, 'tcp_addr_var'):
            self.tcp_addr_var.set("Auto")
        if hasattr(self, 'tcp_addr_label'):
            self.tcp_addr_label.config(foreground=self._TCP_ADDR_COLOR_AUTO)
        self._refresh_tcp_parse_cache()
    
    def _tcp_auto_identify_addr(self):
        """TCP 连接后主动读取设备地址"""
        if not self.tcp_connected:
            return
        addr = self._get_tcp_device_addr(quiet=True)
        if addr is None:
            # 还没有识别到地址，发送读取指令
            frame = NetRawProtocol.build_read_single(0, 254)  # 读 reg254 设备地址
            self.send_frame(frame, "[识别] 读取设备地址 reg254")

    def _set_tcp_addr_display(self, addr: int) -> None:
        """识别到设备地址后更新连接设置标签"""
        if not (0 <= addr <= 255):
            return
        self._tcp_identified_addr = addr
        if hasattr(self, 'tcp_addr_var'):
            self.tcp_addr_var.set(f"0x{addr:02X}")
        if hasattr(self, 'tcp_addr_label'):
            self.tcp_addr_label.config(foreground=self._TCP_ADDR_COLOR_IDENTIFIED)
        self._refresh_tcp_parse_cache()

    def _refresh_tcp_parse_cache(self) -> None:
        """主线程更新 TCP 解析缓存，读线程不访问 Tk 控件"""
        if self._tcp_identified_addr is None:
            self._tcp_parse_expected_addr = -1
        else:
            self._tcp_parse_expected_addr = int(self._tcp_identified_addr)

    def _clear_rx_buffers_safe(self) -> None:
        """清空接收缓冲；持锁带超时，避免与 TCP 读线程死锁"""
        if self.rx_lock.acquire(timeout=0.05):
            try:
                self.rx_buffer = bytearray()
            finally:
                self.rx_lock.release()
        self.serial_line_buffer = bytearray()

    def _is_serial_port_open(self) -> bool:
        """串口物理连接是否可用（不依赖 serial_connected 标志）"""
        return self.serial_monitor.is_open()

    def _normalize_serial_port(self, port_str: Optional[str]) -> str:
        return normalize_port(port_str)

    def _set_port_combo_selection(self, prefer_port: str, port_values: List[str]) -> None:
        """设置串口下拉框选中项，不触发参数变化重连"""
        self._syncing_port_combo = True
        try:
            prefer_port = self._normalize_serial_port(prefer_port)
            if not port_values:
                self.port_combo.set("")
                return
            selected = None
            for item in port_values:
                if self._normalize_serial_port(item) == prefer_port:
                    selected = item
                    break
            self.port_combo.set(selected or port_values[0])
        finally:
            self._syncing_port_combo = False

    def _refresh_serial_port_combo(self, prefer_port: Optional[str] = None) -> None:
        """刷新串口列表并保持指定端口选中（仅主线程调用）"""
        prefer = self._normalize_serial_port(
            prefer_port
            or self._connected_serial_port
            or self.port_combo.get()
        )
        ports = self.serial_monitor.scan_ports()
        port_values = format_port_combo_values(ports)
        self.port_combo["values"] = port_values
        if prefer and any(self._normalize_serial_port(v) == prefer for v in port_values):
            self._set_port_combo_selection(prefer, port_values)
        elif port_values and not self._is_serial_port_open():
            self._set_port_combo_selection(
                self._normalize_serial_port(port_values[0]), port_values
            )
        elif not port_values:
            self._syncing_port_combo = True
            try:
                self.port_combo.set("")
            finally:
                self._syncing_port_combo = False

    def _should_show_channel(self, channel: str) -> bool:
        """当前 UI 模式是否显示该通道的收发/调试信息（读线程可安全调用）"""
        return getattr(self, '_ui_mode_cache', channel) == channel

    def _should_show_tcp_rx_panel(self) -> bool:
        """TCP 模式下是否显示接收窗（读线程可安全调用）"""
        if not self._should_show_channel("tcp"):
            return False
        return getattr(self, '_tcp_log_rx_enabled', True)

    def _should_show_tcp_tx_panel(self) -> bool:
        """TCP 模式下是否显示发送窗"""
        if not self._should_show_channel("tcp"):
            return False
        return getattr(self, '_tcp_log_tx_enabled', True)

    def _infer_log_channel(self, msg: str):
        """从日志前缀推断所属通道；无法推断时返回 None（始终显示）"""
        low = msg.lower()
        if '[tcp' in low or '[ota]' in low or '[自动识别]' in msg:
            return 'tcp'
        if 'tcp 监控' in low or 'tcp监控' in low:
            return 'tcp'
        if '[udp' in low or 'udp 监控' in low:
            return 'udp'
        if ('[串口' in msg or '串口监控' in msg or '[usb dfu]' in low
                or '[bootloader]' in low or '[参数配置]' in msg or '[设备配置]' in msg):
            return 'serial'
        if '[连接监控]' in msg:
            if 'tcp' in low:
                return 'tcp'
            if 'udp' in low:
                return 'udp'
            if '串口' in msg:
                return 'serial'
        return None

    def _is_tcp_related_log(self, msg: str, channel: str = None) -> bool:
        """是否为 TCP/OTA 通道调试信息"""
        if channel == 'tcp':
            return True
        low = msg.lower()
        if '[ota]' in low or '[tcp' in low or 'tcp 监控' in low or 'tcp监控' in low:
            return True
        if '[自动识别]' in msg:
            return True
        return self._infer_log_channel(msg) == 'tcp'

    def _should_show_log(self, msg: str, channel: str = None) -> bool:
        ui_mode = getattr(self, '_ui_mode_cache', 'tcp')
        if ui_mode != 'tcp' and self._is_tcp_related_log(msg, channel):
            return False
        ch = channel or self._infer_log_channel(msg)
        if ch is None:
            return True
        if not self._should_show_channel(ch):
            return False
        if ui_mode == 'tcp' and ch == 'tcp' and not getattr(self, '_tcp_log_debug_enabled', True):
            return False
        return True

    def on_mode_changed(self, event=None):
        """模式切换处理"""
        mode = self.mode_var.get().split(' - ')[0].lower()
        
        # 更新当前模式（读线程通过 _ui_mode_cache 判断显示，不访问 mode_var）
        self.current_mode = mode
        self._ui_mode_cache = mode
        # TCP 仍连接时继续解析/应答，与 UI 模式解耦（避免切到串口后 MCU 5 分钟 ACK 超时）
        if self.tcp_connected:
            self._refresh_tcp_parse_cache()
        
        # 清空缓冲区（TCP 仍连接时保留 rx_buffer，避免半包丢失）
        if self.tcp_connected and mode != "tcp":
            self.serial_line_buffer = bytearray()
        else:
            self._clear_rx_buffers_safe()
        
        # 切换显示对应模式的按钮和标签（使用place布局）
        self.serial_connect_btn.place_forget()
        self.tcp_connect_btn.place_forget()
        self.udp_connect_btn.place_forget()
        self.serial_status_label.place_forget()
        self.tcp_status_label.place_forget()
        self.udp_status_label.place_forget()

        if mode == "serial":
            self.current_connect_btn = self.serial_connect_btn
            self.current_status_label = self.serial_status_label
            # 顺序：状态标签(x=0) → 连接按钮(x=250) → 刷新按钮(x=280)
            self.serial_status_label.place(x=10, y=2)
            self.serial_connect_btn.place(x=145, y=0)
        elif mode == "tcp":
            self.current_connect_btn = self.tcp_connect_btn
            self.current_status_label = self.tcp_status_label
            # 顺序：状态标签(x=0) → 连接按钮(x=250) → 刷新按钮(x=280)
            self.tcp_status_label.place(x=10, y=2)
            self.tcp_connect_btn.place(x=145, y=0)
        elif mode == "udp":
            self.current_connect_btn = self.udp_connect_btn
            self.current_status_label = self.udp_status_label
            # 顺序：状态标签(x=0) → 连接按钮(x=250) → 刷新按钮(x=280)
            self.udp_status_label.place(x=10, y=2)
            self.udp_connect_btn.place(x=145, y=0)
        
        # 显示对应模式的设置框
        # 获取 OTA 状态（安全获取，如果属性不存在默认为 False）
        ota_active = getattr(self, 'firmware_update_active', False)
        
        # 定义所有模式的通用框布局（y 坐标，高度）
        common_frames = {
            'reg_display_frame': (self._reg_display_y, 215),
            'alarm_status_frame': (self._reg_display_y + 215, 215),
            'device_status_frame': (self._reg_display_y + 430, 125),
            'firmware_frame': (self._reg_display_y + 555, 90),
        }
        
        if mode == "serial":
            # 串口模式：显示串口设置框，隐藏 TCP 和 UDP 设置框
            if hasattr(self, 'serial_frame'):
                self.serial_frame.place(x=148, y=5)
            if hasattr(self, 'tcp_frame'):
                self.tcp_frame.place_forget()
            if hasattr(self, 'udp_frame'):
                self.udp_frame.place_forget()
            
            # 显示设备信息框和参数配置框
            if hasattr(self, 'device_info_frame'):
                self.device_info_frame.place(x=0, y=60, relwidth=1.0, height=115)
            if hasattr(self, 'param_config_frame'):
                self.param_config_frame.place(x=0, y=175, relwidth=1.0, height=120)
            
            # 隐藏协议指令框、自定义指令框
            for frame_name in ['cmd_frame', 'custom_frame']:
                frame = getattr(self, frame_name, None)
                if frame:
                    frame.place_forget()
            
            # 隐藏自定义数据框（UDP 模式专用）
            if hasattr(self, 'custom_data_frame'):
                self.custom_data_frame.place_forget()
            
            # 设置通用框的位置
            for frame_name, (y, height) in common_frames.items():
                frame = getattr(self, frame_name, None)
                if frame:
                    frame.place(x=0, y=y, relwidth=1.0, height=height)
            if not ota_active:
                self._set_command_buttons_state(tk.DISABLED)
                
        elif mode == "tcp":
            # TCP 模式：显示 TCP 设置框，隐藏串口和 UDP 设置框
            if hasattr(self, 'serial_frame'):
                self.serial_frame.place_forget()
            if hasattr(self, 'tcp_frame'):
                self.tcp_frame.place(x=148, y=5)
            if hasattr(self, 'udp_frame'):
                self.udp_frame.place_forget()
            
            # 隐藏设备信息框和参数配置框
            for frame_name in ['device_info_frame', 'param_config_frame']:
                frame = getattr(self, frame_name, None)
                if frame:
                    frame.place_forget()
            
            # 显示协议指令框、自定义指令框
            cmd_layouts = [
                ('cmd_frame', 60, self._cmd_frame_height),
                ('custom_frame', self._custom_frame_y, 78),
            ]
            for frame_name, y, height in cmd_layouts:
                frame = getattr(self, frame_name, None)
                if frame:
                    frame.place(x=0, y=y, relwidth=1.0, height=height)
            
            # 隐藏自定义数据框（UDP 模式专用）
            if hasattr(self, 'custom_data_frame'):
                self.custom_data_frame.place_forget()
            
            # 设置通用框的位置
            for frame_name, (y, height) in common_frames.items():
                frame = getattr(self, frame_name, None)
                if frame:
                    frame.place(x=0, y=y, relwidth=1.0, height=height)
            
            # TCP 模式：启用指令按钮（除非 OTA 正在进行）
            if not ota_active:
                self._set_command_buttons_state(tk.NORMAL)
                
        elif mode == "udp":
            # UDP 模式：显示 UDP 设置框，隐藏串口和 TCP 设置框
            if hasattr(self, 'serial_frame'):
                self.serial_frame.place_forget()
            if hasattr(self, 'tcp_frame'):
                self.tcp_frame.place_forget()
            if hasattr(self, 'udp_frame'):
                self.udp_frame.place(x=148, y=5)
            
            # 隐藏设备信息框和参数配置框
            for frame_name in ['device_info_frame', 'param_config_frame']:
                frame = getattr(self, frame_name, None)
                if frame:
                    frame.place_forget()
            
            # 显示协议指令框（UDP 模式下使用自定义数据框替换自定义指令框）
            cmd_layouts = [
                ('cmd_frame', 60, self._cmd_frame_height),
            ]
            for frame_name, y, height in cmd_layouts:
                frame = getattr(self, frame_name, None)
                if frame:
                    frame.place(x=0, y=y, relwidth=1.0, height=height)
            
            # UDP 模式：显示自定义数据框（替换原有的自定义指令框）
            if hasattr(self, 'custom_data_frame'):
                self.custom_data_frame.place(x=0, y=self._custom_frame_y, relwidth=1.0, height=78)
            # 隐藏原有的自定义指令框
            if hasattr(self, 'custom_frame'):
                self.custom_frame.place_forget()
            
            # 设置通用框的位置
            for frame_name, (y, height) in common_frames.items():
                frame = getattr(self, frame_name, None)
                if frame:
                    frame.place(x=0, y=y, relwidth=1.0, height=height)
            
            # UDP 模式：禁用协议指令按钮；剂量率阈值输入框保持可编辑
            if not ota_active:
                self._set_command_buttons_state(tk.DISABLED, keep_dose_threshold_inputs=True)
            self._set_udp_custom_controls_state(tk.NORMAL)
        
        # 更新 UI 状态：按实际连接/重连状态同步（避免 tcp_connected 滞后导致误显示已连接）
        self._apply_connection_ui(mode)
        # 切换到某模式时，若该模式仍保持连接则恢复其监控线程（其它模式监控不受影响）
        if mode == "serial" and self.serial_connected and not self._serial_manual_disconnect:
            self.start_connection_monitor("serial")
        elif mode == "tcp" and self.tcp_connected and not self._tcp_manual_disconnect:
            self.start_connection_monitor("tcp")
        elif mode == "udp" and self.udp_connected and not self._udp_manual_disconnect:
            self.start_connection_monitor("udp")

        if mode == "udp":
            prefer = getattr(self, '_udp_bound_local_ip', None)
            self.root.after(0, lambda p=prefer: self._populate_local_ips(
                prefer_ip=p, force_refresh=True, background=True))
    
    def _set_dose_threshold_widgets_state(self, state):
        """设置剂量率阈值输入控件状态（Entry + 单位 Combobox）"""
        if hasattr(self, 'dose_thr_widgets'):
            for w in self.dose_thr_widgets:
                try:
                    w.config(state="readonly" if (state == tk.DISABLED and isinstance(w, ttk.Combobox)) else state)
                except tk.TclError:
                    w.config(state=state)

    def _set_command_buttons_state(self, state, keep_dose_threshold_inputs=False):
        """设置指令按钮状态

        keep_dose_threshold_inputs: UDP 模式下为 True 时，剂量率上下阈值输入框保持可编辑。
        """
        # 协议指令发送按钮
        if hasattr(self, 'cmd_send_button'):
            self.cmd_send_button.config(state=state)
        
        # 快捷指令按钮
        if hasattr(self, 'quick_buttons'):
            for btn in self.quick_buttons:
                btn.config(state=state)

        # 剂量率阈值输入
        if not keep_dose_threshold_inputs:
            self._set_dose_threshold_widgets_state(state)

        if hasattr(self, 'alarm_setting_widgets'):
            for w in self.alarm_setting_widgets:
                try:
                    w.config(state="readonly" if (state == tk.DISABLED and isinstance(w, ttk.Combobox)) else state)
                except tk.TclError:
                    w.config(state=state)

        if hasattr(self, 'cmd_dev_addr_widgets'):
            for w in self.cmd_dev_addr_widgets:
                w.config(state=state)
        
        # 自定义指令发送按钮
        if hasattr(self, 'custom_send_button'):
            self.custom_send_button.config(state=state)
        
        # 固件更新按钮（UDP 模式下禁用）
        if hasattr(self, 'start_iap_btn'):
            self.start_iap_btn.config(state=state)
        if hasattr(self, 'start_app_btn'):
            self.start_app_btn.config(state=state)
        if hasattr(self, 'stop_update_btn') and self.stop_update_btn:
            # OTA 进行中仍需允许用户点击「停止更新」
            if state == tk.DISABLED:
                self.stop_update_btn.config(state=tk.NORMAL)
            else:
                self.stop_update_btn.config(state=state)

    def _set_udp_custom_controls_state(self, state):
        """设置 UDP 自定义数据区控件状态（OTA 后台传输时保持可发送）"""
        if hasattr(self, 'udp_data_entry') and self.udp_data_entry:
            self.udp_data_entry.config(state=state)
        if hasattr(self, 'udp_send_btn') and self.udp_send_btn:
            self.udp_send_btn.config(state=state)
        if hasattr(self, 'udp_simulate_btn') and self.udp_simulate_btn:
            self.udp_simulate_btn.config(state=state)

    def update_port_list(self):
        """更新串口列表"""
        ports = self.serial_monitor.scan_ports()
        port_values = format_port_combo_values(ports)
        
        # 延迟更新，让已打开的下拉框先关闭
        self.root.after(50, lambda: self._update_port_values(port_values))
    
    def _update_port_values(self, port_values):
        """实际更新下拉列表的值"""
        prefer = self._normalize_serial_port(
            self._connected_serial_port or self.port_combo.get()
        )
        self.port_combo["values"] = port_values
        if prefer and any(self._normalize_serial_port(v) == prefer for v in port_values):
            self._set_port_combo_selection(prefer, port_values)
        elif port_values and not self._is_serial_port_open():
            self._set_port_combo_selection(
                self._normalize_serial_port(port_values[0]), port_values
            )
        elif not port_values:
            self._syncing_port_combo = True
            try:
                self.port_combo.set("")
            finally:
                self._syncing_port_combo = False
    
    def on_serial_param_changed(self, event=None):
        """串口参数（端口或波特率）变化处理"""
        if getattr(self, "_syncing_port_combo", False):
            return
        # 如果当前处于连接状态，断开并重新连接
        if self.serial_connected:
            self.log_receive("[串口] 检测到参数变化，正在重新连接...")
            
            # 主动断开当前连接
            self._serial_manual_disconnect = True
            self.serial_monitor.manual_disconnect = True
            self.stop_connection_monitor("serial")
            self.serial_monitor.suspend_link()
            self.serial_connected = False
            self.connected = (
                self.serial_connected or self.tcp_connected or self.udp_connected
            )
            
            self._update_serial_status_ui()
            
            self.root.after(500, self._reconnect_serial_after_param_change)
    
    def _reconnect_serial_after_param_change(self):
        """串口参数变化后重新连接"""
        self._serial_manual_disconnect = False
        self.serial_monitor.manual_disconnect = False
        
        port = self._normalize_serial_port(self.port_combo.get())
        if not port:
            self.log_receive("[串口] 重连失败：未选择端口")
            self._update_serial_status_ui()
            return
        
        baudrate_str = self.baud_combo.get().split(' - ')[0]
        baudrate = int(baudrate_str)
        
        try:
            if self.serial_monitor.connect(port, baudrate):
                self.serial_connected = True
                self.connected = (
                    self.serial_connected or self.tcp_connected or self.udp_connected
                )
                self._connected_serial_port = port
                self._refresh_serial_port_combo(prefer_port=port)
                self._update_serial_status_ui()
                self.start_connection_monitor("serial")
                self.log_receive(f"[串口] 重连成功：{port} {baudrate}")
            else:
                self.log_receive(f"[串口] 重连失败：{port} {baudrate}")
                self._update_serial_status_ui()
        except Exception as e:
            self.log_receive(f"[串口] 重连异常：{e}")
            self._update_serial_status_ui()
    
    def _get_tcp_ports(self) -> Tuple[int, int]:
        """返回 (控制端口, 数据端口)"""
        ctrl_port = int(self.tcp_ctrl_port_entry.get().strip())
        data_port = int(self.tcp_data_port_entry.get().strip())
        return ctrl_port, data_port

    def _tcp_link_label(self, ip: str) -> str:
        return f"Link: {ip}"

    def _tcp_port_desc(self, ctrl_port: int, data_port: int) -> str:
        if ctrl_port == data_port:
            return str(ctrl_port)
        return f"{ctrl_port}/{data_port}"

    def _note_tcp_link_up(self):
        """记录 TCP 连接成功时刻，监控宽限期内不做激进断线判断"""
        self._tcp_link_grace_until = time.time() + 3.0

    def _is_tcp_monitor_connected(self) -> bool:
        """TCP 监控用连接判断（宽限期内仅看读线程/错误标志）"""
        if self.tcp_dual.single_port_mode:
            client = self.tcp_dual.single
            if not self.tcp_dual.ctrl_connected:
                return False
            if client.read_error:
                return False
            if client.read_thread and not client.read_thread.is_alive():
                return False
            if time.time() < getattr(self, '_tcp_link_grace_until', 0):
                return client.connected
            return self.tcp_dual.is_fully_connected()

        ctrl = self.tcp_dual.ctrl
        data = self.tcp_dual.data
        if not (self.tcp_dual.ctrl_connected and self.tcp_dual.data_connected):
            return False
        if ctrl.read_error or data.read_error:
            return False
        if data.read_thread and not data.read_thread.is_alive():
            return False
        if ctrl.enable_reader and ctrl.read_thread and not ctrl.read_thread.is_alive():
            return False
        if time.time() < getattr(self, '_tcp_link_grace_until', 0):
            return ctrl.connected and data.connected
        return self.tcp_dual.is_fully_connected()

    def _tcp_link_ui_state(self) -> Tuple[str, str, str]:
        """返回 TCP 状态标签 (text, foreground, connect_btn_text)"""
        if self._tcp_manual_disconnect:
            return "未连接", "red", "连接"
        if self._is_tcp_monitor_connected():
            ip = self.tcp_ip_entry.get().strip()
            return self._tcp_link_label(ip), "green", "断开"
        if self.tcp_is_monitoring and self._tcp_disconnect_detected_time is not None:
            return "重新连接中...", "orange", "连接"
        return "未连接", "red", "连接"

    def _serial_link_ui_state(self) -> Tuple[str, str, str]:
        """返回串口状态标签 (text, foreground, connect_btn_text)"""
        if self._serial_manual_disconnect:
            return "未连接", "red", "连接"
        is_open = self._is_serial_port_open()
        if (
            self.serial_connected
            and is_open
            and not self._serial_manual_disconnect
        ):
            port = (
                self.serial_monitor.get_active_port()
                or self._normalize_serial_port(self._connected_serial_port)
                or self._normalize_serial_port(self.port_combo.get())
            )
            try:
                baudrate = int(self.baud_combo.get())
            except ValueError:
                baudrate = self.baud_combo.get()
            return f"Link: {port} {baudrate}", "green", "断开"
        if self.serial_is_monitoring and self.serial_monitor.is_reconnecting:
            return "重新连接中...", "orange", "连接"
        return "未连接", "red", "连接"

    def _udp_link_ui_state(self) -> Tuple[str, str, str]:
        """返回 UDP 状态标签 (text, foreground, connect_btn_text)"""
        if self._udp_manual_disconnect:
            return "未连接", "red", "连接"
        if self.udp_connected and self.udp_server and self.udp_server.bound:
            ip = self.udp_ip_entry.get().strip()
            port = int(self.udp_port_entry.get().strip())
            return f"Link: {ip}:{port}", "green", "断开"
        if self.udp_is_monitoring and self._udp_disconnect_detected_time is not None:
            return "重新连接中...", "orange", "连接"
        return "未连接", "red", "连接"

    def _update_tcp_status_ui(self) -> None:
        text, color, btn = self._tcp_link_ui_state()
        self.tcp_status_label.config(text=text, foreground=color)
        self.tcp_connect_btn.config(text=btn)
        if self.current_mode == "tcp":
            self.current_status_label.config(text=text, foreground=color)
            self.current_connect_btn.config(text=btn)

    def _update_serial_status_ui(self) -> None:
        text, color, btn = self._serial_link_ui_state()
        self.serial_status_label.config(text=text, foreground=color)
        self.serial_connect_btn.config(text=btn)
        if self.current_mode == "serial":
            self.current_status_label.config(text=text, foreground=color)
            self.current_connect_btn.config(text=btn)

    def _update_udp_status_ui(self) -> None:
        text, color, btn = self._udp_link_ui_state()
        self.udp_status_label.config(text=text, foreground=color)
        self.udp_connect_btn.config(text=btn)
        if self.current_mode == "udp":
            self.current_status_label.config(text=text, foreground=color)
            self.current_connect_btn.config(text=btn)

    def _apply_connection_ui(self, mode: str) -> None:
        """按实际连接/重连状态刷新指定模式的标签与按钮"""
        if mode == "serial":
            self._update_serial_status_ui()
        elif mode == "tcp":
            self._update_tcp_status_ui()
        elif mode == "udp":
            self._update_udp_status_ui()

    def _set_input_widgets_state(self, state):
        """设置输入控件的状态（启用/禁用）"""
        mode = self.mode_var.get().split(' - ')[0].lower()
        
        if mode == "serial":
            # 串口模式：禁用端口和波特率下拉框
            if hasattr(self, 'port_combo'):
                self.port_combo.config(state=state)
            if hasattr(self, 'baud_combo'):
                self.baud_combo.config(state=state)
        elif mode == "tcp":
            # TCP 模式：禁用 IP、端口和设备地址输入框
            if hasattr(self, 'tcp_ip_entry'):
                self.tcp_ip_entry.config(state=state)
            if hasattr(self, 'tcp_ctrl_port_entry'):
                self.tcp_ctrl_port_entry.config(state=state)
            if hasattr(self, 'tcp_data_port_entry'):
                self.tcp_data_port_entry.config(state=state)
        elif mode == "udp":
            # UDP 模式：禁用本地 IP 下拉框、远端 IP 和端口输入框
            if hasattr(self, 'udp_local_ip_combo'):
                self.udp_local_ip_combo.config(state=state)
            if hasattr(self, 'udp_ip_entry'):
                self.udp_ip_entry.config(state=state)
            if hasattr(self, 'udp_port_entry'):
                self.udp_port_entry.config(state=state)
    
    def toggle_connection(self):
        """切换连接状态"""
        mode = self.mode_var.get().split(' - ')[0].lower()
        
        if mode == "serial":
            if not self.serial_connected:
                self._serial_manual_disconnect = False
                self.serial_monitor.manual_disconnect = False
                
                port_str = self.port_combo.get()
                if not port_str:
                    messagebox.showwarning("警告", "请选择串口")
                    return
                
                port = self._normalize_serial_port(port_str)
                baudrate = int(self.baud_combo.get())
                
                if self.serial_monitor.connect(port, baudrate):
                    self.serial_connected = True
                    # 更新全局连接状态
                    self.connected = self.serial_connected or self.tcp_connected or self.udp_connected
                    self.current_mode = mode
                    self.current_port = port
                    # 记住连接时的串口号
                    self._connected_serial_port = port
                    self._refresh_serial_port_combo(prefer_port=port)
                    
                    # 禁用输入框
                    self._set_input_widgets_state(tk.DISABLED)
                    
                    # 更新按钮和状态
                    self.current_connect_btn.config(text="断开")
                    self.serial_status_label.config(text=f"Link: {port} {baudrate}", foreground="green")
                    
                    # 启动连接监控
                    self.start_connection_monitor("serial")
                else:
                    messagebox.showerror("错误", "连接失败")
            else:
                # 主动断开：委托串口服务统一清理
                self._serial_manual_disconnect = True
                self.serial_monitor.manual_disconnect = True
                self.serial_connected = False
                self.connected = (
                    self.serial_connected or self.tcp_connected or self.udp_connected
                )
                self.stop_connection_monitor("serial")
                self._connected_serial_port = None
                self.serial_monitor.disconnect(manual=True)
                
                self.current_connect_btn.config(text="连接")
                self.current_status_label.config(text="未连接", foreground="red")
                self._set_input_widgets_state(tk.NORMAL)
                self.auto_scan_active = True
                self.last_port_count = 0
                self.start_auto_port_scan()
                self.update_port_list()
        
        elif mode == "tcp":
            if not self.tcp_connected:
                # 清除主动断开标志
                self._tcp_manual_disconnect = False
                
                try:
                    ip = self.tcp_ip_entry.get().strip()
                    ctrl_port, data_port = self._get_tcp_ports()
                    ok_ctrl, ok_data = self.tcp_dual.connect(ip, ctrl_port, data_port)
                    
                    if ok_ctrl and ok_data:
                        self.tcp_connected = True
                        self._note_tcp_link_up()
                        # 更新全局连接状态
                        self.connected = self.serial_connected or self.tcp_connected or self.udp_connected
                        self.current_mode = mode
                        # 不要立即清除地址，等待接收帧自动识别
                        # self._reset_tcp_addr_display()
                        
                        # 禁用输入框
                        self._set_input_widgets_state(tk.DISABLED)
                        # 更新按钮和状态
                        self.current_connect_btn.config(text="断开")
                        self.current_status_label.config(text=self._tcp_link_label(ip), foreground="green")
                        
                        # 启动连接监控
                        self.start_connection_monitor("tcp")
                        self._refresh_tcp_parse_cache()
                        
                        # 主动读取设备地址（reg254），让单片机返回地址信息
                        self.root.after(100, self._tcp_auto_identify_addr)
                    else:
                        self.tcp_dual.disconnect()
                        messagebox.showerror("错误", "TCP 连接失败")
                except ValueError as e:
                    messagebox.showerror("错误", f"参数错误：{e}")
            else:
                # 断开 TCP 连接
                # 标记为主动断开，阻止重连
                self._tcp_manual_disconnect = True
                
                # 停止连接监控
                self.stop_connection_monitor("tcp")
                
                self.tcp_dual.disconnect()
                self.tcp_connected = False
                # self.connected = False  # 只有所有模式都断开时才为 False
                # 更新全局连接状态
                self.connected = self.serial_connected or self.tcp_connected or self.udp_connected
                
                # 重新启用输入框
                self._set_input_widgets_state(tk.NORMAL)
                self._reset_tcp_addr_display()
                
                # 更新按钮和状态
                self.current_connect_btn.config(text="连接")
                self.current_status_label.config(text="未连接", foreground="red")
        
        elif mode == "udp":
            if not self.udp_connected:
                # 清除主动断开标志
                self._udp_manual_disconnect = False
                
                # 验证 UDP 地址范围
                ip = self.udp_ip_entry.get().strip()
                try:
                    port = int(self.udp_port_entry.get().strip())
                except ValueError:
                    messagebox.showerror("错误", "端口号必须是数字")
                    return
                
                # 验证 IP 地址是否符合组播地址范围
                if not self._validate_multicast_ip(ip):
                    messagebox.showerror(
                        "错误",
                        "UDP 地址范围错误！\n\n"
                        "正确的 UDP 组播地址范围：\n"
                        "• 组播地址：224.0.0.0 - 239.255.255.255\n"
                        "• 推荐地址：236.2.3.6（单片机默认组播地址）\n"
                        "• 广播地址：255.255.255.255\n\n"
                        f"当前输入地址：{ip}\n"
                        "请检查后重新输入！"
                    )
                    return
                
                try:
                    # UDP 绑定到用户选择的本地 IP（有可用的真实 IP 时不默认 0.0.0.0）
                    local_ip = self.udp_local_ip_combo.get().strip()
                    if not local_ip or local_ip == "0.0.0.0":
                        real_ips = [v for v in self.udp_local_ip_combo['values']
                                    if v and v != "0.0.0.0"]
                        if real_ips:
                            local_ip = real_ips[0]
                        else:
                            local_ip = "0.0.0.0"
                    
                    if self.udp_server.bind(local_ip, port, ip):
                        self.udp_connected = True
                        self._udp_bound_local_ip = local_ip
                        self._udp_bound_port = port
                        self._udp_bound_multicast = ip
                        self._udp_nic_waiting = False
                        self._sync_udp_local_ip_combo(local_ip)
                        # 更新全局连接状态
                        self.connected = self.serial_connected or self.tcp_connected or self.udp_connected
                        self.current_mode = mode
                        # 禁用输入框
                        self._set_input_widgets_state(tk.DISABLED)
                        # 更新按钮和状态
                        self.current_connect_btn.config(text="断开")
                        self.current_status_label.config(text=f"Link: {ip}:{port}", foreground="green")
                        
                        # 启动连接监控
                        self.start_connection_monitor("udp")
                    else:
                        messagebox.showerror("错误", "UDP 绑定失败")
                except ValueError as e:
                    messagebox.showerror("错误", f"参数错误：{e}")
            else:
                # 断开 UDP 连接
                # 标记为主动断开，阻止重连
                self._udp_manual_disconnect = True
                
                # 停止连接监控
                self.stop_connection_monitor("udp")
                
                self.udp_server.unbind()
                self.udp_connected = False
                self._udp_bound_local_ip = None
                self._udp_nic_waiting = False
                # self.connected = False  # 只有所有模式都断开时才为 False
                # 更新全局连接状态
                self.connected = self.serial_connected or self.tcp_connected or self.udp_connected
                
                # 重新启用输入框
                self._set_input_widgets_state(tk.NORMAL)
                
                # 更新按钮和状态
                self.current_connect_btn.config(text="连接")
                self.current_status_label.config(text="未连接", foreground="red")
    
    def _is_current_mode_connected(self) -> bool:
        """当前 UI 所选通信方式是否已连接（不用全局 self.connected）"""
        mode = self.mode_var.get().split(' - ')[0].lower()
        if mode == "serial":
            return bool(self.serial_connected and self._is_serial_port_open())
        if mode == "tcp":
            return bool(self.tcp_connected)
        if mode == "udp":
            return bool(self.udp_connected and getattr(self.udp_server, 'bound', False))
        return False

    def _require_current_mode_connected(self) -> bool:
        """发送前检查当前模式连接状态"""
        if self._is_current_mode_connected():
            return True
        mode = self.mode_var.get().split(' - ')[0].lower()
        labels = {"serial": "串口", "tcp": "TCP", "udp": "UDP"}
        label = labels.get(mode, "设备")
        messagebox.showwarning("警告", f"请连接{label}！")
        return False

    def _get_tcp_device_addr(self, quiet: bool = False):
        """获取 TCP 设备地址（与 UI 模式无关）"""
        if not self.tcp_connected:
            if not quiet:
                messagebox.showwarning("警告", "请连接TCP！")
            return None
        addr = getattr(self, '_tcp_identified_addr', None)
        if addr is None:
            if not quiet:
                messagebox.showinfo("提示", "正在识别设备地址中！")
            return None
        if not (0 <= int(addr) <= 255):
            if not quiet:
                messagebox.showwarning("警告", "设备地址必须在 0-255 范围内！")
            return None
        return int(addr)

    def _get_ota_device_addr(self) -> int:
        """OTA 使用快照地址，切换 UI 模式不影响 TCP 升级"""
        cached = getattr(self, '_ota_device_addr', None)
        if cached is not None:
            return int(cached)
        if self.ota_mode_var.get() == "TCP":
            return int(self._get_tcp_device_addr(quiet=True) or 1)
        addr = self._get_current_addr()
        return int(addr if addr is not None else 1)

    def _get_current_addr(self):
        """获取当前模式的设备地址"""
        mode = self.mode_var.get().split(' - ')[0].lower()
        if mode == "tcp":
            return self._get_tcp_device_addr(quiet=False)
        elif mode == "udp":
            # UDP 模式不使用设备地址
            return 1  # 返回默认值
        else:  # serial
            try:
                addr = int(self.param_addr_var.get().strip()) if self.param_addr_var.get().strip() else 1
                return addr
            except ValueError:
                messagebox.showwarning("警告", "设备地址必须是数字")
                return None
        
        return None
    
    def _safe_ui_after(self, delay_ms: int, callback) -> None:
        """关闭窗口后不再调度 Tk 回调，避免读线程触发 RuntimeError"""
        if getattr(self, '_app_closing', False):
            return
        try:
            if not self.root.winfo_exists():
                return
        except tk.TclError:
            return
        try:
            self.root.after(delay_ms, callback)
        except (RuntimeError, tk.TclError):
            pass

    def _dispatch_received(self, data: bytes, source_mode: str) -> None:
        """串口/UDP 切主线程；TCP 在读线程内立即解析并应答（单片机 ACK 超时约 16ms）"""
        if getattr(self, '_app_closing', False):
            return
        if source_mode == "tcp":
            self.on_data_received(data, source_mode)
        else:
            self._safe_ui_after(0, lambda d=data, m=source_mode: self.on_data_received(d, m))

    def on_data_received(self, data: bytes, source_mode: str = None):
        """处理接收到的数据 - 严格参照单片机 Net_Protocol_Resolve 实现"""
        if getattr(self, '_app_closing', False):
            return

        # 读线程路径带 source_mode，不访问 mode_var（关闭时 Tk 已销毁会报错）
        if source_mode is None:
            try:
                ui_mode = self.mode_var.get().split(' - ')[0].lower()
            except RuntimeError:
                return
            source_mode = ui_mode
        
        # 根据数据源模式处理数据（TCP 后台仍解析/应答；显示按 _ui_mode_cache 过滤）
        if source_mode == "serial":
            if not self._is_serial_port_open():
                return
            show_ui = self._should_show_channel("serial")
            self._serial_append_and_flush_lines(data, show_ui=show_ui)
        
        elif source_mode == "tcp":
            # TCP 数据：读线程内解析并应答；与 UI 模式无关，只要 TCP 仍连接即处理
            if not self.tcp_connected or getattr(self, '_app_closing', False):
                return
            
            with self.rx_lock:
                self.rx_buffer.extend(data)
                while True:
                    if not self.parse_and_display_registers(source_mode):
                        break
        
        elif source_mode == "udp":
            if not self._should_show_channel("udp"):
                return
            clean = data.split(b"\x00")[0]
            try:
                text = clean.decode("utf-8", errors="ignore").strip()
            except Exception:
                text = ""
            
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            color = self.rx_colors[self.rx_color_index % len(self.rx_colors)]
            self.rx_color_index += 1
            self.rx_text.insert(tk.END, f"[{timestamp}]\n", "black")
            
            if text:
                self.rx_text.insert(tk.END, text + "\n", ("color_" + color,))
                self._parse_device_info(text)
            else:
                hex_str = " ".join(f"{b:02X}" for b in clean)
                self.rx_text.insert(tk.END, hex_str + "\n", ("color_" + color,))
            
            self.rx_text.see(tk.END)

    def _serial_append_and_flush_lines(self, data: bytes, show_ui: bool = True) -> None:
        """串口数据按行组包后再显示，避免 UTF-8 多字节字符被拆成两帧"""
        self.serial_line_buffer.extend(data)
        while True:
            buf = self.serial_line_buffer
            nl = -1
            for i, b in enumerate(buf):
                if b == ord('\n'):
                    nl = i
                    break
            if nl < 0:
                break
            line_bytes = bytes(buf[:nl + 1])
            del buf[:nl + 1]
            try:
                line = line_bytes.decode('utf-8', errors='replace').strip('\r\n')
            except Exception:
                line = ''
            if line:
                self._display_serial_line(line, show_ui=show_ui)

    def _display_serial_line(self, line: str, show_ui: bool = True) -> None:
        """显示一行完整串口文本"""
        if show_ui:
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            color = self.rx_colors[self.rx_color_index % len(self.rx_colors)]
            self.rx_color_index += 1
            self.rx_text.insert(tk.END, f"[{timestamp}]\n", 'black')
            self.rx_text.insert(tk.END, line + "\n", ('color_' + color,))
            self.rx_text.see(tk.END)
        self._parse_device_info(line + "\r\n")
        self._parse_param_info(line + "\r\n")

    def parse_and_display_registers(self, source_mode: str) -> bool:
        """
        解析并显示寄存器数值 - 严格参照单片机 Net_Frame_Reasm_Process 实现
        返回值：成功处理一帧返回 True，否则返回 False
        """
        if getattr(self, '_app_closing', False):
            return False

        n = len(self.rx_buffer)
        
        # 如果缓冲区为空，直接返回 False
        if n < 4:
            return False
        
        if source_mode == "tcp":
            show_rx_window = self._should_show_tcp_rx_panel()
        else:
            show_rx_window = True
        
        pos = 0
        
        # 重组缓存大小限制（2KB）
        MAX_REASM_SIZE = 2048
        
        # 滑动窗口搜索（严格参照单片机第 1452-1562 行）
        while pos + 4 <= n:
            # 跳过填充零（单片机第 1465-1466 行）
            while pos < n and self.rx_buffer[pos] == 0:
                pos += 1
            
            if pos + 4 > n:
                break
            
            # 快速过滤：功能码不在协议集合内时跳过（单片机第 1478-1483 行）
            func = self.rx_buffer[pos + 1] if pos + 1 < n else 0
            if func not in [0x06, 0x13, 0x15, 0x16, 0x20, 0x23, 0x25]:
                pos += 1
                continue
            
            # 滑动窗口搜索 CRC 匹配（单片机第 1485-1515 行）
            flen = 0
            
            # 计算最大搜索长度（参照单片机 Net_GetCrcSearchMaxTry）
            # 上位机重组缓存 2KB，限制最大帧长为 2KB
            remaining = n - pos
            maxtry = min(remaining, MAX_REASM_SIZE)
            
            # 快路径：优先校验预测长度（单片机第 1494-1499 行）
            pred_len = 0
            if func in [0x13, 0x23]:
                byte_count = self.rx_buffer[pos + 2] if pos + 2 < n else 0
                pred_len = 7 + byte_count
                if (pred_len >= 4 and pred_len <= maxtry and
                    pos + pred_len <= len(self.rx_buffer)):
                    chunk = bytes(self.rx_buffer[pos:pos + pred_len])
                    if CRC16.verify(chunk) and NetRawProtocol.frame_len_format_ok(self.rx_buffer[pos:pos + pred_len], pred_len):
                        flen = pred_len
            
            # 滑动窗口搜索（单片机第 1501-1515 行）- 从大到小搜索
            if not flen:
                for try_len in range(maxtry, 3, -1):  # 从 maxtry 递减到 4
                    # 跳过预测长度（避免重复校验）
                    if try_len == pred_len:
                        continue
                    
                    if pos + try_len > len(self.rx_buffer):
                        continue
                    
                    chunk = bytes(self.rx_buffer[pos:pos + try_len])
                    # CRC 校验 + 帧格式校验（严格参照单片机）
                    if CRC16.verify(chunk) and NetRawProtocol.frame_len_format_ok(self.rx_buffer[pos:pos + try_len], try_len):
                        flen = try_len
                        break
            
            if flen:
                # 找到完整帧，处理（单片机第 1517-1557 行）
                frame = bytes(self.rx_buffer[pos:pos + flen])
                # 保存无效数据的副本（避免缓冲区被修改后数据丢失）
                invalid_data = bytes(self.rx_buffer[0:pos]) if pos > 0 else b''

                # 自动应答所有合法的 Modbus 请求指令（非 OTA 期间）
                # 上位机作为从机时，应答应所有主机的请求指令
                if (source_mode == "tcp" and self._auto_ack_enabled
                        and not getattr(self, 'firmware_update_active', False)):
                    ack_frame = NetRawProtocol.build_modbus_ack_from_request(frame)
                    if ack_frame:
                        self._send_modbus_ack(ack_frame, invalid_data)

                # 显示/寄存器解析放到 UI 线程，避免粘包时拖慢后续帧的 ACK
                frame_copy = bytes(frame)
                invalid_copy = bytes(invalid_data)
                show_win = show_rx_window
                self._safe_ui_after(
                    0,
                    lambda f=frame_copy, inv=invalid_copy, sw=show_win:
                        self._tcp_frame_postprocess(f, inv, sw),
                )

                # 移除已处理的帧和之前的无效数据，保留剩余数据（单片机第 1534-1550 行）
                remain = n - (pos + flen)
                if remain > 0:
                    self.rx_buffer = self.rx_buffer[pos + flen:]
                    # 丢弃发送端多带的尾填充 0x00（如 TxQueue 重复 CRC 时的残留）
                    if self.rx_buffer and all(b == 0 for b in self.rx_buffer):
                        self.rx_buffer = bytearray()
                else:
                    self.rx_buffer = bytearray()
                
                # 处理完一帧后立即返回 True（单片机第 1557 行 return true）
                return True
            
            # 无合法帧，前进 1 字节继续搜索（单片机第 1561 行）
            pos += 1
        
        # 数据不完整或无合法帧，返回 False
        # 检查是否需要清理缓冲区（单片机第 1568-1574 行）
        if n > 256:
            print(f"[缓冲区] 清理缓冲区，保留末尾 16 字节")
            self.rx_buffer = self.rx_buffer[-16:]
        
        return False

    def _tcp_frame_postprocess(self, frame: bytes, invalid_data: bytes, show_rx_window: bool) -> None:
        """TCP 帧 ACK 之后的 UI 显示与寄存器刷新（主线程）"""
        if getattr(self, '_app_closing', False):
            return

        frame_addr = frame[0]

        def _sync_tcp_addr_from_frame(fa=frame_addr):
            try:
                if self.mode_var.get().split(' - ')[0].lower() != "tcp":
                    return
                current_addr = getattr(self, '_tcp_identified_addr', None)
                if current_addr == fa:
                    return
                was_auto = current_addr is None
                self._set_tcp_addr_display(fa)
                if self._should_show_channel("tcp"):
                    if was_auto:
                        self.log_receive(f"[自动识别] 检测到设备地址：0x{fa:02X}")
                    else:
                        self.log_receive(
                            f"[自动识别] 设备地址已刷新：0x{current_addr:02X} → 0x{fa:02X}"
                        )
            except Exception:
                pass

        _sync_tcp_addr_from_frame()

        if show_rx_window:
            color = self.rx_colors[self.rx_color_index % len(self.rx_colors)]
            self.rx_color_index += 1
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            self.rx_text.insert(tk.END, f"[{timestamp}]\n", 'black')
            all_bytes = list(invalid_data) + list(frame)
            hex_bytes = [f"{b:02X}" for b in all_bytes]
            invalid_len = len(invalid_data)
            current_pos = 0
            while current_pos < len(hex_bytes):
                line_end = min(current_pos + 16, len(hex_bytes))
                if current_pos < invalid_len and line_end > invalid_len:
                    invalid_part = ' '.join(hex_bytes[current_pos:invalid_len])
                    valid_part = ' '.join(hex_bytes[invalid_len:line_end])
                    self.rx_text.insert(tk.END, invalid_part + " ", 'black')
                    self.rx_text.insert(tk.END, valid_part + "\n", ('color_' + color))
                elif current_pos < invalid_len:
                    line = ' '.join(hex_bytes[current_pos:line_end])
                    self.rx_text.insert(tk.END, line + "\n", 'black')
                else:
                    line = ' '.join(hex_bytes[current_pos:line_end])
                    self.rx_text.insert(tk.END, line + "\n", ('color_' + color))
                current_pos = line_end
            self.rx_text.see(tk.END)

        fi = NetRawProtocol.parse_frame(frame)
        if not fi:
            return

        func = fi.get('func', 0)
        reg_addr = fi.get('reg_addr', 0)

        if func not in [0x13, 0x23] or 'payload' not in fi:
            return

        payload = fi['payload']
        reg_a = reg_addr

        if func == 0x23 and reg_a == NetRawProtocol.REG_DATA_TIME and len(payload) >= 12:
            history_line = NetRawProtocol.format_5min_record_line(payload[:12])
            if history_line:
                self.append_5min_history(history_line)

        idx = 0
        current_addr = reg_a
        ota_state_found = False
        ota_state = 0
        ota_written = 0

        while idx + 2 <= len(payload):
            if current_addr == 204 and idx + 8 <= len(payload):
                ota_state = (payload[idx] | (payload[idx + 1] << 8) |
                             (payload[idx + 2] << 16) | (payload[idx + 3] << 24))
                ota_written = (payload[idx + 4] | (payload[idx + 5] << 8) |
                               (payload[idx + 6] << 16) | (payload[idx + 7] << 24))
                ota_state_found = True
                idx += 8
                current_addr += 4
                continue

            if current_addr <= 21:
                if idx + 4 <= len(payload):
                    val_32 = (payload[idx] | (payload[idx + 1] << 8) |
                              (payload[idx + 2] << 16) | (payload[idx + 3] << 24))
                    if current_addr == 3 and val_32 & 0x80000000:
                        val_32 = val_32 - 0x100000000
                    self.update_register_display(current_addr, val_32)
                    idx += 4
                    current_addr += 2
                else:
                    break
            elif current_addr >= 50 and current_addr <= 72:
                if idx + 4 <= len(payload):
                    val_32 = (payload[idx] | (payload[idx + 1] << 8) |
                              (payload[idx + 2] << 16) | (payload[idx + 3] << 24))
                    self.update_register_display(current_addr, val_32)
                    idx += 4
                    current_addr += 2
                else:
                    break
            elif current_addr == NetRawProtocol.REG_ALARM_BITEN:
                if idx + 4 <= len(payload):
                    val_32 = (payload[idx] | (payload[idx + 1] << 8) |
                              (payload[idx + 2] << 16) | (payload[idx + 3] << 24))
                    self.update_register_display(NetRawProtocol.REG_ALARM_BITEN, val_32)
                    idx += 4
                    current_addr += 2
                else:
                    break
            elif (NetRawProtocol.REG_REBOOT <= current_addr
                  <= NetRawProtocol.REG_CONTROL_BIT2):
                if idx + 2 <= len(payload):
                    val_16 = payload[idx] | (payload[idx + 1] << 8)
                    self.update_register_display(current_addr, val_16)
                    idx += 2
                    current_addr += 1
                else:
                    break
            elif current_addr >= NetRawProtocol.REG_ALARM_BITEN:
                if idx + 2 <= len(payload):
                    idx += 2
                    current_addr += 1
                else:
                    break
            else:
                break

        if ota_state_found and self.firmware_update_active:
            self.handle_ota_status_response(ota_state, ota_written)

    def parse_string_register(self, payload: bytes, start_addr: int, length: int) -> str:
        """解析字符串寄存器（序列号/软件版本）"""
        try:
            # 计算字符串在 payload 中的起始位置
            # 每个寄存器地址占用 2 字节，字符串寄存器从 start_addr 开始
            # 需要找到对应的数据偏移
            str_bytes = payload[start_addr * 2 : start_addr * 2 + length]
            # 去除末尾的零填充
            str_bytes = str_bytes.rstrip(b'\x00')
            return str_bytes.decode('utf-8', errors='replace')
        except:
            return ""
    
    def get_register_name(self, addr: int) -> str:
        """根据寄存器地址获取名称"""
        for reg_addr, name, _ in self.registers:
            if reg_addr == addr:
                return name
        return f"未知寄存器 (地址{addr})"
    
    # 阈值为 0 时，报警状态框对应项显示「禁用」
    _THRESHOLD_ZERO_ALARM_MAP = {
        50: ("辐射上阈值报警", 0),
        52: ("辐射下阈值报警", 1),
        54: ("温度上阈值报警", 4),
        56: ("温度下阈值报警", 5),
        58: ("气压上阈值报警", 8),
        60: ("气压下阈值报警", 9),
        62: ("湿度上阈值报警", 12),
        64: ("湿度下阈值报警", 13),
        66: ("CO2 上阈值报警", 16),
        68: ("CO2 下阈值报警", 17),
        70: ("PM2.5 上阈值报警", 20),
        72: ("PM2.5 下阈值报警", 21),
    }
    _ALARM_STATUS_COLOR_DISABLED = "#CC99FF"  # 禁用（阈值=0，浅紫色）
    _ALARM_STATUS_COLOR_ACTIVE = "#FF9999"    # 报警中（使能打开且触发，浅红色）
    _ALARM_STATUS_COLOR_NORMAL = "#66CC66"    # 正常（绿色）
    _ALARM_STATUS_COLOR_TRIGGERED = "#66CCCC" # 触发（使能关闭，青色）

    def _update_alarm_label(self, name: str, bit_pos: int, alarm_value: int) -> None:
        """
        更新单个报警状态标签
        逻辑：
          1. 阈值=0 时（reg82 禁止）→ 显示「禁用」（浅红色）
          2. 阈值>0 且 reg82 使能打开 且 报警触发 → 显示「报警中」（浅黄色）
          3. 阈值>0 但 reg82 使能关闭 且 报警触发 → 显示「触发」（红色，不会发生）
          4. 报警未触发 → 显示「正常」（绿色）
        """
        if name not in self.alarm_labels:
            return
        label_info = self.alarm_labels[name]
        
        # 1. 检查是否为阈值=0 的情况（reg82 禁止）
        if name in getattr(self, '_threshold_zero_alarms', set()):
            label_info['label'].config(text="禁用", foreground=self._ALARM_STATUS_COLOR_DISABLED)
            return
        
        # 2. 检查报警是否触发
        is_active = bool(alarm_value & (1 << bit_pos))
        
        # 3. 检查 reg82 报警使能标志位（bit=0 表示使能打开，bit=1 表示禁止）
        alarm_biten = self._reg_raw_cache.get(NetRawProtocol.REG_ALARM_BITEN, 0)
        is_enabled = not bool(alarm_biten & (1 << bit_pos))  # bit=0 → 使能打开
        
        # 4. 根据使能状态和报警状态显示
        if is_active:
            if is_enabled:
                # 使能打开且报警触发 → 显示「报警中」（浅黄色）
                status_text = "报警中"
                fg_color = self._ALARM_STATUS_COLOR_ACTIVE
            else:
                # 使能关闭但报警触发（理论上不应该发生）→ 显示「触发」（红色）
                status_text = "触发"
                fg_color = self._ALARM_STATUS_COLOR_TRIGGERED
        else:
            # 报警未触发 → 显示「正常」（绿色）
            status_text = "正常"
            fg_color = self._ALARM_STATUS_COLOR_NORMAL
        
        label_info['label'].config(text=status_text, foreground=fg_color)

    def update_alarm_status_display(self, value: int):
        """更新报警状态显示（使用标签格式，8 列显示，每列 4 个）"""
        # 如果是第一次初始化，创建标签
        if not hasattr(self, '_alarm_initialized') or not self._alarm_initialized:
            self._init_alarm_display()
            self._alarm_initialized = True
        
        # 更新现有标签的状态
        # 定义报警状态位（位索引，名称）- 包含所有 32 个位
        alarm_bits = [
            (0, "辐射上阈值报警"),
            (1, "辐射下阈值报警"),
            (2, "辐射检测离线"),
            (3, "辐射保留"),
            (4, "温度上阈值报警"),
            (5, "温度下阈值报警"),
            (6, "温度检测离线"),
            (7, "温度保留"),
            (8, "气压上阈值报警"),
            (9, "气压下阈值报警"),
            (10, "气压检测离线"),
            (11, "气压保留"),
            (12, "湿度上阈值报警"),
            (13, "湿度下阈值报警"),
            (14, "湿度检测离线"),
            (15, "湿度保留"),
            (16, "CO2 上阈值报警"),
            (17, "CO2 下阈值报警"),
            (18, "CO2 检测离线"),
            (19, "CO2 保留"),
            (20, "PM2.5 上阈值报警"),
            (21, "PM2.5 下阈值报警"),
            (22, "PM2.5 检测离线"),
            (23, "PM2.5 保留"),
            (24, "声报警损坏"),
            (25, "声报警保留"),
            (26, "声报警离线"),
            (27, "声报警保留 2"),
            (28, "光报警损坏"),
            (29, "光报警保留"),
            (30, "光报警离线"),
            (31, "光报警保留 2"),
        ]
        
        self._last_alarm_status = value
        for bit_pos, name in alarm_bits:
            if name in self.alarm_labels:
                self._update_alarm_label(name, bit_pos, value)
    
    def _init_alarm_display(self):
        """初始化报警状态显示（只调用一次）"""
        # 定义报警状态位（位索引，名称）- 包含所有 32 个位（包括保留位）
        alarm_bits = [
            (0, "辐射上阈值报警"),
            (1, "辐射下阈值报警"),
            (2, "辐射检测离线"),
            (3, "辐射保留"),
            (4, "温度上阈值报警"),
            (5, "温度下阈值报警"),
            (6, "温度检测离线"),
            (7, "温度保留"),
            (8, "气压上阈值报警"),
            (9, "气压下阈值报警"),
            (10, "气压检测离线"),
            (11, "气压保留"),
            (12, "湿度上阈值报警"),
            (13, "湿度下阈值报警"),
            (14, "湿度检测离线"),
            (15, "湿度保留"),
            (16, "CO2 上阈值报警"),
            (17, "CO2 下阈值报警"),
            (18, "CO2 检测离线"),
            (19, "CO2 保留"),
            (20, "PM2.5 上阈值报警"),
            (21, "PM2.5 下阈值报警"),
            (22, "PM2.5 检测离线"),
            (23, "PM2.5 保留"),
            (24, "声报警损坏"),
            (25, "声报警保留"),
            (26, "声报警离线"),
            (27, "声报警保留 2"),
            (28, "光报警损坏"),
            (29, "光报警保留"),
            (30, "光报警离线"),
            (31, "光报警保留 2"),
        ]
        
        # 8 列布局，每列 4 个（32 个状态位 / 8 列 = 4 个/列）- 从上到下，从左到右排布
        items_per_col = 4  # 每列 4 个
        total_items = len(alarm_bits)
        num_cols = 8  # 8 列
        
        # 创建 8 列
        for col_idx in range(num_cols):
            col_frame = tk.Frame(self.alarm_scrollable_frame, bg=self.root.cget('bg'))
            col_frame.grid(row=0, column=col_idx, padx=1, pady=1, sticky="n")
            
            start_idx = col_idx * items_per_col
            end_idx = min(start_idx + items_per_col, total_items)
            
            for i in range(start_idx, end_idx):
                bit_pos, name = alarm_bits[i]
                label_frame = ttk.LabelFrame(col_frame, text=name, padding=2)
                label_frame.pack(fill=tk.X, pady=0, ipadx=1, ipady=0)
                
                value_label = ttk.Label(label_frame, text="-", font=("Microsoft YaHei", 9), foreground="black")
                value_label.pack(anchor=tk.W, padx=2, pady=0)
                
                # 保存标签引用
                self.alarm_labels[name] = {
                    'label': value_label,
                    'bit_pos': bit_pos
                }
    
    # 设备状态：声/光/屏"禁用"时使用浅紫色
    _DEVICE_ALARM_OUTPUT_NAMES = frozenset({"声报警", "光报警", "屏幕"})
    _DEVICE_STATUS_COLOR_ENABLED = "#6699FF"
    _DEVICE_STATUS_COLOR_DISABLED = "#CC99FF"  # 禁用（浅紫色）

    def _device_status_fg_color(self, name: str, status_text: str) -> str:
        if name in self._DEVICE_ALARM_OUTPUT_NAMES:
            return (self._DEVICE_STATUS_COLOR_ENABLED if status_text == "启用"
                    else self._DEVICE_STATUS_COLOR_DISABLED)
        return self._DEVICE_STATUS_COLOR_ENABLED

    def update_device_status_display(self, value: int):
        """更新设备状态显示（使用标签格式，8 列显示，每列 2 个）"""
        # 如果是第一次初始化，创建标签
        if not hasattr(self, '_device_initialized') or not self._device_initialized:
            self._init_device_display()
            self._device_initialized = True
        
        # 更新现有标签的状态
        # 定义设备状态位
        device_states = [
            (0, "门状态", lambda v: "关闭" if v & 1 else "打开"),
            (1, "PM2.5 电源", lambda v: "高电平" if v & 2 else "低电平"),
            (2, "PM2.5 复位", lambda v: "高电平" if v & 4 else "低电平"),
            (3, "蓝牙暂停键", lambda v: "高电平" if v & 8 else "低电平"),
            (4, "蓝牙音量-", lambda v: "高电平" if v & 16 else "低电平"),
            (5, "蓝牙音量+", lambda v: "高电平" if v & 32 else "低电平"),
            (6, "蓝牙静音", lambda v: "静音" if v & 64 else "播放"),
            (7, "风扇", lambda v: "开启" if v & 128 else "关闭"),
            (8, "USB 选择", lambda v: "下载" if v & 256 else "充电"),
            (9, "LORA 电源", lambda v: "高电平" if v & 512 else "低电平"),
            (10, "LORA 模式 M1", lambda v: f"M1={(v & 1024) >> 10}"),
            (11, "LORA 模式 M0", lambda v: f"M0={(v & 2048) >> 11}"),
            (12, "声报警", lambda v: "启用" if v & 4096 else "禁用"),
            (13, "光报警", lambda v: "启用" if v & 8192 else "禁用"),
            (14, "屏幕", lambda v: "启用" if v & 16384 else "禁用"),
        ]
        
        # 更新每个设备状态标签
        for bit_pos, name, get_status in device_states:
            if name in self.device_labels:
                status_text = get_status(value)
                label_info = self.device_labels[name]
                label_info['label'].config(
                    text=status_text,
                    foreground=self._device_status_fg_color(name, status_text),
                )
    
    def _init_device_display(self):
        """初始化设备状态显示（只调用一次）"""
        # 定义设备状态位
        device_states = [
            (0, "门状态", lambda v: "关闭" if v & 1 else "打开"),
            (1, "PM2.5 电源", lambda v: "高电平" if v & 2 else "低电平"),
            (2, "PM2.5 复位", lambda v: "高电平" if v & 4 else "低电平"),
            (3, "蓝牙暂停键", lambda v: "高电平" if v & 8 else "低电平"),
            (4, "蓝牙音量-", lambda v: "高电平" if v & 16 else "低电平"),
            (5, "蓝牙音量+", lambda v: "高电平" if v & 32 else "低电平"),
            (6, "蓝牙静音", lambda v: "静音" if v & 64 else "播放"),
            (7, "风扇", lambda v: "开启" if v & 128 else "关闭"),
            (8, "USB 选择", lambda v: "下载" if v & 256 else "充电"),
            (9, "LORA 电源", lambda v: "高电平" if v & 512 else "低电平"),
            (10, "LORA 模式 M1", lambda v: f"M1={(v & 1024) >> 10}"),
            (11, "LORA 模式 M0", lambda v: f"M0={(v & 2048) >> 11}"),
            (12, "声报警", lambda v: "启用" if v & 4096 else "禁用"),
            (13, "光报警", lambda v: "启用" if v & 8192 else "禁用"),
            (14, "屏幕", lambda v: "启用" if v & 16384 else "禁用"),
        ]
        
        # 8 列布局，每列 2 个（15 个状态位 / 8 列 ≈ 2 个/列）
        items_per_col = 2  # 每列 2 个
        total_items = len(device_states)
        num_cols = 8  # 8 列
        
        # 创建 8 列
        for col_idx in range(num_cols):
            col_frame = tk.Frame(self.device_scrollable_frame, bg=self.root.cget('bg'))
            col_frame.grid(row=0, column=col_idx, padx=1, pady=1, sticky="n")
            
            start_idx = col_idx * items_per_col
            end_idx = min(start_idx + items_per_col, total_items)
            
            for i in range(start_idx, end_idx):
                bit_pos, name, get_status = device_states[i]
                label_frame = ttk.LabelFrame(col_frame, text=name, padding=2)
                label_frame.pack(fill=tk.X, pady=0, ipadx=1, ipady=0)
                
                value_label = ttk.Label(
                    label_frame, text="-", font=("Microsoft YaHei", 9),
                    foreground="black",
                )
                value_label.pack(anchor=tk.W, padx=2, pady=0)
                
                # 保存标签引用
                self.device_labels[name] = {
                    'label': value_label,
                    'bit_pos': bit_pos,
                    'get_status': get_status
                }
    
    def update_register_display(self, reg_addr: int, value: int):
        """更新寄存器显示"""
        self._reg_raw_cache[reg_addr] = value
        if reg_addr in self.register_values:
            reg_info = self.register_values[reg_addr]
            # 有数据时使用浅橙色
            reg_info['label'].config(text=reg_info['convert'](value), foreground=reg_info['data_color'])
            
            # 特殊处理：更新报警状态和设备状态解析框
            if reg_addr == 13:  # 报警状态
                self.update_alarm_status_display(value)
            elif reg_addr == 15:  # 设备状态
                self.update_device_status_display(value)

        if reg_addr in self._THRESHOLD_ZERO_ALARM_MAP:
            name, bit_pos = self._THRESHOLD_ZERO_ALARM_MAP[reg_addr]
            if value == 0:
                self._threshold_zero_alarms.add(name)
            else:
                self._threshold_zero_alarms.discard(name)
            if hasattr(self, '_alarm_initialized') and self._alarm_initialized:
                self._update_alarm_label(name, bit_pos, self._last_alarm_status)
    
    def log_rx(self, msg: str, use_frame_color: bool = False):
        """仅写入接收窗（绿色）。"""
        if use_frame_color:
            # 使用当前帧的颜色
            color = self.rx_colors[self.rx_color_index % len(self.rx_colors)]
            self.rx_text.insert(tk.END, msg, ('color_' + color))
            self.rx_color_index += 1
        else:
            self.rx_text.insert(tk.END, msg)
        self.rx_text.see(tk.END)
    
    def log_tx(self, msg: str, use_frame_color: bool = False):
        """仅写入发送窗（橙色）。"""
        if getattr(self, '_ui_mode_cache', 'tcp') == 'tcp' and not getattr(self, '_tcp_log_tx_enabled', True):
            return
        if use_frame_color:
            # 使用当前帧的颜色
            color = self.tx_colors[self.tx_color_index % len(self.tx_colors)]
            self.tx_text.insert(tk.END, msg, ('color_' + color))
            self.tx_color_index += 1
        else:
            self.tx_text.insert(tk.END, msg)
        self.tx_text.see(tk.END)
    
    def log_receive(self, msg: str, channel: str = None):
        """记录接收日志（用于 OTA 等内部事件，显示在调试信息窗口）"""
        if not self._should_show_log(msg, channel):
            return
        timestamp = time.strftime("%H:%M:%S")
        
        # 使用彩色显示（每帧不同颜色）
        color = self.rx_colors[self.debug_color_index % len(self.rx_colors)]
        self.debug_color_index += 1
        
        # 显示时间戳（黑色）
        self.debug_text.insert(tk.END, f"[{timestamp}] ", 'black')
        # 显示消息（彩色）
        self.debug_text.insert(tk.END, f"{msg}\n", ('debug_color_' + color))
        
        self.debug_text.see(tk.END)

    def _on_auto_ack_toggle(self) -> None:
        """同步自动应答勾选状态（读线程只读 _auto_ack_enabled，不访问 Tk）"""
        if getattr(self, '_ota_auto_ack_restore', None) is not None:
            self.auto_ack_var.set(False)
            self._auto_ack_enabled = False
            return
        self._auto_ack_enabled = bool(self.auto_ack_var.get())

    def _on_tcp_log_toggle(self) -> None:
        """同步 TCP 模式下调试/发送/接收窗显示开关（读线程只读 _tcp_log_*_enabled）"""
        self._tcp_log_debug_enabled = bool(self.tcp_log_debug_var.get())
        self._tcp_log_tx_enabled = bool(self.tcp_log_tx_var.get())
        self._tcp_log_rx_enabled = bool(self.tcp_log_rx_var.get())

    def _get_ota_packet_max_bytes(self) -> int:
        """协议指令框：单帧最大字节（128 / 224）"""
        try:
            n = int(self.ota_packet_max_var.get())
        except (ValueError, tk.TclError, AttributeError):
            n = 224
        return 224 if n == 224 else 128

    def _calc_ota_packet_size(self, remaining: int) -> int:
        """按当前单帧上限计算本包有效字节数（2 字节对齐，供写多寄存器）"""
        if remaining <= 0:
            return 0
        max_bytes = getattr(self, '_ota_packet_max_bytes', None)
        if max_bytes is None:
            max_bytes = self._get_ota_packet_max_bytes()
        packet_size = min(int(max_bytes), remaining)
        packet_size = (packet_size // 2) * 2
        if packet_size <= 0:
            packet_size = min(2, remaining)
        return packet_size

    def _set_ota_packet_max_widgets_state(self, state) -> None:
        for w in getattr(self, 'ota_packet_max_widgets', []):
            try:
                w.config(state=state)
            except tk.TclError:
                pass

    def _suspend_auto_ack_for_ota(self) -> None:
        """TCP OTA 期间临时关闭指令应答，结束后由 _restore_auto_ack_after_ota 恢复"""
        if self._ota_auto_ack_restore is not None:
            return
        self._ota_auto_ack_restore = bool(self._auto_ack_enabled)
        self._auto_ack_enabled = False
        self.auto_ack_var.set(False)
        if hasattr(self, 'auto_ack_checkbtn'):
            self.auto_ack_checkbtn.config(state=tk.DISABLED)
        self._set_ota_packet_max_widgets_state(tk.DISABLED)

    def _restore_auto_ack_after_ota(self) -> None:
        """OTA 结束：若开始前勾选了指令应答则恢复"""
        saved = self._ota_auto_ack_restore
        self._ota_auto_ack_restore = None
        if saved is None:
            return
        self._auto_ack_enabled = saved
        self.auto_ack_var.set(saved)
        if hasattr(self, 'auto_ack_checkbtn'):
            self.auto_ack_checkbtn.config(state=tk.NORMAL)
        self._set_ota_packet_max_widgets_state(tk.NORMAL)

    def _tcp_ack_send_ready(self) -> bool:
        """指令应答只经发送口；双端口时数据口短暂断开不应阻止回 ACK"""
        self.tcp_dual.sync_connection_state()
        return self.tcp_dual.check_ctrl_alive()

    def _send_active_upload_ack(self, ack: bytes, invalid_prefix: bytes = b''):
        """收到 0x23/0x25 主动上传后回 0x20/0x16，满足 Net_TxQueue_Push + NET_TRANSMIT_ACK"""
        if not self._tcp_ack_send_ready():
            return
        ack_hex = ' '.join(f'{b:02X}' for b in ack)
        if not self.tcp_dual.send(ack):
            return

        def _show_ack():
            if not self._should_show_tcp_tx_panel():
                return
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            color = self.tx_colors[self.tx_color_index % len(self.tx_colors)]
            self.tx_color_index += 1
            self.tx_text.insert(tk.END, f"[{timestamp}]\n", 'black')
            self.tx_text.insert(tk.END, ack_hex + "\n", ('color_' + color))
            self.tx_text.see(tk.END)

        self._safe_ui_after(0, _show_ack)
    
    def _send_modbus_ack(self, ack: bytes, invalid_prefix: bytes = b''):
        """
        发送 Modbus 应答帧（上位机作为从机应答所有主机请求）
        支持：0x03→0x13, 0x05→0x15, 0x06→0x16, 0x10→0x20, 0x23→0x20, 0x25→0x16
        """
        if not self._tcp_ack_send_ready():
            return
        ack_hex = ' '.join(f'{b:02X}' for b in ack)
        if not self.tcp_dual.send(ack):
            return

        def _show_ack():
            if not self._should_show_tcp_tx_panel():
                return
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            color = self.tx_colors[self.tx_color_index % len(self.tx_colors)]
            self.tx_color_index += 1
            self.tx_text.insert(tk.END, f"[{timestamp}]\n", 'black')
            self.tx_text.insert(tk.END, ack_hex + "\n", ('color_' + color))
            self.tx_text.see(tk.END)

        self._safe_ui_after(0, _show_ack)
    
    def log_debug(self, msg: str, channel: str = None):
        """记录调试日志（显示在调试信息窗口）"""
        if not self._should_show_log(msg, channel):
            return
        timestamp = time.strftime("%H:%M:%S")
        
        # 使用彩色显示（每帧不同颜色）
        color = self.rx_colors[self.debug_color_index % len(self.rx_colors)]
        self.debug_color_index += 1
        
        # 显示时间戳（黑色）
        self.debug_text.insert(tk.END, f"[{timestamp}] ", 'black')
        # 显示消息（彩色）
        self.debug_text.insert(tk.END, f"{msg}\n", ('debug_color_' + color))
        
        self.debug_text.see(tk.END)

    def clear_rx(self):
        self.rx_text.delete(1.0, tk.END)

    def clear_tx(self):
        self.tx_text.delete(1.0, tk.END)
    
    def clear_history(self):
        self.history_text.delete(1.0, tk.END)
    
    def append_5min_history(self, line: str):
        """追加一条 5 分钟值记录（彩色轮询，一行一条）"""
        color = self.rx_colors[self.history_color_index % len(self.rx_colors)]
        self.history_color_index += 1
        self.history_text.insert(tk.END, line + "\n", ('history_color_' + color,))
        self.history_text.see(tk.END)
    
    def clear_debug(self):
        self.debug_text.delete(1.0, tk.END)
    
    def _populate_serial_ports(self):
        """填充串口列表（与 update_port_list 格式一致，并保持当前选中项）"""
        try:
            self._refresh_serial_port_combo()
        except Exception as e:
            print(f"获取串口列表失败：{e}")
    
    def _on_udp_local_ip_combo_click(self, event=None):
        """展开本地 IP 下拉前刷新列表（后台执行，不阻塞 UI）"""
        prefer = self.udp_local_ip_combo.get().strip() if hasattr(self, 'udp_local_ip_combo') else None
        if prefer == "0.0.0.0":
            prefer = None
        now = time.time()
        if self._local_ip_cache is not None and (now - self._local_ip_cache_ts) < LOCAL_IP_CACHE_TTL:
            return
        self._populate_local_ips(prefer_ip=prefer, force_refresh=True, background=True)

    def _enumerate_ipv4_addresses(self, force_refresh: bool = False) -> List[str]:
        """枚举本机 IPv4（多网卡），排除回环"""
        now = time.time()
        if (
            not force_refresh
            and self._local_ip_cache is not None
            and (now - self._local_ip_cache_ts) < LOCAL_IP_CACHE_TTL
        ):
            return list(self._local_ip_cache)

        ips = set()

        def _add_ip(ip: str) -> None:
            ip = (ip or "").strip()
            if not ip or ip.startswith("127.") or ip == "0.0.0.0":
                return
            try:
                socket.inet_aton(ip)
            except OSError:
                return
            ips.add(ip)

        ipconfig_ok = False
        if sys.platform == "win32":
            try:
                out = _subprocess_check_output(
                    ["ipconfig"],
                    encoding="gbk",
                    errors="ignore",
                )
                for line in out.splitlines():
                    if re.search(r"IPv4|IP Address|IP 地址", line, re.I):
                        m = re.search(r"(\d{1,3}(?:\.\d{1,3}){3})\s*$", line.strip())
                        if m:
                            _add_ip(m.group(1))
                ipconfig_ok = bool(ips)
            except Exception as e:
                print(f"[UDP] ipconfig 枚举 IP 失败：{e}")
        else:
            try:
                out = _subprocess_check_output(
                    ["ip", "-4", "addr"],
                    encoding="utf-8",
                    errors="ignore",
                )
                for m in re.finditer(r"inet ([\d.]+)/", out):
                    _add_ip(m.group(1))
                ipconfig_ok = bool(ips)
            except Exception as e:
                print(f"[UDP] ip addr 枚举 IP 失败：{e}")

        if not ipconfig_ok:
            try:
                for info in socket.getaddrinfo(socket.gethostname(), None):
                    if info[0] == socket.AF_INET:
                        _add_ip(info[4][0])
            except OSError:
                pass

            try:
                probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                probe.connect(("8.8.8.8", 80))
                _add_ip(probe.getsockname()[0])
                probe.close()
            except OSError:
                pass

        result = sorted(ips, key=lambda s: tuple(int(p) for p in s.split(".")))
        self._local_ip_cache = result
        self._local_ip_cache_ts = time.time()
        return result

    def _apply_local_ip_combo(self, ip_list: List[str], prefer_ip: str = None):
        """将 IP 列表写入下拉框（仅主线程调用）；有真实 IP 时不默认选 0.0.0.0"""
        prefer = prefer_ip or getattr(self, '_udp_bound_local_ip', None)
        if prefer in ("", "0.0.0.0"):
            prefer = None
        if prefer and prefer not in ip_list:
            ip_list = list(ip_list) + [prefer]

        if not ip_list:
            combo_values = ["0.0.0.0"]
            default_ip = "0.0.0.0"
        else:
            combo_values = list(ip_list) + ["0.0.0.0"]
            default_ip = prefer if prefer in combo_values else ip_list[0]

        if hasattr(self, 'udp_local_ip_combo'):
            old_values = list(self.udp_local_ip_combo['values'])
            if old_values == combo_values:
                current = self.udp_local_ip_combo.get().strip()
                if default_ip in combo_values and current != default_ip:
                    self.udp_local_ip_combo.current(combo_values.index(default_ip))
                return
            self.udp_local_ip_combo['values'] = combo_values
            self.udp_local_ip_combo.current(combo_values.index(default_ip))
        print(f"[UDP] 本地 IP 列表：{combo_values}，默认：{default_ip}")

    def _populate_local_ips(self, prefer_ip: str = None, force_refresh: bool = False, background: bool = False):
        """获取并填充本地 IP 列表，尽量保留当前/已绑定的 IP 选项"""
        if background:
            if self._local_ip_scanning:
                return
            self._local_ip_scanning = True
            prefer = prefer_ip

            def _worker():
                try:
                    ip_list = self._enumerate_ipv4_addresses(force_refresh=force_refresh)
                    self.root.after(0, lambda: self._apply_local_ip_combo(ip_list, prefer))
                except Exception as e:
                    print(f"获取本地 IP 失败：{e}")
                    self.root.after(0, lambda: self._apply_local_ip_combo([], prefer))
                finally:
                    self._local_ip_scanning = False

            threading.Thread(target=_worker, daemon=True).start()
            return

        try:
            ip_list = self._enumerate_ipv4_addresses(force_refresh=force_refresh)
            self._apply_local_ip_combo(ip_list, prefer_ip)
        except Exception as e:
            print(f"获取本地 IP 失败：{e}")
            if hasattr(self, 'udp_local_ip_combo'):
                self.udp_local_ip_combo['values'] = ["0.0.0.0"]
                self.udp_local_ip_combo.current(0)
    
    def _sync_udp_local_ip_combo(self, local_ip: str):
        """同步本地 IP 下拉框显示（仅主线程调用）"""
        if not hasattr(self, 'udp_local_ip_combo') or not local_ip:
            return
        values = list(self.udp_local_ip_combo['values'])
        if local_ip not in values:
            values = values + [local_ip]
            self.udp_local_ip_combo['values'] = values
        try:
            self.udp_local_ip_combo.current(values.index(local_ip))
        except ValueError:
            pass
    
    def _get_system_ipv4_addresses(self) -> set:
        """获取当前系统可用的 IPv4 地址（不含回环）"""
        return set(self._enumerate_ipv4_addresses())
    
    def _is_local_ip_available(self, ip: str) -> bool:
        """检测指定本地 IP 是否仍可绑定（网卡断开时会失败）"""
        if not ip or ip == "0.0.0.0":
            return True
        try:
            probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            probe.bind((ip, 0))
            probe.close()
            return True
        except OSError:
            return False
    
    def _check_udp_connection(self) -> bool:
        """检查 UDP 绑定与所选网卡是否仍然有效"""
        if not self.udp_connected:
            return False
        if not self.udp_server.socket or not self.udp_server.bound:
            return False
        if time.time() < getattr(self, '_udp_rebind_grace_until', 0):
            return True
        if self.udp_server.read_thread and not self.udp_server.read_thread.is_alive():
            return False
        if getattr(self.udp_server, 'read_error', False):
            return False
        
        local_ip = getattr(self, '_udp_bound_local_ip', None) or self.udp_server.bind_local_ip
        if local_ip and local_ip != "0.0.0.0":
            if not self._is_local_ip_available(local_ip):
                return False
        
        try:
            self.udp_server.socket.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR)
            return True
        except OSError:
            return False
    
    def save_data(self):
        """保存收发两窗内容"""
        from tkinter import filedialog
        file_path = filedialog.asksaveasfilename(
            defaultextension=".txt",
            filetypes=[("文本文件", "*.txt"), ("所有文件", "*.*")]
        )
        if file_path:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write("---------- 接收（设备→本机） ----------\n")
                f.write(self.rx_text.get(1.0, tk.END))
                f.write("\n---------- 发送（本机→设备） ----------\n")
                f.write(self.tx_text.get(1.0, tk.END))
            messagebox.showinfo("成功", f"数据已保存到\n{file_path}")
    
    def send_quick_cmd(self, cmd_type: str):
        """发送快捷指令"""
        if not self._require_current_mode_connected():
            return
        
        # 根据模式获取设备地址
        mode = self.mode_var.get().split(' - ')[0].lower()
        if mode == "tcp":
            addr = self._get_tcp_device_addr(quiet=False)
            if addr is None:
                return
        elif mode == "udp":
            # UDP 模式不使用设备地址，使用默认值 1
            addr = 1
        else:  # serial
            try:
                addr = int(self.param_addr_var.get().strip()) if self.param_addr_var.get().strip() else 1
            except ValueError:
                messagebox.showwarning("警告", "设备地址必须是数字")
                return
        
        if cmd_type == 'reboot':
            frame = NetRawProtocol.build_write_single(addr, NetRawProtocol.REG_REBOOT, 0x0001)
            self.send_frame(frame, "快捷指令：设备重启")
            return
        elif cmd_type == 'timesync':
            if mode != 'tcp' or not self.tcp_connected:
                messagebox.showwarning("警告", "时间同步请在 TCP 模式下连接设备")
                return
            now = datetime.now()
            frame = NetRawProtocol.build_time_sync_frame(addr, now)
            self.log_receive(f"[时间同步] {now.strftime('%Y-%m-%d %H:%M:%S')}")
            self.send_frame(frame, "时间同步 reg94")
            return
        elif cmd_type == 'hist5min':
            if mode != 'tcp' or not self.tcp_connected:
                messagebox.showwarning("警告", "获取记录请在 TCP 模式下连接设备")
                return
            start = datetime(2000, 1, 1, 0, 0, 0)
            end = datetime.now()
            f_start, f_end = NetRawProtocol.build_hist_query_frames(addr, start, end)
            self.log_receive(
                f"[历史记录] 查询记录 {start.strftime('%Y-%m-%d %H:%M:%S')} ~ "
                f"{end.strftime('%Y-%m-%d %H:%M:%S')}"
            )
            self.send_frame(f_start, "历史查询：开始时间 reg108")
            time.sleep(0.05)
            self.send_frame(f_end, "历史查询：结束时间 reg112")
            return
        else:
            return

    def _parse_dose_threshold_input(self, text: str, unit: str):
        """解析剂量率阈值输入，返回 (value_usv, display_str)；失败返回 (None, error_msg)"""
        s = (text or "").strip()
        if not s:
            return None, "请输入阈值"
        try:
            val = round(float(s), 2)
        except ValueError:
            return None, "阈值必须是数字"
        if val < 0.0 or val > 999.99:
            return None, "阈值范围 0.00～999.99"
        if abs(val - round(float(s), 2)) > 1e-6 and len(s.split(".")[-1]) > 2:
            return None, "最多保留两位小数"
        u = (unit or "μSv/h").strip()
        if u not in ("μSv/h", "mSv/h"):
            return None, "单位无效"
        value_usv = NetRawProtocol.dose_display_to_usv(val, u)
        return value_usv, f"{val:.2f} {u}"

    def _get_protocol_device_addr(self):
        """获取当前连接模式下的协议从机地址"""
        mode = self.mode_var.get().split(' - ')[0].lower()
        if mode == "tcp":
            return self._get_tcp_device_addr(quiet=False)
        elif mode == "udp":
            addr = 1
        else:
            try:
                addr = int(self.param_addr_var.get().strip()) if self.param_addr_var.get().strip() else 1
            except ValueError:
                messagebox.showwarning("警告", "设备地址必须是数字")
                return None
        return addr

    def send_dose_hi_threshold(self):
        """写 reg50 剂量率上阈值"""
        if not self._require_current_mode_connected():
            return
        value_usv, info = self._parse_dose_threshold_input(
            self.dose_hi_entry.get(), self.dose_hi_unit.get())
        if value_usv is None:
            messagebox.showwarning("警告", info)
            return
        addr = self._get_protocol_device_addr()
        if addr is None:
            return
        frame = NetRawProtocol.build_dose_threshold_write(
            addr, NetRawProtocol.REG_THR_DOSE_HI, value_usv)
        self.dose_hi_entry.delete(0, tk.END)
        self.dose_hi_entry.insert(0, info.split()[0])
        self.send_frame(frame, f"设置剂量率上阈值 reg50：{info}")

    def send_dose_lo_threshold(self):
        """写 reg52 剂量率下阈值"""
        if not self._require_current_mode_connected():
            return
        value_usv, info = self._parse_dose_threshold_input(
            self.dose_lo_entry.get(), self.dose_lo_unit.get())
        if value_usv is None:
            messagebox.showwarning("警告", info)
            return
        addr = self._get_protocol_device_addr()
        if addr is None:
            return
        frame = NetRawProtocol.build_dose_threshold_write(
            addr, NetRawProtocol.REG_THR_DOSE_LO, value_usv)
        self.dose_lo_entry.delete(0, tk.END)
        self.dose_lo_entry.insert(0, info.split()[0])
        self.send_frame(frame, f"设置剂量率下阈值 reg52：{info}")

    def send_alarm_setting(self):
        """写 reg123 声/光/屏或 reg82 剂量率报警禁止掩码"""
        if not self._require_current_mode_connected():
            return
        alarm_type = (self.alarm_type_combo.get() or "").strip()
        alarm_state = (self.alarm_state_combo.get() or "").strip()
        if alarm_state not in ("打开", "关闭"):
            messagebox.showwarning("警告", "请选择打开或关闭")
            return
        enable = (alarm_state == "打开")
        addr = self._get_protocol_device_addr()
        if addr is None:
            return

        if alarm_type in ("声报警", "光报警", "屏幕"):
            bit_map = {
                "声报警": NetRawProtocol.REG_CTRL2_SOUND_BIT,
                "光报警": NetRawProtocol.REG_CTRL2_LIGHT_BIT,
                "屏幕": NetRawProtocol.REG_CTRL2_DISPLAY_BIT,
            }
            bit = bit_map[alarm_type]
            current = self._reg_raw_cache.get(NetRawProtocol.REG_CONTROL_BIT2, 0) & 0xFFFF
            if enable:
                new_val = current | (1 << bit)
            else:
                new_val = current & ~(1 << bit)
            self._reg_raw_cache[NetRawProtocol.REG_CONTROL_BIT2] = new_val
            frame = NetRawProtocol.build_control_bit2_write(addr, new_val)
            self.send_frame(frame, f"设置{alarm_type} reg123：{alarm_state}")
            return

        if alarm_type in ("剂量率上阈值", "剂量率下阈值"):
            bit_map = {
                "剂量率上阈值": NetRawProtocol.REG_ALARM_BITEN_DOSE_HI_BIT,
                "剂量率下阈值": NetRawProtocol.REG_ALARM_BITEN_DOSE_LO_BIT,
            }
            bit = bit_map[alarm_type]
            current = self._reg_raw_cache.get(NetRawProtocol.REG_ALARM_BITEN, 0) & 0xFFFFFFFF
            if enable:
                new_val = current & ~(1 << bit)
            else:
                new_val = current | (1 << bit)
            self._reg_raw_cache[NetRawProtocol.REG_ALARM_BITEN] = new_val
            frame = NetRawProtocol.build_alarm_biten_write(addr, new_val)
            self.send_frame(frame, f"设置{alarm_type} reg82：{alarm_state}")
            return

        messagebox.showwarning("警告", "请选择报警类型")

    def _parse_dev_addr_text(self, text: str):
        """解析设备地址输入，返回 (addr, display_str)；失败返回 (None, error_msg)"""
        s = (text or "").strip()
        if not s:
            return None, "请输入设备地址"
        try:
            if s.lower().startswith("0x"):
                addr = int(s, 16)
            else:
                addr = int(s)
        except ValueError:
            return None, "设备地址必须是数字"
        if not (0 <= addr <= 255):
            return None, "设备地址范围 0-255"
        return addr, f"0x{addr:02X}"

    def send_cmd_dev_addr(self):
        """写 reg121 设备地址（功能码 0x06）"""
        if not self._require_current_mode_connected():
            return
        new_addr, info = self._parse_dev_addr_text(self.cmd_dev_addr_entry.get())
        if new_addr is None:
            messagebox.showwarning("警告", info)
            return
        cur_addr = self._get_protocol_device_addr()
        if cur_addr is None:
            return
        frame = NetRawProtocol.build_write_single(
            cur_addr, NetRawProtocol.REG_DEV_ADDR, new_addr)
        self._reg_raw_cache[NetRawProtocol.REG_DEV_ADDR] = new_addr
        self.cmd_dev_addr_entry.delete(0, tk.END)
        self.cmd_dev_addr_entry.insert(0, info)
        mode = self.mode_var.get().split(' - ')[0].lower()
        if mode == "tcp":
            self._set_tcp_addr_display(new_addr)
        elif mode == "serial" and hasattr(self, 'param_addr_var'):
            self.param_addr_var.set(str(new_addr))
        self.send_frame(frame, f"设置设备地址 reg121：{info}（当前通信地址 0x{cur_addr:02X}）")

    def send_cmd_alarm_volume(self):
        """写 reg122 报警音量（功能码 0x06）"""
        if not self._require_current_mode_connected():
            return
        text = (self.cmd_alarm_volume_entry.get() or "").strip()
        if not text:
            messagebox.showwarning("警告", "请输入报警音量")
            return
        try:
            vol = int(text)
        except ValueError:
            messagebox.showwarning("警告", "报警音量必须是整数")
            return
        if not (0 <= vol <= 100):
            messagebox.showwarning("警告", "报警音量范围 0-100")
            return
        cur_addr = self._get_protocol_device_addr()
        if cur_addr is None:
            return
        
        # 设置音量值（reg122）
        frame = NetRawProtocol.build_write_single(
            cur_addr, NetRawProtocol.REG_ALARM_VOLUME, vol)
        self._reg_raw_cache[NetRawProtocol.REG_ALARM_VOLUME] = vol
        self.cmd_alarm_volume_entry.delete(0, tk.END)
        self.cmd_alarm_volume_entry.insert(0, str(vol))
        self.send_frame(frame, f"设置报警音量 reg122：{vol}%")
    
    def send_custom_cmd(self):
        """发送自定义指令"""
        if not self._require_current_mode_connected():
            return
        
        try:
            func_str = self.func_combo.get().split(' - ')[0]
            func = int(func_str, 16)
            # 根据模式获取设备地址
            mode = self.mode_var.get().split(' - ')[0].lower()
            if mode == "tcp":
                addr = self._get_tcp_device_addr(quiet=False)
                if addr is None:
                    return
            elif mode == "udp":
                # UDP 模式不使用设备地址，使用默认值 1
                addr = 1
            else:  # serial
                addr = int(self.param_addr_var.get().strip() if self.param_addr_var.get().strip() else "1")
            reg_addr = int(self.reg_addr_entry.get())
            reg_val = int(self.reg_val_entry.get())
            
            if func == NetRawProtocol.FC_READ_SINGLE_REQ:
                qty = max(1, reg_val)
                frame = NetRawProtocol.build_read_single(addr, reg_addr, qty)
            elif func == NetRawProtocol.FC_READ_MULTI_REQ:
                qty = max(1, reg_val) if reg_val > 0 else 5
                frame = NetRawProtocol.build_read_multi(addr, reg_addr, qty)
            elif func == NetRawProtocol.FC_WRITE_SINGLE_REQ:
                frame = NetRawProtocol.build_write_single(addr, reg_addr, reg_val)
            else:
                messagebox.showwarning("警告", "不支持的功能码")
                return
            
            self.send_frame(frame, f"自定义指令：0x{func:02X}")
        except ValueError as e:
            messagebox.showerror("错误", f"参数错误：{e}")
    
    def send_raw_frame(self):
        """发送原始帧"""
        if not self._require_current_mode_connected():
            return
        
        try:
            hex_str = self.hex_entry.get().replace(' ', '').replace(',', '')
            frame = bytes.fromhex(hex_str)
            if len(frame) == 6:
                frame = CRC16.append(frame)
                self.log_tx("  [提示] 已自动追加 CRC16\n")
            self.send_frame(frame, "原始帧")
        except ValueError as e:
            messagebox.showerror("错误", f"HEX 格式错误：{e}")
    
    def send_udp_data(self):
        """发送 UDP 自定义数据（与 TCP OTA 后台传输互不干扰）"""
        if not self.udp_connected or not self.udp_server.bound:
            messagebox.showwarning("警告", "请连接UDP！")
            return
        
        data_str = self.udp_data_entry.get().strip()
        if not data_str:
            messagebox.showwarning("警告", "请输入数据")
            return
        
        try:
            # 直接发送文本数据
            data = data_str.encode('utf-8')
            # 获取远端组播地址和端口
            remote_ip = self.udp_ip_entry.get().strip()
            remote_port = int(self.udp_port_entry.get().strip())
            if self.udp_server.send(data, (remote_ip, remote_port)):
                # 使用时间戳和颜色轮询显示（与 send_frame 保持一致）
                timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                
                # 显示时间戳（黑色，单独一行）
                self.tx_text.insert(tk.END, f"[{timestamp}]\n", 'black')
                
                # 一帧一种颜色（所有行使用相同颜色）
                color = self.tx_colors[self.tx_color_index % len(self.tx_colors)]
                self.tx_color_index += 1
                
                # 显示数据内容（彩色）
                self.tx_text.insert(tk.END, data_str + "\n", ('color_' + color))
                
                # 自动滚动到底部
                self.tx_text.see(tk.END)
            else:
                messagebox.showerror("错误", "发送失败")
        except Exception as e:
            messagebox.showerror("错误", f"发送失败：{e}")
    
    def simulate_device(self):
        """模拟设备发送数据"""
        if not self.udp_connected:
            messagebox.showwarning("警告", "请连接UDP！")
            return
        
        # 发送模拟设备数据
        simulate_data = "RAW-I-V2026,1801RAW0103,192.168.2.2,5001,5000,1,0,0"
        try:
            data = simulate_data.encode('utf-8')
            # 获取远端组播地址和端口
            remote_ip = self.udp_ip_entry.get().strip()
            remote_port = int(self.udp_port_entry.get().strip())
            if self.udp_server.send(data, (remote_ip, remote_port)):
                # 使用时间戳和颜色轮询显示（与 send_frame 保持一致）
                timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                
                # 显示时间戳（黑色，单独一行）
                self.tx_text.insert(tk.END, f"[{timestamp}]\n", 'black')
                
                # 一帧一种颜色（所有行使用相同颜色）
                color = self.tx_colors[self.tx_color_index % len(self.tx_colors)]
                self.tx_color_index += 1
                
                # 显示数据内容（彩色）
                self.tx_text.insert(tk.END, simulate_data + "\n", ('color_' + color))
                
                # 自动滚动到底部
                self.tx_text.see(tk.END)
            else:
                messagebox.showerror("错误", "发送失败")
        except Exception as e:
            messagebox.showerror("错误", f"发送失败：{e}")
    
    def send_data(self, frame: bytes) -> bool:
        """根据当前模式发送数据"""
        try:
            if self.current_mode == "serial":
                if not self._is_serial_port_open():
                    return False
                # 文本串口指令帧已含 \r\n 时，再追加一次 \r\n（与旧版 send_frame 行为一致）
                if frame.endswith(b"\r\n"):
                    to_send = frame + b"\r\n"
                else:
                    to_send = frame
                return self.serial_monitor.send(to_send)
            elif self.current_mode == "tcp":
                return self.tcp_dual.send(frame)
            elif self.current_mode == "udp":
                return self.udp_server.send(frame)
        except Exception as e:
            # 发送失败，检测连接状态
            print(f"发送失败：{e}")
            return False
        return False
    
    def send_ota_data(self, frame: bytes) -> bool:
        """发送 OTA 数据 - 始终通过 TCP 通道（使用异步发送队列）"""
        try:
            # OTA 数据直接通过 TCP 发送，不经过模式判断
            # OTA 已有应用层重试机制，不需要 TCP 层的重试
            return self.tcp_dual.send(frame)
        except Exception as e:
            # 发送失败
            print(f"OTA 发送失败：{e}")
            return False
    
    def send_frame(self, frame: bytes, desc: str):
        """发送帧（TCP/UDP 二进制或串口二进制协议；文本串口指令请用 _send_serial_text_cmd）"""
        to_send = frame
        
        # 发送数据
        self.send_data(to_send)
    
    def browse_iap_file(self):
        """选择 IAP 固件文件"""
        from tkinter import filedialog
        file_path = filedialog.askopenfilename(
            title="选择 IAP 固件文件",
            filetypes=[("固件文件", "*.bin *.hex"), ("所有文件", "*.*")]
        )
        if file_path:
            # 如果是 hex 文件，自动转换为 bin 文件
            if file_path.lower().endswith('.hex'):
                try:
                    bin_path = self.convert_hex_to_bin(file_path)
                    if bin_path:
                        # 自动使用转换后的 bin 文件
                        self.iap_path_var.set(bin_path)
                        # 滚动到右侧（显示文件名）
                        self.iap_path_entry.xview_moveto(1.0)
                        # 计算并显示 CRC32
                        self._calculate_and_show_crc32(bin_path, "IAP")
                    else:
                        self.log_receive("[OTA] IAP HEX 转 BIN 失败")
                        self.iap_path_var.set("")
                except Exception as e:
                    self.log_receive(f"[OTA] IAP HEX 转 BIN 错误：{e}")
                    self.iap_path_var.set("")
            else:
                # bin 文件直接使用
                self.iap_path_var.set(file_path)
                # 滚动到右侧（显示文件名）
                self.iap_path_entry.xview_moveto(1.0)
                # 计算并显示 CRC32
                self._calculate_and_show_crc32(file_path, "IAP")
    
    def browse_app_file(self):
        """选择 APP 固件文件"""
        from tkinter import filedialog
        file_path = filedialog.askopenfilename(
            title="选择 APP 固件文件",
            filetypes=[("固件文件", "*.bin *.hex"), ("所有文件", "*.*")]
        )
        if file_path:
            # 如果是 hex 文件，自动转换为 bin 文件
            if file_path.lower().endswith('.hex'):
                try:
                    bin_path = self.convert_hex_to_bin(file_path)
                    if bin_path:
                        # 自动使用转换后的 bin 文件
                        self.app_path_var.set(bin_path)
                        # 滚动到右侧（显示文件名）
                        self.app_path_entry.xview_moveto(1.0)
                        # 计算并显示 CRC32
                        self._calculate_and_show_crc32(bin_path, "APP")
                    else:
                        self.log_receive("[OTA] APP HEX 转 BIN 失败")
                        self.app_path_var.set("")
                except Exception as e:
                    self.log_receive(f"[OTA] APP HEX 转 BIN 错误：{e}")
                    self.app_path_var.set("")
            else:
                # bin 文件直接使用
                self.app_path_var.set(file_path)
                # 滚动到右侧（显示文件名）
                self.app_path_entry.xview_moveto(1.0)
                # 计算并显示 CRC32
                self._calculate_and_show_crc32(file_path, "APP")
    
    def browse_firmware_file(self):
        """选择固件文件"""
        from tkinter import filedialog
        file_path = filedialog.askopenfilename(
            title="选择固件文件",
            filetypes=[("固件文件", "*.bin *.hex"), ("所有文件", "*.*")]
        )
        if file_path:
            # 如果是 hex 文件，自动转换为 bin 文件
            if file_path.lower().endswith('.hex'):
                try:
                    bin_path = self.convert_hex_to_bin(file_path)
                    if bin_path:
                        # 自动使用转换后的 bin 文件
                        self.firmware_path_var.set(bin_path)
                        # 计算并显示 CRC32
                        self._calculate_and_show_crc32(bin_path, "BootLoader")
                    else:
                        self.log_receive("[OTA] HEX 转 BIN 失败")
                        self.firmware_path_var.set("")
                except Exception as e:
                    self.log_receive(f"[OTA] HEX 转 BIN 错误：{e}")
                    self.firmware_path_var.set("")
            else:
                # bin 文件直接使用
                self.firmware_path_var.set(file_path)
                # 计算并显示 CRC32
                self._calculate_and_show_crc32(file_path, "BootLoader")
    
    def _calculate_and_show_crc32(self, file_path: str, file_type: str):
        """计算并显示固件文件的 CRC32 值"""
        try:
            # 读取固件文件
            with open(file_path, 'rb') as f:
                firmware_data = f.read()
            
            # 计算 CRC32（使用与单片机相同的算法，查表法优化）
            crc32_value = self._bl_crc32(firmware_data)
            
            # 显示 CRC32 值
            self.log_receive(f"[OTA] {file_type} 固件文件 CRC32: 0x{crc32_value:08X}")
            
            # 保存 CRC32 值和固件数据，供 OTA 时使用
            if file_type == "IAP":
                self.iap_crc32 = crc32_value
            elif file_type == "APP":
                self.app_crc32 = crc32_value
            elif file_type == "BootLoader":
                self.bootloader_crc32 = crc32_value
            
        except Exception as e:
            self.log_receive(f"[OTA] 计算{file_type}固件 CRC32 失败：{e}")
    
    # ==================== BootLoader 模式 DFU 更新相关方法 ====================

    def _bl_is_serial_permanently_disconnected(self) -> bool:
        """用户主动断开或未记住端口：不应自动重试 BootLoader 传输"""
        if self._serial_manual_disconnect or self.serial_monitor.manual_disconnect:
            return True
        return not self._normalize_serial_port(self._connected_serial_port)

    def _bl_check_abort(self) -> Optional[str]:
        if self.bootloader_update_aborted:
            return self._BL_CANCEL
        if self._bl_is_serial_permanently_disconnected():
            return self._BL_SERIAL_LOST
        return None

    def _bl_prepare_serial_for_retry(self) -> None:
        """重试前尽量恢复串口句柄（设备复位后 COM 可能短暂消失）"""
        port = self._normalize_serial_port(self._connected_serial_port)
        if not port:
            return
        baudrate = getattr(self.serial_monitor, "target_baudrate", None) or 921600
        if not self.serial_monitor.is_open():
            self.serial_monitor.connect(port, baudrate)

    def _sync_serial_link_ui(self) -> None:
        self.connected = (
            self.serial_connected or self.tcp_connected or self.udp_connected
        )
        self._update_serial_status_ui()
        if self.current_mode == "serial":
            self.current_status_label.config(
                text=self.serial_status_label.cget("text"),
                foreground=self.serial_status_label.cget("foreground"),
            )
            self.current_connect_btn.config(text=self.serial_connect_btn.cget("text"))

    def _on_bootloader_post_update_reconnect_failed(self) -> None:
        """BootLoader 完成后自动重连失败：同步为未连接，并启动看门狗等待插回"""
        self.serial_connected = False
        self.serial_monitor.suspend_link()
        self._serial_manual_disconnect = False
        self.serial_monitor.manual_disconnect = False
        self.is_reconnecting = False
        self._sync_serial_link_ui()
        ui_mode = self.mode_var.get().split(" - ")[0].lower()
        if ui_mode == "serial":
            self._set_input_widgets_state(tk.NORMAL)
        self.log_receive("[BootLoader] 更新完成，串口自动重连失败，请手动连接")
        if self._connected_serial_port:
            self.serial_is_monitoring = False
            self.start_connection_monitor("serial")

    def _on_bootloader_post_update_reconnected(self, port: str, baudrate: int) -> None:
        self.serial_connected = True
        self.connected = (
            self.serial_connected or self.tcp_connected or self.udp_connected
        )
        self.current_port = port
        self._connected_serial_port = port
        self._serial_manual_disconnect = False
        self.serial_monitor.manual_disconnect = False
        self._refresh_serial_port_combo(prefer_port=port)
        self._sync_serial_link_ui()
        ui_mode = self.mode_var.get().split(" - ")[0].lower()
        if ui_mode != "serial":
            self._apply_connection_ui(ui_mode)
        self.serial_is_monitoring = False
        self.start_connection_monitor("serial")
        self.log_receive(
            f"[BootLoader] 串口已自动重连：{port} {baudrate}"
            + (f"（当前界面为 {ui_mode.upper()} 模式）" if ui_mode != "serial" else "")
        )
    
    def _bl_detect_dfu_devices(self):
        """检测 USB DFU 设备（使用 Windows WMI 接口，不依赖 libusb）"""
        try:
            # 方法 1：使用 WMI 枚举 USB 设备（Windows 原生，不需要 libusb）
            try:
                # 确保 pywin32 路径已添加
                import site
                site.addsitedir(os.path.join(os.path.dirname(sys.executable), 'Lib', 'site-packages'))
                site.addsitedir(os.path.join(os.path.dirname(sys.executable), 'Lib', 'site-packages', 'win32'))
                site.addsitedir(os.path.join(os.path.dirname(sys.executable), 'Lib', 'site-packages', 'Pythonwin'))
                
                import win32com.client
                
                # 创建 WMI 连接
                wmi_obj = win32com.client.GetObject("winmgmts:")
                
                # 查找 STM32 DFU 设备
                dfu_devices = []
                
                # 直接查询 Win32_PnPEntity（最可靠的方法）
                try:
                    pnp_entities = wmi_obj.InstancesOf("Win32_PnPEntity")
                    
                    for device in pnp_entities:
                        try:
                            device_name = device.Name if hasattr(device, 'Name') else "Unknown"
                            device_id = device.DeviceID if hasattr(device, 'DeviceID') else ""
                            pnp_id = device.PNPDeviceID if hasattr(device, 'PNPDeviceID') else ""
                            
                            # 检查是否是 STM32 DFU 设备
                            if '0483' in pnp_id.upper() and ('DF11' in pnp_id.upper() or 'DF12' in pnp_id.upper()):
                                vid = 0x0483
                                pid = 0xDF11 if 'DF11' in pnp_id.upper() else 0xDF12
                                
                                dfu_devices.append({
                                    'vid': vid,
                                    'pid': pid,
                                    'product': device_name,
                                    'device_id': device_id,
                                    'pnp_id': pnp_id
                                })
                        except Exception as e:
                            continue
                    
                    if dfu_devices:
                        return dfu_devices
                        
                except Exception as e:
                    pass
                
                # 备用：查询 USB 控制器
                try:
                    self.log_debug(f"[USB DFU] 查询 Win32_USBController...")
                    usb_controllers = wmi_obj.InstancesOf("Win32_USBController")
                    
                    for device in usb_controllers:
                        try:
                            device_name = device.Name if hasattr(device, 'Name') else "Unknown"
                            device_id = device.DeviceID if hasattr(device, 'DeviceID') else ""
                            pnp_id = device.PNPDeviceID if hasattr(device, 'PNPDeviceID') else ""
                            
                            # 检查是否是 STM32 DFU 设备
                            if 'DF11' in pnp_id.upper() or 'DF12' in pnp_id.upper() or 'STM32' in device_name.upper():
                                vid = 0x0483
                                pid = 0xDF11 if 'DF11' in pnp_id.upper() else 0xDF12
                                
                                dfu_devices.append({
                                    'vid': vid,
                                    'pid': pid,
                                    'product': device_name,
                                    'device_id': device_id,
                                    'pnp_id': pnp_id
                                })
                        except Exception as e:
                            continue
                    
                    if dfu_devices:
                        return dfu_devices
                        
                except Exception as e:
                    pass
                
            except Exception as e:
                pass
            
            # 方法 2：尝试使用 pyusb（如果 libusb 可用）
            try:
                import usb.core
                import usb.util
                
                self.log_debug(f"[BootLoader] 尝试使用 pyusb 扫描...")
                
                # 尝试加载 libusb 后端
                backend = None
                try:
                    import usb.backend.libusb1
                    backend = usb.backend.libusb1.get_backend()
                    self.log_debug(f"[BootLoader] libusb1 后端：{'找到' if backend else '未找到'}")
                except Exception as e:
                    self.log_debug(f"[BootLoader] 加载 libusb1 后端失败：{e}")
                
                if backend:
                    dfu_devices = []
                    pid_list = [0xDF11, 0xDF12, 0xDF10]
                    
                    for pid in pid_list:
                        try:
                            device = usb.core.find(idVendor=0x0483, idProduct=pid)
                            if device:
                                dfu_devices.append({
                                    'device': device,
                                    'vid': device.idVendor,
                                    'pid': device.idProduct,
                                    'product': usb.util.get_string(device, device.iProduct) if device.iProduct else "Unknown",
                                    'serial': usb.util.get_string(device, device.iSerialNumber) if device.iSerialNumber else "Unknown"
                                })
                                self.log_debug(f"[BootLoader] ✓ pyusb 检测到 DFU 设备：VID=0x{device.idVendor:04X}, PID=0x{device.idProduct:04X}")
                        except Exception as e:
                            self.log_debug(f"[BootLoader] pyusb 检测 PID 0x{pid:04X} 失败：{e}")
                    
                    if dfu_devices:
                        return dfu_devices
                else:
                    self.log_debug(f"[BootLoader] pyusb 不可用（libusb 后端缺失）")
                    
            except ImportError:
                self.log_debug(f"[BootLoader] pyusb 未安装")
            except Exception as e:
                self.log_debug(f"[BootLoader] pyusb 扫描异常：{e}")
            
            # 如果所有方法都失败
            self.log_debug(f"[BootLoader] 未检测到 DFU 设备")
            return None
            
        except Exception as e:
            self.log_debug(f"[BootLoader] 检测 USB 设备异常：{e}")
            import traceback
            self.log_debug(f"[BootLoader] 详细错误：{traceback.format_exc()}")
            return None
    
    def _bl_calculate_checksum(self, data: bytes) -> int:
        """计算 BootLoader 命令校验和（XOR 所有字节）"""
        checksum = 0
        for byte in data:
            checksum ^= byte
        return checksum
    
    def _bl_enter_bootloader_mode(self, port) -> bool:
        """通过串口引脚控制进入 BootLoader 模式
        
        引脚控制序列：
        1. 拉高 RTS
        2. 拉低 DTR
        3. 拉低 RTS
        4. 释放 DTR
        
        注意：此方法只进行引脚控制，不进行串口同步
        同步由 _bl_sync_bootloader 方法单独处理
        """
        try:
            # 检查中止标志
            if self.bootloader_update_aborted:
                return False
            
            # 配置串口参数（BootLoader 模式：115200, 8E1）
            port.baudrate = 115200
            port.bytesize = serial.EIGHTBITS
            port.parity = serial.PARITY_EVEN
            port.stopbits = serial.STOPBITS_ONE
            # 修复：使用正确的串口流控制属性
            try:
                # 某些 pyserial 版本使用 rtscts=False/xonxoff=False 来禁用流控制
                port.rtscts = False
                port.xonxoff = False
            except:
                pass
            port.timeout = 0.5
            
            # 清空缓冲区
            port.reset_input_buffer()
            port.reset_output_buffer()
            
            # 检查中止标志
            if self.bootloader_update_aborted:
                return False
            
            # 引脚控制序列
            port.setRTS(True)   # 1. 拉高 RTS
            # 非阻塞等待 0.1 秒
            start = time.time()
            while time.time() - start < 0.1:
                if self.bootloader_update_aborted:
                    return False
                time.sleep(0.01)
            
            # 检查中止标志
            if self.bootloader_update_aborted:
                return False
            
            port.setDTR(False)  # 2. 拉低 DTR
            # 非阻塞等待 0.1 秒
            start = time.time()
            while time.time() - start < 0.1:
                if self.bootloader_update_aborted:
                    return False
                time.sleep(0.01)
            
            # 检查中止标志
            if self.bootloader_update_aborted:
                return False
            
            port.setRTS(False)  # 3. 拉低 RTS
            # 非阻塞等待 0.5 秒
            start = time.time()
            while time.time() - start < 0.5:
                if self.bootloader_update_aborted:
                    return False
                time.sleep(0.01)
            
            # 检查中止标志
            if self.bootloader_update_aborted:
                return False
            
            port.setDTR(True)   # 4. 释放 DTR（拉高）
            # 非阻塞等待 0.1 秒
            start = time.time()
            while time.time() - start < 0.1:
                if self.bootloader_update_aborted:
                    return False
                time.sleep(0.01)
            
            # 检查中止标志
            if self.bootloader_update_aborted:
                return False
            
            # 给单片机时间准备
            self.log_debug(f"[USB DFU] 等待单片机进入 BootLoader...")
            # 非阻塞等待 1.0 秒
            start = time.time()
            while time.time() - start < 1.0:
                if self.bootloader_update_aborted:
                    return False
                time.sleep(0.01)
            
            # 检查中止标志
            if self.bootloader_update_aborted:
                return False
            
            # 总是返回 True，因为引脚控制总是成功的
            # 同步由 _bl_sync_bootloader 方法处理
            return True
            
        except Exception as e:
            self.log_debug(f"[BootLoader] 进入 BootLoader 模式异常：{e}")
            return False
    
    def _bl_send_command(self, port, cmd: bytes) -> bool:
        """发送 BootLoader 命令并等待 ACK"""
        try:
            port.write(cmd)
            time.sleep(0.01)
            
            # 等待响应
            timeout = time.time() + 0.5
            while time.time() < timeout:
                if port.in_waiting > 0:
                    response = port.read(port.in_waiting)
                    if response and response[0] == self.BL_CMD_ACK:
                        return True
                    elif response and response[0] == self.BL_CMD_NACK:
                        self.log_debug(f"[BootLoader] 命令被拒绝：{cmd.hex().upper()}")
                        return False
            
            self.log_debug(f"[BootLoader] 命令超时：{cmd.hex().upper()}")
            return False
            
        except Exception as e:
            self.log_debug(f"[BootLoader] 发送命令异常：{e}")
            return False
    
    def _bl_erase_flash(self, port, total_pages: int) -> bool:
        """擦除 Flash（扩展擦除命令）"""
        try:
            self.log_debug(f"[BootLoader] 开始擦除 Flash，共{total_pages}页")
            
            # 发送扩展擦除命令 (0x44 0xBB)
            cmd = bytes([self.BL_CMD_ERASE_EXT, 0xBB])
            if not self._bl_send_command(port, cmd):
                self.log_debug(f"[BootLoader] 擦除命令发送失败")
                return False
            
            # 发送擦除参数（单页模式）
            for page in range(total_pages):
                if self.bootloader_update_aborted:
                    return False
                
                # 页擦除参数：页号（3 字节）+ 校验和
                params = bytes([
                    0x00,  # 页号高字节
                    0x00,  # 页号中字节
                    0x00,  # 起始页
                    page,  # 当前页号
                    page   # 校验和（简化）
                ])
                
                port.write(params)
                time.sleep(0.01)
                
                # 等待擦除完成
                timeout = time.time() + 1.0
                while time.time() < timeout:
                    if port.in_waiting > 0:
                        response = port.read(port.in_waiting)
                        if response and response[0] == self.BL_CMD_ACK:
                            progress = (page + 1) / total_pages * 100
                            self.log_debug(f"[BootLoader] 擦除页{page}成功，进度：{progress:.1f}%")
                            self.root.after(0, lambda p=progress: self.update_progress(p))
                            break
                        elif response and response[0] == self.BL_CMD_NACK:
                            self.log_debug(f"[BootLoader] 擦除页{page}失败")
                            return False
                
                time.sleep(0.05)  # 页间延时
            
            self.log_debug(f"[BootLoader] Flash 擦除完成")
            self.log_debug(f"[BootLoader] 开始固件传输")
            return True
            
        except Exception as e:
            self.log_debug(f"[BootLoader] 擦除 Flash 异常：{e}")
            return False
    
    def _bl_write_memory(self, port, address: int, data: bytes) -> bool:
        """写入内存（BootLoader 写命令）"""
        try:
            # 1. 发送写命令 (0x31 0xCE)
            cmd = bytes([self.BL_CMD_WRITE, 0xCE])
            if not self._bl_send_command(port, cmd):
                return False
            
            # 2. 发送地址（4 字节）+ 校验和
            addr_bytes = bytes([
                (address >> 24) & 0xFF,
                (address >> 16) & 0xFF,
                (address >> 8) & 0xFF,
                address & 0xFF
            ])
            addr_checksum = self._bl_calculate_checksum(addr_bytes)
            port.write(addr_bytes + bytes([addr_checksum]))
            time.sleep(0.01)
            
            # 等待地址确认
            timeout = time.time() + 0.5
            while time.time() < timeout:
                if port.in_waiting > 0:
                    response = port.read(port.in_waiting)
                    if response and response[0] == self.BL_CMD_ACK:
                        break
                    elif response and response[0] == self.BL_CMD_NACK:
                        return False
            
            # 3. 发送数据长度 + 数据 + 校验和
            length = len(data) - 1
            to_send = bytes([length]) + data
            data_checksum = self._bl_calculate_checksum(to_send)
            
            # 分块发送数据
            for i in range(0, len(to_send), self.BOOTLOADER_CHUNK_SIZE):
                if self.bootloader_update_aborted:
                    return False
                
                chunk = to_send[i:i + self.BOOTLOADER_CHUNK_SIZE]
                port.write(chunk)
                time.sleep(0.01)
            
            # 发送校验和
            port.write(bytes([data_checksum]))
            time.sleep(0.01)
            
            # 等待写入完成
            timeout = time.time() + 1.0
            while time.time() < timeout:
                if port.in_waiting > 0:
                    response = port.read(port.in_waiting)
                    if response and response[0] == self.BL_CMD_ACK:
                        return True
                    elif response and response[0] == self.BL_CMD_NACK:
                        return False
            
            return False
            
        except Exception as e:
            self.log_debug(f"[BootLoader] 写入内存异常：{e}")
            return False
    
    def _bl_jump_to_app(self, port, address: int) -> bool:
        """跳转到 APP 程序"""
        try:
            self.log_debug(f"[BootLoader] 跳转到 APP，地址：0x{address:08X}")
            
            # 1. 发送 Go 命令 (0x21 0xDE)
            cmd = bytes([self.BL_CMD_GO, 0xDE])
            port.write(cmd)
            time.sleep(0.01)
            
            # 等待 ACK
            timeout = time.time() + 0.5
            while time.time() < timeout:
                if port.in_waiting > 0:
                    response = port.read(port.in_waiting)
                    if response and response[0] == self.BL_CMD_ACK:
                        break
            
            # 2. 发送目标地址（4 字节）+ 校验和
            addr_bytes = bytes([
                (address >> 24) & 0xFF,
                (address >> 16) & 0xFF,
                (address >> 8) & 0xFF,
                address & 0xFF
            ])
            addr_checksum = self._bl_calculate_checksum(addr_bytes)
            port.write(addr_bytes + bytes([addr_checksum]))
            time.sleep(0.1)  # 给设备跳转时间
            
            self.log_debug(f"[BootLoader] 跳转命令已发送")
            return True
            
        except Exception as e:
            self.log_debug(f"[BootLoader] 跳转 APP 异常：{e}")
            return False
    
    def _bl_reset_device(self, port):
        """复位设备并回到 APP 空闲引脚态（DTR 高 / RTS 低）"""
        try:
            self.log_debug(f"[BootLoader] 复位设备...")

            port.setDTR(True)   # BOOT0 低，确保从 APP 启动
            port.setRTS(True)   # NRST 拉低
            time.sleep(0.1)
            port.setRTS(False)  # NRST 释放
            time.sleep(0.1)
            _set_serial_idle_modem_lines(port)

            self.log_debug(f"[BootLoader] 复位完成")
            
        except Exception as e:
            self.log_debug(f"[BootLoader] 复位异常：{e}")
    
    def _bl_sync_bootloader(self, port) -> bool:
        """尝试与 BootLoader 同步（发送同步命令）"""
        try:
            self.log_debug(f"[BootLoader] 发送同步命令...")
            
            # 清空缓冲区
            port.reset_input_buffer()
            port.reset_output_buffer()
            
            # 发送同步命令（0x7F）并等待响应
            for attempt in range(3):
                port.write(bytes([0x7F]))
                time.sleep(0.05)
                
                if port.in_waiting > 0:
                    response = port.read(port.in_waiting)
                    self.log_debug(f"[BootLoader] 同步响应 (第{attempt+1}次): {response.hex().upper()}")
                    
                    if 0x79 in response or 0x7F in response:
                        self.log_debug(f"[BootLoader] 同步成功")
                        return True
                
                time.sleep(0.1)
            
            self.log_debug(f"[BootLoader] 同步失败")
            return False
            
        except Exception as e:
            self.log_debug(f"[BootLoader] 同步异常：{e}")
            return False
    
    # ==================== USB DFU 模式更新相关方法 ====================
    
    def _usb_dfu_init(self, device) -> bool:
        """初始化 USB DFU 设备（使用 USBDFUDevice）"""
        try:
            import time
            
            # 1. 清除状态
            self.log_debug(f"[USB DFU] 清除设备状态...")
            try:
                device.dfu_clear_status(0)
                self.log_debug(f"[USB DFU] 状态已清除")
            except Exception as e:
                self.log_debug(f"[USB DFU] 清除状态失败：{e} (继续)")
            
            # 2. 获取状态
            self.log_debug(f"[USB DFU] 获取设备状态...")
            status = device.dfu_get_status(0)
            if status:
                state_names = {
                    0: 'appIDLE', 1: 'appDETACH', 2: 'dfuIDLE',
                    3: 'dfuDNLOAD_SYNC', 4: 'dfuDNBUSY', 5: 'dfuDNLOAD_IDLE',
                    6: 'dfuMANIFEST_SYNC', 7: 'dfuMANIFEST', 8: 'dfuMANIFEST_WAIT_RESET',
                    9: 'dfuUPLOAD_IDLE', 10: 'dfuERROR'
                }
                state_name = state_names.get(status['bState'], f"Unknown({status['bState']})")
                self.log_debug(f"[USB DFU] 当前状态：{state_name}")
                
                # 3. 如果已经是 IDLE，返回成功
                if status['bState'] == 2:  # dfuIDLE
                    self.log_debug(f"[USB DFU] 设备已在 IDLE 状态")
                    return True
                
                # 4. 如果是 ERROR 状态，清除
                if status['bState'] == 10:  # dfuERROR
                    self.log_debug(f"[USB DFU] 检测到 ERROR 状态，清除中...")
                    device.dfu_clear_status(0)
                    time.sleep(0.1)
            
            # 5. 发送 ABORT 命令
            self.log_debug(f"[USB DFU] 发送 ABORT 命令...")
            try:
                device.dfu_abort(0)
                self.log_debug(f"[USB DFU] ABORT 命令已发送")
            except Exception as e:
                self.log_debug(f"[USB DFU] ABORT 命令发送失败：{e} (继续)")
            
            time.sleep(0.1)
            
            # 6. 再次检查状态
            self.log_debug(f"[USB DFU] 再次检查状态...")
            status = device.dfu_get_status(0)
            if status:
                state_names = {
                    0: 'appIDLE', 1: 'appDETACH', 2: 'dfuIDLE',
                    3: 'dfuDNLOAD_SYNC', 4: 'dfuDNBUSY', 5: 'dfuDNLOAD_IDLE',
                    6: 'dfuMANIFEST_SYNC', 7: 'dfuMANIFEST', 8: 'dfuMANIFEST_WAIT_RESET',
                    9: 'dfuUPLOAD_IDLE', 10: 'dfuERROR'
                }
                state_name = state_names.get(status['bState'], f"Unknown({status['bState']})")
                self.log_debug(f"[USB DFU] 最终状态：{state_name}")
                
                if status['bState'] == 2:  # dfuIDLE
                    self.log_debug(f"[USB DFU] 设备已进入 IDLE 状态")
                    return True
                else:
                    self.log_debug(f"[USB DFU] 警告：设备未进入 IDLE 状态")
            
            return True
            
        except Exception as e:
            self.log_debug(f"[USB DFU] 初始化失败：{e}")
            return False
    
    def _usb_dfu_get_status(self, device) -> dict:
        """获取 DFU 设备状态（使用 USBDFUDevice）"""
        try:
            return device.dfu_get_status(0)
        except Exception as e:
            self.log_debug(f"[USB DFU] 获取状态失败：{e}")
            return None
    
    def _usb_dfu_erase_flash(self, device, total_pages: int, start_address: int = None) -> bool:
        """USB DFU 模式擦除 Flash（使用 USBDFUDevice）"""
        try:
            self.log_debug(f"[USB DFU] 开始擦除 Flash，共{total_pages}页")
            
            # STM32 DFU 擦除命令：0x41 + 4 字节地址（小端）
            # 每个扇区大小 128KB (0x20000)
            if start_address is None:
                erase_addr = self.BOOTLOADER_IAP_START_ADDR
            else:
                erase_addr = start_address
            
            self.log_debug(f"[USB DFU] 擦除起始地址：0x{erase_addr:08X}")
            
            for page in range(total_pages):
                if self.bootloader_update_aborted:
                    return False
                
                # 构建擦除命令：0x41 + 4 字节地址
                erase_cmd = bytes([
                    0x41,
                    erase_addr & 0xFF,
                    (erase_addr >> 8) & 0xFF,
                    (erase_addr >> 16) & 0xFF,
                    (erase_addr >> 24) & 0xFF
                ])
                
                # 先获取当前设备状态
                try:
                    status = device.dfu_get_status(0)
                    if status:
                        state_names = {
                            0: 'appIDLE', 1: 'appDETACH', 2: 'dfuIDLE',
                            3: 'dfuDNLOAD_SYNC', 4: 'dfuDNBUSY', 5: 'dfuDNLOAD_IDLE',
                            6: 'dfuMANIFEST_SYNC', 7: 'dfuMANIFEST', 8: 'dfuMANIFEST_WAIT_RESET',
                            9: 'dfuUPLOAD_IDLE', 10: 'dfuERROR'
                        }
                        state_name = state_names.get(status['bState'], f"Unknown({status['bState']})")
                except Exception as status_err:
                    pass
                
                # 发送擦除命令
                try:
                    ret = device.dfu_download(list(erase_cmd), 0, block_number=0)
                    
                    if ret:
                        # 等待擦除完成（擦除可能需要较长时间）
                        if not device.dfu_wait_idle(0, timeout=30000):
                            self.log_debug(f"[USB DFU] 擦除页{page}超时")
                            return False
                        
                        # 检查中止标志
                        if self.bootloader_update_aborted:
                            return False
                        
                        progress = (page + 1) / total_pages * 100
                        self.root.after(0, lambda p=progress: self.update_progress(p))
                        # self.log_debug(f"[USB DFU] 擦除页{page}成功，进度：{progress:.1f}%")
                    else:
                        self.log_debug(f"[USB DFU] 擦除页{page}失败 - dfu_download 返回 False")
                        return False
                    
                except Exception as e:
                    self.log_debug(f"[USB DFU] 擦除页{page}异常：{e}")
                    import traceback
                    self.log_debug(f"[USB DFU] 详细错误：{traceback.format_exc()}")
                    return False
                
                # 下一个扇区地址
                erase_addr += 0x20000
            
            self.log_debug(f"[USB DFU] Flash 擦除完成")
            self.log_debug(f"[USB DFU] 开始固件传输")
            return True
            
        except Exception as e:
            self.log_debug(f"[USB DFU] 擦除 Flash 异常：{e}")
            return False
    
    def _usb_dfu_write_memory(self, device, address: int, data: bytes) -> bool:
        """USB DFU 模式写入内存（使用 USBDFUDevice）"""
        try:
            # 1. 先设置写入地址
            # STM32 DFU 写地址命令：0x21 + 4 字节地址（小端）
            addr_cmd = bytes([
                0x21,
                address & 0xFF,
                (address >> 8) & 0xFF,
                (address >> 16) & 0xFF,
                (address >> 24) & 0xFF
            ])
            
            if not device.dfu_download(list(addr_cmd), 0, block_number=0):
                self.log_debug(f"[USB DFU] 设置地址失败")
                return False
            
            # 等待地址设置完成
            if not device.dfu_wait_idle(0, timeout=5000):
                self.log_debug(f"[USB DFU] 等待地址设置超时")
                return False
            
            # 2. 写入数据（关键：wValue=2，参考 C++ 代码）
            if device.dfu_download(list(data), 0, block_number=2):
                # 等待写入完成
                if device.dfu_wait_idle(0, timeout=5000):
                    return True
            
            return False
            
        except Exception as e:
            self.log_debug(f"[USB DFU] 写入失败：{e}")
            return False
    
    def _usb_dfu_jump_to_app(self, device, address: int):
        """跳转到 APP（使用 USBDFUDevice）"""
        try:
            self.log_debug(f"[USB DFU] 跳转到 APP，地址：0x{address:08X}")
            
            # 1. 设置跳转地址
            self.log_debug(f"[USB DFU] 设置跳转地址...")
            addr_cmd = bytes([
                0x21,
                address & 0xFF,
                (address >> 8) & 0xFF,
                (address >> 16) & 0xFF,
                (address >> 24) & 0xFF
            ])
            
            if not device.dfu_download(list(addr_cmd), 0, block_number=0):
                self.log_debug(f"[USB DFU] 设置跳转地址失败")
                return
            
            # 等待地址设置完成
            if not device.dfu_wait_idle(0, timeout=5000):
                self.log_debug(f"[USB DFU] 等待地址设置超时")
                return
            
            self.log_debug(f"[USB DFU] 跳转地址已设置")
            
            # 2. 发送空数据触发跳转（相当于 ABORT 命令，wValue=0）
            self.log_debug(f"[USB DFU] 发送跳转命令...")
            try:
                device.dfu_download([], 0, block_number=0)
            except Exception as e:
                self.log_debug(f"[USB DFU] 发送跳转命令异常：{e}")
                # 忽略异常，设备可能已经开始复位
            
            self.log_debug(f"[USB DFU] 跳转命令已发送，设备将自动复位执行 APP")
            
        except Exception as e:
            self.log_debug(f"[USB DFU] 跳转失败：{e}")
    
    def _usb_dfu_update_once(self, file_path: str, update_type: str) -> Optional[str]:
        """USB DFU 单次更新；None=成功，特殊 token 或 str=失败原因"""
        try:
            from usb_dfu_lib import USBDFUDevice

            if update_type == "IAP":
                start_address = self.BOOTLOADER_IAP_START_ADDR
                self.log_debug(f"[USB DFU] 开始 IAP 更新，起始地址：0x{start_address:08X}")
            else:
                start_address = self.BOOTLOADER_APP_START_ADDR
                self.log_debug(f"[USB DFU] 开始 APP 更新，起始地址：0x{start_address:08X}")

            device = USBDFUDevice()
            if not device.open(vid=0x0483, pid=0xDF11):
                if not device.open(vid=0x0483, pid=0xDF12):
                    return "未找到 USB DFU 设备"

            device.claim_interface(0)

            with open(file_path, 'rb') as f:
                firmware_data = f.read()

            file_size = len(firmware_data)
            self.log_debug(f"[USB DFU] 固件大小：{file_size} 字节")

            abort = self._bl_check_abort()
            if abort:
                device.close()
                return abort

            if not self._usb_dfu_init(device):
                device.close()
                return "DFU 初始化失败"

            abort = self._bl_check_abort()
            if abort:
                device.close()
                return abort

            time.sleep(0.05)

            total_pages = (
                file_size + self.BOOTLOADER_FLASH_PAGE_SIZE - 1
            ) // self.BOOTLOADER_FLASH_PAGE_SIZE
            if not self._usb_dfu_erase_flash(device, total_pages, start_address):
                device.close()
                return "Flash 擦除失败"

            abort = self._bl_check_abort()
            if abort:
                device.close()
                return abort

            written_bytes = 0
            current_addr = start_address
            while written_bytes < file_size:
                abort = self._bl_check_abort()
                if abort:
                    device.close()
                    return abort

                block = firmware_data[
                    written_bytes:written_bytes + self.BOOTLOADER_BLOCK_SIZE
                ]
                if not self._usb_dfu_write_memory(device, current_addr, block):
                    device.close()
                    return "固件写入失败"

                written_bytes += len(block)
                current_addr += len(block)
                progress = written_bytes / file_size * 100
                self.root.after(0, lambda p=progress: self.update_progress(p))

            abort = self._bl_check_abort()
            if abort:
                device.close()
                return abort

            self.root.after(0, lambda: self.log_debug(f"[USB DFU] 固件更新完成！"))
            self.root.after(0, lambda: self.update_progress(100.0))

            try:
                device.release_interface(0)
                device.close()
            except Exception:
                pass

            if self.serial_monitor.serial_port:
                self._bl_reset_device(self.serial_monitor.serial_port)
            return None

        except Exception as e:
            self.root.after(0, lambda: self.log_debug(f"[USB DFU] 更新异常：{e}"))
            import traceback
            self.root.after(0, lambda: self.log_debug(
                f"[USB DFU] 详细错误：{traceback.format_exc()}"))
            return f"更新异常：{e}"

    def _bootloader_update_once(self, file_path: str, update_type: str) -> Optional[str]:
        """BootLoader 单次完整更新；None=成功"""
        abort = self._bl_check_abort()
        if abort:
            return abort

        if not self.serial_monitor.serial_port or not self.serial_monitor.serial_port.is_open:
            self._bl_prepare_serial_for_retry()
        if not self.serial_monitor.serial_port or not self.serial_monitor.serial_port.is_open:
            self.log_debug("[USB DFU] 错误：串口未打开")
            return "串口未连接"

        port = self.serial_monitor.serial_port

        if update_type == "IAP":
            start_address = self.BOOTLOADER_IAP_START_ADDR
        else:
            start_address = self.BOOTLOADER_APP_START_ADDR

        with open(file_path, 'rb') as f:
            firmware_data = f.read()

        file_size = len(firmware_data)
        self.log_debug(f"[USB DFU] 固件文件大小：{file_size} 字节")

        if not self._bl_enter_bootloader_mode(port):
            if self.bootloader_update_aborted:
                return self._BL_CANCEL
            return "进入 BootLoader 模式失败"

        abort = self._bl_check_abort()
        if abort:
            return abort

        start = time.time()
        while time.time() - start < 2.0:
            abort = self._bl_check_abort()
            if abort:
                return abort
            time.sleep(0.01)

        abort = self._bl_check_abort()
        if abort:
            return abort

        self.root.after(0, lambda: self.log_debug("[USB DFU] 检测 USB DFU 设备..."))
        dfu_devices = self._bl_detect_dfu_devices()

        abort = self._bl_check_abort()
        if abort:
            return abort

        if dfu_devices:
            device_info = dfu_devices[0]
            self.root.after(0, lambda: self.log_debug(
                f"[USB DFU] 检测到 USB DFU 设备：VID=0x{device_info['vid']:04X}, "
                f"PID=0x{device_info['pid']:04X}"))
            self.root.after(0, lambda: self.log_debug("[USB DFU] 开始 USB DFU 模式更新..."))
            return self._usb_dfu_update_once(file_path, update_type)

        self.root.after(0, lambda: self.log_debug(
            "[USB DFU] 未检测到 USB DFU 设备，使用串口模式"))
        self.root.after(0, lambda: self.log_debug("[USB DFU] 尝试串口同步..."))

        if not self._bl_sync_bootloader(port):
            return "无法进入 BootLoader 模式"

        self.root.after(0, lambda: self.log_debug("[USB DFU] 串口同步成功，开始擦除 Flash..."))

        total_pages = (
            file_size + self.BOOTLOADER_FLASH_PAGE_SIZE - 1
        ) // self.BOOTLOADER_FLASH_PAGE_SIZE
        if not self._bl_erase_flash(port, total_pages):
            return "Flash 擦除失败"

        self.root.after(0, lambda: self.log_debug("[USB DFU] 开始写入固件..."))
        written_bytes = 0
        while written_bytes < file_size:
            abort = self._bl_check_abort()
            if abort:
                return abort

            block = firmware_data[
                written_bytes:written_bytes + self.BOOTLOADER_BLOCK_SIZE
            ]
            current_address = start_address + written_bytes
            if not self._bl_write_memory(port, current_address, block):
                return "固件写入失败"

            written_bytes += len(block)
            progress = written_bytes / file_size * 100
            self.root.after(0, lambda p=progress: self.update_progress(p))

        abort = self._bl_check_abort()
        if abort:
            return self._BL_CANCEL

        self.root.after(0, lambda: self.log_debug("[USB DFU] 固件更新完成！"))
        self.root.after(0, lambda: self.update_progress(100.0))

        self.root.after(0, lambda: self.log_debug("[USB DFU] 正在跳转到 APP..."))
        if not self._bl_jump_to_app(port, start_address):
            self.root.after(0, lambda: self.log_debug("[USB DFU] 跳转失败，尝试复位..."))

        self.root.after(0, lambda: self.log_debug("[USB DFU] 正在复位设备..."))
        self._bl_reset_device(port)
        return None

    def _bootloader_update_worker(self, file_path: str, update_type: str) -> None:
        """BootLoader 更新（含最多 3 次完整重试）"""
        last_err = "未知错误"
        max_attempts = self.BOOTLOADER_MAX_ATTEMPTS

        for attempt in range(1, max_attempts + 1):
            if self.bootloader_update_aborted:
                self.root.after(0, lambda: self.stop_firmware_update(
                    success=False, user_cancelled=True))
                return
            if self._bl_is_serial_permanently_disconnected():
                self.root.after(0, lambda: self.stop_firmware_update(
                    success=False, failure_reason="串口已断开"))
                return

            if attempt > 1:
                msg = (
                    f"[BootLoader] 传输失败（{last_err}），"
                    f"开始第 {attempt}/{max_attempts} 次完整重试..."
                )
                self.root.after(0, lambda m=msg: self.log_receive(m))
                self.root.after(0, lambda: self.update_progress(0.0))
                self.bootloader_update_aborted = False
                time.sleep(2.0)
                self._bl_prepare_serial_for_retry()

            result = self._bootloader_update_once(file_path, update_type)
            if result is None:
                self.root.after(0, lambda: self.stop_firmware_update(success=True))
                return
            if result == self._BL_CANCEL:
                self.root.after(0, lambda: self.stop_firmware_update(
                    success=False, user_cancelled=True))
                return
            if result == self._BL_SERIAL_LOST:
                self.root.after(0, lambda: self.stop_firmware_update(
                    success=False, failure_reason="串口已断开"))
                return

            last_err = result

        self.root.after(0, lambda e=last_err: self.stop_firmware_update(
            success=False,
            failure_reason=f"更新失败，已完整重试 {max_attempts} 次：{e}",
        ))

    def _start_bootloader_update(self, file_path: str, update_type: str):
        """兼容入口：委托给带重试的工作线程"""
        self._bootloader_update_worker(file_path, update_type)
    
    def _start_bootloader_update_thread(self, file_path: str, update_type: str):
        """在后台线程中启动 BootLoader 更新"""
        # 检查是否已经在更新中
        if self.firmware_update_active:
            self.log_debug(f"[USB DFU] 更新正在进行中，忽略重复请求")
            return
        
        # 重置中止标志
        self.bootloader_update_aborted = False
        
        # 创建并启动后台线程
        self.bootloader_update_thread = threading.Thread(
            target=self._start_bootloader_update,
            args=(file_path, update_type),
            daemon=True
        )
        self.bootloader_update_thread.start()
    
    def convert_hex_to_bin(self, hex_path):
        """将 Intel HEX 文件转换为 BIN 文件"""
        try:
            with open(hex_path, 'r') as f:
                lines = f.readlines()
            
            # 解析 HEX 文件，提取数据
            data_dict = {}  # 地址 -> 数据
            min_addr = 0xFFFFFFFF
            max_addr = 0
            current_base_addr = 0  # 当前基地址（用于扩展线性地址）
            
            for line in lines:
                line = line.strip()
                if not line or not line.startswith(':'):
                    continue
                
                # 解析 HEX 记录
                if len(line) < 11:
                    continue
                
                byte_count = int(line[1:3], 16)
                addr_offset = int(line[3:7], 16)  # 地址偏移量
                record_type = int(line[7:9], 16)
                
                if record_type == 0x00:  # 数据记录
                    # 计算完整地址 = 基地址 + 偏移地址
                    full_addr = current_base_addr + addr_offset
                    
                    # 解析数据
                    data = bytes([int(line[i:i+2], 16) for i in range(9, 9 + byte_count * 2, 2)])
                    for i, b in enumerate(data):
                        data_dict[full_addr + i] = b
                    
                    min_addr = min(min_addr, full_addr)
                    max_addr = max(max_addr, full_addr + len(data) - 1)
                    
                elif record_type == 0x01:  # 结束记录
                    break
                    
                elif record_type == 0x02:  # 扩展段地址记录（8086 模式，16 位基地址）
                    if byte_count == 2:
                        segment_addr = int(line[9:13], 16)
                        current_base_addr = segment_addr << 4  # 左移 4 位
                        
                elif record_type == 0x04:  # 扩展线性地址记录（32 位基地址）
                    if byte_count == 2:
                        linear_addr = int(line[9:13], 16)
                        current_base_addr = linear_addr << 16  # 左移 16 位
                        
                elif record_type == 0x03:  # 开始段地址记录（忽略）
                    pass
                elif record_type == 0x05:  # 开始线性地址记录（忽略）
                    pass
            
            if not data_dict:
                return None
            
            # 创建连续的 BIN 数据
            bin_size = max_addr - min_addr + 1
            bin_data = bytearray([0xFF] * bin_size)  # 用 0xFF 填充空白
            
            for addr, value in data_dict.items():
                bin_data[addr - min_addr] = value
            
            # 保存为 BIN 文件
            bin_path = hex_path.rsplit('.', 1)[0] + '.bin'
            with open(bin_path, 'wb') as f:
                f.write(bin_data)
            
            self.log_debug(f"[HEX 转 BIN] HEX 文件大小：{os.path.getsize(hex_path)} 字节")
            self.log_debug(f"[HEX 转 BIN] BIN 文件大小：{bin_size} 字节 ({bin_size / 1024:.2f} KB)")
            self.log_debug(f"[HEX 转 BIN] 地址范围：0x{min_addr:08X} - 0x{max_addr:08X}")
            
            return bin_path
        except Exception as e:
            print(f"HEX 转 BIN 失败：{e}")
            import traceback
            traceback.print_exc()
            return None
    
    def start_iap_update(self):
        """开始 IAP 固件更新"""
        # 1. 检查是否选择了 IAP 固件文件
        file_path = self.iap_path_var.get()
        if not file_path:
            messagebox.showwarning("警告", "请选择 IAP 固件文件！")
            return
        
        # 2. 检查是否已连接（按 OTA 模式检查对应链路）
        ota_mode = self.ota_mode_var.get()
        if ota_mode == "TCP":
            if not self.tcp_connected:
                messagebox.showwarning("警告", "请连接TCP！")
                return
        elif not (self.serial_connected and self._is_serial_port_open()):
            messagebox.showwarning("警告", "请连接串口！")
            return
        
        # 3. 如果是 TCP 模式，检查是否输入了 IP 地址
        mode = self.ota_mode_var.get()
        if mode == "TCP":
            ui_mode = self.mode_var.get().split(' - ')[0].lower()
            if ui_mode == "tcp":
                ip = self.tcp_ip_entry.get().strip()
                if not ip:
                    messagebox.showwarning("警告", "请输入 IP 地址！")
                    return
        
        self.start_firmware_update(file_path, "IAP")
    
    def start_app_update(self):
        """开始 APP 固件更新"""
        # 1. 检查是否选择了 APP 固件文件
        file_path = self.app_path_var.get()
        if not file_path:
            messagebox.showwarning("警告", "请先选择 APP 固件文件！")
            return
        
        # 2. 检查是否已连接（按 OTA 模式检查对应链路）
        ota_mode = self.ota_mode_var.get()
        if ota_mode == "TCP":
            if not self.tcp_connected:
                messagebox.showwarning("警告", "请连接TCP！")
                return
        elif not (self.serial_connected and self._is_serial_port_open()):
            messagebox.showwarning("警告", "请连接串口！")
            return
        
        # 3. 如果是 TCP 模式，检查是否输入了 IP 地址
        mode = self.ota_mode_var.get()
        if mode == "TCP":
            ui_mode = self.mode_var.get().split(' - ')[0].lower()
            if ui_mode == "tcp":
                ip = self.tcp_ip_entry.get().strip()
                if not ip:
                    messagebox.showwarning("警告", "TCP 模式下请先输入 IP 地址！")
                    return
        
        self.start_firmware_update(file_path, "APP")
    
    def start_firmware_update(self, file_path: str, update_type: str):
        """开始固件更新
        
        Args:
            file_path: 固件文件路径
            update_type: 更新类型（"IAP" 或 "APP"）
        """
        # 防止重复发送开始指令
        if self.iap_update_active or self.app_update_active:
            mode = self.ota_mode_var.get()
            prefix = "[OTA]" if mode == "TCP" else "[USB DFU]"
            self.log_receive(f"{prefix} 警告：{update_type} 已在进行中，忽略重复的开始指令")
            return
        
        if not file_path:
            mode = self.ota_mode_var.get()
            prefix = "[OTA]" if mode == "TCP" else "[USB DFU]"
            self.log_receive(f"{prefix} 警告：请先选择{update_type}固件文件")
            return
        
        # 获取当前模式
        mode = self.ota_mode_var.get()
        
        if mode == "TCP":
            # TCP 模式：只能更新 APP
            if update_type != "APP":
                self.log_receive("[OTA] 警告：TCP 模式下只能更新 APP 固件")
                return
            # 检查 TCP 是否连接
            if not self.tcp_connected:
                self.log_receive("[OTA] 警告：请先在 TCP 模式下连接设备")
                return
            # 额外检查：验证 TCP 连接是否有效（设备重启后可能已断开）
            if hasattr(self, 'tcp_dual') and self.tcp_dual._send_client().socket:
                try:
                    sock = self.tcp_dual._send_client().socket
                    error = sock.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR)
                    if error != 0:
                        self.log_receive(f"[OTA] 警告：TCP 连接已失效（错误码：{error}），请重新连接设备")
                        self.tcp_connected = False
                        return
                except Exception as e:
                    self.log_receive(f"[OTA] 警告：TCP 连接检查失败：{e}，请重新连接设备")
                    self.tcp_connected = False
                    return
            
            # 禁用 IAP 和 APP 更新按钮（检查通过后）
            if hasattr(self, 'iap_update_btn') and self.iap_update_btn:
                self.iap_update_btn.config(state='disabled')
            if hasattr(self, 'app_update_btn') and self.app_update_btn:
                self.app_update_btn.config(state='disabled')
            ota_addr = self._get_tcp_device_addr(quiet=False)
            if ota_addr is None:
                return
            self._ota_device_addr = ota_addr
            self.ota_mode = "TCP"
            self._ota_packet_max_bytes = self._get_ota_packet_max_bytes()
            self._suspend_auto_ack_for_ota()
            self._disable_ota_buttons()
            # 执行 TCP OTA 操作（后台线程）
            self._start_tcp_ota_thread(file_path)
        else:
            # BootLoader 模式：检查串口是否连接
            if not self.serial_connected:
                self.log_receive("[USB DFU] 警告：请先在串口模式下连接设备")
                return
            # 禁用 IAP 和 APP 更新按钮（检查通过后）
            if hasattr(self, 'iap_update_btn') and self.iap_update_btn:
                self.iap_update_btn.config(state='disabled')
            if hasattr(self, 'app_update_btn') and self.app_update_btn:
                self.app_update_btn.config(state='disabled')
            # 执行 BootLoader DFU 更新（后台线程）
            self._start_bootloader_update_thread(file_path, update_type)
    
    # _start_tcp_ota 方法已弃用，改用 _tcp_ota_worker
    # 所有 OTA 操作现在在单一线程中完成，避免定时器累积
    
    def _start_tcp_ota_thread(self, file_path: str):
        """在后台线程中启动 TCP OTA"""
        # 创建并启动后台线程
        self.tcp_ota_thread = threading.Thread(
            target=self._tcp_ota_worker,
            args=(file_path,),
            daemon=False  # 非守护线程，确保更新完成前不会退出
        )
        self.tcp_ota_thread.start()
    
    def _tcp_ota_worker(self, file_path: str):
        """TCP OTA 工作线程：处理所有 OTA 操作（纯事件驱动）"""
        try:
            # 1. 读取固件文件
            with open(file_path, 'rb') as f:
                firmware_data = f.read()
            
            file_size = len(firmware_data)
            
            # 2. 发送 OTA 开始指令
            ota_start_cmd = NetRawProtocol.build_write_multi(
                addr=self._get_ota_device_addr(),
                reg=NetRawProtocol.REG_OTA_FILE_SIZE,
                values=[file_size & 0xFFFF, (file_size >> 16) & 0xFFFF]
            )
            self.send_ota_data(ota_start_cmd)
            
            # 3. 初始化状态
            self.firmware_update_active = True
            self.app_update_active = True
            self.firmware_data = firmware_data
            self.firmware_total_size = file_size
            self.firmware_sent_bytes = 0
            self.ota_start_time = time.time()
            self.ota_last_state = 0  # 重置状态
            self._last_status_time = time.time()
            self._ota_packet_pending = False
            self._ota_finish_sent = False
            self._ota_reconnect_grace_until = 0.0
            self._ota_resume_after_reconnect = False
            self._ota_waiting_for_started = True  # 标记正在等待 STARTED 状态
            self._ota_start_retry_count = 0  # OTA 开始指令重试次数
            self._ota_start_sent_time = time.time()  # 记录发送开始指令的时间
            
            # 4. 记录开始（按钮已在主线程 _disable_ota_buttons 中禁用）
            pkt_max = getattr(self, '_ota_packet_max_bytes', 128)
            self.root.after(0, lambda: self.log_receive(
                f"[OTA] 开始 APP 更新，文件大小：{file_size} 字节，单帧最大：{pkt_max} 字节"))
            if self._ota_auto_ack_restore:
                self.root.after(0, lambda: self.log_receive("[OTA] 已临时关闭指令应答以加速传输"))
            
            # 5. 等待设备进入接收状态（最多等待 OTA_START_WAIT_S 秒，支持重试）
            # 注意：改为事件驱动，由 handle_ota_status_response 回调触发
            # 新增：如果 8 秒内未收到应答，重试发送开始指令（最多 3 次）
            wait_start_time = time.time()
            device_ready = False
            
            # 轮询检查是否收到 STARTED 状态（保留原有超时逻辑）
            while time.time() - wait_start_time < self.OTA_START_WAIT_S:
                if self.ota_last_state == 1:  # 1 = STARTED
                    device_ready = True
                    break
                time.sleep(0.05)
                
                # 检查是否需要重试发送 OTA 开始指令（8 秒无应答）
                if time.time() - self._ota_start_sent_time >= 8.0:
                    if self._ota_start_retry_count < 3:
                        # 重试发送开始指令
                        self._ota_start_retry_count += 1
                        self.root.after(0, lambda count=self._ota_start_retry_count: self.log_receive(
                            f"[OTA] 未收到设备应答，第 {count} 次重试发送开始指令..."))
                        self.send_ota_data(ota_start_cmd)
                        self._ota_start_sent_time = time.time()  # 重置计时
                    else:
                        # 已达到最大重试次数，退出
                        self.root.after(0, lambda: self.log_receive(
                            "[OTA] 错误：已重试 3 次仍未收到设备应答"))
                        break
            
            # 如果超时，记录警告但仍然继续（保持原有行为）
            if not device_ready:
                if self._ota_start_retry_count >= 3:
                    # 重试失败，直接退出
                    self.root.after(0, lambda: self.log_receive(
                        "[OTA] OTA 更新失败：设备未响应开始指令"))
                    self.root.after(0, lambda: self.stop_firmware_update(
                        success=False, failure_reason="设备未响应开始指令（重试 3 次）"))
                    return
                else:
                    self.root.after(0, lambda: self.log_receive(
                        f"[OTA] 警告：设备未在 {int(self.OTA_START_WAIT_S)} 秒内进入接收状态，继续尝试"))
                    self.root.after(0, lambda: self.log_receive("[OTA] 请检查设备是否已重启并进入 BootLoader 模式"))
            
            # 6. 主循环：等待状态响应并发送数据包（事件驱动）
            last_log_state = 0
            while self.firmware_update_active:
                # 检查是否需要发送数据包
                if self._ota_packet_pending:
                    self._ota_packet_pending = False
                    self._send_ota_packet_in_thread()
                
                # 检查是否所有数据已发送完成且结束指令已发送
                if self.firmware_sent_bytes >= self.firmware_total_size and self._ota_finish_sent:
                    # 数据已全部发送完成，结束指令已发送
                    # 等待设备进入 DONE 状态（设备正在校验 Flash，这可能需要几秒到几十秒）
                    if self.ota_last_state == 4:
                        # 已收到 DONE 状态，退出循环
                        break
                    # 继续等待 DONE 状态，只在状态变化时记录日志
                    if self.ota_last_state != last_log_state:
                        last_log_state = self.ota_last_state
                        state_names = {0: "IDLE", 1: "STARTED", 2: "VERIFY", 3: "ERROR", 4: "DONE"}
                        state_name = state_names.get(self.ota_last_state, f"UNKNOWN({self.ota_last_state})")
                        self.root.after(0, lambda s=state_name: self.log_receive(f"[OTA] 等待设备校验完成（当前状态：{s}）"))
                    # 短暂休眠，继续等待（设备校验 Flash 需要时间）
                    # 使用更短的休眠时间，让停止按钮更快响应
                    time.sleep(0.02)
                    continue
                
                # 检查超时（OTA_STATUS_TIMEOUT_S 无状态更新；TCP 重连期间暂停）
                if not self._ota_status_timeout_paused():
                    if time.time() - self._last_status_time > self.OTA_STATUS_TIMEOUT_S:
                        self.root.after(0, lambda: self.log_receive("[OTA] 设备状态响应超时"))
                        self.root.after(0, lambda: self.stop_firmware_update(
                            success=False, failure_reason="设备状态响应超时"))
                        break
                
                # 短暂休眠，避免 CPU 占用（使用更短时间以便更快响应停止按钮）
                time.sleep(0.01)
                
        except Exception as e:
            err = str(e)
            self.root.after(0, lambda m=err: self.log_receive(f"[OTA] 启动失败：{m}"))
            self.root.after(0, lambda m=err: self.stop_firmware_update(
                success=False, failure_reason=f"启动失败：{m}"))
    
    def _send_ota_packet_in_thread(self):
        """在 OTA 线程中发送下一个数据包"""
        try:
            if not self.firmware_update_active:
                return
            
            if not self._ota_tcp_link_ready():
                self._ota_packet_pending = True
                return
            
            # 计算本次发送的数据量（由「单帧最大字节」决定，128 或 224）
            remaining = self.firmware_total_size - self.firmware_sent_bytes
            packet_size = self._calc_ota_packet_size(remaining)
            
            if packet_size <= 0:
                # 所有数据发送完成，发送结束指令
                self._finish_ota_in_thread()
                return
            
            # 获取数据
            start_offset = self.firmware_sent_bytes
            packet_data = self.firmware_data[start_offset:start_offset + packet_size]
            
            # 确保长度是偶数
            if len(packet_data) % 2 != 0:
                packet_data = packet_data + b'\x00'
            
            # 转换为寄存器值
            register_values = []
            for i in range(0, len(packet_data), 2):
                word = packet_data[i] | (packet_data[i + 1] << 8)
                register_values.append(word)
            
            # 构建写多寄存器指令
            ota_data_cmd = NetRawProtocol.build_write_multi(
                addr=self._get_ota_device_addr(),
                reg=NetRawProtocol.REG_OTA_DATA,
                values=register_values
            )
            
            # 发送数据
            if not self.send_ota_data(ota_data_cmd):
                self._ota_packet_pending = True
                return
            
            # 记录发送时间
            self.packet_send_time = time.time()
            
        except Exception as e:
            self.root.after(0, lambda: self.log_receive(f"[OTA] 发送数据包异常：{e}"))
            self.root.after(0, lambda: self.stop_firmware_update(success=False, failure_reason=f"发送数据包异常：{e}"))
    
    def _finish_ota_in_thread(self):
        """在 OTA 线程中完成 OTA 更新"""
        if not self.firmware_update_active or self._ota_finish_sent:
            return
        
        self._ota_finish_sent = True
        
        # 使用已计算的 CRC32（避免重复计算）
        # 根据 OTA 模式选择对应的 CRC32 值
        mode = getattr(self, 'ota_mode', None)
        if mode is None:
            # 兼容旧逻辑：从 ota_mode_var 获取
            mode = self.ota_mode_var.get()
        
        if mode == "tcp":
            # TCP OTA 模式（APP 更新）
            crc32_value = getattr(self, 'app_crc32', None)
            if crc32_value is None:
                # 如果没有预计算，现场计算（兼容旧逻辑）
                crc32_value = self._bl_crc32(self.firmware_data)
        else:
            # BootLoader 模式
            crc32_value = getattr(self, 'bootloader_crc32', None)
            if crc32_value is None:
                # 如果没有预计算，现场计算（兼容旧逻辑）
                crc32_value = self._bl_crc32(self.firmware_data)
        
        self.root.after(0, lambda: self.log_receive(f"[OTA] 所有数据包发送完成，固件传输 100%"))
        self.root.after(0, lambda: self.update_progress(100.0))
        
        # 发送结束指令
        self.root.after(0, lambda: self.log_receive(f"[OTA] 发送结束指令（CRC32: 0x{crc32_value:08X}），等待设备校验..."))
        
        # 构建结束指令
        ota_finish_cmd = NetRawProtocol.build_write_multi(
            addr=self._get_ota_device_addr(),
            reg=NetRawProtocol.REG_OTA_CRC32,
            values=[crc32_value & 0xFFFF, (crc32_value >> 16) & 0xFFFF]
        )
        
        self.send_ota_data(ota_finish_cmd)
        self.packet_send_time = time.time()
    
    def _send_reboot_command_in_thread(self):
        """在 OTA 线程中发送重启指令"""
        def send_reboot_loop():
            count = 0
            while count < 2 and self.firmware_update_active:
                # 构建重启指令
                reboot_cmd = NetRawProtocol.build_write_multi(
                    addr=self._get_ota_device_addr(),
                    reg=NetRawProtocol.REG_OTA_CRC32,
                    values=[0, 0]
                )
                
                self.send_ota_data(reboot_cmd)
                count += 1
                self.root.after(0, lambda c=count: self.log_receive(f"[OTA] 发送重启指令 #{c}/2"))
                
                if count < 2:
                    time.sleep(0.1)  # 100ms 延时
            
            if count >= 2:
                self.root.after(0, lambda: self.log_receive("[OTA] 重启指令已发送 2 次，设备应立即重启"))
                self.root.after(0, lambda: self.stop_firmware_update(success=True))
        
        # 在新线程中发送重启指令
        reboot_thread = threading.Thread(target=send_reboot_loop, daemon=True)
        reboot_thread.start()
    
    def _send_reboot_command_and_finish(self):
        """发送重启指令并完成 OTA 更新（在主线程中调用）"""
        # 发送 2 次重启指令（寄存器 202-203，state=0, written_bytes=0）
        self.reboot_command_count = 0
        self._send_reboot_command_in_thread()
    
    def _disable_ota_buttons(self):
        """禁用 OTA 相关按钮（在主线程中调用）"""
        self._set_command_buttons_state(tk.DISABLED)
        self.iap_update_btn.config(state=tk.DISABLED)
        self.app_update_btn.config(state=tk.DISABLED)
        self.iap_browse_btn.config(state=tk.DISABLED)
        self.app_browse_btn.config(state=tk.DISABLED)
        # OTA 后台传输时仍允许 UDP 自定义数据发送
        self._set_udp_custom_controls_state(tk.NORMAL)
        if hasattr(self, 'stop_update_btn') and self.stop_update_btn:
            self.stop_update_btn.config(state=tk.NORMAL)
    
    def poll_ota_state(self):
        """定时查询 OTA 状态（暂时关闭，依赖单片机主动上传）"""
        if not self.firmware_update_active:
            return
        
        # 暂时关闭主动查询，依赖单片机主动上传
        # 发送读多寄存器指令，读取寄存器 204-207（OTA 状态，8 字节）
        # ota_query_cmd = NetRawProtocol.build_read_multi(
        #     addr=int(self.addr_var.get()),
        #     reg=NetRawProtocol.REG_OTA_STATE,
        #     qty=4  # 4 个寄存器 = 8 字节（state 32 位 + written_bytes 32 位）
        # )
        # 
        # self.send_data(ota_query_cmd)
        # self.log_receive(f"[OTA] 查询状态（第 {int((time.time() - self.ota_start_time) * 1000 / 200)} 次）")
        
        # 检查是否超时（OTA_START_WAIT_S 后状态仍然不是 OTA_STATE_STARTED，考虑擦除时间）
        # 暂时关闭超时检测，依赖单片机主动上传
        elapsed_ms = (time.time() - self.ota_start_time) * 1000
        if elapsed_ms >= int(self.OTA_START_WAIT_S * 1000) and self.ota_last_state != 1:  # 1 = OTA_STATE_STARTED
            self.log_receive(
                f"[OTA] 状态超时！设备未在 {int(self.OTA_START_WAIT_S)} 秒内进入接收状态"
                f"（当前状态：{self.ota_last_state}）")
            # 状态响应超时
            self.stop_firmware_update(
                success=False,
                failure_reason=f"状态响应超时（设备未在 {int(self.OTA_START_WAIT_S)} 秒内进入接收状态）")
            return
        
        # 继续定时查询（每 200ms 一次）- 暂时关闭
        # self.root.after(200, self.poll_ota_state)
    
    def _ota_log_visible(self) -> bool:
        """当前 UI 在 TCP 页时才写入 OTA 调试日志（进度条不受此限制）"""
        return getattr(self, '_ui_mode_cache', 'tcp') == 'tcp'

    def _ota_tcp_link_ready(self) -> bool:
        """OTA 发送前检查 TCP 是否可用（未在重连）"""
        if not self.tcp_connected:
            return False
        if getattr(self, 'is_reconnecting', False):
            return False
        if getattr(self, '_tcp_disconnect_detected_time', None) is not None:
            return False
        try:
            return self.tcp_dual.is_fully_connected()
        except Exception:
            return False

    def _ota_status_timeout_paused(self) -> bool:
        """TCP 重连或重连后宽限期内暂停 OTA 状态超时"""
        if not getattr(self, 'firmware_update_active', False):
            return False
        if time.time() < getattr(self, '_ota_reconnect_grace_until', 0):
            return True
        if getattr(self, 'is_reconnecting', False):
            return True
        if getattr(self, '_tcp_disconnect_detected_time', None) is not None:
            return True
        return False

    def _resume_ota_after_tcp_reconnect(self):
        """TCP 重连成功后同步设备 OTA 状态并继续传输"""
        if not self.firmware_update_active:
            return
        self._last_status_time = time.time()
        self._ota_reconnect_grace_until = time.time() + self.OTA_RECONNECT_GRACE_S
        self._ota_resume_after_reconnect = True
        msg = "[OTA] TCP 已重连，正在同步设备 OTA 状态..."
        if self._ota_log_visible():
            self.log_receive(msg)
        else:
            print(msg)
        ota_query_cmd = NetRawProtocol.build_read_multi(
            addr=self._get_ota_device_addr(),
            reg=NetRawProtocol.REG_OTA_STATE,
            qty=4,
        )
        if not self.send_ota_data(ota_query_cmd):
            self._ota_packet_pending = True

    def handle_ota_status_response(self, state: int, written_bytes: int):
        """处理 OTA 状态响应（纯事件驱动）"""
        if not self.firmware_update_active:
            return
        self.ota_last_state = state
        
        state_names = {
            0: "IDLE",
            1: "STARTED",
            2: "VERIFY",
            3: "ERROR",
            4: "DONE"
        }
        
        state_name = state_names.get(state, f"UNKNOWN({state})")
        if self._ota_log_visible():
            self.log_receive(f"[OTA] {state_name}")
        
        # 只要收到任何状态，就重置超时计数
        self._last_status_time = time.time()
        
        # 如果设备进入 ERROR 状态，立即停止（任何阶段）
        if state == 3:
            if self._ota_log_visible():
                self.log_receive("[OTA] 设备报告错误！")
            # 校验失败：进度条只刷到 99.9%
            self._fast_progress_to_100(100, final_value=99.9)
            # 延时 150ms 等待进度条更新完成，然后停止 OTA
            self.root.after(150, lambda: self.stop_firmware_update(
                success=False, failure_reason="设备校验失败"))
            return
        
        # 如果设备进入 STARTED 状态
        if state == 1:
            # 第一次收到 STARTED 状态，记录日志并触发第一个数据包
            if getattr(self, '_ota_waiting_for_started', False):
                self._ota_waiting_for_started = False
                # 重置重试计数器
                self._ota_start_retry_count = 0
                if self._ota_log_visible():
                    self.log_receive("[OTA] 设备已进入接收状态")
                if self._ota_log_visible():
                    self.log_receive("[OTA] 开始发送固件数据")
                # 触发第一个数据包
                self._ota_packet_pending = True
                return
            
            # written_bytes 增加说明校验通过，发送下一包
            if written_bytes >= self.firmware_sent_bytes:
                if written_bytes > self.firmware_sent_bytes:
                    self.firmware_sent_bytes = written_bytes
                    self._ota_reconnect_grace_until = 0.0
                    self._ota_resume_after_reconnect = False
                    progress = (self.firmware_sent_bytes / self.firmware_total_size) * 100
                    self.update_progress(progress)
                    if self._ota_log_visible():
                        self.log_receive(
                            f"[OTA] {self.firmware_sent_bytes}/"
                            f"{self.firmware_total_size}"
                        )
                    
                    # 设置数据包发送标志
                    self._ota_packet_pending = True
                elif getattr(self, '_ota_resume_after_reconnect', False):
                    self._ota_resume_after_reconnect = False
                    self._ota_reconnect_grace_until = 0.0
                    if self._ota_log_visible():
                        self.log_receive(
                            f"[OTA] 已同步设备进度 {written_bytes} 字节，继续发送固件"
                        )
                    self._ota_packet_pending = True
                elif written_bytes < self.firmware_sent_bytes:
                    # 以设备已写入字节为准（断线期间可能漏收状态）
                    self.firmware_sent_bytes = written_bytes
                    progress = (self.firmware_sent_bytes / self.firmware_total_size) * 100
                    self.update_progress(progress)
                    self._ota_packet_pending = True
                else:
                    # written_bytes == firmware_sent_bytes，可能是重复状态上报
                    # 但如果已经发送完成，也需要触发结束指令
                    if self.firmware_sent_bytes >= self.firmware_total_size:
                        # 所有数据已发送完成，触发结束指令
                        self._ota_packet_pending = True
                    # 否则忽略重复状态
        # 如果设备进入 VERIFY 状态，说明当前包已接收完成，正在校验
        elif state == 2:
            # 等待设备校验完成并返回 STARTED 状态
            pass
        
        # 如果设备进入 DONE 状态，说明全部完成
        elif state == 4:
            if self._ota_log_visible():
                self.log_receive("[OTA] 固件校验成功，发送重启指令...")
            # 校验成功：进度条刷到 100%
            self._fast_progress_to_100(100, final_value=100.0)
            # 延时 150ms 等待进度条更新完成，然后发送重启指令并弹窗
            self.root.after(150, lambda: self._send_reboot_command_and_finish())
    
    def send_next_ota_packet(self):
        """发送下一个 OTA 数据包"""
        if not self.firmware_update_active:
            return
        
        # 计算本次发送的数据量（由「单帧最大字节」决定，128 或 224）
        remaining = self.firmware_total_size - self.firmware_sent_bytes
        packet_size = self._calc_ota_packet_size(remaining)
        
        if packet_size <= 0:
            # 所有数据发送完成，发送结束指令
            self.finish_ota_update()
            return
        
        # 获取数据
        start_offset = self.firmware_sent_bytes
        packet_data = self.firmware_data[start_offset:start_offset + packet_size]
        
        # 确保 packet_data 长度是偶数（2 字节=1 个寄存器）
        if len(packet_data) % 2 != 0:
            packet_data = packet_data + b'\x00'  # 补齐到偶数
        
        # 构建写多寄存器指令（寄存器 208 开始）
        # 将字节数据转换为 16 位寄存器值（小端序）
        register_values = []
        for i in range(0, len(packet_data), 2):
            word = packet_data[i] | (packet_data[i + 1] << 8)
            register_values.append(word)
        
        ota_data_cmd = NetRawProtocol.build_write_multi(
            addr=self._get_ota_device_addr(),
            reg=NetRawProtocol.REG_OTA_DATA,
            values=register_values
        )
        
        # 显示发送数据（按照 send_frame 的格式）
        # 注意：大文件 OTA 时禁用详细显示，避免 GUI 卡顿
        # 每个数据包显示约 10-15 行，713KB 固件约 5500 个包 = 55000-82500 行
        # 即使有 5000 行限制，频繁的插入和滚动也会导致卡顿
        # timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        # self.tx_text.insert(tk.END, f"[{timestamp}]\n", 'black')
        # color = self.tx_colors[self.tx_color_index % len(self.tx_colors)]
        # self.tx_color_index += 1
        # hex_str = " ".join(f"{b:02X}" for b in ota_data_cmd)
        # for i in range(0, len(hex_str.split()), 16):
        #     line = ' '.join(hex_str.split()[i:i+16])
        #     self.tx_text.insert(tk.END, line + "\n", ('color_' + color))
        # self.tx_text.see(tk.END)
        
        # 发送数据（直接通过 TCP 通道）
        self.send_ota_data(ota_data_cmd)
        # self.log_receive(f"[OTA] 发送数据包 #{int(start_offset / 128) + 1}: offset={start_offset}, size={packet_size} bytes")
        
        # 记录发送时间（用于超时检测）
        self.packet_send_time = time.time()
        
        # 不再主动轮询，依赖单片机主动上传
        # self.root.after(200, self.poll_ota_state_after_send)
        
        # 不再创建定时器，由 OTA 线程统一管理超时检查
        # self.root.after(500, self._check_response_timeout)
    
    def _display_tx_data(self, ota_data_cmd: bytes):
        """在发送框显示数据（在主线程中执行）"""
        if not self._should_show_tcp_tx_panel():
            return
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        self.tx_text.insert(tk.END, f"[{timestamp}]\n", 'black')
        color = self.tx_colors[self.tx_color_index % len(self.tx_colors)]
        self.tx_color_index += 1
        hex_str = " ".join(f"{b:02X}" for b in ota_data_cmd)
        for i in range(0, len(hex_str.split()), 16):
            line = ' '.join(hex_str.split()[i:i+16])
            self.tx_text.insert(tk.END, line + "\n", ('color_' + color))
        self.tx_text.see(tk.END)
    
    def poll_ota_state_after_send(self):
        """发送数据包后定时查询状态（暂时关闭，依赖单片机主动上传）"""
        if not self.firmware_update_active:
            return
        
        # 暂时关闭主动查询，依赖单片机主动上传
        # 发送读寄存器指令，读取寄存器 204-207（OTA 状态）
        # ota_query_cmd = NetRawProtocol.build_read_multi(
        #     addr=int(self.addr_var.get()),
        #     reg=NetRawProtocol.REG_OTA_STATE,
        #     qty=4  # 4 个寄存器 = 8 字节（state 32 位 + written_bytes 32 位）
        # )
        # 
        # self.send_data(ota_query_cmd)
        
        # 检查是否超时（2000ms 后状态仍然不是 STARTED）
        # 给设备足够的时间写入 Flash（每包最多 128 字节）
        # 暂时关闭超时检测，依赖单片机主动上传
        elapsed_ms = (time.time() - self.packet_send_time) * 1000
        if elapsed_ms >= 2000 and self.ota_last_state != 1:  # 1 = OTA_STATE_STARTED
            self.log_receive(f"[OTA] 数据包写入超时！设备未在 2000ms 内完成写入（当前状态：{self.ota_last_state}）")
            # 固件包写入失败
            self.stop_firmware_update(success=False, failure_reason="固件包写入失败（设备未在 2 秒内完成写入）")
            return
        
        # 继续定时查询（每 200ms 一次），直到状态变化 - 暂时关闭
        # self.root.after(200, self.poll_ota_state_after_send)
    
    def finish_ota_update(self):
        """完成 OTA 更新（发送结束指令，包含 CRC32）"""
        if not self.firmware_update_active:
            return
        
        # 计算固件数据的 CRC32（使用与单片机相同的 BL_Crc32 算法）
        crc32_value = self._bl_crc32(self.firmware_data)
        
        self.log_receive(f"[OTA] 所有数据包发送完成，发送结束指令（CRC32: 0x{crc32_value:08X}）")
        
        # 更新进度条为 99.9%（表示数据发送完成，但还在等待校验）
        self.update_progress(99.9)
        
        # 构建写多寄存器指令（寄存器 202-203，写入 CRC32）
        ota_finish_cmd = NetRawProtocol.build_write_multi(
            addr=self._get_ota_device_addr(),
            reg=NetRawProtocol.REG_OTA_CRC32,
            values=[crc32_value & 0xFFFF, (crc32_value >> 16) & 0xFFFF]
        )
        
        # 发送结束指令（直接通过 TCP 通道）
        self.send_ota_data(ota_finish_cmd)
        
        # 记录发送时间（用于超时检测）
        self.packet_send_time = time.time()
        
        # 不再主动轮询，依赖单片机主动上传
        # self.root.after(200, self.poll_ota_state_after_finish)
    
    def send_reboot_command(self):
        """发送重启指令（2 次）- 已弃用，改用 _send_reboot_command_in_thread"""
        # 此方法已被 _send_reboot_command_in_thread 替代
        # 避免使用 root.after 创建定时器
        pass
    
    def _bl_crc32(self, data: bytes) -> int:
        """
        计算 CRC32（与单片机 BL_Crc32 算法一致）
        多项式：0x04C11DB7
        初始值：0xFFFFFFFF
        输入：高位对齐，左移
        输出：不异或
        
        使用查表法优化性能（比逐位计算快 10-20 倍）
        """
        # CRC32 查找表（多项式 0x04C11DB7）
        # 静态表，避免重复计算
        if not hasattr(self, '_crc32_table'):
            self._crc32_table = []
            for i in range(256):
                crc = i << 24
                for _ in range(8):
                    if crc & 0x80000000:
                        crc = (crc << 1) ^ 0x04C11DB7
                    else:
                        crc <<= 1
                self._crc32_table.append(crc & 0xFFFFFFFF)
        
        # 使用查找表快速计算
        crc = 0xFFFFFFFF
        for byte in data:
            table_index = ((crc >> 24) ^ byte) & 0xFF
            crc = self._crc32_table[table_index] ^ ((crc << 8) & 0xFFFFFFFF)
        
        return crc & 0xFFFFFFFF
    
    def poll_ota_state_after_finish(self):
        """发送结束指令后定时查询状态（暂时关闭，依赖单片机主动上传）"""
        if not self.firmware_update_active:
            return
        
        # 暂时关闭主动查询，依赖单片机主动上传
        # 发送读多寄存器指令，读取寄存器 204-207（OTA 状态）
        # ota_query_cmd = NetRawProtocol.build_read_multi(
        #     addr=int(self.addr_var.get()),
        #     reg=NetRawProtocol.REG_OTA_STATE,
        #     qty=4  # 4 个寄存器 = 8 字节（state 32 位 + written_bytes 32 位）
        # )
        # 
        # self.send_data(ota_query_cmd)
        
        # 检查是否超时（2000ms 后状态仍然不是 DONE）
        # 暂时关闭超时检测，依赖单片机主动上传
        elapsed_ms = (time.time() - self.packet_send_time) * 1000
        if elapsed_ms >= 2000:
            if self.ota_last_state != 4:  # 4 = OTA_STATE_DONE
                self.log_receive(f"[OTA] 结束指令超时！设备未在 2000ms 内完成校验（当前状态：{self.ota_last_state}）")
                # 固件校验失败
                self.stop_firmware_update(success=False, failure_reason="固件校验失败（设备未在 2 秒内完成校验）")
                return
            else:
                # 已经收到 DONE 状态，停止查询
                return
        
        # 继续定时查询（每 200ms 一次），直到收到 DONE 状态 - 暂时关闭
        # if self.ota_last_state != 4:
        #     self.root.after(200, self.poll_ota_state_after_finish)
    
    # _check_response_timeout 方法已删除
    # 超时检查现在由 _tcp_ota_worker 线程统一管理
    
    def _reconnect_after_update(self):
        """BootLoader 更新完成后重连串口（仅更新串口状态，不切换当前 UI 通信模式）"""
        try:
            self.stop_connection_monitor("serial")
            self.serial_monitor.suspend_link()
            self.serial_connected = False
            self._sync_serial_link_ui()

            port = self._normalize_serial_port(
                self._connected_serial_port or self.port_combo.get()
            )
            if not port:
                self._on_bootloader_post_update_reconnect_failed()
                return

            baudrate = getattr(self.serial_monitor, "target_baudrate", None) or 921600
            try:
                baudrate = int(self.baud_combo.get())
            except (ValueError, tk.TclError):
                pass

            for _ in range(20):
                if self.serial_monitor.connect(port, baudrate):
                    self._on_bootloader_post_update_reconnected(port, baudrate)
                    return
                time.sleep(0.5)

            self._on_bootloader_post_update_reconnect_failed()
        except Exception:
            self._on_bootloader_post_update_reconnect_failed()
    
    def _set_buttons_state(self, enabled: bool):
        """设置按钮状态
        
        Args:
            enabled: True=启用，False=禁用（除了停止更新和清空按钮）
        """
        # 协议指令发送按钮
        if hasattr(self, 'cmd_send_button') and self.cmd_send_button:
            state = 'normal' if enabled else 'disabled'
            self.cmd_send_button.config(state=state)
        
        # 快捷指令按钮
        if hasattr(self, 'quick_buttons'):
            state = 'normal' if enabled else 'disabled'
            for btn in self.quick_buttons:
                btn.config(state=state)

        if hasattr(self, 'dose_thr_widgets'):
            ui_state = tk.NORMAL if enabled else tk.DISABLED
            for w in self.dose_thr_widgets:
                try:
                    w.config(state="readonly" if (not enabled and isinstance(w, ttk.Combobox)) else ui_state)
                except tk.TclError:
                    w.config(state=ui_state)

        if hasattr(self, 'alarm_setting_widgets'):
            ui_state = tk.NORMAL if enabled else tk.DISABLED
            for w in self.alarm_setting_widgets:
                try:
                    w.config(state="readonly" if (not enabled and isinstance(w, ttk.Combobox)) else ui_state)
                except tk.TclError:
                    w.config(state=ui_state)

        if hasattr(self, 'cmd_dev_addr_widgets'):
            ui_state = tk.NORMAL if enabled else tk.DISABLED
            for w in self.cmd_dev_addr_widgets:
                w.config(state=ui_state)
        
        # 自定义指令发送按钮
        if hasattr(self, 'custom_send_button') and self.custom_send_button:
            state = 'normal' if enabled else 'disabled'
            self.custom_send_button.config(state=state)
        
        # 停止更新按钮始终保持可点击（不受 enabled 参数影响）
        # 不需要在这里设置，因为它应该一直可用
    
    def user_stop_update(self):
        """用户点击停止按钮"""
        # 只有当前处于更新状态才执行停止操作
        if self.firmware_update_active or self.iap_update_active or self.app_update_active:
            self.stop_firmware_update(user_cancelled=True)
    
    def stop_firmware_update(self, success: bool = False, failure_reason: str = None, user_cancelled: bool = False):
        """停止固件更新
        
        Args:
            success: 是否成功完成
            failure_reason: 失败原因（仅在 success=False 时使用）
            user_cancelled: 是否是用户主动取消
        """
        # 获取当前 OTA 模式（用于日志前缀）
        ota_mode = self.ota_mode_var.get()
        log_prefix = "[USB DFU]" if ota_mode == "BootLoader" else "[OTA]"
        
        # 如果是用户主动取消，立即设置中止标志
        if user_cancelled:
            self.bootloader_update_aborted = True
            self.log_receive(f"{log_prefix} 正在停止更新...")
        
        self.firmware_update_active = False
        self._ota_device_addr = None
        self._ota_packet_max_bytes = None
        self._ota_reconnect_grace_until = 0.0
        self._ota_resume_after_reconnect = False
        self._restore_auto_ack_after_ota()
        self.firmware_sent_bytes = 0
        self.firmware_data = b''
        self.firmware_total_size = 0
        self.ota_last_state = 0
        if hasattr(self, 'ota_mode'):
            del self.ota_mode  # 清除保存的通信模式
        
        # 重置 IAP 和 APP 更新状态
        self.iap_update_active = False
        self.app_update_active = False
        
        # 如果是 BootLoader 模式或 TCP OTA 模式，设置中止标志并等待线程结束
        mode = self.ota_mode_var.get()
        if mode == "BootLoader":
            self.bootloader_update_aborted = True
            # 等待后台线程结束
            if self.bootloader_update_thread and self.bootloader_update_thread.is_alive():
                self.bootloader_update_thread.join(timeout=2.0)
            self.bootloader_update_thread = None
        elif mode == "TCP":
            # TCP OTA 模式：等待后台线程结束
            if hasattr(self, 'tcp_ota_thread') and self.tcp_ota_thread and self.tcp_ota_thread.is_alive():
                self.tcp_ota_thread.join(timeout=2.0)
            self.tcp_ota_thread = None
        
        if success:
            # 固件更新成功
            update_type = getattr(self, 'current_update_type', '固件')
            # 显示对应的提示词和弹窗
            if update_type == 'IAP':
                self.log_receive(f"{log_prefix} IAP 固件更新完成")
                self.root.after(100, lambda: messagebox.showinfo(
                    "IAP 固件更新完成",
                    "IAP 固件更新成功！",
                    parent=self.root
                ))
            elif update_type == 'APP':
                self.log_receive(f"{log_prefix} APP 固件更新完成")
                self.root.after(100, lambda: messagebox.showinfo(
                    "APP 固件更新完成",
                    "APP 固件更新成功！",
                    parent=self.root
                ))
            else:
                self.log_receive(f"{log_prefix} {update_type}更新完成")
                self.root.after(100, lambda: messagebox.showinfo(
                    "更新完成",
                    f"{update_type}更新成功！",
                    parent=self.root
                ))
            
            # BootLoader 完成后设备会复位，旧串口句柄失效；先标记断开再尝试重连
            if mode == "BootLoader" and self._connected_serial_port:
                self.serial_monitor.suspend_link()
                self.serial_connected = False
                self._sync_serial_link_ui()
                self.root.after(1000, self._reconnect_after_update)
        else:
            # 固件更新失败
            # 如果是用户主动取消，不显示失败消息
            if not user_cancelled:
                if failure_reason:
                    self.log_receive(f"{log_prefix} 固件更新失败：{failure_reason}")
                    self.root.after(100, lambda: messagebox.showerror(
                        "固件更新失败",
                        f"{failure_reason}！",
                        parent=self.root
                    ))
                else:
                    self.log_receive(f"{log_prefix} 固件更新已停止")
        
        # 恢复所有相关按钮
        self._set_buttons_state(True)  # 恢复按钮状态
        # 停止更新按钮一直保持可点击，不需要设置
        
        self.iap_browse_btn.config(state=tk.NORMAL)
        self.app_browse_btn.config(state=tk.NORMAL)
        
        # 根据当前模式重新设置按钮状态（TCP 模式下禁用 IAP 更新）
        self._update_ota_buttons_state()
    
    def update_progress(self, value):
        """更新进度条
        
        Args:
            value: 进度值（0-100，可以是小数）
        """
        self.firmware_progress['value'] = value
        self.progress_label.config(text=f"{value:.1f}%")
    
    def _fast_progress_to_100(self, duration_ms=100, final_value=100.0):
        """快速更新进度条到指定值（100ms 内完成）
        
        Args:
            duration_ms: 总持续时间（毫秒），默认 100ms
            final_value: 最终进度值，默认 100.0（校验失败时可为 99.9）
        """
        if not self.firmware_update_active:
            return
        
        # 分 34 步更新，每步约 3ms，每步增加 3%（0%, 3%, 6%, ..., 99%, final_value）
        steps = 34  # 100/3 ≈ 33.33，向上取整为 34 步
        step_delay = duration_ms // steps  # 约 2.9ms 每步
        step_value = 3.0  # 每步增加 3%
        
        def update_step(step):
            if not self.firmware_update_active:
                return
            # 计算当前进度，最后一步使用 final_value
            if step >= steps:
                progress = final_value
            else:
                progress = min(step * step_value, final_value)
            self.update_progress(progress)
            if step < steps:
                self.root.after(step_delay, lambda s=step+1: update_step(s))
        
        # 启动更新：从第 0 步开始（0%）
        update_step(0)
    
    # ==================== 连接监控相关方法 ====================
    
    def _tcp_monitor_loop(self):
        """TCP 连接监控循环"""
        retry_count = 0
        max_retries = 5
        
        while self.tcp_is_monitoring:
            try:
                is_connected = self._is_tcp_monitor_connected()
                if not is_connected and self.tcp_connected and not self._tcp_manual_disconnect:
                    if self.tcp_dual.single_port_mode:
                        client = self.tcp_dual.single
                        if client.read_error or (
                                client.read_thread and not client.read_thread.is_alive()):
                            self.log_receive("[TCP 监控] TCP 连接已失效")
                    else:
                        if self.tcp_dual.ctrl.read_error or (
                                self.tcp_dual.ctrl.enable_reader and
                                self.tcp_dual.ctrl.read_thread and
                                not self.tcp_dual.ctrl.read_thread.is_alive()):
                            self.log_receive("[TCP 监控] 控制口连接已失效")
                        if self.tcp_dual.data.read_error or (
                                self.tcp_dual.data.read_thread and
                                not self.tcp_dual.data.read_thread.is_alive()):
                            self.log_receive("[TCP 监控] 数据口连接已失效")
                
                if not is_connected:
                    # 检测到断开
                    if self._tcp_manual_disconnect:
                        # 如果是主动断开，直接退出，不重连
                        self.log_receive("[TCP 监控] 检测到主动断开，停止监控")
                        break
                    
                    if self._tcp_disconnect_detected_time is None:
                        # 第一次检测到断开
                        self._tcp_disconnect_detected_time = time.time()
                        self.tcp_connected = False
                        self.is_reconnecting = True
                        self.log_receive("[TCP 监控] 检测到连接断开，正在尝试重连...")
                        if self.firmware_update_active:
                            self._last_status_time = time.time()
                            ota_msg = "[OTA] TCP 断开，暂停状态超时等待重连..."
                            if self._ota_log_visible():
                                self.log_receive(ota_msg)
                            else:
                                print(ota_msg)
                        self.root.after(0, self._update_tcp_status_ui)
                    
                    # 尝试重连
                    retry_count += 1
                    if retry_count <= max_retries:
                        self.log_receive(f"[TCP 监控] 第 {retry_count} 次重连...")
                        
                        # 先完全断开并清理
                        try:
                            self.tcp_dual.disconnect()
                        except Exception:
                            pass
                        
                        time.sleep(0.5)  # 等待 TCP 完全释放
                        
                        # 重新连接
                        ip = self.tcp_ip_entry.get().strip()
                        try:
                            ctrl_port, data_port = self._get_tcp_ports()
                        except ValueError:
                            ctrl_port, data_port = DEFAULT_TCP_CTRL_PORT, DEFAULT_TCP_DATA_PORT
                        
                        try:
                            ok_ctrl, ok_data = self.tcp_dual.connect(ip, ctrl_port, data_port)
                            if ok_ctrl and ok_data:
                                port_desc = self._tcp_port_desc(ctrl_port, data_port)
                                self.log_receive(f"[TCP 监控] 重连成功：{ip} {port_desc}")
                                # 重连成功，重置状态
                                self._tcp_disconnect_detected_time = None
                                retry_count = 0
                                self.tcp_connected = True
                                self.is_reconnecting = False
                                self._note_tcp_link_up()
                                if self.firmware_update_active:
                                    self._resume_ota_after_tcp_reconnect()
                                self.root.after(0, self._update_tcp_status_ui)
                            else:
                                port_desc = self._tcp_port_desc(ctrl_port, data_port)
                                self.log_receive(f"[TCP 监控] 重连失败：{ip} {port_desc}")
                        except Exception as e:
                            self.log_receive(f"[TCP 监控] 重连异常：{e}")
                    else:
                        # 超过最大重试次数
                        self.log_receive(f"[TCP 监控] 重连失败，已重试 {max_retries} 次")
                        self.tcp_connected = False
                        self.is_reconnecting = False
                        self._tcp_disconnect_detected_time = None
                        if self.firmware_update_active:
                            self.root.after(0, lambda: self.stop_firmware_update(
                                success=False, failure_reason="TCP 连接断开，无法继续 OTA"))
                        self.root.after(0, self._update_tcp_status_ui)
                        self.root.after(0, lambda: (
                            self.tcp_ip_entry.config(state=tk.NORMAL) if hasattr(self, 'tcp_ip_entry') else None,
                            self.tcp_ctrl_port_entry.config(state=tk.NORMAL) if hasattr(self, 'tcp_ctrl_port_entry') else None,
                            self.tcp_data_port_entry.config(state=tk.NORMAL) if hasattr(self, 'tcp_data_port_entry') else None,
                            self._reset_tcp_addr_display()
                        ))
                        break
                else:
                    # 连接正常，重置状态
                    if self._tcp_disconnect_detected_time is not None:
                        self._tcp_disconnect_detected_time = None
                        retry_count = 0
                        was_reconnecting = self.is_reconnecting
                        self.tcp_connected = True
                        self.is_reconnecting = False
                        if was_reconnecting and self.firmware_update_active:
                            self._resume_ota_after_tcp_reconnect()
                        self.root.after(0, self._update_tcp_status_ui)
                    
                    # 每 1 秒检测一次
                time.sleep(1.0)
                
            except Exception as e:
                print(f"TCP 监控异常：{e}")
                time.sleep(1.0)
        
        # 监控线程退出
        self.tcp_is_monitoring = False
    
    def _udp_monitor_loop(self):
        """UDP 连接监控循环（检测网卡断开并自动重绑）"""
        retry_count = 0
        max_retries = 5
        
        while self.udp_is_monitoring:
            try:
                is_connected = self._check_udp_connection()
                
                if not is_connected:
                    if self._udp_manual_disconnect:
                        self.log_receive("[UDP 监控] 检测到主动断开，停止监控")
                        break
                    
                    if self._udp_disconnect_detected_time is None:
                        self._udp_disconnect_detected_time = time.time()
                        local_ip = getattr(self, '_udp_bound_local_ip', '0.0.0.0')
                        if local_ip and local_ip != "0.0.0.0" and not self._is_local_ip_available(local_ip):
                            self.log_receive(f"[UDP 监控] 本地网卡 {local_ip} 已断开，正在等待恢复并重绑...")
                        else:
                            self.log_receive("[UDP 监控] UDP 绑定失效，正在尝试重绑...")
                        self.root.after(0, lambda: self.udp_status_label.config(
                            text="重新连接中...", foreground="orange"))
                    
                    port = getattr(self, '_udp_bound_port', None) or 2468
                    multicast_ip = getattr(self, '_udp_bound_multicast', None) or "236.2.3.6"
                    local_ip = getattr(self, '_udp_bound_local_ip', None) or "0.0.0.0"
                    
                    if local_ip != "0.0.0.0" and not self._is_local_ip_available(local_ip):
                        if self.udp_connected:
                            try:
                                self.udp_server.unbind()
                            except Exception:
                                pass
                            self.udp_connected = False
                        self._udp_nic_waiting = True
                        self.log_receive(f"[UDP 监控] 本地 IP {local_ip} 仍不可用，等待网卡恢复...")
                        time.sleep(2.0)
                    elif retry_count < max_retries:
                        if self._udp_nic_waiting:
                            self.log_receive(f"[UDP 监控] 本地网卡 {local_ip} 已恢复，正在重绑...")
                            self._udp_nic_waiting = False
                        retry_count += 1
                        self.log_receive(f"[UDP 监控] 第 {retry_count} 次重绑...")
                        
                        try:
                            self.udp_server.unbind()
                        except Exception:
                            pass
                        
                        time.sleep(0.5)
                        
                        if self.udp_server.bind(local_ip, port, multicast_ip):
                            self._udp_bound_local_ip = local_ip
                            self._udp_bound_port = port
                            self._udp_bound_multicast = multicast_ip
                            self._udp_rebind_grace_until = time.time() + 1.5
                            if self._udp_disconnect_detected_time is not None and local_ip != "0.0.0.0":
                                self.log_receive(
                                    f"[UDP 监控] 网卡恢复，重绑成功：{local_ip} -> {multicast_ip}:{port}")
                            else:
                                self.log_receive(f"[UDP 监控] 重绑成功：{local_ip} -> {multicast_ip}:{port}")
                            self._udp_disconnect_detected_time = None
                            retry_count = 0
                            self.udp_connected = True
                            self._udp_nic_waiting = False
                            lip = local_ip
                            self.root.after(0, lambda ip=lip: self._sync_udp_local_ip_combo(ip))
                            self.root.after(0, lambda: self.udp_status_label.config(
                                text=f"Link: {multicast_ip}:{port}", foreground="green"))
                            self.root.after(0, lambda: self.udp_connect_btn.config(text="断开"))
                        else:
                            self.log_receive("[UDP 监控] 重绑失败")
                    else:
                        self.log_receive(f"[UDP 监控] 重绑失败，已重试 {max_retries} 次")
                        self.udp_connected = False
                        self.root.after(0, lambda: (
                            self.udp_status_label.config(text="未连接", foreground="red"),
                            self.udp_connect_btn.config(text="连接")
                        ))
                        self.root.after(0, lambda: (
                            self.udp_ip_entry.config(state=tk.NORMAL) if hasattr(self, 'udp_ip_entry') else None,
                            self.udp_port_entry.config(state=tk.NORMAL) if hasattr(self, 'udp_port_entry') else None,
                            self.udp_local_ip_combo.config(state="readonly") if hasattr(self, 'udp_local_ip_combo') else None
                        ))
                        break
                    time.sleep(1.0)
                else:
                    if self._udp_disconnect_detected_time is not None:
                        self._udp_disconnect_detected_time = None
                        retry_count = 0
                        self._udp_nic_waiting = False
                    
                    time.sleep(1.0)
                
            except Exception as e:
                print(f"UDP 监控异常：{e}")
                time.sleep(1.0)
        
        self.udp_is_monitoring = False
    
    def start_connection_monitor(self, mode: Optional[str] = None):
        """启动指定模式的连接监控线程（各模式互不干扰）"""
        mode = (mode or self.current_mode).lower()
        
        if mode == "serial" and not self.serial_monitor.watchdog_running:
            self.serial_is_monitoring = True
            self.serial_monitor.manual_disconnect = False
            self.serial_monitor.start_watchdog()
        elif mode == "tcp" and not self.tcp_is_monitoring:
            self.tcp_is_monitoring = True
            self.tcp_monitor_thread = threading.Thread(target=self._tcp_monitor_loop, daemon=True)
            self.tcp_monitor_thread.start()
            self.log_receive("[连接监控] 启动 TCP 监控线程")
        elif mode == "udp" and not self.udp_is_monitoring:
            self.udp_is_monitoring = True
            self.udp_monitor_thread = threading.Thread(target=self._udp_monitor_loop, daemon=True)
            self.udp_monitor_thread.start()
            self.log_receive("[连接监控] 启动 UDP 监控线程")
    
    def stop_connection_monitor(self, mode: Optional[str] = None):
        """停止连接监控；mode 为 None 时停止全部"""
        modes = {mode.lower()} if mode else {"serial", "tcp", "udp"}
        
        if "serial" in modes:
            self.serial_is_monitoring = False
            self.serial_monitor.stop_watchdog()
        
        if "tcp" in modes:
            self.tcp_is_monitoring = False
            if self.tcp_monitor_thread and self.tcp_monitor_thread.is_alive():
                self.tcp_monitor_thread.join(timeout=0.5)
            self.tcp_monitor_thread = None
        
        if "udp" in modes:
            self.udp_is_monitoring = False
            if self.udp_monitor_thread and self.udp_monitor_thread.is_alive():
                self.udp_monitor_thread.join(timeout=0.5)
            self.udp_monitor_thread = None
    
    def _try_reconnect_tcp(self):
        """尝试重连 TCP（后台线程调用）"""
        try:
            # 再次检查主动断开标志
            if self._tcp_manual_disconnect:
                self.log_receive("[TCP 监控] 检测到主动断开，停止重连")
                return
            
            # 先完全断开并清理
            try:
                self.tcp_dual.disconnect()
            except Exception as e:
                self.log_receive(f"[TCP 监控] 关闭 TCP 失败：{e}")
            
            time.sleep(0.5)  # 等待 TCP 完全释放
            
            # 尝试重新连接
            ip = self.tcp_ip_entry.get().strip()
            ctrl_port, data_port = self._get_tcp_ports()
            port_desc = self._tcp_port_desc(ctrl_port, data_port)
            self.log_receive(f"[TCP 监控] 尝试重连：{ip} {port_desc}")
            
            ok_ctrl, ok_data = self.tcp_dual.connect(ip, ctrl_port, data_port)
            if ok_ctrl and ok_data:
                self.log_receive("[TCP 监控] 重连成功")
                self._note_tcp_link_up()
                # 重连成功，重置断开时间
                self._tcp_disconnect_detected_time = None
                self.is_reconnecting = False
                label = self._tcp_link_label(ip)
                self.root.after(0, lambda lbl=label: self.tcp_status_label.config(
                    text=lbl, foreground="green"))
                self.root.after(0, lambda: self.tcp_connect_btn.config(text="断开"))
                return  # 成功，直接返回
            
            # 重连失败，等待一段时间后重试
            time.sleep(2)
            
            # 最多重试 5 次
            for i in range(4):
                if self._tcp_manual_disconnect:
                    break
                
                self.log_receive(f"[TCP 监控] 重连失败，第 {i+2} 次重试...")
                ok_ctrl, ok_data = self.tcp_dual.connect(ip, ctrl_port, data_port)
                if ok_ctrl and ok_data:
                    self.log_receive("[TCP 监控] 重连成功")
                    self._note_tcp_link_up()
                    self._tcp_disconnect_detected_time = None
                    self.is_reconnecting = False
                    label = self._tcp_link_label(ip)
                    self.root.after(0, lambda lbl=label: self.tcp_status_label.config(
                        text=lbl, foreground="green"))
                    self.root.after(0, lambda: self.tcp_connect_btn.config(text="断开"))
                    return
            
            # 5 次重试后仍然失败
            self.log_receive("[TCP 监控] 重连失败，已重试 5 次")
            
        except Exception as e:
            print(f"TCP 重连异常：{e}")
    
    def _try_reconnect_udp(self):
        """尝试重连 UDP（后台线程调用）"""
        try:
            # 再次检查主动断开标志
            if self._udp_manual_disconnect:
                self.log_receive("[UDP 监控] 检测到主动断开，停止重连")
                return
            
            # 先完全断开并清理
            try:
                if self.udp_server.socket:
                    self.udp_server.socket.close()
                    self.udp_server.socket = None
                    self.udp_server.bound = False
            except Exception as e:
                self.log_receive(f"[UDP 监控] 关闭 UDP 失败：{e}")
            
            time.sleep(0.5)  # 等待 UDP 完全释放
            
            # 尝试重新绑定（优先使用已记住的绑定参数，避免后台线程读 Tk 控件）
            local_ip = getattr(self, '_udp_bound_local_ip', None) or "0.0.0.0"
            port = getattr(self, '_udp_bound_port', None) or 2468
            multicast_ip = getattr(self, '_udp_bound_multicast', None) or "236.2.3.6"
            
            self.log_receive(f"[UDP 监控] 尝试重连：{local_ip} -> {multicast_ip}:{port}")
            
            if self.udp_server.bind(local_ip, port, multicast_ip):
                self.log_receive("[UDP 监控] 重连成功")
                self._udp_bound_local_ip = local_ip
                self._udp_bound_port = port
                self._udp_bound_multicast = multicast_ip
                self._udp_rebind_grace_until = time.time() + 1.5
                lip = local_ip
                self.root.after(0, lambda ip=lip: self._sync_udp_local_ip_combo(ip))
                # 重连成功，重置断开时间
                self._udp_disconnect_detected_time = None
                self.is_reconnecting = False
                self.root.after(0, lambda mip=multicast_ip, p=port: self.udp_status_label.config(
                    text=f"Link: {mip}:{p}", foreground="green"))
                self.root.after(0, lambda: self.udp_connect_btn.config(text="断开"))
                return  # 成功，直接返回
            
            # 重连失败，等待一段时间后重试
            time.sleep(2)
            
            # 最多重试 5 次
            for i in range(4):
                if self._udp_manual_disconnect:
                    break
                
                self.log_receive(f"[UDP 监控] 重连失败，第 {i+2} 次重试...")
                if self.udp_server.bind(local_ip, port, multicast_ip):
                    self.log_receive("[UDP 监控] 重连成功")
                    self._udp_disconnect_detected_time = None
                    self.is_reconnecting = False
                    self.root.after(0, lambda: self.udp_status_label.config(
                        text=f"Link: {ip}:{port}", foreground="green"))
                    self.root.after(0, lambda: self.udp_connect_btn.config(text="断开"))
                    return
            
            # 5 次重试后仍然失败
            self.log_receive("[UDP 监控] 重连失败，已重试 5 次")
            self.udp_connected = False
            self.root.after(0, lambda: (
                self.udp_status_label.config(text="未连接", foreground="red"),
                self.udp_connect_btn.config(text="连接")
            ))
            # 直接恢复 UDP 的输入框，不依赖当前模式
            self.root.after(0, lambda: (
                self.udp_ip_entry.config(state=tk.NORMAL) if hasattr(self, 'udp_ip_entry') else None,
                self.udp_port_entry.config(state=tk.NORMAL) if hasattr(self, 'udp_port_entry') else None
            ))
            
        except Exception as e:
            print(f"UDP 重连异常：{e}")
    
    def _update_reconnecting_status(self):
        """更新 UI 为重新连接中状态"""
        self.current_status_label.config(text="重新连接中...", foreground="orange")
        self.log_receive("[连接监控] 检测到连接断开，正在尝试重连...")
    
    # ==================== 连接监控相关方法结束 ====================
    
    # ==================== 设备信息配置相关方法（串口模式） ====================

    def _send_serial_text_cmd(self, cmd: str, desc: str) -> None:
        """
        发送单片机文本串口指令（走 send_frame → send_data 串口路由）。
        与原先可用写法一致：指令 bytes 含 \\r\\n，send_data 再追加 \\r\\n。
        """
        if not self._is_serial_port_open():
            messagebox.showwarning("警告", "请连接串口！")
            return
        body = (cmd or "").rstrip("\r\n")
        if not body:
            return
        frame = (body + "\r\n").encode("utf-8")
        prev_mode = self.current_mode
        self.current_mode = "serial"
        try:
            self.send_frame(frame, desc)
        finally:
            self.current_mode = prev_mode
        self.serial_connected = True
        self.is_reconnecting = False
    
    def on_get_device_info(self):
        """获取设备信息 - 发送 IF 指令"""
        cmd = "IF\r\n"
        self.log_receive(f"[设备配置] 获取设备信息：{repr(cmd)}")
        self._send_serial_text_cmd(cmd, "获取设备信息")
    
    def _parse_device_info(self, data: str):
        """解析设备信息数据并更新UI
        
        数据格式：
        SN:xxx\r\n
        HW:xxx\r\n
        SW:xxx\r\n
        SD:May 20 2026  09:48:05\r\n
        IP:xxx.xxx.xxx.xxx : ctrl-> 5001 data -> 5001\r\n
        LORA:addr -> 0x01 ch -> 2(412MHz)\r\n
        """
        # 按行分割数据
        lines = data.split('\r\n')
        
        for line in lines:
            line = line.strip()
            if not line:
                continue
            
            # 解析 SN 行
            if line.startswith('SN:'):
                sn_value = line[3:].strip()  # 提取冒号后的内容
                if sn_value and sn_value != '':
                    if hasattr(self, 'device_sn_var'):
                        self.device_sn_var.set(sn_value)
                    self.log_receive(f"[设备信息] 解析到 SN: {sn_value}")
            
            # 解析 HW 行
            elif line.startswith('HW:'):
                hw_value = line[3:].strip()  # 提取冒号后的内容
                if hw_value and hw_value != '':
                    if hasattr(self, 'device_hw_ver_var'):
                        self.device_hw_ver_var.set(hw_value)
                    self.log_receive(f"[设备信息] 解析到硬件版本: {hw_value}")
            
            # 解析 SW 行
            elif line.startswith('SW:'):
                sw_value = line[3:].strip()
                if sw_value and sw_value != '':
                    if hasattr(self, 'device_sw_ver_label'):
                        self.device_sw_ver_label.config(text=sw_value)
                    self.log_receive(f"[设备信息] 解析到软件版本: {sw_value}")

            elif line.startswith('SD:'):
                sd_value = line[3:].strip()
                if sd_value and sd_value != '':
                    if hasattr(self, 'device_sw_date_label'):
                        self.device_sw_date_label.config(text=sd_value)
                    self.log_receive(f"[设备信息] 解析到软件日期: {sd_value}")
    
            elif line.startswith('IP:'):
                ip_value = line[3:].strip()  # 提取冒号后的内容
                if ip_value and ip_value != '':
                    if hasattr(self, 'device_ip_label'):
                        self.device_ip_label.config(text=ip_value)
                    self.log_receive(f"[设备信息] 解析到 IP 地址：{ip_value}")

            elif line.startswith('LORA:'):
                lora_value = line[5:].strip()
                if lora_value and hasattr(self, 'device_lora_label'):
                    self.device_lora_label.config(text=lora_value, foreground="#26C6DA")
                    self.log_receive(f"[设备信息] 解析到 LoRa: {lora_value}")
    
    def on_set_device_sn(self):
        """设置设备 SN 码"""
        sn = self.device_sn_var.get().strip()
        if not sn:
            self.log_receive("[设备配置] 错误：SN 码不能为空")
            return
        # 发送串口指令：cfg,sn,xxx,end（帧尾 \r\n 由 _send_serial_text_cmd 统一追加）
        cmd = f"cfg,sn,{sn},end"
        self.log_receive(f"[设备配置] 设置 SN: {cmd}")
        self._send_serial_text_cmd(cmd, "设置 SN")
    
    def on_set_device_hw_ver(self):
        """设置设备硬件版本"""
        hw_ver = self.device_hw_ver_var.get().strip()
        if not hw_ver:
            self.log_receive("[设备配置] 错误：硬件版本不能为空")
            return
        # 发送串口指令：cfg,hw,xxx,end
        cmd = f"cfg,hw,{hw_ver},end"
        self.log_receive(f"[设备配置] 设置硬件版本：{cmd}")
        self._send_serial_text_cmd(cmd, "设置硬件版本")
    
    def on_get_device_hw_ver(self):
        """获取设备硬件版本（IF 指令含 HW 字段）"""
        self.on_get_device_info()
    
    def on_get_param_info(self):
        """获取参数信息 - 发送 PM 指令"""
        cmd = "PM\r\n"
        self.log_receive(f"[参数配置] 获取参数信息：{repr(cmd)}")
        self._send_serial_text_cmd(cmd, "获取参数信息")
    
    def _parse_param_info(self, data: str):
        """解析参数配置数据并更新 UI
        
        数据格式：
        ADDR:xx
        SEN:xx.xx cpm/uSv/h
        """
        # 按行分割数据
        lines = data.split('\r\n')
        
        for line in lines:
            line = line.strip()
            if not line:
                continue
            
            # 解析 ADDR 行
            if line.startswith('ADDR:'):
                addr_value = line[5:].strip()  # 提取冒号后的内容
                if addr_value and addr_value != '':
                    # 转换为整数并格式化为两位十六进制
                    try:
                        addr_int = int(addr_value, 16) if addr_value.lower().startswith('0x') else int(addr_value)
                        addr_formatted = f"0x{addr_int:02X}"  # 格式化为两位十六进制
                        if hasattr(self, 'param_addr_var'):
                            self.param_addr_var.set(addr_formatted)
                        self.log_receive(f"[参数信息] 解析到设备地址：{addr_formatted}")
                    except ValueError:
                        self.log_receive(f"[参数信息] 设备地址格式错误：{addr_value}")
            
            # 解析 SEN 行
            elif line.startswith('SEN:'):
                # 提取冒号后的内容，去掉 " cpm/uSv/h" 后缀
                sens_str = line[4:].strip()
                # 提取数值部分（去掉 " cpm/uSv/h"）
                sens_value = sens_str.split(' ')[0].strip()
                if sens_value and sens_value != '':
                    if hasattr(self, 'param_gm_sens_var'):
                        self.param_gm_sens_var.set(sens_value)
                    self.log_receive(f"[参数信息] 解析到灵敏度：{sens_value}")
    
    def on_set_param_addr(self):
        """设置设备地址"""
        try:
            addr_str = self.param_addr_var.get().strip()
            # 支持十六进制 (0x01) 和十进制 (1) 格式
            if addr_str.lower().startswith('0x'):
                addr = int(addr_str, 16)
            else:
                addr = int(addr_str)
            if addr < 0 or addr > 255:
                self.log_receive("[参数配置] 错误：设备地址范围 0-255")
                return
        except ValueError:
            self.log_receive("[参数配置] 错误：设备地址必须是数字")
            return
        # 发送串口指令：cfg,addr,xxx,end
        cmd = f"cfg,addr,{addr},end"
        self.log_receive(f"[参数配置] 设置设备地址：{cmd}")
        self._send_serial_text_cmd(cmd, "设置设备地址")
    
    def on_get_param_addr(self):
        """获取设备地址（PM 指令含 ADDR 字段）"""
        self.on_get_param_info()
    
    def on_set_param_gm_sens(self):
        """设置盖革管灵敏度"""
        try:
            gm_sens = float(self.param_gm_sens_var.get().strip())
            if gm_sens <= 0:
                self.log_receive("[参数配置] 错误：灵敏度必须大于 0")
                return
        except ValueError:
            self.log_receive("[参数配置] 错误：灵敏度必须是数字")
            return
        # 发送串口指令：cfg,sens,xxx,end
        cmd = f"cfg,sens,{gm_sens},end"
        self.log_receive(f"[参数配置] 设置盖革管灵敏度：{cmd}")
        self._send_serial_text_cmd(cmd, "设置灵敏度")
    
    def on_get_param_gm_sens(self):
        """获取盖革管灵敏度（PM 指令含 SEN 字段）"""
        self.on_get_param_info()
    
    def on_time_sync(self):
        """时间同步 - 发送当前时间到单片机"""
        import datetime
        now = datetime.datetime.now()
        # 格式：YYMMDD,HHMMSS
        date_str = now.strftime("%y%m%d")
        time_str = now.strftime("%H%M%S")
        # 发送串口指令：settime,YYMMDD,HHMMSS,end
        cmd = f"settime,{date_str},{time_str},end"
        self.log_receive(f"[参数配置] 时间同步：{cmd}")
        self._send_serial_text_cmd(cmd, "时间同步")
    
    def on_set_lang_zh(self):
        """设置界面语言为中文"""
        cmd = "cfg,lang,0,end"
        self.log_receive(f"[参数配置] 设置中文界面：{cmd}")
        self._send_serial_text_cmd(cmd, "中文界面")
    
    def on_set_lang_en(self):
        """设置界面语言为英文"""
        cmd = "cfg,lang,1,end"
        self.log_receive(f"[参数配置] 设置英文界面：{cmd}")
        self._send_serial_text_cmd(cmd, "英文界面")
    
    def on_ht_test(self):
        """声光测试 - 发送 HT 指令"""
        cmd = "HT\r\n"
        self.log_receive(f"[参数配置] 声光测试：{repr(cmd)}")
        self._send_serial_text_cmd(cmd, "声光测试")
    
    def on_simulate_data(self):
        """模拟数据 - 生成 250 条 5 分钟历史模拟数据"""
        cmd = "data5min,sim,250,end"
        self.log_receive(f"[参数配置] 模拟数据：{cmd}")
        self._send_serial_text_cmd(cmd, "模拟数据")
    
    def on_print_data(self):
        """打印数据 - 发送 5 分钟历史数据读取指令"""
        cmd = "data5min,all,end"
        self.log_receive(f"[参数配置] 打印数据：{cmd}")
        self._send_serial_text_cmd(cmd, "打印数据")
    
    def on_clear_data(self):
        """清空数据 - 清空 5 分钟历史数据"""
        cmd = "CLR,5min,end"
        self.log_receive(f"[参数配置] 清空数据：{cmd}")
        self._send_serial_text_cmd(cmd, "清空数据")
    
    def on_factory_reset(self):
        """恢复出厂设置"""
        cmd = "FD\r\n"
        self.log_receive(f"[参数配置] 恢复出厂设置：{repr(cmd)}")
        self._send_serial_text_cmd(cmd, "恢复出厂设置")
    
    # ==================== 设备信息配置相关方法结束 ====================
    
    def on_closing(self):
        """窗口关闭处理"""
        self._app_closing = True

        self.stop_connection_monitor()

        self.serial_monitor.on_data_received = None
        self.tcp_dual.set_on_data_received(None)
        self.udp_server.on_data_received = None

        try:
            self.serial_monitor.disconnect()
        except Exception:
            pass
        try:
            self.tcp_dual.disconnect()
        except Exception:
            pass
        try:
            self.udp_server.unbind()
        except Exception:
            pass

        self.connected = False
        self.serial_connected = False
        self.tcp_connected = False
        self.udp_connected = False

        try:
            self.root.destroy()
        except tk.TclError:
            pass
    
    def _validate_multicast_ip(self, ip: str) -> bool:
        """验证 IP 地址是否为有效的组播地址或广播地址
        
        Args:
            ip: IP 地址字符串，如 "236.2.3.6"
            
        Returns:
            bool: 如果是有效的组播地址或广播地址返回 True，否则返回 False
        """
        try:
            # 分割 IP 地址
            parts = ip.split('.')
            if len(parts) != 4:
                return False
            
            # 转换为整数
            octets = [int(p) for p in parts]
            
            # 检查每个字节是否在 0-255 范围内
            for octet in octets:
                if octet < 0 or octet > 255:
                    return False
            
            # 检查是否为广播地址 255.255.255.255
            if octets == [255, 255, 255, 255]:
                return True
            
            # 检查是否为组播地址（224.0.0.0 - 239.255.255.255）
            # 组播地址的第一个字节范围：224-239
            first_octet = octets[0]
            if 224 <= first_octet <= 239:
                return True
            
            return False
            
        except (ValueError, AttributeError):
            return False


try:
    import net_raw_pack_support as _nps
    _nps.register_gui_class(NetRawTesterGUI)
except ImportError:
    pass


def main():
    root = tk.Tk()
    app = NetRawTesterGUI(root)
    root.protocol("WM_DELETE_WINDOW", app.on_closing)
    root.mainloop()


if __name__ == "__main__":
    main()
