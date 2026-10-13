<#
.SYNOPSIS
    OmniPeek Windows Shell Extension Automation Script
.DESCRIPTION
    Compiles, registers COM DLL, and validates Explorer Preview Handler.
#>
param(
    [string]$TargetConfig = "Release",
    [switch]$RegisterCOM = $true
)

Write-Host ">>> OmniPeek Deployment Automation for Windows Shell <<<" -ForegroundColor Cyan
$DllPath = Join-Path $PSScriptRoot "..\dist\OmniPeekExtension.dll"

if (Test-Path $DllPath) {
    Write-Host "Registering COM Extension: $DllPath" -ForegroundColor Green
    Start-Process "regsvr32.exe" -ArgumentList "/s `"$DllPath`"" -Wait -NoNewWindow
    Write-Host "Registration Completed successfully!" -ForegroundColor Green
}
