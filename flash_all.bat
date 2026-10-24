@echo off
setlocal
cd /d "%~dp0"
if "%~1"=="" (
  echo Usage: flash_all.bat COM5 [--with-files]
  exit /b 1
)
where py >nul 2>nul
if errorlevel 1 (set "FRESLEST_PY=python") else (set "FRESLEST_PY=py -3")
%FRESLEST_PY% tools\flash_freslest.py %* 
exit /b %errorlevel%
