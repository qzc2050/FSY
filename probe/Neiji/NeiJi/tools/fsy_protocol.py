# -*- coding: utf-8 -*-
"""
辐射报警仪 Net Raw 协议（与 辐射报警仪协议命令和寄存器表10.xlsx / NeiJi Protocol 一致）。
"""
from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import Callable, Dict, Iterator, List, Optional, Tuple

from crc16_modbus import append_crc, verify_crc

# 功能码
FC_READ_HOLDING_REQ = 0x03
FC_WRITE_SINGLE_REQ = 0x06
FC_WRITE_MULTI_REQ = 0x10
FC_READ_SINGLE_REQ = 0x05
FC_READ_HOLDING_RESP = 0x13
FC_WRITE_SINGLE_RESP = 0x16
FC_WRITE_MULTI_RESP = 0x20
FC_READ_SINGLE_RESP = 0x15
FC_ACTIVE_UPLOAD = 0x23
FC_READ_SINGLE_ACTIVE = 0x25

KNOWN_FC = frozenset(
    {
        0x03,
        0x05,
        0x06,
        0x10,
        0x13,
        0x15,
        0x16,
        0x20,
        0x23,
        0x25,
        0x83,
        0x85,
        0x86,
        0x90,
    }
)

DEFAULT_SLAVE_ADDR = 0x01
RT_REG_START = 0x0001
RT_REG_COUNT = 11
RT_PAYLOAD_BYTES = RT_REG_COUNT * 4

REG_SERIALNUM = 86
REG_SERIALNUM_COUNT = 8
REG_ADDRESS = 121
REG_PRODUCT_MODEL = 130
REG_PRODUCT_MODEL_COUNT = 8
REG_PRODUCT_NAME = 146
REG_PRODUCT_NAME_COUNT = 8
REG_CURRENT_IP = 6
REG_CURRENT_IP_COUNT = 2
REG_DOSE_HI_TH = 50
REG_DOSE_LO_TH = 52
REG_U32_COUNT = 2
REG_ALARM_ENABLE = 82
REG_ALARM_ENABLE_COUNT = 2
ALARM_BIT_DOSE_HI = 0
ALARM_BIT_DOSE_LO = 1

# 实时区 0x000D 报警状态 — 环境传感器离线位（与 fsy_regmap.c / alarm_output.c 一致）
ALARM_BIT_AHT20 = (1 << 6) | (1 << 14)
ALARM_BIT_BMP280 = 1 << 10
ALARM_BIT_ENS160 = 1 << 18
ALARM_BIT_PM25 = 1 << 22
ALARM_BIT_ENV_MASK = ALARM_BIT_AHT20 | ALARM_BIT_BMP280 | ALARM_BIT_ENS160 | ALARM_BIT_PM25
REG_TIME = 94
REG_TIME_COUNT = 4
REG_SOFTWARE_VERSION = 98
REG_SOFTWARE_VERSION_COUNT = 10
REG_STATIC_IP = 138
REG_STATIC_IP_COUNT = 2
REG_DHCP_ENABLE = 170
CFG_SN_MAX_LEN = 12
CFG_MODEL_MAX_LEN = 12
CFG_PRODUCT_NAME_MAX_BYTES = 16


def format_dose_rate(raw_x100: int) -> str:
    dose_usv = max(0.0, raw_x100 / 100.0)
    if dose_usv > 999.99:
        return f"{dose_usv / 1000.0:.2f} mSv/h"
    return f"{dose_usv:.2f} μSv/h"


def _fmt_temp(v: int) -> str:
    if v & 0x80000000:
        v -= 0x100000000
    return f"{v / 10.0:.1f} ℃"


def _fmt_press(v: int) -> str:
    return f"{v / 100.0:.1f} hPa ({v} Pa)"


