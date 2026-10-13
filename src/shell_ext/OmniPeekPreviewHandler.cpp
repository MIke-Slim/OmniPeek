#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include "OmniPeekPreviewHandler.h"
#include "../common/Guids.h"
#include "WebView2Callbacks.h"
#include "Logger.h"
#include <shlwapi.h>
#include <shlobj.h>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstdint>

extern HINSTANCE g_hInst;
extern long g_serverLocks;

static const WCHAR* PREVIEW_WND_CLASS = L"OmniPeekPreviewHostClass";

static std::wstring UrlEncodePathComponent(const std::wstring& str) {
    std::string u8 = OmniPeek::WideToUtf8(str);
    std::wstringstream out;
    const char* hex = "0123456789ABCDEF";
    for (unsigned char c : u8) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~' || c == '/' || c == ':') {
            out << (WCHAR)c;
        } else if (c == '\\') {
            out << L'/';
        } else {
            out << L'%' << (WCHAR)hex[(c >> 4) & 0xF] << (WCHAR)hex[c & 0xF];
        }
    }
    return out.str();
}

static std::wstring DumpStreamToTemp(IStream* pStream, LPCWSTR origName) {
    std::wstring ext = L".bin";
    if (origName) {
        LPCWSTR dot = wcsrchr(origName, L'.');
        if (dot) ext = dot;
    }
    WCHAR tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    WCHAR tempFile[MAX_PATH];
    GetTempFileNameW(tempPath, L"opk", 0, tempFile);
    std::wstring finalTemp = std::wstring(tempFile) + ext;
    MoveFileW(tempFile, finalTemp.c_str());

    FILE* f = _wfopen(finalTemp.c_str(), L"wb");
    if (!f) return L"";

    LARGE_INTEGER liZero = {0};
    pStream->Seek(liZero, STREAM_SEEK_SET, NULL);

    char buf[16384];
    ULONG bytesRead = 0;
    while (SUCCEEDED(pStream->Read(buf, sizeof(buf), &bytesRead)) && bytesRead > 0) {
        fwrite(buf, 1, bytesRead, f);
    }
    fclose(f);
    return finalTemp;
}

OmniPeekPreviewHandler::OmniPeekPreviewHandler()
    : m_refCount(1), m_hwndParent(NULL), m_hwndPreview(NULL),
      m_punkSite(nullptr), m_controller(nullptr), m_webview(nullptr) {
    ZeroMemory(&m_rcParent, sizeof(m_rcParent));
    LogTrace(L"OmniPeekPreviewHandler created");
    InterlockedIncrement(&g_serverLocks);
}

OmniPeekPreviewHandler::~OmniPeekPreviewHandler() {
    LogTrace(L"OmniPeekPreviewHandler destroyed");
    Unload();
    if (m_punkSite) {
        m_punkSite->Release();
        m_punkSite = nullptr;
    }
    InterlockedDecrement(&g_serverLocks);
}

STDMETHODIMP OmniPeekPreviewHandler::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == __uuidof(IPreviewHandler)) {
        *ppv = static_cast<IPreviewHandler*>(this);
    } else if (riid == __uuidof(IInitializeWithFile)) {
        *ppv = static_cast<IInitializeWithFile*>(this);
    } else if (riid == __uuidof(IInitializeWithItem)) {
        *ppv = static_cast<IInitializeWithItem*>(this);
    } else if (riid == __uuidof(IInitializeWithStream)) {
        *ppv = static_cast<IInitializeWithStream*>(this);
    } else if (riid == __uuidof(IObjectWithSite)) {
        *ppv = static_cast<IObjectWithSite*>(this);
    } else if (riid == __uuidof(IOleWindow)) {
        *ppv = static_cast<IOleWindow*>(this);
    } else if (riid == __uuidof(IPreviewHandlerVisuals)) {
        *ppv = static_cast<IPreviewHandlerVisuals*>(this);
    } else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) OmniPeekPreviewHandler::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

STDMETHODIMP_(ULONG) OmniPeekPreviewHandler::Release() {
    ULONG count = InterlockedDecrement(&m_refCount);
    if (count == 0) delete this;
    return count;
}

