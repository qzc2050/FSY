@echo off
chcp 65001 >nul
cd /d "%~dp0"

REM 辐射报警仪上位机打包脚本
REM 调用 Python 脚本执行实际打包工作

python build_installer.py