def decode_alarm_status(raw: int) -> List[str]:
    """解析 0x000D 报警状态位 → 中文说明列表。"""
    items: List[str] = []
    if raw & (1 << ALARM_BIT_DOSE_HI):
        items.append("剂量率超上限")
    if raw & (1 << ALARM_BIT_DOSE_LO):
        items.append("剂量率超下限")
    if raw & ALARM_BIT_AHT20:
        items.append("温湿度传感器离线(AHT20)")
    if raw & ALARM_BIT_BMP280:
        items.append("气压传感器离线(BMP280)")
    if raw & ALARM_BIT_ENS160:
        items.append("气体传感器离线(ENS160)")
    if raw & ALARM_BIT_PM25:
        items.append("PM2.5传感器离线")
    return items


def suggest_led_effect(raw: int) -> str:
    """与固件 alarm_output 灯带逻辑一致（不含探头 120s 无计数，该位未上报寄存器）。"""
    if raw & (1 << ALARM_BIT_DOSE_HI):
        return "红快流水（超上限）"
    if raw & (1 << ALARM_BIT_DOSE_LO):
        return "红慢流水（超下限）"
    if raw & ALARM_BIT_ENV_MASK:
        return "红常亮（传感器故障）"
    return "白灯慢流水（正常）"


def format_alarm_status_short(raw: int) -> str:
    items = decode_alarm_status(raw)
    if not items:
        return "正常"
    if len(items) == 1:
        return items[0]
    return f"{items[0]} 等{len(items)}项"


def format_alarm_status_detail(raw: int) -> str:
    lines = [f"原始值 0x{raw:08X}"]
    items = decode_alarm_status(raw)
    if not items:
        lines.append("当前无报警/故障。")
    else:
        lines.append("活动项：")
        for name in items:
            lines.append(f"  · {name}")
    lines.append(f"灯带（光报警开）: {suggest_led_effect(raw)}")
    lines.append("说明: 探头长时间无计数故障未写入本寄存器，仅灯带可表现。")
    return "\n".join(lines)


def _fmt_alarm_status(v: int) -> str:
    short = format_alarm_status_short(v)
    if v == 0:
        return short
    return f"{short}  [0x{v:08X}]"


# 实时寄存器（0x23 主动上传，每个 uint32，地址步进 2）
RT_REGISTER_FMT: Dict[int, Tuple[str, Callable[[int], str]]] = {
    0x0001: ("剂量率", format_dose_rate),
    0x0003: ("温度", _fmt_temp),
    0x0005: ("气压", _fmt_press),
    0x0007: ("湿度", lambda v: f"{v:.1f} %"),
    0x0009: ("CO2", lambda v: f"{v:.0f} ppm"),
    0x000B: ("PM2.5", lambda v: f"{v / 10.0:.1f} μg/m³"),
    0x000D: ("报警状态", _fmt_alarm_status),
    0x000F: ("设备/IO状态", lambda v: f"0x{v:08X}"),
    0x0011: ("预留1", lambda v: f"0x{v:08X}"),
    0x0013: ("预留2", lambda v: f"0x{v:08X}"),
    0x0015: ("预留3", lambda v: f"0x{v:08X}"),
}


@dataclass
class ParsedFrame:
    addr: int
    func: int
    raw: bytes
    reg_addr: int = 0
    reg_qty: int = 0
    reg_val: int = 0
    byte_count: int = 0
    payload: bytes = b""
    error_code: int = 0


def build_read_holding(addr: int, start_reg: int, reg_count: int) -> bytes:
    frame = struct.pack("<BB", addr, FC_READ_HOLDING_REQ)
    frame += struct.pack("<HH", start_reg, reg_count)
    return append_crc(frame)


def build_write_single(addr: int, reg: int, value: int) -> bytes:
    frame = struct.pack("<BB", addr, FC_WRITE_SINGLE_REQ)
    frame += struct.pack("<HH", reg, value & 0xFFFF)
    return append_crc(frame)


def build_write_multi(addr: int, start_reg: int, reg_values: List[int]) -> bytes:
    reg_count = len(reg_values)
    byte_count = reg_count * 2
    frame = struct.pack("<BB", addr, FC_WRITE_MULTI_REQ)
    frame += struct.pack("<HH", start_reg, reg_count)
    frame += struct.pack("B", byte_count)
    for val in reg_values:
        frame += struct.pack("<H", val & 0xFFFF)
    return append_crc(frame)


