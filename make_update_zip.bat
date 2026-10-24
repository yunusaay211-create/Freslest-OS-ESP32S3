@echo off
setlocal
cd /d "%~dp0"
where py >nul 2>nul
if errorlevel 1 (set "FRESLEST_PY=python") else (set "FRESLEST_PY=py -3")
%FRESLEST_PY% tools\make_update_zip.py %*
exit /b %errorlevel%
