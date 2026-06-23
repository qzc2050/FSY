#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
NeiJi 串口协议工具 — 打开 USART1，解析辐射报警仪通用协议（0x03/0x13/0x23 等）。

用法示例:
  python fsy_serial_tool.py listen -p COM5
  python fsy_serial_tool.py read -p COM5 --start 0x0001 --count 11
  python fsy_serial_tool.py poll -p COM5 -i 2
"""
from __future__ import annotations

import argparse
import sys
import time
from datetime import datetime

import serial
import serial.tools.list_ports

from fsy_protocol import (
    DEFAULT_SLAVE_ADDR,
    RT_REG_START,
    FrameScanner,
    build_read_holding,
    describe_frame,
    parse_frame,
)


def list_ports() -> None:
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("未发现串口")
        return
    for p in ports:
        print(f"  {p.device}  {p.description}")


def open_port(port: str, baud: int) -> serial.Serial:
    return serial.Serial(
        port,
        baud,
        timeout=0.2,
        write_timeout=2.0,
        dsrdtr=False,
        rtscts=False,
    )


def ts() -> str:
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]


def cmd_listen(args: argparse.Namespace) -> int:
    scanner = FrameScanner(slave_filter=args.addr if args.filter_addr else None)
    with open_port(args.port, args.baud) as sp:
        print(f"[{ts()}] 监听 {args.port} @ {args.baud}，Ctrl+C 退出")
        print("  等待 0x23 主动上传 或 0x13 读应答；文本日志原样显示\n")
        try:
            while True:
                chunk = sp.read(max(1, sp.in_waiting or 256))
                if not chunk:
                    continue
                frames, text = scanner.feed(chunk)
                if text and args.show_log:
                    for line in text.splitlines():
                        print(f"[{ts()}] LOG  {line}")
                for pf in frames:
                    print(f"[{ts()}] RX  {pf.raw.hex(' ')}")
                    print(describe_frame(pf))
                    print()
        except KeyboardInterrupt:
            print("\n已停止")
    return 0


def _wait_response(sp: serial.Serial, timeout: float, addr: int) -> bytes:
    deadline = time.monotonic() + timeout
    buf = bytearray()
    while time.monotonic() < deadline:
        n = sp.in_waiting
        if n:
            buf.extend(sp.read(n))
            scanner = FrameScanner(slave_filter=addr)
            frames, _ = scanner.feed(bytes(buf))
            if frames:
                return frames[0].raw
        time.sleep(0.02)
    return bytes(buf)


def cmd_read(args: argparse.Namespace) -> int:
    req = build_read_holding(args.addr, args.start, args.count)
    with open_port(args.port, args.baud) as sp:
        sp.reset_input_buffer()
        sp.write(req)
        sp.flush()
        print(f"[{ts()}] TX  {req.hex(' ')}")
        raw = _wait_response(sp, args.timeout, args.addr)
        if not raw:
            print("超时：未收到应答")
            return 1
        pf = parse_frame(raw)
        if not pf:
            print(f"收到数据但 CRC/格式无效: {raw.hex(' ')}")
            return 1
        print(f"[{ts()}] RX  {raw.hex(' ')}")
        print(describe_frame(pf))
        if pf.func == 0x13 and args.count >= 11:
            print(
                "\n提示: NeiJi 固件 0x03 读回应为 uint16；完整传感器数据请用 listen 收 0x23 主动上传。"
            )
    return 0


def cmd_poll(args: argparse.Namespace) -> int:
    with open_port(args.port, args.baud) as sp:
        print(f"[{ts()}] 轮询读 0x{args.start:04X} x{args.count}，间隔 {args.interval}s")
        try:
            while True:
                req = build_read_holding(args.addr, args.start, args.count)
                sp.reset_input_buffer()
                sp.write(req)
                sp.flush()
                raw = _wait_response(sp, args.timeout, args.addr)
                pf = parse_frame(raw) if raw else None
                if pf and pf.func == 0x13:
                    print(f"\n[{ts()}]")
                    print(describe_frame(pf))
                else:
                    print(f"[{ts()}] 无有效应答")
                time.sleep(args.interval)
        except KeyboardInterrupt:
            print("\n已停止")
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="NeiJi 串口协议解析（辐射报警仪协议命令和寄存器表10）"
    )
    p.add_argument("--list", action="store_true", help="列出可用串口")
    sub = p.add_subparsers(dest="cmd", required=False)

    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("-p", "--port", required=True, help="串口，如 COM5")
    common.add_argument("-b", "--baud", type=int, default=115200, help="波特率，默认 115200")
    common.add_argument(
        "-a",
        "--addr",
        type=lambda x: int(x, 0),
        default=DEFAULT_SLAVE_ADDR,
        help="从机地址，默认 0x01",
    )

    listen_p = sub.add_parser("listen", parents=[common], help="监听主动上传 0x23 与日志")
    listen_p.add_argument(
        "--filter-addr",
        action="store_true",
        help="只解析指定从机地址的帧",
    )
    listen_p.add_argument(
        "--no-log",
        dest="show_log",
        action="store_false",
        help="不显示 ASCII 日志行",
    )
    listen_p.set_defaults(show_log=True)

    read_p = sub.add_parser("read", parents=[common], help="发送 0x03 读寄存器")
    read_p.add_argument(
        "--start",
        type=lambda x: int(x, 0),
        default=RT_REG_START,
        help="起始寄存器地址，默认 0x0001",
    )
    read_p.add_argument(
        "--count",
        type=int,
        default=11,
        help="寄存器数量，默认 11",
    )
    read_p.add_argument("--timeout", type=float, default=1.0, help="应答超时秒")

    poll_p = sub.add_parser("poll", parents=[common], help="周期 0x03 读寄存器")
    poll_p.add_argument("--start", type=lambda x: int(x, 0), default=RT_REG_START)
    poll_p.add_argument("--count", type=int, default=11)
    poll_p.add_argument("-i", "--interval", type=float, default=1.0)
    poll_p.add_argument("--timeout", type=float, default=1.0)

    return p


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if args.list:
        list_ports()
        return 0

    if not args.cmd:
        parser.print_help()
        print("\n示例:")
        print("  python fsy_serial_tool.py --list")
        print("  python fsy_serial_tool.py listen -p COM5")
        print("  python fsy_serial_tool.py read -p COM5 --start 0x0001 --count 11")
        return 0

    if args.cmd == "listen":
        return cmd_listen(args)
    if args.cmd == "read":
        return cmd_read(args)
    if args.cmd == "poll":
        return cmd_poll(args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
