#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
模拟从机图形界面：组播发现 + TCP 服务端（单连接）。
双击或运行: python tools/fsy_tcp_slave_gui.py
"""

from __future__ import annotations

import queue
import sys
import threading
import tkinter as tk
from typing import Optional
from pathlib import Path
from tkinter import messagebox, scrolledtext, ttk

# 保证与脚本同目录的模块可被导入
_TOOLS = Path(__file__).resolve().parent
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from fsy_tcp_slave_simulator import (  # noqa: E402
    DEFAULT_TCP_PORT,
    DEFAULT_UPLOAD_VALUES,
    DEFAULT_THRESHOLDS,
    infer_local_ipv4,
    SN,
    TcpSlave,
)


class SlaveGui:
    def __init__(self) -> None:
        self.root = tk.Tk()
        self.root.title("FSY 模拟从机（组播 + TCP）")
        self.root.minsize(520, 420)
        try:
            self.root.lift()
            self.root.attributes("-topmost", True)
            self.root.after(300, lambda: self.root.attributes("-topmost", False))
        except tk.TclError:
            pass

        self._slave: Optional[TcpSlave] = None
        self._worker: Optional[threading.Thread] = None
        self._log_q: queue.Queue[str] = queue.Queue()
        self._thr_q: queue.Queue[list[int]] = queue.Queue()
        self._status_q: queue.Queue[tuple[int, int]] = queue.Queue()
        self._serial_q: queue.Queue[str] = queue.Queue()
        self._poll_scheduled = False

        self._build()
        self._refresh_local_ip()
        self._poll_log()
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    def _build(self) -> None:
        pad = {"padx": 8, "pady": 4}
        frm = ttk.Frame(self.root, padding=10)
        frm.pack(fill=tk.BOTH, expand=True)

        ttk.Label(frm, text="本机 IPv4（组播串，自动推断）:").grid(row=0, column=0, sticky=tk.W, **pad)
        self.var_local = tk.StringVar(value="—")
        ttk.Label(frm, textvariable=self.var_local, font=("Consolas", 10)).grid(
            row=0, column=1, columnspan=2, sticky=tk.W, **pad
        )
        ttk.Button(frm, text="刷新 IP", command=self._refresh_local_ip).grid(row=0, column=3, **pad)

        ttk.Label(frm, text="可选 peer（多网卡时填安卓 IP）:").grid(row=1, column=0, sticky=tk.W, **pad)
        self.entry_peer = ttk.Entry(frm, width=28)
        self.entry_peer.grid(row=1, column=1, columnspan=3, sticky=tk.EW, **pad)

        ttk.Label(frm, text="TCP 端口:").grid(row=2, column=0, sticky=tk.W, **pad)
        self.entry_port = ttk.Entry(frm, width=12)
        self.entry_port.insert(0, str(DEFAULT_TCP_PORT))
        self.entry_port.grid(row=2, column=1, sticky=tk.W, **pad)

        ttk.Label(frm, text="从机地址:").grid(row=2, column=2, sticky=tk.E, **pad)
        self.entry_addr = ttk.Entry(frm, width=10)
        self.entry_addr.insert(0, "0x01")
        self.entry_addr.grid(row=2, column=3, sticky=tk.W, **pad)

        ttk.Label(frm, text="固件版本号(≤20字符):").grid(row=3, column=0, sticky=tk.W, **pad)
        self.entry_ver = ttk.Entry(frm, width=22)
        self.entry_ver.insert(0, "V1.0.0.SIM")
        self.entry_ver.grid(row=3, column=1, sticky=tk.EW, **pad)

        ttk.Label(frm, text="序列号(≤16字符):").grid(row=3, column=2, sticky=tk.E, **pad)
        self.entry_serial = ttk.Entry(frm, width=18)
        self.entry_serial.insert(0, SN)
        self.entry_serial.grid(row=3, column=3, sticky=tk.EW, **pad)

        ttk.Label(frm, text="组播周期(秒):").grid(row=4, column=0, sticky=tk.W, **pad)
        self.entry_mcast = ttk.Entry(frm, width=12)
        self.entry_mcast.insert(0, "1.0")
        self.entry_mcast.grid(row=4, column=1, sticky=tk.W, **pad)

        ttk.Label(frm, text="TCP 推送 0x23 周期(秒):").grid(row=4, column=2, sticky=tk.E, **pad)
        self.entry_push = ttk.Entry(frm, width=10)
        self.entry_push.insert(0, "1.0")
        self.entry_push.grid(row=4, column=3, sticky=tk.W, **pad)

        # 实时上传字段（工程值）编辑区：每次发送 0x23 前自动读取当前输入
        upload_box = ttk.LabelFrame(frm, text="0x23 实时值编辑（自动生效）")
        upload_box.grid(row=5, column=0, columnspan=4, sticky=tk.EW, padx=8, pady=6)
        upload_box.columnconfigure(1, weight=1)
        upload_box.columnconfigure(3, weight=1)

        ttk.Label(upload_box, text="辐射量(uSv/h):").grid(row=0, column=0, sticky=tk.W, **pad)
        self.entry_dose = ttk.Entry(upload_box, width=14)
        self.entry_dose.grid(row=0, column=1, sticky=tk.EW, **pad)
        ttk.Label(upload_box, text="温度(℃):").grid(row=0, column=2, sticky=tk.W, **pad)
        self.entry_temp = ttk.Entry(upload_box, width=14)
        self.entry_temp.grid(row=0, column=3, sticky=tk.EW, **pad)

        ttk.Label(upload_box, text="气压(Pa):").grid(row=1, column=0, sticky=tk.W, **pad)
        self.entry_press = ttk.Entry(upload_box, width=14)
        self.entry_press.grid(row=1, column=1, sticky=tk.EW, **pad)
        ttk.Label(upload_box, text="湿度(%):").grid(row=1, column=2, sticky=tk.W, **pad)
        self.entry_hum = ttk.Entry(upload_box, width=14)
        self.entry_hum.grid(row=1, column=3, sticky=tk.EW, **pad)

        ttk.Label(upload_box, text="CO2(ppm):").grid(row=2, column=0, sticky=tk.W, **pad)
        self.entry_co2 = ttk.Entry(upload_box, width=14)
        self.entry_co2.grid(row=2, column=1, sticky=tk.EW, **pad)
        ttk.Label(upload_box, text="PM2.5(ug/m3):").grid(row=2, column=2, sticky=tk.W, **pad)
        self.entry_pm25 = ttk.Entry(upload_box, width=14)
        self.entry_pm25.grid(row=2, column=3, sticky=tk.EW, **pad)

        ttk.Label(upload_box, text="报警状态(bit):").grid(row=3, column=0, sticky=tk.W, **pad)
        self.entry_alarm = ttk.Entry(upload_box, width=14)
        self.entry_alarm.grid(row=3, column=1, sticky=tk.EW, **pad)
        ttk.Label(upload_box, text="设备状态(bit):").grid(row=3, column=2, sticky=tk.W, **pad)
        self.entry_status = ttk.Entry(upload_box, width=14)
        self.entry_status.grid(row=3, column=3, sticky=tk.EW, **pad)

        ttk.Label(upload_box, text="预留1:").grid(row=4, column=0, sticky=tk.W, **pad)
        self.entry_r1 = ttk.Entry(upload_box, width=14)
        self.entry_r1.grid(row=4, column=1, sticky=tk.EW, **pad)
        ttk.Label(upload_box, text="预留2:").grid(row=4, column=2, sticky=tk.W, **pad)
        self.entry_r2 = ttk.Entry(upload_box, width=14)
        self.entry_r2.grid(row=4, column=3, sticky=tk.EW, **pad)
        ttk.Label(upload_box, text="预留3:").grid(row=5, column=0, sticky=tk.W, **pad)
        self.entry_r3 = ttk.Entry(upload_box, width=14)
        self.entry_r3.grid(row=5, column=1, sticky=tk.EW, **pad)

        self._sync_fields_from_values(DEFAULT_UPLOAD_VALUES)

        # 五分钟值手动上传区
        five_min_box = ttk.LabelFrame(frm, text="五分钟值主动上传（0x23 / 寄存器 0x001E）")
        five_min_box.grid(row=6, column=0, columnspan=4, sticky=tk.EW, padx=8, pady=4)
        five_min_box.columnconfigure(1, weight=1)

        ttk.Label(five_min_box, text="辐射量(uSv/h):").grid(row=0, column=0, sticky=tk.W, **pad)
        self.entry_5min_dose = ttk.Entry(five_min_box, width=14)
        self.entry_5min_dose.insert(0, "0.00")
        self.entry_5min_dose.grid(row=0, column=1, sticky=tk.W, **pad)
        self.btn_send_5min = ttk.Button(
            five_min_box,
            text="立即发送五分钟值",
            command=self._on_send_five_minute,
            state=tk.DISABLED,
        )
        self.btn_send_5min.grid(row=0, column=2, columnspan=2, padx=8, pady=4)

        # 报警阈值编辑区（对应寄存器 0x0040，12 个 u32）
        thr_box = ttk.LabelFrame(frm, text="报警阈值编辑（改完点应用，寄存器 0x0040）")
        thr_box.grid(row=7, column=0, columnspan=4, sticky=tk.EW, padx=8, pady=4)
        for c in range(4):
            thr_box.columnconfigure(c, weight=1)

        sensor_rows = [
            ("辐射上限(uSv/h):", "dose_hi", "辐射下限(uSv/h):", "dose_lo"),
            ("温度上限(℃):",     "temp_hi", "温度下限(℃):",     "temp_lo"),
            ("气压上限(Pa):",     "pres_hi", "气压下限(Pa):",     "pres_lo"),
            ("湿度上限(%):",      "hum_hi",  "湿度下限(%):",      "hum_lo"),
            ("CO2上限(ppm):",     "co2_hi",  "CO2下限(ppm):",     "co2_lo"),
            ("PM2.5上限(ug/m3):", "pm25_hi", "PM2.5下限(ug/m3):", "pm25_lo"),
        ]
        self._thr_entries: dict[str, ttk.Entry] = {}
        for r, (lbl1, key1, lbl2, key2) in enumerate(sensor_rows):
            ttk.Label(thr_box, text=lbl1).grid(row=r, column=0, sticky=tk.W, **pad)
            e1 = ttk.Entry(thr_box, width=14)
            e1.grid(row=r, column=1, sticky=tk.EW, **pad)
            self._thr_entries[key1] = e1
            ttk.Label(thr_box, text=lbl2).grid(row=r, column=2, sticky=tk.W, **pad)
            e2 = ttk.Entry(thr_box, width=14)
            e2.grid(row=r, column=3, sticky=tk.EW, **pad)
            self._thr_entries[key2] = e2

        self._sync_thr_fields_from_values(DEFAULT_THRESHOLDS)

        self.btn_apply_thr = ttk.Button(
            thr_box,
            text="应用阈值",
            command=self._on_apply_thresholds,
            state=tk.DISABLED,
        )
        self.btn_apply_thr.grid(row=len(sensor_rows), column=0, columnspan=4, pady=4)

        row_btns = ttk.Frame(frm)
        row_btns.grid(row=8, column=0, columnspan=4, pady=10)
        self.btn_start = ttk.Button(row_btns, text="启动模拟从机", command=self._on_start)
        self.btn_start.pack(side=tk.LEFT, padx=4)
        self.btn_stop = ttk.Button(row_btns, text="停止", command=self._on_stop, state=tk.DISABLED)
        self.btn_stop.pack(side=tk.LEFT, padx=4)

        ttk.Label(frm, text="日志:").grid(row=9, column=0, sticky=tk.NW, **pad)
        self.txt = scrolledtext.ScrolledText(frm, height=12, font=("Consolas", 9), state=tk.DISABLED)
        self.txt.grid(row=10, column=0, columnspan=4, sticky=tk.NSEW, **pad)
        frm.rowconfigure(10, weight=1)
        frm.columnconfigure(1, weight=1)

        hint = "会自动组播/广播发现信息；TCP 请连接 本机IP:TCP端口。"
        ttk.Label(frm, text=hint, foreground="#555").grid(row=11, column=0, columnspan=4, sticky=tk.W, **pad)

    @staticmethod
    def _parse_u32(raw: str, field: str) -> int:
        v = int(raw.strip(), 0)
        if v < 0 or v > 0xFFFFFFFF:
            raise ValueError(f"{field} 超出 u32 范围: {v}")
        return v

    def _sync_thr_fields_from_values(self, values: list[int]) -> None:
        keys = ["dose_hi", "dose_lo", "temp_hi", "temp_lo",
                "pres_hi", "pres_lo", "hum_hi",  "hum_lo",
                "co2_hi",  "co2_lo",  "pm25_hi", "pm25_lo"]
        scales = [100.0, 100.0, 10.0, 10.0, 1.0, 1.0, 1.0, 1.0, 10.0, 10.0, 10.0, 10.0]
        for i, key in enumerate(keys):
            e = self._thr_entries[key]
            e.delete(0, tk.END)
            if scales[i] == 1.0:
                e.insert(0, str(values[i]))
            else:
                e.insert(0, f"{values[i] / scales[i]:.1f}" if scales[i] == 10.0 else f"{values[i] / scales[i]:.2f}")

    def _build_thresholds_from_fields(self) -> list[int]:
        try:
            dose_hi  = max(0, int(round(float(self._thr_entries["dose_hi"].get().strip()) * 100)))
            dose_lo  = max(0, int(round(float(self._thr_entries["dose_lo"].get().strip()) * 100)))
            temp_hi  = int(round(float(self._thr_entries["temp_hi"].get().strip()) * 10))
            temp_lo  = int(round(float(self._thr_entries["temp_lo"].get().strip()) * 10))
            pres_hi  = self._parse_u32(self._thr_entries["pres_hi"].get(), "气压上限")
            pres_lo  = self._parse_u32(self._thr_entries["pres_lo"].get(), "气压下限")
            hum_hi   = self._parse_u32(self._thr_entries["hum_hi"].get(),  "湿度上限")
            hum_lo   = self._parse_u32(self._thr_entries["hum_lo"].get(),  "湿度下限")
            co2_hi   = max(0, int(round(float(self._thr_entries["co2_hi"].get().strip()) * 10)))
            co2_lo   = max(0, int(round(float(self._thr_entries["co2_lo"].get().strip()) * 10)))
            pm25_hi  = max(0, int(round(float(self._thr_entries["pm25_hi"].get().strip()) * 10)))
            pm25_lo  = max(0, int(round(float(self._thr_entries["pm25_lo"].get().strip()) * 10)))
        except ValueError as e:
            raise ValueError(f"阈值输入错误: {e}") from e
        return [dose_hi, dose_lo, temp_hi, temp_lo, pres_hi, pres_lo,
                hum_hi, hum_lo, co2_hi, co2_lo, pm25_hi, pm25_lo]

    def _sync_fields_from_values(self, values: list[int]) -> None:
        self.entry_dose.delete(0, tk.END)
        self.entry_dose.insert(0, f"{values[0] / 100:.2f}")
        self.entry_temp.delete(0, tk.END)
        self.entry_temp.insert(0, f"{values[1] / 10:.1f}")
        self.entry_press.delete(0, tk.END)
        self.entry_press.insert(0, str(values[2]))
        self.entry_hum.delete(0, tk.END)
        self.entry_hum.insert(0, str(values[3]))
        self.entry_co2.delete(0, tk.END)
        self.entry_co2.insert(0, f"{values[4] / 10:.1f}")
        self.entry_pm25.delete(0, tk.END)
        self.entry_pm25.insert(0, f"{values[5] / 10:.1f}")
        self.entry_alarm.delete(0, tk.END)
        self.entry_alarm.insert(0, f"0x{values[6]:X}")
        self.entry_status.delete(0, tk.END)
        self.entry_status.insert(0, f"0x{values[7]:X}")
        self.entry_r1.delete(0, tk.END)
        self.entry_r1.insert(0, str(values[8]))
        self.entry_r2.delete(0, tk.END)
        self.entry_r2.insert(0, str(values[9]))
        self.entry_r3.delete(0, tk.END)
        self.entry_r3.insert(0, str(values[10]))

    def _build_upload_values_from_fields(self) -> list[int]:
        try:
            dose = max(0, int(round(float(self.entry_dose.get().strip()) * 100)))
            temp = int(round(float(self.entry_temp.get().strip()) * 10))
            press = self._parse_u32(self.entry_press.get(), "气压")
            hum = self._parse_u32(self.entry_hum.get(), "湿度")
            co2 = max(0, int(round(float(self.entry_co2.get().strip()) * 10)))
            pm25 = max(0, int(round(float(self.entry_pm25.get().strip()) * 10)))
            alarm = self._parse_u32(self.entry_alarm.get(), "报警状态")
            status = self._parse_u32(self.entry_status.get(), "设备状态")
            r1 = self._parse_u32(self.entry_r1.get(), "预留1")
            r2 = self._parse_u32(self.entry_r2.get(), "预留2")
            r3 = self._parse_u32(self.entry_r3.get(), "预留3")
        except ValueError as e:
            raise ValueError(f"字段输入错误: {e}") from e
        vals = [dose, temp, press, hum, co2, pm25, alarm, status, r1, r2, r3]
        return vals

    def _append_log(self, line: str) -> None:
        self.txt.configure(state=tk.NORMAL)
        self.txt.insert(tk.END, line + "\n")
        self.txt.see(tk.END)
        self.txt.configure(state=tk.DISABLED)

    def _poll_log(self) -> None:
        try:
            while True:
                msg = self._log_q.get_nowait()
                self._append_log(msg)
        except queue.Empty:
            pass
        try:
            while True:
                values = self._thr_q.get_nowait()
                self._sync_thr_fields_from_values(values)
                self._append_log("[系统] 阈值已由远端写入，界面已自动刷新")
        except queue.Empty:
            pass
        try:
            while True:
                status_bit, control_bit2 = self._status_q.get_nowait()
                self.entry_status.delete(0, tk.END)
                self.entry_status.insert(0, f"0x{status_bit:X}")
                self._append_log(
                    f"[系统] controlbit2 已由远端写入，status15=0x{status_bit:X} controlbit2=0x{control_bit2:X}"
                )
        except queue.Empty:
            pass
        try:
            while True:
                serial = self._serial_q.get_nowait()
                self.entry_serial.delete(0, tk.END)
                self.entry_serial.insert(0, serial)
                self._append_log(f"[系统] 序列号已由远端写入，界面已自动刷新: {serial}")
        except queue.Empty:
            pass
        self.root.after(120, self._poll_log)

    def _refresh_local_ip(self) -> None:
        peer = self.entry_peer.get().strip() or None
        try:
            ip = infer_local_ipv4(peer)
        except OSError:
            ip = "127.0.0.1"
        self.var_local.set(ip)

    def _on_start(self) -> None:
        if self._slave is not None:
            return
        try:
            tcp_port = int(self.entry_port.get().strip())
            dev = int(self.entry_addr.get().strip(), 0)
            push = float(self.entry_push.get().strip())
            mcast = float(self.entry_mcast.get().strip())
            upload_values = self._build_upload_values_from_fields()
        except ValueError:
            messagebox.showerror("错误", "请检查参数：端口整数、地址可用0x01、周期小数、各变量输入合法。")
            return

        firmware_ver = self.entry_ver.get().strip() or "V1.0.0.SIM"
        if len(firmware_ver.encode("ascii", errors="replace")) > 20:
            messagebox.showerror("错误", "固件版本号超出 20 个 ASCII 字符限制。")
            return

        serial = self.entry_serial.get().strip() or SN
        if len(serial.encode("ascii", errors="replace")) > 16:
            messagebox.showerror("错误", "序列号超出 16 个 ASCII 字符限制。")
            return

        peer = self.entry_peer.get().strip() or None

        def log_to_gui(msg: str) -> None:
            self._log_q.put(msg)

        def on_thresholds_updated(values: list[int]) -> None:
            self._thr_q.put(list(values))

        def on_status_updated(status_bit: int, control_bit2: int) -> None:
            self._status_q.put((status_bit, control_bit2))

        def on_serial_updated(serial_number: str) -> None:
            self._serial_q.put(serial_number)

        self._slave = TcpSlave(
            device_addr=dev,
            tcp_port=tcp_port,
            push_interval=push,
            peer_for_local_ip=peer,
            mcast_interval=mcast,
            log=log_to_gui,
            upload_values=upload_values,
            upload_values_provider=self._build_upload_values_from_fields,
            firmware_version=firmware_ver,
            serial_number=serial,
            thresholds_updated_callback=on_thresholds_updated,
            status_updated_callback=on_status_updated,
            serial_updated_callback=on_serial_updated,
        )
        self.var_local.set(self._slave.local_ip)

        def run_slave() -> None:
            assert self._slave is not None
            try:
                self._slave.run()
            finally:
                self._log_q.put("[系统] 模拟从机线程已结束")

        self._worker = threading.Thread(target=run_slave, daemon=True)
        self._worker.start()

        self.btn_start.configure(state=tk.DISABLED)
        self.btn_stop.configure(state=tk.NORMAL)
        self.btn_send_5min.configure(state=tk.NORMAL)
        self.btn_apply_thr.configure(state=tk.NORMAL)
        self._append_log("[系统] 已启动（组播 + TCP 监听）")

    def _on_stop(self) -> None:
        if self._slave is None:
            return
        self._slave.stop()
        self._slave = None
        self.btn_start.configure(state=tk.NORMAL)
        self.btn_stop.configure(state=tk.DISABLED)
        self.btn_send_5min.configure(state=tk.DISABLED)
        self.btn_apply_thr.configure(state=tk.DISABLED)
        self._append_log("[系统] 已请求停止")

    def _on_apply_thresholds(self) -> None:
        if self._slave is None:
            return
        try:
            values = self._build_thresholds_from_fields()
            self._slave.set_thresholds(values)
            self._append_log(f"[系统] 已应用报警阈值: {values}")
        except Exception as e:
            messagebox.showerror("错误", f"应用阈值失败: {e}")

    def _on_send_five_minute(self) -> None:
        if self._slave is None:
            return
        try:
            dose_x100 = max(0, int(round(float(self.entry_5min_dose.get().strip()) * 100)))
        except ValueError:
            messagebox.showerror("错误", "辐射量输入非法，请填写有效数字（如 0.25）。")
            return
        ok = self._slave.push_five_minute_value(dose_x100)
        if not ok:
            messagebox.showwarning("提示", "发送失败：当前无 TCP 客户端连接，请先连接安卓端。")

    def _on_close(self) -> None:
        if self._slave is not None:
            self._slave.stop()
            self._slave = None
        self.root.destroy()

    def run(self) -> None:
        self.root.mainloop()


def main() -> None:
    SlaveGui().run()


def _show_fatal_traceback() -> None:
    import traceback

    tb = traceback.format_exc()
    print(tb, file=sys.stderr)
    if sys.platform == "win32":
        try:
            import ctypes

            msg = tb if len(tb) <= 2000 else tb[:1997] + "..."
            ctypes.windll.user32.MessageBoxW(0, msg, "FSY TCP Slave GUI — error", 0x10)
        except Exception:
            pass
    try:
        input("Press Enter to exit...")
    except Exception:
        pass


if __name__ == "__main__":
    try:
        main()
    except Exception:
        _show_fatal_traceback()
        raise SystemExit(1)
