#pragma once
#include <windows.h>
#include <initguid.h>

// OmniPeek Thumbnail Provider CLSID: {9A4E8B21-5C6D-4E3F-9A1B-8C7D6E5F4A32}
DEFINE_GUID(CLSID_OmniPeekThumbnailProvider,
    0x9a4e8b21, 0x5c6d, 0x4e3f, 0x9a, 0x1b, 0x8c, 0x7d, 0x6e, 0x5f, 0x4a, 0x32);

// OmniPeek Preview Handler CLSID: {7B3D9C12-4A8E-4F2D-8B3C-6E5F4A3B2C19}
DEFINE_GUID(CLSID_OmniPeekPreviewHandler,
    0x7b3d9c12, 0x4a8e, 0x4f2d, 0x8b, 0x3c, 0x6e, 0x5f, 0x4a, 0x3b, 0x2c, 0x19);

#define CLSID_OMNI_THUMBNAIL_STRING L"{9A4E8B21-5C6D-4E3F-9A1B-8C7D6E5F4A32}"
#define CLSID_OMNI_PREVIEW_STRING   L"{7B3D9C12-4A8E-4F2D-8B3C-6E5F4A3B2C19}"

// Standard Windows Shell Extension Interface GUID strings
#define SHELLEX_THUMBNAIL_KEY L"{e357fccd-a995-4576-b01f-234630154e96}"
#define SHELLEX_PREVIEW_KEY   L"{8895b1c6-b41f-4c1c-a562-0d564250836f}"

// Windows prevhost.exe Surrogate AppID (64-bit / standard)
#define PREVHOST_APPID_STRING L"{6d2b5079-2f0b-48dd-ab7f-97cec514d30b}"

// Named Pipe for OmniPeek Worker Daemon IPC
#define OMNIPEEK_PIPE_NAME    L"\\\\.\\pipe\\OmniPeekWorkerPipe"
#define OMNIPEEK_VERSION_STR  L"1.0.0"
