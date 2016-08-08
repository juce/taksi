#include <windows.h>
#include <stdio.h>
#include "mydll.h"
#include "shared.h"
#include "win32gui.h"

HWND g_captureDirectoryControl;     // displays capture directory path
HWND g_frameRateControl;            // displays adaptive frame rate

HWND g_keyIndicatorControl;         // displays key binding for indicator toggle
HWND g_keyModeToggleControl;        // displays key binding for mode toggle
HWND g_keySmallScreenshotControl;   // displays key binding for small screenshot
HWND g_keyScreenshotControl;        // displays key binding for normal screenshot
HWND g_keyVideoToggleControl;       // displays key binding for video toggle

HWND g_customSettingsListControl;   // drop-down list of custom settings
HWND g_customAddButtonControl;      // "Add" button for custom settings
HWND g_customDropButtonControl;     // "Drop" button for custom settings
HWND g_customPatternControl;        // displays current custom pattern string
HWND g_customFrameRateControl;      // displays current custom frame rate
HWND g_customFrameWeightControl;    // displays current custom frame weight

HWND g_statusTextControl;           // displays status messages
HWND g_restoreButtonControl;        // restore settings button
HWND g_saveButtonControl;           // save settings button


/**
 * build all controls
 */
bool BuildControls(HWND parent)
{
	HGDIOBJ hObj;
	DWORD style, xstyle;
	int x, y, spacer;
	int boxW, boxH, statW, statH, borW, borH, butW, butH;

	spacer = 6; 
	x = spacer, y = spacer;
	butH = 20;

	// use default extended style
	xstyle = WS_EX_LEFT;
	style = WS_CHILD | WS_VISIBLE;

	// TOP SECTION: Adaptive Mode settings

	borW = WIN_WIDTH - spacer*3;
	statW = 85;
	boxW = borW - statW - spacer*3; boxH = 20; statH = 16;
	borH = spacer*3 + boxH*2;

	HWND staticBorderTopControl = CreateWindowEx(
			xstyle, "Static", "", WS_CHILD | WS_VISIBLE | SS_ETCHEDFRAME,
			x, y, borW, borH,
			parent, NULL, NULL, NULL);

	x += spacer; y += spacer;

	HWND captureDirectoryLabel = CreateWindowEx(
			xstyle, "Static", "Capture directory:", style,
			x, y+4, statW, statH, 
			parent, NULL, NULL, NULL);

	x += statW + spacer;

	char captureDir[BUFLEN];
	ZeroMemory(captureDir, BUFLEN);
	GetCaptureDir(captureDir);

	g_captureDirectoryControl = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", captureDir, style | WS_TABSTOP,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

	x = spacer*2;
	y += boxH + spacer;
	boxW = 50;

	HWND frameRateLabel = CreateWindowEx(
			xstyle, "Static", "Target frame rate:", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

	x += statW + spacer;

	char frameRate[BUFLEN];
	ZeroMemory(frameRate, BUFLEN);
	sprintf(frameRate, "%d", GetTargetFrameRate());

	g_frameRateControl = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", frameRate, style | WS_TABSTOP,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

	x += boxW + spacer;
	statW = 30;

	HWND frameRateLabel2 = CreateWindowEx(
			xstyle, "Static", "FPS", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

	x = spacer;
	y = spacer*2 + borH;
	borH = spacer*4 + boxH*3;
	borW = borW/2 - spacer/2;

	// MIDDLE SECTION: key bindings
	
	char keyName[BUFLEN];
	
	HWND staticBorderMiddleLeft = CreateWindowEx(
			xstyle, "Static", "", style | SS_ETCHEDFRAME,
			x, y, borW, borH,
			parent, NULL, NULL, NULL);

	x += borW + spacer;

	HWND staticBorderMiddleRight = CreateWindowEx(
			xstyle, "Static", "", style | SS_ETCHEDFRAME,
			x, y, borW, borH,
			parent, NULL, NULL, NULL);

	borH = spacer*5 + boxH*4;
	x = spacer*2; y += spacer;
	statW = 85; boxW = borW - spacer*3 - statW; 

	HWND keyIndicatorLabel = CreateWindowEx(
			xstyle, "Static", "Indicator ON/OFF", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

	x += statW + spacer;

	VKEY_TEXT(VKEY_INDICATOR_TOGGLE, keyName, BUFLEN);

	g_keyIndicatorControl = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", keyName, style,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

	x += boxW + spacer*3;

	HWND keySmallScreenshotLabel = CreateWindowEx(
			xstyle, "Static", "Small Screenshot", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

	x += statW + spacer;

	VKEY_TEXT(VKEY_HOOK_MODE_TOGGLE, keyName, BUFLEN);

	g_keySmallScreenshotControl = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", keyName, style,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

	x = spacer*2;
	y += boxH + spacer;

	HWND keyModeToggleLabel = CreateWindowEx(
			xstyle, "Static", "Hook Mode", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

	x += statW + spacer;

	VKEY_TEXT(VKEY_SMALL_SCREENSHOT, keyName, BUFLEN);

	g_keyModeToggleControl = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", keyName, style,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

	x += boxW + spacer*3;

	HWND keyScreenshotLabel = CreateWindowEx(
			xstyle, "Static", "Screenshot", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

	x += statW + spacer;

	VKEY_TEXT(VKEY_SCREENSHOT, keyName, BUFLEN);

	g_keyScreenshotControl = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", keyName, style,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

	x = spacer*2 + statW + boxW + spacer*4;
	y += boxH + spacer;

	HWND keyVideoToggleLabel = CreateWindowEx(
			xstyle, "Static", "Video ON/OFF", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

	x += statW + spacer;

	VKEY_TEXT(VKEY_VIDEO_CAPTURE, keyName, BUFLEN);

	g_keyVideoToggleControl = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", keyName, style,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

	y += spacer*2 + boxH;
	x = spacer;

	// BOTTOM SECTION: custom settings

	borW = WIN_WIDTH - spacer*3;

	HWND staticBorderBottom = CreateWindowEx(
			xstyle, "Static", "", style | SS_ETCHEDFRAME,
			x, y, borW, borH,
			parent, NULL, NULL, NULL);

	y += spacer;
	x += spacer;
	statW = 85;

	HWND customSettingsListLabel = CreateWindowEx(
			xstyle, "Static", "Custom settings: ", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

	x += statW + spacer;
	boxW = 100;

	style = WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL;

	g_customSettingsListControl = CreateWindowEx(
			xstyle, "ComboBox", "", style | WS_TABSTOP,
			x, y, boxW, boxH * 6,
			parent, NULL, NULL, NULL);

	x += boxW + spacer;
	style = WS_CHILD | WS_VISIBLE;
	butW = 40;

	g_customAddButtonControl = CreateWindowEx(
			xstyle, "Button", "New", style,
			x, y, butW, butH,
			parent, NULL, NULL, NULL);

	x += butW + spacer;
	butW = 52;

	g_customDropButtonControl = CreateWindowEx(
			xstyle, "Button", "Delete", style,
			x, y, butW, butH,
			parent, NULL, NULL, NULL);

	y += boxH + spacer;
	x = spacer*2;

	HWND customPatternLabel = CreateWindowEx(
			xstyle, "Static", "Pattern:", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

	x += statW + spacer;
	boxW = WIN_WIDTH - spacer*6 - statW;

	g_customPatternControl = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", "", style | WS_TABSTOP,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

	y += boxH + spacer;
	x = spacer*2;

	HWND customFrameRateLabel = CreateWindowEx(
			xstyle, "Static", "Frame rate:", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

	x += statW + spacer;
	boxW = 50;

	g_customFrameRateControl = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", "", style | WS_TABSTOP,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

	x += boxW + spacer;

	HWND frameRateLabel3 = CreateWindowEx(
			xstyle, "Static", "FPS", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

	y += boxH + spacer;
	x = spacer*2;

	HWND customFrameWeightLabel = CreateWindowEx(
			xstyle, "Static", "Frame weight:", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

	x += statW + spacer;

	g_customFrameWeightControl = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", "", style | WS_TABSTOP,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

	y += boxH + spacer*2;
	x = spacer;

	butW = 100; butH = 24;
	x = WIN_WIDTH - spacer*2 - butW;

	g_saveButtonControl = CreateWindowEx(
			xstyle, "Button", "Save and Apply", style,
			x, y, butW, butH,
			parent, NULL, NULL, NULL);

	butW = 60;
	x -= butW + spacer;

	g_restoreButtonControl = CreateWindowEx(
			xstyle, "Button", "Restore", style,
			x, y, butW, butH,
			parent, NULL, NULL, NULL);

	x = spacer;
	statW = WIN_WIDTH - spacer*4 - 160;

	g_statusTextControl = CreateWindowEx(
			xstyle, "Static", "", style,
			x, y+6, statW, statH,
			parent, NULL, NULL, NULL);

	// If any control wasn't created, return false
	if (captureDirectoryLabel == NULL || g_captureDirectoryControl == NULL ||
		frameRateLabel == NULL || g_frameRateControl == NULL ||
		frameRateLabel2 == NULL ||
		keyIndicatorLabel == NULL || g_keyIndicatorControl == NULL ||
		keyModeToggleLabel == NULL || g_keyModeToggleControl == NULL ||
		keySmallScreenshotLabel == NULL || g_keySmallScreenshotControl == NULL ||
		keyScreenshotLabel == NULL || g_keyScreenshotControl == NULL ||
		keyVideoToggleLabel == NULL || g_keyVideoToggleControl == NULL ||
		customSettingsListLabel == NULL || g_customSettingsListControl == NULL ||
		g_customAddButtonControl == NULL || g_customDropButtonControl == NULL ||
		customPatternLabel == NULL || g_customPatternControl == NULL ||
		customFrameRateLabel == NULL || g_customFrameRateControl == NULL ||
		frameRateLabel3 == NULL ||
		customFrameWeightLabel == NULL || g_customFrameWeightControl == NULL ||
		g_statusTextControl == NULL ||
		g_restoreButtonControl == NULL ||
		g_saveButtonControl == NULL)
		return false;

	// Get a handle to the stock font object
	hObj = GetStockObject(DEFAULT_GUI_FONT);

	SendMessage(captureDirectoryLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_captureDirectoryControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(frameRateLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_frameRateControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(frameRateLabel2, WM_SETFONT, (WPARAM)hObj, true);

	SendMessage(keyIndicatorLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_keyIndicatorControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(keyModeToggleLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_keyModeToggleControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(keySmallScreenshotLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_keySmallScreenshotControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(keyScreenshotLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_keyScreenshotControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(keyVideoToggleLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_keyVideoToggleControl, WM_SETFONT, (WPARAM)hObj, true);

	SendMessage(customSettingsListLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_customSettingsListControl, WM_SETFONT, (WPARAM)hObj, true);

	SendMessage(g_customAddButtonControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_customDropButtonControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(customPatternLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_customPatternControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(customFrameRateLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_customFrameRateControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(frameRateLabel3, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(customFrameWeightLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_customFrameWeightControl, WM_SETFONT, (WPARAM)hObj, true);

	SendMessage(g_statusTextControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_restoreButtonControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_saveButtonControl, WM_SETFONT, (WPARAM)hObj, true);

	return true;
}

