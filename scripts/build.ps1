# OmniPeek Universal Previewer One-Click Build Script (scripts/build.ps1)
$ErrorActionPreference = "Stop"
$RootDir = Split-Path -Parent $PSScriptRoot
Set-Location $RootDir

$env:PATH = "$RootDir\tools\w64devkit\bin;" + $env:PATH

Write-Host "=====================================================" -ForegroundColor Cyan
Write-Host "  Building OmniPeek (万能预览器) v1.0.0" -ForegroundColor Cyan
Write-Host "=====================================================" -ForegroundColor Cyan

# Stop running processes to prevent file lock during compilation
Stop-Process -Name prevhost -Force -ErrorAction SilentlyContinue
Stop-Process -Name OmniPeekWorker -Force -ErrorAction SilentlyContinue
Stop-Process -Name OmniPeekSettings -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 250

if (Test-Path "dist\OmniPeekExtension.dll") {
    Remove-Item -Force "dist\OmniPeekExtension.dll.old" -ErrorAction SilentlyContinue
    Move-Item -Force "dist\OmniPeekExtension.dll" "dist\OmniPeekExtension.dll.old" -ErrorAction SilentlyContinue
}

# Ensure dist directories exist
New-Item -ItemType Directory -Force -Path "dist\viewer\libs" | Out-Null
New-Item -ItemType Directory -Force -Path "dist\plugins" | Out-Null

# 1. Build OmniPeekWorker.exe
Write-Host "[1/4] Compiling OmniPeekWorker.exe (Multi-Modal Backend & Thumbnail Engine)..." -ForegroundColor Yellow
g++.exe -O2 -march=x86-64 -std=c++20 -DUNICODE -D_UNICODE -static -static-libgcc -static-libstdc++ `
    "src/worker/main.cpp" `
    -o "dist/OmniPeekWorker.exe" `
    -lgdi32 -luser32 -lshell32 -lshlwapi -lole32 -luuid
if ($LASTEXITCODE -ne 0) { throw "Failed to compile OmniPeekWorker.exe" }

# 2. Build OmniPeekExtension.dll
Write-Host "[2/4] Compiling OmniPeekExtension.dll (64-bit Shell COM Microkernel)..." -ForegroundColor Yellow
g++.exe -shared -O2 -march=x86-64 -std=c++20 -DUNICODE -D_UNICODE `
    -I"src/shell_ext" -I"src/shell_ext/webview2" `
    "src/shell_ext/DllMain.cpp" `
    "src/shell_ext/OmniPeekThumbnailProvider.cpp" `
    "src/shell_ext/OmniPeekPreviewHandler.cpp" `
    "src/shell_ext/OmniPeekExtension.def" `
    -o "dist/OmniPeekExtension.dll" `
    "tools/WebView2Loader.dll.lib" `
    -lshlwapi -lole32 -loleaut32 -luuid -lshell32 -luser32 -lgdi32 -ladvapi32 -static-libgcc -static-libstdc++
if ($LASTEXITCODE -ne 0) { throw "Failed to compile OmniPeekExtension.dll" }

# 3. Build OmniPeekSettings.exe
Write-Host "[3/4] Compiling OmniPeekSettings.exe (Plugin Control Center GUI)..." -ForegroundColor Yellow
g++.exe -O2 -march=x86-64 -std=c++20 -DUNICODE -D_UNICODE -mwindows -municode -static -static-libgcc -static-libstdc++ `
    "src/settings/main.cpp" `
    -o "dist/OmniPeekSettings.exe" `
    -lcomctl32 -lshlwapi -lshell32 -ladvapi32 -luser32 -lgdi32 -lole32 -luuid
if ($LASTEXITCODE -ne 0) { throw "Failed to compile OmniPeekSettings.exe" }

# 4. Sync Frontend Viewer & Plugin Assets into dist/
Write-Host "[4/4] Syncing viewer host & plugin packs to dist/..." -ForegroundColor Yellow
Copy-Item -Path "src\viewer\index.html" -Destination "dist\viewer\index.html" -Force
Copy-Item -Path "src\viewer\viewer.css" -Destination "dist\viewer\viewer.css" -Force
Copy-Item -Path "src\viewer\viewer.js"  -Destination "dist\viewer\viewer.js"  -Force
Copy-Item -Path "src\viewer\libs\*"     -Destination "dist\viewer\libs\"     -Recurse -Force
Copy-Item -Path "plugins\*"             -Destination "dist\plugins\"         -Recurse -Force
if (Test-Path "src\worker\cad_processor.py") {
    Copy-Item -Path "src\worker\cad_processor.py" -Destination "dist\cad_processor.py" -Force
}

# Re-register COM extension
regsvr32.exe /s "$RootDir\dist\OmniPeekExtension.dll"

Write-Host "Build & Registration completed successfully! Output is in dist/" -ForegroundColor Green
