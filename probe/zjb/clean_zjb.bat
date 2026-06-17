@echo off
setlocal

cd /d "%~dp0"

echo ========================================
echo Cleaning zjb build outputs...
echo Project root: %CD%
echo ========================================

call :clean_dir "%CD%\zjb\MDK-ARM\zjb" "zjb app output"
call :clean_dir "%CD%\Bootloader\MDK-ARM\bootloader" "bootloader output"

echo.
echo Clean finished.
pause
exit /b 0

:clean_dir
set "TARGET_DIR=%~1"
set "TARGET_NAME=%~2"

if not exist "%TARGET_DIR%" (
    echo [SKIP] %TARGET_NAME%: %TARGET_DIR%
    exit /b 0
)

echo.
echo [CLEAN] %TARGET_NAME%
echo         %TARGET_DIR%

del /q "%TARGET_DIR%\*.o" 2>nul
del /q "%TARGET_DIR%\*.d" 2>nul
del /q "%TARGET_DIR%\*.crf" 2>nul
del /q "%TARGET_DIR%\*.dep" 2>nul
del /q "%TARGET_DIR%\*.axf" 2>nul
del /q "%TARGET_DIR%\*.bin" 2>nul
del /q "%TARGET_DIR%\*.hex" 2>nul
del /q "%TARGET_DIR%\*.map" 2>nul
del /q "%TARGET_DIR%\*.htm" 2>nul
del /q "%TARGET_DIR%\*.lnp" 2>nul

exit /b 0
