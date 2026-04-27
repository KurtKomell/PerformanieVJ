@echo off
REM Double-click or run from cmd.exe — loads MSVC environment then CMake.
cd /d "%~dp0.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0cmake-with-msvc.ps1" %*
exit /b %ERRORLEVEL%
