@echo off
setlocal
cd /d "%~dp0\.."
py -3.11 -m pip install -r requirements-build.txt
if errorlevel 1 exit /b 1
py -3.11 -m PyInstaller --noconfirm --clean --onefile --windowed --noupx --paths viewer --name "Freslest USB Viewer" --collect-all serial viewer\freslest_viewer.py
if errorlevel 1 exit /b 1
echo READY: dist\Freslest USB Viewer.exe
