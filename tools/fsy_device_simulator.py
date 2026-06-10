#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
模拟辐射报警仪下位机：组播发现 + 定时 UDP 主动上传 (0x23)。
用法:
  python tools/fsy_device_simulator.py --peer 192.168.3.148

peer 填安卓设备 IP，用于自动推断本机发往该地址时使用的源网卡 IPv4。
"""

from __future__ import annotations

import argparse
import socket
import struct
import threading
import time

# ---------- 协议与端口（与文档示例一致，可按需改） ----------
MCAST_GRP = "236.2.3.6"
MCAST_PORT = 2468

DEVICE_ADDR = 0x01
MODEL = "FSY-I"
SN = "1905CCM0101"
CTRL_PORT = 5001
DATA_PORT = 5000
PROTO_ADDR = "1"
PROTO_TYPE = "0"


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
    """本机发往 peer 时使用的源 IPv4（不真正发包）。"""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect((peer_ip, 9))
        return s.getsockname()[0]
    finally:
        s.close()


def build_upload_frame() -> bytes:
    """与调试示例一致: EF 23 2C 01 00 ... 寄存器数据 ... CRC."""
    func = 0x23
    byte_count = 0x2C
    start_reg = 0x0001
    values = [
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
    payload = b"".join(u32le(v) for v in values)
    head = bytes([DEVICE_ADDR, func, byte_count]) + struct.pack("<H", start_reg) + payload
    crc = crc16_modbus(head)
    return head + struct.pack("<H", crc)


def multicast_loop(local_ip: str, interval: float) -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 1)
    while True:
        msg = f"{MODEL},{SN},{local_ip},{CTRL_PORT},{DATA_PORT},{PROTO_ADDR},{PROTO_TYPE},0"
        sock.sendto(msg.encode("ascii"), (MCAST_GRP, MCAST_PORT))
        print(f"[MCAST] {msg}")
        time.sleep(interval)


def upload_loop(peer_ip: str, data_port: int, interval: float) -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    frame = build_upload_frame()
    while True:
        sock.sendto(frame, (peer_ip, data_port))
        print(f"[UDP 0x23 -> {peer_ip}:{data_port}] {frame.hex(' ').upper()}")
        time.sleep(interval)


def main() -> None:
    p = argparse.ArgumentParser(description="FSY 设备模拟器")
    p.add_argument("--peer", required=True, help="安卓设备 IP，用于推断本机源 IP 与上传目标")
    p.add_argument("--data-port", type=int, default=DATA_PORT, help="数据流 UDP 端口")
    p.add_argument("--mcast-interval", type=float, default=1.0, help="组播周期(秒)")
    p.add_argument("--upload-interval", type=float, default=1.0, help="0x23 上传周期(秒)")
    args = p.parse_args()

    local_ip = local_ip_towards(args.peer)
    print(f"本机出口 IP(发往 {args.peer}): {local_ip}")

    t1 = threading.Thread(
        target=multicast_loop,
        args=(local_ip, args.mcast_interval),
        daemon=True,
    )
    t2 = threading.Thread(
        target=upload_loop,
        args=(args.peer, args.data_port, args.upload_interval),
        daemon=True,
    )
    t1.start()
    t2.start()
    try:
        while True:
            time.sleep(3600)
    except KeyboardInterrupt:
        print("exit")


if __name__ == "__main__":
    main()
