#pragma once
#ifndef OMNIPEEK_CORE_API_H
#define OMNIPEEK_CORE_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OMNI_SUCCESS = 0,
    OMNI_ERROR_FILE_NOT_FOUND = -1,
    OMNI_ERROR_UNSUPPORTED_FORMAT = -2
} OmniResult;

OmniResult OmniPeek_InitEngine(void);
OmniResult OmniPeek_RenderPreview(const char* filePath, void* hwndTarget);
void OmniPeek_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // OMNIPEEK_CORE_API_H
