@echo off
cd /d "%~dp0"
powershell -ExecutionPolicy Bypass -File "启动桥接服务.ps1"
