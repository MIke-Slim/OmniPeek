@echo off
chcp 65001 >nul
title OmniPeek 万能预览器 - 一键注销与卸载还原
cd /d "%~dp0"

echo ========================================================
echo   OmniPeek 万能预览器 v1.0.0 - 卸载与无损还原向导
echo ========================================================
echo.

echo [1/2] 正在释放预览宿主进程 (prevhost.exe) 与后台工作进程...
taskkill /f /im prevhost.exe >nul 2>&1
taskkill /f /im OmniPeekWorker.exe >nul 2>&1

echo [2/2] 正在注销 OmniPeekExtension.dll 并还原原有系统注册表项...
regsvr32.exe /u /s "%~dp0OmniPeekExtension.dll"

echo.
echo [完成] 已成功注销 OmniPeek 并恢复系统默认预览设置！
pause