def reg_payload_to_ascii(payload: bytes) -> str:
    """Modbus 寄存器 u16 小端对 → ASCII（每 reg 2 字符）。"""
    chars: List[str] = []
    for i in range(0, len(payload), 2):
        lo = payload[i] if i < len(payload) else 0
        hi = payload[i + 1] if i + 1 < len(payload) else 0
        if lo:
            chars.append(chr(lo))
        if hi:
            chars.append(chr(hi))
    return "".join(chars).rstrip("\x00 ")


def reg_payload_to_utf8(payload: bytes) -> str:
    """寄存器原始字节 → UTF-8 字符串（去尾部 \\0）。"""
    raw = payload.rstrip(b"\x00")
    if not raw:
        return ""
    return raw.decode("utf-8", errors="replace")


def truncate_utf8(text: str, max_bytes: int) -> str:
    raw = text.encode("utf-8")
    if len(raw) <= max_bytes:
        return text
    raw = raw[:max_bytes]
    while raw:
        try:
            return raw.decode("utf-8")
        except UnicodeDecodeError:
            raw = raw[:-1]
    return ""


def utf8_to_reg_values(text: str, reg_count: int) -> List[int]:
    raw = truncate_utf8(text, reg_count * 2).encode("utf-8")
    raw = raw.ljust(reg_count * 2, b"\x00")
    values: List[int] = []
    for i in range(0, reg_count * 2, 2):
        values.append(raw[i] | (raw[i + 1] << 8))
    return values


def ascii_to_reg_values(text: str, reg_count: int) -> List[int]:
    raw = text.encode("ascii", errors="ignore")[: reg_count * 2]
    raw = raw.ljust(reg_count * 2, b"\x00")
    values: List[int] = []
    for i in range(0, reg_count * 2, 2):
        values.append(raw[i] | (raw[i + 1] << 8))
    return values


def reg_payload_to_ipv4(payload: bytes) -> str:
    """reg 6-7 / 138-139：每寄存器 2 字节，小端对为 IP 四段。"""
    if len(payload) < 4:
        return "0.0.0.0"
    return f"{payload[0]}.{payload[1]}.{payload[2]}.{payload[3]}"


def ipv4_to_reg_values(ip_str: str) -> List[int]:
    parts = ip_str.strip().split(".")
    if len(parts) != 4:
        raise ValueError(f"IP 格式无效: {ip_str}")
    octets = [max(0, min(255, int(p))) for p in parts]
    return [octets[0] | (octets[1] << 8), octets[2] | (octets[3] << 8)]


def reg_payload_to_u32(payload: bytes) -> int:
    if len(payload) < 4:
        return 0
    return struct.unpack("<I", payload[:4])[0]


def u32_to_reg_values(value: int) -> List[int]:
    value &= 0xFFFFFFFF
    return [value & 0xFFFF, (value >> 16) & 0xFFFF]


def reg_payload_to_time(payload: bytes) -> str:
    if len(payload) < 6:
        return "—"
    yy, mon, day, hour, minute, second = payload[:6]
    return f"20{yy:02d}-{mon:02d}-{day:02d} {hour:02d}:{minute:02d}:{second:02d}"


def datetime_to_time_reg_values(dt) -> List[int]:
    yy = dt.year % 100
    raw = bytes([yy, dt.month, dt.day, dt.hour, dt.minute, dt.second, 0, 0])
    return [raw[i] | (raw[i + 1] << 8) for i in range(0, 8, 2)]


def build_read_single(addr: int, reg: int, qty: int = 1) -> bytes:
    frame = struct.pack("<BB", addr, FC_READ_SINGLE_REQ)
    frame += struct.pack("<HH", reg, qty)
    return append_crc(frame)


