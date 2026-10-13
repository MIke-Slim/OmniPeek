#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

#include "../common/Guids.h"
#include "../common/PluginRegistry.h"

// Theme Colors
static const COLORREF CLR_BG          = RGB(248, 250, 252);
static const COLORREF CLR_CARD_BG     = RGB(255, 255, 255);
static const COLORREF CLR_BORDER      = RGB(226, 232, 240);
static const COLORREF CLR_TEXT_DARK   = RGB(15, 23, 42);
static const COLORREF CLR_TEXT_MUTED  = RGB(100, 116, 139);
static const COLORREF CLR_PRIMARY     = RGB(2, 132, 199);

static HBRUSH g_hBrushBg = NULL;
static HBRUSH g_hBrushCard = NULL;
static HPEN g_hPenBorder = NULL;

static HFONT g_hFontHeader = NULL;
static HFONT g_hFontTitle = NULL;
static HFONT g_hFontBold = NULL;
static HFONT g_hFontNormal = NULL;
static HFONT g_hFontSmall = NULL;

struct PluginUIItem {
    OmniPeek::PluginManifest manifest;
    HWND hCheckbox = NULL;
    HWND hExtDesc = NULL;
};

static std::vector<PluginUIItem> g_pluginItems;
static HWND g_hChkAutoplay = NULL;
static HWND g_hComboVolume = NULL;

static HWND g_hTab = NULL;
static HWND g_hPanelPlugins = NULL;
static HWND g_hPanelStatus = NULL;
static HWND g_hPanelCache = NULL;
static HWND g_hCacheInfoText = NULL;
static HWND g_hStatusInfoText = NULL;

BOOL IsRunAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin;
}

std::wstring GetAppDir() {
    return OmniPeek::PluginRegistry::GetModuleDir();
}

bool IsPluginEnabledInIni(const std::wstring& pluginId) {
    std::wstring iniPath = GetAppDir() + L"\\OmniPeek.ini";
    return GetPrivateProfileIntW(L"Plugins", pluginId.c_str(), 1, iniPath.c_str()) != 0;
}

void SetPluginEnabledInIni(const std::wstring& pluginId, bool enabled) {
    std::wstring iniPath = GetAppDir() + L"\\OmniPeek.ini";
    WritePrivateProfileStringW(L"Plugins", pluginId.c_str(), enabled ? L"1" : L"0", iniPath.c_str());
}

void ApplyExtensionShellEx(const std::wstring& ext, bool enable) {
    HKEY root = IsRunAsAdmin() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    std::wstring baseKey = L"Software\\Classes\\" + ext;
    std::wstring thumbSub = baseKey + L"\\ShellEx\\" + SHELLEX_THUMBNAIL_KEY;
    std::wstring prevSub  = baseKey + L"\\ShellEx\\" + SHELLEX_PREVIEW_KEY;

    if (enable) {
        HKEY hKey;
        if (RegCreateKeyExW(root, thumbSub.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)CLSID_OMNI_THUMBNAIL_STRING,
                           (DWORD)((wcslen(CLSID_OMNI_THUMBNAIL_STRING) + 1) * sizeof(WCHAR)));
            RegCloseKey(hKey);
        }
        if (RegCreateKeyExW(root, prevSub.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)CLSID_OMNI_PREVIEW_STRING,
                           (DWORD)((wcslen(CLSID_OMNI_PREVIEW_STRING) + 1) * sizeof(WCHAR)));
            RegCloseKey(hKey);
        }
    } else {
        RegDeleteKeyW(HKEY_CURRENT_USER, thumbSub.c_str());
        RegDeleteKeyW(HKEY_CURRENT_USER, prevSub.c_str());
        if (IsRunAsAdmin()) {
            RegDeleteKeyW(HKEY_LOCAL_MACHINE, thumbSub.c_str());
            RegDeleteKeyW(HKEY_LOCAL_MACHINE, prevSub.c_str());
        }
    }
}

std::wstring GetBaseCacheDir() {
    WCHAR localAppData[MAX_PATH] = {0};
    if (SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData) == S_OK) {
        return std::wstring(localAppData) + L"\\OmniPeek\\cache";
    }
    return L"";
}

