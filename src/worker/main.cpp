#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include "../common/Guids.h"
#include "../common/PluginRegistry.h"
#include "cad_loaders.h"

struct ArchiveEntryInfo {
    std::string name;
    uint64_t uncompressedSize = 0;
    uint64_t compressedSize = 0;
    bool isDir = false;
};

static std::string JsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (int)c);
                    out += buf;
                } else {
                    out += (char)c;
                }
        }
    }
    return out;
}

static bool IsValidUtf8(const char* data, size_t len) {
    size_t i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)data[i];
        if (c < 0x80) {
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= len || ((unsigned char)data[i + 1] & 0xC0) != 0x80) return false;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= len || ((unsigned char)data[i + 1] & 0xC0) != 0x80 || ((unsigned char)data[i + 2] & 0xC0) != 0x80) return false;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= len || ((unsigned char)data[i + 1] & 0xC0) != 0x80 || ((unsigned char)data[i + 2] & 0xC0) != 0x80 || ((unsigned char)data[i + 3] & 0xC0) != 0x80) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

static std::string NormalizeFilenameToUtf8(const std::string& raw) {
    if (raw.empty()) return "";
    if (IsValidUtf8(raw.data(), raw.size())) return raw;
    int wlen = MultiByteToWideChar(CP_ACP, 0, raw.data(), (int)raw.size(), NULL, 0);
    if (wlen <= 0) return raw;
    std::wstring wstr(wlen, 0);
    MultiByteToWideChar(CP_ACP, 0, raw.data(), (int)raw.size(), &wstr[0], wlen);
    return OmniPeek::WideToUtf8(wstr);
}

