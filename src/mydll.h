#ifndef _DEFINED_MYDLL
#define _DEFINED_MYDLL

#if _MSC_VER > 1000
#pragma once
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define LIBSPEC __declspec(dllexport)

#define BUFLEN 4096  /* 4K buffer length */

#define WM_APP_REHOOK WM_APP + 1
#define WM_APP_KEYDEF WM_APP + 2

#define VKEY_INDICATOR_TOGGLE 0
#define VKEY_HOOK_MODE_TOGGLE 1
#define VKEY_SMALL_SCREENSHOT 2
#define VKEY_SCREENSHOT       3
#define VKEY_VIDEO_CAPTURE    4

LIBSPEC LRESULT CALLBACK CBTProc(int code, WPARAM wParam, LPARAM lParam);
LIBSPEC LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam);
LIBSPEC LRESULT CALLBACK DummyKeyboardProc(int code, WPARAM wParam, LPARAM lParam);

LIBSPEC void TriggerScreenShot(void);
LIBSPEC void TriggerSmallScreenShot(void);
LIBSPEC void TriggerVideo(void);

LIBSPEC BOOL TakeScreenShot(DWORD processId);

LIBSPEC BOOL InstallTheHook(void);
LIBSPEC BOOL UninstallTheHook(void);

DWORD QPC();
DWORD GetFrameDups();
void InstallKeyboardHook(DWORD tid);
void InstallDummyKeyboardHook(DWORD tid);
void UninstallKeyboardHook();

typedef enum { CUSTOM, ADAPTIVE } ALGORITHM_TYPE;

typedef struct _struct_DXPOINTERS {
	DWORD present8;
	DWORD reset8;
	DWORD present9;
	DWORD reset9;
} DXPOINTERS;

/* ... more declarations as needed */
#undef LIBSPEC

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _DEFINED_MYDLL */

