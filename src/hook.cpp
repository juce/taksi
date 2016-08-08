/* hook */
/* Version 1.1 (with Win32 GUI) by Juce. */

#include <windows.h>
#include <windef.h>
#include <string.h>
#include <stdio.h>

#include "mydll.h"
#include "shared.h"
#include "win32gui.h"
#include "config.h"
#include "hook.h"

FILE* log = NULL;

// Program Configuration
static TAXI_CONFIG config = {
	DEFAULT_DEBUG,
	DEFAULT_CAPTURE_DIR,
	DEFAULT_TARGET_FRAME_RATE,
	DEFAULT_VKEY_INDICATOR, DEFAULT_VKEY_HOOK_MODE, 
	DEFAULT_VKEY_SMALL_SCREENSHOT, DEFAULT_VKEY_SCREENSHOT,
	DEFAULT_VKEY_VIDEO_CAPTURE,
	DEFAULT_USE_DIRECT_INPUT,
	DEFAULT_STARTUP_MODE_SYSTEM_WIDE,
	DEFAULT_FULL_SIZE_VIDEO,
	DEFAULT_CUSTOM_LIST
};

BOOL dropDownSelecting = false;
TAXI_CUSTOM_CONFIG* g_curCfg = NULL;

// function prototypes
void ApplySettings(void);
void ForceReconf(void);
void RestoreSettings(void);
void InitializeCustomConfigs(void);

/**
 * Increase the reconf-counter thus telling all the mapped DLLs that
 * they need to reconfigure themselves.
 */
void ForceReconf(void)
{
	ZeroMemory(config.captureDir, BUFLEN);
	SendMessage(g_captureDirectoryControl, WM_GETTEXT, (WPARAM)BUFLEN, (LPARAM)config.captureDir);
	// make sure it ends with backslash.
	if (config.captureDir[lstrlen(config.captureDir)-1] != '\\') 
	{
		lstrcat(config.captureDir, "\\");
		SendMessage(g_captureDirectoryControl, WM_SETTEXT, 0, (LPARAM)config.captureDir); 
	}

	int frate = 0;
	char buf[BUFLEN];
	ZeroMemory(buf, BUFLEN); 
	SendMessage(g_frameRateControl, WM_GETTEXT, (WPARAM)BUFLEN, (LPARAM)buf);
	if (sscanf(buf, "%d", &frate) == 1 && frate > 0) 
	{
		config.targetFrameRate = frate;
	}
	else
	{
		config.targetFrameRate = GetTargetFrameRate();
		sprintf(buf, "%d", config.targetFrameRate);
		SendMessage(g_frameRateControl, WM_SETTEXT, 0, (LPARAM)buf); 
	}

	// Apply new settings
	ApplySettings();

	// Save
	if (WriteConfig(&config))
	{
		SetReconfCounter(GetReconfCounter() + 1);
	}
}

/**
 * Apply settings from config
 */
void ApplySettings(void)
{
	char buf[BUFLEN];

	// apply general settings
	SetDebug(config.debug);
	if (lstrlen(config.captureDir)==0)
	{
		// If capture directory is still unknown at this point
		// set it to the current directory then.
		ZeroMemory(config.captureDir, BUFLEN);
		GetModuleFileName(NULL, config.captureDir, BUFLEN);
		// strip off file name
		char* p = config.captureDir + lstrlen(config.captureDir);
		while (p > config.captureDir && *p != '\\') p--;
		p[1] = '\0';
	}
	SetCaptureDir(config.captureDir);

	SendMessage(g_captureDirectoryControl, WM_SETTEXT, 0, (LPARAM)config.captureDir);
	SetTargetFrameRate(config.targetFrameRate);
	ZeroMemory(buf, BUFLEN);
	sprintf(buf,"%d",config.targetFrameRate);
	SendMessage(g_frameRateControl, WM_SETTEXT, 0, (LPARAM)buf);

	// apply keys
	SetKey(VKEY_INDICATOR_TOGGLE, config.vKeyIndicator);
	VKEY_TEXT(VKEY_INDICATOR_TOGGLE, buf, BUFLEN); 
	SendMessage(g_keyIndicatorControl, WM_SETTEXT, 0, (LPARAM)buf);
	SetKey(VKEY_HOOK_MODE_TOGGLE, config.vKeyHookMode);
	VKEY_TEXT(VKEY_HOOK_MODE_TOGGLE, buf, BUFLEN); 
	SendMessage(g_keyModeToggleControl, WM_SETTEXT, 0, (LPARAM)buf);
	SetKey(VKEY_SMALL_SCREENSHOT, config.vKeySmallScreenShot);
	VKEY_TEXT(VKEY_SMALL_SCREENSHOT, buf, BUFLEN); 
	SendMessage(g_keySmallScreenshotControl, WM_SETTEXT, 0, (LPARAM)buf);
	SetKey(VKEY_SCREENSHOT, config.vKeyScreenShot);
	VKEY_TEXT(VKEY_SCREENSHOT, buf, BUFLEN); 
	SendMessage(g_keyScreenshotControl, WM_SETTEXT, 0, (LPARAM)buf);
	SetKey(VKEY_VIDEO_CAPTURE, config.vKeyVideoCapture);
	VKEY_TEXT(VKEY_VIDEO_CAPTURE, buf, BUFLEN); 
	SendMessage(g_keyVideoToggleControl, WM_SETTEXT, 0, (LPARAM)buf);

	// apply useDirectInput
	SetUseDirectInput(config.useDirectInput);
	// apply startupModeSystemWide
	SetStartupModeSystemWide(config.startupModeSystemWide);
	// apply fullSizeVideo
	SetFullSizeVideo(config.fullSizeVideo);

	// apply custom configs
	SendMessage(g_customSettingsListControl, CB_RESETCONTENT, 0, 0);
	SendMessage(g_customPatternControl, WM_SETTEXT, 0, (LPARAM)"");
	SendMessage(g_customFrameRateControl, WM_SETTEXT, 0, (LPARAM)"");
	SendMessage(g_customFrameWeightControl, WM_SETTEXT, 0, (LPARAM)"");
	InitializeCustomConfigs();
}

