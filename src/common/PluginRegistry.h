#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cwctype>

namespace OmniPeek {

struct FormatDescriptor {
    std::wstring ext;               // e.g. L".step", L".mp4", L".md"
    std::wstring label;             // e.g. L".step (STEP 工业三维零件)"
    std::wstring category;          // e.g. L"3D 与工业 CAD"
    std::wstring pluginId;          // e.g. L"cad-3d"
    std::wstring pluginName;        // e.g. L"3D 与工业 CAD 引擎"
    std::wstring pluginEntry;       // e.g. L"plugins/cad-3d/plugin.js"
    std::wstring previewMode;       // e.g. L"cad-3d", L"media", L"dev-docs", L"archive-font"
    std::wstring thumbnailMode;     // e.g. L"3d-raster", L"media-card", L"doc-card", L"archive-card", L"font-card"
    std::wstring workerAction;      // e.g. L"", L"convert-cad", L"archive-list", L"text-head"
    bool defaultThumbEnabled = true;
};

struct PluginManifest {
    std::wstring id;
    std::wstring name;
    std::wstring version;
    std::wstring description;
    std::wstring entryScript;       // Relative to viewer or root, e.g. "plugins/cad-3d/plugin.js"
    std::wstring previewMode;
    std::wstring thumbnailMode;
    std::vector<FormatDescriptor> formats;
};

struct GlobalSettings {
    int defaultVolume = 60;         // 0 - 100
    bool autoplayMedia = true;
    int maxTextPreviewKB = 512;     // Safe truncation threshold for large text/code/log files
    std::wstring defaultTheme = L"dark";
};

inline std::wstring ToLowerW(std::wstring s) {
    for (auto& c : s) c = (WCHAR)towlower(c);
    return s;
}

inline std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int sz = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), NULL, 0);
    if (sz <= 0) return L"";
    std::wstring res(sz, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &res[0], sz);
    return res;
}

inline std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int sz = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), NULL, 0, NULL, NULL);
    if (sz <= 0) return "";
    std::string res(sz, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &res[0], sz, NULL, NULL);
    return res;
}

// Lightweight JSON string field extractor for plugin.json manifests
inline std::string ExtractJsonString(const std::string& json, const std::string& key, size_t startPos = 0, size_t endPos = std::string::npos) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern, startPos);
    if (pos == std::string::npos || (endPos != std::string::npos && pos >= endPos)) return "";
    pos = json.find(':', pos + pattern.size());
    if (pos == std::string::npos || (endPos != std::string::npos && pos >= endPos)) return "";
    size_t q1 = json.find('"', pos + 1);
    if (q1 == std::string::npos || (endPos != std::string::npos && q1 >= endPos)) return "";
    size_t q2 = q1 + 1;
    while (q2 < json.size() && (endPos == std::string::npos || q2 < endPos)) {
        if (json[q2] == '"' && json[q2 - 1] != '\\') break;
        q2++;
    }
    if (q2 >= json.size()) return "";
    return json.substr(q1 + 1, q2 - q1 - 1);
}

inline bool ExtractJsonBool(const std::string& json, const std::string& key, bool defVal, size_t startPos = 0, size_t endPos = std::string::npos) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern, startPos);
    if (pos == std::string::npos || (endPos != std::string::npos && pos >= endPos)) return defVal;
    pos = json.find(':', pos + pattern.size());
    if (pos == std::string::npos || (endPos != std::string::npos && pos >= endPos)) return defVal;
    size_t valPos = json.find_first_not_of(" \t\r\n", pos + 1);
    if (valPos == std::string::npos || (endPos != std::string::npos && valPos >= endPos)) return defVal;
    if (json.compare(valPos, 4, "true") == 0) return true;
    if (json.compare(valPos, 5, "false") == 0) return false;
    return defVal;
}

