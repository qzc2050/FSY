import os
import re
import sys
import serial
import serial.tools.list_ports
import time
import datetime
import tkinter as tk
from tkinter import ttk, filedialog, messagebox, scrolledtext
from tkinter.ttk import Progressbar
import threading

import math
import struct
from intelhex import IntelHex

# YMODEM 协议常量
SOH = 0x01  # 128 字节数据包开始标志
EOT = 0x04  # 文件传输结束标志
ACK = 0x06  # 确认
NAK = 0x15  # 否定确认
CAN = 0x18  # 取消传输
ERR = 0xDF  # 异常错误
CRC16_POLY = 0x1021  # CRC-16 多项式

# 串口配置
SERIAL_PORT = None
BAUD_RATE = 115200
STOP_BITS = serial.STOPBITS_ONE
PARITY = serial.PARITY_NONE
BYTE_SIZE = serial.EIGHTBITS
TIMEOUT = 0.5

#重发变量配置
RESEND_NULL = 0
RESEND_DET = 1
RESEND_REQ = 2
RESEND_RUN = 3

# 全局变量
file_paths = []
is_serial_open = False
resend_file = RESEND_NULL
iap_update = False
bootloader_update = False
current_file_index = 0
is_ymodem_transfer = False
reconnect_attempts = 0
original_port = ""
serial_config = {}
software_ver = "V1.0.250520"

dev_sn = None  # 序列号
dev_version = None  # 版本号
dev_sensitivity = None  # 灵敏度

# 定义正则表达式用于匹配数据格式
SENSITIVITY_PATTERN = re.compile(r"SEN: ([0-9.]+) cpm/uSv/h\r\n")
SN_PATTERN = re.compile(r"SN: ([^\r\n]+)\r\n")
VERSION_PATTERN = re.compile(r"Ver\. ([0-9A-Fa-f]{2})([0-9A-Fa-f]{8})\r\n")


def calculate_checksum(data):
    """计算字节数据的异或校验和"""
    checksum = 0
    for byte in data:
        checksum ^= byte
    return checksum


def hex_to_bin(hex_file):
    """将 HEX 文件转换为 BIN 文件"""
    hex = IntelHex(hex_file)
    bin_file = hex_file.replace(".hex", ".bin")
    hex.tofile(bin_file, format='bin')  # 使用 tofile 方法并指定格式为 bin
    return bin_file


def enter_bootloader_mode(ser):
    retries = 0
    max_retries = 5  # 最大重试次数
    retry_delay = 0.5  # 每次重试的延迟时间（秒）
    
    while retries < max_retries:
        try:
            ser.reset_input_buffer()
            ser.reset_output_buffer()
            time.sleep(0.1)  # 等待清空完成
            ser.setRTS(True)  # NRST 拉低
            ser.setDTR(False)
            time.sleep(0.5)  # 等待 BOOT0 稳定
            ser.setRTS(False)  # NRST 拉高
            time.sleep(0.5)  # 等待 STM32 启动 Bootloader

            # 检查串口是否仍然打开
            if ser.is_open:
                if not bootloader_update:
                    raise Exception("用户取消更新！")
                for _ in range(2):  # 重试 2 次
                    ser.write(bytes([0x7F]))
                    log_sent_data(bytes([0x7F]));
                    response = ser.read(1)
                    log_received_data(response, is_hex=True)

                    if response == b'\x79':
                        log("成功进入 Bootloader！")
                        return True
                raise Exception("未收到 ACK！")
            else:
                raise serial.SerialException("串口断开！")

        except serial.SerialException as e:
            log(f"串口断开，尝试重新连接 ({retries + 1}/{max_retries})...", "error")
            retries += 1
            time.sleep(retry_delay)  # 等待一段时间后重试
            try:
                # 尝试重新打开串口
                ser.close()
                ser.open()
                for _ in range(2):  # 重试 2 次
                    if not bootloader_update:
                        raise Exception("用户取消更新！")
                    ser.write(bytes([0x7F]))
                    log_sent_data(bytes([0x7F]));
                    response = ser.read(1)
                    log_received_data(response, is_hex=True)

                    if response == b'\x79':
                        log("成功进入 Bootloader")
                        return True
                raise Exception("未收到 ACK")
            except serial.SerialException:
                continue
    log("无法重新连接串口，进入 Bootloader 模式失败", "error")
    return False


def send_extended_erase_command(ser, pages):
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    
    # 步骤 1: 发送扩展擦除命令 (0x44) 和校验和
    erase_command = 0x44
    checksum = erase_command ^ 0xFF
    
    ser.write(bytes([erase_command, checksum]))
    log_sent_data(bytes([erase_command, checksum]))
    # 检查是否收到 ACK
    response = ser.read(1)
    log_received_data(response, is_hex=True)
    if response != b'\x79':
        return False

    # 步骤 2: 准备页数据
    num_pages = len(pages) - 1  # N = 页数 - 1
    page_data = struct.pack(">H", num_pages)  # 页数 (2 字节，大端序)
    checksum = num_pages & 0xFF  # 页数校验和 (低字节)

    # 添加每个页的编号
    for page in pages:
        page_bytes = struct.pack(">H", page)  # 页号 (2 字节，大端序)
        page_data += page_bytes
        checksum ^= page_bytes[0] ^ page_bytes[1]  # 计算校验和

    # 添加校验和字节
    page_data += bytes([checksum])

    # 步骤 3: 发送页数据
    ser.write(page_data)
    log_sent_data(page_data)
    response = ser.read(1)
    log_received_data(response, is_hex=True)
    if response != b'\x79':
        return False
    return True


def erase_all_pages(ser, total_pages):
    for page in range(total_pages):
        if not bootloader_update:
            raise Exception("用户取消更新！")
        success = send_extended_erase_command(ser, [page])
        if not success:
            raise Exception("擦除页 {page} 失败！")
    log("全部分页擦除成功")
    return True

