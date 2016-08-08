// win32gui.h

#ifndef _JUCE_WIN32GUI
#define _JUCE_WIN32GUI

#include <windows.h>

#define WIN_WIDTH 420 
#define WIN_HEIGHT 335

extern HWND g_captureDirectoryControl;     // displays capture directory path
extern HWND g_frameRateControl;            // displays adaptive frame rate

extern HWND g_keyIndicatorControl;         // displays key binding for indicator toggle
extern HWND g_keyModeToggleControl;        // displays key binding for mode toggle
extern HWND g_keySmallScreenshotControl;   // displays key binding for small screenshot
extern HWND g_keyScreenshotControl;        // displays key binding for normal screenshot
extern HWND g_keyVideoToggleControl;       // displays key binding for video toggle

extern HWND g_customSettingsListControl;   // drop-down list of custom settings
extern HWND g_customAddButtonControl;      // "Add" button for custom settings
extern HWND g_customDropButtonControl;     // "Drop" button for custom settings
extern HWND g_customPatternControl;        // displays current custom pattern string
extern HWND g_customFrameRateControl;      // displays current custom frame rate
extern HWND g_customFrameWeightControl;    // displays current custom frame weight

extern HWND g_statusTextControl;           // displays status messages
extern HWND g_restoreButtonControl;        // restore settings button
extern HWND g_saveButtonControl;           // save settings button

// macros
#define VKEY_TEXT(key, buffer, buflen) \
	ZeroMemory(buffer, buflen); \
	GetKeyNameText(MapVirtualKey(GetKey(key), 0) << 16, buffer, buflen)

// functions
bool BuildControls(HWND parent);

#endif
