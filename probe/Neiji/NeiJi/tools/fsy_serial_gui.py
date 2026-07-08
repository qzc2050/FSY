#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""NeiJi 工厂生产 — 串口 / TCP + 实时参数 + 生产配置读写。"""
from __future__ import annotations

import csv
import json
import os
import threading
import time
import tkinter as tk
from dataclasses import dataclass
from datetime import datetime
from tkinter import filedialog, messagebox, scrolledtext, ttk
from typing import Callable, Optional

import serial
import serial.tools.list_ports

from fsy_protocol import (
    CFG_MODEL_MAX_LEN,
    CFG_HW_VERSION_MAX_LEN,
    CFG_PRODUCT_NAME_MAX_BYTES,
    CFG_SN_MAX_LEN,
    DEFAULT_TCP_PORT,
    FC_ACTIVE_UPLOAD,
    FC_READ_HOLDING_RESP,
    FC_WRITE_MULTI_RESP,
    FC_WRITE_SINGLE_RESP,
    ALARM_BIT_DOSE_HI,
    ALARM_BIT_DOSE_LO,
    REG_ADDRESS,
    REG_ALARM_ENABLE,
    REG_ALARM_ENABLE_COUNT,
    REG_ALARM_VOLUME,
    REG_CONTROL_BIT2,
    REG_CONTROL_BIT2_COUNT,
    NEIJI_CTRL2_DEFAULT,
    ZJB_PROTOCOL_ADDR,
    ZJB_CTRL2_DEFAULT,
    control_bit2_enables,
    merge_control_bit2,
    merge_zjb_control_bit2,
    zjb_control_bit2_flags,
    REG_DOSE_HI_TH,
    REG_DOSE_LO_TH,
    REG_PRODUCT_MODEL,
    REG_PRODUCT_MODEL_COUNT,
    REG_PRODUCT_NAME,
    REG_PRODUCT_NAME_COUNT,
    REG_CURRENT_IP,
    REG_CURRENT_IP_COUNT,
    REG_STATIC_IP,
    REG_STATIC_IP_COUNT,
    REG_DHCP_ENABLE,
    REG_GEIGER_SENS,
    REG_GEIGER_SEC_CPS,
    REG_GEIGER_SEC_CPS_COUNT,
    REG_EWMA_THRESHOLD_CPS,
    REG_EWMA_THRESHOLD_DELTA,
    REG_EWMA_ALPHA_LOW,
    REG_EWMA_ALPHA_HIGH,
    REG_EWMA_BOOST_DURATION,
    REG_RATE_LIMIT,
    REG_GEIGER_BACKGROUND_CPM,
    REG_GEIGER_DEAD_TIME_US,
    REG_GEIGER_PARAM_COUNT,
    REG_HW_VERSION,
    REG_HW_VERSION_COUNT,
    REG_LANGUAGE,
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
    ipv4_to_reg_values,
    reg_payload_to_time,
    reg_payload_to_u32,
    u32_to_reg_values,
)
from fsy_tcp_worker import TcpWorker

APP_TITLE = "NeiJi 生产测试 — 串口 / TCP"
DEFAULT_BAUD = 115200
CONFIG_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".last_conn.json")
LEGACY_PORT_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".last_port.json")

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

CPS_CSV_ROOT = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "geiger_cps_logs"
)


def cps_csv_path_for_time(base_dir: str, when: datetime | None = None) -> str:
    """按 日期/小时 分目录，每小时一个 cps.csv。"""
    when = when or datetime.now()
    hour_dir = os.path.join(
        base_dir,
        when.strftime("%Y-%m-%d"),
        when.strftime("%H"),
    )
    return os.path.join(hour_dir, "cps.csv")
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


def load_conn_config() -> dict:
    default = {
        "mode": "serial",
        "port": "",
        "baud": DEFAULT_BAUD,
        "host": "192.168.2.100",
        "tcp_port": DEFAULT_TCP_PORT,
    }
    try:
        if os.path.isfile(CONFIG_FILE):
            with open(CONFIG_FILE, "r", encoding="utf-8") as f:
                data = json.load(f)
            if isinstance(data, dict):
                default.update({k: data[k] for k in default if k in data})
        elif os.path.isfile(LEGACY_PORT_FILE):
            with open(LEGACY_PORT_FILE, "r", encoding="utf-8") as f:
                data = json.load(f)
            if isinstance(data, dict) and data.get("port"):
                default["port"] = str(data["port"])
    except (OSError, json.JSONDecodeError, TypeError):
        pass
    try:
        default["baud"] = int(default["baud"])
        default["tcp_port"] = int(default["tcp_port"])
    except (TypeError, ValueError):
        default["baud"] = DEFAULT_BAUD
        default["tcp_port"] = DEFAULT_TCP_PORT
    return default


def save_conn_config(
    mode: str,
    *,
    port: str = "",
    baud: int = DEFAULT_BAUD,
    host: str = "",
    tcp_port: int = DEFAULT_TCP_PORT,
) -> None:
    try:
        with open(CONFIG_FILE, "w", encoding="utf-8") as f:
            json.dump(
                {
                    "mode": mode,
                    "port": port,
                    "baud": baud,
                    "host": host,
                    "tcp_port": tcp_port,
                },
                f,
            )
    except OSError:
        pass


def load_last_port() -> str:
    return str(load_conn_config().get("port", ""))


