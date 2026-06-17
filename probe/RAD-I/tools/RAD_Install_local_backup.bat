@echo off
setlocal enabledelayedexpansion

chcp 65001 >nul

cd /d "%~dp0"



echo ========================================

echo   辐射报警仪上位机 - 本地打包

echo ========================================

echo.



set "UPX_DIR=d:\software\upx-5.0.0-win64"

if exist "%UPX_DIR%\upx.exe" (

    set "PATH=%UPX_DIR%;%PATH%"

) else (

    echo [提示] 未找到 UPX：%UPX_DIR%，将跳过 UPX 压缩

)



if not exist "net_raw_tester.py" (

    echo [错误] 缺少 net_raw_tester.py

    pause

    exit /b 1

)

if not exist "crc16_dev.py" (

    echo [错误] 缺少 crc16_dev.py

    pause

    exit /b 1

)

if not exist "usb_dfu_lib.py" (

    echo [错误] 缺少 usb_dfu_lib.py

    pause

    exit /b 1

)

if not exist "net_raw_pack_support.py" (

    echo [错误] 缺少 net_raw_pack_support.py

    pause

    exit /b 1

)

if not exist "pyi_rth_net_raw_resources.py" (

    echo [错误] 缺少 pyi_rth_net_raw_resources.py

    pause

    exit /b 1

)

if not exist "run_net_raw_tester.py" (

    echo [错误] 缺少 run_net_raw_tester.py

    pause

    exit /b 1

)

if not exist "rw.ico" (

    echo [错误] 缺少 rw.ico（窗口/EXE 图标）

    pause

    exit /b 1

)

if not exist "libusb-1.0.dll" (

    echo [警告] 缺少 libusb-1.0.dll，打包后 USB DFU 功能不可用

)


REM 自动检测并生成 .spec 文件（使用动态时间戳命名）

if not exist "net_raw_tester.spec" (

    echo [提示] 未找到 net_raw_tester.spec，正在自动生成...

    pyi-makespec --windowed --icon=rw.ico --name=RWD-I-V1.0 --add-binary="libusb-1.0.dll;." --add-data="rw.ico;." net_raw_tester.py

    if errorlevel 1 (

        echo [错误] 生成 .spec 文件失败

        pause

        exit /b 1

    )

    echo [成功] 已生成 net_raw_tester.spec

)



where pyinstaller >nul 2>&1

if errorlevel 1 (

    echo [错误] 未找到 pyinstaller，请先执行：pip install pyinstaller

    pause

    exit /b 1

)



REM 生成时间戳：YYMMDD_HHMM

for /f "tokens=2 delims==" %%a in ('wmic OS Get localdatetime ^| find "."') do set "dt=%%a"

set "YY=!dt:~2,2!"

set "MM=!dt:~4,2!"

set "DD=!dt:~6,2!"

set "HH=!dt:~8,2!"

set "Mi=!dt:~10,2!"

set "timestamp=!YY!!MM!!DD!_!HH!!Mi!"

set "output_name=RWD-I-V1.0-!timestamp!.exe"



echo 开始打包...

echo 输出文件名：%output_name%

echo 打包内容：

echo   - run_net_raw_tester.py（打包入口：图标 + 防闪屏）

echo   - net_raw_tester.py（主程序）

echo   - crc16_dev.py / usb_dfu_lib.py / net_raw_pack_support.py

echo   - rw.ico（窗口图标 + EXE 图标）

echo   - libusb-1.0.dll（USB DFU，若存在）

echo   - pyi_rth_net_raw_resources.py（运行时：图标 + DLL 路径）

echo.



pyinstaller --noconfirm --clean net_raw_tester.spec

if errorlevel 1 (

    echo.

    echo [错误] 打包失败

    pause

    exit /b 1

)



REM 重命名生成的文件（添加时间戳）

if exist "%~dp0dist\RWD-I-V1.0.exe" (

    echo.

    echo 正在重命名输出文件...

    move /y "%~dp0dist\RWD-I-V1.0.exe" "%~dp0dist\%output_name%"

    if errorlevel 1 (

        echo [警告] 重命名失败，保留原名 RWD-I-V1.0.exe

    ) else (

        echo [成功] 已重命名为：%output_name%

    )

)



echo.

echo ========================================

echo 打包完成，输出目录：%~dp0dist

echo 输出文件：%output_name%

echo ========================================

pause

