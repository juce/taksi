#define DIRECTINPUT_VERSION 0x0800
#include "dinput.h"

#define _COMPILING_MYDLL
#include "mydll.h"

#include "config.h"
#include "shared.h"
#include "log.h"

#define SAFE_RELEASE(p) if (p) { if (p->Release() == 0) p = NULL; }

typedef HRESULT (WINAPI *DIRECTINPUT8CREATE)(HINSTANCE, DWORD, REFIID, LPVOID, LPUNKNOWN);

HMODULE hDI = NULL;
IDirectInput8* g_lpDI = NULL;
IDirectInputDevice8* g_lpDIDevice = NULL;

DIRECTINPUT8CREATE g_DirectInput8Create = NULL;

// key states (for DirectInput)
BOOL bSS_DOWN = false;
BOOL bSmallSS_DOWN = false;
BOOL bHM_DOWN = false;
BOOL bI_DOWN = false;
BOOL bVC_DOWN = false;

// externals
extern HINSTANCE hInst;
extern MYSTATE g_mystate;
extern BOOL   sg_bHookInstalled;

////////////////////////////////////////////
// FUNCTIONS
////////////////////////////////////////////

void GetDirectInputCreator()
{
	g_DirectInput8Create = (DIRECTINPUT8CREATE)GetProcAddress(hDI, "DirectInput8Create");
	if (!g_DirectInput8Create) 
	{
		Log("GetDirectInputCreator: lookup for DirectInput8Create failed.");
	}
}

void ProcessDirectInput(TAXI_CONFIG* config)
{
	/* process keyboard input using DirectInput */
	if (hDI)
	{
		char buffer[256]; 
		if (g_lpDIDevice)
		{
			if (SUCCEEDED(g_lpDIDevice->GetDeviceState(sizeof(buffer),(LPVOID)&buffer)))
			{
				#define KEYDOWN(name, key) (name[key] & 0x80) 

				if (KEYDOWN(buffer, MapVirtualKey(config->vKeyScreenShot, 0))) bSS_DOWN = true;
				else {
					if (bSS_DOWN)
					{
						bSS_DOWN = false;
						Log("ProcessDirectInput: VKEY_SCREENSHOT pressed.");
						g_mystate.bMakeScreenShot = true;
					}
				}

				if (KEYDOWN(buffer, MapVirtualKey(config->vKeySmallScreenShot, 0))) bSmallSS_DOWN = true;
				else {
					if (bSmallSS_DOWN)
					{
						bSmallSS_DOWN = false;
						Log("ProcessDirectInput: VKEY_SMALL_SCREENSHOT pressed.");
						g_mystate.bMakeSmallScreenShot = true;
					}
				}

				if (KEYDOWN(buffer, MapVirtualKey(config->vKeyHookMode, 0))) bHM_DOWN = true;
				else {
					if (bHM_DOWN)
					{
						bHM_DOWN = false;
						Log("ProcessDirectInput: VKEY_HOOK_MODE_TOGGLE pressed.");
						if (!g_mystate.bNowRecording) // don't switch during video capture.
						{
							if (sg_bHookInstalled)
							{
								Log("ProcessDirectInput: Signaling to unhook GetMsgProc.");
								if (UninstallTheHook())
								{
									Log("ProcessDirectInput: GetMsgProc unhooked.");
									// change indicator to "green"
								}
							}
							else
							{
								Log("ProcessDirectInput: Signaling to install GetMsgProc hook.");
								if (InstallTheHook())
								{
									Log("ProcessDirectInput: GetMsgProc successfully hooked.");
									// change indicator to "blue"
								}
							}
						}
					}
				}

				if (KEYDOWN(buffer, MapVirtualKey(config->vKeyIndicator, 0))) bI_DOWN = true;
				else {
					if (bI_DOWN)
					{
						bI_DOWN = false;
						Log("ProcessDirectInput: VKEY_INDICATOR_TOGGLE pressed.");
						g_mystate.bIndicate = !g_mystate.bIndicate;
					}
				}

				if (KEYDOWN(buffer, MapVirtualKey(config->vKeyVideoCapture, 0))) bVC_DOWN = true;
				else {
					if (bVC_DOWN)
					{
						bVC_DOWN = false;
						Log("ProcessDirectInput: VKEY_VIDEO_CAPTURE_TOGGLE pressed.");
						// toggle video capturing mode
						switch (g_mystate.bNowRecording)
						{
							case FALSE:
								// change indicator to "red"
								g_mystate.bStartRecording = true;
								break;

							case TRUE:
								// change indicator from "red" to previous one
								g_mystate.bStopRecording = true;
								break;
						}
					}
				}

			}
		}
		else
		{
			// Create device
			if (!g_lpDIDevice) 
			{
				if (FAILED(g_DirectInput8Create(hInst, DIRECTINPUT_VERSION, 
							IID_IDirectInput8, (void**)&g_lpDI, NULL)))
				{
					// DirectInput not available; take appropriate action 
					Log("DirectInput8Create failed.");
				}
				if (g_lpDI)
				{
					if (FAILED(g_lpDI->CreateDevice(GUID_SysKeyboard, &g_lpDIDevice, NULL)))
					{
						Log("JucePresent: g_lpDI->CreateDevice() failed.");
					}
				}
				if (g_lpDIDevice)
				{
					if (FAILED(g_lpDIDevice->SetDataFormat(&c_dfDIKeyboard)))
					{
						Log("JucePresent: g_lpDIDevice->SetDataFormat() failed.");
					} 
				}
			}
			// Acquire device
			if (g_lpDIDevice) g_lpDIDevice->Acquire();
		}
	}
}

void CloseDirectInput()
{
	if (g_lpDIDevice) g_lpDIDevice->Unacquire();
	SAFE_RELEASE(g_lpDIDevice);
	SAFE_RELEASE(g_lpDI);
}