def predict_frame_len(data: bytes) -> int:
    if len(data) < 4:
        return 0
    fc = data[1]
    if fc & 0x80:
        return 8 if len(data) >= 8 else 0
    if fc in (FC_READ_HOLDING_REQ, FC_WRITE_SINGLE_REQ, FC_WRITE_SINGLE_RESP,
              FC_WRITE_MULTI_RESP, FC_READ_SINGLE_REQ, FC_READ_SINGLE_RESP):
        return 8 if len(data) >= 8 else 0
    if fc == FC_WRITE_MULTI_REQ:
        if len(data) < 7:
            return 0
        return 9 + data[6]
    if fc in (FC_READ_HOLDING_RESP, FC_ACTIVE_UPLOAD):
        if len(data) < 3:
            return 0
        return 7 + data[2]
    return 0


def frame_format_ok(frame: bytes) -> bool:
    if len(frame) < 4:
        return False
    pred = predict_frame_len(frame[: max(len(frame), 4)])
    return pred == len(frame)


def parse_frame(frame: bytes) -> Optional[ParsedFrame]:
    if not verify_crc(frame):
        return None
    if not frame_format_ok(frame):
        return None

    addr = frame[0]
    func = frame[1]
    pf = ParsedFrame(addr=addr, func=func, raw=frame)

    if func & 0x80:
        if len(frame) >= 8:
            pf.reg_addr = struct.unpack("<H", frame[2:4])[0]
            pf.error_code = struct.unpack("<H", frame[4:6])[0]
        return pf

    if func in (FC_READ_HOLDING_REQ, FC_READ_SINGLE_REQ):
        pf.reg_addr, pf.reg_qty = struct.unpack("<HH", frame[2:6])
    elif func in (FC_WRITE_SINGLE_REQ, FC_WRITE_SINGLE_RESP, FC_READ_SINGLE_RESP):
        pf.reg_addr, pf.reg_val = struct.unpack("<HH", frame[2:6])
    elif func == FC_WRITE_MULTI_RESP:
        pf.reg_addr, pf.reg_qty = struct.unpack("<HH", frame[2:6])
    elif func in (FC_READ_HOLDING_RESP, FC_ACTIVE_UPLOAD):
        pf.byte_count = frame[2]
        pf.reg_addr = struct.unpack("<H", frame[3:5])[0]
        pf.payload = frame[5 : 5 + pf.byte_count]
    elif func == FC_WRITE_MULTI_REQ and len(frame) >= 7:
        pf.reg_addr, pf.reg_qty = struct.unpack("<HH", frame[2:6])

    return pf


def iter_u32_payload(start_reg: int, payload: bytes) -> Iterator[Tuple[int, int]]:
    """按协议地址步进 2 解析 uint32 小端 payload。"""
    addr = start_reg
    idx = 0
    while idx + 4 <= len(payload):
        val = struct.unpack("<I", payload[idx : idx + 4])[0]
        yield addr, val
        idx += 4
        addr += 2


def format_rt_registers(start_reg: int, payload: bytes) -> List[str]:
    lines: List[str] = []
    for reg_addr, raw in iter_u32_payload(start_reg, payload):
        name_fmt = RT_REGISTER_FMT.get(reg_addr)
        if name_fmt:
            name, fmt = name_fmt
            lines.append(f"  0x{reg_addr:04X} {name}: {fmt(raw)}  (raw={raw})")
        else:
            lines.append(f"  0x{reg_addr:04X}: 0x{raw:08X} ({raw})")
    return lines


def format_read_holding_u16(start_reg: int, payload: bytes) -> List[str]:
    """0x13 应答：每个 Modbus 寄存器 2 字节（NeiJi 当前固件仅低 16 位）。"""
    lines: List[str] = []
    addr = start_reg
    idx = 0
    while idx + 2 <= len(payload):
        val = struct.unpack("<H", payload[idx : idx + 2])[0]
        name_fmt = RT_REGISTER_FMT.get(addr)
        if name_fmt:
            name, fmt = name_fmt
            lines.append(f"  0x{addr:04X} {name}: {fmt(val)}  (raw u16={val})")
        else:
            lines.append(f"  0x{addr:04X}: 0x{val:04X} ({val})")
        idx += 2
        addr += 1
    return lines


