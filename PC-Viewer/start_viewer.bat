@echo off
setlocal
cd /d "%~dp0"
where py >nul 2>nul
if errorlevel 1 (set "FRESLEST_VIEWER_PY=python") else (set "FRESLEST_VIEWER_PY=py -3")
%FRESLEST_VIEWER_PY% -c "import sys,tkinter; assert sys.version_info >= (3,9)" >nul 2>&1
if errorlevel 1 (
 echo Install Python 3.9 or later for Windows with Tcl/Tk support, then run this file again.
 pause
 exit /b 1
)
%FRESLEST_VIEWER_PY% -c "import serial" >nul 2>&1
if errorlevel 1 (
 echo Installing the USB serial dependency...
 %FRESLEST_VIEWER_PY% -m pip install "pyserial==3.5"
 if errorlevel 1 (
  pause
  exit /b 1
 )
)
%FRESLEST_VIEWER_PY% freslest_viewer.py
if errorlevel 1 pause
