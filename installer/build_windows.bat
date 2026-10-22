@echo off
setlocal
cd /d "%~dp0\.."
py -3.11 -m pip install -r requirements-build.txt
if errorlevel 1 exit /b 1
py -3.11 prepare_firmware.py
if errorlevel 1 exit /b 1
py -3.11 -m PyInstaller --noconfirm --clean --onefile --windowed --noupx --name "Freslest Installer" --collect-all esptool --collect-all serial --add-data "firmware;firmware" installer\installer.py
if errorlevel 1 exit /b 1
echo READY: dist\Freslest Installer.exe
