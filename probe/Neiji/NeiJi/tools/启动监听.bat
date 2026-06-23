@echo off
chcp 65001 >nul
title NeiJi 串口监听 0x23
cd /d "%~dp0"

python --version >nul 2>&1
if errorlevel 1 (
    echo [错误] 未找到 Python，请先安装 Python 3
    pause
    exit /b 1
)

pip install -q -r requirements.txt

if not "%~1"=="" (
    set "PORT=%~1"
) else (
    set /p PORT=请输入串口名（如 COM5）: 
)

if "%PORT%"=="" (
    echo [错误] 串口不能为空
    pause
    exit /b 1
)

echo.
echo 监听 %PORT% @ 115200 — 解析 0x23 主动上传，Ctrl+C 停止
echo.

python fsy_serial_tool.py listen -p %PORT%
echo.
pause
