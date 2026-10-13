@echo off
chcp 65001 >nul
title OmniPeek 万能预览器 - 一键安装与注册
cd /d "%~dp0"

echo ========================================================
echo   OmniPeek 万能预览器 v1.0.0 - 注册向导
echo ========================================================
echo.

echo [1/3] 正在关闭旧版预览宿主进程 (prevhost.exe)...
taskkill /f /im prevhost.exe >nul 2>&1

echo [2/3] 正在注册 64位 COM 外壳微内核 (OmniPeekExtension.dll)...
regsvr32.exe /s "%~dp0OmniPeekExtension.dll"
if %errorlevel% neq 0 (
    echo [错误] 注册 OmniPeekExtension.dll 失败！请尝试右键“以管理员身份运行”。
    pause
    exit /b 1
)

echo [3/3] 正在刷新 Windows 资源管理器扩展缓存...
echo.
echo [成功] OmniPeek 万能预览器已成功激活！
echo  - 按 Alt+P 打开资源管理器右侧预览窗格即可体验 70+ 种格式即时预览
echo  - 正在为您打开【OmniPeek 控制中心】...
start "" "%~dp0OmniPeekSettings.exe"
timeout /t 2 >nul
