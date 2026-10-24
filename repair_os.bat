@echo off
setlocal
cd /d "%~dp0"
if "%~1"=="" (
  echo Usage: repair_os.bat COM5
  exit /b 1
)
where py >nul 2>nul
if errorlevel 1 (set "FRESLEST_PY=python") else (set "FRESLEST_PY=py -3")
%FRESLEST_PY% tools\flash_freslest.py %1 --os-only
exit /b %errorlevel%
