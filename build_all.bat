@echo off
setlocal
cd /d "%~dp0"
python tools\build_all.py
exit /b %errorlevel%
