@echo off
setlocal
cd /d "%~dp0"
py -3.11 -m pip install -r requirements-build.txt
if errorlevel 1 exit /b 1
py -3.11 prepare_firmware.py
if errorlevel 1 exit /b 1
py -3.11 tests\run_all.py
if errorlevel 1 exit /b 1
py -3.11 build_windows.py
if errorlevel 1 exit /b 1
py -3.11 tests\smoke_exes_windows.py
if errorlevel 1 exit /b 1
echo READY: dist\Freslest Installer.exe
echo READY: dist\Freslest USB Viewer.exe
