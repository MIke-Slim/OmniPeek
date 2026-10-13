#pragma once
#include <windows.h>
#include <shobjidl.h>
#include <thumbcache.h>
#include <string>
#include <cstdint>

class OmniPeekThumbnailProvider : public IThumbnailProvider,
                                  public IInitializeWithFile,
                                  public IInitializeWithItem,
                                  public IInitializeWithStream {
public:
    OmniPeekThumbnailProvider();
    virtual ~OmniPeekThumbnailProvider();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IInitializeWithFile
    STDMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode) override;

    // IInitializeWithItem
    STDMETHODIMP Initialize(IShellItem *psi, DWORD grfMode) override;

    // IInitializeWithStream
    STDMETHODIMP Initialize(IStream *pstream, DWORD grfMode) override;

    // IThumbnailProvider
    STDMETHODIMP GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha) override;

private:
    long m_refCount;
    std::wstring m_filePath;

    std::wstring GetCacheDirectory();
    std::wstring GetThumbnailCachePath(const std::wstring& filePath, UINT cx);
    bool GenerateThumbnail(const std::wstring& filePath, const std::wstring& outBmpPath, UINT cx);
};
