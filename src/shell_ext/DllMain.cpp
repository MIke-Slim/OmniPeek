#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <vector>
#include <string>
#include "../common/Guids.h"
#include "../common/PluginRegistry.h"
#include "ClassFactory.h"
#include "OmniPeekThumbnailProvider.h"
#include "OmniPeekPreviewHandler.h"
#include "Logger.h"

HINSTANCE g_hInst = NULL;
long g_serverLocks = 0;

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        g_hInst = hInstance;
        DisableThreadLibraryCalls(hInstance);
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    if (IsEqualCLSID(rclsid, CLSID_OmniPeekThumbnailProvider)) {
        ClassFactory<OmniPeekThumbnailProvider> *pFactory = new (std::nothrow) ClassFactory<OmniPeekThumbnailProvider>();
        if (!pFactory) return E_OUTOFMEMORY;
        HRESULT hr = pFactory->QueryInterface(riid, ppv);
        pFactory->Release();
        return hr;
    } else if (IsEqualCLSID(rclsid, CLSID_OmniPeekPreviewHandler)) {
        ClassFactory<OmniPeekPreviewHandler> *pFactory = new (std::nothrow) ClassFactory<OmniPeekPreviewHandler>();
        if (!pFactory) return E_OUTOFMEMORY;
        HRESULT hr = pFactory->QueryInterface(riid, ppv);
        pFactory->Release();
        return hr;
    }

    return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow() {
    return (g_serverLocks == 0) ? S_OK : S_FALSE;
}

