@echo off
REM ASCII-only: avoids GBK/UTF-8 misparsing on Chinese Windows CMD.
setlocal EnableExtensions
cd /d "%~dp0"
set "GUI=%~dp0fsy_tcp_slave_gui.py"

where py >nul 2>&1
if errorlevel 1 goto use_python

echo [run] py -3
py -3 -c "import sys; print(sys.version); print(sys.executable)"
if errorlevel 1 (
  echo [err] py -3 failed. Install Python 3 and add to PATH.
  goto fail
)
py -3 -c "import tkinter"
if errorlevel 1 (
  echo [err] tkinter missing. Use full python.org installer ^(with Tcl/Tk^) or apt install python3-tk.
  goto fail
)
py -3 -u "%GUI%"
set "_RC=%errorlevel%"
goto after_run

:use_python
echo [info] No "py" in PATH - using "python" instead ^(OK; Launcher is optional^)
where python >nul 2>&1
if errorlevel 1 (
  echo [err] python not found. Install Python 3 and add to PATH.
  goto fail
)
python -c "import sys; print(sys.version); print(sys.executable)"
if errorlevel 1 goto fail
python -c "import tkinter"
if errorlevel 1 (
  echo [err] tkinter missing. Use full python.org installer.
  goto fail
)
python -u "%GUI%"
set "_RC=%errorlevel%"

:after_run
if "%_RC%"=="0" exit /b 0
echo.
echo [err] exit code: %_RC%
echo Copy all text above for troubleshooting.
goto fail

:fail
pause
exit /b 1