/**
 * Restores last saved settings.
 */
void RestoreSettings(void)
{
	g_curCfg = NULL;
	FreeCustomConfigs(&config);
	// read optional configuration file
	ReadConfig(&config);
	// check for "silent" mode
	if (config.debug == 0)
	{
		if (log != NULL) fclose(log);
		DeleteFile(MAINLOGFILE);
		log = NULL;
	}
	ApplySettings();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int home, away, timecode;
	char buf[BUFLEN];

	switch(uMsg)
	{
		case WM_DESTROY:
			// Set flag, so that mydll knows to unhook device methods
			SetUnhookFlag(true);
			SetExiting(true);

			// Free memory taken by custom configs
			FreeCustomConfigs(&config);

			// close LOG file
			if (log != NULL) fclose(log);

			// Uninstall the hook
			UninstallTheHook();
			// Exit the application when the window closes
			PostQuitMessage(1);
			return true;

		case WM_APP_REHOOK:
			// Re-install system-wide hook
			LOG(log, (log, "Received message to re-hook GetMsgProc\n"));
			if (InstallTheHook())
			{
				LOG(log, (log, "GetMsgProc successfully re-hooked.\n"));
			}
			break;

		case WM_COMMAND:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				if ((HWND)lParam == g_saveButtonControl)
				{
					// update current custom config
					if (g_curCfg != NULL)
					{
						SendMessage(g_customPatternControl,WM_GETTEXT,(WPARAM)BUFLEN,(LPARAM)g_curCfg->pattern);
						int value = 0;
						ZeroMemory(buf, BUFLEN);
						SendMessage(g_customFrameRateControl,WM_GETTEXT,(WPARAM)BUFLEN,(LPARAM)buf);
						if (sscanf(buf,"%d",&value)==1 && value >= 0) g_curCfg->frameRate = value;
						float dvalue = 0.0f;
						ZeroMemory(buf, BUFLEN);
						SendMessage(g_customFrameWeightControl,WM_GETTEXT,(WPARAM)BUFLEN,(LPARAM)buf);
						if (sscanf(buf,"%f",&dvalue)==1 && dvalue >= 0.0) g_curCfg->frameWeight = dvalue;
					}

					ForceReconf();
					// modify status text
					SendMessage(g_statusTextControl, WM_SETTEXT, 0, (LPARAM)"SAVED");
				}
				else if ((HWND)lParam == g_restoreButtonControl)
				{
					RestoreSettings();
					// modify status text
					SendMessage(g_statusTextControl, WM_SETTEXT, 0, (LPARAM)"RESTORED");
				}
				else if ((HWND)lParam == g_customAddButtonControl)
				{
					// update current custom config
					if (g_curCfg != NULL)
					{
						SendMessage(g_customPatternControl,WM_GETTEXT,(WPARAM)BUFLEN,(LPARAM)g_curCfg->pattern);
						int value = 0;
						ZeroMemory(buf, BUFLEN);
						SendMessage(g_customFrameRateControl,WM_GETTEXT,(WPARAM)BUFLEN,(LPARAM)buf);
						if (sscanf(buf,"%d",&value)==1 && value >= 0) g_curCfg->frameRate = value;
						float dvalue = 0.0f;
						ZeroMemory(buf, BUFLEN);
						SendMessage(g_customFrameWeightControl,WM_GETTEXT,(WPARAM)BUFLEN,(LPARAM)buf);
						if (sscanf(buf,"%f",&dvalue)==1 && dvalue >= 0.0) g_curCfg->frameWeight = dvalue;
					}

					g_curCfg = NULL;

					// clear out controls
					SendMessage(g_customSettingsListControl,WM_SETTEXT,(WPARAM)0,(LPARAM)"");
					SendMessage(g_customPatternControl,WM_SETTEXT,(WPARAM)0,(LPARAM)"");
					SendMessage(g_customFrameRateControl,WM_SETTEXT,(WPARAM)0,(LPARAM)"");
					SendMessage(g_customFrameWeightControl,WM_SETTEXT,(WPARAM)0,(LPARAM)"");

					// set focus on appId comboBox
					SendMessage(g_customSettingsListControl,WM_SETFOCUS, 0, 0);
				}
				else if ((HWND)lParam == g_customDropButtonControl)
				{
					// remove current custom config
					if (g_curCfg != NULL) 
					{
						int idx = (int)SendMessage(g_customSettingsListControl, CB_FINDSTRING, BUFLEN, (LPARAM)g_curCfg->appId);

						//ZeroMemory(buf, BUFLEN);
						//sprintf(buf,"idx = %d", idx);
						//SendMessage(g_statusTextControl, WM_SETTEXT, 0, (LPARAM)buf);

						if (idx != -1)
						{
							SendMessage(g_customSettingsListControl, CB_DELETESTRING, idx, 0);
							DeleteCustomConfig(&config, g_curCfg->appId);
						}
						SendMessage(g_customSettingsListControl, CB_SETCURSEL, 0, 0);
						ZeroMemory(buf, BUFLEN);
						SendMessage(g_customSettingsListControl, CB_GETLBTEXT, 0, (LPARAM)buf);
						g_curCfg = LookupExistingCustomConfig(&config, buf);

						if (g_curCfg != NULL)
						{
							// update custom controls
							SendMessage(g_customPatternControl,WM_SETTEXT,(WPARAM)0,(LPARAM)g_curCfg->pattern);
							ZeroMemory(buf, BUFLEN);
							sprintf(buf, "%d", g_curCfg->frameRate);
							SendMessage(g_customFrameRateControl,WM_SETTEXT,(WPARAM)0,(LPARAM)buf);
							ZeroMemory(buf, BUFLEN);
							sprintf(buf, "%1.4f", g_curCfg->frameWeight);
							SendMessage(g_customFrameWeightControl,WM_SETTEXT,(WPARAM)0,(LPARAM)buf);
						}
						else
						{
							// clear out controls
							SendMessage(g_customPatternControl,WM_SETTEXT,(WPARAM)0,(LPARAM)"");
							SendMessage(g_customFrameRateControl,WM_SETTEXT,(WPARAM)0,(LPARAM)"");
							SendMessage(g_customFrameWeightControl,WM_SETTEXT,(WPARAM)0,(LPARAM)"");
						}
					}
				}
			}
			else if (HIWORD(wParam) == CBN_KILLFOCUS)
			{
				ZeroMemory(buf, BUFLEN);
				SendMessage(g_customSettingsListControl, WM_GETTEXT, (WPARAM)BUFLEN, (LPARAM)buf);
				if (lstrlen(buf)>0)
				{
					if (lstrcmp(buf, g_curCfg->appId)!=0)
					{
						if (g_curCfg != NULL)
						{
							// find list item with old appId
							int idx = SendMessage(g_customSettingsListControl, CB_FINDSTRING, BUFLEN, (LPARAM)g_curCfg->appId);
							// delete this item, if found
							if (idx != -1) SendMessage(g_customSettingsListControl, CB_DELETESTRING, idx, 0);
							// change appId
							strncpy(g_curCfg->appId, buf, BUFLEN-1);
							// add a new list item
							SendMessage(g_customSettingsListControl, CB_ADDSTRING, 0, (LPARAM)g_curCfg->appId);
						}
						else
						{
							// we have a new custom config
							g_curCfg = LookupCustomConfig(&config, buf);
							// add a new list item
							SendMessage(g_customSettingsListControl, CB_ADDSTRING, 0, (LPARAM)g_curCfg->appId);
						}
					}
				}
				else
				{
					// cannot use empty string. Restore the old one, if known.
					if (g_curCfg != NULL)
					{
						// restore previous text
						SendMessage(g_customSettingsListControl, WM_SETTEXT, 0, (LPARAM)g_curCfg->appId);
					}
				}
			}
			else if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				// update previously selected custom config
				if (g_curCfg != NULL)
				{
					SendMessage(g_customPatternControl,WM_GETTEXT,(WPARAM)BUFLEN,(LPARAM)g_curCfg->pattern);
					int value = 0;
					ZeroMemory(buf, BUFLEN);
					SendMessage(g_customFrameRateControl,WM_GETTEXT,(WPARAM)BUFLEN,(LPARAM)buf);
					if (sscanf(buf,"%d",&value)==1 && value >= 0) g_curCfg->frameRate = value;
					float dvalue = 0.0f;
					ZeroMemory(buf, BUFLEN);
					SendMessage(g_customFrameWeightControl,WM_GETTEXT,(WPARAM)BUFLEN,(LPARAM)buf);
					if (sscanf(buf,"%f",&dvalue)==1 && dvalue >= 0.0) g_curCfg->frameWeight = dvalue;
				}

				// user selected different custom setting in drop-down
				int idx = (int)SendMessage(g_customSettingsListControl, CB_GETCURSEL, 0, 0);

				ZeroMemory(buf, BUFLEN);
				SendMessage(g_customSettingsListControl, CB_GETLBTEXT, idx, (LPARAM)buf);
				TAXI_CUSTOM_CONFIG* cfg = LookupExistingCustomConfig(&config, buf);
				if (cfg == NULL) break;

				// update custom controls
				dropDownSelecting = true;

				SendMessage(g_customPatternControl,WM_SETTEXT,(WPARAM)0,(LPARAM)cfg->pattern);
				ZeroMemory(buf, BUFLEN);
				sprintf(buf, "%d", cfg->frameRate);
				SendMessage(g_customFrameRateControl,WM_SETTEXT,(WPARAM)0,(LPARAM)buf);
				ZeroMemory(buf, BUFLEN);
				sprintf(buf, "%1.4f", cfg->frameWeight);
				SendMessage(g_customFrameWeightControl,WM_SETTEXT,(WPARAM)0,(LPARAM)buf);

				dropDownSelecting = false;

				// remember new current config
				g_curCfg = cfg;
			}
			else if (HIWORD(wParam) == EN_CHANGE)
			{
				HWND control = (HWND)lParam;
				if (!dropDownSelecting)
				{
					// modify status text
					SendMessage(g_statusTextControl, WM_SETTEXT, 0, (LPARAM)"CHANGES MADE");
				}
			}
			else if (HIWORD(wParam) == CBN_EDITCHANGE)
			{
				HWND control = (HWND)lParam;
				// modify status text
				SendMessage(g_statusTextControl, WM_SETTEXT, 0, (LPARAM)"CHANGES MADE");
			}
			break;

		case WM_APP_KEYDEF:
			HWND target = (HWND)lParam;
			ZeroMemory(buf, BUFLEN);
			GetKeyNameText(MapVirtualKey(wParam, 0) << 16, buf, BUFLEN);
			SendMessage(target, WM_SETTEXT, 0, (LPARAM)buf);
			SendMessage(g_statusTextControl, WM_SETTEXT, 0, (LPARAM)"CHANGES MADE");
			// update config
			if (target == g_keyIndicatorControl) config.vKeyIndicator = (WORD)wParam;
			else if (target == g_keyModeToggleControl) config.vKeyHookMode = (WORD)wParam;
			else if (target == g_keySmallScreenshotControl) config.vKeySmallScreenShot = (WORD)wParam;
			else if (target == g_keyScreenshotControl) config.vKeyScreenShot = (WORD)wParam;
			else if (target == g_keyVideoToggleControl) config.vKeyVideoCapture = (WORD)wParam;
			break;
	}
	return DefWindowProc(hwnd,uMsg,wParam,lParam);
}

