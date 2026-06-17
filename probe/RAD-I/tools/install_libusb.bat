@echo off
echo ==================== 安装 STM32 DFU 驱动 ====================
echo.
echo 正在下载并安装 libusb 驱动...
echo.

:: 创建临时目录
mkdir %TEMP%\libusb-win32
cd %TEMP%\libusb-win32

:: 下载 libusb-win32
echo 正在下载 libusb-win32...
powershell -Command "& {Invoke-WebRequest -Uri 'https://sourceforge.net/projects/libusb-win32/files/libusb-win32-releases/1.2.6.0/libusb-win32-bin-1.2.6.0.zip/download' -OutFile 'libusb-win32.zip'}"

:: 解压
echo 正在解压...
powershell -Command "& {Expand-Archive -Path 'libusb-win32.zip' -DestinationPath '.' -Force}"

:: 安装驱动（需要设备连接到电脑）
echo.
echo ==================== 重要提示 ====================
echo 1. 请先将单片机进入 BootLoader 模式（连接 USB）
echo 2. 然后运行以下命令安装驱动：
echo.
echo    %TEMP%\libusb-win32\libusb-win32-bin-1.2.6.0\bin\inf-wizard.exe
echo.
echo 3. 在 inf-wizard 中选择你的 STM32 DFU 设备
echo 4. 安装 WinUSB 驱动
echo.
echo 或者使用 Zadig 工具（推荐）：
echo 1. 下载 Zadig: https://zadig.akeo.ie/
echo 2. 运行 Zadig，选择 Options -> List All Devices
echo 3. 选择 "STM32 BOOTLOADER" 或类似设备
echo 4. 选择 WinUSB 驱动
echo 5. 点击 "Replace Driver" 或 "Install Driver"
echo.
pause
