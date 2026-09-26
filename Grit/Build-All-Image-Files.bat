@echo off
setlocal
cd /d "%~dp0"
powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Build-All-Image-Files.ps1"
if errorlevel 1 (
    echo.
    echo IMAGE BUILD FAILED.
    pause
    exit /b 1
)
echo.
echo IMAGE BUILD COMPLETE.
pause