def save_last_port(port: str) -> None:
    cfg = load_conn_config()
    save_conn_config(
        "serial",
        port=port,
        baud=int(cfg.get("baud", DEFAULT_BAUD)),
        host=str(cfg.get("host", "192.168.2.100")),
        tcp_port=int(cfg.get("tcp_port", DEFAULT_TCP_PORT)),
    )


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
    config_mode: bool = True


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
        self.geometry("1080x1080")
        self.minsize(860, 640)

        cfg = load_conn_config()
        self._conn_mode = tk.StringVar(value=str(cfg.get("mode", "serial")))

        link_cb = {
            "on_data": lambda d: self.after(0, lambda: self._on_link_data(d)),
            "on_error": lambda e: self.after(0, lambda: self._on_link_error(e)),
            "on_closed": lambda: self.after(0, self._on_link_closed),
        }
        self.serial_worker = SerialWorker(**link_cb)
        self.tcp_worker = TcpWorker(**link_cb)
        self.worker: SerialWorker | TcpWorker = self.serial_worker
        self.connected = False
        self._link_kind = "serial"
        self.frame_count = 0
        self.value_labels: dict[int, tk.Label] = {}
        self._pending: Optional[PendingRequest] = None
        self._poll_after_id: Optional[str] = None
        self._cfg_busy = False
        self._cps_poll_after_id: Optional[str] = None
        self._cps_csv_base_dir = CPS_CSV_ROOT
        self._cps_csv_path = cps_csv_path_for_time(self._cps_csv_base_dir)
        self.scanner = FrameScanner(slave_filter=None)

        self._build_ui()
        self.refresh_ports()
        self._apply_conn_mode_ui()
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    def _build_ui(self) -> None:
        conn_bar = ttk.Frame(self, padding=(8, 8, 8, 0))
        conn_bar.pack(fill=tk.X)

        ttk.Label(conn_bar, text="连接:").pack(side=tk.LEFT, padx=(0, 6))
        self.rb_serial = ttk.Radiobutton(
            conn_bar,
            text="串口",
            variable=self._conn_mode,
            value="serial",
            command=self._apply_conn_mode_ui,
        )
        self.rb_serial.pack(side=tk.LEFT)
        self.rb_tcp = ttk.Radiobutton(
            conn_bar,
            text="TCP",
            variable=self._conn_mode,
            value="tcp",
            command=self._apply_conn_mode_ui,
        )
        self.rb_tcp.pack(side=tk.LEFT, padx=(8, 0))

        top = ttk.Frame(self, padding=8)
        top.pack(fill=tk.X)

        cfg = load_conn_config()

        self.serial_frame = ttk.Frame(top)
        self.serial_frame.pack(side=tk.LEFT, fill=tk.X, expand=True)

        ttk.Label(self.serial_frame, text="串口:").pack(side=tk.LEFT, padx=(0, 4))
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(
            self.serial_frame, textvariable=self.port_var, width=34, state="readonly"
        )
        self.port_combo.pack(side=tk.LEFT, padx=4)
        self.btn_refresh_ports = ttk.Button(
            self.serial_frame, text="刷新", command=self.refresh_ports, width=6
        )
        self.btn_refresh_ports.pack(side=tk.LEFT, padx=4)
        ttk.Label(self.serial_frame, text="波特率:").pack(side=tk.LEFT, padx=(12, 4))
        self.baud_var = tk.StringVar(value=str(cfg.get("baud", DEFAULT_BAUD)))
        self.baud_combo = ttk.Combobox(
            self.serial_frame,
            textvariable=self.baud_var,
            values=["9600", "115200", "921600"],
            width=8,
            state="readonly",
        )
        self.baud_combo.pack(side=tk.LEFT, padx=4)

        self.tcp_frame = ttk.Frame(top)
        ttk.Label(self.tcp_frame, text="IP:").pack(side=tk.LEFT, padx=(0, 4))
        self.tcp_host_var = tk.StringVar(value=str(cfg.get("host", "192.168.2.100")))
        self.tcp_host_entry = ttk.Entry(self.tcp_frame, textvariable=self.tcp_host_var, width=18)
        self.tcp_host_entry.pack(side=tk.LEFT, padx=4)
        ttk.Label(self.tcp_frame, text="端口:").pack(side=tk.LEFT, padx=(12, 4))
        self.tcp_port_var = tk.StringVar(value=str(cfg.get("tcp_port", DEFAULT_TCP_PORT)))
        self.tcp_port_entry = ttk.Entry(self.tcp_frame, textvariable=self.tcp_port_var, width=8)
        self.tcp_port_entry.pack(side=tk.LEFT, padx=4)
        ttk.Label(self.tcp_frame, text=f"(默认 {DEFAULT_TCP_PORT})", foreground="#666").pack(
            side=tk.LEFT, padx=(4, 0)
        )

        ctrl = ttk.Frame(top)
        ctrl.pack(side=tk.RIGHT)
        ttk.Label(ctrl, text="从机地址:").pack(side=tk.LEFT, padx=(0, 4))
        self.slave_addr_var = tk.StringVar(value="1")
        self.slave_spin = ttk.Spinbox(
            ctrl,
            from_=1,
            to=247,
            textvariable=self.slave_addr_var,
            width=5,
        )
        self.slave_spin.pack(side=tk.LEFT, padx=4)
        self.slave_addr_var.trace_add("write", lambda *_: self._update_cfg_target_hint())
        self.btn_connect = ttk.Button(
            ctrl, text="连接", command=self.toggle_connect, width=10
        )
        self.btn_connect.pack(side=tk.LEFT, padx=(12, 4))
        ttk.Button(ctrl, text="清空日志", command=self.clear_log, width=8).pack(
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
        geiger_tab = ttk.Frame(notebook, padding=12)
        notebook.add(rt_tab, text="实时参数")
        notebook.add(cfg_tab, text="生产配置")
        notebook.add(geiger_tab, text="盖革算法")

        self._build_rt_tab(rt_tab)
        self._build_cfg_tab(cfg_tab)
        self._build_geiger_tab(geiger_tab)

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

    def _cfg_write_btn(self, form: ttk.Frame, row: int, command: Callable[[], None], text: str = "写入") -> None:
        ttk.Button(form, text=text, width=8, command=command).grid(
            row=row, column=3, sticky=tk.W, padx=4, pady=6
        )

    def _build_cfg_tab(self, parent: ttk.Frame) -> None:
        form = ttk.Frame(parent)
        form.pack(fill=tk.X)
        form.columnconfigure(1, weight=1)

        self._control_bit2_raw = NEIJI_CTRL2_DEFAULT

        row = 0
        self._cfg_mode_hint = ttk.Label(
            form,
            text="",
            foreground="#0066CC",
            wraplength=820,
        )
        self._cfg_mode_hint.grid(row=row, column=0, columnspan=4, sticky=tk.W, pady=(0, 8))
        row += 1

        ttk.Label(form, text="序列号 (reg 86):").grid(row=row, column=0, sticky=tk.W, pady=6)
        self.sn_var = tk.StringVar()
        ttk.Entry(form, textvariable=self.sn_var, width=28).grid(
            row=row, column=1, sticky=tk.W, padx=8, pady=6
        )
        ttk.Label(form, text=f"最多 {CFG_SN_MAX_LEN} 字符", foreground="#666").grid(
            row=row, column=2, sticky=tk.W
        )
        self._cfg_write_btn(form, row, self.write_sn_only)
        row += 1

        ttk.Label(form, text="产品名称 (reg 146):").grid(row=row, column=0, sticky=tk.W, pady=6)
        self.name_var = tk.StringVar(value="雷沃-探测器")
        ttk.Entry(form, textvariable=self.name_var, width=28).grid(
            row=row, column=1, sticky=tk.W, padx=8, pady=6
        )
        ttk.Label(
            form,
            text=f"UTF-8，最多 {CFG_PRODUCT_NAME_MAX_BYTES} 字节",
            foreground="#666",
        ).grid(row=row, column=2, sticky=tk.W)
        self._cfg_write_btn(form, row, self.write_name_only)
        row += 1

        ttk.Label(form, text="产品型号 (reg 130):").grid(row=row, column=0, sticky=tk.W, pady=6)
        self.model_var = tk.StringVar()
        ttk.Entry(form, textvariable=self.model_var, width=28).grid(
            row=row, column=1, sticky=tk.W, padx=8, pady=6
        )
        ttk.Label(form, text=f"最多 {CFG_MODEL_MAX_LEN} 字符", foreground="#666").grid(
            row=row, column=2, sticky=tk.W
        )
        self._cfg_write_btn(form, row, self.write_model_only)
        row += 1

        ttk.Label(form, text="硬件版本 (reg 180):").grid(row=row, column=0, sticky=tk.W, pady=6)
        self.hw_version_var = tk.StringVar(value="HW1314520168")
        ttk.Entry(form, textvariable=self.hw_version_var, width=28).grid(
            row=row, column=1, sticky=tk.W, padx=8, pady=6
        )
        ttk.Label(form, text=f"ASCII，最多 {CFG_HW_VERSION_MAX_LEN} 字符", foreground="#666").grid(
            row=row, column=2, sticky=tk.W
        )
        self._cfg_write_btn(form, row, self.write_hw_version_only)
        row += 1

        ttk.Label(form, text="协议地址 (reg 121):").grid(row=row, column=0, sticky=tk.W, pady=6)
        self.dev_addr_var = tk.StringVar(value="1")
        ttk.Spinbox(
            form,
            from_=1,
            to=247,
            textvariable=self.dev_addr_var,
            width=8,
        ).grid(row=row, column=1, sticky=tk.W, padx=8, pady=6)
        ttk.Label(form, text="写入后请同步修改顶部「从机地址」", foreground="#666").grid(
            row=row, column=2, sticky=tk.W
        )
        self._cfg_write_btn(form, row, self.write_addr_only)
        row += 1

        ttk.Label(form, text="当前 IP (reg 192):").grid(row=row, column=0, sticky=tk.W, pady=6)
        self.current_ip_var = tk.StringVar(value="—")
        ttk.Entry(
            form, textvariable=self.current_ip_var, width=28, state="readonly"
        ).grid(row=row, column=1, sticky=tk.W, padx=8, pady=6)
        ttk.Label(form, text="只读；设备实时在用 IP（W5500 SIPR）", foreground="#666").grid(
            row=row, column=2, sticky=tk.W
        )
        row += 1

        ttk.Label(form, text="静态 IP (reg 138):").grid(row=row, column=0, sticky=tk.W, pady=6)
        self.static_ip_var = tk.StringVar(value="192.168.16.12")
        ttk.Entry(form, textvariable=self.static_ip_var, width=28).grid(
            row=row, column=1, sticky=tk.W, padx=8, pady=6
        )
        ttk.Label(form, text="写入 W25Q；DHCP 关闭时复位后生效", foreground="#666").grid(
            row=row, column=2, sticky=tk.W
        )
        self._cfg_write_btn(form, row, self.write_ip_only, text="写静态IP")
        row += 1

        ttk.Label(form, text="DHCP 使能 (reg 170):").grid(row=row, column=0, sticky=tk.W, pady=6)
        self.dhcp_enable_var = tk.IntVar(value=1)
        ttk.Checkbutton(
            form,
            text="启用 DHCP",
            variable=self.dhcp_enable_var,
        ).grid(row=row, column=1, sticky=tk.W, padx=8, pady=6)
        ttk.Label(form, text="取消勾选=静态 IP；写入后复位生效", foreground="#666").grid(
            row=row, column=2, sticky=tk.W
        )
        self._cfg_write_btn(form, row, self.write_dhcp_only, text="写模式")
        row += 1

        ttk.Label(form, text="剂量率上阈值 (reg 50):").grid(row=row, column=0, sticky=tk.W, pady=6)
        self.dose_hi_var = tk.StringVar(value="10000.00")
        ttk.Entry(form, textvariable=self.dose_hi_var, width=28).grid(
            row=row, column=1, sticky=tk.W, padx=8, pady=6
        )
        ttk.Label(form, text="单位 μSv/h，协议值=×100", foreground="#666").grid(
            row=row, column=2, sticky=tk.W
        )
        self._cfg_write_btn(form, row, self.write_dose_hi_only)
        row += 1

        ttk.Label(form, text="剂量率下阈值 (reg 52):").grid(row=row, column=0, sticky=tk.W, pady=6)
        self.dose_lo_var = tk.StringVar(value="0.00")
        ttk.Entry(form, textvariable=self.dose_lo_var, width=28).grid(
            row=row, column=1, sticky=tk.W, padx=8, pady=6
        )
        ttk.Label(form, text="0 表示下阈值不触发", foreground="#666").grid(
            row=row, column=2, sticky=tk.W
        )
        self._cfg_write_btn(form, row, self.write_dose_lo_only)
        row += 1

        ttk.Label(form, text="报警使能 (reg 82):").grid(row=row, column=0, sticky=tk.W, pady=6)
        alarm_row = ttk.Frame(form)
        alarm_row.grid(row=row, column=1, sticky=tk.W, padx=8, pady=6)
        self.alarm_hi_var = tk.IntVar(value=1)
        self.alarm_lo_var = tk.IntVar(value=1)
        ttk.Checkbutton(alarm_row, text="bit0 上阈值", variable=self.alarm_hi_var).pack(
            side=tk.LEFT, padx=(0, 12)
        )
        ttk.Checkbutton(alarm_row, text="bit1 下阈值", variable=self.alarm_lo_var).pack(
            side=tk.LEFT
        )
        ttk.Label(form, text="勾选=允许报警", foreground="#666").grid(
            row=row, column=2, sticky=tk.W
        )
        self._cfg_write_btn(form, row, self.write_alarm_enable_only)
        row += 1

        ttk.Label(form, text="报警音量 (reg 122):").grid(row=row, column=0, sticky=tk.W, pady=6)
        vol_row = ttk.Frame(form)
        vol_row.grid(row=row, column=1, sticky=tk.W, padx=8, pady=6)
        self.alarm_volume_var = tk.IntVar(value=50)
        ttk.Spinbox(
            vol_row,
            from_=0,
            to=100,
            textvariable=self.alarm_volume_var,
            width=8,
        ).pack(side=tk.LEFT)
        ttk.Label(vol_row, text="  (0~100)").pack(side=tk.LEFT)
        ttk.Label(form, text="与 App 探头管理音量一致", foreground="#666").grid(
            row=row, column=2, sticky=tk.W
        )
        self._cfg_write_btn(form, row, self.write_volume_only)
        row += 1

        ttk.Label(form, text="reg123 控制 (bit):").grid(row=row, column=0, sticky=tk.W, pady=6)
        ctrl_row = ttk.Frame(form)
        ctrl_row.grid(row=row, column=1, sticky=tk.W, padx=8, pady=6)
        self.lora_on_var = tk.IntVar(value=0)
        self.screen_on_var = tk.IntVar(value=1)
        self.light_on_var = tk.IntVar(value=1)
        self._lora_cb = ttk.Checkbutton(
            ctrl_row, text="LoRa bit9", variable=self.lora_on_var
        )
        self._lora_cb.pack(side=tk.LEFT, padx=(0, 10))
        self._screen_cb = ttk.Checkbutton(
            ctrl_row, text="屏 bit14", variable=self.screen_on_var
        )
        self._screen_cb.pack(side=tk.LEFT, padx=(0, 10))
        ttk.Checkbutton(ctrl_row, text="光 bit13", variable=self.light_on_var).pack(
            side=tk.LEFT
        )
        self.ctrl2_hex_var = tk.StringVar(value="—")
        ttk.Label(form, textvariable=self.ctrl2_hex_var, foreground="#666").grid(
            row=row, column=2, sticky=tk.W
        )
        self._cfg_write_btn(form, row, self.write_control_bit2_only)
        row += 1
        self._ctrl2_hint = ttk.Label(form, text="", foreground="#666")
        self._ctrl2_hint.grid(row=row, column=0, columnspan=3, sticky=tk.W, pady=(0, 6))
        row += 1
        self._update_cfg_target_hint()

        ttk.Label(form, text="RTC 时间 (reg 94):").grid(row=row, column=0, sticky=tk.W, pady=6)
        self.rtc_time_var = tk.StringVar(value="—")
        ttk.Entry(
            form, textvariable=self.rtc_time_var, width=28, state="readonly"
        ).grid(row=row, column=1, sticky=tk.W, padx=8, pady=6)
        ttk.Label(form, text="读 PCF85063，写入不落 W25Q", foreground="#666").grid(
            row=row, column=2, sticky=tk.W
        )
        self._cfg_write_btn(form, row, self.sync_current_time, text="同步时间")
        row += 1

        ttk.Label(form, text="软件版本 (reg 98):").grid(row=row, column=0, sticky=tk.W, pady=6)
        self.sw_version_var = tk.StringVar(value="—")
        ttk.Entry(
            form, textvariable=self.sw_version_var, width=28, state="readonly"
        ).grid(row=row, column=1, sticky=tk.W, padx=8, pady=6)
        ttk.Label(form, text="只读，固件编译期版本", foreground="#666").grid(
            row=row, column=2, sticky=tk.W
        )
        row += 1

        ttk.Label(form, text="语言 (reg 174):").grid(row=row, column=0, sticky=tk.W, pady=6)
        self.language_var = tk.StringVar(value="0 中文")
        ttk.Combobox(
            form,
            textvariable=self.language_var,
            values=("0 中文", "1 English"),
            width=26,
            state="readonly",
        ).grid(row=row, column=1, sticky=tk.W, padx=8, pady=6)
        ttk.Label(form, text="0=中文，1=英文，落 W25Q", foreground="#666").grid(
            row=row, column=2, sticky=tk.W
        )
        self._cfg_write_btn(form, row, self.write_language_only)
        row += 1

        btn_row = ttk.Frame(parent)
        btn_row.pack(fill=tk.X, pady=(16, 8))
        ttk.Button(btn_row, text="读取配置", command=self.read_factory_cfg, width=12).pack(
            side=tk.LEFT, padx=(0, 8)
        )
        ttk.Button(btn_row, text="写入全部", command=self.write_factory_cfg, width=12).pack(
            side=tk.LEFT, padx=8
        )

        hint = ttk.Label(
            parent,
            text="各字段右侧可单独写入；「读取配置 / 写入全部」一次操作全部项。"
            "配置写入后落 W25Q，断电保持。",
            foreground="#444",
            wraplength=820,
        )
        hint.pack(anchor=tk.W, pady=(8, 0))

    def _language_reg_value(self) -> int | None:
        text = self.language_var.get().strip()
        if text.startswith("1"):
            return 1
        if text.startswith("0"):
            return 0
        messagebox.showwarning("提示", "语言无效，请选择 0 中文 或 1 English")
        return None

    def _set_language_from_reg(self, value: int) -> None:
        self.language_var.set("1 English" if value else "0 中文")

    def _is_zjb(self) -> bool:
        return self._slave_addr() == ZJB_PROTOCOL_ADDR

    def _update_cfg_target_hint(self) -> None:
        if not hasattr(self, "_cfg_mode_hint"):
            return
        if self._is_zjb():
            self._cfg_mode_hint.config(
                text="当前目标：zjb 转接板 (239)。可读写 reg123 bit9 LoRa / bit13 光 / bit14 屏"
            )
            if hasattr(self, "_lora_cb"):
                if not self._lora_cb.winfo_ismapped():
                    self._lora_cb.pack(side=tk.LEFT, padx=(0, 10), before=self._screen_cb)
            if hasattr(self, "_ctrl2_hint"):
                self._ctrl2_hint.config(
                    text="zjb reg123：bit9=LoRa 电源+桥接总开关（写后立即生效并落 Flash）"
                )
        else:
            addr = self._slave_addr()
            self._cfg_mode_hint.config(
                text=f"当前目标：Neiji 内机 (从机地址 {addr}，应等于 reg121)。"
                "生产配置连内机串口，读写 SN / 阈值 / 屏光 / LoRa 等"
            )
            if hasattr(self, "_lora_cb"):
                if not self._lora_cb.winfo_ismapped():
                    self._lora_cb.pack(side=tk.LEFT, padx=(0, 10), before=self._screen_cb)
            if hasattr(self, "_ctrl2_hint"):
                self._ctrl2_hint.config(
                    text="Neiji reg123：bit9=LoRa 协议输出开关（无电源脚，"
                    "关=停发 0x23/停收/停 Poll）；bit13 光、bit14 屏"
                )

    def _control_bit2_from_ui(self) -> int:
        light_on = bool(self.light_on_var.get())
        screen_on = bool(self.screen_on_var.get())
        lora_on = bool(self.lora_on_var.get())
        return merge_zjb_control_bit2(
            self._control_bit2_raw, lora_on, screen_on, light_on
        )

    def _set_control_bit2_from_reg(self, value: int) -> None:
        self._control_bit2_raw = value & 0xFFFFFFFF
        self.ctrl2_hex_var.set(f"0x{self._control_bit2_raw:08X}")
        lora_on, light_on, screen_on = zjb_control_bit2_flags(value)
        self.lora_on_var.set(1 if lora_on else 0)
        self.light_on_var.set(1 if light_on else 0)
        self.screen_on_var.set(1 if screen_on else 0)

    def _alarm_enable_from_ui(self) -> int:
        alarm_enable = 0
        if self.alarm_hi_var.get():
            alarm_enable |= 1 << ALARM_BIT_DOSE_HI
        if self.alarm_lo_var.get():
            alarm_enable |= 1 << ALARM_BIT_DOSE_LO
        return alarm_enable

    def _parse_dose_hi_x100(self) -> int | None:
        try:
            return max(0, int(float(self.dose_hi_var.get()) * 100.0))
        except ValueError:
            messagebox.showwarning("提示", "剂量率上阈值无效")
            return None

    def _parse_dose_lo_x100(self) -> int | None:
        try:
            return max(0, int(float(self.dose_lo_var.get()) * 100.0))
        except ValueError:
            messagebox.showwarning("提示", "剂量率下阈值无效")
            return None

    def _parse_alarm_volume(self) -> int | None:
        try:
            return max(0, min(100, int(self.alarm_volume_var.get())))
        except (ValueError, tk.TclError):
            messagebox.showwarning("提示", "报警音量无效（0~100）")
            return None

    def _build_geiger_tab(self, parent: ttk.Frame) -> None:
        poll_frame = ttk.LabelFrame(
            parent,
            text="每秒计数 CPS（reg 190，0x03 轮询；串口 / TCP 均可）",
            padding=12,
        )
        poll_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 8))

        ctrl = ttk.Frame(poll_frame)
        ctrl.pack(fill=tk.X)

        self.cps_poll_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(
            ctrl,
            text="启用每秒读取 reg190",
            variable=self.cps_poll_var,
            command=self._on_cps_poll_toggled,
        ).pack(side=tk.LEFT)

        ttk.Button(ctrl, text="立即读一次", command=self.read_geiger_cps_once, width=12).pack(
            side=tk.LEFT, padx=(12, 4)
        )
        ttk.Button(ctrl, text="选择 CSV 目录", command=self._pick_cps_csv_dir, width=14).pack(
            side=tk.LEFT, padx=4
        )
        ttk.Button(ctrl, text="清空列表", command=self._clear_cps_table, width=10).pack(
            side=tk.LEFT, padx=4
        )

        info = ttk.Frame(poll_frame)
        info.pack(fill=tk.X, pady=(10, 6))
        ttk.Label(info, text="当前 CPS:").pack(side=tk.LEFT)
        self.cps_current_lbl = tk.Label(
            info, text="—", font=("Microsoft YaHei", 22, "bold"), fg="#0066CC"
        )
        self.cps_current_lbl.pack(side=tk.LEFT, padx=(8, 24))
        ttk.Label(info, text="最近采样:").pack(side=tk.LEFT)
        self.cps_time_lbl = ttk.Label(info, text="—")
        self.cps_time_lbl.pack(side=tk.LEFT, padx=(8, 0))

        self.cps_csv_path_var = tk.StringVar(value=self._cps_csv_path)
        ttk.Label(poll_frame, textvariable=self.cps_csv_path_var, foreground="#666").pack(
            anchor=tk.W, pady=(0, 6)
        )

        table_wrap = ttk.Frame(poll_frame)
        table_wrap.pack(fill=tk.BOTH, expand=True)
        cols = ("time", "cps")
        self.cps_tree = ttk.Treeview(
            table_wrap, columns=cols, show="headings", height=12
        )
        self.cps_tree.heading("time", text="时间")
        self.cps_tree.heading("cps", text="CPS")
        self.cps_tree.column("time", width=200, anchor=tk.W)
        self.cps_tree.column("cps", width=100, anchor=tk.E)
        scroll = ttk.Scrollbar(table_wrap, orient=tk.VERTICAL, command=self.cps_tree.yview)
        self.cps_tree.configure(yscrollcommand=scroll.set)
        self.cps_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scroll.pack(side=tk.RIGHT, fill=tk.Y)

        form = ttk.LabelFrame(parent, text="盖革 / EWMA（reg 154–169、171–172，每项 uint32 占 2 reg）", padding=12)
        form.pack(fill=tk.X, pady=(0, 0))

        rows = [
            ("灵敏度 (reg 154)", "geiger_sens_var", "600.00", "cpm/(μSv/h)，协议值×100"),
            ("EWMA threshold_cps (156)", "ewma_cps_var", "200", "CPS"),
            ("EWMA threshold_delta (158)", "ewma_delta_var", "100", ""),
            ("EWMA alpha_low (160)", "ewma_alpha_lo_var", "0.01", "协议值×100"),
            ("EWMA alpha_high (162)", "ewma_alpha_hi_var", "0.35", "协议值×100"),
            ("EWMA boost_duration (164)", "ewma_boost_var", "20", "秒"),
            ("剂量率量程上限 (166)", "rate_limit_var", "10000.00", "μSv/h，协议值×100"),
            ("盖革本底 (168)", "background_cpm_var", "20", "CPM，参与剂量率前先扣除"),
            ("盖革死时间 (171)", "dead_time_us_var", "200.00", "μs，协议值×100；0=不修正"),
        ]
        self.geiger_sens_var = tk.StringVar(value="600.00")
        self.ewma_cps_var = tk.StringVar(value="200")
        self.ewma_delta_var = tk.StringVar(value="100")
        self.ewma_alpha_lo_var = tk.StringVar(value="0.01")
        self.ewma_alpha_hi_var = tk.StringVar(value="0.35")
        self.ewma_boost_var = tk.StringVar(value="20")
        self.rate_limit_var = tk.StringVar(value="10000.00")
        self.background_cpm_var = tk.StringVar(value="20")
        self.dead_time_us_var = tk.StringVar(value="200.00")

        for i, (label, attr, _default, hint) in enumerate(rows):
            ttk.Label(form, text=label).grid(row=i, column=0, sticky=tk.W, pady=4)
            var = getattr(self, attr)
            ttk.Entry(form, textvariable=var, width=20).grid(
                row=i, column=1, sticky=tk.W, padx=8, pady=4
            )
            if hint:
                ttk.Label(form, text=hint, foreground="#666").grid(
                    row=i, column=2, sticky=tk.W, pady=4
                )

        btn_row = ttk.Frame(parent)
        btn_row.pack(fill=tk.X, pady=(12, 0))
        ttk.Button(btn_row, text="读取盖革参数", command=self.read_geiger_cfg, width=14).pack(
            side=tk.LEFT, padx=(0, 8)
        )
        ttk.Button(btn_row, text="写入盖革参数", command=self.write_geiger_cfg, width=14).pack(
            side=tk.LEFT
        )

    def _pick_cps_csv_dir(self) -> None:
        path = filedialog.askdirectory(
            title="CPS 记录根目录（其下按 日期/小时 分文件夹）",
            initialdir=self._cps_csv_base_dir,
        )
        if path:
            self._cps_csv_base_dir = path
            self._cps_csv_path = cps_csv_path_for_time(self._cps_csv_base_dir)
            self.cps_csv_path_var.set(self._cps_csv_path)

    def _resolve_cps_csv_path(self, time_text: str) -> str:
        try:
            when = datetime.strptime(time_text, "%Y-%m-%d %H:%M:%S")
        except ValueError:
            when = datetime.now()
        return cps_csv_path_for_time(self._cps_csv_base_dir, when)

    def _clear_cps_table(self) -> None:
        for item in self.cps_tree.get_children():
            self.cps_tree.delete(item)

    def _ensure_cps_csv_header(self, path: str | None = None) -> None:
        path = path or cps_csv_path_for_time(self._cps_csv_base_dir)
        self._cps_csv_path = path
        self.cps_csv_path_var.set(path)
        if os.path.isfile(path) and os.path.getsize(path) > 0:
            return
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        with open(path, "w", encoding="utf-8-sig", newline="") as f:
            csv.writer(f).writerow(["time", "cps"])

    def _append_cps_csv(self, time_text: str, cps: int) -> None:
        try:
            path = self._resolve_cps_csv_path(time_text)
            if path != self._cps_csv_path:
                self._cps_csv_path = path
                self.cps_csv_path_var.set(path)
            if not os.path.isfile(path) or os.path.getsize(path) == 0:
                self._ensure_cps_csv_header(path)
            with open(path, "a", encoding="utf-8-sig", newline="") as f:
                csv.writer(f).writerow([time_text, cps])
        except OSError as exc:
            self._log(f"WARN CPS CSV 写入失败: {exc}")

    def _append_cps_row(self, time_text: str, cps: int) -> None:
        self.cps_current_lbl.configure(text=str(cps))
        self.cps_time_lbl.configure(text=time_text)
        self.cps_tree.insert("", 0, values=(time_text, cps))
        children = self.cps_tree.get_children()
        if len(children) > 500:
            for item in children[500:]:
                self.cps_tree.delete(item)
        self._append_cps_csv(time_text, cps)

    def _on_cps_poll_toggled(self) -> None:
        if self.cps_poll_var.get():
            if not self.connected:
                self.cps_poll_var.set(False)
                messagebox.showinfo("提示", "请先连接（串口或 TCP）")
                return
            self._ensure_cps_csv_header()
            self._schedule_cps_poll(delay_ms=0)
        else:
            self._stop_cps_poll()

    def _stop_cps_poll(self) -> None:
        if self._cps_poll_after_id is not None:
            try:
                self.after_cancel(self._cps_poll_after_id)
            except tk.TclError:
                pass
            self._cps_poll_after_id = None

    def _schedule_cps_poll(self, delay_ms: int = 1000) -> None:
        self._stop_cps_poll()
        if not self.cps_poll_var.get() or not self.connected:
            return
        self._cps_poll_after_id = self.after(delay_ms, self._cps_poll_tick)

    def _cps_poll_tick(self) -> None:
        self._cps_poll_after_id = None
        if not self.cps_poll_var.get() or not self.connected:
            return
        if self._pending is not None:
            self._schedule_cps_poll(delay_ms=200)
            return
        self.read_geiger_cps_once(schedule_next=self.cps_poll_var.get())

    def read_geiger_cps_once(self, schedule_next: bool = False) -> None:
        if not self._require_connected():
            return

        def on_ok(pf: ParsedFrame) -> None:
            cps = reg_payload_to_u32(pf.payload)
            time_text = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            self._append_cps_row(time_text, cps)
            if schedule_next:
                self._schedule_cps_poll(delay_ms=1000)

        def on_fail(msg: str) -> None:
            self.cps_time_lbl.configure(text=f"读失败")
            self._log(f"WARN reg190 CPS 读失败: {msg}")
            if schedule_next:
                self._schedule_cps_poll(delay_ms=1000)

        req = build_read_holding(
            self._slave_addr(), REG_GEIGER_SEC_CPS, REG_GEIGER_SEC_CPS_COUNT
        )
        self._begin_request(
            req,
            FC_READ_HOLDING_RESP,
            on_ok,
            on_fail,
            expect_reg=REG_GEIGER_SEC_CPS,
            config_mode=False,
            retries=1,
            timeout=2.0,
        )

    def _geiger_form_u32_values(self) -> list[tuple[int, list[int]]] | None:
        try:
            items = [
                (REG_GEIGER_SENS, int(float(self.geiger_sens_var.get()) * 100.0)),
                (REG_EWMA_THRESHOLD_CPS, int(self.ewma_cps_var.get())),
                (REG_EWMA_THRESHOLD_DELTA, int(self.ewma_delta_var.get())),
                (REG_EWMA_ALPHA_LOW, int(float(self.ewma_alpha_lo_var.get()) * 100.0)),
                (REG_EWMA_ALPHA_HIGH, int(float(self.ewma_alpha_hi_var.get()) * 100.0)),
                (REG_EWMA_BOOST_DURATION, int(self.ewma_boost_var.get())),
                (REG_RATE_LIMIT, int(float(self.rate_limit_var.get()) * 100.0)),
                (REG_GEIGER_BACKGROUND_CPM, int(self.background_cpm_var.get())),
                (REG_GEIGER_DEAD_TIME_US, int(float(self.dead_time_us_var.get()) * 100.0)),
            ]
        except ValueError:
            messagebox.showwarning("提示", "盖革参数格式无效")
            return None

        out: list[tuple[int, list[int]]] = []
        for reg, val in items:
            if val < 0:
                messagebox.showwarning("提示", "盖革参数不能为负")
                return None
            out.append((reg, u32_to_reg_values(val)))
        return out

    def read_geiger_cfg(self) -> None:
        if not self._require_connected():
            return

        results: dict[str, int] = {}
        chain = [
            (REG_GEIGER_SENS, "sens"),
            (REG_EWMA_THRESHOLD_CPS, "cps"),
            (REG_EWMA_THRESHOLD_DELTA, "delta"),
            (REG_EWMA_ALPHA_LOW, "alpha_lo"),
            (REG_EWMA_ALPHA_HIGH, "alpha_hi"),
            (REG_EWMA_BOOST_DURATION, "boost"),
            (REG_RATE_LIMIT, "rate_limit"),
            (REG_GEIGER_BACKGROUND_CPM, "background_cpm"),
            (REG_GEIGER_DEAD_TIME_US, "dead_time_us"),
        ]

        def finish() -> None:
            self.geiger_sens_var.set(f"{results.get('sens', 0) / 100.0:.2f}")
            self.ewma_cps_var.set(str(results.get("cps", 0)))
            self.ewma_delta_var.set(str(results.get("delta", 0)))
            self.ewma_alpha_lo_var.set(f"{results.get('alpha_lo', 0) / 100.0:.2f}")
            self.ewma_alpha_hi_var.set(f"{results.get('alpha_hi', 0) / 100.0:.2f}")
            self.ewma_boost_var.set(str(results.get("boost", 0)))
            self.rate_limit_var.set(f"{results.get('rate_limit', 0) / 100.0:.2f}")
            self.background_cpm_var.set(str(results.get("background_cpm", 20)))
            self.dead_time_us_var.set(f"{results.get('dead_time_us', 0) / 100.0:.2f}")
            self.status_var.set("盖革参数读取完成")
            messagebox.showinfo("完成", "已读取盖革 / EWMA 参数（reg 154–169、171–172）")

        def make_step(idx: int) -> None:
            reg, key = chain[idx]

            def on_ok(pf: ParsedFrame) -> None:
                results[key] = reg_payload_to_u32(pf.payload)
                if idx + 1 < len(chain):
                    make_step(idx + 1)
                else:
                    finish()

            req = build_read_holding(self._slave_addr(), reg, REG_GEIGER_PARAM_COUNT)
            self._begin_request(
                req, FC_READ_HOLDING_RESP, on_ok, self._on_cfg_fail, expect_reg=reg
            )

        make_step(0)

    def write_geiger_cfg(self) -> None:
        if not self._require_connected():
            return
        items = self._geiger_form_u32_values()
        if items is None:
            return

        def make_step(idx: int) -> None:
            reg, values = items[idx]

            def on_ok(_pf: ParsedFrame) -> None:
                if idx + 1 < len(items):
                    make_step(idx + 1)
                else:
                    messagebox.showinfo("完成", "盖革 / EWMA 参数已写入 W25Q（含 reg 171 死时间）")

            req = build_write_multi(self._slave_addr(), reg, values)
            self._begin_request(
                req, FC_WRITE_MULTI_RESP, on_ok, self._on_cfg_fail, expect_reg=reg
            )

        make_step(0)

    def _slave_addr(self) -> int:
        try:
            addr = int(self.slave_addr_var.get())
        except ValueError:
            addr = 1
        return max(1, min(247, addr))

    def _require_connected(self) -> bool:
        if not self.connected:
            messagebox.showinfo("提示", "请先连接（串口或 TCP）")
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
        if self._pending is not None and self._pending.config_mode:
            self._end_config_tx()
        self._pending = None
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
        config_mode: bool = True,
    ) -> None:
        self._cancel_pending()
        if config_mode:
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
                config_mode=config_mode,
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
                        config_mode=pending.config_mode,
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
        if self._is_zjb():
            self._read_zjb_factory_cfg()
            return

        results: dict[str, str | int] = {}

        def finish_read() -> None:
            self.sn_var.set(str(results.get("sn", "")))
            self.name_var.set(str(results.get("name", "")))
            self.model_var.set(str(results.get("model", "")))
            self.hw_version_var.set(str(results.get("hw_version", "")))
            self.dev_addr_var.set(str(results.get("addr", self.dev_addr_var.get())))
            self.current_ip_var.set(str(results.get("current_ip", "—")))
            self.static_ip_var.set(str(results.get("static_ip", self.static_ip_var.get())))
            self.dhcp_enable_var.set(1 if int(results.get("dhcp_enable", 1)) != 0 else 0)
            self.dose_hi_var.set(str(results.get("dose_hi", self.dose_hi_var.get())))
            self.dose_lo_var.set(str(results.get("dose_lo", self.dose_lo_var.get())))
            alarm_enable = int(results.get("alarm_enable", 0))
            self.alarm_hi_var.set(1 if (alarm_enable & (1 << ALARM_BIT_DOSE_HI)) else 0)
            self.alarm_lo_var.set(1 if (alarm_enable & (1 << ALARM_BIT_DOSE_LO)) else 0)
            self.rtc_time_var.set(str(results.get("rtc_time", "—")))
            self.sw_version_var.set(str(results.get("sw_version", "—")))
            self._set_language_from_reg(int(results.get("language", 0)))
            self.alarm_volume_var.set(int(results.get("volume", 50)))
            self._set_control_bit2_from_reg(int(results.get("control_bit2", NEIJI_CTRL2_DEFAULT)))
            self.status_var.set("配置读取完成")
            messagebox.showinfo(
                "完成",
                "已读取：SN / 名称 / 型号 / 硬件版本 / 地址 / 当前 IP / 阈值 / "
                "DHCP / 报警使能 / 音量 / 屏光 / 时间 / 语言 / 版本",
            )

        def read_sw_version_step() -> None:
            req = build_read_holding(
                self._slave_addr(), REG_SOFTWARE_VERSION, REG_SOFTWARE_VERSION_COUNT
            )

            def on_ok(pf: ParsedFrame) -> None:
                results["sw_version"] = reg_payload_to_ascii(pf.payload)
                read_language_step()

            self._begin_request(
                req,
                FC_READ_HOLDING_RESP,
                on_ok,
                self._on_cfg_fail,
                expect_reg=REG_SOFTWARE_VERSION,
            )

        def read_language_step() -> None:
            req = build_read_holding(self._slave_addr(), REG_LANGUAGE, 1)

            def on_ok(pf: ParsedFrame) -> None:
                if len(pf.payload) >= 2:
                    results["language"] = int.from_bytes(pf.payload[:2], "little")
                finish_read()

            self._begin_request(
                req, FC_READ_HOLDING_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_LANGUAGE
            )

        def read_time_step() -> None:
            req = build_read_holding(self._slave_addr(), REG_TIME, REG_TIME_COUNT)

            def on_ok(pf: ParsedFrame) -> None:
                results["rtc_time"] = reg_payload_to_time(pf.payload)
                read_hw_version_step()

            self._begin_request(
                req, FC_READ_HOLDING_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_TIME
            )

        def read_hw_version_step() -> None:
            req = build_read_holding(
                self._slave_addr(), REG_HW_VERSION, REG_HW_VERSION_COUNT
            )

            def on_ok(pf: ParsedFrame) -> None:
                results["hw_version"] = reg_payload_to_ascii(pf.payload)
                read_sw_version_step()

            self._begin_request(
                req, FC_READ_HOLDING_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_HW_VERSION
            )

        def read_alarm_enable_step() -> None:
            req = build_read_holding(
                self._slave_addr(), REG_ALARM_ENABLE, REG_ALARM_ENABLE_COUNT
            )

            def on_ok(pf: ParsedFrame) -> None:
                results["alarm_enable"] = reg_payload_to_u32(pf.payload)
                read_control_bit2_step()

            self._begin_request(
                req, FC_READ_HOLDING_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_ALARM_ENABLE
            )

        def read_control_bit2_step() -> None:
            req = build_read_holding(
                self._slave_addr(), REG_CONTROL_BIT2, REG_CONTROL_BIT2_COUNT
            )

            def on_ok(pf: ParsedFrame) -> None:
                results["control_bit2"] = reg_payload_to_u32(pf.payload)
                read_volume_step()

            self._begin_request(
                req, FC_READ_HOLDING_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_CONTROL_BIT2
            )

        def read_volume_step() -> None:
            req = build_read_holding(self._slave_addr(), REG_ALARM_VOLUME, 1)

            def on_ok(pf: ParsedFrame) -> None:
                if len(pf.payload) >= 2:
                    results["volume"] = int.from_bytes(pf.payload[:2], "little")
                read_time_step()

            self._begin_request(
                req, FC_READ_HOLDING_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_ALARM_VOLUME
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
                read_static_ip_step()

            self._begin_request(
                req, FC_READ_HOLDING_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_CURRENT_IP
            )

        def read_static_ip_step() -> None:
            req = build_read_holding(
                self._slave_addr(), REG_STATIC_IP, REG_STATIC_IP_COUNT
            )

            def on_ok(pf: ParsedFrame) -> None:
                results["static_ip"] = reg_payload_to_ipv4(pf.payload)
                read_dhcp_step()

            self._begin_request(
                req, FC_READ_HOLDING_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_STATIC_IP
            )

        def read_dhcp_step() -> None:
            req = build_read_holding(self._slave_addr(), REG_DHCP_ENABLE, 1)

            def on_ok(pf: ParsedFrame) -> None:
                if len(pf.payload) >= 2:
                    results["dhcp_enable"] = int.from_bytes(pf.payload[:2], "little")
                read_dose_hi_step()

            self._begin_request(
                req, FC_READ_HOLDING_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_DHCP_ENABLE
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

    def _read_zjb_factory_cfg(self) -> None:
        results: dict[str, str | int] = {}

        def finish_read() -> None:
            self.sn_var.set(str(results.get("sn", "")))
            self._set_control_bit2_from_reg(
                int(results.get("control_bit2", ZJB_CTRL2_DEFAULT))
            )
            self.sw_version_var.set(str(results.get("sw_version", "—")))
            self.status_var.set("zjb 配置读取完成")
            messagebox.showinfo(
                "完成",
                f"已读取 zjb：SN / reg123=0x{int(results.get('control_bit2', 0)):08X} / 软件版本",
            )

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

        def read_control_bit2_step() -> None:
            req = build_read_holding(
                self._slave_addr(), REG_CONTROL_BIT2, REG_CONTROL_BIT2_COUNT
            )

            def on_ok(pf: ParsedFrame) -> None:
                results["control_bit2"] = reg_payload_to_u32(pf.payload)
                read_sw_version_step()

            self._begin_request(
                req, FC_READ_HOLDING_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_CONTROL_BIT2
            )

        def read_sn_step() -> None:
            req = build_read_holding(
                self._slave_addr(), REG_SERIALNUM, REG_SERIALNUM_COUNT
            )

            def on_ok(pf: ParsedFrame) -> None:
                results["sn"] = reg_payload_to_ascii(pf.payload)
                read_control_bit2_step()

            self._begin_request(
                req, FC_READ_HOLDING_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_SERIALNUM
            )

        self.status_var.set("正在读取 zjb 配置...")
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

    def _parse_ipv4_text(self, text: str, field_name: str = "IP") -> list[int] | None:
        try:
            return ipv4_to_reg_values(text.strip())
        except ValueError:
            messagebox.showwarning("提示", f"{field_name} 格式无效")
            return None

    def write_ip_only(self) -> None:
        if not self._require_connected():
            return
        ip_text = self.static_ip_var.get().strip()
        if not ip_text:
            messagebox.showwarning("提示", "请先填写静态 IP")
            return
        values = self._parse_ipv4_text(ip_text, "静态 IP")
        if values is None:
            return
        req = build_write_multi(self._slave_addr(), REG_STATIC_IP, values)

        def on_ok(_pf: ParsedFrame) -> None:
            messagebox.showinfo(
                "完成",
                f"静态 IP 已写入 W25Q: {ip_text}\nDHCP 关闭时复位后生效。",
            )

        self._begin_request(
            req, FC_WRITE_MULTI_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_STATIC_IP
        )

    def write_dhcp_only(self) -> None:
        if not self._require_connected():
            return
        enable = 1 if self.dhcp_enable_var.get() else 0
        req = build_write_single(self._slave_addr(), REG_DHCP_ENABLE, enable)
        mode = "DHCP" if enable else "静态 IP"

        def on_ok(_pf: ParsedFrame) -> None:
            messagebox.showinfo(
                "完成",
                f"网络模式已写入 W25Q: {mode}\n请复位或重新上电后生效。",
            )

        self._begin_request(
            req, FC_WRITE_SINGLE_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_DHCP_ENABLE
        )

    def write_language_only(self) -> None:
        if not self._require_connected():
            return
        lang = self._language_reg_value()
        if lang is None:
            return
        req = build_write_single(self._slave_addr(), REG_LANGUAGE, lang)
        label = self.language_var.get()

        def on_ok(_pf: ParsedFrame) -> None:
            messagebox.showinfo("完成", f"语言已写入 W25Q: {label} (reg 174)")

        self._begin_request(
            req, FC_WRITE_SINGLE_RESP, on_ok, self._on_cfg_fail, expect_reg=REG_LANGUAGE
        )

    def write_hw_version_only(self) -> None:
        if not self._require_connected():
            return
        hw_version = self.hw_version_var.get().strip()[:CFG_HW_VERSION_MAX_LEN]
        if not hw_version:
            messagebox.showwarning("提示", "请输入硬件版本")
            return
        values = ascii_to_reg_values(hw_version, REG_HW_VERSION_COUNT)
        req = build_write_multi(self._slave_addr(), REG_HW_VERSION, values)
        self._begin_request(
            req,
            FC_WRITE_MULTI_RESP,
            lambda _pf: messagebox.showinfo("完成", f"硬件版本已写入: {hw_version}"),
            self._on_cfg_fail,
            expect_reg=REG_HW_VERSION,
        )

    def write_dose_hi_only(self) -> None:
        if not self._require_connected():
            return
        dose_hi_x100 = self._parse_dose_hi_x100()
        if dose_hi_x100 is None:
            return
        req = build_write_multi(
            self._slave_addr(), REG_DOSE_HI_TH, u32_to_reg_values(dose_hi_x100)
        )
        self._begin_request(
            req,
            FC_WRITE_MULTI_RESP,
            lambda _pf: messagebox.showinfo("完成", f"上阈值已写入: {self.dose_hi_var.get()} μSv/h"),
            self._on_cfg_fail,
            expect_reg=REG_DOSE_HI_TH,
        )

    def write_dose_lo_only(self) -> None:
        if not self._require_connected():
            return
        dose_lo_x100 = self._parse_dose_lo_x100()
        if dose_lo_x100 is None:
            return
        req = build_write_multi(
            self._slave_addr(), REG_DOSE_LO_TH, u32_to_reg_values(dose_lo_x100)
        )
        self._begin_request(
            req,
            FC_WRITE_MULTI_RESP,
            lambda _pf: messagebox.showinfo("完成", f"下阈值已写入: {self.dose_lo_var.get()} μSv/h"),
            self._on_cfg_fail,
            expect_reg=REG_DOSE_LO_TH,
        )

    def write_alarm_enable_only(self) -> None:
        if not self._require_connected():
            return
        alarm_enable = self._alarm_enable_from_ui()
        req = build_write_multi(
            self._slave_addr(), REG_ALARM_ENABLE, u32_to_reg_values(alarm_enable)
        )
        self._begin_request(
            req,
            FC_WRITE_MULTI_RESP,
            lambda _pf: messagebox.showinfo("完成", f"报警使能已写入: 0x{alarm_enable:X}"),
            self._on_cfg_fail,
            expect_reg=REG_ALARM_ENABLE,
        )

    def write_volume_only(self) -> None:
        if not self._require_connected():
            return
        vol = self._parse_alarm_volume()
        if vol is None:
            return
        req = build_write_single(self._slave_addr(), REG_ALARM_VOLUME, vol)
        self._begin_request(
            req,
            FC_WRITE_SINGLE_RESP,
            lambda _pf: messagebox.showinfo("完成", f"报警音量已写入: {vol}"),
            self._on_cfg_fail,
            expect_reg=REG_ALARM_VOLUME,
        )

    def write_control_bit2_only(self) -> None:
        if not self._require_connected():
            return
        merged = self._control_bit2_from_ui()
        req = build_write_multi(
            self._slave_addr(), REG_CONTROL_BIT2, u32_to_reg_values(merged)
        )

        def on_ok(_pf: ParsedFrame) -> None:
            self._control_bit2_raw = merged
            self.ctrl2_hex_var.set(f"0x{merged:08X}")
            if self._is_zjb():
                msg = f"reg123 已写入 zjb: 0x{merged:08X}（LoRa/屏/光）"
            else:
                msg = f"reg123 已写入 Neiji: 0x{merged:08X}（LoRa/屏/光）"
            messagebox.showinfo("完成", msg)

        self._begin_request(
            req,
            FC_WRITE_MULTI_RESP,
            on_ok,
            self._on_cfg_fail,
            expect_reg=REG_CONTROL_BIT2,
        )

    def _alarm_form_values(self) -> tuple[int, int, int] | None:
        dose_hi_x100 = self._parse_dose_hi_x100()
        if dose_hi_x100 is None:
            return None
        dose_lo_x100 = self._parse_dose_lo_x100()
        if dose_lo_x100 is None:
            return None
        return dose_hi_x100, dose_lo_x100, self._alarm_enable_from_ui()

    def write_alarm_only(self) -> None:
        """兼容旧入口：依次写上/下阈值与报警使能。"""
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
        if self._is_zjb():
            self._write_zjb_factory_cfg()
            return
        sn = self.sn_var.get().strip()[:CFG_SN_MAX_LEN]
        name = self.name_var.get().strip()
        model = self.model_var.get().strip()[:CFG_MODEL_MAX_LEN]
        hw_version = self.hw_version_var.get().strip()[:CFG_HW_VERSION_MAX_LEN]
        if not sn or not name or not model or not hw_version:
            messagebox.showwarning("提示", "请填写序列号、产品名称、产品型号和硬件版本")
            return
        try:
            addr = int(self.dev_addr_var.get())
        except ValueError:
            messagebox.showwarning("提示", "协议地址无效")
            return
        addr = max(1, min(247, addr))
        dose_hi_x100 = self._parse_dose_hi_x100()
        if dose_hi_x100 is None:
            return
        dose_lo_x100 = self._parse_dose_lo_x100()
        if dose_lo_x100 is None:
            return
        vol = self._parse_alarm_volume()
        if vol is None:
            return
        alarm_enable = self._alarm_enable_from_ui()
        control_bit2 = self._control_bit2_from_ui()

        def write_alarm_enable_step() -> None:
            req = build_write_multi(
                self._slave_addr(), REG_ALARM_ENABLE, u32_to_reg_values(alarm_enable)
            )

            def on_ok(_pf: ParsedFrame) -> None:
                self._control_bit2_raw = control_bit2
                self.slave_addr_var.set(str(addr))
                self.status_var.set("配置写入完成")
                messagebox.showinfo(
                    "完成",
                    "SN / 名称 / 型号 / 硬件版本 / 地址 / 阈值 / 语言 / "
                    "音量 / 屏光 / 报警使能已写入",
                )

            self._begin_request(
                req,
                FC_WRITE_MULTI_RESP,
                on_ok,
                self._on_cfg_fail,
                expect_reg=REG_ALARM_ENABLE,
            )

        def write_control_bit2_step() -> None:
            req = build_write_multi(
                self._slave_addr(), REG_CONTROL_BIT2, u32_to_reg_values(control_bit2)
            )
            self._begin_request(
                req,
                FC_WRITE_MULTI_RESP,
                lambda _pf: write_alarm_enable_step(),
                self._on_cfg_fail,
                expect_reg=REG_CONTROL_BIT2,
            )

        def write_volume_step() -> None:
            req = build_write_single(self._slave_addr(), REG_ALARM_VOLUME, vol)
            self._begin_request(
                req,
                FC_WRITE_SINGLE_RESP,
                lambda _pf: write_control_bit2_step(),
                self._on_cfg_fail,
                expect_reg=REG_ALARM_VOLUME,
            )

        def write_language_step() -> None:
            lang = self._language_reg_value()
            if lang is None:
                return
            req = build_write_single(self._slave_addr(), REG_LANGUAGE, lang)
            self._begin_request(
                req,
                FC_WRITE_SINGLE_RESP,
                lambda _pf: write_volume_step(),
                self._on_cfg_fail,
                expect_reg=REG_LANGUAGE,
            )

        def write_hw_version_step() -> None:
            values = ascii_to_reg_values(hw_version, REG_HW_VERSION_COUNT)
            req = build_write_multi(self._slave_addr(), REG_HW_VERSION, values)
            self._begin_request(
                req,
                FC_WRITE_MULTI_RESP,
                lambda _pf: write_language_step(),
                self._on_cfg_fail,
                expect_reg=REG_HW_VERSION,
            )

        def write_dose_lo_step() -> None:
            req = build_write_multi(
                self._slave_addr(), REG_DOSE_LO_TH, u32_to_reg_values(dose_lo_x100)
            )
            self._begin_request(
                req,
                FC_WRITE_MULTI_RESP,
                lambda _pf: write_hw_version_step(),
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

    def _write_zjb_factory_cfg(self) -> None:
        sn = self.sn_var.get().strip()[:CFG_SN_MAX_LEN]
        control_bit2 = self._control_bit2_from_ui()

        def finish_write() -> None:
            self._control_bit2_raw = control_bit2
            self.ctrl2_hex_var.set(f"0x{control_bit2:08X}")
            self.status_var.set("zjb 配置写入完成")
            messagebox.showinfo(
                "完成",
                f"zjb 已写入：SN / reg123=0x{control_bit2:08X}",
            )

        def write_control_bit2_step() -> None:
            req = build_write_multi(
                self._slave_addr(), REG_CONTROL_BIT2, u32_to_reg_values(control_bit2)
            )
            self._begin_request(
                req,
                FC_WRITE_MULTI_RESP,
                lambda _pf: finish_write(),
                self._on_cfg_fail,
                expect_reg=REG_CONTROL_BIT2,
            )

        def write_sn_step() -> None:
            values = ascii_to_reg_values(sn, REG_SERIALNUM_COUNT)
            req = build_write_multi(self._slave_addr(), REG_SERIALNUM, values)
            self._begin_request(
                req,
                FC_WRITE_MULTI_RESP,
                lambda _pf: write_control_bit2_step(),
                self._on_cfg_fail,
                expect_reg=REG_SERIALNUM,
            )

        self.status_var.set("正在写入 zjb 配置...")
        if sn:
            write_sn_step()
        else:
            write_control_bit2_step()

    def _apply_conn_mode_ui(self) -> None:
        if self.connected:
            return
        if self._conn_mode.get() == "tcp":
            self.serial_frame.pack_forget()
            self.tcp_frame.pack(side=tk.LEFT, fill=tk.X, expand=True)
        else:
            self.tcp_frame.pack_forget()
            self.serial_frame.pack(side=tk.LEFT, fill=tk.X, expand=True)

    def _set_conn_inputs_state(self, connected: bool) -> None:
        state_ro = "disabled" if connected else "readonly"
        state_norm = "disabled" if connected else "normal"
        self.rb_serial.configure(state=state_norm)
        self.rb_tcp.configure(state=state_norm)
        self.port_combo.configure(state=state_ro)
        self.btn_refresh_ports.configure(state=state_norm)
        self.baud_combo.configure(state=state_ro)
        self.tcp_host_entry.configure(state=state_norm)
        self.tcp_port_entry.configure(state=state_norm)
        self.slave_spin.configure(state=state_norm)
        self.btn_connect.configure(text="断开" if connected else "连接")

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
        if self._conn_mode.get() == "tcp":
            self._connect_tcp()
        else:
            self._connect_serial()

    def _connect_serial(self) -> None:
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
            self.worker = self.serial_worker
            self.worker.start(port, baud)
        except serial.SerialException as exc:
            messagebox.showerror("连接失败", str(exc))
            return

        self._link_kind = "serial"
        self._on_connected(f"已连接 串口 {port} @ {baud}")
        save_conn_config(
            "serial",
            port=port,
            baud=baud,
            host=self.tcp_host_var.get().strip(),
            tcp_port=int(self.tcp_port_var.get() or DEFAULT_TCP_PORT),
        )
        self._log(f"=== 已连接 串口 {port} @ {baud} ===")

    def _connect_tcp(self) -> None:
        host = self.tcp_host_var.get().strip()
        if not host:
            messagebox.showwarning("提示", "请填写 IP 地址")
            return
        try:
            port = int(self.tcp_port_var.get().strip() or DEFAULT_TCP_PORT)
        except ValueError:
            messagebox.showwarning("提示", "TCP 端口无效")
            return
        if port <= 0 or port > 65535:
            messagebox.showwarning("提示", "TCP 端口须在 1..65535")
            return
        try:
            self.worker = self.tcp_worker
            self.worker.start(host, port)
        except OSError as exc:
            messagebox.showerror("TCP 连接失败", str(exc))
            return

        self._link_kind = "tcp"
        self._on_connected(f"已连接 TCP {host}:{port}")
        save_conn_config(
            "tcp",
            port=port_from_combo(self.port_var.get()),
            baud=int(self.baud_var.get() or DEFAULT_BAUD),
            host=host,
            tcp_port=port,
        )
        self._log(f"=== 已连接 TCP {host}:{port} ===")

    def _on_connected(self, status: str) -> None:
        self.connected = True
        self.scanner = FrameScanner(slave_filter=None)
        self.frame_count = 0
        self._set_conn_inputs_state(True)
        self.status_var.set(f"{status} — 等待数据...")

    def disconnect(self) -> None:
        self.cps_poll_var.set(False)
        self._stop_cps_poll()
        self._cancel_pending()
        self.worker.stop()
        self.connected = False
        self._set_conn_inputs_state(False)
        self.status_var.set("未连接")
        self._log("=== 已断开 ===")

    def read_registers(self) -> None:
        if not self._require_connected():
            return
        req = build_read_holding(self._slave_addr(), 0x0001, 11)
        self._send_frame(req)

    def _on_link_data(self, data: bytes) -> None:
        frames, text = self.scanner.feed(data)
        if text and self._link_kind == "serial":
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

    def _on_link_error(self, msg: str) -> None:
        self._log(f"ERR {msg}")
        kind = "TCP" if self._link_kind == "tcp" else "串口"
        messagebox.showerror(f"{kind}错误", msg)
        self.disconnect()

    def _on_link_closed(self) -> None:
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
        self._stop_cps_poll()
        self._cancel_pending()
        self.serial_worker.stop()
        self.tcp_worker.stop()
        self.destroy()


def main() -> None:
    app = FactoryApp()
    app.mainloop()


if __name__ == "__main__":
    main()