class PluginRegistry {
public:
    static std::wstring GetModuleDir(HINSTANCE hInst = NULL) {
        WCHAR path[MAX_PATH] = {0};
        GetModuleFileNameW(hInst, path, MAX_PATH);
        PathRemoveFileSpecW(path);
        return path;
    }

    static std::wstring FindPluginsDir(HINSTANCE hInst = NULL) {
        std::wstring base = GetModuleDir(hInst);
        std::wstring p1 = base + L"\\plugins";
        if (PathFileExistsW(p1.c_str())) return p1;
        std::wstring p2 = base + L"\\..\\plugins";
        if (PathFileExistsW(p2.c_str())) return p2;
        return p1;
    }

    static GlobalSettings LoadGlobalSettings(HINSTANCE hInst = NULL) {
        GlobalSettings cfg;
        std::wstring iniPath = GetModuleDir(hInst) + L"\\OmniPeek.ini";
        if (!PathFileExistsW(iniPath.c_str())) {
            WCHAR appData[MAX_PATH] = {0};
            if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appData))) {
                std::wstring altIni = std::wstring(appData) + L"\\OmniPeek\\OmniPeek.ini";
                if (PathFileExistsW(altIni.c_str())) iniPath = altIni;
            }
        }
        cfg.defaultVolume = (int)GetPrivateProfileIntW(L"Media", L"Volume", 60, iniPath.c_str());
        if (cfg.defaultVolume < 0) cfg.defaultVolume = 0;
        if (cfg.defaultVolume > 100) cfg.defaultVolume = 100;
        cfg.autoplayMedia = GetPrivateProfileIntW(L"Media", L"Autoplay", 1, iniPath.c_str()) != 0;
        cfg.maxTextPreviewKB = (int)GetPrivateProfileIntW(L"Docs", L"MaxTextPreviewKB", 512, iniPath.c_str());
        WCHAR themeBuf[64] = {0};
        GetPrivateProfileStringW(L"General", L"Theme", L"dark", themeBuf, 64, iniPath.c_str());
        cfg.defaultTheme = themeBuf;
        return cfg;
    }

    static void SaveGlobalSettings(const GlobalSettings& cfg, HINSTANCE hInst = NULL) {
        std::wstring iniPath = GetModuleDir(hInst) + L"\\OmniPeek.ini";
        WritePrivateProfileStringW(L"Media", L"Volume", std::to_wstring(cfg.defaultVolume).c_str(), iniPath.c_str());
        WritePrivateProfileStringW(L"Media", L"Autoplay", cfg.autoplayMedia ? L"1" : L"0", iniPath.c_str());
        WritePrivateProfileStringW(L"Docs", L"MaxTextPreviewKB", std::to_wstring(cfg.maxTextPreviewKB).c_str(), iniPath.c_str());
        WritePrivateProfileStringW(L"General", L"Theme", cfg.defaultTheme.c_str(), iniPath.c_str());
    }

    static bool ParsePluginJsonFile(const std::wstring& jsonPath, const std::wstring& folderName, PluginManifest& outPlugin) {
        FILE* f = _wfopen(jsonPath.c_str(), L"rb");
        if (!f) return false;
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (len <= 0 || len > 1024 * 1024) {
            fclose(f);
            return false;
        }
        std::string content(len, '\0');
        fread(&content[0], 1, len, f);
        fclose(f);

        outPlugin.id = Utf8ToWide(ExtractJsonString(content, "id"));
        if (outPlugin.id.empty()) outPlugin.id = folderName;
        outPlugin.name = Utf8ToWide(ExtractJsonString(content, "name"));
        if (outPlugin.name.empty()) outPlugin.name = outPlugin.id;
        outPlugin.version = Utf8ToWide(ExtractJsonString(content, "version"));
        outPlugin.description = Utf8ToWide(ExtractJsonString(content, "description"));
        std::wstring entryFile = Utf8ToWide(ExtractJsonString(content, "entry"));
        if (entryFile.empty()) entryFile = L"plugin.js";
        outPlugin.entryScript = L"plugins/" + folderName + L"/" + entryFile;
        outPlugin.previewMode = Utf8ToWide(ExtractJsonString(content, "previewMode"));
        outPlugin.thumbnailMode = Utf8ToWide(ExtractJsonString(content, "thumbnailMode"));

        // Parse "formats": [ { ... }, ... ]
        size_t fmtArrPos = content.find("\"formats\"");
        if (fmtArrPos != std::string::npos) {
            size_t bracketStart = content.find('[', fmtArrPos);
            size_t bracketEnd = content.rfind(']');
            if (bracketStart != std::string::npos && bracketEnd != std::string::npos && bracketEnd > bracketStart) {
                size_t cur = bracketStart;
                while (cur < bracketEnd) {
                    size_t objStart = content.find('{', cur);
                    if (objStart == std::string::npos || objStart >= bracketEnd) break;
                    size_t objEnd = content.find('}', objStart);
                    if (objEnd == std::string::npos || objEnd > bracketEnd) break;

                    FormatDescriptor fd;
                    fd.ext = ToLowerW(Utf8ToWide(ExtractJsonString(content, "ext", objStart, objEnd)));
                    if (!fd.ext.empty()) {
                        if (fd.ext[0] != L'.') fd.ext = L"." + fd.ext;
                        fd.label = Utf8ToWide(ExtractJsonString(content, "label", objStart, objEnd));
                        if (fd.label.empty()) fd.label = fd.ext;
                        fd.category = Utf8ToWide(ExtractJsonString(content, "category", objStart, objEnd));
                        if (fd.category.empty()) fd.category = outPlugin.name;
                        fd.pluginId = outPlugin.id;
                        fd.pluginName = outPlugin.name;
                        fd.pluginEntry = outPlugin.entryScript;
                        std::wstring itemMode = Utf8ToWide(ExtractJsonString(content, "previewMode", objStart, objEnd));
                        fd.previewMode = itemMode.empty() ? outPlugin.previewMode : itemMode;
                        std::wstring itemThumb = Utf8ToWide(ExtractJsonString(content, "thumbnailMode", objStart, objEnd));
                        fd.thumbnailMode = itemThumb.empty() ? outPlugin.thumbnailMode : itemThumb;
                        fd.workerAction = Utf8ToWide(ExtractJsonString(content, "workerAction", objStart, objEnd));
                        fd.defaultThumbEnabled = ExtractJsonBool(content, "thumbnail", true, objStart, objEnd);
                        outPlugin.formats.push_back(fd);
                    }
                    cur = objEnd + 1;
                }
            }
        }
        return !outPlugin.formats.empty();
    }

    // Built-in fallback registry if plugins/ directory is not found
    static std::vector<PluginManifest> GetBuiltinFallbackPlugins() {
        std::vector<PluginManifest> list;

        // 1. CAD & 3D Plugin
        PluginManifest p3d;
        p3d.id = L"cad-3d";
        p3d.name = L"3D 与工业 CAD 引擎";
        p3d.version = L"1.0.0";
        p3d.entryScript = L"plugins/cad-3d/plugin.js";
        p3d.previewMode = L"cad-3d";
        p3d.thumbnailMode = L"3d-raster";
        auto add3d = [&](LPCWSTR ext, LPCWSTR label, LPCWSTR workerAct) {
            FormatDescriptor fd{ ext, label, L"1. 3D 与工业 CAD 格式", p3d.id, p3d.name, p3d.entryScript, p3d.previewMode, L"3d-raster", workerAct, true };
            p3d.formats.push_back(fd);
        };
        add3d(L".step", L".step (STEP 工业三维零件)", L"convert-cad");
        add3d(L".stp",  L".stp (STEP 工业装配模型)", L"convert-cad");
        add3d(L".iges", L".iges (IGES 曲面与实体)", L"convert-cad");
        add3d(L".igs",  L".igs (IGES 工业交换模型)", L"convert-cad");
        add3d(L".brep", L".brep (OpenCASCADE 边界表示)", L"convert-cad");
        add3d(L".brp",  L".brp (B-Rep 拓扑几何)", L"convert-cad");
        add3d(L".dxf",  L".dxf (AutoCAD 二维/三维工程图)", L"");
        add3d(L".stl",  L".stl (3D 打印三角网格)", L"");
        add3d(L".obj",  L".obj (Wavefront 三维网格)", L"");
        add3d(L".glb",  L".glb (二进制 glTF 三维资产)", L"");
        add3d(L".gltf", L".gltf (WebGL 标准三维场景)", L"");
        add3d(L".3mf",  L".3mf (新一代增材制造格式)", L"");
        add3d(L".fbx",  L".fbx (Autodesk 三维交换格式)", L"");
        add3d(L".ply",  L".ply (Stanford 三维扫描点云/网格)", L"");
        add3d(L".pcd",  L".pcd (PCL 激光雷达点云)", L"");
        add3d(L".gcode",L".gcode (CNC / 3D 打印加工刀轨)", L"");
        add3d(L".dae",  L".dae (Collada 三维模型)", L"");
        add3d(L".3ds",  L".3ds (3D Studio 经典网格)", L"");
        list.push_back(p3d);

        // 2. Media Player Plugin
        PluginManifest pMedia;
        pMedia.id = L"media-player";
        pMedia.name = L"音视频与多媒体工作室";
        pMedia.version = L"1.0.0";
        pMedia.entryScript = L"plugins/media-player/plugin.js";
        pMedia.previewMode = L"media";
        pMedia.thumbnailMode = L"media-card";
        auto addMedia = [&](LPCWSTR ext, LPCWSTR label, LPCWSTR mode) {
            FormatDescriptor fd{ ext, label, L"2. 音视频与多媒体格式", pMedia.id, pMedia.name, pMedia.entryScript, mode, L"media-card", L"", true };
            pMedia.formats.push_back(fd);
        };
        addMedia(L".mp4",  L".mp4 (MPEG-4 高清视频)", L"video");
        addMedia(L".mkv",  L".mkv (Matroska 多轨封装视频)", L"video");
        addMedia(L".webm", L".webm (WebM 开放媒体视频)", L"video");
        addMedia(L".mov",  L".mov (QuickTime 视频)", L"video");
        addMedia(L".avi",  L".avi (Audio Video Interleave)", L"video");
        addMedia(L".m4v",  L".m4v (iTunes 视频格式)", L"video");
        addMedia(L".3gp",  L".3gp (移动设备视频)", L"video");
        addMedia(L".ogv",  L".ogv (Ogg Theora 视频)", L"video");
        addMedia(L".mp3",  L".mp3 (MPEG Audio Layer III)", L"audio");
        addMedia(L".flac", L".flac (无损音频编码格式)", L"audio");
        addMedia(L".wav",  L".wav (Waveform 无损音频)", L"audio");
        addMedia(L".aac",  L".aac (Advanced Audio Coding)", L"audio");
        addMedia(L".ogg",  L".ogg (Ogg Vorbis 音频)", L"audio");
        addMedia(L".m4a",  L".m4a (MPEG-4 音频流)", L"audio");
        addMedia(L".opus", L".opus (Opus 低延迟高音质音频)", L"audio");
        addMedia(L".svg",  L".svg (可缩放矢量图形)", L"image");
        addMedia(L".webp", L".webp (WebP 现代高清图像)", L"image");
        addMedia(L".gif",  L".gif (GIF 动态图像)", L"image");
        list.push_back(pMedia);

        // 3. Developer & Docs Plugin
        PluginManifest pDocs;
        pDocs.id = L"dev-docs";
        pDocs.name = L"开发者与文档表格引擎";
        pDocs.version = L"1.0.0";
        pDocs.entryScript = L"plugins/dev-docs/plugin.js";
        pDocs.previewMode = L"dev-docs";
        pDocs.thumbnailMode = L"doc-card";
        auto addDoc = [&](LPCWSTR ext, LPCWSTR label, LPCWSTR mode, LPCWSTR workerAct) {
            FormatDescriptor fd{ ext, label, L"3. 开发者与文档表格格式", pDocs.id, pDocs.name, pDocs.entryScript, mode, L"doc-card", workerAct, true };
            pDocs.formats.push_back(fd);
        };
        addDoc(L".md",       L".md (Markdown 文档渲染与大纲)", L"markdown", L"text-head");
        addDoc(L".markdown", L".markdown (Markdown 文档)", L"markdown", L"text-head");
        addDoc(L".json",     L".json (JSON 交互折叠树与高亮)", L"json", L"text-head");
        addDoc(L".yaml",     L".yaml (YAML 配置清单)", L"code", L"text-head");
        addDoc(L".yml",      L".yml (YML 配置文件)", L"code", L"text-head");
        addDoc(L".toml",     L".toml (TOML 配置文件)", L"code", L"text-head");
        addDoc(L".xml",      L".xml (XML 结构化文档)", L"code", L"text-head");
        addDoc(L".ini",      L".ini (Windows 初始化配置)", L"code", L"text-head");
        addDoc(L".csv",      L".csv (逗号分隔数据表格)", L"table-csv", L"text-head");
        addDoc(L".tsv",      L".tsv (制表符分隔数据表格)", L"table-tsv", L"text-head");
        addDoc(L".xlsx",     L".xlsx (Excel 工作簿表格)", L"table-xlsx", L"");
        addDoc(L".pdf",      L".pdf (PDF 便携式文档格式)", L"pdf", L"");
        addDoc(L".py",       L".py (Python 源代码)", L"code", L"text-head");
        addDoc(L".cpp",      L".cpp (C++ 源代码)", L"code", L"text-head");
        addDoc(L".c",        L".c (C 语言源代码)", L"code", L"text-head");
        addDoc(L".h",        L".h (C/C++ 头文件)", L"code", L"text-head");
        addDoc(L".js",       L".js (JavaScript 脚本)", L"code", L"text-head");
        addDoc(L".ts",       L".ts (TypeScript 源代码)", L"code", L"text-head");
        addDoc(L".rs",       L".rs (Rust 源代码)", L"code", L"text-head");
        addDoc(L".go",       L".go (Go 语言源代码)", L"code", L"text-head");
        addDoc(L".java",     L".java (Java 源代码)", L"code", L"text-head");
        addDoc(L".cs",       L".cs (C# 源代码)", L"code", L"text-head");
        addDoc(L".sql",      L".sql (SQL 数据库脚本)", L"code", L"text-head");
        addDoc(L".ps1",      L".ps1 (PowerShell 脚本)", L"code", L"text-head");
        addDoc(L".sh",       L".sh (Shell 脚本)", L"code", L"text-head");
        addDoc(L".log",      L".log (系统与应用日志文件)", L"code", L"text-head");
        addDoc(L".txt",      L".txt (纯文本文档)", L"code", L"text-head");
        list.push_back(pDocs);

        // 4. Archive & Font Plugin
        PluginManifest pArch;
        pArch.id = L"archive-font";
        pArch.name = L"压缩包与字体特殊格式引擎";
        pArch.version = L"1.0.0";
        pArch.entryScript = L"plugins/archive-font/plugin.js";
        pArch.previewMode = L"archive-font";
        pArch.thumbnailMode = L"archive-card";
        auto addArch = [&](LPCWSTR ext, LPCWSTR label, LPCWSTR mode, LPCWSTR thumbMode, LPCWSTR workerAct) {
            FormatDescriptor fd{ ext, label, L"4. 压缩包与特殊格式", pArch.id, pArch.name, pArch.entryScript, mode, thumbMode, workerAct, true };
            pArch.formats.push_back(fd);
        };
        addArch(L".zip",   L".zip (ZIP 压缩归档目录树)", L"archive", L"archive-card", L"archive-list");
        addArch(L".7z",    L".7z (7-Zip 高压缩比归档目录树)", L"archive", L"archive-card", L"archive-list");
        addArch(L".rar",   L".rar (RAR 压缩归档目录树)", L"archive", L"archive-card", L"archive-list");
        addArch(L".tar",   L".tar (TAR 归档包目录树)", L"archive", L"archive-card", L"archive-list");
        addArch(L".gz",    L".gz (Gzip 压缩包预览)", L"archive", L"archive-card", L"archive-list");
        addArch(L".ttf",   L".ttf (TrueType 字体瀑布流与字形预览)", L"font", L"font-card", L"");
        addArch(L".otf",   L".otf (OpenType 字体瀑布流与字形预览)", L"font", L"font-card", L"");
        addArch(L".woff",  L".woff (Web Open Font 字体预览)", L"font", L"font-card", L"");
        addArch(L".woff2", L".woff2 (WOFF2 现代网页字体预览)", L"font", L"font-card", L"");
        list.push_back(pArch);

        return list;
    }

    // Dynamically load all plugins from plugins/*/plugin.json, merging with built-in defaults
    static std::vector<PluginManifest> LoadAllPlugins(HINSTANCE hInst = NULL) {
        std::vector<PluginManifest> plugins;
        std::wstring pluginsDir = FindPluginsDir(hInst);

        if (PathFileExistsW(pluginsDir.c_str())) {
            std::wstring search = pluginsDir + L"\\*";
            WIN32_FIND_DATAW fd;
            HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                        wcscmp(fd.cFileName, L".") != 0 &&
                        wcscmp(fd.cFileName, L"..") != 0) {
                        std::wstring manifestPath = pluginsDir + L"\\" + fd.cFileName + L"\\plugin.json";
                        if (PathFileExistsW(manifestPath.c_str())) {
                            PluginManifest pm;
                            if (ParsePluginJsonFile(manifestPath, fd.cFileName, pm)) {
                                plugins.push_back(pm);
                            }
                        }
                    }
                } while (FindNextFileW(hFind, &fd));
                FindClose(hFind);
            }
        }

        std::sort(plugins.begin(), plugins.end(), [](const PluginManifest& a, const PluginManifest& b) {
            std::wstring ca = a.formats.empty() ? a.name : a.formats[0].category;
            std::wstring cb = b.formats.empty() ? b.name : b.formats[0].category;
            return ca < cb;
        });

        if (plugins.empty()) {
            return GetBuiltinFallbackPlugins();
        }
        return plugins;
    }

    static std::vector<FormatDescriptor> GetAllFormats(HINSTANCE hInst = NULL) {
        std::vector<FormatDescriptor> all;
        auto plugins = LoadAllPlugins(hInst);
        for (const auto& p : plugins) {
            for (const auto& f : p.formats) {
                all.push_back(f);
            }
        }
        return all;
    }

    static bool FindFormatByExtension(const std::wstring& rawExt, FormatDescriptor& outDesc, HINSTANCE hInst = NULL) {
        std::wstring ext = ToLowerW(rawExt);
        if (!ext.empty() && ext[0] != L'.') ext = L"." + ext;
        auto plugins = LoadAllPlugins(hInst);
        for (const auto& p : plugins) {
            for (const auto& f : p.formats) {
                if (f.ext == ext) {
                    outDesc = f;
                    return true;
                }
            }
        }
        // Fallback check in builtin list in case a custom plugin.json omitted a builtin format
        auto builtins = GetBuiltinFallbackPlugins();
        for (const auto& p : builtins) {
            for (const auto& f : p.formats) {
                if (f.ext == ext) {
                    outDesc = f;
                    return true;
                }
            }
        }
        return false;
    }
};

} // namespace OmniPeek
