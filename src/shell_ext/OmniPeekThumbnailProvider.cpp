#include "OmniPeekThumbnailProvider.h"
#include "../common/Guids.h"
#include "Logger.h"
#include <shlwapi.h>
#include <shlobj.h>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <vector>

extern HINSTANCE g_hInst;
extern long g_serverLocks;

static std::wstring DumpThumbStreamToTemp(IStream* pStream, LPCWSTR origName) {
    std::wstring ext = L".stl";
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

OmniPeekThumbnailProvider::OmniPeekThumbnailProvider() : m_refCount(1) {
    InterlockedIncrement(&g_serverLocks);
}

OmniPeekThumbnailProvider::~OmniPeekThumbnailProvider() {
    InterlockedDecrement(&g_serverLocks);
}

STDMETHODIMP OmniPeekThumbnailProvider::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == __uuidof(IThumbnailProvider)) {
        *ppv = static_cast<IThumbnailProvider*>(this);
    } else if (riid == __uuidof(IInitializeWithFile)) {
        *ppv = static_cast<IInitializeWithFile*>(this);
    } else if (riid == __uuidof(IInitializeWithItem)) {
        *ppv = static_cast<IInitializeWithItem*>(this);
    } else if (riid == __uuidof(IInitializeWithStream)) {
        *ppv = static_cast<IInitializeWithStream*>(this);
    } else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) OmniPeekThumbnailProvider::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

STDMETHODIMP_(ULONG) OmniPeekThumbnailProvider::Release() {
    ULONG count = InterlockedDecrement(&m_refCount);
    if (count == 0) delete this;
    return count;
}

STDMETHODIMP OmniPeekThumbnailProvider::Initialize(LPCWSTR pszFilePath, DWORD /*grfMode*/) {
    if (!pszFilePath) return E_INVALIDARG;
    m_filePath = pszFilePath;
    return S_OK;
}

STDMETHODIMP OmniPeekThumbnailProvider::Initialize(IShellItem *psi, DWORD /*grfMode*/) {
    if (!psi) return E_INVALIDARG;
    LPWSTR pszPath = nullptr;
    HRESULT hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
    if (SUCCEEDED(hr) && pszPath) {
        m_filePath = pszPath;
        CoTaskMemFree(pszPath);
        return S_OK;
    }
    return hr;
}

STDMETHODIMP OmniPeekThumbnailProvider::Initialize(IStream *pStream, DWORD /*grfMode*/) {
    if (!pStream) return E_INVALIDARG;
    STATSTG stat = {0};
    if (SUCCEEDED(pStream->Stat(&stat, STATFLAG_DEFAULT)) && stat.pwcsName) {
        if (PathFileExistsW(stat.pwcsName)) {
            m_filePath = stat.pwcsName;
            CoTaskMemFree(stat.pwcsName);
            return S_OK;
        } else {
            m_filePath = DumpThumbStreamToTemp(pStream, stat.pwcsName);
            CoTaskMemFree(stat.pwcsName);
            return m_filePath.empty() ? E_FAIL : S_OK;
        }
    }
    m_filePath = DumpThumbStreamToTemp(pStream, L"model.stl");
    return m_filePath.empty() ? E_FAIL : S_OK;
}

std::wstring OmniPeekThumbnailProvider::GetCacheDirectory() {
    WCHAR appData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appData))) {
        std::wstring dir = std::wstring(appData) + L"\\OmniPeek\\cache\\thumbnails";
        CreateDirectoryW((std::wstring(appData) + L"\\OmniPeek").c_str(), NULL);
        CreateDirectoryW((std::wstring(appData) + L"\\OmniPeek\\cache").c_str(), NULL);
        CreateDirectoryW(dir.c_str(), NULL);
        return dir;
    }
    return L"";
}

