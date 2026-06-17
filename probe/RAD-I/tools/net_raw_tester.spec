# -*- mode: python ; coding: utf-8 -*-

block_cipher = None

a = Analysis(
    ['run_net_raw_tester.py'],
    pathex=[],
    binaries=[],
    datas=[('run_net_raw_tester.py', '.'), ('rw.ico', '.'), ('libusb-1.0.dll', '.'), ('crc16_dev.py', '.'), ('usb_dfu_lib.py', '.'), ('net_raw_pack_support.py', '.'), ('pyi_rth_net_raw_resources.py', '.'), ('net_raw_tester.py', '.')],
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
    ],
    hookspath=[],
    hooksconfig={},
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
