@echo off
chcp 65001 >nul
title NeiJi 生产测试上位机
cd /d "%~dp0"

python --version >nul 2>&1
if errorlevel 1 (
    echo [错误] 未找到 Python，请先安装 Python 3
    pause
    exit /b 1
)

pip install -q -r requirements.txt
python fsy_serial_gui.py
if errorlevel 1 pause
