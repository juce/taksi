#ifndef _JUCE_SHAREDMEM
#define _JUCE_SHAREDMEM

#if _MSC_VER > 1000
	#pragma once
#endif

#define LIBSPEC __declspec(dllexport)

#define TAKSI_EXE "taksi.exe"
#define MAX_FILEPATH 4096

// Process information pack
typedef struct _MYINFO {
	char file[MAX_FILEPATH];
	char dir[MAX_FILEPATH];
	char logfile[MAX_FILEPATH];
	char *shortLogfile;
	char processfile[MAX_FILEPATH];
	char *shortProcessfile;
	char shortProcessfileNoExt[MAX_FILEPATH];
} MYINFO;

// Taksi state information pack
typedef struct _MYSTATE {
	BOOL bIndicate;
	BOOL bKeyboardInitDone;
	BOOL bMakeScreenShot;
	BOOL bMakeSmallScreenShot;
	BOOL bStartRecording;
	BOOL bNowRecording;
	BOOL bSystemWideMode;
	BOOL bStopRecording;
	BOOL bUnhookGetMsgProc;
} MYSTATE;

EXTERN_C LIBSPEC void  SetUnhookFlag(BOOL);
EXTERN_C LIBSPEC BOOL  GetUnhookFlag(void);
EXTERN_C LIBSPEC void  SetTargetFrameRate(DWORD);
EXTERN_C LIBSPEC DWORD GetTargetFrameRate(void);
EXTERN_C LIBSPEC void  SetIgnoreOverhead(BOOL);
EXTERN_C LIBSPEC BOOL  GetIgnoreOverhead(void);
EXTERN_C LIBSPEC void  SetDebug(DWORD);
EXTERN_C LIBSPEC DWORD GetDebug(void);
EXTERN_C LIBSPEC void  SetKey(int, int);
EXTERN_C LIBSPEC int   GetKey(int);
EXTERN_C LIBSPEC void  SetUseDirectInput(bool);
EXTERN_C LIBSPEC bool  GetUseDirectInput(void);
EXTERN_C LIBSPEC void  SetStartupModeSystemWide(bool);
EXTERN_C LIBSPEC bool  GetStartupModeSystemWide(void);
EXTERN_C LIBSPEC void  SetFullSizeVideo(bool);
EXTERN_C LIBSPEC bool  GetFullSizeVideo(void);
EXTERN_C LIBSPEC void  SetExiting(BOOL);
EXTERN_C LIBSPEC BOOL  GetExiting(void);
EXTERN_C LIBSPEC void  SetServerWnd(HWND);
EXTERN_C LIBSPEC HWND  GetServerWnd(void);
EXTERN_C LIBSPEC void  SetCaptureDir(char* path);
EXTERN_C LIBSPEC void  GetCaptureDir(char* path);
EXTERN_C LIBSPEC void  SetReconfCounter(DWORD);
EXTERN_C LIBSPEC DWORD GetReconfCounter(void);
EXTERN_C LIBSPEC void  SetPresent8(DWORD);
EXTERN_C LIBSPEC DWORD GetPresent8(void);
EXTERN_C LIBSPEC void  SetReset8(DWORD);
EXTERN_C LIBSPEC DWORD GetReset8(void);
EXTERN_C LIBSPEC void  SetPresent9(DWORD);
EXTERN_C LIBSPEC DWORD GetPresent9(void);
EXTERN_C LIBSPEC void  SetReset9(DWORD);
EXTERN_C LIBSPEC DWORD GetReset9(void);

#endif /* _JUCE_SHAREDMEM */

