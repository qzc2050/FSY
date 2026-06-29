#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""NeiJi 工厂生产 — 串口选择 + 实时参数 + 生产配置读写。"""
from __future__ import annotations

import json
import os
import threading
import time
import tkinter as tk
from dataclasses import dataclass
from datetime import datetime
from tkinter import messagebox, scrolledtext, ttk
from typing import Callable, Optional

import serial
import serial.tools.list_ports

from fsy_protocol import (
    CFG_MODEL_MAX_LEN,
    CFG_PRODUCT_NAME_MAX_BYTES,
    CFG_SN_MAX_LEN,
    FC_ACTIVE_UPLOAD,
    FC_READ_HOLDING_RESP,
    FC_WRITE_MULTI_RESP,
    FC_WRITE_SINGLE_RESP,
    ALARM_BIT_DOSE_HI,
    ALARM_BIT_DOSE_LO,
    REG_ADDRESS,
    REG_ALARM_ENABLE,
    REG_ALARM_ENABLE_COUNT,
    REG_CURRENT_IP,
    REG_CURRENT_IP_COUNT,
    REG_DOSE_HI_TH,
    REG_DOSE_LO_TH,
    REG_PRODUCT_MODEL,
    REG_PRODUCT_MODEL_COUNT,
    REG_PRODUCT_NAME,
    REG_PRODUCT_NAME_COUNT,
    REG_U32_COUNT,
    REG_SERIALNUM,
    REG_SERIALNUM_COUNT,
    REG_SOFTWARE_VERSION,
    REG_SOFTWARE_VERSION_COUNT,
    REG_TIME,
    REG_TIME_COUNT,
    RT_REGISTER_FMT,
    FrameScanner,
    ParsedFrame,
    ascii_to_reg_values,
    build_read_holding,
    build_write_multi,
    build_write_single,
    datetime_to_time_reg_values,
    describe_frame,
    format_alarm_status_detail,
    iter_u32_payload,
    reg_payload_to_ascii,
    reg_payload_to_utf8,
    utf8_to_reg_values,
    reg_payload_to_ipv4,
    reg_payload_to_time,
    reg_payload_to_u32,
    u32_to_reg_values,
)

APP_TITLE = "NeiJi 生产测试 — 串口协议"
DEFAULT_BAUD = 115200
CONFIG_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".last_port.json")

DISPLAY_REGS = [
    0x0001,
    0x0003,
    0x0005,
    0x0007,
    0x0009,
    0x000B,
    0x000D,
    0x000F,
]
RT_REG_ALARM_STATUS = 0x000D


def scan_port_items() -> list[str]:
    items = []
    for p in serial.tools.list_ports.comports():
        items.append(f"{p.device} — {p.description}")
    return items


def port_from_combo(text: str) -> str:
    if not text:
        return ""
    return text.split(" — ")[0].strip()


def load_last_port() -> str:
    try:
        if os.path.isfile(CONFIG_FILE):
            with open(CONFIG_FILE, "r", encoding="utf-8") as f:
                data = json.load(f)
            return str(data.get("port", ""))
    except (OSError, json.JSONDecodeError, TypeError):
        pass
    return ""


def save_last_port(port: str) -> None:
    try:
        with open(CONFIG_FILE, "w", encoding="utf-8") as f:
            json.dump({"port": port}, f)
    except OSError:
        pass


@dataclass
class PendingRequest:
    expect_fc: int
    on_ok: Callable[[ParsedFrame], None]
    on_fail: Callable[[str], None]
    deadline: float
    expect_reg: Optional[int] = None
    slave_addr: Optional[int] = None
    retries_left: int = 0
    tx_frame: bytes = b""


class SerialWorker:
    """后台读串口线程。"""

    def __init__(self, on_data, on_error, on_closed) -> None:
        self._on_data = on_data
        self._on_error = on_error
        self._on_closed = on_closed
        self._sp: serial.Serial | None = None
        self._running = False
        self._thread: threading.Thread | None = None

    def start(self, port: str, baud: int) -> None:
        self.stop()
        self._sp = serial.Serial(
            port,
            baud,
            timeout=0.2,
            write_timeout=2.0,
            dsrdtr=False,
            rtscts=False,
        )
        self._running = True
        self._thread = threading.Thread(target=self._read_loop, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._running = False
        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=1.0)
        self._thread = None
        if self._sp:
            try:
                self._sp.close()
            except Exception:
                pass
            self._sp = None

    def send(self, data: bytes) -> bool:
        if not self._sp or not self._sp.is_open:
            return False
        try:
            self._sp.write(data)
            self._sp.flush()
            return True
        except serial.SerialException as exc:
            self._on_error(str(exc))
            return False

    def flush_rx(self) -> None:
        if not self._sp or not self._sp.is_open:
            return
        try:
            self._sp.reset_input_buffer()
        except serial.SerialException:
            pass

    def _read_loop(self) -> None:
        try:
            while self._running and self._sp and self._sp.is_open:
                n = self._sp.in_waiting
                if n <= 0:
                    time.sleep(0.01)
                    continue
                chunk = self._sp.read(n)
                if chunk:
                    self._on_data(chunk)
        except serial.SerialException as exc:
            if self._running:
                self._on_error(str(exc))
        finally:
            self._on_closed()


