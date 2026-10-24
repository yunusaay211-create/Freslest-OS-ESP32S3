@echo off
setlocal
cd /d "%~dp0"
if "%~1"=="" (
  echo Usage: repair_doom.bat COM5
  echo Repairs Doom firmware only. Existing Freslest A/B installation required.
  exit /b 1
)
where py >nul 2>nul
if errorlevel 1 (set "FRESLEST_PY=python") else (set "FRESLEST_PY=py -3")
%FRESLEST_PY% tools\flash_freslest.py %1 --doom-only
exit /b %errorlevel%