static HRESULT SetRegString(HKEY root, LPCWSTR subKey, LPCWSTR valueName, LPCWSTR value) {
    HKEY hKey;
    LONG res = RegCreateKeyExW(root, subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (res != ERROR_SUCCESS) return HRESULT_FROM_WIN32(res);
    res = RegSetValueExW(hKey, valueName, 0, REG_SZ, (const BYTE*)value, (DWORD)((wcslen(value) + 1) * sizeof(WCHAR)));
    RegCloseKey(hKey);
    return HRESULT_FROM_WIN32(res);
}

static HRESULT SetRegDword(HKEY root, LPCWSTR subKey, LPCWSTR valueName, DWORD value) {
    HKEY hKey;
    LONG res = RegCreateKeyExW(root, subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (res != ERROR_SUCCESS) return HRESULT_FROM_WIN32(res);
    res = RegSetValueExW(hKey, valueName, 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));
    RegCloseKey(hKey);
    return HRESULT_FROM_WIN32(res);
}

static std::wstring ReadRegString(HKEY root, LPCWSTR subKey, LPCWSTR valueName) {
    HKEY hKey = NULL;
    if (RegOpenKeyExW(root, subKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) return L"";
    WCHAR buf[256] = {0};
    DWORD sz = sizeof(buf);
    std::wstring res;
    if (RegQueryValueExW(hKey, valueName, NULL, NULL, (LPBYTE)buf, &sz) == ERROR_SUCCESS) {
        res = buf;
    }
    RegCloseKey(hKey);
    return res;
}

static HKEY GetRegistryRoot() {
    HKEY testKey = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Classes", 0, KEY_WRITE, &testKey) == ERROR_SUCCESS) {
        RegCloseKey(testKey);
        return HKEY_LOCAL_MACHINE;
    }
    return HKEY_CURRENT_USER;
}

// Backup previous ShellEx handler before overwriting so we can restore cleanly on unregister
static void RegisterShellExWithBackup(HKEY root, const std::wstring& shellExKeyPath, LPCWSTR omniClsid) {
    std::wstring existing = ReadRegString(root, shellExKeyPath.c_str(), NULL);
    if (!existing.empty() && _wcsicmp(existing.c_str(), omniClsid) != 0) {
        SetRegString(root, shellExKeyPath.c_str(), L"OmniPeekBackup", existing.c_str());
    }
    SetRegString(root, shellExKeyPath.c_str(), NULL, omniClsid);
}

static void UnregisterShellExWithRestore(HKEY root, const std::wstring& shellExKeyPath, LPCWSTR omniClsid) {
    std::wstring current = ReadRegString(root, shellExKeyPath.c_str(), NULL);
    std::wstring backup  = ReadRegString(root, shellExKeyPath.c_str(), L"OmniPeekBackup");
    if (!backup.empty()) {
        SetRegString(root, shellExKeyPath.c_str(), NULL, backup.c_str());
        HKEY hKey = NULL;
        if (RegOpenKeyExW(root, shellExKeyPath.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, L"OmniPeekBackup");
            RegCloseKey(hKey);
        }
    } else if (_wcsicmp(current.c_str(), omniClsid) == 0) {
        RegDeleteTreeW(root, shellExKeyPath.c_str());
    }
}

STDAPI DllRegisterServer() {
    WCHAR dllPath[MAX_PATH];
    GetModuleFileNameW(g_hInst, dllPath, MAX_PATH);
    LogTrace(std::wstring(L"DllRegisterServer started for: ") + dllPath);

    HKEY root = GetRegistryRoot();

    // 1. Register Thumbnail Provider CLSID
    std::wstring thumbClsidKey = L"Software\\Classes\\CLSID\\" + std::wstring(CLSID_OMNI_THUMBNAIL_STRING);
    SetRegString(root, thumbClsidKey.c_str(), NULL, L"OmniPeek Universal Thumbnail Provider");
    SetRegString(root, (thumbClsidKey + L"\\InprocServer32").c_str(), NULL, dllPath);
    SetRegString(root, (thumbClsidKey + L"\\InprocServer32").c_str(), L"ThreadingModel", L"Apartment");
    SetRegDword(root, thumbClsidKey.c_str(), L"DisableProcessIsolation", 1);

    // 2. Register Preview Handler CLSID
    std::wstring prevClsidKey = L"Software\\Classes\\CLSID\\" + std::wstring(CLSID_OMNI_PREVIEW_STRING);
    SetRegString(root, prevClsidKey.c_str(), NULL, L"OmniPeek Universal Preview Handler");
    SetRegString(root, prevClsidKey.c_str(), L"DisplayName", L"OmniPeek Universal Previewer");
    SetRegString(root, prevClsidKey.c_str(), L"AppID", PREVHOST_APPID_STRING);
    SetRegString(root, (prevClsidKey + L"\\InprocServer32").c_str(), NULL, dllPath);
    SetRegString(root, (prevClsidKey + L"\\InprocServer32").c_str(), L"ThreadingModel", L"Apartment");
    SetRegDword(root, prevClsidKey.c_str(), L"DisableProcessIsolation", 1);

    // 3. Register to Windows PreviewHandlers list
    SetRegString(root,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers",
        CLSID_OMNI_PREVIEW_STRING,
        L"OmniPeek Universal Previewer");

    // 4. Dynamically discover all plugins and formats from PluginRegistry
    auto formats = OmniPeek::PluginRegistry::GetAllFormats(g_hInst);
    for (const auto& fmt : formats) {
        const std::wstring& ext = fmt.ext;
        std::wstring shellExThumb = L"Software\\Classes\\" + ext + L"\\ShellEx\\" + SHELLEX_THUMBNAIL_KEY;
        RegisterShellExWithBackup(root, shellExThumb, CLSID_OMNI_THUMBNAIL_STRING);

        std::wstring shellExPrev = L"Software\\Classes\\" + ext + L"\\ShellEx\\" + SHELLEX_PREVIEW_KEY;
        RegisterShellExWithBackup(root, shellExPrev, CLSID_OMNI_PREVIEW_STRING);

        // Also register on associated ProgIDs without changing OpenWith / default double-click app
        std::vector<std::wstring> progIds;
        auto QueryProgId = [&](HKEY hR, const std::wstring& kPath, LPCWSTR vName) {
            std::wstring val = ReadRegString(hR, kPath.c_str(), vName);
            if (!val.empty()) progIds.push_back(val);
        };
        QueryProgId(root, L"Software\\Classes\\" + ext, NULL);
        QueryProgId(HKEY_CURRENT_USER, L"Software\\Classes\\" + ext, NULL);
        QueryProgId(HKEY_LOCAL_MACHINE, L"Software\\Classes\\" + ext, NULL);
        QueryProgId(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\" + ext + L"\\UserChoice", L"ProgId");

        for (const auto& pid : progIds) {
            if (pid.empty()) continue;
            std::wstring pThumb = L"Software\\Classes\\" + pid + L"\\ShellEx\\" + SHELLEX_THUMBNAIL_KEY;
            RegisterShellExWithBackup(root, pThumb, CLSID_OMNI_THUMBNAIL_STRING);
            std::wstring pPrev = L"Software\\Classes\\" + pid + L"\\ShellEx\\" + SHELLEX_PREVIEW_KEY;
            RegisterShellExWithBackup(root, pPrev, CLSID_OMNI_PREVIEW_STRING);
        }
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    LogTrace(L"DllRegisterServer completed successfully for " + std::to_wstring(formats.size()) + L" formats.");
    return S_OK;
}

STDAPI DllUnregisterServer() {
    HKEY root = GetRegistryRoot();

    std::wstring thumbClsidKey = L"Software\\Classes\\CLSID\\" + std::wstring(CLSID_OMNI_THUMBNAIL_STRING);
    RegDeleteTreeW(root, thumbClsidKey.c_str());

    std::wstring prevClsidKey = L"Software\\Classes\\CLSID\\" + std::wstring(CLSID_OMNI_PREVIEW_STRING);
    RegDeleteTreeW(root, prevClsidKey.c_str());

    HKEY hKey;
    if (RegOpenKeyExW(root, L"Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, CLSID_OMNI_PREVIEW_STRING);
        RegCloseKey(hKey);
    }

    auto formats = OmniPeek::PluginRegistry::GetAllFormats(g_hInst);
    for (const auto& fmt : formats) {
        const std::wstring& ext = fmt.ext;
        std::wstring shellExThumb = L"Software\\Classes\\" + ext + L"\\ShellEx\\" + SHELLEX_THUMBNAIL_KEY;
        UnregisterShellExWithRestore(root, shellExThumb, CLSID_OMNI_THUMBNAIL_STRING);

        std::wstring shellExPrev = L"Software\\Classes\\" + ext + L"\\ShellEx\\" + SHELLEX_PREVIEW_KEY;
        UnregisterShellExWithRestore(root, shellExPrev, CLSID_OMNI_PREVIEW_STRING);
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return S_OK;
}