STDMETHODIMP OmniPeekPreviewHandler::Initialize(LPCWSTR pszFilePath, DWORD /*grfMode*/) {
    LogTrace(std::wstring(L"PreviewHandler::Initialize(File): ") + (pszFilePath ? pszFilePath : L"null"));
    if (!pszFilePath) return E_INVALIDARG;
    m_filePath = pszFilePath;
    return S_OK;
}

STDMETHODIMP OmniPeekPreviewHandler::Initialize(IShellItem *psi, DWORD /*grfMode*/) {
    if (!psi) return E_INVALIDARG;
    LPWSTR pszPath = nullptr;
    HRESULT hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
    if (SUCCEEDED(hr) && pszPath) {
        m_filePath = pszPath;
        LogTrace(std::wstring(L"PreviewHandler::Initialize(IShellItem): ") + m_filePath);
        CoTaskMemFree(pszPath);
        return S_OK;
    }
    return hr;
}

STDMETHODIMP OmniPeekPreviewHandler::Initialize(IStream *pStream, DWORD /*grfMode*/) {
    if (!pStream) return E_INVALIDARG;
    STATSTG stat = {0};
    if (SUCCEEDED(pStream->Stat(&stat, STATFLAG_DEFAULT)) && stat.pwcsName) {
        if (PathFileExistsW(stat.pwcsName)) {
            m_filePath = stat.pwcsName;
            CoTaskMemFree(stat.pwcsName);
            return S_OK;
        } else {
            m_filePath = DumpStreamToTemp(pStream, stat.pwcsName);
            CoTaskMemFree(stat.pwcsName);
            return m_filePath.empty() ? E_FAIL : S_OK;
        }
    }
    m_filePath = DumpStreamToTemp(pStream, L"file.bin");
    return m_filePath.empty() ? E_FAIL : S_OK;
}

STDMETHODIMP OmniPeekPreviewHandler::SetSite(IUnknown *pUnkSite) {
    if (m_punkSite) {
        m_punkSite->Release();
        m_punkSite = nullptr;
    }
    m_punkSite = pUnkSite;
    if (m_punkSite) m_punkSite->AddRef();
    return S_OK;
}

STDMETHODIMP OmniPeekPreviewHandler::GetSite(REFIID riid, void **ppvSite) {
    if (!ppvSite) return E_POINTER;
    *ppvSite = nullptr;
    if (!m_punkSite) return E_FAIL;
    return m_punkSite->QueryInterface(riid, ppvSite);
}