// Fast zero-extraction archive header parser supporting ZIP, TAR, GZ, RAR, and 7Z
bool ParseArchiveEntries(const std::wstring& archivePath, std::vector<ArchiveEntryInfo>& entries, uint64_t& outArchiveSize, std::string& outFormat) {
    entries.clear();
    outArchiveSize = 0;

    std::wstring ext;
    size_t dot = archivePath.find_last_of(L'.');
    if (dot != std::wstring::npos) ext = OmniPeek::ToLowerW(archivePath.substr(dot));

    FILE* f = _wfopen(archivePath.c_str(), L"rb");
    if (!f) return false;

    _fseeki64(f, 0, SEEK_END);
    int64_t fileSize = _ftelli64(f);
    _fseeki64(f, 0, SEEK_SET);
    outArchiveSize = (fileSize > 0) ? (uint64_t)fileSize : 0;

    unsigned char magic[8] = {0};
    size_t magicRead = fread(magic, 1, sizeof(magic), f);
    _fseeki64(f, 0, SEEK_SET);

    // 1. ZIP format (PK\x03\x04 or .zip)
    if ((magicRead >= 4 && magic[0] == 'P' && magic[1] == 'K') || ext == L".zip") {
        outFormat = "ZIP";
        int64_t searchLen = std::min<int64_t>(fileSize, 65557);
        if (searchLen >= 22) {
            std::vector<unsigned char> tail((size_t)searchLen);
            _fseeki64(f, fileSize - searchLen, SEEK_SET);
            fread(tail.data(), 1, (size_t)searchLen, f);

            int64_t eocdPos = -1;
            for (int64_t i = searchLen - 22; i >= 0; --i) {
                if (tail[(size_t)i] == 0x50 && tail[(size_t)i + 1] == 0x4B &&
                    tail[(size_t)i + 2] == 0x05 && tail[(size_t)i + 3] == 0x06) {
                    eocdPos = i;
                    break;
                }
            }

            if (eocdPos >= 0) {
                uint16_t totalEntries = *(uint16_t*)(&tail[(size_t)eocdPos + 10]);
                uint32_t cdSize       = *(uint32_t*)(&tail[(size_t)eocdPos + 12]);
                uint32_t cdOffset     = *(uint32_t*)(&tail[(size_t)eocdPos + 16]);

                if ((int64_t)cdOffset + (int64_t)cdSize <= fileSize && cdSize <= 32 * 1024 * 1024) {
                    std::vector<unsigned char> cd(cdSize);
                    _fseeki64(f, cdOffset, SEEK_SET);
                    fread(cd.data(), 1, cdSize, f);

                    size_t pos = 0;
                    uint16_t count = 0;
                    while (pos + 46 <= cd.size() && count < totalEntries && entries.size() < 5000) {
                        if (cd[pos] != 0x50 || cd[pos + 1] != 0x4B || cd[pos + 2] != 0x01 || cd[pos + 3] != 0x02) break;
                        uint32_t compSize   = *(uint32_t*)(&cd[pos + 20]);
                        uint32_t uncompSize = *(uint32_t*)(&cd[pos + 24]);
                        uint16_t nameLen    = *(uint16_t*)(&cd[pos + 28]);
                        uint16_t extraLen   = *(uint16_t*)(&cd[pos + 30]);
                        uint16_t commentLen = *(uint16_t*)(&cd[pos + 32]);
                        uint32_t extAttr    = *(uint32_t*)(&cd[pos + 38]);

                        if (pos + 46 + nameLen > cd.size()) break;
                        std::string rawName((const char*)&cd[pos + 46], nameLen);
                        for (auto& ch : rawName) if (ch == '\\') ch = '/';

                        ArchiveEntryInfo item;
                        item.name = NormalizeFilenameToUtf8(rawName);
                        item.compressedSize = compSize;
                        item.uncompressedSize = uncompSize;
                        item.isDir = (!item.name.empty() && item.name.back() == '/') || ((extAttr & 0x10) != 0);
                        entries.push_back(item);

                        pos += 46 + nameLen + extraLen + commentLen;
                        count++;
                    }
                }
            }
        }
        fclose(f);
        return true;
    }

    // 2. TAR format (.tar)
    if (ext == L".tar") {
        outFormat = "TAR";
        unsigned char block[512];
        while (fread(block, 1, 512, f) == 512 && entries.size() < 5000) {
            if (block[0] == 0) break;
            char nameBuf[101] = {0};
            memcpy(nameBuf, block, 100);
            uint64_t fsize = 0;
            for (int i = 124; i < 136 && block[i]; ++i) {
                if (block[i] >= '0' && block[i] <= '7') {
                    fsize = (fsize << 3) + (block[i] - '0');
                }
            }
            char typeFlag = (char)block[156];
            ArchiveEntryInfo item;
            item.name = NormalizeFilenameToUtf8(nameBuf);
            item.uncompressedSize = fsize;
            item.compressedSize = fsize;
            item.isDir = (typeFlag == '5' || (!item.name.empty() && item.name.back() == '/'));
            entries.push_back(item);

            int64_t skipBlocks = (fsize + 511) / 512;
            _fseeki64(f, skipBlocks * 512, SEEK_CUR);
        }
        fclose(f);
        return true;
    }

    // 3. GZIP format (\x1f\x8b or .gz / .tgz)
    if ((magicRead >= 2 && magic[0] == 0x1F && magic[1] == 0x8B) || ext == L".gz" || ext == L".tgz") {
        outFormat = "GZIP";
        unsigned char hdr[10] = {0};
        _fseeki64(f, 0, SEEK_SET);
        fread(hdr, 1, 10, f);
        std::string origName;
        if (hdr[3] & 0x08) { // FNAME flag
            if (hdr[3] & 0x04) { // FEXTRA
                uint16_t xlen = 0;
                fread(&xlen, 2, 1, f);
                _fseeki64(f, xlen, SEEK_CUR);
            }
            int ch;
            while ((ch = fgetc(f)) != EOF && ch != 0 && origName.size() < 255) {
                origName.push_back((char)ch);
            }
        }
        if (origName.empty()) {
            std::wstring stem = archivePath;
            size_t slash = stem.find_last_of(L"\\/");
            if (slash != std::wstring::npos) stem = stem.substr(slash + 1);
            if (stem.size() > 3 && OmniPeek::ToLowerW(stem.substr(stem.size() - 3)) == L".gz") {
                stem = stem.substr(0, stem.size() - 3);
            }
            origName = OmniPeek::WideToUtf8(stem);
        }
        uint32_t isize = 0;
        if (fileSize >= 18) {
            _fseeki64(f, fileSize - 4, SEEK_SET);
            fread(&isize, 4, 1, f);
        }
        ArchiveEntryInfo item;
        item.name = NormalizeFilenameToUtf8(origName);
        item.compressedSize = (uint64_t)fileSize;
        item.uncompressedSize = isize;
        item.isDir = false;
        entries.push_back(item);
        fclose(f);
        return true;
    }

    // 4. 7-Zip / RAR binary header & filename table scanner (.7z, .rar)
    outFormat = (ext == L".7z") ? "7Z" : (ext == L".rar" ? "RAR" : "ARCHIVE");
    int64_t scanBytes = std::min<int64_t>(fileSize, 512 * 1024);
    std::vector<unsigned char> buf((size_t)scanBytes);
    _fseeki64(f, 0, SEEK_SET);
    fread(buf.data(), 1, (size_t)scanBytes, f);
    fclose(f);

    // Scan for UTF-16LE filenames (used in 7z headers) and UTF-8/ASCII path entries (used in RAR headers)
    auto looksLikeEntryName = [](const std::string& s) -> bool {
        if (s.size() < 2 || s.size() > 200) return false;
        if (s.find("Rar!") != std::string::npos) return false;
        bool hasDotOrSlash = false;
        for (unsigned char c : s) {
            if (c < 0x20 || c == '*' || c == '?' || c == '<' || c == '>' || c == '|') return false;
            if (c == '.' || c == '/' || c == '\\') hasDotOrSlash = true;
        }
        return hasDotOrSlash;
    };

    if (ext == L".7z") {
        // 7z stores file names as UTF-16LE strings in the header (often near the end of the archive)
        FILE* f7 = _wfopen(archivePath.c_str(), L"rb");
        if (f7) {
            int64_t tailLen = std::min<int64_t>(fileSize, 256 * 1024);
            std::vector<unsigned char> tailBuf((size_t)tailLen);
            _fseeki64(f7, fileSize - tailLen, SEEK_SET);
            fread(tailBuf.data(), 1, (size_t)tailLen, f7);
            fclose(f7);

            std::wstring curW;
            for (size_t i = 0; i + 1 < tailBuf.size(); i += 2) {
                uint16_t ch = *(uint16_t*)(&tailBuf[i]);
                if ((ch >= 0x20 && ch <= 0x7E) || (ch >= 0x4E00 && ch <= 0x9FFF)) {
                    curW.push_back((WCHAR)ch);
                } else {
                    if (curW.size() >= 3 && curW.size() <= 160) {
                        std::string u8 = OmniPeek::WideToUtf8(curW);
                        for (auto& c : u8) if (c == '\\') c = '/';
                        if (looksLikeEntryName(u8)) {
                            ArchiveEntryInfo item;
                            item.name = u8;
                            item.uncompressedSize = 0;
                            item.compressedSize = 0;
                            item.isDir = (!u8.empty() && u8.back() == '/');
                            entries.push_back(item);
                        }
                    }
                    curW.clear();
                }
            }
        }
    }

    if (entries.empty()) {
        std::string cur;
        for (size_t i = 0; i < buf.size(); ++i) {
            unsigned char c = buf[i];
            if (c >= 0x20 && c <= 0x7E) {
                cur.push_back((char)c);
            } else {
                if (looksLikeEntryName(cur)) {
                    for (auto& ch : cur) if (ch == '\\') ch = '/';
                    ArchiveEntryInfo item;
                    item.name = cur;
                    item.isDir = (!cur.empty() && cur.back() == '/');
                    entries.push_back(item);
                }
                cur.clear();
            }
        }
    }

    return true;
}