bool InitApp(HINSTANCE hInstance, LPSTR lpCmdLine)
{
	WNDCLASSEX wcx;

	// cbSize - the size of the structure.
	wcx.cbSize = sizeof(WNDCLASSEX);
	wcx.style = CS_HREDRAW | CS_VREDRAW;
	wcx.lpfnWndProc = (WNDPROC)WindowProc;
	wcx.cbClsExtra = 0;
	wcx.cbWndExtra = 0;
	wcx.hInstance = hInstance;
	wcx.hIcon = LoadIcon(NULL,IDI_APPLICATION);
	wcx.hCursor = LoadCursor(NULL,IDC_ARROW);
	wcx.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wcx.lpszMenuName = NULL;
	wcx.lpszClassName = "TAXICLS";
	wcx.hIconSm = NULL;

	// Register the class with Windows
	if(!RegisterClassEx(&wcx))
		return false;

	// open log
	log = fopen(MAINLOGFILE, "wt");

	// read optional configuration file
	ReadConfig(&config);
	// check for "silent" mode
	if (config.debug == 0)
	{
		if (log != NULL) fclose(log);
		DeleteFile(MAINLOGFILE);
		log = NULL;
	}

	// clear exiting flag
	SetExiting(false);

	return true;
}

HWND BuildWindow(int nCmdShow)
{
	DWORD style, xstyle;
	HWND retval;

	style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	xstyle = WS_EX_LEFT;

	retval = CreateWindowEx(xstyle,
        "TAXICLS",      // class name
        TAXI_WINDOW_TITLE, // title for our window (appears in the titlebar)
        style,
        CW_USEDEFAULT,  // initial x coordinate
        CW_USEDEFAULT,  // initial y coordinate
        WIN_WIDTH, WIN_HEIGHT,   // width and height of the window
        NULL,           // no parent window.
        NULL,           // no menu
        NULL,           // no creator
        NULL);          // no extra data

	if (retval == NULL) return NULL;  // BAD.

	ShowWindow(retval,nCmdShow);  // Show the window
	return retval; // return its handle for future use.
}

