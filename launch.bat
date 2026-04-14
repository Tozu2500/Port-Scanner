@echo off

set "EXE=%~dp0port_scanner\port_scanner.exe"

if not exist "%EXE%" (
    echo Error: Could not find port_scanner.exe at:
    echo   %EXE%
    echo Please build the project first.
    pause
    exit /b 1
)

start "" "%EXE%"