def send_write_memory_command(ser, address, data):
    # 数据分块，每次发送最大 256 字节
    block_size = 256
    data_len = len(data)
    total_blocks = (data_len + block_size - 1) // block_size  # 计算总共多少块
    log(f"数据总长度：{data_len} 字节，数据块总数：{total_blocks}")

    for block_num in range(total_blocks):
        if not bootloader_update:
            raise Exception("用户取消更新！")
        start = block_num * block_size
        end = min(start + block_size, data_len)
        block_data = data[start:end]

        # 步骤 1: 启动 Write Memory 命令 (0x31) 和校验字节 (0xCE)
        cmd = b"\x31\xCE"
        ser.write(cmd)
        log_sent_data(cmd);
        
        # 步骤 2: 等待 ACK 或 NACK
        response = ser.read(1)
        log_received_data(response, is_hex=True)
        
        if response != b'\x79':  # 如果没有 ACK
            raise Exception("未收到 ACK，命令初始化失败！")

        # 步骤 3: 发送地址和校验和（4 字节地址，1 字节校验和）
        address_bytes = struct.pack(">I", address + start)  # 将地址打包为 4 字节（大端字节序）
        address_checksum = calculate_checksum(address_bytes)  # 计算地址的校验和
        ser.write(address_bytes + bytes([address_checksum]))  # 一次性发送地址和校验和
        log_sent_data(address_bytes + bytes([address_checksum]));

        # 步骤 4: 等待 ACK 或 NACK
        response = ser.read(1)
        log_received_data(response, is_hex=True)

        if response != b'\x79':  # 如果没有 ACK
            raise Exception("未收到 ACK，地址校验失败！")

        # 步骤 5: 填充数据长度为 4 字节的倍数
        nr_of_bytes = len(block_data)
        if nr_of_bytes % 4 != 0:
            padding_bytes = 4 - (nr_of_bytes % 4)
            block_data += bytes([0xFF] * padding_bytes)  # 用 0xFF 填充到 4 字节倍数

        # 步骤 6: 计算数据块的校验和
        data_length_byte = nr_of_bytes - 1  # 数据长度字节，减去1，因为协议中 N 表示实际长度减1
        data_checksum = calculate_checksum([data_length_byte] + list(block_data))  # 数据长度和数据本身的校验和

        # 一次性发送数据长度、数据和校验和
        ser.write(bytes([data_length_byte]) + block_data + bytes([data_checksum]))
        log_sent_data(bytes([data_length_byte]) + block_data + bytes([data_checksum]));

        # 步骤 7: 等待 ACK 或 NACK
        response = ser.read(1)
        log_received_data(response, is_hex=True)
        
        if response != b'\x79':  # 如果没有 ACK
            raise Exception(f"数据块 {block_num + 1} 传输失败，未收到 ACK！")

        # 更新进度
        update_len = ((block_num + 1) / total_blocks) * 100
        if update_len >= 100:
            update_len = 100
        progress["value"] = update_len
        update_progress_percentage()
        root.update_idletasks()

    log("所有数据块写入成功！")

def bootloader_update_thread():
    global SERIAL_PORT, is_serial_open, file_paths, bootloader_update
    
    if not is_serial_open:
        log("错误: IAP串口未打开！", "error")
        return
    
    if not file_paths:
        log("错误: 未选择文件！", "error")
        return
    
    try:
        log("开始 BootLoader 更新")
        bootloader_update = True
        mask_button()
        
        serial_bootloader_config()
        
        # 1. 转换 HEX 文件为 BIN 文件
        bin_file = file_paths[0]
        
        # 2. 进入 Bootloader 模式
        if not enter_bootloader_mode(SERIAL_PORT):
            return
        
        # 3. 读取 BIN 文件数据
        with open(bin_file, "rb") as f:
            data = f.read()

        progress["value"] = 0
        progress["maximum"] = 100
        update_progress_percentage()
        root.update_idletasks()
        
        bin_size = round(len(data) / 1024, 2)
        pages = math.ceil(len(data)/ 128)
        log(f"BIN 文件长度: {bin_size}KB, 页数: {pages}")

        # 4. 分页擦除所有页
        log("开始擦除...")

        if not erase_all_pages(SERIAL_PORT, pages):
            return
        
        # 5. 发送写内存命令并显示进度
        log("开始写入...")
        address = 0x08000000  # Flash 起始地址
        send_write_memory_command(SERIAL_PORT, address, data)
        log("BootLoader 更新完成！")
        
    except Exception as e:
        log(f"BootLoader 更新失败: {str(e)}", "error")
    finally:
        log("正在跳转到应用程序...")
        jump_to_application(SERIAL_PORT)
         
        serial_cmd_config()  # 恢复串口配置
        cancel_mask_button()


def jump_to_application(ser):
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    
    try:
        # 步骤 1: 发送跳转命令头 (0x21) 和校验和
        jump_command = 0x21
        checksum = jump_command ^ 0xFF
        
        ser.write(bytes([jump_command, checksum]))
        log_sent_data(bytes([jump_command, checksum]))
        
        # 检查是否收到 ACK (0x79)
        response = ser.read(1)
        log_received_data(response, is_hex=True)
        if response != b'\x79':
            log("跳转命令未收到ACK", "error")
            return False

        # 步骤 2: 准备应用程序地址数据
        app_address = 0x08000000  # 根据实际应用程序地址修改
        address_bytes = app_address.to_bytes(4, 'big')  # 4字节大端序
        checksum = sum(address_bytes) & 0xFF  # 校验和（所有字节累加后取低字节）

        # 步骤 3: 发送地址数据 + 校验和
        data_to_send = address_bytes + bytes([checksum])
        ser.write(data_to_send)
        log_sent_data(data_to_send)
        
        # 检查是否收到 ACK
        response = ser.read(1)
        log_received_data(response, is_hex=True)
        if response != b'\x79':
            log("地址数据未收到ACK", "error")
            return False

        log("跳转到应用程序命令执行成功")
        return True
        
    except Exception as e:
        log(f"跳转失败: {str(e)}", "error")
        return False


def start_bootloader_update():
    threading.Thread(target=bootloader_update_thread,daemon = True).start()



