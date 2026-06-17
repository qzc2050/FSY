#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
USB DFU 操作库
使用 ctypes 直接调用 libusb DLL，不依赖 pyusb 后端
"""

import os
import sys
import ctypes
from ctypes import c_int, c_char, c_void_p, c_ubyte, c_uint, c_uint16, c_uint8, POINTER, byref, Structure


def _resource_dir() -> str:
    if getattr(sys, "frozen", False) and hasattr(sys, "_MEIPASS"):
        return sys._MEIPASS
    return os.path.dirname(os.path.abspath(__file__))

# 常量定义
LIBUSB_SUCCESS = 0
LIBUSB_ENDPOINT_OUT = 0x00
LIBUSB_ENDPOINT_IN = 0x80
LIBUSB_REQUEST_TYPE_CLASS = 0x01
LIBUSB_RECIPIENT_INTERFACE = 0x01

# DFU 请求
DFU_DETACH = 0
DFU_DNLOAD = 1
DFU_UPLOAD = 2
DFU_GETSTATUS = 3
DFU_CLRSTATUS = 4
DFU_GETSTATE = 5
DFU_ABORT = 6

class USBDFUDevice:
    """USB DFU 设备类"""
    
    def __init__(self):
        self.context = None
        self.handle = None
        self.is_open = False
        
        # 加载 libusb DLL
        dll_path = os.path.join(_resource_dir(), 'libusb-1.0.dll')
        if not os.path.exists(dll_path):
            raise FileNotFoundError(f"libusb DLL 未找到：{dll_path}")
        
        self.libusb = ctypes.CDLL(dll_path)
        
        # 设置函数原型
        self._setup_function_prototypes()
    
    def _setup_function_prototypes(self):
        """设置 libusb 函数原型"""
        # 初始化/退出
        self.libusb.libusb_init.argtypes = [POINTER(c_void_p)]
        self.libusb.libusb_init.restype = c_int
        
        self.libusb.libusb_exit.argtypes = [c_void_p]
        self.libusb.libusb_exit.restype = None
        
        # 设备打开/关闭
        self.libusb.libusb_open_device_with_vid_pid.argtypes = [c_void_p, c_int, c_int]
        self.libusb.libusb_open_device_with_vid_pid.restype = c_void_p
        
        self.libusb.libusb_close.argtypes = [c_void_p]
        self.libusb.libusb_close.restype = None
        
        # 接口管理
        self.libusb.libusb_claim_interface.argtypes = [c_void_p, c_int]
        self.libusb.libusb_claim_interface.restype = c_int
        
        self.libusb.libusb_release_interface.argtypes = [c_void_p, c_int]
        self.libusb.libusb_release_interface.restype = c_int
        
        # 控制传输
        self.libusb.libusb_control_transfer.argtypes = [
            c_void_p, c_int, c_int, c_int, c_int,
            POINTER(c_ubyte), c_int, c_int
        ]
        self.libusb.libusb_control_transfer.restype = c_int
        
        # 批量传输
        self.libusb.libusb_bulk_transfer.argtypes = [
            c_void_p, c_ubyte, POINTER(c_ubyte), c_int, POINTER(c_int), c_uint
        ]
        self.libusb.libusb_bulk_transfer.restype = c_int
    
    def open(self, vid=0x0483, pid=0xDF11):
        """打开 USB DFU 设备"""
        # 初始化 libusb
        ctx = c_void_p()
        ret = self.libusb.libusb_init(byref(ctx))
        if ret != LIBUSB_SUCCESS:
            raise Exception(f"libusb 初始化失败：{ret}")
        
        self.context = ctx
        
        # 打开设备
        handle = self.libusb.libusb_open_device_with_vid_pid(ctx, vid, pid)
        if not handle:
            self.libusb.libusb_exit(ctx)
            self.context = None
            return False
        
        self.handle = handle
        self.is_open = True
        return True
    
    def claim_interface(self, interface_number=0):
        """声明接口"""
        if not self.handle:
            raise Exception("设备未打开")
        
        ret = self.libusb.libusb_claim_interface(self.handle, interface_number)
        if ret != LIBUSB_SUCCESS:
            raise Exception(f"声明接口失败：{ret}")
    
    def release_interface(self, interface_number=0):
        """释放接口"""
        if not self.handle:
            return
        
        try:
            self.libusb.libusb_release_interface(self.handle, interface_number)
        except:
            pass
    
    def close(self):
        """关闭设备"""
        if self.handle:
            try:
                self.libusb.libusb_close(self.handle)
            except:
                pass
            self.handle = None
        
        if self.context:
            try:
                self.libusb.libusb_exit(self.context)
            except:
                pass
            self.context = None
        
        self.is_open = False
    
    def control_transfer(self, bmRequestType, bRequest, wValue, wIndex, data, timeout=1000):
        """执行控制传输"""
        if not self.handle:
            raise Exception("设备未打开")
        
        if data is None:
            data_len = 0
            ret = self.libusb.libusb_control_transfer(
                self.handle,
                bmRequestType,
                bRequest,
                wValue,
                wIndex,
                None,
                0,
                timeout
            )
        else:
            data_len = len(data)
            data_ptr = (c_ubyte * data_len)(*data)
            ret = self.libusb.libusb_control_transfer(
                self.handle,
                bmRequestType,
                bRequest,
                wValue,
                wIndex,
                data_ptr,
                data_len,
                timeout
            )
        
        if ret < 0:
            raise Exception(f"控制传输失败：{ret}")
        
        return ret if data is not None else 0
    
    def dfu_get_status(self, interface=0):
        """获取 DFU 状态"""
        bmRequestType = 0xA1 | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE
        data = (c_ubyte * 6)()
        
        ret = self.libusb.libusb_control_transfer(
            self.handle,
            bmRequestType,
            DFU_GETSTATUS,
            0,
            interface,
            data,
            6,
            1000
        )
        
        if ret < 0:
            return None
        
        return {
            'bStatus': data[0],
            'bwPollTimeout': data[1] | (data[2] << 8) | (data[3] << 16),
            'bState': data[4],
            'iString': data[5]
        }
    
    def dfu_download(self, data, interface=0, block_number=0):
        """DFU 下载数据
        
        Args:
            data: 要发送的数据
            interface: 接口号
            block_number: 块编号（wValue），默认为 0，发送数据时应为 2
        """
        bmRequestType = 0x21 | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE
        
        if not data:
            # 空数据用于初始化或触发跳转
            data_len = 0
            data_ptr = None
        else:
            data_len = len(data)
            data_ptr = (c_ubyte * data_len)(*data)
        
        ret = self.libusb.libusb_control_transfer(
            self.handle,
            bmRequestType,
            DFU_DNLOAD,
            block_number,  # wValue = block number
            interface,
            data_ptr,
            data_len,
            5000
        )
        
        if ret < 0:
            raise Exception(f"DFU_DNLOAD 失败：错误代码 {ret}")
        
        return ret >= 0
    
    def dfu_clear_status(self, interface=0):
        """清除 DFU 错误状态"""
        bmRequestType = 0x21 | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE
        
        ret = self.libusb.libusb_control_transfer(
            self.handle,
            bmRequestType,
            DFU_CLRSTATUS,
            0,
            interface,
            None,
            0,
            1000
        )
        
        return ret >= 0
    
    def dfu_abort(self, interface=0):
        """发送 DFU ABORT 命令"""
        bmRequestType = 0x21 | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE
        
        ret = self.libusb.libusb_control_transfer(
            self.handle,
            bmRequestType,
            DFU_ABORT,
            0,
            interface,
            None,
            0,
            1000
        )
        
        return ret >= 0
    
    def dfu_wait_idle(self, interface=0, timeout=5000):
        """等待 DFU 进入空闲状态"""
        import time
        
        start_time = time.time()
        while (time.time() - start_time) * 1000 < timeout:
            status = self.dfu_get_status(interface)
            if status:
                state = status['bState']
                if state == 5:  # dfuDNLOAD_IDLE
                    return True
                elif state == 10:  # dfuERROR
                    self.dfu_clear_status(interface)
            time.sleep(0.01)
        
        return False


# 测试代码
if __name__ == '__main__':
    print("=" * 70)
    print("USB DFU 库测试")
    print("=" * 70)
    
    try:
        device = USBDFUDevice()
        print("[OK] USBDFUDevice 创建成功")
        
        print("\n打开设备 (VID: 0x0483, PID: 0xDF11)...")
        if device.open(vid=0x0483, pid=0xDF11):
            print("[OK] 设备已打开")
            
            print("\n声明接口...")
            device.claim_interface(0)
            print("[OK] 接口已声明")
            
            print("\n获取 DFU 状态...")
            status = device.dfu_get_status()
            if status:
                state_names = {
                    0: 'appIDLE', 1: 'appDETACH', 2: 'dfuIDLE',
                    3: 'dfuDNLOAD_SYNC', 4: 'dfuDNBUSY', 5: 'dfuDNLOAD_IDLE',
                    6: 'dfuMANIFEST_SYNC', 7: 'dfuMANIFEST', 8: 'dfuMANIFEST_WAIT_RESET',
                    9: 'dfuUPLOAD_IDLE', 10: 'dfuERROR'
                }
                print(f"[OK] 状态：{state_names.get(status['bState'], 'Unknown')}")
            else:
                print("[FAIL] 无法获取状态")
            
            print("\n释放接口...")
            device.release_interface(0)
            print("[OK] 接口已释放")
            
            print("\n关闭设备...")
            device.close()
            print("[OK] 设备已关闭")
        else:
            print("[FAIL] 无法打开设备")
            print("请确保单片机已进入 BootLoader 模式")
        
        print("\n" + "=" * 70)
        print("测试完成")
        print("=" * 70)
        
    except Exception as e:
        print(f"\n[ERROR] 测试失败：{e}")
        import traceback
        traceback.print_exc()
