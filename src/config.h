#ifndef _JUCE_CONFIG
#define _JUCE_CONFIG

#include <windows.h>

#define CONFIG_FILE "taksi.cfg"
#define BUFLEN 4096

#define CUSTOM_PATTERN_SAVED      0x0001
#define CUSTOM_FRAME_RATE_SAVED   0x0002
#define CUSTOM_FRAME_WEIGHT_SAVED 0x0004
#define ALL_SAVED \
	(CUSTOM_PATTERN_SAVED | CUSTOM_FRAME_RATE_SAVED | CUSTOM_FRAME_WEIGHT_SAVED)

typedef struct _TAXI_CUSTOM_CONFIG {
	char   appId[BUFLEN];
	char   pattern[BUFLEN];
	int    frameRate;
	float  frameWeight;
	DWORD  flags;
	struct _TAXI_CUSTOM_CONFIG* next;

} TAXI_CUSTOM_CONFIG;

#define DEFAULT_DEBUG 0
#define DEFAULT_CAPTURE_DIR ""
#define DEFAULT_TARGET_FRAME_RATE 20
#define DEFAULT_VKEY_INDICATOR 0x74
#define DEFAULT_VKEY_HOOK_MODE 0x75
#define DEFAULT_VKEY_SMALL_SCREENSHOT 0x76
#define DEFAULT_VKEY_SCREENSHOT 0x77
#define DEFAULT_VKEY_VIDEO_CAPTURE 0x7b
#define DEFAULT_USE_DIRECT_INPUT true
#define DEFAULT_STARTUP_MODE_SYSTEM_WIDE false
#define DEFAULT_FULL_SIZE_VIDEO false
#define DEFAULT_CUSTOM_LIST NULL

typedef struct _TAXI_CONFIG_STRUCT {
	DWORD  debug;
	char   captureDir[BUFLEN];
	DWORD  targetFrameRate;
	WORD   vKeyIndicator;
	WORD   vKeyHookMode;
	WORD   vKeySmallScreenShot;
	WORD   vKeyScreenShot;
	WORD   vKeyVideoCapture;
	bool   useDirectInput;
	bool   startupModeSystemWide;
	bool   fullSizeVideo;
	TAXI_CUSTOM_CONFIG* customList;

} TAXI_CONFIG;

BOOL ReadConfigFromDir(TAXI_CONFIG* config, char* dir);
BOOL ReadConfig(TAXI_CONFIG* config);
BOOL WriteConfig(TAXI_CONFIG* config);

TAXI_CUSTOM_CONFIG* LookupExistingCustomConfig(TAXI_CONFIG* config, char* appId);
TAXI_CUSTOM_CONFIG* LookupCustomConfig(TAXI_CONFIG* config, char* appId);
VOID DeleteCustomConfig(TAXI_CONFIG* config, char* appId);
VOID FreeCustomConfigs(TAXI_CONFIG* config);

BOOL SearchCustomConfig(char* processfile, char* dir, TAXI_CUSTOM_CONFIG** cfg);

#endif