# CRC-16 计算
def crc16_ccitt(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
            crc &= 0xFFFF
    return crc


# YMODEM 发送数据包
def send_packet(ser, packet_number, data, is_file_info=False):
    if not is_serial_open:
        raise Exception("串口已断开，停止发送数据包")
    ser.reset_input_buffer()  # 清空输入缓冲区
    packet = bytearray()
    packet.append(SOH)
    packet.append(packet_number)
    packet.append(0xFF - packet_number)
    packet.extend(data)
    if is_file_info:
        packet.extend(b'\x00' * (128 - len(data)))
    crc = crc16_ccitt(packet[3:])
    packet.append((crc >> 8) & 0xFF)
    packet.append(crc & 0xFF)
    ser.reset_output_buffer()  # 清空发送缓冲区
    ser.write(packet)
    return packet


# 等待应答
def wait_for_ack(ser, timeout=TIMEOUT):
    start_time = time.time()
    while time.time() - start_time < timeout:
        if ser.in_waiting > 0:
            response = ser.read(ser.in_waiting)
            log_received_data(response, is_hex=True)
            for byte in response:
                if byte == ACK:
                    return True
                elif byte == NAK:
                    return False
                elif byte == CAN:
                    raise Exception("传输被接收方取消")
            raise Exception("未知异常")

    raise Exception("应答等待超时 -> 设备状态异常/未正确打开串口")
    return False


# YMODEM 发送文件
def send_file(ser, file_path):  
    global iap_update, is_ymodem_transfer

    max_retries = 3  # 最大重试次数
    current_retry = 0
    success = False

    while current_retry < max_retries and not success:
        try:
            is_ymodem_transfer = True
            ser.reset_output_buffer()  # 清空发送缓冲区
            ser.write(bytes([CAN]))  # 发送CAN
            log_sent_data(bytes([CAN]))
            if not wait_for_ack(ser, timeout=0.5):
                log("重发指令应答接收失败！","tip")
            time.sleep(0.1)

            ser.write(bytes([CAN]))  # 发送CAN
            log_sent_data(bytes([CAN]))
            if not wait_for_ack(ser, timeout=0.5):
                log("重发指令应答接收失败！","tip")
            time.sleep(0.1)

            with open(file_path, "rb") as file:
                file_data = file.read()
                file_size = len(file_data)
                packet_number = 1  # 初始化包序号

                # ========== 发送文件信息包 ==========
                file_name = os.path.basename(file_path).encode("gb2312")
                file_info = file_name + b"\x00" + str(file_size).encode("gb2312")
                retry_count = 0
                while True:
                    try:
                        if not iap_update:
                            raise Exception("用户取消发送")
                        if not is_serial_open:
                            raise Exception("串口已断开")
                        
                        # 超过重试次数发送CAN并触发重试
                        if retry_count > 1:
                            ser.reset_output_buffer()  # 清空发送缓冲区
                            ser.write(bytes([CAN]))  # 发送CAN
                            log_sent_data(bytes([CAN]))
                            if not wait_for_ack(ser, timeout=0.5):
                                log("重发指令应答接收失败！","tip")
                            time.sleep(0.1)

                            ser.write(bytes([CAN]))  # 发送CAN
                            log_sent_data(bytes([CAN]))
                            if not wait_for_ack(ser, timeout=0.5):
                                log("重发指令应答接收失败！","tip")
                            time.sleep(0.1)
                            raise Exception("文件信息包重试次数超限")

                        # 发送数据包并等待响应
                        sent_packet = send_packet(ser, 0, file_info, is_file_info=True)
                        log_sent_data(sent_packet)
                        
                        if wait_for_ack(ser, timeout=0.5):
                            packet_number = 1  # 收到ACK后重置包序号为1
                            break
                        retry_count += 1

                    except Exception as e:
                        log("文件信息包发送失败！","error")
                        raise Exception(f"{str(e)}")

                # ========== 发送文件数据 ==========
                progress["maximum"] = file_size
                for i in range(0, file_size, 128):
                    if not iap_update or not is_serial_open:
                        break
                    
                    chunk = file_data[i:i+128]
                    if len(chunk) < 128:
                        chunk = chunk.ljust(128, b"\x00")
                    
                    data_retry_count = 0
                    while True:
                        try:
                            # 超过重试次数发送CAN并触发重试
                            if data_retry_count > 3:
                                ser.reset_output_buffer()  # 清空发送缓冲区
                                ser.write(bytes([CAN]))  # 发送CAN
                                log_sent_data(bytes([CAN]))
                                if not wait_for_ack(ser, timeout=0.5):
                                    log("重发指令应答接收失败！","tip")
                                time.sleep(0.1)
                                
                                raise Exception("数据包重试次数超限")

                            # 发送数据包并等待响应
                            sent_packet = send_packet(ser, packet_number, chunk)
                            log_sent_data(sent_packet)
                            
                            if wait_for_ack(ser, timeout=0.5):
                                packet_number = (packet_number + 1) % 256  # 收到ACK后递增包序号
                                break
                            data_retry_count += 1

                        except Exception as e:
                            log("数据包发送失败！","error")
                            raise Exception(f"{str(e)}")

                    # 更新进度
                    progress["value"] = i + 128
                    update_progress_percentage()
                    root.update_idletasks()

                # ========== 发送结束标志 ==========
                if iap_update and is_serial_open:
                    ser.reset_output_buffer()  # 清空发送缓冲区
                    ser.write(bytes([EOT]))
                    log_sent_data(bytes([EOT]))

                log(f"文件 {os.path.basename(file_path)} 发送成功！")
                success = True

        except Exception as e:
            raise Exception(f"{str(e)}")

        finally:
            is_ymodem_transfer = False

    if not success:
        log(f"文件传输失败，已达最大重试次数 {max_retries} 次", "error")
        iap_update = False
        raise Exception("文件传输失败")


# 增强型串口重连逻辑
def reconnect_serial():
    global SERIAL_PORT, is_serial_open, reconnect_attempts, original_port
    max_attempts = 2
    success = False
    
    while reconnect_attempts < max_attempts and not success:
        reconnect_attempts += 1
        try:
            SERIAL_PORT = serial.Serial(
                port=original_port,
                baudrate=serial_config.get('baudrate', BAUD_RATE),
                bytesize=serial_config.get('bytesize', BYTE_SIZE),
                parity=serial_config.get('parity', PARITY),
                stopbits=serial_config.get('stopbits', STOP_BITS),
                timeout=TIMEOUT
            )
            is_serial_open = True
            success = True
            root.after(0, lambda: [
                serial_button.config(text="关闭串口"),
                port_combobox.set(original_port)
            ])
            threading.Thread(target=listen_serial, daemon=True).start()
        except Exception as e:
            time.sleep(0.5)
    
    if not success:
        root.after(0, lambda: serial_button.config(text="打开串口"))
        is_serial_open = False


# 更新进度条百分比
def update_progress_percentage():
    if progress["maximum"] > 0:
        percentage = (progress["value"] / progress["maximum"]) * 100
        percentage = min(percentage, 100.0)
        progress_percentage_label.config(text=f"{percentage:.2f}%")


# 打开/关闭串口
def toggle_serial():
    global SERIAL_PORT, is_serial_open, BAUD_RATE, original_port
    if is_serial_open:
        try:
            SERIAL_PORT.close()
        except Exception as e:
            log(f"关闭串口时出错: {str(e)}", "error")
        serial_button.config(text="打开串口")
        is_serial_open = False
    else:
        try:
            if SERIAL_PORT:
                if SERIAL_PORT.is_open:
                    SERIAL_PORT.close()
                
            original_port = port_combobox.get()
            SERIAL_PORT = serial.Serial(
                port=original_port,
                baudrate=BAUD_RATE,
                bytesize=BYTE_SIZE,
                parity=PARITY,
                stopbits=STOP_BITS,
                timeout=TIMEOUT
            )
            serial_button.config(text="关闭串口")
            is_serial_open = True
            log(f"串口 {original_port} 已成功打开")
            # 更新端口显示
            port_combobox.set(original_port)
            # 启动后台线程监听串口数据
            threading.Thread(target=listen_serial, daemon=True).start()
        except Exception as e:
            log(f"错误: {str(e)}", "error")


# 清空接收
def clear_log():
    log_area.config(state="normal")
    log_area.delete("1.0", tk.END)
    log_area.config(state="disabled")


def listen_serial():
    global SERIAL_PORT, is_serial_open, original_port, serial_config, reconnect_attempts, dev_sensitivity, dev_sn, dev_version
    
    # 用于累积接收到的数据
    received_data = ""

    while is_serial_open:
        try:
            if SERIAL_PORT.baudrate != BAUD_RATE:
                return
            if SERIAL_PORT.parity != PARITY:
                return
            if SERIAL_PORT.stopbits != STOP_BITS:
                return
            if iap_update or resend_file:
                    return
                
            if SERIAL_PORT.in_waiting > 0: 
                data = SERIAL_PORT.read(SERIAL_PORT.in_waiting)
                log_received_data(data, is_hex=False)
                
                # 将接收到的数据解码为字符串并累积
                received_data += data.decode('gb2312', errors='ignore')
                
                # 检查累积的数据是否包含完整的格式
                while True:
                    # 检查灵敏度信息
                    sensitivity_match = SENSITIVITY_PATTERN.search(received_data)
                    if sensitivity_match:
                        dev_sensitivity = float(sensitivity_match.group(1))
##                        log(f"灵敏度更新: {dev_sensitivity} cpm/uSv/h", "info")
                        sensitivity_entry.delete(0, tk.END)  # 清空输入框内容
                        sensitivity_entry.insert(0, f"{dev_sensitivity:.2f}")
                        received_data = received_data[sensitivity_match.end():]  # 移除已处理的部分
                        continue
                    
                    # 检查序列号信息
                    sn_match = SN_PATTERN.search(received_data)
                    if sn_match:
                        dev_sn = sn_match.group(1)
##                        log(f"序列号更新: {dev_sn}", "info")
                        serial_number_entry.delete(0, tk.END)  # 清空输入框内容
                        serial_number_entry.insert(0, str(dev_sn))
                        received_data = received_data[sn_match.end():]  # 移除已处理的部分
                        continue
                    
                    # 检查版本号信息
                    version_match = VERSION_PATTERN.search(received_data)
                    if version_match:
                        dev_version = version_match.group(1) + version_match.group(2)
##                        log(f"版本号更新: {dev_version}", "info")
                        software_version.config(text=str(dev_version))
                        received_data = received_data[version_match.end():]  # 移除已处理的部分
                        continue
                    
                    # 如果没有匹配到任何信息，退出循环
                    break
                
            time.sleep(0.1)
        except (serial.SerialException, AttributeError) as e:
            if SERIAL_PORT.baudrate != BAUD_RATE:
                return
            if SERIAL_PORT.parity != PARITY:
                return
            if SERIAL_PORT.stopbits != STOP_BITS:
                return
            if iap_update or resend_file:
                return
            
            if is_serial_open:
                try:
                    SERIAL_PORT.close()
                except:
                    pass
                is_serial_open = False
                root.after(0, lambda: [
                    serial_button.config(text="打开串口"),
                    port_combobox.set("")
                ])
                serial_config = {
                    'baudrate': BAUD_RATE,
                    'bytesize': BYTE_SIZE,
                    'parity': PARITY,
                    'stopbits': STOP_BITS
                }
                reconnect_attempts = 0
                threading.Thread(target=reconnect_serial, daemon=True).start()
            break
        except Exception as e:
            log(f"监听错误: {str(e)}", "error")
            break


# 刷新串口列表
def refresh_ports():
    global is_serial_open
    
    ports = serial.tools.list_ports.comports()
    current_port = port_combobox.get()
    available_ports = [port.device for port in ports]
    port_combobox["values"] = available_ports

    if current_port and current_port not in available_ports:
        if is_serial_open:
            try:
                SERIAL_PORT.close()
            except Exception as e:
                log(f"关闭串口时出错: {str(e)}", "error")
            serial_button.config(text="打开串口")
            is_serial_open = False
            
            log(f"串口 {current_port} 已拔出")
        port_combobox.set("")
    elif current_port in available_ports and is_serial_open:
        pass
    elif not current_port and available_ports:
        port_combobox.current(0)

    root.after(400, refresh_ports)


# 端口号选择事件绑定
def on_port_select(event):
    global SERIAL_PORT, is_serial_open, original_port
    selected_port = port_combobox.get()

    if is_serial_open:
        SERIAL_PORT.close()
        is_serial_open = False
        serial_button.config(text="打开串口")
        log(f"串口 {original_port}已关闭")

    if selected_port:
        try:
            original_port = selected_port
            SERIAL_PORT = serial.Serial(
                port=selected_port,
                baudrate=BAUD_RATE,
                bytesize=BYTE_SIZE,
                parity=PARITY,
                stopbits=STOP_BITS,
                timeout=TIMEOUT
            )
            is_serial_open = True
            serial_button.config(text="关闭串口")
            log(f"已打开串口 {selected_port}")
            port_combobox.set(selected_port)  # 确保显示当前选择的端口
            threading.Thread(target=listen_serial, daemon=True).start()
        except Exception as e:
            log(f"错误: 无法打开串口 {selected_port}: {str(e)}", "error")



# 串口参数配置（发送指令）
def serial_cmd_config():
    global SERIAL_PORT

    if not is_serial_open:
        log("错误: cmd串口未打开！", "error")  # 红色显示错误信息
        return

    SERIAL_PORT.close()
    time.sleep(0.010)
    SERIAL_PORT = serial.Serial(
        port=port_combobox.get(),
        baudrate=BAUD_RATE,
        bytesize=BYTE_SIZE,
        parity=PARITY,
        stopbits=STOP_BITS,
        timeout=TIMEOUT
    )
    log("CMD/IAP串口配置！", "tip")
    threading.Thread(target=listen_serial, daemon=True).start()


# 串口参数配置（APP）
def serial_iap_config():
    global SERIAL_PORT

    if not is_serial_open:
        log("错误: iap串口未打开！", "error")  # 红色显示错误信息
        return

    SERIAL_PORT.close()
    time.sleep(0.010)
    SERIAL_PORT = serial.Serial(
        port=port_combobox.get(),
        baudrate=921600,
        bytesize=BYTE_SIZE,
        parity=PARITY,
        stopbits=serial.STOPBITS_TWO,
        timeout=TIMEOUT                  
    )
    log("APP串口配置！", "tip")
        

# 串口参数配置（BOOTLOADER）
def serial_bootloader_config():
    global SERIAL_PORT

    if not is_serial_open:
        log("错误: bootloader串口未打开！", "error")  # 红色显示错误信息
        return

    SERIAL_PORT.close()
    time.sleep(0.010)
    SERIAL_PORT = serial.Serial(
        port=port_combobox.get(),
        baudrate=115200,
        bytesize=BYTE_SIZE,
        parity=serial.PARITY_EVEN,    ##一定要偶校验
        stopbits=serial.STOPBITS_ONE,
        timeout=TIMEOUT
    )
    log("Bootloader串口配置！", "tip")


# 打开文件
def open_file():
    global file_paths
    file_types = [
        ("BIN 文件", "*.bin"),
        ("HEX 文件", "*.hex"),
        ("BIN/HEX 文件", "*.bin *.hex"),
##        ("所有文件", "*.*")
    ]
    file_paths = filedialog.askopenfilenames(filetypes=file_types)  # 支持多文件选择
    if file_paths:
        file_label.config(state="normal")
        file_label.delete(0, tk.END)
        file_label.insert(0, os.path.basename(file_paths[0]))  # 显示第一个文件名
        file_label.config(state="readonly")
        log(f"已选择文件: {', '.join(file_paths)}")

        # 检查每个文件的扩展名，并替换 HEX 文件为转换后的 BIN 文件
        new_file_paths = []
        for file_path in file_paths:
            if file_path.lower().endswith('.hex'):
                # 如果是 HEX 文件，转换为 BIN 文件
                log("执行：HEX 文件 -> BIN 文件")
                bin_file_path = hex_to_bin(file_path)  # 假设 hex_to_bin 返回转换后的 BIN 文件路径
                new_file_paths.append(bin_file_path)
            else:
                # 如果不是 HEX 文件，直接保留
                new_file_paths.append(file_path)
        
        # 更新 file_paths 为替换后的文件列表
        file_paths = tuple(new_file_paths)
    else:
        file_label.config(state="normal")
        file_label.delete(0, tk.END)
        file_label.config(state="readonly")


# 发送文件
def start_send():
    global iap_update, current_file_index, resend_file
    if not is_serial_open:
        iap_update = False
        cancel_mask_button()
        resend_file = RESEND_NULL
        log("错误: APP串口未打开！", "error")  # 红色显示错误信息
        return
    if not file_paths:
        log("错误: 未选择文件！", "error")  # 红色显示错误信息
        return
    if iap_update:
        log("错误: 文件传输正在进行中！", "error")  # 红色显示错误信息
        return
    
    send_req_program()
    time.sleep(0.25)
    send_req_program()
    time.sleep(0.25)
    # 配置串口传输参数
    serial_iap_config()
    
    if not is_serial_open:
        iap_update = False
        cancel_mask_button()
        resend_file = RESEND_NULL
        log("错误: 串口未正确打开！", "error")  # 红色显示错误信息
        return
		
    iap_update = True
    current_file_index = 0

    if resend_file == RESEND_NULL:
        resend_file = RESEND_DET
        threading.Thread(target=restart_send_detect, daemon=True).start()
    if resend_file == RESEND_RUN:
        resend_file = RESEND_NULL
    # 使用多线程发送文件
    threading.Thread(target=send_next_file, daemon=True).start()

# 发送文件
def restart_send_detect():
    global resend_file

    while resend_file != RESEND_NULL:
        if resend_file == RESEND_REQ:
            # 使用多线程发送文件
            resend_file = RESEND_RUN
            threading.Thread(target=start_send, daemon=True).start()
        time.sleep(0.25)


# 发送下一个文件
def send_next_file():
    global iap_update, current_file_index, resend_file
    if current_file_index >= len(file_paths):
        if iap_update:  # 只有在发送过程中完成所有文件才显示提示
            iap_update = False
            resend_file = RESEND_NULL
            log("文件发送成功！")
            serial_cmd_config()
        return

    file_path = file_paths[current_file_index]
    try:
        log(f"正在发送文件: {os.path.basename(file_path)}")
        mask_button()
        send_file(SERIAL_PORT, file_path)
        cancel_mask_button()
        current_file_index += 1
        if iap_update and is_serial_open:  # 只有在发送过程中且串口未断开才继续发送下一个文件
            send_next_file()
    except Exception as e:
        iap_update = False
        if str(e) == "未知异常":
            if resend_file == RESEND_DET:
                resend_file = RESEND_REQ
                log("开启重发请求！", "tip")  # 红色显示错误信息
                return
            log(f"错误: {str(e)} -> {resend_file}", "error")  # 红色显示错误信息
        elif str(e) != "用户取消发送":  # 不显示“用户取消发送”的提示
            log(f"错误: {str(e)}", "error")  # 红色显示错误信息
        
        cancel_mask_button()
        resend_file = RESEND_NULL


# 停止发送
def stop_send():
    global iap_update, bootloader_update
    if iap_update:  # 只有在发送过程中点击“停止发送”才显示提示
        iap_update = False
        log("文件发送已停止。")
        progress["value"] = 0  # 重置进度条
        SERIAL_PORT.write(bytes([CAN]))
        log_sent_data(bytes([CAN]))
        update_progress_percentage()

    if bootloader_update:
        bootloader_update = False
        log("Bootloader 更新已停止。")
        progress["value"] = 0  # 重置进度条
        update_progress_percentage()


# 更新日志
def log(message, tag=None):
    log_area.config(state="normal")  # 临时启用文本框以插入日志
    log_area.insert(tk.END, message + "\n", tag)  # 根据 tag 设置颜色
    log_area.see(tk.END)  # 自动滚动到底部
    log_area.config(state="disabled")  # 恢复只读状态


# 记录已发送的数据（蓝色显示）
def log_sent_data(data):
    hex_data = ' '.join(f'{byte:02X}' for byte in data)  # 将数据转换为十六进制字符串
    log_area.config(state="normal")  # 临时启用文本框以插入日志
    log_area.insert(tk.END, "\n", "sent")  # 显示 "Send:" 并换行
    # 每 16 个数据为一行
    for i in range(0, len(hex_data.split()), 16):
        line = ' '.join(hex_data.split()[i:i + 16])  # 每行最多 16 个数据
        log_area.insert(tk.END, line + "\n", "sent")  # 显示数据并换行
    log_area.see(tk.END)  # 自动滚动到底部
    log_area.config(state="disabled")  # 恢复只读状态


# 记录接收的数据（黄色显示）
def log_received_data(data, is_hex=False):
    log_area.config(state="normal")  # 临时启用文本框以插入日志
    log_area.insert(tk.END, "", "received")  # 显示 "Receive:" 并换行
    if is_hex:
        # 以十六进制显示
        hex_data = ' '.join(f'{byte:02X}' for byte in data)
        for i in range(0, len(hex_data.split()), 16):
            line = ' '.join(hex_data.split()[i:i + 16])  # 每行最多 16 个数据
            log_area.insert(tk.END, line + "\n", "received")  # 显示数据并换行
    else:
        # 以字符显示
        try:
            text = data.decode("gb2312", errors="replace")  # 尝试解码为 GB2312
            log_area.insert(tk.END, text + "\n", "received")  # 显示字符数据
        except Exception as e:
            log(f"错误: 无法解码接收到的数据 - {str(e)}", "error")  # 红色显示错误信息
    log_area.see(tk.END)  # 自动滚动到底部
    log_area.config(state="disabled")  # 恢复只读状态


# 发送序列号
def send_serial_number():
    if not is_serial_open:
        log("错误: 串口未打开！", "error")  # 红色显示错误信息
        return

    serial_number = serial_number_entry.get().strip()
    if not serial_number:
        log("错误: 序列号不能为空！", "error")  # 红色显示错误信息
        return

    command = f"setSN,{serial_number},end\r\n"
    try:
        SERIAL_PORT.reset_output_buffer()  # 清空发送缓冲区
        SERIAL_PORT.write(command.encode("gb2312"))
        log(f"{command.strip()}", "sent")
    except Exception as e:
        log(f"错误: 序列号修改失败 - {str(e)}", "error")  # 红色显示错误信息


# 发送灵敏度
def send_sensitivity():
    if not is_serial_open:
        log("错误: 串口未打开！", "error")  # 红色显示错误信息
        return

    sensitivity = sensitivity_entry.get().strip()
    if not sensitivity:
        log("错误: 灵敏度不能为空！", "error")  # 红色显示错误信息
        return

    command = f"setsen,{sensitivity},end\r\n"
    try:
        SERIAL_PORT.reset_output_buffer()  # 清空发送缓冲区
        SERIAL_PORT.write(command.encode("gb2312"))
        log(f"{command.strip()}", "sent")
    except Exception as e:
        log(f"错误: 灵敏度设置失败 - {str(e)}", "error")  # 红色显示错误信息


# 发送日期时间
def send_datetime():
    if not is_serial_open:
        log("错误: 串口未打开！", "error")  # 红色显示错误信息
        return

    current_time = datetime.datetime.now()  # 获取当前时间并格式化
    date_str = current_time.strftime("%y%m%d")  # 获取日期，格式为年月日
    time_str = current_time.strftime("%H%M%S")  # 获取时间，格式为时分秒
    
    command = f'setptime,{date_str},{time_str},end\r\n'
    try:
        SERIAL_PORT.reset_output_buffer()  # 清空发送缓冲区
        SERIAL_PORT.write(command.encode("gb2312"))
        log(f"{command.strip()}", "sent")
    except Exception as e:
        log(f"错误: 日期时间设置失败 - {str(e)}", "error")  # 红色显示错误信息


# 发送获取历史记录指令
def send_history():
    if not is_serial_open:
        log("错误: 串口未打开！", "error")  # 红色显示错误信息
        return

    command = f'F1\r\n'
    try:
        SERIAL_PORT.reset_output_buffer()  # 清空发送缓冲区
        SERIAL_PORT.write(command.encode("gb2312"))
        log(f"{command.strip()}", "sent")
    except Exception as e:
        log(f"错误: 获取历史记录失败 - {str(e)}", "error")  # 红色显示错误信息


# 发送恢复出厂设置指令
def send_status_reset():
    if not is_serial_open:
        log("错误: 串口未打开！", "error")  # 红色显示错误信息
        return

    command = f'CU\r\n'
    try:
        SERIAL_PORT.reset_output_buffer()  # 清空发送缓冲区
        SERIAL_PORT.write(command.encode("gb2312"))
        log(f"{command.strip()}", "sent")
    except Exception as e:
        log(f"错误: 恢复出厂设置失败 - {str(e)}", "error")  # 红色显示错误信息


# 发送报警测试指令
def send_alarm_test():
    if not is_serial_open:
        log("错误: 串口未打开！", "error")  # 红色显示错误信息
        return

    command = f'BT\r\n'
    try:
        SERIAL_PORT.reset_output_buffer()  # 清空发送缓冲区
        SERIAL_PORT.write(command.encode("gb2312"))
        log(f"{command.strip()}", "sent")
    except Exception as e:
        log(f"错误: 报警测试模式切换失败 - {str(e)}", "error")  # 红色显示错误信息


# 发送待机测试指令
def send_aging_test():
    if not is_serial_open:
        log("错误: 串口未打开！", "error")  # 红色显示错误信息
        return

    command = f'AT\r\n'
    try:
        SERIAL_PORT.reset_output_buffer()  # 清空发送缓冲区
        SERIAL_PORT.write(command.encode("gb2312"))
        log(f"{command.strip()}", "sent")
    except Exception as e:
        log(f"错误: 待机测试模式切换失败 - {str(e)}", "error")  # 红色显示错误信息


# 发送设备信息指令
def send_device_info():
    if not is_serial_open:
        log("错误: 串口未打开！", "error")  # 红色显示错误信息
        return

    command = f'SN\r\n'
    try:
        SERIAL_PORT.reset_output_buffer()  # 清空发送缓冲区
        SERIAL_PORT.write(command.encode("gb2312"))
        log(f"{command.strip()}", "sent")
    except Exception as e:
        log(f"错误: 获取设备信息失败 - {str(e)}", "error")  # 红色显示错误信息


# 发送清除功耗时间指令
def send_clr_pt():
    if not is_serial_open:
        log("错误: 串口未打开！", "error")  # 红色显示错误信息
        return

    command = f'CP\r\n'
    try:
        SERIAL_PORT.reset_output_buffer()  # 清空发送缓冲区
        SERIAL_PORT.write(command.encode("gb2312"))
        log(f"{command.strip()}", "sent")
    except Exception as e:
        log(f"错误: 清除功耗时间失败 - {str(e)}", "error")  # 红色显示错误信息


# 发送请求更新程序指令
def send_req_program():
    if not is_serial_open:
        log("错误: 串口未打开！", "error")  # 红色显示错误信息
        return
    
    command = f'CC\r\n'
    try:
        SERIAL_PORT.reset_output_buffer()  # 清空发送缓冲区
        SERIAL_PORT.write(command.encode("gb2312"))
        log(f"{command.strip()}", "sent")
    except Exception as e:
        log(f"错误: 请求更新程序失败 - {str(e)}", "error")  # 红色显示错误信息




# 传输文件时屏蔽部分按键
def mask_button():
    serial_button.config(state=tk.DISABLED)
    port_combobox.config(state=tk.DISABLED)
    dev_info_button.config(state=tk.DISABLED)
    req_history_button.config(state=tk.DISABLED)
    status_reset_button.config(state=tk.DISABLED)
    datetime_sync_button.config(state=tk.DISABLED)
    clr_pt_button.config(state=tk.DISABLED)
    alarm_test_button.config(state=tk.DISABLED)
    aging_test_button.config(state=tk.DISABLED)
    send_sn_button.config(state=tk.DISABLED)
    send_sen_button.config(state=tk.DISABLED)
    file_button.config(state=tk.DISABLED)
    send_button.config(state=tk.DISABLED)
    bootload_button.config(state=tk.DISABLED)


# 取消按键屏蔽
def cancel_mask_button():
    serial_button.config(state=tk.NORMAL)
    port_combobox.config(state=tk.NORMAL)
    dev_info_button.config(state=tk.NORMAL)
    req_history_button.config(state=tk.NORMAL)
    status_reset_button.config(state=tk.NORMAL)
    datetime_sync_button.config(state=tk.NORMAL)
    clr_pt_button.config(state=tk.NORMAL)
    alarm_test_button.config(state=tk.NORMAL)
    aging_test_button.config(state=tk.NORMAL)
    send_sn_button.config(state=tk.NORMAL)
    send_sen_button.config(state=tk.NORMAL)
    file_button.config(state=tk.NORMAL)
    send_button.config(state=tk.NORMAL)
    bootload_button.config(state=tk.NORMAL)

def set_icon(root):
    try:
        # 打包后，图标在临时目录 _MEIPASS 下
        base_path = sys._MEIPASS
    except AttributeError:
        # 开发时，直接在当前目录下
        base_path = os.path.dirname(__file__)
    
    icon_path = os.path.join(base_path, "IAD-I.ico")
    if os.path.exists(icon_path):
        root.iconbitmap(icon_path)
    else:
        print(f"警告：找不到图标文件 {icon_path}")


def ui_close():
    # 关闭串口连接
    if hasattr(root, 'serial_conn') and root.serial_conn and root.serial_conn.is_open:
        root.serial_conn.close()
    
    # 销毁窗口
    root.destroy()


# 创建 GUI
root = tk.Tk()
root.title("IAD-I")

# 设置关闭窗口时的回调函数
root.protocol("WM_DELETE_WINDOW", ui_close)

# 设置默认窗口大小
root.geometry("800x600")

# 设置窗口最小大小
root.minsize(800, 600)

##root.iconbitmap('IAD-I.ico')  # 设置窗口图标
set_icon(root)  # 替换原来的 root.iconbitmap('IAD-I.ico')

# 配置 grid 布局权重
root.rowconfigure(0, weight=1)  # 第一行（串口数据和参数配置）占满剩余空间
root.columnconfigure(0, weight=1)  # 第一列（串口数据）占满剩余空间
root.columnconfigure(1, weight=0)  # 第二列（参数配置和串口配置）固定宽度

# 串口数据显示（第一行第一列）
log_frame = ttk.LabelFrame(root, text="串口数据", padding=10)
log_frame.grid(row=0, column=0, padx=10, pady=10, sticky="nsew")

# 配置 log_frame 的 grid 权重
log_frame.rowconfigure(0, weight=1)
log_frame.columnconfigure(0, weight=1)

log_area = scrolledtext.ScrolledText(log_frame, wrap=tk.WORD, height=20, width=60, bg="#D3D3D3", state="disabled")
log_area.grid(row=0, column=0, padx=5, pady=5, sticky="nsew")

# 设置发送和接收数据的颜色
log_area.tag_config("sent", foreground="magenta")  # 发送数据为蓝色
log_area.tag_config("received", foreground="#7F39E9")  # 接收数据为自定义紫色
##log_area.tag_config("received", foreground="magenta")  # 接收数据为平红色
##log_area.tag_config("received", foreground="purple")  # 接收数据为紫色
log_area.tag_config("error", foreground="red")  # 错误信息为红色
log_area.tag_config("tip", foreground="yellow")  # 调试信息为黄色

# 串口操作框
config_frame = ttk.LabelFrame(root, text="串口操作", padding=0, width=265, height=86)
config_frame.grid(row=0, column=1, padx=(0,10), pady=10, sticky="nw")
config_frame.grid_propagate(False)  # 禁止自动调整大小

# 端口号
ttk.Label(config_frame, text="端口号:").grid(row=0, column=0, padx=10, pady=15, sticky="nw")
port_combobox = ttk.Combobox(config_frame, width=10)
port_combobox.grid(row=0, column=0, padx=64, pady=15, sticky="nw")
port_combobox.grid_propagate(False)  # 禁止自动调整大小

### 波特率
##ttk.Label(config_frame, text="波特率:").grid(row=1, column=0, padx=5, pady=5, sticky="ew")
##baudrate_options = [
##    "110", "300", "600", "1200", "2400", "4800", "9600", "14400", "19200", "38400",
##    "43000", "57600", "76800", "115200", "128000", "230400", "256000", "460800",
##    "921600", "1000000", "2000000", "3000000"
##]
##baudrate_combobox = ttk.Combobox(config_frame, values=baudrate_options, width=15)
##baudrate_combobox.grid(row=1, column=1, padx=5, pady=5)
##baudrate_combobox.current(baudrate_options.index("115200"))  # 默认选择 115200
##
### 停止位
##ttk.Label(config_frame, text="停止位:").grid(row=2, column=0, padx=5, pady=5, sticky="ew")
##stopbits_options = ["1", "1.5", "2"]
##stopbits_combobox = ttk.Combobox(config_frame, values=stopbits_options, width=15)
##stopbits_combobox.grid(row=2, column=1, padx=5, pady=5)
##stopbits_combobox.current(0)  # 默认选择 1
##
### 数据位
##ttk.Label(config_frame, text="数据位:").grid(row=3, column=0, padx=5, pady=5, sticky="ew")
##databits_options = ["5", "6", "7", "8"]
##databits_combobox = ttk.Combobox(config_frame, values=databits_options, width=15)
##databits_combobox.grid(row=3, column=1, padx=5, pady=5)
##databits_combobox.current(3)  # 默认选择 8
##
### 校验位
##ttk.Label(config_frame, text="校验位:").grid(row=4, column=0, padx=5, pady=5, sticky="ew")
##parity_options = ["无", "奇校验", "偶校验"]
##parity_combobox = ttk.Combobox(config_frame, values=parity_options, width=15)
##parity_combobox.grid(row=4, column=1, padx=5, pady=5)
##parity_combobox.current(0)  # 默认选择无校验

# 串口开关
serial_button = ttk.Button(config_frame, text="打开串口", command=toggle_serial)
serial_button.grid(row=0, column=0, padx=165, pady=0, sticky="nw")

# 清空接收
clear_button = ttk.Button(config_frame, text="清空接收", command=clear_log)
clear_button.grid(row=0, column=0, padx=165, pady=30, sticky="nw")

# 设备信息框（第一行第二列）
info_frame = ttk.LabelFrame(root, text="设备信息", padding=2, width=265, height=168)
info_frame.grid(row=0, column=1, padx=(0,10), pady=(100,10), sticky="nw")
info_frame.grid_propagate(False)  # 禁止自动调整大小

# 程序版本
ttk.Label(info_frame, text="程序版本:").grid(row=0, column=0, padx=5, pady=0, sticky="nw")
software_version = ttk.Label(info_frame, text="未获取")
software_version.grid(row=0, column=0, padx=64, pady=0, sticky="nw")

# 序列号输入框
ttk.Label(info_frame, text="序列号:").grid(row=1, column=0, padx=5, pady=3, sticky="nw")
serial_number_entry = ttk.Entry(info_frame, width=20, justify="right")
serial_number_entry.grid(row=1, column=0, padx=60, pady=3, sticky="nw")

# 发送序列号按钮
send_sn_button = ttk.Button(info_frame, text="→", width=3, command=send_serial_number)
send_sn_button.grid(row=1, column=0, padx=215, pady=1, sticky="nw")

# 灵敏度输入框
ttk.Label(info_frame, text="灵敏度:").grid(row=2, column=0, padx=5, pady=3, sticky="nw")
sensitivity_entry = ttk.Entry(info_frame, width=20, justify="right")
sensitivity_entry.grid(row=2, column=0, padx=60, pady=3, sticky="nw")

# 发送灵敏度按钮
send_sen_button = ttk.Button(info_frame, text="→", width=3, command=send_sensitivity)
send_sen_button.grid(row=2, column=0, padx=215, pady=1, sticky="nw")

# 校正值输入框
ttk.Label(info_frame, text="校正值:").grid(row=3, column=0, padx=5, pady=3, sticky="nw")
correct_entry = ttk.Entry(info_frame, width=20, justify="right")
correct_entry.grid(row=3, column=0, padx=60, pady=3, sticky="nw")
correct_entry.insert(0, "暂不支持")
correct_entry.config(state=tk.DISABLED)

# 发送校正值按钮
send_correct_button = ttk.Button(info_frame, text="→", width=3, command=send_sensitivity)
send_correct_button.grid(row=3, column=0, padx=215, pady=1, sticky="nw")
send_correct_button.config(state=tk.DISABLED)

# 获取设备信息
dev_info_button = ttk.Button(info_frame, text="获取设备信息", command=send_device_info, width=12)
dev_info_button.grid(row=4, column=0, padx=80, pady=3, sticky="w")

# 参数配置框（第一行第二列）
param_frame = ttk.LabelFrame(root, text="参数配置", padding=1, width=265, height=125)
param_frame.grid(row=0, column=1, padx=(0,10), pady=(273,0), sticky="nw")
param_frame.grid_propagate(False)  # 禁止自动调整大小

# 设备信息
req_history_button = ttk.Button(param_frame, text="历史记录", command=send_history, width=12)
req_history_button.grid(row=0, column=0, padx=22, pady=3, sticky="nw")

# 恢复出厂设置
status_reset_button = ttk.Button(param_frame, text="恢复出厂设置", command=send_status_reset, width=12)
status_reset_button.grid(row=0, column=0, padx=142, pady=3, sticky="nw")

# 时间同步
datetime_sync_button = ttk.Button(param_frame, text="时间同步", command=send_datetime, width=12)
datetime_sync_button.grid(row=1, column=0, padx=22, pady=3, sticky="nw")

# 清除功耗时间
clr_pt_button = ttk.Button(param_frame, text="清除功耗时间", command=send_clr_pt, width=12)
clr_pt_button.grid(row=1, column=0, padx=142, pady=3, sticky="nw")

# 报警测试
alarm_test_button = ttk.Button(param_frame, text="报警测试", command=send_alarm_test, width=12)
alarm_test_button.grid(row=2, column=0, padx=22, pady=3, sticky="nw")

# 待机测试
aging_test_button = ttk.Button(param_frame, text="待机测试", command=send_aging_test, width=12)
aging_test_button.grid(row=2, column=0, padx=142, pady=3, sticky="nw")



# 软件信息框
programer_frame = ttk.LabelFrame(root, text="软件信息", padding=1, width=265, height=77)
programer_frame.grid(row=0, column=1, padx=(0,10), pady=(403,0), sticky="nw")
programer_frame.grid_propagate(False)  # 禁止自动调整大小

# 软件版本
ttk.Label(programer_frame, text="软件版本:").grid(row=0, column=0, padx=5, pady=3, sticky="nw")
programer_version = ttk.Label(programer_frame, text=software_ver)
programer_version.grid(row=0, column=0, padx=64, pady=3, sticky="nw")


# 开发商
ttk.Label(programer_frame, text="开发商:  广州瑞多思医疗科技有限公司").grid(row=1, column=0, padx=5, pady=0, sticky="nw")

# 文件操作（第二行）
file_frame = ttk.LabelFrame(root, text="文件操作", padding=5)
file_frame.grid(row=1, column=0, columnspan=2, padx=10, pady=(5,5), sticky="nsew")

# 配置列的权重，使它们可以扩展
file_frame.columnconfigure(0, weight=1)  # file_label所在的列
file_frame.columnconfigure(1, weight=0)  # 打开文件按钮所在的列
file_frame.columnconfigure(2, weight=0)  # 发送文件按钮所在的列
file_frame.columnconfigure(3, weight=0)  # 停止发送按钮所在的列

# 文件选择
file_label = ttk.Entry(file_frame, state="readonly")
file_label.grid(row=0, column=0, padx=5, pady=5, sticky="ew")  # 添加 sticky="ew"

file_button = ttk.Button(file_frame, text="打开文件", command=open_file)
file_button.grid(row=0, column=1, padx=5, pady=5)

# 发送控制
bootload_button = ttk.Button(file_frame, text="IAP更新", command=start_bootloader_update)
bootload_button.grid(row=0, column=2, padx=5, pady=5)

send_button = ttk.Button(file_frame, text="APP更新", command=start_send)
send_button.grid(row=0, column=3, padx=5, pady=5)

stop_button = ttk.Button(file_frame, text="停止更新", command=stop_send)
stop_button.grid(row=0, column=4, padx=5, pady=5)

# 进度条（与文件操作界面等长）
progress = Progressbar(file_frame, orient="horizontal", mode="determinate")
progress.grid(row=1, column=0, columnspan=4, padx=5, pady=5, sticky="ew")  # 添加 sticky="ew"

# 进度条百分比显示
progress_percentage_label = ttk.Label(file_frame, text="0.00%", font=("Arial", 10))
progress_percentage_label.grid(row=1, column=4, padx=(5, 5), pady=5, sticky="nw")

# 绑定端口号选择事件
port_combobox.bind("<<ComboboxSelected>>", on_port_select)

# 初始化串口刷新
refresh_ports()

# 运行 GUI
root.mainloop()
