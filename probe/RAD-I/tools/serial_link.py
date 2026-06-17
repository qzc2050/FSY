# -*- coding: utf-8 -*-
"""
串口 I/O 与自动重连（供 net_raw_tester 使用）。

分层：
  - normalize_port / scan_ports：端口名工具
  - SerialLink：读写与链路探测（无 Tk、无重连）
  - SerialConnectionService：连接生命周期 + 后台看门狗重连
"""
from __future__ import annotations

import threading
import time
from dataclasses import dataclass
from typing import Callable, List, Optional, Tuple

import serial
import serial.tools.list_ports

LogFn = Callable[[str], None]
UiFn = Callable[[], None]


def normalize_port(port_str: Optional[str]) -> str:
    """从 'COM3 - USB Serial' 或 'COM3' 提取设备名。"""
    if not port_str:
        return ""
    return str(port_str).split(" - ")[0].strip()


def scan_ports() -> List[Tuple[str, str]]:
    return [(p.device, p.description) for p in serial.tools.list_ports.comports()]


def format_port_combo_values(ports: List[Tuple[str, str]]) -> List[str]:
    return [f"{dev} - {desc}" for dev, desc in ports]


def _set_serial_idle_modem_lines(sp: serial.Serial) -> None:
    """
    RAD-I 硬件：DTR→BOOT0，RTS→NRST（经反相/三极管）。
    空闲通信态：DTR=高(BOOT0 低，运行 APP)，RTS=低(NRST 释放，不复位)。
    与 docs/stm32bootloader.cpp resetToApp() 一致。
    """
    try:
        sp.dtr = True
        sp.rts = False
    except (serial.SerialException, AttributeError, OSError, ValueError):
        pass


def open_serial_port(port: str, baudrate: int = 921600) -> serial.Serial:
    """打开串口并固定为「无流控、DTR 高 / RTS 低」空闲态，供 APP 正常通信。"""
    sp = serial.Serial(
        port,
        baudrate,
        timeout=0.5,
        write_timeout=2.0,  # 发送超时，避免永久阻塞
        dsrdtr=False,
        rtscts=False,
    )
    _set_serial_idle_modem_lines(sp)
    return sp


@dataclass(frozen=True)
class WatchdogConfig:
    poll_interval_s: float = 1.0
    max_retries: int = 5
    release_delay_s: float = 0.5
    retry_delay_s: float = 1.0
    link_grace_s: float = 2.5  # 重连/连接后宽限，避免误判立即再断


