#pragma once
#include <windows.h>
#include <string>
#include <shlobj.h>
#include <knownfolders.h>
#include <cstdio>

inline std::wstring GetLocalLowDir() {
    PWSTR path = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppDataLow, 0, NULL, &path)) && path) {
        std::wstring dir = std::wstring(path) + L"\\OmniPeek";
        CoTaskMemFree(path);
        CreateDirectoryW(dir.c_str(), NULL);
        return dir;
    }
    WCHAR userProfile[MAX_PATH];
    if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH)) {
        std::wstring dir = std::wstring(userProfile) + L"\\AppData\\LocalLow\\OmniPeek";
        CreateDirectoryW(dir.c_str(), NULL);
        return dir;
    }
    return L"";
}

inline void LogTrace(const std::wstring& msg) {
    std::wstring dir = GetLocalLowDir();
    if (!dir.empty()) {
        std::wstring logFile = dir + L"\\omnipeek_shell.log";
        FILE* f = _wfopen(logFile.c_str(), L"a+,ccs=UTF-8");
        if (f) {
            SYSTEMTIME st;
            GetLocalTime(&st);
            fwprintf(f, L"[%02d:%02d:%02d.%03d] [PID %lu] %ls\n",
                     st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                     GetCurrentProcessId(), msg.c_str());
            fclose(f);
        }
    }
}
