@echo off
chcp 65001 >nul
title Pixel Claude Pet - Service Manager

echo ========================================
echo   Pixel Claude Pet - Service Manager
echo ========================================
echo.
echo Choose an option:
echo   1. Install service
echo   2. Start service
echo   3. Stop service
echo   4. Remove service
echo   5. Debug run (foreground)
echo.

set /p choice="Enter choice (1-5): "

cd /d "%~dp0"

if "%choice%"=="1" (
    echo Installing service...
    python pc_monitor_service.py install
    python pc_monitor_service.py start
    echo Done! Service installed and started.
)
if "%choice%"=="2" (
    echo Starting service...
    python pc_monitor_service.py start
)
if "%choice%"=="3" (
    echo Stopping service...
    python pc_monitor_service.py stop
)
if "%choice%"=="4" (
    echo Removing service...
    python pc_monitor_service.py stop
    python pc_monitor_service.py remove
    echo Done! Service removed.
)
if "%choice%"=="5" (
    echo Running in debug mode...
    python pc_monitor_service.py
)
pause