class SerialLink:
    """底层串口：读线程、发送、拔线探测。"""

    def __init__(self) -> None:
        self.serial_port: Optional[serial.Serial] = None
        self.active_port: Optional[str] = None
        self.is_reading = False
        self.read_thread: Optional[threading.Thread] = None
        self.read_error = False
        self.lock = threading.Lock()
        self.on_data_received: Optional[Callable[[bytes], None]] = None

    def open(self, port: str, baudrate: int = 921600) -> bool:
        port = normalize_port(port)
        if not port:
            return False
        self.close()
        try:
            self.serial_port = open_serial_port(port, baudrate)
            self.active_port = port
            self.read_error = False
            self.is_reading = True
            self.read_thread = threading.Thread(target=self._read_loop, daemon=True)
            self.read_thread.start()
            return True
        except Exception as exc:
            print(f"连接失败：{exc}")
            self.active_port = None
            self.read_error = True
            return False

    def close(self) -> None:
        self.is_reading = False
        thread = self.read_thread
        self.read_thread = None
        if thread and thread.is_alive():
            thread.join(timeout=1.0)
        if self.serial_port:
            try:
                _set_serial_idle_modem_lines(self.serial_port)
                self.serial_port.close()
            except Exception:
                pass
            self.serial_port = None
        self.active_port = None
        self.read_error = False

    def send(self, data: bytes) -> bool:
        if not self.is_open():
            return False
        try:
            # 直接发送，不使用锁（serial.write 本身是线程安全的）
            # 避免长时间持有锁导致读线程无法进行健康检查
            assert self.serial_port is not None
            self.serial_port.write(data)
            self.serial_port.flush()
            return True
        except (serial.SerialException, OSError) as exc:
            print(f"发送失败：{exc}")
            self.read_error = True
            return False
        except Exception as exc:
            print(f"发送失败：{exc}")
            return False

    def is_open(self) -> bool:
        try:
            if not self.serial_port or not self.serial_port.is_open:
                return False
            if self.read_error:
                return False
            if self.read_thread and not self.read_thread.is_alive():
                return False
            return True
        except Exception:
            return False

    def is_alive(self) -> bool:
        """监控用：在 is_open 基础上做端口枚举与 I/O 探测。"""
        if not self.is_open():
            return False
        if self.active_port:
            available = {p[0] for p in scan_ports()}
            if self.active_port not in available:
                self.read_error = True
                return False
        return self._probe_io()

    def _probe_io(self) -> bool:
        try:
            with self.lock:
                sp = self.serial_port
                if not sp or not sp.is_open:
                    return False
                sp.in_waiting
                prev = sp.timeout
                sp.timeout = 0
                try:
                    sp.read(1)
                finally:
                    sp.timeout = prev
            return True
        except (serial.SerialException, OSError, AttributeError):
            self.read_error = True
            return False
        except Exception:
            return False

    def _read_loop(self) -> None:
        idle_ticks = 0
        while self.is_reading and self.serial_port and self.serial_port.is_open:
            try:
                if self.serial_port.in_waiting > 0:
                    idle_ticks = 0
                    data = self.serial_port.read(self.serial_port.in_waiting)
                    if self.on_data_received:
                        self.on_data_received(data)
                else:
                    idle_ticks += 1
                    if idle_ticks >= 100:
                        idle_ticks = 0
                        with self.lock:
                            if not (self.is_reading and self.serial_port and self.serial_port.is_open):
                                break
                            prev = self.serial_port.timeout
                            self.serial_port.timeout = 0
                            try:
                                self.serial_port.read(1)
                            finally:
                                self.serial_port.timeout = prev
            except serial.SerialException as exc:
                print(f"串口异常：{exc}")
                break
            except OSError as exc:
                print(f"系统 I/O 错误：{exc}")
                break
            except Exception as exc:
                print(f"读取错误：{exc}")
                break
            time.sleep(0.01)
        self.read_error = True
        self.is_reading = False