static HWND FindActivePreviewHost(HWND hTop) {
    struct SearchContext { HWND hHost; } ctx = { NULL };
    EnumChildWindows(hTop, [](HWND hChild, LPARAM lp) -> BOOL {
        WCHAR cls[128] = { 0 };
        GetClassNameW(hChild, cls, 128);
        if (wcscmp(cls, L"Shell Preview Extension Host") == 0 && IsWindowVisible(hChild)) {
            RECT rc;
            GetWindowRect(hChild, &rc);
            if ((rc.right - rc.left) > 50 && (rc.bottom - rc.top) > 50) {
                reinterpret_cast<SearchContext*>(lp)->hHost = hChild;
                return FALSE;
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));
    return ctx.hHost;
}

HWND OmniPeekPreviewHandler::ResolveValidParent(HWND candidateHwnd, IUnknown* pSite, RECT* outRc) {
    if (candidateHwnd && IsWindow(candidateHwnd)) {
        if (outRc && (outRc->right - outRc->left <= 0 || outRc->bottom - outRc->top <= 0)) {
            GetClientRect(candidateHwnd, outRc);
        }
        return candidateHwnd;
    }

    if (pSite) {
        IOleWindow* pOleWnd = nullptr;
        if (SUCCEEDED(pSite->QueryInterface(IID_IOleWindow, (void**)&pOleWnd)) && pOleWnd) {
            HWND hSite = NULL;
            if (SUCCEEDED(pOleWnd->GetWindow(&hSite)) && IsWindow(hSite)) {
                pOleWnd->Release();
                if (outRc && (outRc->right - outRc->left <= 0 || outRc->bottom - outRc->top <= 0)) {
                    GetClientRect(hSite, outRc);
                }
                return hSite;
            }
            pOleWnd->Release();
        }
    }

    HWND hForeground = GetForegroundWindow();
    if (hForeground) {
        HWND hHost = FindActivePreviewHost(hForeground);
        if (hHost && IsWindow(hHost)) {
            if (outRc) GetClientRect(hHost, outRc);
            return hHost;
        }
    }

    HWND hFound = NULL;
    EnumWindows([](HWND hTop, LPARAM lp) -> BOOL {
        WCHAR cls[128] = { 0 };
        GetClassNameW(hTop, cls, 128);
        if (wcscmp(cls, L"CabinetWClass") == 0 && IsWindowVisible(hTop)) {
            HWND hHost = FindActivePreviewHost(hTop);
            if (hHost && IsWindow(hHost)) {
                *reinterpret_cast<HWND*>(lp) = hHost;
                return FALSE;
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&hFound));

    if (hFound && IsWindow(hFound)) {
        if (outRc) GetClientRect(hFound, outRc);
        return hFound;
    }
    return candidateHwnd;
}

STDMETHODIMP OmniPeekPreviewHandler::SetWindow(HWND hwnd, const RECT *prc) {
    if (!prc) return E_INVALIDARG;
    m_rcParent = *prc;
    m_hwndParent = ResolveValidParent(hwnd, m_punkSite, &m_rcParent);
    if (m_hwndPreview && IsWindow(m_hwndPreview)) {
        if (m_hwndParent && IsWindow(m_hwndParent)) {
            SetParent(m_hwndPreview, m_hwndParent);
        }
        SetRect(&m_rcParent);
    }
    return S_OK;
}

STDMETHODIMP OmniPeekPreviewHandler::SetRect(const RECT *prc) {
    if (!prc) return E_INVALIDARG;
    m_rcParent = *prc;
    int w = m_rcParent.right - m_rcParent.left;
    int h = m_rcParent.bottom - m_rcParent.top;
    if (w <= 0 && m_hwndParent && IsWindow(m_hwndParent)) {
        RECT clRc = {0};
        GetClientRect(m_hwndParent, &clRc);
        w = clRc.right - clRc.left;
        h = clRc.bottom - clRc.top;
    }
    if (w <= 0) w = 360;
    if (h <= 0) h = 360;

    if (m_hwndPreview && IsWindow(m_hwndPreview)) {
        SetWindowPos(m_hwndPreview, NULL, 0, 0, w, h, SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    if (m_controller) {
        RECT bounds = { 0, 0, w, h };
        m_controller->put_Bounds(bounds);
    }
    return S_OK;
}

LRESULT CALLBACK OmniPeekPreviewHandler::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_SIZE: {
            OmniPeekPreviewHandler* pThis = (OmniPeekPreviewHandler*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            if (pThis && pThis->m_controller) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                if (rc.right > 0 && rc.bottom > 0) {
                    pThis->m_controller->put_Bounds(rc);
                }
            }
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

bool OmniPeekPreviewHandler::CreateChildWindow() {
    if (!m_hwndParent || !IsWindow(m_hwndParent)) {
        m_hwndParent = ResolveValidParent(m_hwndParent, m_punkSite, &m_rcParent);
    }
    if (!m_hwndParent || !IsWindow(m_hwndParent)) {
        LogTrace(L"CreateChildWindow: invalid m_hwndParent");
        return false;
    }

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = g_hInst;
    wc.lpszClassName = PREVIEW_WND_CLASS;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassExW(&wc);

    int w = m_rcParent.right - m_rcParent.left;
    int h = m_rcParent.bottom - m_rcParent.top;
    if (w <= 0 || h <= 0) {
        RECT clRc = {0};
        GetClientRect(m_hwndParent, &clRc);
        w = clRc.right - clRc.left;
        h = clRc.bottom - clRc.top;
    }
    if (w <= 0) w = 360;
    if (h <= 0) h = 360;

    m_hwndPreview = CreateWindowExW(
        0, PREVIEW_WND_CLASS, L"OmniPeekPreview",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        0, 0, w, h,
        m_hwndParent, NULL, g_hInst, NULL
    );

    if (m_hwndPreview) {
        SetWindowLongPtrW(m_hwndPreview, GWLP_USERDATA, (LONG_PTR)this);
        LogTrace(L"CreateChildWindow succeeded HWND=" + std::to_wstring((UINT_PTR)m_hwndPreview));
        return true;
    }
    LogTrace(L"CreateChildWindow FAILED err=" + std::to_wstring(GetLastError()));
    return false;
}

std::wstring OmniPeekPreviewHandler::GetViewerHtmlPath() {
    WCHAR modulePath[MAX_PATH];
    GetModuleFileNameW(g_hInst, modulePath, MAX_PATH);
    PathRemoveFileSpecW(modulePath);

    std::wstring viewerPath = std::wstring(modulePath) + L"\\viewer\\index.html";
    if (!PathFileExistsW(viewerPath.c_str())) {
        viewerPath = std::wstring(modulePath) + L"\\..\\src\\viewer\\index.html";
    }
    return viewerPath;
}

bool OmniPeekPreviewHandler::InvokeWorkerAction(const std::wstring& action, const std::wstring& inPath, const std::wstring& outPath, int extraParam) {
    LPCWSTR pipeName = OMNIPEEK_PIPE_NAME;
    if (WaitNamedPipeW(pipeName, 20)) {
        HANDLE hPipe = CreateFileW(pipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hPipe != INVALID_HANDLE_VALUE) {
            std::wstringstream req;
            req << action << L"\t" << inPath << L"\t" << outPath;
            if (extraParam > 0) req << L"\t" << extraParam;
            std::wstring reqStr = req.str();
            DWORD written = 0;
            if (WriteFile(hPipe, reqStr.c_str(), (DWORD)(reqStr.length() * sizeof(WCHAR)), &written, NULL)) {
                WCHAR resp[128] = {0};
                DWORD read = 0;
                ReadFile(hPipe, resp, sizeof(resp) - sizeof(WCHAR), &read, NULL);
            }
            CloseHandle(hPipe);
            if (PathFileExistsW(outPath.c_str())) return true;
        }
    }

    WCHAR modulePath[MAX_PATH];
    GetModuleFileNameW(g_hInst, modulePath, MAX_PATH);
    PathRemoveFileSpecW(modulePath);
    std::wstring workerExe = std::wstring(modulePath) + L"\\OmniPeekWorker.exe";

    std::wstringstream cmd;
    cmd << L"\"" << workerExe << L"\" " << action << L" \"" << inPath << L"\" \"" << outPath << L"\"";
    if (extraParam > 0) cmd << L" " << extraParam;

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    std::wstring cmdStr = cmd.str();
    std::vector<WCHAR> cmdLine(cmdStr.begin(), cmdStr.end());
    cmdLine.push_back(L'\0');

    if (CreateProcessW(NULL, cmdLine.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 10000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return PathFileExistsW(outPath.c_str()) == TRUE;
}

std::wstring OmniPeekPreviewHandler::BuildPreviewUrl(const std::wstring& rawFilePath) {
    std::wstring ext;
    size_t dot = rawFilePath.find_last_of(L'.');
    if (dot != std::wstring::npos) {
        ext = OmniPeek::ToLowerW(rawFilePath.substr(dot));
    }

    OmniPeek::FormatDescriptor fmtDesc;
    if (!OmniPeek::PluginRegistry::FindFormatByExtension(ext, fmtDesc, g_hInst)) {
        fmtDesc.ext = ext;
        fmtDesc.pluginId = L"dev-docs";
        fmtDesc.pluginEntry = L"plugins/dev-docs/plugin.js";
        fmtDesc.previewMode = L"code";
        fmtDesc.workerAction = L"text-head";
    }

    OmniPeek::GlobalSettings cfg = OmniPeek::PluginRegistry::LoadGlobalSettings(g_hInst);

    WIN32_FILE_ATTRIBUTE_DATA attr = {0};
    uint64_t fileSize = 0;
    uint64_t lastWrite = 0;
    if (GetFileAttributesExW(rawFilePath.c_str(), GetFileExInfoStandard, &attr)) {
        fileSize = ((uint64_t)attr.nFileSizeHigh << 32) | attr.nFileSizeLow;
        lastWrite = ((uint64_t)attr.ftLastWriteTime.dwHighDateTime << 32) | attr.ftLastWriteTime.dwLowDateTime;
    }

    uint64_t hash = 14695981039346656037ULL;
    for (WCHAR c : rawFilePath) {
        hash ^= (uint64_t)(towlower(c));
        hash *= 1099511628211ULL;
    }
    hash ^= lastWrite;
    hash *= 1099511628211ULL;
    hash ^= fileSize;
    hash *= 1099511628211ULL;

    std::wstring targetFile = rawFilePath;
    bool truncated = false;
    std::wstring lowDir = GetLocalLowDir();
    if (!lowDir.empty() && !fmtDesc.workerAction.empty()) {
        std::wstring cacheDir = lowDir + L"\\cache\\preview_data";
        CreateDirectoryW((lowDir + L"\\cache").c_str(), NULL);
        CreateDirectoryW(cacheDir.c_str(), NULL);

        std::wstringstream ssCache;
        ssCache << cacheDir << L"\\" << std::hex << hash;

        if (fmtDesc.workerAction == L"convert-cad") {
            std::wstring cachedStl = ssCache.str() + L".stl";
            if (PathFileExistsW(cachedStl.c_str()) || InvokeWorkerAction(L"convert-cad", rawFilePath, cachedStl)) {
                targetFile = cachedStl;
            }
        } else if (fmtDesc.workerAction == L"archive-list") {
            std::wstring cachedJson = ssCache.str() + L".json";
            if (PathFileExistsW(cachedJson.c_str()) || InvokeWorkerAction(L"archive-list", rawFilePath, cachedJson)) {
                targetFile = cachedJson;
            }
        } else if (fmtDesc.workerAction == L"text-head") {
            std::wstring cachedTxt = ssCache.str() + L".utf8.txt";
            uint64_t maxBytes = (uint64_t)cfg.maxTextPreviewKB * 1024ULL;
            if (fileSize > maxBytes) truncated = true;
            if (PathFileExistsW(cachedTxt.c_str()) || InvokeWorkerAction(L"text-head", rawFilePath, cachedTxt, cfg.maxTextPreviewKB)) {
                targetFile = cachedTxt;
            }
        }
    }

    std::wstring viewerPath = GetViewerHtmlPath();
    std::wstringstream url;
    url << L"file:///" << UrlEncodePathComponent(viewerPath)
        << L"?file=" << UrlEncodePathComponent(targetFile)
        << L"&orig=" << UrlEncodePathComponent(rawFilePath)
        << L"&ext=" << UrlEncodePathComponent(ext)
        << L"&plugin=" << UrlEncodePathComponent(fmtDesc.pluginId)
        << L"&entry=" << UrlEncodePathComponent(fmtDesc.pluginEntry)
        << L"&mode=" << UrlEncodePathComponent(fmtDesc.previewMode)
        << L"&size=" << fileSize
        << L"&truncated=" << (truncated ? 1 : 0)
        << L"&vol=" << cfg.defaultVolume
        << L"&autoplay=" << (cfg.autoplayMedia ? 1 : 0)
        << L"&theme=" << UrlEncodePathComponent(cfg.defaultTheme);
    return url.str();
}

void OmniPeekPreviewHandler::NavigateToCurrentFile() {
    if (!m_webview || m_filePath.empty()) return;
    std::wstring url = BuildPreviewUrl(m_filePath);
    LogTrace(L"Navigating WebView2 to: " + url);
    m_webview->Navigate(url.c_str());
}

bool OmniPeekPreviewHandler::InitOrReuseWebView2() {
    if (!m_hwndPreview || !IsWindow(m_hwndPreview)) {
        if (!CreateChildWindow()) return false;
    }

    std::wstring lowDir = GetLocalLowDir();
    std::wstring userDataFolder = lowDir + L"\\webview_data";
    CreateDirectoryW(userDataFolder.c_str(), NULL);

    SetEnvironmentVariableW(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS", L"--allow-file-access-from-files --disable-web-security --autoplay-policy=no-user-gesture-required");

    auto onEnvCreated = [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
        if (FAILED(result) || !env) {
            LogTrace(L"onEnvCreated FAILED hr=" + std::to_wstring(result));
            return result;
        }

        auto onControllerCreated = [this](HRESULT res, ICoreWebView2Controller* controller) -> HRESULT {
            if (FAILED(res) || !controller) {
                LogTrace(L"onControllerCreated FAILED hr=" + std::to_wstring(res));
                return res;
            }
            if (!m_hwndPreview || !IsWindow(m_hwndPreview)) {
                controller->Close();
                return E_ABORT;
            }

            m_controller = controller;
            m_controller->AddRef();
            m_controller->get_CoreWebView2(&m_webview);

            RECT clientRc = { 0 };
            GetClientRect(m_hwndPreview, &clientRc);
            if (clientRc.right <= 0 || clientRc.bottom <= 0) {
                clientRc.right = m_rcParent.right - m_rcParent.left;
                clientRc.bottom = m_rcParent.bottom - m_rcParent.top;
            }
            if (clientRc.right <= 0) clientRc.right = 360;
            if (clientRc.bottom <= 0) clientRc.bottom = 360;
            m_controller->put_Bounds(clientRc);
            m_controller->put_IsVisible(TRUE);

            ICoreWebView2Controller2* ctrl2 = nullptr;
            if (SUCCEEDED(m_controller->QueryInterface(IID_ICoreWebView2Controller2, (void**)&ctrl2)) && ctrl2) {
                COREWEBVIEW2_COLOR darkBg = { 255, 18, 22, 31 };
                ctrl2->put_DefaultBackgroundColor(darkBg);
                ctrl2->Release();
            }

            ICoreWebView2Settings* settings = nullptr;
            if (m_webview && SUCCEEDED(m_webview->get_Settings(&settings)) && settings) {
                settings->put_IsStatusBarEnabled(FALSE);
                settings->put_AreDefaultContextMenusEnabled(FALSE);
                settings->put_AreDevToolsEnabled(FALSE);
                settings->Release();
            }

            NavigateToCurrentFile();
            return S_OK;
        };

        return env->CreateCoreWebView2Controller(m_hwndPreview, MakeControllerHandler(onControllerCreated));
    };

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userDataFolder.c_str(), nullptr,
        MakeEnvHandler(onEnvCreated)
    );
    LogTrace(L"CreateCoreWebView2EnvironmentWithOptions hr=" + std::to_wstring(hr));
    return SUCCEEDED(hr);
}

STDMETHODIMP OmniPeekPreviewHandler::DoPreview() {
    LogTrace(L"PreviewHandler::DoPreview called for: " + m_filePath);
    if (m_filePath.empty()) return E_FAIL;

    if (!m_hwndParent || !IsWindow(m_hwndParent)) {
        m_hwndParent = ResolveValidParent(m_hwndParent, m_punkSite, &m_rcParent);
    }
    if (!m_hwndParent || !IsWindow(m_hwndParent)) {
        LogTrace(L"DoPreview: invalid m_hwndParent");
        return E_UNEXPECTED;
    }

    if (!InitOrReuseWebView2()) return E_FAIL;
    return S_OK;
}

STDMETHODIMP OmniPeekPreviewHandler::Unload() {
    LogTrace(L"PreviewHandler::Unload called");
    if (m_controller) {
        m_controller->Close();
        m_controller->Release();
        m_controller = nullptr;
    }
    if (m_webview) {
        m_webview->Release();
        m_webview = nullptr;
    }
    if (m_hwndPreview) {
        if (IsWindow(m_hwndPreview)) {
            DestroyWindow(m_hwndPreview);
        }
        m_hwndPreview = NULL;
    }
    m_hwndParent = NULL;
    ZeroMemory(&m_rcParent, sizeof(m_rcParent));
    return S_OK;
}

STDMETHODIMP OmniPeekPreviewHandler::SetFocus() {
    if (m_controller) {
        m_controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    } else if (m_hwndPreview) {
        ::SetFocus(m_hwndPreview);
    }
    return S_OK;
}

STDMETHODIMP OmniPeekPreviewHandler::QueryFocus(HWND *phwnd) {
    if (!phwnd) return E_POINTER;
    *phwnd = ::GetFocus();
    return S_OK;
}

STDMETHODIMP OmniPeekPreviewHandler::TranslateAccelerator(MSG *pmsg) {
    if (!pmsg) return E_POINTER;
    return S_FALSE;
}

STDMETHODIMP OmniPeekPreviewHandler::GetWindow(HWND *phwnd) {
    if (!phwnd) return E_POINTER;
    *phwnd = m_hwndPreview;
    return S_OK;
}

STDMETHODIMP OmniPeekPreviewHandler::ContextSensitiveHelp(BOOL /*fEnterMode*/) {
    return E_NOTIMPL;
}

STDMETHODIMP OmniPeekPreviewHandler::SetBackgroundColor(COLORREF /*color*/) {
    return S_OK;
}

STDMETHODIMP OmniPeekPreviewHandler::SetFont(const LOGFONTW* /*plf*/) {
    return S_OK;
}

STDMETHODIMP OmniPeekPreviewHandler::SetTextColor(COLORREF /*color*/) {
    return S_OK;
}