def describe_frame(pf: ParsedFrame) -> str:
    fc_name = {
        FC_READ_HOLDING_REQ: "读保持寄存器 请求 0x03",
        FC_READ_HOLDING_RESP: "读保持寄存器 应答 0x13",
        FC_ACTIVE_UPLOAD: "主动上传 0x23",
        FC_READ_SINGLE_REQ: "读单寄存器 请求 0x05",
        FC_WRITE_SINGLE_REQ: "写单寄存器 请求 0x06",
    }.get(pf.func, f"功能码 0x{pf.func:02X}")

    head = f"[{pf.addr:02X}] {fc_name}"
    if pf.func & 0x80:
        return f"{head} 错误 reg=0x{pf.reg_addr:04X} code=0x{pf.error_code:04X}"

    lines = [head]
    if pf.func in (FC_READ_HOLDING_RESP, FC_ACTIVE_UPLOAD) and pf.payload:
        lines.append(
            f"  起始寄存器 0x{pf.reg_addr:04X}, 数据 {pf.byte_count} 字节, hex={pf.payload.hex(' ')}"
        )
        if pf.func == FC_ACTIVE_UPLOAD or pf.byte_count == RT_PAYLOAD_BYTES:
            lines.extend(format_rt_registers(pf.reg_addr, pf.payload))
        else:
            lines.extend(format_read_holding_u16(pf.reg_addr, pf.payload))
    elif pf.func == FC_READ_HOLDING_REQ:
        lines.append(f"  起始 0x{pf.reg_addr:04X}, 数量 {pf.reg_qty}")
    elif pf.func in (FC_WRITE_SINGLE_REQ, FC_WRITE_SINGLE_RESP):
        lines.append(f"  寄存器 0x{pf.reg_addr:04X} = 0x{pf.reg_val:04X}")

    return "\n".join(lines)


class FrameScanner:
    """滑动窗口从字节流中提取协议帧（可与 printf 文本混流）。"""

    def __init__(self, slave_filter: Optional[int] = None) -> None:
        self._buf = bytearray()
        self._slave_filter = slave_filter
        self.ignore_text = False

    def reset(self) -> None:
        self._buf.clear()

    def feed(self, data: bytes) -> Tuple[List[ParsedFrame], str]:
        self._buf.extend(data)
        frames: List[ParsedFrame] = []
        text_out: List[str] = []

        while True:
            if not self._buf:
                break

            if not self.ignore_text:
                nl = self._buf.find(b"\n")
                if nl >= 0:
                    line_b = bytes(self._buf[: nl + 1])
                    try:
                        line = line_b.decode("utf-8", errors="replace").strip("\r\n")
                    except Exception:
                        line = ""
                    if line and self._looks_like_text(line):
                        del self._buf[: nl + 1]
                        text_out.append(line)
                        continue

            if len(self._buf) < 4:
                break

            consumed = self._try_extract_one(frames)
            if consumed == 0:
                del self._buf[0]

        return frames, "\n".join(text_out)

    @staticmethod
    def _looks_like_text(line: str) -> bool:
        if not line:
            return False
        printable = sum(1 for c in line if c.isprintable() or c in "\t")
        return printable >= max(1, len(line) * 3 // 4)

    def _try_extract_one(self, out: List[ParsedFrame]) -> int:
        n = len(self._buf)
        pos = 0
        while pos + 4 <= n:
            while pos < n and self._buf[pos] == 0:
                pos += 1
            if pos + 4 > n:
                if pos > 0:
                    del self._buf[:pos]
                return pos if pos > 0 else 0

            fc = self._buf[pos + 1]
            if fc not in KNOWN_FC:
                pos += 1
                continue

            if self._slave_filter is not None and self._buf[pos] != self._slave_filter:
                pos += 1
                continue

            pred = predict_frame_len(bytes(self._buf[pos:]))
            if pred < 4 or pos + pred > n:
                pos += 1
                continue

            chunk = bytes(self._buf[pos : pos + pred])
            if not verify_crc(chunk) or not frame_format_ok(chunk):
                pos += 1
                continue

            pf = parse_frame(chunk)
            if pf:
                out.append(pf)
            del self._buf[: pos + pred]
            return pos + pred

        if pos > 0:
            del self._buf[:pos]
            return pos
        return 0