class SerialConnectionService:
    """
    对外统一入口：兼容原 SerialMonitor 字段/方法，并内置看门狗重连。
    """

    def __init__(self, config: Optional[WatchdogConfig] = None) -> None:
        self._link = SerialLink()
        self._cfg = config or WatchdogConfig()
        self.target_port: Optional[str] = None
        self.target_baudrate: int = 921600
        self.manual_disconnect = False
        self.is_reconnecting = False
        self._watchdog_running = False
        self._watchdog_thread: Optional[threading.Thread] = None
        self._retry_count = 0
        self._link_grace_until = 0.0

        self.on_log: LogFn = lambda _msg: None
        self.on_reconnecting: UiFn = lambda: None
        self.on_connected: Callable[[str, int], None] = lambda _p, _b: None
        self.on_disconnected: UiFn = lambda: None
        self.on_reconnect_failed: UiFn = lambda: None

    # --- 兼容旧代码直接访问 link 属性 ---

    @property
    def serial_port(self) -> Optional[serial.Serial]:
        return self._link.serial_port

    @property
    def connected_port(self) -> Optional[str]:
        return self._link.active_port

    @property
    def read_error(self) -> bool:
        return self._link.read_error

    @property
    def is_reading(self) -> bool:
        return self._link.is_reading

    @is_reading.setter
    def is_reading(self, value: bool) -> None:
        self._link.is_reading = value

    @property
    def read_thread(self) -> Optional[threading.Thread]:
        return self._link.read_thread

    @read_thread.setter
    def read_thread(self, value: Optional[threading.Thread]) -> None:
        self._link.read_thread = value

    @property
    def lock(self) -> threading.Lock:
        return self._link.lock

    @property
    def on_data_received(self) -> Optional[Callable[[bytes], None]]:
        return self._link.on_data_received

    @on_data_received.setter
    def on_data_received(self, cb: Optional[Callable[[bytes], None]]) -> None:
        self._link.on_data_received = cb

    def scan_ports(self) -> List[Tuple[str, str]]:
        return scan_ports()

    def get_active_port(self) -> Optional[str]:
        return self._link.active_port if self._link.is_open() else None

    def is_open(self) -> bool:
        return self._link.is_open()

    def is_alive(self) -> bool:
        return self._link.is_alive()

    def _note_link_up(self) -> None:
        self._link_grace_until = time.monotonic() + self._cfg.link_grace_s

    def _in_link_grace(self) -> bool:
        return time.monotonic() < self._link_grace_until

    def connect(self, port: str, baudrate: int = 921600) -> bool:
        port = normalize_port(port)
        if not port:
            return False
        if self._link.open(port, baudrate):
            self.target_port = port
            self.target_baudrate = baudrate
            self.manual_disconnect = False
            self.is_reconnecting = False
            self._retry_count = 0
            self._note_link_up()
            return True
        return False

    def disconnect(self, manual: bool = False) -> None:
        if manual:
            self.manual_disconnect = True
            self.stop_watchdog()
            self.target_port = None
            self.is_reconnecting = False
            self._retry_count = 0
            self._link_grace_until = 0.0
        self._link.close()

    def suspend_link(self) -> None:
        """关闭链路并停止看门狗，保留 target_port（改端口/波特率时）。"""
        self.stop_watchdog()
        self._link.close()
        self._link_grace_until = 0.0

    def send(self, data: bytes) -> bool:
        return self._link.send(data)

    def start_watchdog(self) -> None:
        if self._watchdog_thread and self._watchdog_thread.is_alive():
            return
        self._watchdog_running = True
        self._watchdog_thread = threading.Thread(
            target=self._watchdog_loop,
            daemon=True,
        )
        self._watchdog_thread.start()
        self.on_log("[连接监控] 启动串口监控线程")

    def stop_watchdog(self) -> None:
        self._watchdog_running = False
        if self._watchdog_thread and self._watchdog_thread.is_alive():
            self._watchdog_thread.join(timeout=0.5)
        self._watchdog_thread = None

    @property
    def watchdog_running(self) -> bool:
        return bool(
            self._watchdog_running
            and self._watchdog_thread
            and self._watchdog_thread.is_alive()
        )

    def _attempt_reconnect(self) -> bool:
        port = normalize_port(self.target_port)
        if not port:
            self.on_log("[串口监控] 重连失败：未记住连接端口")
            return False

        available = {p[0] for p in scan_ports()}
        if port not in available:
            self.on_log(f"[串口监控] 端口 {port} 已不可用，等待重新插入...")
            return False

        baudrate = self.target_baudrate

        self.on_log(f"[串口监控] 尝试重连到 {port} {baudrate}...")
        if not self.connect(port, baudrate):
            self.on_log(f"[串口监控] 重连失败：{port} {baudrate}")
            return False

        self.on_log(f"[串口监控] 重连成功：{port} {baudrate}")
        self.on_connected(port, baudrate)
        return True

    def _watchdog_loop(self) -> None:
        while self._watchdog_running:
            try:
                if self.manual_disconnect:
                    self.on_log("[串口监控] 检测到主动断开，停止监控")
                    break

                alive = self._link.is_alive()
                in_grace = self._in_link_grace()

                if alive or in_grace:
                    if alive and self.is_reconnecting:
                        self.is_reconnecting = False
                        self._retry_count = 0
                    time.sleep(self._cfg.poll_interval_s)
                    continue

                if not self.is_reconnecting:
                    self.is_reconnecting = True
                    self._retry_count = 0
                    self.on_log("[串口监控] 检测到连接断开，正在尝试重连...")
                    self.on_reconnecting()

                self._retry_count += 1
                if self._retry_count > self._cfg.max_retries:
                    self.on_log(
                        f"[串口监控] 重连失败，已重试 {self._cfg.max_retries} 次"
                    )
                    self.is_reconnecting = False
                    self._retry_count = 0
                    self.on_reconnect_failed()
                    break

                self.on_log(f"[串口监控] 第 {self._retry_count} 次重连...")
                self._link.close()
                time.sleep(self._cfg.release_delay_s)

                if self._attempt_reconnect():
                    continue

                time.sleep(self._cfg.retry_delay_s)

            except Exception as exc:
                print(f"串口监控异常：{exc}")
                time.sleep(self._cfg.poll_interval_s)

        self._watchdog_running = False
        self._watchdog_thread = None
