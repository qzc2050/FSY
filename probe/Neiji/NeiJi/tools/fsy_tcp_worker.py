#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""NeiJi Tools — TCP 传输（Modbus RTU + CRC 透传，与串口帧格式相同）。"""
from __future__ import annotations

import socket
import threading
from typing import Callable, Optional


class TcpWorker:
    """后台读 TCP 线程；接口与 SerialWorker 对齐。"""

    def __init__(
        self,
        on_data: Callable[[bytes], None],
        on_error: Callable[[str], None],
        on_closed: Callable[[], None],
    ) -> None:
        self._on_data = on_data
        self._on_error = on_error
        self._on_closed = on_closed
        self._sock: Optional[socket.socket] = None
        self._running = False
        self._thread: threading.Thread | None = None

    def start(self, host: str, port: int, connect_timeout: float = 8.0) -> None:
        self.stop()
        host = host.strip()
        if not host:
            raise OSError("IP 地址为空")
        if port <= 0 or port > 65535:
            raise OSError(f"端口无效: {port}")

        sock = socket.create_connection((host, port), timeout=connect_timeout)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        sock.settimeout(1.0)
        self._sock = sock
        self._running = True
        self._thread = threading.Thread(
            target=self._read_loop, name="fsy-tcp-read", daemon=True
        )
        self._thread.start()

    def stop(self) -> None:
        self._running = False
        sock = self._sock
        self._sock = None
        if sock is not None:
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            try:
                sock.close()
            except OSError:
                pass
        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=1.5)
        self._thread = None

    def send(self, data: bytes) -> bool:
        sock = self._sock
        if sock is None:
            return False
        try:
            sock.sendall(data)
            return True
        except OSError as exc:
            self._on_error(str(exc))
            return False

    def flush_rx(self) -> None:
        sock = self._sock
        if sock is None:
            return
        try:
            prev = sock.gettimeout()
            sock.settimeout(0.0)
            while True:
                try:
                    chunk = sock.recv(4096)
                except BlockingIOError:
                    break
                except socket.timeout:
                    break
                if not chunk:
                    break
            sock.settimeout(prev if prev is not None else 1.0)
        except OSError:
            pass

    def _read_loop(self) -> None:
        sock = self._sock
        try:
            while self._running and sock is not None:
                try:
                    chunk = sock.recv(4096)
                except socket.timeout:
                    continue
                if not chunk:
                    if self._running:
                        break
                    return
                self._on_data(chunk)
        except OSError as exc:
            if self._running:
                self._on_error(str(exc))
        finally:
            if self._running:
                self._running = False
            self._on_closed()