void InitializeCustomConfigs(void)
{
	TAXI_CUSTOM_CONFIG* cfg = config.customList;

	while (cfg != NULL)
	{
        SendMessage(g_customSettingsListControl,CB_INSERTSTRING,(WPARAM)0, (LPARAM)cfg->appId);

		if (cfg->next == NULL)
		{
			// Pre-select iteam
			SendMessage(g_customSettingsListControl,CB_SETCURSEL,(WPARAM)0,(LPARAM)0);
			SendMessage(g_customPatternControl,WM_SETTEXT,(WPARAM)0,(LPARAM)cfg->pattern);
			char buf[BUFLEN];
			ZeroMemory(buf, BUFLEN);
			sprintf(buf, "%d", cfg->frameRate);
			SendMessage(g_customFrameRateControl,WM_SETTEXT,(WPARAM)0,(LPARAM)buf);
			ZeroMemory(buf, BUFLEN);
			sprintf(buf, "%1.4f", cfg->frameWeight);
			SendMessage(g_customFrameWeightControl,WM_SETTEXT,(WPARAM)0,(LPARAM)buf);

			// remember current config
			g_curCfg = cfg;
		}

		cfg = cfg->next;
	}

	// clear status text
	SendMessage(g_statusTextControl, WM_SETTEXT, (WPARAM)0, (LPARAM)"");
}

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	MSG msg; int retval;

 	if(InitApp(hInstance, lpCmdLine) == false)
		return 0;

	HWND hWnd = BuildWindow(nCmdShow);
	if(hWnd == NULL)
		return 0;

	// build GUI
	if (!BuildControls(hWnd))
		return 0;

	// Initialize all controls
	ApplySettings();

	// show credits
	char buf[BUFLEN];
	ZeroMemory(buf, BUFLEN);
	strncpy(buf, CREDITS, BUFLEN-1);
	SendMessage(g_statusTextControl, WM_SETTEXT, 0, (LPARAM)buf);

	// Clear the flag, so that mydll hooks on device methods
	SetUnhookFlag(false);

	// Set HWND for later use
	SetServerWnd(hWnd);

	// Install the hook
	InstallTheHook();

	while((retval = GetMessage(&msg,NULL,0,0)) != 0)
	{
		// capture key-def events
		if ((msg.hwnd == g_keyIndicatorControl || msg.hwnd == g_keyModeToggleControl ||
		 	 msg.hwnd == g_keySmallScreenshotControl || msg.hwnd == g_keyScreenshotControl ||
			 msg.hwnd == g_keyVideoToggleControl) &&
		    (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN))
		{
			PostMessage(hWnd, WM_APP_KEYDEF, msg.wParam, (LPARAM)msg.hwnd);
			continue;
		}

		if(retval == -1)
			return 0;	// an error occured while getting a message

		if (!IsDialogMessage(hWnd, &msg)) // need to call this to make WS_TABSTOP work
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return 0;
}

