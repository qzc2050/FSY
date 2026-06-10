#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
模拟从机：组播发现 + TCP 服务端（单连接：新连接会关闭旧连接）+ 定时在 TCP 上推送 0x23 主动上传帧。

与安卓配合：
  1) 本脚本周期组播设备信息（236.2.3.6:2468）
  2) 安卓作为 TCP 客户端连接本机 IP:tcp_port
  3) 建连后，本脚本每秒在 TCP 上发送一帧 0x23（模拟下位机主动推）
  4) 收到读/写寄存器请求时返回简化应答（便于联调）

用法:
  python tools/fsy_tcp_slave_simulator.py

可选 --peer：多网卡时指定与安卓同网段的目标 IP，用于选对网卡；省略则用默认路由推断本机 IPv4。
"""

from __future__ import annotations

import argparse
import socket
import struct
import threading
import time
from typing import Callable, List, Optional

# ---------- 协议与端口 ----------
MCAST_GRP = "236.2.3.6"
MCAST_PORT = 2468

MODEL = "FSY-I"
SN = "1905CCM0101"
DEFAULT_TCP_PORT = 5001
PROTO_ADDR = "1"
PROTO_TYPE = "0"
# 文档里数据流端口；组播串里展示用（TCP 模式仍以 tcp_port 为准）
DATA_PORT_STR = "5000"


def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 0x0001:
                crc >>= 1
                crc ^= 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def u32le(v: int) -> bytes:
    return struct.pack("<I", v & 0xFFFFFFFF)


def local_ip_towards(peer_ip: str) -> str:
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect((peer_ip, 9))
        return s.getsockname()[0]
    finally:
        s.close()


def infer_local_ipv4(peer: Optional[str] = None) -> str:
    """未指定 peer 时，用默认路由（向 8.8.8.8 推断）得到本机 IPv4，无需知道安卓 IP。"""
    if peer:
        return local_ip_towards(peer)
    try:
        return local_ip_towards("8.8.8.8")
    except OSError:
        return "127.0.0.1"


DEFAULT_UPLOAD_VALUES: List[int] = [
    0,
    304,
    101759,
    32,
    1540,
    370,
    0,
    0x3BE,
    0,
    0,
    0,
]

# 报警阈值默认值（顺序：辐射上/下、温度上/下、气压上/下、湿度上/下、CO2上/下、PM2.5上/下）
# 单位同实时上传：辐射×100 uSv/h，温度×10 ℃，气压 Pa，湿度 %，CO2×10 ppm，PM2.5×10 ug/m3
DEFAULT_THRESHOLDS: List[int] = [
    10000,  0,       # 辐射  上限 100.00 uSv/h / 下限 0.00 uSv/h
    600,    0,       # 温度  上限 60.0 ℃      / 下限 0.0 ℃
    110000, 90000,   # 气压  上限 110000 Pa    / 下限 90000 Pa
    90,     10,      # 湿度  上限 90 %         / 下限 10 %
    10000,  0,       # CO2   上限 1000.0 ppm   / 下限 0.0 ppm
    1500,   0,       # PM2.5 上限 150.0 ug/m3  / 下限 0.0 ug/m3
]

# 报警使能默认：bit0/bit1=0 表示辐射上下限报警均启用（与安卓 UI 一致）
DEFAULT_ALARM_ENABLE = 0
DEFAULT_VOLUME = 50  # 寄存器 0x7A，0~100

# 寄存器地址定义
REG_REALTIME    = 0x0001  # 实时上传数据（11×u32）
REG_STATUS_BIT  = 0x000F  # 状态位图（2 regs = 1×u32）
REG_FIVE_MIN    = 0x001E  # 五分钟值（表地址 30）
REG_DEVICE_TIME = 0x0020  # 设备 RTC 时间（4 regs = 8 bytes）
REG_RAD_UPPER   = 0x0032  # 辐射报警上阈值（2 regs = 1×u32）
REG_RAD_LOWER   = 0x0034  # 辐射报警下阈值（2 regs = 1×u32）
REG_THRESHOLDS  = 0x0040  # 报警阈值（24 regs = 48 bytes = 12×u32）
REG_ALARM_ENABLE = 0x0052  # 报警使能（2 regs = 1×u32，bit=1 禁止）
REG_SERIALNUM   = 0x0056  # 序列号（8 regs = 16 bytes ASCII）
REG_VERSION     = 0x0062  # 固件版本字符串（10 regs = 20 bytes）
REG_CONTROL_VOL  = 0x007A  # controlbit1 音量（1 reg = uint16 0~100）
REG_CONTROL_BIT2 = 0x007B # controlbit2（2 regs = 1×u32）

CONTROL_BIT2_MASK = 0x7FFE  # bit1..bit14，可控制；bit0 门状态不允许控制

# 报警使能 0x52：bit=1 表示禁止该项报警
_ALARM_ENABLE_BIT_NAMES = (
    "辐射上阈值", "辐射下阈值", "辐射离线",
    "保留3", "温度上阈值", "温度下阈值", "温度离线",
    "保留7", "气压上阈值", "气压下阈值", "气压离线",
    "保留11", "湿度上阈值", "湿度下阈值", "湿度离线",
    "保留15", "CO2上阈值", "CO2下阈值", "CO2离线",
    "保留19", "PM2.5上阈值", "PM2.5下阈值", "PM2.5离线",
    "保留23", "声报警损坏", "保留25", "声报警离线",
    "保留27", "光报警损坏", "保留29", "光报警离线",
    "保留31",
)

_CTRL_REG_NAMES = {
    0x0058: "声报警",
    0x0059: "光报警",
    0x005A: "风扇",
}

_REG_READ_NAMES = {
    REG_REALTIME: "实时数据(0x0001)",
    REG_STATUS_BIT: "设备状态位(0x000F)",
    REG_DEVICE_TIME: "设备时间(0x0020)",
    REG_RAD_UPPER: "辐射报警上阈值(0x0032)",
    REG_RAD_LOWER: "辐射报警下阈值(0x0034)",
    REG_THRESHOLDS: "报警阈值块(0x0040)",
    REG_ALARM_ENABLE: "报警使能(0x0052)",
    REG_SERIALNUM: "序列号(0x0056)",
    REG_VERSION: "固件版本(0x0062)",
    REG_CONTROL_VOL: "报警音量(0x007A)",
    REG_CONTROL_BIT2: "controlbit2(0x007B)",
}


def _u32_list_from_bytes(data: bytes, n: int) -> List[int]:
    return [struct.unpack_from("<I", data, i * 4)[0] for i in range(n) if i * 4 + 4 <= len(data)]


# 报警阈值 0x40 起 12 项 u32（每项占 2 个 16 位寄存器）
_THR_ITEM_FMT: List[tuple] = [
    ("辐射上限", 100.0, "uSv/h", 2),
    ("辐射下限", 100.0, "uSv/h", 2),
    ("温度上限", 10.0, "℃", 1),
    ("温度下限", 10.0, "℃", 1),
    ("气压上限", 1.0, "Pa", 0),
    ("气压下限", 1.0, "Pa", 0),
    ("湿度上限", 1.0, "%", 0),
    ("湿度下限", 1.0, "%", 0),
    ("CO2上限", 10.0, "ppm", 1),
    ("CO2下限", 10.0, "ppm", 1),
    ("PM2.5上限", 10.0, "ug/m3", 1),
    ("PM2.5下限", 10.0, "ug/m3", 1),
]


def _format_thr_item(idx: int, raw: int) -> str:
    if idx < 0 or idx >= len(_THR_ITEM_FMT):
        return f"[{idx}]={raw}"
    name, scale, unit, prec = _THR_ITEM_FMT[idx]
    val = raw / scale
    if prec == 0:
        sval = str(int(val))
    elif prec == 1:
        sval = f"{val:.1f}"
    else:
        sval = f"{val:.2f}"
    return f"{name} {sval} {unit}"


def _format_thr_values(values: List[int], base_index: int = 0) -> str:
    return ", ".join(_format_thr_item(base_index + i, v) for i, v in enumerate(values))


def _format_thresholds_brief(thr: List[int]) -> str:
    if not thr:
        return "(空)"
    if len(thr) >= 12:
        return _format_thr_values(thr[:12], 0)
    return _format_thr_values(thr, 0)


def _format_controlbit2(v: int) -> str:
    v &= 0xFFFFFFFF
    hints: List[str] = []
    hints.append("门关" if (v & 1) else "门开")
    if (v >> 7) & 1:
        hints.append("风扇开")
    if (v >> 8) & 1:
        hints.append("USB4程序口")
    if (v >> 12) & 1:
        hints.append("声报警启用")
    if (v >> 13) & 1:
        hints.append("光报警启用")
    if (v >> 14) & 1:
        hints.append("屏幕启用")
    return f"0x{v:08X} ({', '.join(hints)})"


def _format_alarm_enable_mask(mask: int) -> str:
    mask &= 0xFFFFFFFF
    if mask == 0:
        return "全部启用(bit全0)"
    disabled = [
        name
        for bit, name in enumerate(_ALARM_ENABLE_BIT_NAMES)
        if bit < 32 and (mask >> bit) & 1 and not name.startswith("保留")
    ]
    return f"禁止: {', '.join(disabled) if disabled else '无'} (raw=0x{mask:08X})"


def _reg_operation_name(start: int) -> str:
    return _REG_READ_NAMES.get(start, f"寄存器0x{start:04X}")


def describe_host_request(req: bytes) -> str:
    """解析上位机发来的读/写请求，生成可读说明。"""
    if len(req) < 6:
        return f"请求过短 len={len(req)} hex={req.hex(' ')}"
    fc = req[1]
    start = struct.unpack_from("<H", req, 2)[0]
    op = _reg_operation_name(start)

    if fc == 0x03:
        count = struct.unpack_from("<H", req, 4)[0] if len(req) >= 6 else 0
        extra = ""
        if start == REG_THRESHOLDS and count > 0:
            extra = f"，约 {count // 2} 项阈值"
        return f"读 {op} count={count}{extra} | {req.hex(' ').upper()}"

    if fc == 0x06 and len(req) >= 6:
        val = struct.unpack_from("<H", req, 4)[0]
        if start == REG_CONTROL_VOL:
            return f"写 报警音量(0x7A)={val} | {req.hex(' ').upper()}"
        ctrl = _CTRL_REG_NAMES.get(start)
        if ctrl:
            return f"写 {ctrl} {'开' if val else '关'} (reg=0x{start:04X}) | {req.hex(' ').upper()}"
        return f"写单寄存器 {op} val=0x{val:04X} | {req.hex(' ').upper()}"

    if fc == 0x10 and len(req) >= 7:
        count = struct.unpack_from("<H", req, 4)[0]
        bc = req[6]
        data = req[7:7 + bc] if len(req) >= 7 + bc else b""

        if start == REG_DEVICE_TIME and bc >= 6:
            y, mo, d, h, mi, s = data[0], data[1], data[2], data[3], data[4], data[5]
            return (
                f"写 设备时间 → 20{y:02d}-{mo:02d}-{d:02d} {h:02d}:{mi:02d}:{s:02d} | "
                f"{req.hex(' ').upper()}"
            )

        if start in (REG_RAD_UPPER, REG_RAD_LOWER) and bc >= 4:
            v = struct.unpack_from("<I", data, 0)[0]
            which = "上" if start == REG_RAD_UPPER else "下"
            return f"写 辐射报警{which}限 = {v / 100:.2f} uSv/h (raw={v}) | {req.hex(' ').upper()}"

        if start == REG_THRESHOLDS and bc >= 4:
            n = min(bc // 4, 12)
            thr = _u32_list_from_bytes(data, n)
            return f"写 报警阈值(0x40) {n}项: {_format_thresholds_brief(thr)} | {req.hex(' ').upper()}"

        if start == REG_ALARM_ENABLE and bc >= 4:
            v = struct.unpack_from("<I", data, 0)[0]
            return f"写 报警使能(0x52) {_format_alarm_enable_mask(v)} | {req.hex(' ').upper()}"

        if start == REG_SERIALNUM and bc >= 1:
            serial = data[: min(16, bc)].decode("ascii", errors="ignore").rstrip("\x00").strip()
            return f"写 序列号 = {serial!r} | {req.hex(' ').upper()}"

        if start == REG_CONTROL_BIT2 and bc >= 4:
            v = struct.unpack_from("<I", data, 0)[0]
            return f"写 controlbit2(0x7B) raw=0x{v:08X} | {req.hex(' ').upper()}"

        return f"写多寄存器 {op} count={count} bc={bc} | {req.hex(' ').upper()}"

    return f"未知功能码 0x{fc:02X} | {req.hex(' ').upper()}"


def describe_host_response(req: bytes, rsp: bytes) -> str:
    """解析对上位机的应答（侧重读 0x13 返回的数据含义）。"""
    if len(req) < 4 or len(rsp) < 7:
        return rsp.hex(" ").upper()
    fc_req = req[1]
    start = struct.unpack_from("<H", req, 2)[0]

    if fc_req == 0x03 and len(rsp) >= 7 and rsp[1] == 0x13:
        bc = rsp[2]
        payload = rsp[5:5 + bc] if len(rsp) >= 5 + bc else b""
        reg_count = struct.unpack_from("<H", req, 4)[0] if len(req) >= 6 else 0

        if start == REG_THRESHOLDS and len(payload) >= 4:
            n_u32 = min(len(payload) // 4, max(1, reg_count // 2))
            thr = _u32_list_from_bytes(payload, n_u32)
            return f"读应答 阈值({n_u32}项): {_format_thr_values(thr, 0)}"

        if start == REG_ALARM_ENABLE and len(payload) >= 4:
            v = struct.unpack_from("<I", payload, 0)[0]
            return f"读应答 报警使能: {_format_alarm_enable_mask(v)}"

        if start in (REG_RAD_UPPER, REG_RAD_LOWER) and len(payload) >= 4:
            v = struct.unpack_from("<I", payload, 0)[0]
            which = "上" if start == REG_RAD_UPPER else "下"
            return f"读应答 辐射{which}限 = {v / 100:.2f} uSv/h"

        if start == REG_VERSION and payload:
            ver = payload.decode("ascii", errors="replace").rstrip("\x00").strip()
            return f"读应答 固件版本: {ver!r}"

        if start == REG_SERIALNUM and payload:
            serial = payload.decode("ascii", errors="replace").rstrip("\x00").strip()
            return f"读应答 序列号: {serial!r}"

        if start == REG_DEVICE_TIME and len(payload) >= 6:
            y, mo, d, h, mi, s = payload[0], payload[1], payload[2], payload[3], payload[4], payload[5]
            return f"读应答 设备时间: 20{y:02d}-{mo:02d}-{d:02d} {h:02d}:{mi:02d}:{s:02d}"

        if start == REG_STATUS_BIT and len(payload) >= 4:
            v = struct.unpack_from("<I", payload, 0)[0]
            return f"读应答 状态位 status_bit=0x{v:08X}"

        if start == REG_CONTROL_VOL and len(payload) >= 2:
            v = struct.unpack_from("<H", payload, 0)[0]
            return f"读应答 音量 = {v}"

        if start == REG_CONTROL_BIT2 and len(payload) >= 4:
            v = struct.unpack_from("<I", payload, 0)[0]
            return f"读应答 controlbit2 {_format_controlbit2(v)}"

        return f"读应答 {_reg_operation_name(start)} {len(payload)}B data={payload.hex(' ')}"

    if fc_req in (0x06, 0x10) and len(rsp) >= 6:
        fc_rsp = rsp[1]
        if fc_rsp in (0x16, 0x20):
            rstart = struct.unpack_from("<H", rsp, 2)[0] if len(rsp) >= 4 else 0
            return f"写应答 OK reg=0x{rstart:04X}"

    return rsp.hex(" ").upper()


def build_upload_frame(device_addr: int, values: Optional[List[int]] = None) -> bytes:
    """0x23 主动上传（与调试示例同结构，地址用 device_addr）。"""
    func = 0x23
    byte_count = 0x2C
    start_reg = 0x0001
    vals = list(values) if values is not None else list(DEFAULT_UPLOAD_VALUES)
    if len(vals) != 11:
        raise ValueError(f"upload values 必须是 11 项，当前 {len(vals)}")
    payload = b"".join(u32le(v) for v in vals)
    head = bytes([device_addr, func, byte_count]) + struct.pack("<H", start_reg) + payload
    crc = crc16_modbus(head)
    return head + struct.pack("<H", crc)


def build_five_minute_upload_frame(device_addr: int, dose_rate_x100: int, ts: Optional[time.struct_time] = None) -> bytes:
    """
    0x23 五分钟值（主动上传 / 历史应答）：
    地址|0x23|0x0C|起始寄存器(0x001E, LE)|data_time[8]|dose_rate(uint32, *100)|CRC
    """
    func = 0x23
    byte_count = 0x0C
    start_reg = REG_FIVE_MIN
    t = ts or time.localtime()
    # 文档约定 data_time[0:6] = 年月日时分秒，后两字节预留
    # 年按 0-99 存储（如 2026 -> 26）
    data_time = bytes(
        [
            t.tm_year % 100,
            t.tm_mon & 0xFF,
            t.tm_mday & 0xFF,
            t.tm_hour & 0xFF,
            t.tm_min & 0xFF,
            t.tm_sec & 0xFF,
            0x00,
            0x00,
        ]
    )
    payload = data_time + u32le(max(0, int(dose_rate_x100)))
    head = bytes([device_addr, func, byte_count]) + struct.pack("<H", start_reg) + payload
    crc = crc16_modbus(head)
    return head + struct.pack("<H", crc)


def _crc_ok(frame: bytes) -> bool:
    if len(frame) < 4:
        return False
    body, crc_le = frame[:-2], frame[-2:]
    expect = crc16_modbus(body)
    got = crc_le[0] | (crc_le[1] << 8)
    return expect == got


def _try_take_one_request(buf: bytearray, device_addr: int) -> Optional[bytes]:
    """从缓冲区取走第一帧合法请求（仅处理常见长度）。"""
    if len(buf) < 8:
        return None
    # 对齐到本机地址
    if buf[0] != device_addr:
        # 丢弃首字节重新对齐（抗少量噪声）
        del buf[0]
        return None
    fc = buf[1]
    if fc == 0x03:
        cand = bytes(buf[:8])
        if _crc_ok(cand):
            del buf[:8]
            return cand
        return None
    if fc == 0x06:
        cand = bytes(buf[:8])
        if _crc_ok(cand):
            del buf[:8]
            return cand
        return None
    if fc == 0x10 and len(buf) >= 9:
        bc = buf[6]
        # 0x10 请求总长 = addr(1)+func(1)+start(2)+count(2)+byteCount(1)+data(bc)+crc(2) = 9 + bc
        total = 9 + bc
        if len(buf) < total:
            return None
        cand = bytes(buf[:total])
        if _crc_ok(cand):
            del buf[:total]
            return cand
        return None
    # 无法识别则丢一字节防死锁
    del buf[0]
    return None


def _reply_read_holding_static(device_addr: int, req: bytes) -> bytes:
    """仅供外部无上下文时调用，不含阈值/时间寄存器。"""
    # 与 zjb protocol.c / 安卓 buildReadRegsFrame 一致：寄存器与数量为线路上小端（先低字节）
    start = struct.unpack_from("<H", req, 2)[0]
    count = struct.unpack_from("<H", req, 4)[0]
    reg_bytes = count * 2
    data = bytearray()
    data.append(device_addr)
    data.append(0x13)
    data.append(reg_bytes & 0xFF)
    data.extend(struct.pack("<H", start))
    if start == REG_VERSION and count >= 0x0A:
        ver = b"V1.0.0.SIM\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        data.extend(ver[:reg_bytes])
    else:
        data.extend([0x00] * reg_bytes)
    crc = crc16_modbus(bytes(data))
    data.extend(struct.pack("<H", crc))
    return bytes(data)


def reply_write_single(device_addr: int, req: bytes) -> bytes:
    """0x06 -> 0x16 回显。"""
    body = req[:-2]
    out = bytearray()
    out.append(device_addr)
    out.append(0x16)
    out.extend(body[2:6])
    crc = crc16_modbus(bytes(out))
    out.extend(struct.pack("<H", crc))
    return bytes(out)


def reply_write_multi(device_addr: int, req: bytes) -> bytes:
    """0x10 -> 0x20。"""
    body = req[:-2]
    out = bytearray()
    out.append(device_addr)
    out.append(0x20)
    out.extend(body[2:6])
    crc = crc16_modbus(bytes(out))
    out.extend(struct.pack("<H", crc))
    return bytes(out)


def _subnet_broadcast_addr(local_ip: str) -> Optional[str]:
    try:
        parts = local_ip.strip().split(".")
        if len(parts) == 4:
            return f"{parts[0]}.{parts[1]}.{parts[2]}.255"
    except Exception:
        pass
    return None


def multicast_loop(
    local_ip: str,
    tcp_port: int,
    interval: float,
    stop_event: threading.Event,
    log: Optional[Callable[[str], None]] = None,
    discovery_unicast_to: Optional[str] = None,
) -> None:
    _log: Callable[[str], None] = log or print
    mcast_logged = False
    bcast_logged = False
    unicast_logged = False
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 1)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    except OSError:
        pass
    try:
        while not stop_event.is_set():
            msg = f"{MODEL},{SN},{local_ip},{tcp_port},{DATA_PORT_STR},{PROTO_ADDR},{PROTO_TYPE},0"
            payload = msg.encode("ascii")
            sock.sendto(payload, (MCAST_GRP, MCAST_PORT))
            if not mcast_logged:
                _log(f"[MCAST] {msg}")
                mcast_logged = True
            bcast = _subnet_broadcast_addr(local_ip)
            if bcast:
                try:
                    sock.sendto(payload, (bcast, MCAST_PORT))
                    if not bcast_logged:
                        _log(f"[BCAST] {bcast}:{MCAST_PORT} {msg}")
                        bcast_logged = True
                except OSError as e:
                    _log(f"[BCAST] fail: {e}")
            if discovery_unicast_to:
                try:
                    sock.sendto(payload, (discovery_unicast_to.strip(), MCAST_PORT))
                    if not unicast_logged:
                        _log(f"[UNICAST] {discovery_unicast_to}:{MCAST_PORT} {msg}")
                        unicast_logged = True
                except OSError as e:
                    _log(f"[UNICAST] fail: {e}")
            if stop_event.wait(timeout=interval):
                break
    finally:
        try:
            sock.close()
        except OSError:
            pass


class TcpSlave:
    def __init__(
        self,
        device_addr: int,
        tcp_port: int,
        push_interval: float,
        peer_for_local_ip: Optional[str],
        mcast_interval: float = 1.0,
        log: Optional[Callable[[str], None]] = None,
        discovery_unicast_to: Optional[str] = None,
        upload_values: Optional[List[int]] = None,
        upload_values_provider: Optional[Callable[[], List[int]]] = None,
        firmware_version: str = "V1.0.0.SIM",
        serial_number: str = SN,
        thresholds_updated_callback: Optional[Callable[[List[int]], None]] = None,
        status_updated_callback: Optional[Callable[[int, int], None]] = None,
        serial_updated_callback: Optional[Callable[[str], None]] = None,
        alarm_enable_updated_callback: Optional[Callable[[int], None]] = None,
        volume_updated_callback: Optional[Callable[[int], None]] = None,
        alarm_enable: int = DEFAULT_ALARM_ENABLE,
        volume: int = DEFAULT_VOLUME,
    ) -> None:
        self.device_addr = device_addr
        self.tcp_port = tcp_port
        self.push_interval = push_interval
        self.mcast_interval = mcast_interval
        self.discovery_unicast_to = discovery_unicast_to
        self._log: Callable[[str], None] = log or print
        self.local_ip = infer_local_ipv4(peer_for_local_ip)
        self.firmware_version: str = firmware_version
        self.serial_number: str = serial_number
        self._lock = threading.Lock()       # 保护 _client / _upload_values / _thresholds
        self._write_lock = threading.Lock() # 保护同一 socket 的并发写（push_loop vs handle_client）
        self._client: Optional[socket.socket] = None
        self._srv: Optional[socket.socket] = None
        self._stop = threading.Event()
        self._upload_values: List[int] = list(upload_values) if upload_values else list(DEFAULT_UPLOAD_VALUES)
        # 可选：由 GUI 提供实时值。若提供，则每次 0x23 发送前都会重新读取。
        self._upload_values_provider = upload_values_provider
        self._thresholds: List[int] = list(DEFAULT_THRESHOLDS)
        self._alarm_enable: int = int(alarm_enable) & 0xFFFFFFFF
        self._volume: int = max(0, min(100, int(volume)))
        self._thresholds_updated_callback = thresholds_updated_callback
        self._status_updated_callback = status_updated_callback
        self._serial_updated_callback = serial_updated_callback
        self._alarm_enable_updated_callback = alarm_enable_updated_callback
        self._volume_updated_callback = volume_updated_callback
        self._status_bit: int = self._upload_values[7] if len(self._upload_values) > 7 else 0
        self._control_bit2: int = self._status_bit & CONTROL_BIT2_MASK
        self._push_send_count = 0  # 0x23 推送计数，用于降低日志频率
        # 存储被写入的设备时间（year%100, month, day, hour, min, sec），None 表示用实时时钟
        self._device_time: Optional[tuple] = None

    def stop(self) -> None:
        self._stop.set()
        self._log("[TCP] 停止：关闭监听套接字与当前客户端连接")
        if self._srv is not None:
            try:
                self._srv.close()
            except OSError:
                pass
            self._srv = None
        with self._lock:
            self._close_locked()

    def _close_locked(self) -> None:
        if self._client is not None:
            try:
                self._client.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            try:
                self._client.close()
            except OSError:
                pass
            self._client = None

    def replace_client(self, conn: socket.socket) -> None:
        with self._lock:
            self._close_locked()
            self._client = conn
            self._log(f"[TCP] 新连接: {conn.getpeername()}（已关闭旧连接）")

    def set_upload_values(self, values: List[int]) -> None:
        if len(values) != 11:
            raise ValueError(f"upload values 必须是 11 项，当前 {len(values)}")
        with self._lock:
            self._upload_values = list(values)
            self._status_bit = values[7]
            self._control_bit2 = self._status_bit & CONTROL_BIT2_MASK
        self._log(f"[TCP PUSH CFG] 已更新 0x23 数据项: {values}")

    def set_thresholds(self, values: List[int]) -> None:
        if len(values) != 12:
            raise ValueError(f"thresholds 必须是 12 项，当前 {len(values)}")
        with self._lock:
            self._thresholds = list(values)
        self._emit_thresholds_updated(values)
        self._log(f"[THR CFG] 已更新报警阈值: {values}")

    def get_thresholds(self) -> List[int]:
        with self._lock:
            return list(self._thresholds)

    def set_alarm_enable(self, value: int) -> None:
        with self._lock:
            self._alarm_enable = int(value) & 0xFFFFFFFF
        self._emit_alarm_enable_updated(self._alarm_enable)

    def get_alarm_enable(self) -> int:
        with self._lock:
            return self._alarm_enable

    def set_volume(self, value: int) -> None:
        v = max(0, min(100, int(value)))
        with self._lock:
            self._volume = v
        self._emit_volume_updated(v)
        self._log(f"[CFG] 音量 0x7A = {v}")

    def get_volume(self) -> int:
        with self._lock:
            return self._volume

    def _emit_thresholds_updated(self, values: List[int]) -> None:
        cb = self._thresholds_updated_callback
        if cb is None:
            return
        try:
            cb(list(values))
        except Exception as e:
            self._log(f"[THR CALLBACK] 回调异常: {e}")

    def _emit_alarm_enable_updated(self, value: int) -> None:
        cb = self._alarm_enable_updated_callback
        if cb is None:
            return
        try:
            cb(int(value) & 0xFFFFFFFF)
        except Exception as e:
            self._log(f"[CFG CALLBACK] 报警使能回调异常: {e}")

    def _emit_volume_updated(self, value: int) -> None:
        cb = self._volume_updated_callback
        if cb is None:
            return
        try:
            cb(max(0, min(100, int(value))))
        except Exception as e:
            self._log(f"[CFG CALLBACK] 音量回调异常: {e}")

    def _emit_status_updated(self, status_bit: int, control_bit2: int) -> None:
        cb = self._status_updated_callback
        if cb is None:
            return
        try:
            cb(status_bit, control_bit2)
        except Exception as e:
            self._log(f"[STATUS CALLBACK] 回调异常: {e}")

    def _emit_serial_updated(self, serial_number: str) -> None:
        cb = self._serial_updated_callback
        if cb is None:
            return
        try:
            cb(serial_number)
        except Exception as e:
            self._log(f"[SERIAL CALLBACK] 回调异常: {e}")

    def _set_status_and_control(self, status_bit: int, control_bit2: Optional[int] = None) -> None:
        status_bit &= 0xFFFFFFFF
        if control_bit2 is None:
            control_bit2 = status_bit & CONTROL_BIT2_MASK
        control_bit2 &= 0xFFFFFFFF
        with self._lock:
            self._status_bit = status_bit
            self._control_bit2 = control_bit2
            if len(self._upload_values) >= 8:
                self._upload_values[7] = status_bit
        self._emit_status_updated(status_bit, control_bit2)

    def _apply_control_bit2(self, raw_value: int) -> tuple[int, int]:
        raw_value &= 0xFFFFFFFF
        with self._lock:
            current_status = self._status_bit
        effective_status = (current_status & ~CONTROL_BIT2_MASK) | (raw_value & CONTROL_BIT2_MASK)
        effective_control = (raw_value & CONTROL_BIT2_MASK) | (effective_status & 0x0001)
        self._set_status_and_control(effective_status, effective_control)
        return effective_status, effective_control

    def _build_read_reply(self, req: bytes) -> bytes:
        """0x03 → 0x13：根据起始寄存器分区返回数据。"""
        # 请求中 reg/count 为小端，与 STM32 固件、安卓一致（非标准 Modbus 大端 PDU）
        start = struct.unpack_from("<H", req, 2)[0]
        count = struct.unpack_from("<H", req, 4)[0]
        reg_bytes = count * 2
        data = bytearray()
        data.append(self.device_addr)
        data.append(0x13)
        data.append(reg_bytes & 0xFF)
        data.extend(struct.pack("<H", start))

        if start == REG_VERSION and count >= 0x0A:
            # 固件版本字符串（20 bytes ASCII），右侧补 0x00 到 20 字节
            ver = self.firmware_version.encode("ascii", errors="replace")[:20].ljust(20, b"\x00")
            data.extend(ver[:reg_bytes])

        elif start == REG_DEVICE_TIME:
            # 设备 RTC 时间：[year%100, month, day, hour, min, sec, 0, 0]
            # 若曾被同步则返回存储值，否则返回本机实时时钟
            if self._device_time is not None:
                y, mo, d, h, mi, s = self._device_time
            else:
                t = time.localtime()
                y, mo, d, h, mi, s = t.tm_year % 100, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec
            time_bytes = bytes([y, mo, d, h, mi, s, 0, 0])
            payload = (time_bytes + bytes(max(0, reg_bytes - 8)))[:reg_bytes]
            data.extend(payload)

        elif start == REG_THRESHOLDS:
            # 12 个 u32 报警阈值（48 bytes）
            with self._lock:
                thr = list(self._thresholds)
            payload = b"".join(u32le(v) for v in thr)
            extra = bytes(max(0, reg_bytes - len(payload)))
            data.extend((payload + extra)[:reg_bytes])

        elif start == REG_SERIALNUM and count >= 0x08:
            serial = self.serial_number.encode("ascii", errors="replace")[:16].ljust(16, b"\x00")
            data.extend(serial[:reg_bytes])

        elif start == REG_STATUS_BIT:
            with self._lock:
                status_bit = self._status_bit
            payload = u32le(status_bit)
            data.extend(payload[:reg_bytes].ljust(reg_bytes, b"\x00"))

        elif start == REG_ALARM_ENABLE:
            with self._lock:
                enable = self._alarm_enable
            payload = u32le(enable)
            data.extend(payload[:reg_bytes].ljust(reg_bytes, b"\x00"))

        elif start == REG_CONTROL_VOL:
            with self._lock:
                vol = self._volume
            data.extend(struct.pack("<H", vol & 0xFFFF)[:reg_bytes].ljust(reg_bytes, b"\x00"))

        elif start == REG_CONTROL_BIT2:
            with self._lock:
                control_bit2 = self._control_bit2
            payload = u32le(control_bit2)
            data.extend(payload[:reg_bytes].ljust(reg_bytes, b"\x00"))

        else:
            data.extend([0x00] * reg_bytes)

        crc = crc16_modbus(bytes(data))
        data.extend(struct.pack("<H", crc))
        return bytes(data)

    def _handle_write_multi(self, req: bytes) -> None:
        """处理 0x10 写多寄存器请求：按起始寄存器更新内部状态。"""
        if len(req) < 9:
            return
        start = struct.unpack_from("<H", req, 2)[0]
        bc = req[6]
        if len(req) < 7 + bc:
            return
        data = req[7:7 + bc]

        if start == REG_DEVICE_TIME and bc >= 8:
            y, mo, d, h, mi, s = data[0], data[1], data[2], data[3], data[4], data[5]
            self._device_time = (y, mo, d, h, mi, s)

        elif start in (REG_RAD_UPPER, REG_RAD_LOWER) and bc >= 4:
            value = struct.unpack_from("<I", data, 0)[0]
            idx = 0 if start == REG_RAD_UPPER else 1
            with self._lock:
                thr = list(self._thresholds)
                thr[idx] = value
                self._thresholds = thr
            self._emit_thresholds_updated(thr)

        elif start == REG_THRESHOLDS and bc >= 4:
            n = min(bc // 4, 12)
            with self._lock:
                thr = list(self._thresholds)
            for i in range(n):
                thr[i] = struct.unpack_from("<I", data, i * 4)[0]
            with self._lock:
                self._thresholds = thr
            self._emit_thresholds_updated(thr)

        elif start == REG_ALARM_ENABLE and bc >= 4:
            enable = struct.unpack_from("<I", data, 0)[0]
            self.set_alarm_enable(enable)

        elif start == REG_SERIALNUM and bc >= 16:
            serial = data[:16].decode("ascii", errors="ignore").rstrip("\x00").strip()
            self.serial_number = serial
            self._emit_serial_updated(self.serial_number)

        elif start == REG_CONTROL_BIT2 and bc >= 4:
            raw_value = struct.unpack_from("<I", data, 0)[0]
            self._apply_control_bit2(raw_value)

    def _handle_write_single(self, req: bytes) -> None:
        """处理 0x06 写单寄存器请求。"""
        if len(req) < 8:
            return
        reg = struct.unpack_from("<H", req, 2)[0]
        val = struct.unpack_from("<H", req, 4)[0]
        if reg == REG_CONTROL_VOL:
            self.set_volume(val)
            return
        if _CTRL_REG_NAMES.get(reg):
            return

    def push_five_minute_value(self, dose_rate_x100: int) -> bool:
        """
        手动触发一次五分钟值主动上传（0x23 / 0x001E）。
        返回是否发送成功（仅代表已写入 socket）。
        """
        with self._lock:
            c = self._client
        if c is None:
            self._log("[TCP PUSH 5MIN] 跳过：当前无客户端连接")
            return False

        frame = build_five_minute_upload_frame(self.device_addr, dose_rate_x100)
        try:
            with self._write_lock:
                c.sendall(frame)
            self._log(f"[TCP PUSH 5MIN 0x23] {frame.hex(' ').upper()}")
            return True
        except OSError as e:
            self._log(f"[TCP PUSH 5MIN 失败] {e}")
            with self._lock:
                self._close_locked()
            return False

    def push_loop(self) -> None:
        while not self._stop.is_set():
            time.sleep(self.push_interval)
            with self._lock:
                c = self._client
                vals = list(self._upload_values)
                provider = self._upload_values_provider
            if provider is not None:
                try:
                    dyn = provider()
                    if len(dyn) == 11:
                        vals = list(dyn)
                        with self._lock:
                            self._upload_values = list(vals)
                            self._status_bit = vals[7]
                            self._control_bit2 = self._status_bit & CONTROL_BIT2_MASK
                    else:
                        self._log(f"[TCP PUSH CFG] 忽略非法实时值项数: {len(dyn)}")
                except Exception as e:
                    self._log(f"[TCP PUSH CFG] 读取实时值失败，沿用上次值: {e}")
            frame = build_upload_frame(self.device_addr, vals)
            if c is None:
                continue
            try:
                with self._write_lock:
                    c.sendall(frame)
                self._push_send_count += 1
                if self._push_send_count % 10 == 0:
                    self._log(
                        f"[TCP PUSH 0x23] 第{self._push_send_count}次 "
                        f"{frame.hex(' ').upper()}"
                    )
            except OSError as e:
                self._log(f"[TCP PUSH 失败] {e}")
                with self._lock:
                    self._close_locked()

    def handle_client(self, conn: socket.socket) -> None:
        buf = bytearray()
        try:
            while not self._stop.is_set():
                chunk = conn.recv(4096)
                if not chunk:
                    self._log("[TCP] 对端关闭")
                    break
                buf.extend(chunk)
                while True:
                    req = _try_take_one_request(buf, self.device_addr)
                    if req is None:
                        break
                    rsp: Optional[bytes] = None
                    fc = req[1]
                    if fc == 0x03:
                        rsp = self._build_read_reply(req)
                    elif fc == 0x06:
                        rsp = reply_write_single(self.device_addr, req)
                        self._handle_write_single(req)
                    elif fc == 0x10:
                        rsp = reply_write_multi(self.device_addr, req)
                        self._handle_write_multi(req)
                    else:
                        self._log(
                            f"[TCP 上位机] 未处理功能码 0x{fc:02X} | {describe_host_request(req)}"
                        )
                        continue
                    if rsp:
                        with self._write_lock:
                            conn.sendall(rsp)
                        self._log(f"[TCP 上位机←] {describe_host_request(req)}")
                        rsp_desc = describe_host_response(req, rsp)
                        if rsp_desc:
                            self._log(f"[TCP 下位机→] {rsp_desc}")
        except Exception as e:
            self._log(f"[TCP] 会话异常: {type(e).__name__}: {e}")
        finally:
            with self._lock:
                if self._client is conn:
                    self._close_locked()
            try:
                conn.close()
            except OSError:
                pass

    def accept_loop(self) -> None:
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind(("0.0.0.0", self.tcp_port))
        srv.listen(4)
        self._srv = srv
        self._log(f"[TCP] 监听 0.0.0.0:{self.tcp_port}，本机 IP（组播用）: {self.local_ip}")
        while not self._stop.is_set():
            try:
                srv.settimeout(0.5)
                try:
                    conn, addr = srv.accept()
                except socket.timeout:
                    continue
            except OSError:
                break
            conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            self.replace_client(conn)
            t = threading.Thread(target=self.handle_client, args=(conn,), daemon=True)
            t.start()
        try:
            srv.close()
        except OSError:
            pass
        self._srv = None

    def run(self) -> None:
        t_mcast = threading.Thread(
            target=multicast_loop,
            args=(
                self.local_ip,
                self.tcp_port,
                self.mcast_interval,
                self._stop,
                self._log,
                self.discovery_unicast_to,
            ),
            daemon=True,
        )
        t_push = threading.Thread(target=self.push_loop, daemon=True)
        t_mcast.start()
        t_push.start()
        self.accept_loop()


def main() -> None:
    ap = argparse.ArgumentParser(description="FSY 模拟从机：组播 + TCP（单连接）")
    ap.add_argument(
        "--peer",
        default=None,
        help="可选：指定目标 IP（如安卓 IP）以在多网卡下选择与该局域网对应的源地址；省略则用默认路由推断",
    )
    ap.add_argument("--tcp-port", type=int, default=DEFAULT_TCP_PORT, help="TCP 服务端口（组播里控制端口）")
    ap.add_argument("--device-addr", type=lambda x: int(x, 0), default=0x01, help="从机协议地址，如 0x01")
    ap.add_argument("--push-interval", type=float, default=1.0, help="TCP 上 0x23 推送周期(秒)")
    ap.add_argument(
        "--discovery-unicast",
        default=None,
        metavar="IP",
        help="除组播外，把发现串再发到该 IP:2468（填安卓 IP，便于 Wi‑Fi 组播被拦时仍能发现）",
    )
    ap.add_argument(
        "--firmware-version",
        default="V1.0.0.SIM",
        help="固件版本字符串，最长 20 个 ASCII 字符（默认 V1.0.0.SIM）",
    )
    args = ap.parse_args()

    slave = TcpSlave(
        device_addr=args.device_addr,
        tcp_port=args.tcp_port,
        push_interval=args.push_interval,
        peer_for_local_ip=args.peer,
        discovery_unicast_to=args.discovery_unicast,
        firmware_version=args.firmware_version,
    )
    try:
        slave.run()
    except KeyboardInterrupt:
        slave._stop.set()
        print("exit")


if __name__ == "__main__":
    main()