bool ExtractArchiveManifestJson(const std::wstring& archivePath, const std::wstring& outJsonPath) {
    std::vector<ArchiveEntryInfo> entries;
    uint64_t archiveSize = 0;
    std::string fmt = "ARCHIVE";
    if (!ParseArchiveEntries(archivePath, entries, archiveSize, fmt)) return false;

    uint64_t totalUncomp = 0;
    uint64_t totalComp = 0;
    for (const auto& e : entries) {
        totalUncomp += e.uncompressedSize;
        totalComp += e.compressedSize;
    }
    if (totalComp == 0) totalComp = archiveSize;

    std::ostringstream oss;
    oss << "{\n"
        << "  \"format\": \"" << JsonEscape(fmt) << "\",\n"
        << "  \"archiveSize\": " << archiveSize << ",\n"
        << "  \"totalUncompressed\": " << totalUncomp << ",\n"
        << "  \"totalCompressed\": " << totalComp << ",\n"
        << "  \"entryCount\": " << entries.size() << ",\n"
        << "  \"entries\": [\n";
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        oss << "    { \"name\": \"" << JsonEscape(e.name) << "\", \"size\": " << e.uncompressedSize
            << ", \"packed\": " << e.compressedSize << ", \"isDir\": " << (e.isDir ? "true" : "false") << " }"
            << (i + 1 < entries.size() ? "," : "") << "\n";
    }
    oss << "  ]\n}\n";

    std::string jsonStr = oss.str();
    FILE* out = _wfopen(outJsonPath.c_str(), L"wb");
    if (!out) return false;
    fwrite(jsonStr.data(), 1, jsonStr.size(), out);
    fclose(out);
    return true;
}