void ScanDirStatsRecursive(const std::wstring& dir, int& outFiles, unsigned __int64& totalBytes) {
    if (dir.empty() || !PathFileExistsW(dir.c_str())) return;
    std::wstring search = dir + L"\\*.*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring full = dir + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ScanDirStatsRecursive(full, outFiles, totalBytes);
        } else {
            outFiles++;
            unsigned __int64 fileSize = ((unsigned __int64)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            totalBytes += fileSize;
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

void ClearDirFilesRecursive(const std::wstring& dir) {
    if (dir.empty() || !PathFileExistsW(dir.c_str())) return;
    std::wstring search = dir + L"\\*.*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring full = dir + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ClearDirFilesRecursive(full);
        } else {
            DeleteFileW(full.c_str());
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

void UnblockDirectory(const std::wstring& folderPath, int& unblockedCount) {
    std::wstring search = folderPath + L"\\*.*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring fullPath = folderPath + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            UnblockDirectory(fullPath, unblockedCount);
        } else {
            std::wstring zoneStream = fullPath + L":Zone.Identifier";
            if (DeleteFileW(zoneStream.c_str())) {
                unblockedCount++;
            }
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

void RestartPreviewHostAndNotify() {
    system("taskkill /f /im prevhost.exe >nul 2>&1");
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
}

LRESULT CALLBACK PanelProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND:
        return SendMessageW(GetParent(hWnd), WM_COMMAND, wParam, lParam);
    case WM_NOTIFY:
        return SendMessageW(GetParent(hWnd), WM_NOTIFY, wParam, lParam);
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        return SendMessageW(GetParent(hWnd), msg, wParam, lParam);
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, g_hBrushCard);
        return 1;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void PopulatePluginsControls() {
    for (auto& item : g_pluginItems) {
        if (item.hCheckbox) DestroyWindow(item.hCheckbox);
        if (item.hExtDesc) DestroyWindow(item.hExtDesc);
    }
    g_pluginItems.clear();

    auto manifests = OmniPeek::PluginRegistry::LoadAllPlugins();
    int y = 38;
    for (size_t i = 0; i < manifests.size(); i++) {
        const auto& m = manifests[i];
        PluginUIItem ui;
        ui.manifest = m;

        std::wstringstream titleSs;
        titleSs << L"【" << m.name << L"】 (" << m.id << L" v" << m.version << L") — 支持 " << m.formats.size() << L" 种后缀";

        ui.hCheckbox = CreateWindowExW(0, L"BUTTON", titleSs.str().c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            20, y, 620, 20, g_hPanelPlugins, (HMENU)(UINT_PTR)(1000 + i), GetModuleHandleW(NULL), NULL);
        SendMessageW(ui.hCheckbox, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);
        SendMessageW(ui.hCheckbox, BM_SETCHECK, IsPluginEnabledInIni(m.id) ? BST_CHECKED : BST_UNCHECKED, 0);

        std::wstringstream extSs;
        extSs << L"   支持格式: ";
        for (size_t k = 0; k < m.formats.size(); k++) {
            if (k > 0) extSs << L", ";
            extSs << m.formats[k].ext;
            if (extSs.str().length() > 92 && k + 1 < m.formats.size()) {
                extSs << L" ... (等共 " << m.formats.size() << L" 种)";
                break;
            }
        }

        ui.hExtDesc = CreateWindowExW(0, L"STATIC", extSs.str().c_str(),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            24, y + 21, 615, 18, g_hPanelPlugins, NULL, GetModuleHandleW(NULL), NULL);
        SendMessageW(ui.hExtDesc, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

        g_pluginItems.push_back(ui);
        y += 45;
    }
}

// Panel 0: 插件与格式引擎管理
void CreatePluginsPanel(HWND hParent) {
    g_hPanelPlugins = CreateWindowExW(0, L"OmniPeekPanelClass", L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        20, 118, 660, 360, hParent, NULL, GetModuleHandleW(NULL), NULL);

    HWND hLbl = CreateWindowExW(0, L"STATIC", L"动态扫描 plugins/*/plugin.json 发现的预览引擎插件（新增格式只需放入新插件目录）：",
        WS_CHILD | WS_VISIBLE, 16, 10, 620, 20, g_hPanelPlugins, NULL, GetModuleHandleW(NULL), NULL);
    SendMessageW(hLbl, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    PopulatePluginsControls();

    auto settings = OmniPeek::PluginRegistry::LoadGlobalSettings();

    g_hChkAutoplay = CreateWindowExW(0, L"BUTTON", L"🎬 音视频预览时自动播放 (Autoplay)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        20, 232, 260, 22, g_hPanelPlugins, (HMENU)110, GetModuleHandleW(NULL), NULL);
    SendMessageW(g_hChkAutoplay, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    SendMessageW(g_hChkAutoplay, BM_SETCHECK, settings.autoplayMedia ? BST_CHECKED : BST_UNCHECKED, 0);

    HWND hVolLbl = CreateWindowExW(0, L"STATIC", L"🔊 默认媒体音量:",
        WS_CHILD | WS_VISIBLE, 300, 235, 110, 20, g_hPanelPlugins, NULL, GetModuleHandleW(NULL), NULL);
    SendMessageW(hVolLbl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    g_hComboVolume = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        415, 231, 100, 160, g_hPanelPlugins, (HMENU)111, GetModuleHandleW(NULL), NULL);
    SendMessageW(g_hComboVolume, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    const int vols[] = { 0, 20, 40, 60, 80, 100 };
    int selIdx = 3;
    for (int i = 0; i < 6; i++) {
        std::wstring vstr = std::to_wstring(vols[i]) + L"%";
        SendMessageW(g_hComboVolume, CB_ADDSTRING, 0, (LPARAM)vstr.c_str());
        if (abs(settings.defaultVolume - vols[i]) <= 10) selIdx = i;
    }
    SendMessageW(g_hComboVolume, CB_SETCURSEL, selIdx, 0);

    HWND btnSelectAll = CreateWindowExW(0, L"BUTTON", L"全选插件", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        20, 272, 90, 30, g_hPanelPlugins, (HMENU)100, GetModuleHandleW(NULL), NULL);
    SendMessageW(btnSelectAll, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    HWND btnRescan = CreateWindowExW(0, L"BUTTON", L"🔄 重新扫描 plugins 目录", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        120, 272, 175, 30, g_hPanelPlugins, (HMENU)101, GetModuleHandleW(NULL), NULL);
    SendMessageW(btnRescan, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    HWND btnApply = CreateWindowExW(0, L"BUTTON", L"💾 保存配置并同步注册表", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        435, 270, 205, 34, g_hPanelPlugins, (HMENU)102, GetModuleHandleW(NULL), NULL);
    SendMessageW(btnApply, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    HWND hHint = CreateWindowExW(0, L"STATIC",
        L"🛡️ 安全承诺：OmniPeek 严格只挂载 ShellEx 预览与缩略图接口，绝不篡改任何文件的双击默认打开程序！",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 316, 620, 32, g_hPanelPlugins, NULL, GetModuleHandleW(NULL), NULL);
    SendMessageW(hHint, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
}

void UpdateStatusUI(HWND hEdit) {
    std::wstring appDir = GetAppDir();
    std::wstring dllPath = appDir + L"\\OmniPeekExtension.dll";
    std::wstring workerPath = appDir + L"\\OmniPeekWorker.exe";
    auto plugins = OmniPeek::PluginRegistry::LoadAllPlugins();
    size_t totalExts = 0;
    for (const auto& p : plugins) totalExts += p.formats.size();

    std::wstringstream ss;
    ss << L"【OmniPeek 万能预览器 · 核心微内核与插件生态体检报告】\r\n\r\n"
       << L"● 程序运行根目录:\r\n  " << appDir << L"\r\n\r\n"
       << L"● COM 64位微内核外壳扩展 (OmniPeekExtension.dll):\r\n  "
       << (PathFileExistsW(dllPath.c_str()) ? L"✓ 正常就绪 (支持 IPreviewHandler 进程级常驻热复用 + IThumbnailProvider)" : L"✗ 未找到") << L"\r\n\r\n"
       << L"● 多模态后台渲染与缩略图引擎 (OmniPeekWorker.exe):\r\n  "
       << (PathFileExistsW(workerPath.c_str()) ? L"✓ 正常就绪 (支持 3D 光栅化 + 字体/归档/代码高清卡片缩略图 + 零解压头部读取)" : L"✗ 未找到") << L"\r\n\r\n"
       << L"● 动态插件注册中心 (PluginRegistry):\r\n  ✓ 已发现 " << plugins.size() << L" 个插件包，共覆盖 " << totalExts << L" 种文件后缀名\r\n\r\n"
       << L"● 当前进程权限:\r\n  "
       << (IsRunAsAdmin() ? L"✓ 管理员权限 (Administrator)" : L"● 当前用户权限 (HKCU 无感注册模式)");

    SetWindowTextW(hEdit, ss.str().c_str());
}

// Panel 1: 系统体检与扩展开发
void CreateStatusPanel(HWND hParent) {
    g_hPanelStatus = CreateWindowExW(0, L"OmniPeekPanelClass", L"", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        20, 118, 660, 360, hParent, NULL, GetModuleHandleW(NULL), NULL);

    HWND hTitle = CreateWindowExW(0, L"STATIC", L"核心组件状态与插件 SDK 维护",
        WS_CHILD | WS_VISIBLE, 16, 10, 620, 20, g_hPanelStatus, NULL, GetModuleHandleW(NULL), NULL);
    SendMessageW(hTitle, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    g_hStatusInfoText = CreateWindowExW(WS_EX_STATICEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        16, 36, 625, 245, g_hPanelStatus, NULL, GetModuleHandleW(NULL), NULL);
    SendMessageW(g_hStatusInfoText, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    UpdateStatusUI(g_hStatusInfoText);

    HWND btnReinstall = CreateWindowExW(0, L"BUTTON", L"🔧 一键重新注册 COM 扩展", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        16, 296, 195, 34, g_hPanelStatus, (HMENU)201, GetModuleHandleW(NULL), NULL);
    SendMessageW(btnReinstall, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    HWND btnUninstall = CreateWindowExW(0, L"BUTTON", L"🛑 注销并恢复系统默认预览", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        225, 296, 195, 34, g_hPanelStatus, (HMENU)202, GetModuleHandleW(NULL), NULL);
    SendMessageW(btnUninstall, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    HWND btnOpenPlugins = CreateWindowExW(0, L"BUTTON", L"📂 打开 plugins 插件目录", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        435, 296, 205, 34, g_hPanelStatus, (HMENU)203, GetModuleHandleW(NULL), NULL);
    SendMessageW(btnOpenPlugins, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
}

void UpdateCacheUI(HWND hLabel) {
    int count = 0;
    unsigned __int64 totalBytes = 0;
    std::wstring cacheDir = GetBaseCacheDir();
    ScanDirStatsRecursive(cacheDir, count, totalBytes);
    double sizeMB = (double)totalBytes / (1024.0 * 1024.0);

    std::wstringstream ss;
    ss << L"【多模态缩略图与预处理二级缓存管理】\r\n\r\n"
       << L"● 缓存根目录:\r\n  " << cacheDir << L"\r\n\r\n"
       << L"● 已缓存文件总数 (含缩略图/CAD网格/大文本切片/压缩包索引): " << count << L" 个\r\n"
       << L"● 累计磁盘占用空间: " << std::fixed << std::setprecision(2) << sizeMB << L" MB\r\n\r\n"
       << L"说明与排错指南：\r\n"
       << L"1. 缩略图与 STEP/归档预处理结果均基于 [路径 + 修改时间戳 + 文件字节数] 生成哈希指纹，源文件修改后自动失效重算。\r\n"
       << L"2. 若从浏览器下载的文件在资源管理器右侧预览窗格提示“可能对你的计算机有害”，请点击下方【🔓 解除文件夹网络锁定】一键清除 Zone.Identifier 标记。";

    SetWindowTextW(hLabel, ss.str().c_str());
}

// Panel 2: 缓存与排错
void CreateCachePanel(HWND hParent) {
    g_hPanelCache = CreateWindowExW(0, L"OmniPeekPanelClass", L"", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        20, 118, 660, 360, hParent, NULL, GetModuleHandleW(NULL), NULL);

    HWND hTitle = CreateWindowExW(0, L"STATIC", L"二级缓存加速与网络锁定排错",
        WS_CHILD | WS_VISIBLE, 16, 10, 620, 20, g_hPanelCache, NULL, GetModuleHandleW(NULL), NULL);
    SendMessageW(hTitle, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    g_hCacheInfoText = CreateWindowExW(WS_EX_STATICEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        16, 36, 625, 245, g_hPanelCache, NULL, GetModuleHandleW(NULL), NULL);
    SendMessageW(g_hCacheInfoText, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    UpdateCacheUI(g_hCacheInfoText);

    HWND btnClear = CreateWindowExW(0, L"BUTTON", L"🧹 一键清空所有缓存", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        16, 296, 185, 34, g_hPanelCache, (HMENU)301, GetModuleHandleW(NULL), NULL);
    SendMessageW(btnClear, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    HWND btnRestartExp = CreateWindowExW(0, L"BUTTON", L"🔄 刷新资源管理器预览宿主", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        215, 296, 200, 34, g_hPanelCache, (HMENU)302, GetModuleHandleW(NULL), NULL);
    SendMessageW(btnRestartExp, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    HWND btnUnblock = CreateWindowExW(0, L"BUTTON", L"🔓 解除文件夹网络锁定", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        430, 296, 210, 34, g_hPanelCache, (HMENU)303, GetModuleHandleW(NULL), NULL);
    SendMessageW(btnUnblock, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
}

void SwitchTab(int index) {
    ShowWindow(g_hPanelPlugins, (index == 0) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hPanelStatus,  (index == 1) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hPanelCache,   (index == 2) ? SW_SHOW : SW_HIDE);

    HWND active = g_hPanelPlugins;
    if (index == 1) active = g_hPanelStatus;
    else if (index == 2) active = g_hPanelCache;

    BringWindowToTop(active);
    InvalidateRect(active, NULL, TRUE);

    if (index == 1 && g_hStatusInfoText) UpdateStatusUI(g_hStatusInfoText);
    if (index == 2 && g_hCacheInfoText) UpdateCacheUI(g_hCacheInfoText);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hTab = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            20, 84, 660, 400, hWnd, (HMENU)1, GetModuleHandleW(NULL), NULL);
        SendMessageW(g_hTab, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        TCITEMW tie = {0};
        tie.mask = TCIF_TEXT;
        tie.pszText = (LPWSTR)L"🧩 插件与格式引擎";
        TabCtrl_InsertItem(g_hTab, 0, &tie);
        tie.pszText = (LPWSTR)L"🩺 系统体检与开发";
        TabCtrl_InsertItem(g_hTab, 1, &tie);
        tie.pszText = (LPWSTR)L"⚡ 缓存与排错";
        TabCtrl_InsertItem(g_hTab, 2, &tie);

        CreatePluginsPanel(hWnd);
        CreateStatusPanel(hWnd);
        CreateCachePanel(hWnd);
        SwitchTab(0);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rcClient;
        GetClientRect(hWnd, &rcClient);

        RECT rcTop = { 0, 0, rcClient.right, 72 };
        HBRUSH hBrTop = CreateSolidBrush(CLR_CARD_BG);
        FillRect(hdc, &rcTop, hBrTop);
        DeleteObject(hBrTop);

        HPEN hPenDiv = CreatePen(PS_SOLID, 1, CLR_BORDER);
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPenDiv);
        MoveToEx(hdc, 0, 72, NULL);
        LineTo(hdc, rcClient.right, 72);

        SelectObject(hdc, g_hFontHeader);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CLR_TEXT_DARK);
        const wchar_t* titleText = L"OmniPeek 万能预览器控制中心";
        TextOutW(hdc, 24, 12, titleText, (int)wcslen(titleText));

        RECT rcBadge = { 315, 14, 385, 34 };
        HBRUSH hBrBadge = CreateSolidBrush(RGB(224, 242, 254));
        HPEN hPenBadge = CreatePen(PS_SOLID, 1, RGB(125, 211, 252));
        SelectObject(hdc, hBrBadge);
        SelectObject(hdc, hPenBadge);
        RoundRect(hdc, rcBadge.left, rcBadge.top, rcBadge.right, rcBadge.bottom, 8, 8);
        DeleteObject(hBrBadge);
        DeleteObject(hPenBadge);

        SelectObject(hdc, g_hFontSmall);
        SetTextColor(hdc, CLR_PRIMARY);
        DrawTextW(hdc, L"v1.0.0", -1, &rcBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, g_hFontSmall);
        SetTextColor(hdc, CLR_TEXT_MUTED);
        const wchar_t* subText = L"插件化微内核 Windows 资源管理器预览与缩略图引擎 · 支持 3D/CAD、音视频、代码文档、表格、字体与压缩包";
        TextOutW(hdc, 24, 42, subText, (int)wcslen(subText));

        SelectObject(hdc, hOldPen);
        DeleteObject(hPenDiv);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, g_hBrushBg);
        return 1;
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
        if (pdis->CtlID == 102) {
            HDC hdc = pdis->hDC;
            RECT rc = pdis->rcItem;

            bool isPressed = (pdis->itemState & ODS_SELECTED);
            COLORREF btnColor = isPressed ? RGB(3, 105, 161) : CLR_PRIMARY;

            HBRUSH hBrBtn = CreateSolidBrush(btnColor);
            HPEN hPenBtn = CreatePen(PS_SOLID, 1, btnColor);
            HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hBrBtn);
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPenBtn);

            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);

            SelectObject(hdc, g_hFontBold);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            DrawTextW(hdc, L"💾 保存配置并同步注册表", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, hOldBr);
            SelectObject(hdc, hOldPen);
            DeleteObject(hBrBtn);
            DeleteObject(hPenBtn);
            return TRUE;
        }
        break;
    }

    case WM_NOTIFY: {
        LPNMHDR pnmh = (LPNMHDR)lParam;
        if (pnmh->hwndFrom == g_hTab && pnmh->code == TCN_SELCHANGE) {
            int cur = TabCtrl_GetCurSel(g_hTab);
            SwitchTab(cur);
        }
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == 100) { // Select all plugins
            for (auto& item : g_pluginItems) {
                if (item.hCheckbox) {
                    SendMessageW(item.hCheckbox, BM_SETCHECK, BST_CHECKED, 0);
                }
            }
        } else if (id == 101) { // Rescan plugins directory
            PopulatePluginsControls();
            InvalidateRect(g_hPanelPlugins, NULL, TRUE);
            std::wstringstream ss;
            ss << L"已重新扫描 plugins/ 目录，共发现 " << g_pluginItems.size() << L" 个插件包！";
            MessageBoxW(hWnd, ss.str().c_str(), L"插件扫描完成", MB_OK | MB_ICONINFORMATION);
        } else if (id == 102) { // Save and apply
            int enabledPlugins = 0;
            size_t enabledExts = 0;
            for (auto& item : g_pluginItems) {
                bool chk = (SendMessageW(item.hCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
                SetPluginEnabledInIni(item.manifest.id, chk);
                for (const auto& fmt : item.manifest.formats) {
                    ApplyExtensionShellEx(fmt.ext, chk);
                }
                if (chk) {
                    enabledPlugins++;
                    enabledExts += item.manifest.formats.size();
                }
            }

            OmniPeek::GlobalSettings settings = OmniPeek::PluginRegistry::LoadGlobalSettings();
            settings.autoplayMedia = (SendMessageW(g_hChkAutoplay, BM_GETCHECK, 0, 0) == BST_CHECKED);
            int volIdx = (int)SendMessageW(g_hComboVolume, CB_GETCURSEL, 0, 0);
            const int vols[] = { 0, 20, 40, 60, 80, 100 };
            if (volIdx >= 0 && volIdx < 6) settings.defaultVolume = vols[volIdx];
            OmniPeek::PluginRegistry::SaveGlobalSettings(settings);

            RestartPreviewHostAndNotify();
            std::wstringstream msg;
            msg << L"🎉 配置已保存至 OmniPeek.ini 并同步至系统注册表！\r\n\r\n"
                << L"● 已启用插件包: " << enabledPlugins << L" / " << g_pluginItems.size() << L" 个\r\n"
                << L"● 已激活预览与缩略图后缀: " << enabledExts << L" 种格式\r\n"
                << L"● 媒体默认音量: " << settings.defaultVolume << L"% ("
                << (settings.autoplayMedia ? L"自动播放开启" : L"自动播放关闭") << L")";
            MessageBoxW(hWnd, msg.str().c_str(), L"OmniPeek 配置已生效", MB_OK | MB_ICONINFORMATION);
        } else if (id == 201) { // Re-register COM DLL
            std::wstring regCmd = L"regsvr32.exe /s \"" + GetAppDir() + L"\\OmniPeekExtension.dll\"";
            _wsystem(regCmd.c_str());
            RestartPreviewHostAndNotify();
            MessageBoxW(hWnd, L"已重新注册 OmniPeekExtension.dll 并同步所有已启用插件的格式！", L"注册成功", MB_OK | MB_ICONINFORMATION);
        } else if (id == 202) { // Unregister COM DLL
            std::wstring unregCmd = L"regsvr32.exe /u /s \"" + GetAppDir() + L"\\OmniPeekExtension.dll\"";
            _wsystem(unregCmd.c_str());
            RestartPreviewHostAndNotify();
            MessageBoxW(hWnd, L"已注销 OmniPeek 扩展并无损还原系统原有的预览器注册表项。", L"已注销", MB_OK | MB_ICONINFORMATION);
        } else if (id == 203) { // Open plugins directory
            std::wstring pluginsDir = OmniPeek::PluginRegistry::FindPluginsDir();
            ShellExecuteW(hWnd, L"explore", pluginsDir.c_str(), NULL, NULL, SW_SHOWNORMAL);
        } else if (id == 301) { // Clear cache
            ClearDirFilesRecursive(GetBaseCacheDir());
            UpdateCacheUI(g_hCacheInfoText);
            MessageBoxW(hWnd, L"OmniPeek 本地缩略图与预处理二级缓存已全部清空！", L"清理完成", MB_OK | MB_ICONINFORMATION);
        } else if (id == 302) { // Restart prevhost
            RestartPreviewHostAndNotify();
            MessageBoxW(hWnd, L"已重启 prevhost.exe 预览宿主并通知资源管理器刷新图标缓存！", L"刷新完成", MB_OK | MB_ICONINFORMATION);
        } else if (id == 303) { // Unblock folder
            IFileDialog* pfd = NULL;
            if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) {
                DWORD dwOptions = 0;
                if (SUCCEEDED(pfd->GetOptions(&dwOptions))) {
                    pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
                }
                pfd->SetTitle(L"选择需要解除网络锁定 (Zone.Identifier) 的文件夹");
                if (SUCCEEDED(pfd->Show(hWnd))) {
                    IShellItem* psi = NULL;
                    if (SUCCEEDED(pfd->GetResult(&psi))) {
                        LPWSTR pszPath = NULL;
                        if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                            int count = 0;
                            UnblockDirectory(pszPath, count);
                            std::wstringstream msg;
                            msg << L"🎉 已完成文件夹网络锁定扫描与解除！\r\n\r\n"
                                << L"目录: " << pszPath << L"\r\n"
                                << L"成功解除 " << count << L" 个文件的 Zone.Identifier 标记。";
                            MessageBoxW(hWnd, msg.str().c_str(), L"解除锁定成功", MB_OK | MB_ICONINFORMATION);
                            CoTaskMemFree(pszPath);
                        }
                        psi->Release();
                    }
                }
                pfd->Release();
            }
        }
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, CLR_TEXT_DARK);
        SetBkColor(hdcStatic, CLR_CARD_BG);
        return (INT_PTR)g_hBrushCard;
    }

    case WM_CTLCOLORBTN: {
        return (INT_PTR)g_hBrushCard;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_TAB_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icex);

    g_hBrushBg = CreateSolidBrush(CLR_BG);
    g_hBrushCard = CreateSolidBrush(CLR_CARD_BG);
    g_hPenBorder = CreatePen(PS_SOLID, 1, CLR_BORDER);

    g_hFontHeader = CreateFontW(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    g_hFontTitle  = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    g_hFontBold   = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    g_hFontNormal = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    g_hFontSmall  = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");

    WNDCLASSEXW pc = { sizeof(pc) };
    pc.lpfnWndProc = PanelProc;
    pc.hInstance = hInstance;
    pc.lpszClassName = L"OmniPeekPanelClass";
    pc.hbrBackground = g_hBrushCard;
    pc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    RegisterClassExW(&pc);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"OmniPeekSettingsClass";
    wc.hbrBackground = g_hBrushBg;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    RegisterClassExW(&wc);

    int w = 720;
    int h = 540;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    HWND hWnd = CreateWindowExW(0, L"OmniPeekSettingsClass", L"OmniPeek 万能预览器控制中心 v1.0.0",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, w, h,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hFontHeader) DeleteObject(g_hFontHeader);
    if (g_hFontTitle) DeleteObject(g_hFontTitle);
    if (g_hFontBold) DeleteObject(g_hFontBold);
    if (g_hFontNormal) DeleteObject(g_hFontNormal);
    if (g_hFontSmall) DeleteObject(g_hFontSmall);
    if (g_hBrushBg) DeleteObject(g_hBrushBg);
    if (g_hBrushCard) DeleteObject(g_hBrushCard);
    if (g_hPenBorder) DeleteObject(g_hPenBorder);

    CoUninitialize();
    return (int)msg.wParam;
}