class FactoryApp(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title(APP_TITLE)
        self.geometry("960x760")
        self.minsize(860, 620)

        self.worker = SerialWorker(
            on_data=lambda d: self.after(0, lambda: self._on_serial_data(d)),
            on_error=lambda e: self.after(0, lambda: self._on_serial_error(e)),
            on_closed=lambda: self.after(0, self._on_serial_closed),
        )
        self.scanner = FrameScanner(slave_filter=None)
        self.connected = False
        self.frame_count = 0
        self.value_labels: dict[int, tk.Label] = {}
        self._pending: Optional[PendingRequest] = None
        self._poll_after_id: Optional[str] = None
        self._cfg_busy = False

        self._build_ui()
        self.refresh_ports()
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    def _build_ui(self) -> None:
        top = ttk.Frame(self, padding=8)
        top.pack(fill=tk.X)

        ttk.Label(top, text="串口:").pack(side=tk.LEFT, padx=(0, 4))
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(
            top, textvariable=self.port_var, width=38, state="readonly"
        )
        self.port_combo.pack(side=tk.LEFT, padx=4)

        ttk.Button(top, text="刷新", command=self.refresh_ports, width=6).pack(
            side=tk.LEFT, padx=4
        )

        ttk.Label(top, text="波特率:").pack(side=tk.LEFT, padx=(12, 4))
        self.baud_var = tk.StringVar(value=str(DEFAULT_BAUD))
        ttk.Combobox(
            top,
            textvariable=self.baud_var,
            values=["9600", "115200", "921600"],
            width=8,
            state="readonly",
        ).pack(side=tk.LEFT, padx=4)

        ttk.Label(top, text="从机地址:").pack(side=tk.LEFT, padx=(12, 4))
        self.slave_addr_var = tk.StringVar(value="1")
        ttk.Spinbox(
            top,
            from_=1,
            to=247,
            textvariable=self.slave_addr_var,
            width=5,
        ).pack(side=tk.LEFT, padx=4)

        self.btn_connect = ttk.Button(
            top, text="连接", command=self.toggle_connect, width=10
        )
        self.btn_connect.pack(side=tk.LEFT, padx=(12, 4))

        ttk.Button(top, text="清空日志", command=self.clear_log, width=8).pack(
            side=tk.LEFT, padx=4
        )

        status = ttk.Frame(self, padding=(8, 0, 8, 4))
        status.pack(fill=tk.X)
        self.status_var = tk.StringVar(value="未连接")
        ttk.Label(status, textvariable=self.status_var, foreground="#666").pack(
            anchor=tk.W
        )

        notebook = ttk.Notebook(self)
        notebook.pack(fill=tk.BOTH, expand=True, padx=8, pady=4)

        rt_tab = ttk.Frame(notebook, padding=4)
        cfg_tab = ttk.Frame(notebook, padding=12)
        notebook.add(rt_tab, text="实时参数")
        notebook.add(cfg_tab, text="生产配置")

        self._build_rt_tab(rt_tab)
        self._build_cfg_tab(cfg_tab)

        log_frame = ttk.LabelFrame(self, text="通信日志", padding=4)
        log_frame.pack(fill=tk.BOTH, expand=True, padx=8, pady=(0, 8))
        self.log_text = scrolledtext.ScrolledText(
            log_frame, height=12, font=("Consolas", 9), wrap=tk.WORD
        )
        self.log_text.pack(fill=tk.BOTH, expand=True)
        self.log_text.configure(state=tk.DISABLED)

    def _build_rt_tab(self, parent: ttk.Frame) -> None:
        grid_wrap = ttk.LabelFrame(parent, text="0x23 主动上传", padding=12)
        grid_wrap.pack(fill=tk.X)

        for i, reg in enumerate(DISPLAY_REGS):
            row, col = divmod(i, 4)
            name, _ = RT_REGISTER_FMT.get(reg, (f"0x{reg:04X}", lambda v: str(v)))
            cell = ttk.Frame(grid_wrap, padding=8, relief=tk.GROOVE, borderwidth=1)
            cell.grid(row=row, column=col, padx=6, pady=6, sticky="nsew")
            grid_wrap.columnconfigure(col, weight=1)

            ttk.Label(cell, text=name, font=("Microsoft YaHei", 10)).pack(anchor=tk.W)
            val_lbl = tk.Label(
                cell,
                text="—",
                font=("Microsoft YaHei", 18, "bold"),
                fg="#0066CC",
                anchor=tk.W,
            )
            val_lbl.pack(anchor=tk.W, fill=tk.X)
            self.value_labels[reg] = val_lbl

        detail_frame = ttk.LabelFrame(parent, text="报警 / 故障明细（0x000D）", padding=10)
        detail_frame.pack(fill=tk.X, pady=(10, 0))
        self.alarm_detail_lbl = tk.Label(
            detail_frame,
            text="—",
            font=("Microsoft YaHei", 11),
            fg="#333333",
            justify=tk.LEFT,
            anchor=tk.W,
            wraplength=900,
        )
        self.alarm_detail_lbl.pack(fill=tk.X)

        ttk.Button(parent, text="手动 0x03 读实时区", command=self.read_registers).pack(
            anchor=tk.W, pady=(8, 0)
        )

    def _build_cfg_tab(self, parent: ttk.Frame) -> None:
        form = ttk.Frame(parent)
        form.pack(fill=tk.X)

        ttk.Label(form, text="序列号 (reg 86):").grid(row=0, column=0, sticky=tk.W, pady=6)
        self.sn_var = tk.StringVar()
        ttk.Entry(form, textvariable=self.sn_var, width=28).grid(
            row=0, column=1, sticky=tk.W, padx=8, pady=6
        )
        ttk.Label(form, text=f"最多 {CFG_SN_MAX_LEN} 字符", foreground="#666").grid(
            row=0, column=2, sticky=tk.W
        )

        ttk.Label(form, text="产品名称 (reg 146):").grid(row=1, column=0, sticky=tk.W, pady=6)
        self.name_var = tk.StringVar(value="雷沃-探测器")
        ttk.Entry(form, textvariable=self.name_var, width=28).grid(
            row=1, column=1, sticky=tk.W, padx=8, pady=6
        )
        ttk.Label(
            form,
            text=f"UTF-8，最多 {CFG_PRODUCT_NAME_MAX_BYTES} 字节",
            foreground="#666",
        ).grid(row=1, column=2, sticky=tk.W)

        ttk.Label(form, text="产品型号 (reg 130):").grid(row=2, column=0, sticky=tk.W, pady=6)
        self.model_var = tk.StringVar()
        ttk.Entry(form, textvariable=self.model_var, width=28).grid(
            row=2, column=1, sticky=tk.W, padx=8, pady=6
        )
        ttk.Label(form, text=f"最多 {CFG_MODEL_MAX_LEN} 字符", foreground="#666").grid(
            row=2, column=2, sticky=tk.W
        )

        ttk.Label(form, text="协议地址 (reg 121):").grid(row=3, column=0, sticky=tk.W, pady=6)
        self.dev_addr_var = tk.StringVar(value="1")
        ttk.Spinbox(
            form,
            from_=1,
            to=247,
            textvariable=self.dev_addr_var,
            width=8,
        ).grid(row=3, column=1, sticky=tk.W, padx=8, pady=6)
        ttk.Label(form, text="写入后请同步修改顶部「从机地址」", foreground="#666").grid(
            row=3, column=2, sticky=tk.W
        )

        ttk.Label(form, text="当前 IP (reg 6):").grid(row=4, column=0, sticky=tk.W, pady=6)
        self.current_ip_var = tk.StringVar(value="—")
        ttk.Entry(
            form, textvariable=self.current_ip_var, width=28, state="readonly"
        ).grid(row=4, column=1, sticky=tk.W, padx=8, pady=6)
        ttk.Label(form, text="只读，读 W5500 实际 IP", foreground="#666").grid(
            row=4, column=2, sticky=tk.W
        )

        ttk.Label(form, text="剂量率上阈值 (reg 50):").grid(row=5, column=0, sticky=tk.W, pady=6)
        self.dose_hi_var = tk.StringVar(value="10000.00")
        ttk.Entry(form, textvariable=self.dose_hi_var, width=28).grid(
            row=5, column=1, sticky=tk.W, padx=8, pady=6
        )
        ttk.Label(form, text="单位 μSv/h，协议值=×100", foreground="#666").grid(
            row=5, column=2, sticky=tk.W
        )

        ttk.Label(form, text="剂量率下阈值 (reg 52):").grid(row=6, column=0, sticky=tk.W, pady=6)
        self.dose_lo_var = tk.StringVar(value="0.00")
        ttk.Entry(form, textvariable=self.dose_lo_var, width=28).grid(
            row=6, column=1, sticky=tk.W, padx=8, pady=6
        )
        ttk.Label(form, text="0 表示下阈值不触发", foreground="#666").grid(
            row=6, column=2, sticky=tk.W
        )

        ttk.Label(form, text="报警使能 (reg 82):").grid(row=7, column=0, sticky=tk.W, pady=6)
        alarm_row = ttk.Frame(form)
        alarm_row.grid(row=7, column=1, sticky=tk.W, padx=8, pady=6)
        self.alarm_hi_var = tk.IntVar(value=1)
        self.alarm_lo_var = tk.IntVar(value=1)
        ttk.Checkbutton(alarm_row, text="bit0 上阈值", variable=self.alarm_hi_var).pack(
            side=tk.LEFT, padx=(0, 12)
        )
        ttk.Checkbutton(alarm_row, text="bit1 下阈值", variable=self.alarm_lo_var).pack(
            side=tk.LEFT
        )
        ttk.Label(form, text="勾选=允许报警，取消=清报警位", foreground="#666").grid(
            row=7, column=2, sticky=tk.W
        )

        ttk.Label(form, text="RTC 时间 (reg 94):").grid(row=8, column=0, sticky=tk.W, pady=6)
        self.rtc_time_var = tk.StringVar(value="—")
        ttk.Entry(
            form, textvariable=self.rtc_time_var, width=28, state="readonly"
        ).grid(row=8, column=1, sticky=tk.W, padx=8, pady=6)
        ttk.Label(form, text="读 PCF85063，写入不落 W25Q", foreground="#666").grid(
            row=8, column=2, sticky=tk.W
        )

        ttk.Label(form, text="软件版本 (reg 98):").grid(row=9, column=0, sticky=tk.W, pady=6)
        self.sw_version_var = tk.StringVar(value="—")
        ttk.Entry(
            form, textvariable=self.sw_version_var, width=28, state="readonly"
        ).grid(row=9, column=1, sticky=tk.W, padx=8, pady=6)
        ttk.Label(form, text="只读，固件编译期版本", foreground="#666").grid(
            row=9, column=2, sticky=tk.W
        )

        btn_row = ttk.Frame(parent)
        btn_row.pack(fill=tk.X, pady=(16, 8))
        ttk.Button(btn_row, text="读取配置", command=self.read_factory_cfg, width=12).pack(
            side=tk.LEFT, padx=(0, 8)
        )
        ttk.Button(btn_row, text="写入全部", command=self.write_factory_cfg, width=12).pack(
            side=tk.LEFT, padx=8
        )
        ttk.Button(btn_row, text="仅写 SN", command=self.write_sn_only, width=10).pack(
            side=tk.LEFT, padx=8
        )
        ttk.Button(btn_row, text="仅写名称", command=self.write_name_only, width=10).pack(
            side=tk.LEFT, padx=8
        )
        ttk.Button(btn_row, text="仅写型号", command=self.write_model_only, width=10).pack(
            side=tk.LEFT, padx=8
        )
        ttk.Button(btn_row, text="仅写地址", command=self.write_addr_only, width=10).pack(
            side=tk.LEFT, padx=8
        )
        ttk.Button(btn_row, text="仅写阈值/报警", command=self.write_alarm_only, width=14).pack(
            side=tk.LEFT, padx=8
        )
        ttk.Button(btn_row, text="同步当前时间", command=self.sync_current_time, width=14).pack(
            side=tk.LEFT, padx=8
        )

        hint = ttk.Label(
            parent,
            text="配置写入后落 W25Q，断电保持。当前 IP 需网络就绪后读取。"
            "修改协议地址后需用新地址通信。",
            foreground="#444",
            wraplength=820,
        )
        hint.pack(anchor=tk.W, pady=(8, 0))

    def _slave_addr(self) -> int:
        try:
            addr = int(self.slave_addr_var.get())
        except ValueError:
            addr = 1
        return max(1, min(247, addr))

    def _require_connected(self) -> bool:
        if not self.connected:
            messagebox.showinfo("提示", "请先连接串口")
            return False
        return True

    def _prep_config_tx(self) -> None:
        self._cfg_busy = True
        self.scanner.reset()
        self.scanner.ignore_text = True
        self.worker.flush_rx()

    def _end_config_tx(self) -> None:
        self._cfg_busy = False
        self.scanner.ignore_text = False

    def _cancel_pending(self) -> None:
        self._pending = None
        self._end_config_tx()
        if self._poll_after_id is not None:
            try:
                self.after_cancel(self._poll_after_id)
            except tk.TclError:
                pass
            self._poll_after_id = None

    def _send_frame(self, frame: bytes) -> bool:
        ok = self.worker.send(frame)
        if ok:
            self._log(f"TX  {frame.hex(' ').upper()}")
        return ok

    def _begin_request(
        self,
        frame: bytes,
        expect_fc: int,
        on_ok: Callable[[ParsedFrame], None],
        on_fail: Callable[[str], None],
        timeout: float = 5.0,
        expect_reg: Optional[int] = None,
        retries: int = 3,
    ) -> None:
        self._cancel_pending()
        self._prep_config_tx()

        def start_attempt(remaining: int) -> None:
            if not self._send_frame(frame):
                self._cancel_pending()
                on_fail("发送失败")
                return

            self._pending = PendingRequest(
                expect_fc=expect_fc,
                on_ok=on_ok,
                on_fail=on_fail,
                deadline=time.monotonic() + timeout,
                expect_reg=expect_reg,
                slave_addr=self._slave_addr(),
                retries_left=remaining,
                tx_frame=frame,
            )
            self._poll_after_id = self.after(50, self._poll_pending)

        start_attempt(retries)

    def _poll_pending(self) -> None:
        if self._pending is None:
            return
        if time.monotonic() > self._pending.deadline:
            pending = self._pending
            if pending.retries_left > 0:
                self._log(
                    f"WARN 配置应答超时，重试 ({pending.retries_left} 次剩余)..."
                )
                self.worker.flush_rx()
                self.scanner.reset()

                def retry() -> None:
                    if not self._send_frame(pending.tx_frame):
                        self._cancel_pending()
                        pending.on_fail("发送失败")
                        return
                    self._pending = PendingRequest(
                        expect_fc=pending.expect_fc,
                        on_ok=pending.on_ok,
                        on_fail=pending.on_fail,
                        deadline=time.monotonic() + 5.0,
                        expect_reg=pending.expect_reg,
                        slave_addr=pending.slave_addr,
                        retries_left=pending.retries_left - 1,
                        tx_frame=pending.tx_frame,
                    )
                    self._poll_after_id = self.after(50, self._poll_pending)

                retry()
                return

            fail = pending.on_fail
            addr = pending.slave_addr
            self._cancel_pending()
            fail(
                f"应答超时。请确认：\n"
                f"1) 固件已烧录 P0 版本\n"
                f"2) 顶部「从机地址」= 设备 reg121（当前尝试 {addr}）\n"
                f"3) 串口线/波特率 115200 正确"
            )
            return
        self._poll_after_id = self.after(50, self._poll_pending)

    def _dispatch_pending(self, pf: ParsedFrame) -> bool:
        pending = self._pending
        if pending is None:
            return False

        if pending.slave_addr is not None and pf.addr != pending.slave_addr:
            return False

        if pf.func & 0x80:
            fail = pending.on_fail
            self._cancel_pending()
            fail(f"设备错误 reg=0x{pf.reg_addr:04X} code=0x{pf.error_code:04X}")
            return True

        if pf.func != pending.expect_fc:
            return False

        if pending.expect_reg is not None and pf.reg_addr != pending.expect_reg:
            return False

        ok = pending.on_ok
        self._cancel_pending()
        ok(pf)
        return True

    def read_factory_cfg(self) -> None:
        if not self._require_connected():
            return

        results: dict[str, str | int] = {}

        def finish_read() -> None:
            self.sn_var.set(str(results.get("sn", "")))
            self.name_var.set(str(results.get("name", "")))
            self.model_var.set(str(results.get("model", "")))
            self.dev_addr_var.set(str(results.get("addr", self.dev_addr_var.get())))
            self.current_ip_var.set(str(results.get("current_ip", "—")))
            self.dose_hi_var.set(str(results.get("dose_hi", self.dose_hi_var.get())))
            self.dose_lo_var.set(str(results.get("dose_lo", self.dose_lo_var.get())))
            alarm_enable = int(results.get("alarm_enable", 0))
            self.alarm_hi_var.set(1 if (alarm_enable & (1 << ALARM_BIT_DOSE_HI)) else 0)
            self.alarm_lo_var.set(1 if (alarm_enable & (1 << ALARM_BIT_DOSE_LO)) else 0)
            self.rtc_time_var.set(str(results.get("rtc_time", "—")))
            self.sw_version_var.set(str(results.get("sw_version", "—")))
            self.status_var.set("配置读取完成")
            messagebox.showinfo("完成", "已读取：SN / 名称 / 型号 / 地址 / 当前 IP / 阈值 / 报警使能 / 时间 / 版本")

        def read_sw_version_step() -> None:
            req = build_read_holding(
                self._slave_addr(), REG_SOFTWARE_VERSION, REG_SOFTWARE_VERSION_COUNT
            )

            def on_ok(pf: ParsedFrame) -> None:
                results["sw_version"] = reg_payload_to_ascii(pf.payload)
                finish_read()

            self._begin_request(
                req,
                FC_READ_HOLDING_RESP,
                on_ok,
                self._on_cfg_fail,
                expect_reg=REG_SOFTWARE_VERSION,
            )

        def read_time_step() -> None:
            req = build_read_holding(self._slave_addr(), REG_TIME, REG_TIME_COUNT)

            def on_ok(pf: ParsedFrame) -> None:
                results["rtc_time"] = reg_payload_to_time(pf.payload)
                read_sw_version_step()

            self._begin_request(
                req, FC_READ_HOLDING_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_TIME
            )

        def read_alarm_enable_step() -> None:
            req = build_read_holding(
                self._slave_addr(), REG_ALARM_ENABLE, REG_ALARM_ENABLE_COUNT
            )

            def on_ok(pf: ParsedFrame) -> None:
                results["alarm_enable"] = reg_payload_to_u32(pf.payload)
                read_time_step()

            self._begin_request(
                req, FC_READ_HOLDING_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_ALARM_ENABLE
            )

        def read_dose_lo_step() -> None:
            req = build_read_holding(self._slave_addr(), REG_DOSE_LO_TH, REG_U32_COUNT)

            def on_ok(pf: ParsedFrame) -> None:
                results["dose_lo"] = f"{reg_payload_to_u32(pf.payload) / 100.0:.2f}"
                read_alarm_enable_step()

            self._begin_request(
                req, FC_READ_HOLDING_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_DOSE_LO_TH
            )

        def read_dose_hi_step() -> None:
            req = build_read_holding(self._slave_addr(), REG_DOSE_HI_TH, REG_U32_COUNT)

            def on_ok(pf: ParsedFrame) -> None:
                results["dose_hi"] = f"{reg_payload_to_u32(pf.payload) / 100.0:.2f}"
                read_dose_lo_step()

            self._begin_request(
                req, FC_READ_HOLDING_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_DOSE_HI_TH
            )

        def read_current_ip_step() -> None:
            req = build_read_holding(
                self._slave_addr(), REG_CURRENT_IP, REG_CURRENT_IP_COUNT
            )

            def on_ok(pf: ParsedFrame) -> None:
                results["current_ip"] = reg_payload_to_ipv4(pf.payload)
                read_dose_hi_step()

            self._begin_request(
                req, FC_READ_HOLDING_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_CURRENT_IP
            )

        def read_model_step() -> None:
            req = build_read_holding(
                self._slave_addr(), REG_PRODUCT_MODEL, REG_PRODUCT_MODEL_COUNT
            )

            def on_ok(pf: ParsedFrame) -> None:
                results["model"] = reg_payload_to_ascii(pf.payload)
                read_current_ip_step()

            self._begin_request(req, FC_READ_HOLDING_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_PRODUCT_MODEL)

        def read_name_step() -> None:
            req = build_read_holding(
                self._slave_addr(), REG_PRODUCT_NAME, REG_PRODUCT_NAME_COUNT
            )

            def on_ok(pf: ParsedFrame) -> None:
                results["name"] = reg_payload_to_utf8(pf.payload)
                read_model_step()

            self._begin_request(
                req, FC_READ_HOLDING_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_PRODUCT_NAME
            )

        def read_addr_step() -> None:
            req = build_read_holding(self._slave_addr(), REG_ADDRESS, 1)

            def on_ok(pf: ParsedFrame) -> None:
                if len(pf.payload) >= 2:
                    results["addr"] = int.from_bytes(pf.payload[:2], "little")
                read_name_step()

            self._begin_request(req, FC_READ_HOLDING_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_ADDRESS)

        def read_sn_step() -> None:
            req = build_read_holding(
                self._slave_addr(), REG_SERIALNUM, REG_SERIALNUM_COUNT
            )

            def on_ok(pf: ParsedFrame) -> None:
                results["sn"] = reg_payload_to_ascii(pf.payload)
                read_addr_step()

            self._begin_request(req, FC_READ_HOLDING_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_SERIALNUM)

        self.status_var.set("正在读取配置...")
        read_sn_step()

    def _on_cfg_fail(self, msg: str) -> None:
        self.status_var.set(f"配置操作失败: {msg}")
        messagebox.showerror("失败", msg)

    def write_sn_only(self) -> None:
        if not self._require_connected():
            return
        sn = self.sn_var.get().strip()[:CFG_SN_MAX_LEN]
        if not sn:
            messagebox.showwarning("提示", "请输入序列号")
            return
        values = ascii_to_reg_values(sn, REG_SERIALNUM_COUNT)
        req = build_write_multi(self._slave_addr(), REG_SERIALNUM, values)
        self._begin_request(
            req,
            FC_WRITE_MULTI_RESP,
            lambda _pf: messagebox.showinfo("完成", f"序列号已写入: {sn}"),
            self._on_cfg_fail,
            expect_reg=REG_SERIALNUM,
        )

    def write_name_only(self) -> None:
        if not self._require_connected():
            return
        name = self.name_var.get().strip()
        if not name:
            messagebox.showwarning("提示", "请输入产品名称")
            return
        values = utf8_to_reg_values(name, REG_PRODUCT_NAME_COUNT)
        req = build_write_multi(self._slave_addr(), REG_PRODUCT_NAME, values)
        self._begin_request(
            req,
            FC_WRITE_MULTI_RESP,
            lambda _pf: messagebox.showinfo("完成", f"产品名称已写入: {name}"),
            self._on_cfg_fail,
            expect_reg=REG_PRODUCT_NAME,
        )

    def write_model_only(self) -> None:
        if not self._require_connected():
            return
        model = self.model_var.get().strip()[:CFG_MODEL_MAX_LEN]
        if not model:
            messagebox.showwarning("提示", "请输入产品型号")
            return
        values = ascii_to_reg_values(model, REG_PRODUCT_MODEL_COUNT)
        req = build_write_multi(self._slave_addr(), REG_PRODUCT_MODEL, values)
        self._begin_request(
            req,
            FC_WRITE_MULTI_RESP,
            lambda _pf: messagebox.showinfo("完成", f"型号已写入: {model}"),
            self._on_cfg_fail,
            expect_reg=REG_PRODUCT_MODEL,
        )

    def write_addr_only(self) -> None:
        if not self._require_connected():
            return
        try:
            addr = int(self.dev_addr_var.get())
        except ValueError:
            messagebox.showwarning("提示", "协议地址无效")
            return
        addr = max(1, min(247, addr))
        req = build_write_single(self._slave_addr(), REG_ADDRESS, addr)

        def on_ok(_pf: ParsedFrame) -> None:
            self.slave_addr_var.set(str(addr))
            messagebox.showinfo(
                "完成",
                f"协议地址已写入: {addr}\n顶部「从机地址」已同步，后续通信用此地址。",
            )

        self._begin_request(req, FC_WRITE_SINGLE_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_ADDRESS)

    def _alarm_form_values(self) -> tuple[int, int, int] | None:
        try:
            dose_hi_x100 = max(0, int(float(self.dose_hi_var.get()) * 100.0))
            dose_lo_x100 = max(0, int(float(self.dose_lo_var.get()) * 100.0))
        except ValueError:
            messagebox.showwarning("提示", "剂量率阈值无效")
            return None

        alarm_enable = 0
        if self.alarm_hi_var.get():
            alarm_enable |= (1 << ALARM_BIT_DOSE_HI)
        if self.alarm_lo_var.get():
            alarm_enable |= (1 << ALARM_BIT_DOSE_LO)

        return dose_hi_x100, dose_lo_x100, alarm_enable

    def write_alarm_only(self) -> None:
        if not self._require_connected():
            return
        values = self._alarm_form_values()
        if values is None:
            return
        dose_hi_x100, dose_lo_x100, alarm_enable = values

        def write_alarm_enable_step() -> None:
            req = build_write_multi(
                self._slave_addr(), REG_ALARM_ENABLE, u32_to_reg_values(alarm_enable)
            )
            self._begin_request(
                req,
                FC_WRITE_MULTI_RESP,
                lambda _pf: messagebox.showinfo("完成", "阈值 / 报警使能已写入 W25Q"),
                self._on_cfg_fail,
                expect_reg=REG_ALARM_ENABLE,
            )

        def write_dose_lo_step() -> None:
            req = build_write_multi(
                self._slave_addr(), REG_DOSE_LO_TH, u32_to_reg_values(dose_lo_x100)
            )
            self._begin_request(
                req,
                FC_WRITE_MULTI_RESP,
                lambda _pf: write_alarm_enable_step(),
                self._on_cfg_fail,
                expect_reg=REG_DOSE_LO_TH,
            )

        req = build_write_multi(
            self._slave_addr(), REG_DOSE_HI_TH, u32_to_reg_values(dose_hi_x100)
        )
        self._begin_request(
            req,
            FC_WRITE_MULTI_RESP,
            lambda _pf: write_dose_lo_step(),
            self._on_cfg_fail,
            expect_reg=REG_DOSE_HI_TH,
        )

    def sync_current_time(self) -> None:
        if not self._require_connected():
            return

        now = datetime.now()
        req = build_write_multi(self._slave_addr(), REG_TIME, datetime_to_time_reg_values(now))

        def on_ok(_pf: ParsedFrame) -> None:
            self.rtc_time_var.set(now.strftime("%Y-%m-%d %H:%M:%S"))
            messagebox.showinfo("完成", f"RTC 时间已同步: {self.rtc_time_var.get()}")

        self._begin_request(
            req,
            FC_WRITE_MULTI_RESP,
            on_ok,
            self._on_cfg_fail,
            expect_reg=REG_TIME,
        )

    def write_factory_cfg(self) -> None:
        if not self._require_connected():
            return
        sn = self.sn_var.get().strip()[:CFG_SN_MAX_LEN]
        name = self.name_var.get().strip()
        model = self.model_var.get().strip()[:CFG_MODEL_MAX_LEN]
        if not sn or not name or not model:
            messagebox.showwarning("提示", "请填写序列号、产品名称和产品型号")
            return
        try:
            addr = int(self.dev_addr_var.get())
        except ValueError:
            messagebox.showwarning("提示", "协议地址无效")
            return
        addr = max(1, min(247, addr))
        try:
            dose_hi_x100 = max(0, int(float(self.dose_hi_var.get()) * 100.0))
            dose_lo_x100 = max(0, int(float(self.dose_lo_var.get()) * 100.0))
        except ValueError:
            messagebox.showwarning("提示", "剂量率阈值无效")
            return

        alarm_enable = 0
        if self.alarm_hi_var.get():
            alarm_enable |= (1 << ALARM_BIT_DOSE_HI)
        if self.alarm_lo_var.get():
            alarm_enable |= (1 << ALARM_BIT_DOSE_LO)

        def write_alarm_enable_step() -> None:
            req = build_write_multi(
                self._slave_addr(), REG_ALARM_ENABLE, u32_to_reg_values(alarm_enable)
            )

            def on_ok(_pf: ParsedFrame) -> None:
                self.slave_addr_var.set(str(addr))
                self.status_var.set("配置写入完成")
                messagebox.showinfo("完成", "SN / 名称 / 型号 / 地址 / 阈值 / 报警使能已写入 W25Q")

            self._begin_request(
                req,
                FC_WRITE_MULTI_RESP,
                on_ok,
                self._on_cfg_fail,
                expect_reg=REG_ALARM_ENABLE,
            )

        def write_dose_lo_step() -> None:
            req = build_write_multi(
                self._slave_addr(), REG_DOSE_LO_TH, u32_to_reg_values(dose_lo_x100)
            )
            self._begin_request(
                req,
                FC_WRITE_MULTI_RESP,
                lambda _pf: write_alarm_enable_step(),
                self._on_cfg_fail,
                expect_reg=REG_DOSE_LO_TH,
            )

        def write_dose_hi_step() -> None:
            req = build_write_multi(
                self._slave_addr(), REG_DOSE_HI_TH, u32_to_reg_values(dose_hi_x100)
            )
            self._begin_request(
                req,
                FC_WRITE_MULTI_RESP,
                lambda _pf: write_dose_lo_step(),
                self._on_cfg_fail,
                expect_reg=REG_DOSE_HI_TH,
            )

        def write_addr_step() -> None:
            req = build_write_single(self._slave_addr(), REG_ADDRESS, addr)
            self._begin_request(
                req,
                FC_WRITE_SINGLE_RESP,
                lambda _pf: write_dose_hi_step(),
                self._on_cfg_fail,
                expect_reg=REG_ADDRESS,
            )

        def write_model_step() -> None:
            values = ascii_to_reg_values(model, REG_PRODUCT_MODEL_COUNT)
            req = build_write_multi(self._slave_addr(), REG_PRODUCT_MODEL, values)
            self._begin_request(
                req,
                FC_WRITE_MULTI_RESP,
                lambda _pf: write_addr_step(),
                self._on_cfg_fail,
                expect_reg=REG_PRODUCT_MODEL,
            )

        def write_name_step() -> None:
            values = utf8_to_reg_values(name, REG_PRODUCT_NAME_COUNT)
            req = build_write_multi(self._slave_addr(), REG_PRODUCT_NAME, values)
            self._begin_request(
                req,
                FC_WRITE_MULTI_RESP,
                lambda _pf: write_model_step(),
                self._on_cfg_fail,
                expect_reg=REG_PRODUCT_NAME,
            )

        def write_sn_step() -> None:
            values = ascii_to_reg_values(sn, REG_SERIALNUM_COUNT)
            req = build_write_multi(self._slave_addr(), REG_SERIALNUM, values)
            self._begin_request(
                req,
                FC_WRITE_MULTI_RESP,
                lambda _pf: write_name_step(),
                self._on_cfg_fail,
                expect_reg=REG_SERIALNUM,
            )

        self.status_var.set("正在写入配置...")
        write_sn_step()

    def refresh_ports(self) -> None:
        items = scan_port_items()
        self.port_combo["values"] = items
        last = load_last_port()
        if last:
            for item in items:
                if item.startswith(last + " "):
                    self.port_var.set(item)
                    return
            self.port_var.set(last)
        elif items:
            self.port_var.set(items[0])
        else:
            self.port_var.set("")
            self._log("未发现串口，请插入 USB 转串口后点「刷新」")

    def toggle_connect(self) -> None:
        if self.connected:
            self.disconnect()
        else:
            self.connect()

    def connect(self) -> None:
        port = port_from_combo(self.port_var.get())
        if not port:
            messagebox.showwarning("提示", "请先选择串口")
            return
        try:
            baud = int(self.baud_var.get())
        except ValueError:
            messagebox.showwarning("提示", "波特率无效")
            return
        try:
            self.worker.start(port, baud)
        except serial.SerialException as exc:
            messagebox.showerror("连接失败", str(exc))
            return

        self.connected = True
        self.scanner = FrameScanner(slave_filter=None)
        self.frame_count = 0
        save_last_port(port)
        self.btn_connect.configure(text="断开")
        self.port_combo.configure(state="disabled")
        self.status_var.set(f"已连接 {port} @ {baud} — 等待数据...")
        self._log(f"=== 已连接 {port} @ {baud} ===")

    def disconnect(self) -> None:
        self._cancel_pending()
        self.worker.stop()
        self.connected = False
        self.btn_connect.configure(text="连接")
        self.port_combo.configure(state="readonly")
        self.status_var.set("未连接")
        self._log("=== 已断开 ===")

    def read_registers(self) -> None:
        if not self._require_connected():
            return
        req = build_read_holding(self._slave_addr(), 0x0001, 11)
        self._send_frame(req)

    def _on_serial_data(self, data: bytes) -> None:
        frames, text = self.scanner.feed(data)
        if text:
            for line in text.splitlines():
                self._log(f"LOG {line}")
        for pf in frames:
            self._handle_frame(pf)

    def _handle_frame(self, pf: ParsedFrame) -> None:
        if self._dispatch_pending(pf):
            self._log(f"RX  {pf.raw.hex(' ').upper()}")
            return

        if self._cfg_busy and pf.func == FC_ACTIVE_UPLOAD:
            return

        self.frame_count += 1
        ts = datetime.now().strftime("%H:%M:%S")
        self._log(f"[{ts}] RX  {pf.raw.hex(' ').upper()}")

        if pf.func in (FC_ACTIVE_UPLOAD, FC_READ_HOLDING_RESP) and pf.payload:
            if pf.func == FC_ACTIVE_UPLOAD:
                for reg_addr, raw in iter_u32_payload(pf.reg_addr, pf.payload):
                    self._update_reg_display(reg_addr, raw)
                self.status_var.set(
                    f"已连接 — 收到 0x23  #{self.frame_count}  "
                    f"更新 {datetime.now().strftime('%H:%M:%S')}"
                )
            else:
                self._log(describe_frame(pf))
        else:
            self._log(describe_frame(pf))

    def _update_reg_display(self, reg_addr: int, raw: int) -> None:
        lbl = self.value_labels.get(reg_addr)
        if lbl is None:
            return
        name_fmt = RT_REGISTER_FMT.get(reg_addr)
        if name_fmt:
            _, fmt = name_fmt
            lbl.configure(text=fmt(raw))
            if reg_addr == RT_REG_ALARM_STATUS:
                if raw == 0:
                    lbl.configure(fg="#0066CC")
                elif raw & ((1 << ALARM_BIT_DOSE_HI) | (1 << ALARM_BIT_DOSE_LO)):
                    lbl.configure(fg="#CC3300")
                else:
                    lbl.configure(fg="#CC0000")
                self.alarm_detail_lbl.configure(
                    text=format_alarm_status_detail(raw),
                    fg="#CC0000" if raw else "#333333",
                )
        else:
            lbl.configure(text=str(raw))

    def _on_serial_error(self, msg: str) -> None:
        self._log(f"ERR {msg}")
        messagebox.showerror("串口错误", msg)
        self.disconnect()

    def _on_serial_closed(self) -> None:
        if self.connected:
            self.disconnect()

    def _log(self, line: str) -> None:
        self.log_text.configure(state=tk.NORMAL)
        self.log_text.insert(tk.END, line + "\n")
        self.log_text.see(tk.END)
        self.log_text.configure(state=tk.DISABLED)

    def clear_log(self) -> None:
        self.log_text.configure(state=tk.NORMAL)
        self.log_text.delete("1.0", tk.END)
        self.log_text.configure(state=tk.DISABLED)

    def _on_close(self) -> None:
        self._cancel_pending()
        self.worker.stop()
        self.destroy()


def main() -> None:
    app = FactoryApp()
    app.mainloop()


if __name__ == "__main__":
    main()