// Safe head extractor & UTF-8 normalizer for text/code/markdown/json/csv/log files
bool ExtractTextHeadFile(const std::wstring& inputPath, const std::wstring& outUtf8Path, int maxKB = 512) {
    FILE* f = _wfopen(inputPath.c_str(), L"rb");
    if (!f) return false;

    _fseeki64(f, 0, SEEK_END);
    int64_t fileSize = _ftelli64(f);
    _fseeki64(f, 0, SEEK_SET);

    size_t maxBytes = (size_t)std::max(16, maxKB) * 1024;
    size_t readLen = (fileSize > 0) ? (size_t)std::min<int64_t>(fileSize, (int64_t)maxBytes) : 0;

    std::vector<unsigned char> raw(readLen);
    if (readLen > 0) {
        readLen = fread(raw.data(), 1, readLen, f);
        raw.resize(readLen);
    }
    fclose(f);

    std::string utf8Out;
    if (raw.size() >= 2 && raw[0] == 0xFF && raw[1] == 0xFE) {
        // UTF-16LE BOM
        const WCHAR* wptr = (const WCHAR*)(raw.data() + 2);
        size_t wlen = (raw.size() - 2) / 2;
        utf8Out = OmniPeek::WideToUtf8(std::wstring(wptr, wlen));
    } else if (raw.size() >= 2 && raw[0] == 0xFE && raw[1] == 0xFF) {
        // UTF-16BE BOM
        size_t wlen = (raw.size() - 2) / 2;
        std::wstring wstr(wlen, 0);
        for (size_t i = 0; i < wlen; ++i) {
            wstr[i] = (WCHAR)((raw[2 + i * 2] << 8) | raw[2 + i * 2 + 1]);
        }
        utf8Out = OmniPeek::WideToUtf8(wstr);
    } else if (raw.size() >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF) {
        // UTF-8 with BOM
        utf8Out.assign((const char*)raw.data() + 3, raw.size() - 3);
    } else if (IsValidUtf8((const char*)raw.data(), raw.size())) {
        utf8Out.assign((const char*)raw.data(), raw.size());
    } else {
        // Fallback: Convert system ANSI / GBK to UTF-8
        int wlen = MultiByteToWideChar(CP_ACP, 0, (const char*)raw.data(), (int)raw.size(), NULL, 0);
        if (wlen > 0) {
            std::wstring wstr(wlen, 0);
            MultiByteToWideChar(CP_ACP, 0, (const char*)raw.data(), (int)raw.size(), &wstr[0], wlen);
            utf8Out = OmniPeek::WideToUtf8(wstr);
        } else {
            utf8Out.assign((const char*)raw.data(), raw.size());
        }
    }

    FILE* out = _wfopen(outUtf8Path.c_str(), L"wb");
    if (!out) return false;
    if (!utf8Out.empty()) {
        fwrite(utf8Out.data(), 1, utf8Out.size(), out);
    }
    fclose(out);
    return true;
}

// Helper to save a 32-bit DIB pixel buffer (top-down BGRA) to a .bmp file
static bool SaveDibPixelsToBmp(const void* bits, int W, int H, const std::wstring& outBmp) {
    if (!bits || W <= 0 || H <= 0) return false;
    BITMAPFILEHEADER bfh = {0};
    BITMAPINFOHEADER bih = {0};
    bfh.bfType = 0x4D42;
    bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + W * H * 4;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = W;
    bih.biHeight = -H; // top-down
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;

    FILE* out = _wfopen(outBmp.c_str(), L"wb");
    if (!out) return false;
    fwrite(&bfh, sizeof(bfh), 1, out);
    fwrite(&bih, sizeof(bih), 1, out);
    fwrite(bits, 4, (size_t)W * H, out);
    fclose(out);
    return true;
}