std::wstring OmniPeekThumbnailProvider::GetThumbnailCachePath(const std::wstring& filePath, UINT cx) {
    std::wstring dir = GetCacheDirectory();
    if (dir.empty()) return L"";

    WIN32_FILE_ATTRIBUTE_DATA attr;
    uint64_t lastWrite = 0;
    uint64_t fileSize = 0;
    if (GetFileAttributesExW(filePath.c_str(), GetFileExInfoStandard, &attr)) {
        lastWrite = ((uint64_t)attr.ftLastWriteTime.dwHighDateTime << 32) | attr.ftLastWriteTime.dwLowDateTime;
        fileSize = ((uint64_t)attr.nFileSizeHigh << 32) | attr.nFileSizeLow;
    }

    uint64_t hash = 14695981039346656037ULL;
    for (WCHAR c : filePath) {
        hash ^= (uint64_t)(towlower(c));
        hash *= 1099511628211ULL;
    }
    hash ^= lastWrite;
    hash *= 1099511628211ULL;
    hash ^= fileSize;
    hash *= 1099511628211ULL;

    std::wstringstream ss;
    ss << dir << L"\\" << std::hex << hash << L"_" << cx << L".bmp";
    return ss.str();
}

bool OmniPeekThumbnailProvider::GenerateThumbnail(const std::wstring& filePath, const std::wstring& outBmpPath, UINT cx) {
    // 1. Fast path via Named Pipe IPC if daemon worker is running
    LPCWSTR pipeName = OMNIPEEK_PIPE_NAME;
    if (WaitNamedPipeW(pipeName, 20)) {
        HANDLE hPipe = CreateFileW(pipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hPipe != INVALID_HANDLE_VALUE) {
            std::wstringstream req;
            req << L"thumbnail\t" << filePath << L"\t" << outBmpPath << L"\t" << cx;
            std::wstring reqStr = req.str();
            DWORD written = 0;
            if (WriteFile(hPipe, reqStr.c_str(), (DWORD)(reqStr.length() * sizeof(WCHAR)), &written, NULL)) {
                WCHAR resp[128] = {0};
                DWORD read = 0;
                ReadFile(hPipe, resp, sizeof(resp) - sizeof(WCHAR), &read, NULL);
            }
            CloseHandle(hPipe);
            if (PathFileExistsW(outBmpPath.c_str())) {
                return true;
            }
        }
    }

    // 2. Out-of-process worker spawn fallback
    WCHAR modulePath[MAX_PATH];
    GetModuleFileNameW(g_hInst, modulePath, MAX_PATH);
    PathRemoveFileSpecW(modulePath);

    std::wstring workerExe = std::wstring(modulePath) + L"\\OmniPeekWorker.exe";
    std::wstringstream cmd;
    cmd << L"\"" << workerExe << L"\" thumbnail \"" << filePath << L"\" \"" << outBmpPath << L"\" " << cx;

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::wstring cmdStr = cmd.str();
    std::vector<WCHAR> cmdLine(cmdStr.begin(), cmdStr.end());
    cmdLine.push_back(L'\0');

    if (CreateProcessW(NULL, cmdLine.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 15000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    return (PathFileExistsW(outBmpPath.c_str()) == TRUE);
}

STDMETHODIMP OmniPeekThumbnailProvider::GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha) {
    if (!phbmp || !pdwAlpha) return E_POINTER;
    if (m_filePath.empty()) return E_UNEXPECTED;

    std::wstring cachePath = GetThumbnailCachePath(m_filePath, cx);
    if (cachePath.empty()) return E_FAIL;

    if (!PathFileExistsW(cachePath.c_str())) {
        if (!GenerateThumbnail(m_filePath, cachePath, cx)) {
            return E_FAIL;
        }
    }

    HBITMAP hBmp = (HBITMAP)LoadImageW(NULL, cachePath.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    if (!hBmp) return E_FAIL;

    *phbmp = hBmp;
    *pdwAlpha = WTSAT_RGB;
    return S_OK;
}
