#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
辐射报警仪上位机打包脚本
Radiation Warning Instrument Host Tool Build Script
"""

import os
import sys
import subprocess
from datetime import datetime
from pathlib import Path

def main():
    script_dir = Path(__file__).parent
    os.chdir(script_dir)
    
    print("=" * 40)
    print("  辐射报警仪上位机 - 本地打包")
    print("=" * 40)
    print()
    
    # Check UPX
    upx_dir = Path(r"d:\software\upx-5.0.0-win64")
    upx_path = upx_dir / "upx.exe"
    if upx_path.exists():
        os.environ["PATH"] = str(upx_dir) + os.pathsep + os.environ.get("PATH", "")
        print(f"[提示] UPX 已找到：{upx_dir}")
    else:
        print(f"[提示] 未找到 UPX：{upx_dir}，将跳过 UPX 压缩")
    
    print()
    
    # Check required files
    required_files = [
        "net_raw_tester.py", "crc16_dev.py", "usb_dfu_lib.py",
        "net_raw_pack_support.py", "pyi_rth_net_raw_resources.py",
        "run_net_raw_tester.py", "serial_link.py", "rw.ico",
        "libusb-1.0.dll"
    ]
    
    for file in required_files:
        if not (script_dir / file).exists():
            print(f"[错误] 缺少 {file}")
            input("按任意键继续...")
            return 1
    
    # Generate .spec file automatically with all required files
    spec_file = script_dir / "net_raw_tester.spec"
    print("[提示] 正在生成 net_raw_tester.spec（包含所有必要文件）...")
    
    # Collect all data files
    datas = [
        ('run_net_raw_tester.py', '.'),
        ('rw.ico', '.'),
        ('libusb-1.0.dll', '.'),
    ]
    
    # Add other Python modules as data files (they will be imported by net_raw_tester.py)
    for py_file in ["crc16_dev.py", "usb_dfu_lib.py", "net_raw_pack_support.py", 
                    "pyi_rth_net_raw_resources.py", "net_raw_tester.py", "serial_link.py"]:
        if (script_dir / py_file).exists():
            datas.append((py_file, '.'))
    
    # Create .spec file content
    spec_content = f'''# -*- mode: python ; coding: utf-8 -*-

block_cipher = None

a = Analysis(
    ['run_net_raw_tester.py'],
    pathex=[],
    binaries=[],
    datas={repr(datas)},
    hiddenimports=[
        'serial',
        'serial.tools',
        'serial.tools.list_ports',
        'PIL',
        'PIL.Image',
        'tkinter',
        'crc16_dev',
        'usb_dfu_lib',
        'net_raw_pack_support',
        'serial_link',
    ],
    hookspath=[],
    hooksconfig={{}},
    runtime_hooks=['pyi_rth_net_raw_resources.py'],
    excludes=[],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name='net_raw_tester',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon='rw.ico',
)
'''
    
    # Write .spec file
    with open(spec_file, 'w', encoding='utf-8') as f:
        f.write(spec_content)
    
    print("[成功] 已生成 net_raw_tester.spec")
    print()
    
    # Check pyinstaller
    try:
        import PyInstaller
        print(f"[提示] pyinstaller 已找到：PyInstaller {PyInstaller.__version__}")
    except ImportError:
        print("[错误] 未找到 pyinstaller，请先执行：pip install pyinstaller")
        input("按任意键继续...")
        return 1
    
    # Generate timestamp
    timestamp = datetime.now().strftime("%y%m%d_%H%M")
    output_name = f"RWD-I-V1.0-{timestamp}.exe"
    
    print()
    print("开始打包...")
    print(f"输出文件名：{output_name}")
    print("打包内容：")
    print("  - run_net_raw_tester.py（打包入口：图标 + 防闪屏）")
    print("  - net_raw_tester.py（主程序）")
    print("  - serial_link.py（串口连接服务）")
    print("  - crc16_dev.py / usb_dfu_lib.py / net_raw_pack_support.py")
    print("  - rw.ico（窗口图标 + EXE 图标）")
    print("  - libusb-1.0.dll（USB DFU 必需）")
    print("  - pyi_rth_net_raw_resources.py（运行时：图标 + DLL 路径）")
    print()
    
    # Build with pyinstaller
    result = subprocess.run([
        sys.executable.replace("python.exe", "Scripts\\pyinstaller.exe"),
        "--noconfirm", "--clean", "net_raw_tester.spec"
    ], cwd=script_dir)
    
    if result.returncode != 0:
        print()
        print("[错误] 打包失败")
        input("按任意键继续...")
        return 1
    
    # Rename output file
    dist_dir = script_dir / "dist"
    source_exe = dist_dir / "net_raw_tester.exe"
    dest_exe = dist_dir / output_name
    
    if source_exe.exists():
        print()
        print("正在重命名输出文件...")
        try:
            source_exe.rename(dest_exe)
            print(f"[成功] 已重命名为：{output_name}")
        except Exception as e:
            print(f"[警告] 重命名失败：{e}，保留原名 net_raw_tester.exe")
    
    print()
    print("=" * 40)
    print(f"打包完成，输出目录：{dist_dir}")
    print(f"输出文件：{output_name}")
    print("=" * 40)
    
    input("按任意键继续...")
    return 0

if __name__ == "__main__":
    sys.exit(main())