static std::wstring FormatFileSizeW(uint64_t bytes) {
    wchar_t buf[64];
    if (bytes < 1024) {
        swprintf(buf, 64, L"%llu B", (unsigned long long)bytes);
    } else if (bytes < 1024 * 1024) {
        swprintf(buf, 64, L"%.1f KB", bytes / 1024.0);
    } else if (bytes < 1024ULL * 1024 * 1024) {
        swprintf(buf, 64, L"%.2f MB", bytes / (1024.0 * 1024.0));
    } else {
        swprintf(buf, 64, L"%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    }
    return buf;
}

// Read TrueType/OpenType font family name from 'name' table if possible
static std::wstring ExtractTrueTypeFontName(const std::wstring& fontPath) {
    FILE* f = _wfopen(fontPath.c_str(), L"rb");
    if (!f) return L"";
    unsigned char hdr[12];
    if (fread(hdr, 1, 12, f) != 12) { fclose(f); return L""; }
    uint16_t numTables = (hdr[4] << 8) | hdr[5];
    if (numTables > 128) { fclose(f); return L""; }

    uint32_t nameOffset = 0, nameLen = 0;
    for (int i = 0; i < numTables; ++i) {
        unsigned char rec[16];
        if (fread(rec, 1, 16, f) != 16) break;
        if (rec[0] == 'n' && rec[1] == 'a' && rec[2] == 'm' && rec[3] == 'e') {
            nameOffset = ((uint32_t)rec[8] << 24) | ((uint32_t)rec[9] << 16) | ((uint32_t)rec[10] << 8) | rec[11];
            nameLen    = ((uint32_t)rec[12] << 24) | ((uint32_t)rec[13] << 16) | ((uint32_t)rec[14] << 8) | rec[15];
            break;
        }
    }
    if (nameOffset == 0 || nameLen < 6 || nameLen > 65536) { fclose(f); return L""; }
    std::vector<unsigned char> tbl(nameLen);
    _fseeki64(f, nameOffset, SEEK_SET);
    if (fread(tbl.data(), 1, nameLen, f) != nameLen) { fclose(f); return L""; }
    fclose(f);

    uint16_t count = (tbl[2] << 8) | tbl[3];
    uint16_t strOffset = (tbl[4] << 8) | tbl[5];
    for (int i = 0; i < count; ++i) {
        size_t pos = 6 + i * 12;
        if (pos + 12 > tbl.size()) break;
        uint16_t platformId = (tbl[pos] << 8) | tbl[pos + 1];
        uint16_t nameId     = (tbl[pos + 6] << 8) | tbl[pos + 7];
        uint16_t len        = (tbl[pos + 8] << 8) | tbl[pos + 9];
        uint16_t off        = (tbl[pos + 10] << 8) | tbl[pos + 11];
        if (nameId == 1 && platformId == 3 && strOffset + off + len <= tbl.size() && len >= 2) {
            std::wstring res;
            for (size_t k = 0; k + 1 < len; k += 2) {
                WCHAR ch = (WCHAR)((tbl[strOffset + off + k] << 8) | tbl[strOffset + off + k + 1]);
                res.push_back(ch);
            }
            if (!res.empty()) return res;
        }
    }
    return L"";
}

// High-DPI GDI Card Thumbnail Generator for Media, Dev/Docs, Archives, and Fonts
bool RenderRichCardThumbnail(const std::wstring& inputPath, const std::wstring& outBmpPath, int size, const OmniPeek::FormatDescriptor& desc) {
    int W = std::max(128, std::min(512, size));
    int H = W;

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = W;
    bmi.bmiHeader.biHeight = -H; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pvBits = nullptr;
    HDC hdcScreen = GetDC(NULL);
    HDC hdc = CreateCompatibleDC(hdcScreen);
    HBITMAP hbm = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0);
    ReleaseDC(NULL, hdcScreen);
    if (!hbm || !pvBits) {
        if (hdc) DeleteDC(hdc);
        return false;
    }

    HGDIOBJ hOldBmp = SelectObject(hdc, hbm);
    SetBkMode(hdc, TRANSPARENT);

    // Choose accent color by category/mode
    uint8_t accR = 59, accG = 130, accB = 246; // Default Blue
    if (desc.thumbnailMode == L"media-card") {
        accR = 236; accG = 72; accB = 153;     // Rose/Magenta for Media
        if (desc.previewMode == L"audio") { accR = 168; accG = 85; accB = 247; } // Purple for Audio
    } else if (desc.thumbnailMode == L"doc-card") {
        accR = 16; accG = 185; accB = 129;     // Emerald for Docs/Code
        if (desc.previewMode == L"json") { accR = 245; accG = 158; accB = 11; }
        else if (desc.previewMode == L"pdf") { accR = 239; accG = 68; accB = 68; }
    } else if (desc.thumbnailMode == L"archive-card") {
        accR = 245; accG = 158; accB = 11;     // Amber for Archives
    } else if (desc.thumbnailMode == L"font-card") {
        accR = 6; accG = 182; accB = 212;      // Cyan for Fonts
    }

    // Fill background with studio dark gradient + border
    uint32_t* px = (uint32_t*)pvBits;
    int pad = W / 16;
    for (int y = 0; y < H; ++y) {
        float t = (float)y / (float)H;
        uint8_t bgR = (uint8_t)(20 + t * 12);
        uint8_t bgG = (uint8_t)(24 + t * 14);
        uint8_t bgB = (uint8_t)(34 + t * 20);
        for (int x = 0; x < W; ++x) {
            bool inCard = (x >= pad && x < W - pad && y >= pad && y < H - pad);
            if (!inCard) {
                px[y * W + x] = (0xFFu << 24) | (15u << 16) | (18u << 8) | 25u;
            } else if (y < pad + (H / 18)) {
                // Top accent bar
                px[y * W + x] = (0xFFu << 24) | ((uint32_t)accR << 16) | ((uint32_t)accG << 8) | accB;
            } else {
                px[y * W + x] = (0xFFu << 24) | ((uint32_t)bgR << 16) | ((uint32_t)bgG << 8) | bgB;
            }
        }
    }

    WIN32_FILE_ATTRIBUTE_DATA fileAttr = {0};
    uint64_t fileBytes = 0;
    if (GetFileAttributesExW(inputPath.c_str(), GetFileExInfoStandard, &fileAttr)) {
        fileBytes = ((uint64_t)fileAttr.nFileSizeHigh << 32) | fileAttr.nFileSizeLow;
    }

    // Mode-specific foreground rendering
    if (desc.thumbnailMode == L"font-card") {
        int added = AddFontResourceExW(inputPath.c_str(), FR_PRIVATE, 0);
        std::wstring family = ExtractTrueTypeFontName(inputPath);
        if (family.empty()) family = L"Segoe UI";

        HFONT hGlyphFont = CreateFontW(W * 38 / 100, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, family.c_str());
        HFONT hSampleFont = CreateFontW(W * 9 / 100, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, family.c_str());

        SelectObject(hdc, hGlyphFont);
        SetTextColor(hdc, RGB(248, 250, 252));
        RECT rcGlyph = { pad + 12, pad + W / 9, W - pad - 12, H - pad - W / 3 };
        DrawTextW(hdc, L"Aa\x6C38", -1, &rcGlyph, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, hSampleFont);
        SetTextColor(hdc, RGB(148, 163, 184));
        RECT rcSub = { pad + 14, H - pad - W * 36 / 100, W - pad - 14, H - pad - W * 20 / 100 };
        DrawTextW(hdc, L"ABCDEFGHIJKLM\n0123456789", -1, &rcSub, DT_CENTER | DT_NOPREFIX);

        DeleteObject(hGlyphFont);
        DeleteObject(hSampleFont);
        if (added > 0) RemoveFontResourceExW(inputPath.c_str(), FR_PRIVATE, 0);
    } else if (desc.thumbnailMode == L"archive-card") {
        std::vector<ArchiveEntryInfo> entries;
        uint64_t archSz = 0;
        std::string fmtName;
        ParseArchiveEntries(inputPath, entries, archSz, fmtName);

        HFONT hTitleFont = CreateFontW(W * 9 / 100, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        HFONT hListFont = CreateFontW(W * 6 / 100, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");

        SelectObject(hdc, hTitleFont);
        SetTextColor(hdc, RGB(251, 191, 36));
        std::wstring summary = std::to_wstring(entries.size()) + L" 项归档 · " + FormatFileSizeW(fileBytes);
        RECT rcSum = { pad + 12, pad + W / 10, W - pad - 12, pad + W / 4 };
        DrawTextW(hdc, summary.c_str(), -1, &rcSum, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        SelectObject(hdc, hListFont);
        SetTextColor(hdc, RGB(203, 213, 225));
        int yCur = pad + W * 24 / 100;
        int lineH = W * 8 / 100;
        for (size_t i = 0; i < std::min<size_t>(entries.size(), 6); ++i) {
            std::wstring itemW = (entries[i].isDir ? L"[DIR] " : L"• ") + OmniPeek::Utf8ToWide(entries[i].name);
            RECT rcLine = { pad + 14, yCur, W - pad - 14, yCur + lineH };
            DrawTextW(hdc, itemW.c_str(), -1, &rcLine, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            yCur += lineH;
        }
        DeleteObject(hTitleFont);
        DeleteObject(hListFont);
    } else if (desc.thumbnailMode == L"media-card") {
        // Draw audio/video studio visualizer bars in pixel buffer
        int centerY = H * 44 / 100;
        int numBars = 14;
        int barAreaW = W - 2 * (pad + 22);
        int barStep = barAreaW / numBars;
        int barW = std::max(4, barStep * 6 / 10);
        for (int b = 0; b < numBars; ++b) {
            float wave = 0.25f + 0.65f * std::abs(std::sin((b + 1) * 0.78f + (float)(fileBytes % 17)));
            int halfH = (int)((H * 22 / 100) * wave);
            int x0 = pad + 22 + b * barStep;
            for (int y = centerY - halfH; y <= centerY + halfH; ++y) {
                if (y < pad + 16 || y >= H - pad - 48) continue;
                for (int x = x0; x < x0 + barW && x < W - pad - 16; ++x) {
                    px[y * W + x] = (0xFFu << 24) | ((uint32_t)accR << 16) | ((uint32_t)accG << 8) | accB;
                }
            }
        }

        HFONT hMetaFont = CreateFontW(W * 8 / 100, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        SelectObject(hdc, hMetaFont);
        SetTextColor(hdc, RGB(226, 232, 240));
        std::wstring mediaTag = (desc.previewMode == L"audio" ? L"♪ AUDIO · " : L"▶ VIDEO · ") + FormatFileSizeW(fileBytes);
        RECT rcMeta = { pad + 14, H - pad - W * 34 / 100, W - pad - 14, H - pad - W * 20 / 100 };
        DrawTextW(hdc, mediaTag.c_str(), -1, &rcMeta, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
        DeleteObject(hMetaFont);
    } else {
        // doc-card: Read first 9 lines of text/code/markdown/json/csv and render miniature page
        std::wstring tempHead = outBmpPath + L".head.txt";
        std::vector<std::wstring> lines;
        if (desc.ext != L".pdf" && desc.ext != L".xlsx" && ExtractTextHeadFile(inputPath, tempHead, 8)) {
            FILE* tf = _wfopen(tempHead.c_str(), L"rb");
            if (tf) {
                char lineBuf[256];
                while (fgets(lineBuf, sizeof(lineBuf), tf) && lines.size() < 9) {
                    std::wstring wl = OmniPeek::Utf8ToWide(lineBuf);
                    while (!wl.empty() && (wl.back() == L'\r' || wl.back() == L'\n')) wl.pop_back();
                    if (!wl.empty()) lines.push_back(wl);
                }
                fclose(tf);
            }
            DeleteFileW(tempHead.c_str());
        }
        if (lines.empty()) {
            lines.push_back(desc.label);
            lines.push_back(L"Size: " + FormatFileSizeW(fileBytes));
        }

        HFONT hCodeFont = CreateFontW(std::max(10, W * 55 / 1000), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
        SelectObject(hdc, hCodeFont);
        int yCur = pad + W * 10 / 100;
        int lineH = std::max(12, W * 72 / 1000);
        for (size_t i = 0; i < lines.size(); ++i) {
            SetTextColor(hdc, (i == 0) ? RGB(125, 211, 252) : RGB(203, 213, 225));
            RECT rcLine = { pad + 12, yCur, W - pad - 12, yCur + lineH };
            DrawTextW(hdc, lines[i].c_str(), -1, &rcLine, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            yCur += lineH;
        }
        DeleteObject(hCodeFont);
    }

    // Bottom extension pill badge
    std::wstring badgeText = desc.ext;
    if (!badgeText.empty() && badgeText[0] == L'.') badgeText = badgeText.substr(1);
    for (auto& c : badgeText) c = (WCHAR)towupper(c);

    int badgeH = W * 15 / 100;
    int badgeY = H - pad - badgeH - (W / 25);
    int badgeX1 = pad + (W / 20);
    int badgeX2 = W - pad - (W / 20);
    for (int y = badgeY; y < badgeY + badgeH && y < H - pad; ++y) {
        for (int x = badgeX1; x < badgeX2 && x < W - pad; ++x) {
            px[y * W + x] = (0xFFu << 24) | ((uint32_t)accR << 16) | ((uint32_t)accG << 8) | accB;
        }
    }

    HFONT hBadgeFont = CreateFontW(W * 10 / 100, 0, 0, 0, FW_EXTRABOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    SelectObject(hdc, hBadgeFont);
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT rcBadge = { badgeX1, badgeY, badgeX2, badgeY + badgeH };
    DrawTextW(hdc, badgeText.c_str(), -1, &rcBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DeleteObject(hBadgeFont);

    GdiFlush();
    bool ok = SaveDibPixelsToBmp(pvBits, W, H, outBmpPath);

    SelectObject(hdc, hOldBmp);
    DeleteObject(hbm);
    DeleteDC(hdc);
    return ok;
}

bool ProcessUniversalThumbnail(const std::wstring& input, const std::wstring& output, int size) {
    std::wstring ext;
    size_t dot = input.find_last_of(L'.');
    if (dot != std::wstring::npos) {
        ext = OmniPeek::ToLowerW(input.substr(dot));
    }

    OmniPeek::FormatDescriptor desc;
    if (OmniPeek::PluginRegistry::FindFormatByExtension(ext, desc)) {
        if (desc.thumbnailMode == L"3d-raster") {
            return Process3DThumbnail(input, output, size);
        }
        return RenderRichCardThumbnail(input, output, size, desc);
    }

    // Default fallback if extension not in registry: try 3D first, then generic doc-card
    if (Process3DThumbnail(input, output, size)) return true;
    desc.ext = ext.empty() ? L".file" : ext;
    desc.label = input;
    desc.thumbnailMode = L"doc-card";
    return RenderRichCardThumbnail(input, output, size, desc);
}

void RunDaemonMode() {
    LPCWSTR pipeName = OMNIPEEK_PIPE_NAME;
    std::wcout << L"OmniPeek Worker Daemon started on " << pipeName << std::endl;
    while (true) {
        HANDLE hPipe = CreateNamedPipeW(
            pipeName,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            8192, 8192, 5000, NULL
        );
        if (hPipe == INVALID_HANDLE_VALUE) {
            Sleep(100);
            continue;
        }

        BOOL connected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (connected) {
            WCHAR buffer[4096] = {0};
            DWORD bytesRead = 0;
            if (ReadFile(hPipe, buffer, sizeof(buffer) - sizeof(WCHAR), &bytesRead, NULL)) {
                std::wstring request = buffer;
                std::wstringstream ss(request);
                std::wstring op, arg1, arg2, arg3;
                std::getline(ss, op, L'\t');
                std::getline(ss, arg1, L'\t');
                std::getline(ss, arg2, L'\t');
                std::getline(ss, arg3, L'\t');

                std::wstring response = L"FAILED\n";
                if (op == L"thumbnail" && !arg1.empty() && !arg2.empty()) {
                    int size = arg3.empty() ? 256 : _wtoi(arg3.c_str());
                    if (ProcessUniversalThumbnail(arg1, arg2, size)) {
                        response = L"SUCCESS\n";
                    }
                } else if ((op == L"convert" || op == L"convert-cad") && !arg1.empty() && !arg2.empty()) {
                    if (Convert3DCadToStl(arg1, arg2)) {
                        response = L"SUCCESS\n";
                    }
                } else if (op == L"archive-list" && !arg1.empty() && !arg2.empty()) {
                    if (ExtractArchiveManifestJson(arg1, arg2)) {
                        response = L"SUCCESS\n";
                    }
                } else if (op == L"text-head" && !arg1.empty() && !arg2.empty()) {
                    int maxKB = arg3.empty() ? 512 : _wtoi(arg3.c_str());
                    if (ExtractTextHeadFile(arg1, arg2, maxKB)) {
                        response = L"SUCCESS\n";
                    }
                }

                DWORD bytesWritten = 0;
                WriteFile(hPipe, response.c_str(), (DWORD)(response.length() * sizeof(WCHAR)), &bytesWritten, NULL);
            }
        }
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
}

int main(int, char*[]) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || argc < 2) {
        std::wcout << L"OmniPeek Universal Worker v" << OMNIPEEK_VERSION_STR << L"\n"
                   << L"Usage:\n"
                   << L"  OmniPeekWorker.exe thumbnail <input_file> <output_bmp> [size]\n"
                   << L"  OmniPeekWorker.exe convert-cad <input_cad> <output_stl>\n"
                   << L"  OmniPeekWorker.exe archive-list <input_archive> <output_json>\n"
                   << L"  OmniPeekWorker.exe text-head <input_text> <output_utf8> [max_kb]\n"
                   << L"  OmniPeekWorker.exe --daemon\n";
        if (argv) LocalFree(argv);
        return 0;
    }

    std::wstring cmd = argv[1];
    int rc = 1;

    if (cmd == L"--daemon" || cmd == L"daemon") {
        LocalFree(argv);
        RunDaemonMode();
        return 0;
    } else if ((cmd == L"convert" || cmd == L"convert-cad") && argc >= 4) {
        if (Convert3DCadToStl(argv[2], argv[3])) {
            std::wcout << L"SUCCESS" << std::endl;
            rc = 0;
        } else {
            std::wcerr << L"FAILED" << std::endl;
        }
    } else if (cmd == L"archive-list" && argc >= 4) {
        if (ExtractArchiveManifestJson(argv[2], argv[3])) {
            std::wcout << L"SUCCESS" << std::endl;
            rc = 0;
        } else {
            std::wcerr << L"FAILED" << std::endl;
        }
    } else if (cmd == L"text-head" && argc >= 4) {
        int maxKB = (argc >= 5) ? _wtoi(argv[4]) : 512;
        if (ExtractTextHeadFile(argv[2], argv[3], maxKB)) {
            std::wcout << L"SUCCESS" << std::endl;
            rc = 0;
        } else {
            std::wcerr << L"FAILED" << std::endl;
        }
    } else if (cmd == L"thumbnail" && argc >= 4) {
        int size = (argc >= 5) ? _wtoi(argv[4]) : 256;
        if (ProcessUniversalThumbnail(argv[2], argv[3], size)) {
            std::wcout << L"SUCCESS" << std::endl;
            rc = 0;
        } else {
            std::wcerr << L"FAILED" << std::endl;
        }
    }

    LocalFree(argv);
    return rc;
}
