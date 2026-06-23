@echo off
chcp 65001 >nul
title NeiJi 串口协议工具
cd /d "%~dp0"

python --version >nul 2>&1
if errorlevel 1 (
    echo [错误] 未找到 Python，请先安装 Python 3 并加入 PATH
    goto :end
)

pip install -q -r requirements.txt

echo ========================================
echo   NeiJi 串口协议工具
echo ========================================
echo.
echo  1 - 打开图形界面（工厂生产，推荐）
echo  2 - 命令行监听 0x23
echo  3 - 列出串口
echo  0 - 退出
echo.
set /p CHOICE=请选择 [0-3]: 

if "%CHOICE%"=="0" goto :eof
if "%CHOICE%"=="1" (
    start "" python fsy_serial_gui.py
    goto :eof
)
if "%CHOICE%"=="2" goto :cli_listen
if "%CHOICE%"=="3" (
    python fsy_serial_tool.py --list
    goto :end
)

echo [错误] 无效选项
goto :end

:cli_listen
set /p PORT=请输入串口名（如 COM5）: 
if "%PORT%"=="" goto :end
python fsy_serial_tool.py listen -p %PORT%
goto :end

:end
pause
