#include <windows.h>
#include "d3d9types.h"
#include "d3d9.h"

#define _COMPILING_MYDLL
#include "mydll.h"

#include "config.h"
#include "log.h"
#include "dinp.h"
#include "shared.h"
#include "util.h"
#include "heap.h"
#include "graphx_dx9.h"

extern HINSTANCE hInst;
HMODULE hD3D9 = NULL;
extern HMODULE hDI;

#define RESET_REL_POS 10
#define PRESENT_REL_POS 11
#define RELEASE_REL_POS -4

#define ADDREF_POS 1
#define RELEASE_POS 2
#define RESET_POS 16
#define PRESENT_POS 17

// external SHARED SEGEMENT variables
extern HWND   sg_hServerWnd;
extern HHOOK  sg_hGetMsgHook;
extern BOOL   sg_bHookInstalled;
extern BOOL   sg_bExiting;
extern BOOL   sg_bIgnoreOverhead;
extern BOOL   sg_bUnhookDevice;
extern DWORD  sg_reconfCounter;

// global flag inidicating whether logging
// and config params were initialized.
extern BOOL g_initDone;

// window handle of the process
extern HWND hProcWnd;

// Thread-specific keyboard hook
extern HHOOK g_hKeyboardHook;
// information about process
extern MYINFO g_myinfo;
// information about current state
extern MYSTATE g_mystate;
extern DWORD g_reconfCounter;
extern TAXI_CONFIG g_config;
extern DXPOINTERS g_dxInfo;

BOOL g_needToUnloadLibrary = FALSE;

// frame rate calculators
extern float g_frameRate;
extern DWORD g_skipped;
extern DWORD g_lastOverhead;
extern DWORD g_freq;
extern DWORD g_customFrameRate;
extern float g_customFrameWeight;

// custom config (if any exists for current process)
static TAXI_CUSTOM_CONFIG* customConfig = NULL;

struct CUSTOMVERTEX { 
	FLOAT x,y,z,w;
	DWORD color;
};

static HANDLE procHeap = NULL;
// name of the process-specific log
static char logName[BUFLEN];

static IDirect3DSurface9* g_backBuffer;
static D3DFORMAT g_bbFormat;
static BOOL g_bGotFormat = false;
static int bpp = 0;
extern HANDLE videoFile;

// back-buffer dimensions
extern UINT g_bbWidth;
extern UINT g_bbHeight;

// buffer to keep current frame 
static BYTE* g_rgbBuf = NULL;
static INT g_rgbSize = 0;
static INT g_Pitch = 0;

// hook-helping code holders
BYTE g_codeFragment_p9[5] = {0, 0, 0, 0, 0};
BYTE g_codeFragment_r9[5] = {0, 0, 0, 0, 0};
BYTE g_jmp_p9[5] = {0, 0, 0, 0, 0};
BYTE g_jmp_r9[5] = {0, 0, 0, 0, 0};
DWORD g_savedProtection_p9 = 0;
DWORD g_savedProtection_r9 = 0;

// video capture parameters
extern ALGORITHM_TYPE g_algorithm;

typedef DWORD   (STDMETHODCALLTYPE *GETPROCESSHEAPS)(DWORD, PHANDLE);

static ULONG g_refCount = 1;
static ULONG g_myCount = 0;

static IDirect3DDevice9* g_dev = NULL;
static IDirect3DVertexBuffer9* g_pVBidle = NULL;
static IDirect3DVertexBuffer9* g_pVBswHook = NULL;
static IDirect3DVertexBuffer9* g_pVBrec = NULL;
static IDirect3DVertexBuffer9* g_pVBborder = NULL;
static IDirect3DStateBlock9* g_pSavedStateBlock = 0L;
static IDirect3DStateBlock9* g_pDrawTextStateBlock = 0L;

/* IDirect3DDevice9 method-types */
typedef ULONG   (STDMETHODCALLTYPE *ADDREF9)(IUnknown* self);
typedef ULONG   (STDMETHODCALLTYPE *RELEASE9)(IUnknown* self);
typedef HRESULT (STDMETHODCALLTYPE *RESET9)(IDirect3DDevice9* self, LPVOID);
typedef HRESULT (STDMETHODCALLTYPE *PRESENT9)(IDirect3DDevice9* self, CONST RECT*, CONST RECT*, HWND, LPVOID);

/* function pointers */
static ADDREF9 g_D3D9_AddRef = NULL;
static RELEASE9 g_D3D9_Release = NULL;
static RESET9   g_D3D9_Reset = NULL;
static PRESENT9 g_D3D9_Present = NULL;

static DWORD* pAddRefPtr = NULL;
static DWORD* pReleasePtr = NULL;
 
#define IWIDTH 16.0f
#define IHEIGHT 16.0f
#define INDX 8.0f
#define INDY 8.0f

static CUSTOMVERTEX g_VertIdle[] =
{
    {INDX,        INDY,         0.0f, 1.0f, 0xff88ff00 }, // x, y, z, rhw, color
    {INDX,        INDY+IHEIGHT, 0.0f, 1.0f, 0xff88ff00 }, // x, y, z, rhw, color
    {INDX+IWIDTH, INDY,         0.0f, 1.0f, 0xff88ff00 }, // x, y, z, rhw, color
    {INDX+IWIDTH, INDY+IHEIGHT, 0.0f, 1.0f, 0xff88ff00 }, // x, y, z, rhw, color
};
static CUSTOMVERTEX g_VertSWHook[] =
{
    {INDX,        INDY,         0.0f, 1.0f, 0xff4488ff }, // x, y, z, rhw, color
    {INDX,        INDY+IHEIGHT, 0.0f, 1.0f, 0xff4488ff }, // x, y, z, rhw, color
    {INDX+IWIDTH, INDY,         0.0f, 1.0f, 0xff4488ff }, // x, y, z, rhw, color
    {INDX+IWIDTH, INDY+IHEIGHT, 0.0f, 1.0f, 0xff4488ff }, // x, y, z, rhw, color
};
static CUSTOMVERTEX g_VertRec[] =
{
    {INDX,        INDY,         0.0f, 1.0f, 0xffff4400 }, // x, y, z, rhw, color
    {INDX,        INDY+IHEIGHT, 0.0f, 1.0f, 0xffff4400 }, // x, y, z, rhw, color
    {INDX+IWIDTH, INDY,         0.0f, 1.0f, 0xffff4400 }, // x, y, z, rhw, color
    {INDX+IWIDTH, INDY+IHEIGHT, 0.0f, 1.0f, 0xffff4400 }, // x, y, z, rhw, color
};
static CUSTOMVERTEX g_VertBorder[] =
{
    {INDX,        INDY,         0.0f, 1.0f, 0xff000000 }, // x, y, z, rhw, color
    {INDX,        INDY+IHEIGHT, 0.0f, 1.0f, 0xff000000 }, // x, y, z, rhw, color
    {INDX+IWIDTH, INDY+IHEIGHT, 0.0f, 1.0f, 0xff000000 }, // x, y, z, rhw, color
    {INDX+IWIDTH, INDY,         0.0f, 1.0f, 0xff000000 }, // x, y, z, rhw, color
    {INDX,        INDY,         0.0f, 1.0f, 0xff000000 }, // x, y, z, rhw, color
};

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE)

static HRESULT LoadSurfaceFromFile(IDirect3DSurface9* surf, char* filename);
static HRESULT SaveSurfaceToFile(IDirect3DSurface9* surf, char* filename, RECT* rect);
static HRESULT InvalidateDeviceObjects(IDirect3DDevice9* dev, BOOL detaching);
static HRESULT DeleteDeviceObjects(IDirect3DDevice9* dev);
static void DrawIndicator(LPVOID);
static HRESULT JuceGetRGB(IDirect3DDevice9* d3dDevice, 
IDirect3DSurface9* srcSurf, RECT* rect, D3DFORMAT format, BYTE* destBuf, INT destPitch);
static HRESULT JuceGetHalfSizeRGB(IDirect3DDevice9* d3dDevice, 
IDirect3DSurface9* srcSurf, RECT* rect, D3DFORMAT format, BYTE* destBuf, INT destPitch);

EXTERN_C ULONG _declspec(dllexport) STDMETHODCALLTYPE JuceAddRef9(
IUnknown* self);
EXTERN_C ULONG _declspec(dllexport) STDMETHODCALLTYPE JuceRelease9(
IUnknown* self);
EXTERN_C HRESULT _declspec(dllexport) STDMETHODCALLTYPE JuceReset9(
IDirect3DDevice9* self, LPVOID params);
EXTERN_C HRESULT _declspec(dllexport) STDMETHODCALLTYPE JucePresent9(
IDirect3DDevice9* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused);

static void VerifyHooks(IDirect3DDevice9* dev);
void InitHooksD9();
static void HookDeviceMethods9(HMODULE hMod);

static HWND GetProcWnd(IDirect3DDevice9* d3dDevice);
static void GetBackBufferInfo(IDirect3DDevice9* d3dDevice);
static void MakeScreenShot(IDirect3DDevice9* d3dDevice, BYTE* rgbBuf, SIZE_T size, INT pitch);
static void MakeSmallScreenShot(IDirect3DDevice9* d3dDevice, BYTE* rgbBuf, SIZE_T size, INT pitch);
static void GetFrame(IDirect3DDevice9* d3dDevice, BYTE* rgbBuf, SIZE_T size, INT pitch);
static HRESULT InitVB(IDirect3DDevice9* dev);
static HRESULT RestoreDeviceObjects(IDirect3DDevice9* dev);

// function pointer for grabbing RGB-data
typedef HRESULT (*PFNGETPIXELSPROC)(IDirect3DDevice9*, IDirect3DSurface9*, RECT*, D3DFORMAT, BYTE*, INT);
static PFNGETPIXELSPROC GetPixels;


///////////////////////////////////////
// FUNCTIONS
///////////////////////////////////////

/* This function hooks two IDirect3DDevice9 methods, using code overwriting technique. */
void HookDeviceMethods9(HMODULE hMod)
{
	// hook IDirect3DDevice9::Present(), using code modifications at run-time.
	// ALGORITHM: we overwrite the beginning of real IDirect3DDevice9::Present
	// with a JMP instruction to our routine (JucePresent9).
	// When our routine gets called, first thing that it does - it restores 
	// the original bytes of IDirect3DDevice9::Present, then performs its pre-call tasks, 
	// then calls IDirect3DDevice9::Present, then does post-call tasks, then writes
	// the JMP instruction back into the beginning of IDirect3DDevice9::Present, and
	// returns.
	
	if (!hMod)
	{
		Log("HookDeviceMethods9: d3d9 is not loaded at this moment. No hooking performed.");
		return;
	}
	if (!g_dxInfo.present9 || !g_dxInfo.reset9)
	{
		Log("HookDeviceMethods9: No info on Present and/or Reset.");
		Log("HookDeviceMethods9: Possible reason: DirectX 9 is not installed.");
		return;
	}
	
	g_D3D9_Present = (PRESENT9)((DWORD)hMod + g_dxInfo.present9);
	g_D3D9_Reset = (RESET9)((DWORD)hMod + g_dxInfo.reset9);

	TRACE("HookDeviceMethods9: checking JMP-implants...");
	if (memcmp(g_D3D9_Present, g_jmp_p9, 5)==0)
	{
		TRACE("HookDeviceMethods9: Present already has JMP-implant.");
	}
	else
	{
		TRACE2("HookDeviceMethods9: g_D3D9_Present = %08x", (DWORD)g_D3D9_Present);
		TRACE2("HookDeviceMethods9: JucePresent9 = %08x", (DWORD)JucePresent9);

		// unconditional JMP to relative address is 5 bytes.
		g_jmp_p9[0] = 0xe9;
		DWORD addr = (DWORD)JucePresent9 - (DWORD)g_D3D9_Present - 5;
		TRACE2("JMP %08x", addr);
		memcpy(g_jmp_p9 + 1, &addr, sizeof(DWORD));

		memcpy(g_codeFragment_p9, g_D3D9_Present, 5);
		DWORD newProtection = PAGE_EXECUTE_READWRITE;
		if (VirtualProtect(g_D3D9_Present, 8, newProtection, &g_savedProtection_p9))
		{
			memcpy(g_D3D9_Present, g_jmp_p9, 5);
			TRACE("HookDeviceMethods9: JMP-hook for Present planted.");
		}
	}

	if (memcmp(g_D3D9_Reset, g_jmp_r9, 5)==0)
	{
		TRACE("HookDeviceMethods9: Reset alreadry has JMP-implant.");
	}
	else
	{
		TRACE2("HookDeviceMethods9: g_D3D9_Reset = %08x", (DWORD)g_D3D9_Reset);
		TRACE2("HookDeviceMethods9: JuceReset9 = %08x", (DWORD)JuceReset9);

		// unconditional JMP to relative address is 5 bytes.
		g_jmp_r9[0] = 0xe9;
		DWORD addr = (DWORD)JuceReset9 - (DWORD)g_D3D9_Reset - 5;
		TRACE2("JMP %08x", addr);
		memcpy(g_jmp_r9 + 1, &addr, sizeof(DWORD));

		memcpy(g_codeFragment_r9, g_D3D9_Reset, 5);
		DWORD newProtection = PAGE_EXECUTE_READWRITE;
		if (VirtualProtect(g_D3D9_Reset, 8, newProtection, &g_savedProtection_r9))
		{
			memcpy(g_D3D9_Reset, g_jmp_r9, 5);
			TRACE("HookDeviceMethods9: JMP-hook for Reset planted.");
		}
	}
}

/* Restore original Reset() and Present() */
EXTERN_C _declspec(dllexport) void RestoreDeviceMethods9()
{
	if (pAddRefPtr != NULL && g_D3D9_AddRef != NULL)
		*pAddRefPtr = (DWORD)g_D3D9_AddRef;

	if (pReleasePtr != NULL && g_D3D9_Release != NULL)
		*pReleasePtr = (DWORD)g_D3D9_Release;

	Log("RestoreDeviceMethods9: done.");
}


/* determine window handle */
HWND GetProcWnd(IDirect3DDevice9* d3dDevice)
{
	TRACE("GetProcWnd: called.");
	D3DDEVICE_CREATION_PARAMETERS params;	
	if (SUCCEEDED(d3dDevice->GetCreationParameters(&params)))
	{
		return params.hFocusWindow;
	}
	return NULL;
}

/**
 * Determine format of back buffer and its dimensions.
 */
void GetBackBufferInfo(IDirect3DDevice9* d3dDevice)
{
	TRACE("GetBackBufferInfo: called.");

	// get the 0th backbuffer.
	if (SUCCEEDED(d3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &g_backBuffer)))
	{
		D3DSURFACE_DESC desc;
		g_backBuffer->GetDesc(&desc);
		g_bbFormat = desc.Format;
		g_bbWidth = desc.Width;
		g_bbHeight = desc.Height;

		// check dest.surface format
		bpp = 0;
		if (g_bbFormat == D3DFMT_R8G8B8) bpp = 3;
		else if (g_bbFormat == D3DFMT_R5G6B5 || g_bbFormat == D3DFMT_X1R5G5B5) bpp = 2;
		else if (g_bbFormat == D3DFMT_A8R8G8B8 || g_bbFormat == D3DFMT_X8R8G8B8) bpp = 4;

		// release backbuffer
		g_backBuffer->Release();

		Log("GetBackBufferInfo: got new back buffer format and info.");
		g_bGotFormat = true;
	}
}

/**
 * Loads the bitmap and puts on the surface.
 */
HRESULT LoadSurfaceFromFile(IDirect3DSurface9* surf, char* filename)
{
	BYTE* rgbBuf = NULL;
	BITMAPINFOHEADER* infoheader = NULL;
	if (FAILED(LoadFromBMP(filename, procHeap, &rgbBuf, &infoheader)))
	{
		Log("LoadSurfaceFromFile: LoadFromBMP failed.");
		return E_FAIL;
	}

	D3DSURFACE_DESC desc;
	if (FAILED(surf->GetDesc(&desc)))
	{
		Log("LoadSurfaceFromFile: GetDesc failed for dest.surface.");
		return E_FAIL;
	}

	if (infoheader->biBitCount < 24)
	{
		Log("LoadSurfaceFromFile: can only work with 24-bit bitmaps.");
		return E_FAIL;
	}
	INT bytes = infoheader->biWidth * infoheader->biBitCount / 8;
	INT rem = bytes % sizeof(ULONG);
	INT srcPitch = (rem == 0) ? bytes : bytes + sizeof(ULONG) - rem;
	INT width = infoheader->biWidth;
	INT height = infoheader->biHeight;
	LogWithNumber("LoadSurfaceFromFile: width = %d", width);
	LogWithNumber("LoadSurfaceFromFile: height = %d", height);

	D3DLOCKED_RECT destLockedRect;
	RECT rect = {0, 0, width, height};

	// lock rect
	if (FAILED(surf->LockRect(&destLockedRect, &rect, 0)))
	{
		Log("LoadSurfaceFromFile: failed to lock rect.");
		return E_FAIL;
	}
	BYTE* destPtr = (BYTE*)destLockedRect.pBits;
	INT destPitch = destLockedRect.Pitch;

	// write data
	int i,j,k;
	BYTE b0, b1, b2;
	switch (desc.Format)
	{
		case D3DFMT_X1R5G5B5:
			Log("LoadSurfaceFromFile: desc.Format = D3DFMT_X1R5G5B5");
			// 16-bit target surface: we need to do some conversion
			for (i=0, k=height-1; i<height, k>=0; i++, k--)
			{
				for (j=0; j<width; j++)
				{
					b0 = rgbBuf[k*srcPitch + j*3];      // blue
					b1 = rgbBuf[k*srcPitch + j*3 + 1];  // green
					b2 = rgbBuf[k*srcPitch + j*3 + 2];  // red

					//  memory layout 24 bit:
					//  --------------------------------------------------
					//  Lo            Hi Lo            Hi Lo            Hi
					//  b0b1b2b3b4b5b6b7 g0g1g2g3g4g5g6g7 r0r1r2r3r4r5r6r7
					//  --------------------------------------------------
					//       blue            green              red
					//  
					//                turns into:->
					//
					//  memory layout 16 bit:
					//  ---------------------------------
					//  Lo            Hi Lo            Hi
					//  b3b4b5b6b7g3g4g5 g6g7r3r4r5r6r700
					//  ---------------------------------
					//    blue      green      red
					
					destPtr[i*destPitch + j*2] = ((b0 >> 3) & 0x1f) | ((b1 << 2) & 0xe0);
					destPtr[i*destPitch + j*2 + 1] = ((b1 >> 6) & 0x03) | ((b2 >> 1) & 0x7c);
				}
			}
			break;

		case D3DFMT_R5G6B5:
			Log("LoadSurfaceFromFile: desc.Format = D3DFMT_R5G6B5");
			// 16-bit target surface: we need to do some conversion
			for (i=0, k=height-1; i<height, k>=0; i++, k--)
			{
				for (j=0; j<width; j++)
				{
					b0 = rgbBuf[k*srcPitch + j*3];      // blue
					b1 = rgbBuf[k*srcPitch + j*3 + 1];  // green
					b2 = rgbBuf[k*srcPitch + j*3 + 2];  // red

					//  memory layout 24 bit:
					//  --------------------------------------------------
					//  Lo            Hi Lo            Hi Lo            Hi
					//  b0b1b2b3b4b5b6b7 g0g1g2g3g4g5g6g7 r0r1r2r3r4r5r6r7
					//  --------------------------------------------------
					//       blue            green              red
					//  
					//                turns into:->
					//
					//  memory layout 16 bit:
					//  ---------------------------------
					//  Lo            Hi Lo            Hi
					//  b3b4b5b6b7g2g3g4 g5g6g7r3r4r5r6r7
					//  ---------------------------------
					//    blue        green        red
					
					destPtr[i*destPitch + j*2] = ((b0 >> 3) & 0x1f) | ((b1 << 3) & 0xe0);
					destPtr[i*destPitch + j*2 + 1] = ((b1 >> 5) & 0x07) | (b2 & 0xf8);
				}
			}
			break;

		case D3DFMT_R8G8B8:
			Log("LoadSurfaceFromFile: desc.Format = D3DFMT_R8G8B8");
			// 24-bit target surface: straight copy
			for (i=0, k=height-1; i<height, k>=0; i++, k--)
			{
				memcpy(destPtr + i*destPitch, rgbBuf + k*srcPitch, bytes);
			}
			break;

		case D3DFMT_A8R8G8B8:
		case D3DFMT_X8R8G8B8:
			Log("LoadSurfaceFromFile: desc.Format = D3DFMT_A8R8G8B8 or D3DFMT_X8R8G8B8");
			// 32-bit target surface: we need to do some conversion
			for (i=0, k=height-1; i<height, k>=0; i++, k--)
			{
				for (j=0; j<width; j++)
				{
					destPtr[i*destPitch + j*4] = rgbBuf[k*srcPitch + j*3];
					destPtr[i*destPitch + j*4 + 1] = rgbBuf[k*srcPitch + j*3 + 1];
					destPtr[i*destPitch + j*4 + 2] = rgbBuf[k*srcPitch + j*3 + 2];
					destPtr[i*destPitch + j*4 + 3] = 0xff;
				}
			}
			break;
		
		default:
			Log("LoadSurfaceFromFile: surface format not supported.");
			return E_FAIL;
	}

	// unlock rect
	if (FAILED(surf->UnlockRect()))
	{
		Log("LoadSurfaceFromFile: failed to unlock rect.");
		return E_FAIL;
	}

	// release memory
	if (rgbBuf != NULL) HeapFree(procHeap, 0, rgbBuf);
	if (infoheader != NULL) HeapFree(procHeap, 0, infoheader);

	return S_OK;
}

/**
 * Saves the surface into bitmap file.
 */
HRESULT SaveSurfaceToFile(IDirect3DSurface9* surf, char* filename, RECT* rect)
{
	D3DSURFACE_DESC desc;
	if (FAILED(surf->GetDesc(&desc)))
	{
		Log("SaveSurfaceToFile: GetDesc failed for surface.");
		return E_FAIL;
	}

	// allocate buffer for pixel data
	INT width = rect->right - rect->left;
	INT height = rect->bottom - rect->top;

	INT bytes = width * 3;
	INT rem = bytes % sizeof(ULONG);
	INT pitch = (rem == 0) ? bytes : bytes + sizeof(ULONG) - rem;
	SIZE_T size = pitch * height;

	//LogWithNumber("SaveSurfaceToFile: bytes = %d", bytes);
	//LogWithNumber("SaveSurfaceToFile: pitch = %d", pitch);
	//LogWithNumber("SaveSurfaceToFile: size = %d", size);

	BYTE* rgbBuf = (BYTE*)HeapAlloc(procHeap, HEAP_ZERO_MEMORY, size);

	//LogWithNumber("SaveSurfaceToFile: rgbBuf = %d", (DWORD)rgbBuf);

	// get RGB data
	INT bpp;
	IDirect3DDevice9* d3dDevice;
	if (FAILED(surf->GetDevice(&d3dDevice)))
	{
		Log("SaveSurfaceToFile: GetDevice failed for surface.");
		return E_FAIL;
	}
	if (FAILED(JuceGetRGB(d3dDevice, surf, rect, desc.Format, rgbBuf, pitch)))
	{
		Log("SaveSurfaceToFile: JuceGetRGB failed for surface.");
		return E_FAIL;
	}

	// save the bitmap
	if (FAILED(SaveAsBMP(NULL, rgbBuf, size, width, height, 3)))
	{
		Log("SaveSurfaceToFile: SaveAsBMP failed.");
		return E_FAIL;
	}
	if (rgbBuf) HeapFree(procHeap, 0, rgbBuf);

	return S_OK;
}

/* Makes a screen shot from back buffer. Doesn't use D3DX functions.*/
void MakeScreenShot(IDirect3DDevice9* d3dDevice, BYTE* rgbBuf, SIZE_T size, INT pitch)
{
	IDirect3DSurface9* buffer;
	if (FAILED(d3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &buffer)))
	{
		Log("MakeScreenShot: failed to get back-buffer");
		return;
	}

	// get pixels from the backbuffer into the new buffer
	RECT rect = {0, 0, g_bbWidth, g_bbHeight};
	if (FAILED(JuceGetRGB(d3dDevice, buffer, &rect, g_bbFormat, rgbBuf, pitch)))
	{
		Log("MakeScreenShot: unable to get RGB-data");
		return;
	}

	// save as bitmap
	LONG width = rect.right - rect.left;
	LONG height = rect.bottom - rect.top;
	if (FAILED(SaveAsBMP(NULL, rgbBuf, size, width, height, 3)))
	{
		Log("MakeScreenShot: SaveAsBMP failed.");
		return;
	}

	// release back-buffer
	buffer->Release();
}

/* Makes a small screen shot (1/2 original size) from back buffer. Doesn't use D3DX functions.*/
void MakeSmallScreenShot(IDirect3DDevice9* d3dDevice, BYTE* rgbBuf, SIZE_T size, INT pitch)
{
	IDirect3DSurface9* buffer;
	if (FAILED(d3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &buffer)))
	{
		Log("MakeSmallScreenShot: failed to get back-buffer");
		return;
	}

	// get pixels from the backbuffer into the new buffer
	int bpp = 0;
	RECT rect = {0, 0, g_bbWidth, g_bbHeight};
	if (FAILED(JuceGetHalfSizeRGB(d3dDevice, buffer, &rect, g_bbFormat, rgbBuf, pitch)))
	{
		Log("MakeSmallScreenShot: unable to get RGB-data");
		return;
	}

	// save as bitmap
	LONG width = rect.right - rect.left;
	LONG height = rect.bottom - rect.top;
	if (FAILED(SaveAsBMP(NULL, rgbBuf, size, width/2, height/2, 3)))
	{
		Log("MakeSmallScreenShot: SaveAsBMP failed.");
		return;
	}

	// release back-buffer
	buffer->Release();
}

/* Makes a small screen shot (1/2 original size) from back buffer. Doesn't use D3DX functions.*/
void GetFrame(IDirect3DDevice9* d3dDevice, BYTE* rgbBuf, SIZE_T size, INT pitch)
{
	IDirect3DSurface9* buffer;
	if (FAILED(d3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &buffer)))
	{
		Log("GetFrame: failed to get back-buffer");
		return;
	}

	// get pixels from the backbuffer into the new buffer
	int bpp = 0;
	RECT rect = {0, 0, g_bbWidth, g_bbHeight};
	if (FAILED(GetPixels(d3dDevice, buffer, &rect, g_bbFormat, rgbBuf, pitch)))
	{
		Log("GetFrame: unable to get RGB-data");
		return;
	}

	// release back-buffer
	buffer->Release();
}

/* New Reset function */
EXTERN_C HRESULT _declspec(dllexport) STDMETHODCALLTYPE JuceReset9(
IDirect3DDevice9* self, LPVOID params)
{
	TRACE("#################################");
	TRACE("JuceReset9: called.");
	Log("JuceReset9: cleaning-up.");

	BYTE* codeDest = (BYTE*)g_D3D9_Reset;

	// put back saved code fragment
	codeDest[0] = g_codeFragment_r9[0];
	*((DWORD*)(codeDest + 1)) = *((DWORD*)(g_codeFragment_r9 + 1));

	// drop HWND 
	hProcWnd = NULL;

	// if in recording mode, close the AVI file,
	// and set the flag so that the next Present() will continue.
	if (g_mystate.bNowRecording)
	{
		CloseAVIFile(videoFile);
		g_mystate.bNowRecording = false;
		Log("Recording stopped. (Reset is called).");
		g_mystate.bStartRecording = true;
	}

	// clean-up
	if (g_rgbBuf != NULL) 
	{
		HeapFree(procHeap, 0, g_rgbBuf);
		g_rgbBuf = NULL;
	}

	InvalidateDeviceObjects(self, false);
	DeleteDeviceObjects(self);

	g_bGotFormat = false;

	/* call real Present() */
	HRESULT res = g_D3D9_Reset(self, params);

	VerifyHooks(self);
	TRACE("JuceReset9: Reset() is done. About to return.");

	// put JMP instruction again
	codeDest[0] = g_jmp_r9[0];
	*((DWORD*)(codeDest + 1)) = *((DWORD*)(g_jmp_r9 + 1));

	return res;
}

/* New AddRef function */
EXTERN_C ULONG _declspec(dllexport) STDMETHODCALLTYPE JuceAddRef9(IUnknown* self)
{
	g_refCount = g_D3D9_AddRef(self);
	TRACE2("JuceAddRef9: called (g_refCount = %d).", g_refCount);
	return g_refCount;
}

/* New Release function */
EXTERN_C ULONG _declspec(dllexport) STDMETHODCALLTYPE JuceRelease9(IUnknown* self)
{
	// a "fall-through" case
	if ((g_refCount > g_myCount + 1) && g_D3D9_Release) 
	{
		g_refCount = g_D3D9_Release(self);
		TRACE2("JuceRelease9: called (g_refCount = %d).", g_refCount);
		return g_refCount;
	}

	TRACE("+++++++++++++++++++++++++++++++++++++");
	TRACE("JuceRelease9: called.");
	TRACE2("JuceRelease9: self = %08x", (DWORD)self);
	TRACE2("JuceRelease9: VTABLE[0] = %08x", ((DWORD*)(*((DWORD*)self)))[0]);
	TRACE2("JuceRelease9: VTABLE[1] = %08x", ((DWORD*)(*((DWORD*)self)))[1]);
	TRACE2("JuceRelease9: VTABLE[2] = %08x", ((DWORD*)(*((DWORD*)self)))[2]);

	// if in recording mode, close the AVI file,
	// and set the flag so that the next Present() will continue.
	if (g_mystate.bNowRecording)
	{
		CloseAVIFile(videoFile);
		g_mystate.bNowRecording = false;
		Log("Recording stopped. (Reset is called).");
		g_mystate.bStartRecording = true;
	}

	// unhook device methods
	RestoreDeviceMethods9();
	
	// clean up
	IDirect3DDevice9* dev = (IDirect3DDevice9*)self;
	InvalidateDeviceObjects(dev, false);
	DeleteDeviceObjects(dev);

	// reset the pointers
	pAddRefPtr = NULL;
	pReleasePtr = NULL;

	// call the real Release()
	TRACE("JuceRelease9: about to call real Release.");
	g_refCount = g_D3D9_Release(self);
	TRACE("JuceRelease9: real Release done.");
	TRACE2("JuceRelease9: g_refCount = %d", g_refCount);

	TRACE("JuceRelease9() done.");

	return g_refCount;
}

/* New Present function */
EXTERN_C HRESULT _declspec(dllexport) STDMETHODCALLTYPE JucePresent9(
IDirect3DDevice9* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused)
{
	TRACE("--------------------------------");
	TRACE("JucePresent9: called.");
	/* log the usage */
	//Log("JucePresent9: Present() is about to be called.");
	
	BYTE* codeDest = (BYTE*)g_D3D9_Present;

	// put back saved code fragment
	codeDest[0] = g_codeFragment_p9[0];
	*((DWORD*)(codeDest + 1)) = *((DWORD*)(g_codeFragment_p9 + 1));

	// rememeber IDirect3DDevice9::Release pointer so that we can clean-up properly.
	if (pAddRefPtr == NULL || pReleasePtr == NULL)
	{
		DWORD* vtable = (DWORD*)(*((DWORD*)self));
		pAddRefPtr = vtable + 1;
		pReleasePtr = vtable + 2;

		TRACE2("*pAddRefPtr  = %08x", *pAddRefPtr);
		TRACE2("*pReleasePtr  = %08x", *pReleasePtr);

		// hook AddRef method
		g_D3D9_AddRef = (RELEASE9)(*pAddRefPtr);
		*pAddRefPtr = (DWORD)JuceAddRef9;

		// hook Release method
		g_D3D9_Release = (RELEASE9)(*pReleasePtr);
		*pReleasePtr = (DWORD)JuceRelease9;
	}

	// determine whether this frame needs to be grabbed when recording.
	DWORD frameDups = GetFrameDups();

	// determine backbuffer's format and dimensions, if not done yet.
	if (!g_bGotFormat) GetBackBufferInfo(self);

	TRACE("JucePresent9: new frame.");

	// determine how we are going to handle keyboard hot-keys:
	// Use DirectInput, if configured so, 
	// Otherwise use thread-specific keyboard hook.
	if (!g_mystate.bKeyboardInitDone)
	{
		if (g_config.useDirectInput)
		{
			hDI = (HINSTANCE)GetModuleHandle("dinput8.dll");
			if (!hDI) hDI = (HINSTANCE)LoadLibrary("dinput8.dll");
			if (hDI)
			{
				TRACE("JucePresent9: KEYBOARD INIT: using DirectInput.");
				GetDirectInputCreator();

				// install dummy keyboard hook so that we don't get unmapped,
				// when going to exclusive mode (i.e. unhooking GetMsgProc)
				if (hProcWnd == NULL) hProcWnd = GetProcWnd(self);
				DWORD tid = GetWindowThreadProcessId(hProcWnd, NULL);
				InstallDummyKeyboardHook(tid);

				g_mystate.bKeyboardInitDone = TRUE;
			}
			else
			{
				TRACE("JucePresent9: KEYBOARD INIT: Unable to load dinput8.dll.");
			}
		}

		// if we're not done at this point, use keyboard hook
		if (!g_mystate.bKeyboardInitDone)
		{
			TRACE("JucePresent9: KEYBOARD INIT: using keyboard hook.");

			// install keyboard hook 
			if (hProcWnd == NULL) hProcWnd = GetProcWnd(self);
			DWORD tid = GetWindowThreadProcessId(hProcWnd, NULL);
			InstallKeyboardHook(tid);
		}

		// Goto "exclusive" mode (i.e. unhook GetMsgProc)
		// if specified so in the g_mystate
		if (g_mystate.bUnhookGetMsgProc)
		{
			Log("JucePresent9: Signaling to unhook GetMsgProc.");
			if (UninstallTheHook())
			{
				Log("JucePresent9: GetMsgProc unhooked.");
				// change indicator to "green"
			}
			g_mystate.bUnhookGetMsgProc = FALSE;
		}

		// keyboard configuration done
		g_mystate.bKeyboardInitDone = TRUE;
	}

	// Process DirectInput input
	if (g_config.useDirectInput) ProcessDirectInput(&g_config);

	// Open AVI file, if asked so.
	if (g_mystate.bStartRecording) 
	{
		TRACE("JucePresent9: calling OpenAVIFile.");
		videoFile = OpenAVIFile();
		g_mystate.bStartRecording = false;
		Log("Recording started.");
		g_mystate.bNowRecording = true;
	}
	// Close AVI file, if asked so.
	else if (g_mystate.bStopRecording)
	{
		TRACE("JucePresent9: calling CloseAVIFile.");
		CloseAVIFile(videoFile);
		g_mystate.bStopRecording = false;
		Log("Recording stopped.");
		g_mystate.bNowRecording = false;
	}

	/* make custom screen shot, if requested so. */
	else if (g_mystate.bMakeScreenShot)
	{
		//DWORD start, stop;
		//start = GetTickCount();

		// allocate buffer for pixel data
		INT bytes = g_bbWidth * 3;
		INT rem = bytes % sizeof(ULONG);
		INT pitch = (rem == 0) ? bytes : bytes + sizeof(ULONG) - rem;
		SIZE_T size = pitch * g_bbHeight;
		BYTE* rgbBuf = (BYTE*)HeapAlloc(procHeap, HEAP_ZERO_MEMORY, size);

		// shoot
		MakeScreenShot(self, rgbBuf, size, pitch);

		// free the buffer
		HeapFree(procHeap, 0, rgbBuf);

		g_mystate.bMakeScreenShot = false;

		//stop = GetTickCount();
		//LogWithNumber("MakeScreenShot: %d ticks", (stop-start));
	}

	/* make small screen shot, if requested so. */
	else if (g_mystate.bMakeSmallScreenShot)
	{
		//DWORD start, stop;
		//start = GetTickCount();
		
		// allocate buffer for pixel data
		INT bytes = g_bbWidth / 2 * 3;
		INT rem = bytes % sizeof(ULONG);
		INT pitch = (rem == 0) ? bytes : bytes + sizeof(ULONG) - rem;
		SIZE_T size = pitch * g_bbHeight / 2;
		BYTE* rgbBuf = (BYTE*)HeapAlloc(procHeap, HEAP_ZERO_MEMORY, size);

		// shoot
		MakeSmallScreenShot(self, rgbBuf, size, pitch);

		// free the buffer
		HeapFree(procHeap, 0, rgbBuf);

		g_mystate.bMakeSmallScreenShot = false;

		//stop = GetTickCount();
		//LogWithNumber("MakeSmallScreenShot: %d ticks", (stop-start));
	}

	TRACE2("g_mystate.bNowRecording = %d", g_mystate.bNowRecording);
	/* write AVI-file, if in recording mode. */
	if (g_mystate.bNowRecording)
	{
		if (frameDups > 0)
		{
			DWORD start = QPC();
			
			if (g_rgbBuf == NULL)
			{
				INT divider = (g_config.fullSizeVideo) ? 1 : 2;

				// allocate buffer for pixel data
				INT bytes = g_bbWidth / divider * 3;
				INT rem = bytes % sizeof(ULONG);
				g_Pitch = (rem == 0) ? bytes : bytes + sizeof(ULONG) - rem;
				g_rgbSize = g_Pitch * g_bbHeight / divider;
				g_rgbBuf = (BYTE*)HeapAlloc(procHeap, HEAP_ZERO_MEMORY, g_rgbSize);
			}

			// shoot a frame
			TRACE("Getting FRAME");
			GetFrame(self, g_rgbBuf, g_rgbSize, g_Pitch);
			TRACE("Got FRAME");
			WriteAVIFrame(videoFile, g_rgbBuf, g_rgbSize, frameDups);

			// keep track of recording overhead
			DWORD stop = QPC();

			g_lastOverhead = stop - start;
			g_skipped = 1;
			g_frameRate = 0.0;
		}
		else
		{
			g_skipped++;
		}
	}

	/* draw the indicator */
	if (g_mystate.bIndicate || g_mystate.bNowRecording) DrawIndicator(self);

	/* call real Present() */
	HRESULT res = g_D3D9_Present(self, src, dest, hWnd, unused);

	/* check unhook flag */
	//TRACE2("JucePresent9: sg_bUnhookDevice = %d.", GetUnhookFlag());
	if (GetUnhookFlag())
	{
		// we're ordered to unhook IDirect3DDevice9 methods.
		RestoreDeviceMethods9();

		// uninstall keyboard hook
		UninstallKeyboardHook();
	}

	/* check if we need to reconfigure */
	if (sg_reconfCounter != g_reconfCounter && !g_mystate.bNowRecording)
	{
		Log("JucePresent9: Reconfiguring custom settings...");
		g_algorithm = SearchCustomConfig(g_myinfo.processfile, g_myinfo.dir, &customConfig) ? 
			CUSTOM : ADAPTIVE;
		if (customConfig)
		{
			g_customFrameRate = customConfig->frameRate;
			g_customFrameWeight = customConfig->frameWeight;
		}
		g_reconfCounter = sg_reconfCounter;
	}

	VerifyHooks(self);
	TRACE("JucePresent9: Present() is done. About to return.");

	// put JMP instruction again
	codeDest[0] = g_jmp_p9[0];
	*((DWORD*)(codeDest + 1)) = *((DWORD*)(g_jmp_p9 + 1));

	return res;
}

HRESULT InitVB(IDirect3DDevice9* dev)
{
	VOID* pVertices;

	// create vertex buffer for green light
	if (FAILED(dev->CreateVertexBuffer(4*sizeof(CUSTOMVERTEX), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &g_pVBidle, NULL)))
	{
		Log("CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log("CreateVertexBuffer() done.");
	g_myCount++;

	if (FAILED(g_pVBidle->Lock(0, sizeof(g_VertIdle), &pVertices, 0)))
	{
		Log("g_pVBidle->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_VertIdle, sizeof(g_VertIdle));
	g_pVBidle->Unlock();

	// create vertex buffer for blue light
	if (FAILED(dev->CreateVertexBuffer(4*sizeof(CUSTOMVERTEX), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &g_pVBswHook, NULL)))
	{
		Log("CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log("CreateVertexBuffer() done.");
	g_myCount++;

	if (FAILED(g_pVBswHook->Lock(0, sizeof(g_VertSWHook), &pVertices, 0)))
	{
		Log("g_pVBswHook->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_VertSWHook, sizeof(g_VertSWHook));
	g_pVBswHook->Unlock();

	// create vertex buffer for red light
	if (FAILED(dev->CreateVertexBuffer(4*sizeof(CUSTOMVERTEX), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &g_pVBrec, NULL)))
	{
		Log("CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log("CreateVertexBuffer() done.");
	g_myCount++;

	if (FAILED(g_pVBrec->Lock(0, sizeof(g_VertRec), &pVertices, 0)))
	{
		Log("g_pVrec->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_VertRec, sizeof(g_VertRec));
	g_pVBrec->Unlock();

	// create vertex buffer for black border
	if (FAILED(dev->CreateVertexBuffer(5*sizeof(CUSTOMVERTEX), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &g_pVBborder, NULL)))
	{
		Log("CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log("CreateVertexBuffer() done.");
	g_myCount++;

	if (FAILED(g_pVBborder->Lock(0, sizeof(g_VertBorder), &pVertices, 0)))
	{
		Log("g_pVBborder->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_VertBorder, sizeof(g_VertBorder));
	g_pVBborder->Unlock();
	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: RestoreDeviceObjects()
// Desc:
//-----------------------------------------------------------------------------
HRESULT RestoreDeviceObjects(IDirect3DDevice9* dev)
{
	g_dev = dev;

    HRESULT hr = InitVB(dev);
    if (FAILED(hr))
    {
		Log("InitVB() failed.");
        return hr;
    }
	Log("InitVB() done.");

	D3DVIEWPORT9 vp;
	vp.X      = 0;
	vp.Y      = 0;
	vp.Width  = INDX*2 + IWIDTH;
	vp.Height = INDY*2 + IHEIGHT;
	vp.MinZ   = 0.0f;
	vp.MaxZ   = 1.0f;

	// Create the state blocks for rendering text
	for( UINT which=0; which<2; which++ )
	{
		dev->BeginStateBlock();

		dev->SetViewport( &vp );
		dev->SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
		dev->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
		dev->SetRenderState( D3DRS_FILLMODE,   D3DFILL_SOLID );
		dev->SetRenderState( D3DRS_CULLMODE,   D3DCULL_CW );
		dev->SetRenderState( D3DRS_STENCILENABLE,    FALSE );
		dev->SetRenderState( D3DRS_CLIPPING, TRUE );
		dev->SetRenderState( D3DRS_CLIPPLANEENABLE, FALSE );
		dev->SetRenderState( D3DRS_FOGENABLE,        FALSE );

		dev->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_DISABLE );
		dev->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );

		dev->SetVertexShader( NULL );
		dev->SetFVF( D3DFVF_CUSTOMVERTEX );
		dev->SetPixelShader ( NULL );
		dev->SetStreamSource( 0, g_pVBidle, 0, sizeof(CUSTOMVERTEX) );

		if( which==0 )
		{
			dev->EndStateBlock( &g_pSavedStateBlock );
			g_myCount++;
		}
		else
		{
			dev->EndStateBlock( &g_pDrawTextStateBlock );
			g_myCount++;
		}
	}

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: InvalidateDeviceObjects()
// Desc: Destroys all device-dependent objects
//-----------------------------------------------------------------------------
HRESULT InvalidateDeviceObjects(IDirect3DDevice9* dev, BOOL detaching)
{
	TRACE("InvalidateDeviceObjects: called.");
	//if (detaching) return S_OK;

    // Delete the state blocks
    //if( dev && !detaching )
    {
		SAFE_RELEASE2( g_pSavedStateBlock, g_myCount );
		SAFE_RELEASE2( g_pDrawTextStateBlock, g_myCount );
		Log("InvalidateDeviceObjects: DeleteStateBlock(s) done.");
    }

	SAFE_RELEASE2( g_pVBidle, g_myCount );
	SAFE_RELEASE2( g_pVBswHook, g_myCount );
	SAFE_RELEASE2( g_pVBborder, g_myCount );
	SAFE_RELEASE2( g_pVBrec, g_myCount );
	Log("InvalidateDeviceObjects: SAFE_RELEASE(s) done.");

	Log("InvalidateDeviceObjects: done.");
    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: DeleteDeviceObjects()
// Desc: Destroys all device-dependent objects
//-----------------------------------------------------------------------------
HRESULT DeleteDeviceObjects(IDirect3DDevice9* dev)
{
    return S_OK;
}

void DrawIndicator(LPVOID self)
{
	IDirect3DDevice9* dev = (IDirect3DDevice9*)self;

	if (g_pVBidle == NULL) 
	{
		if (FAILED(RestoreDeviceObjects(dev)))
		{
			Log("RestoreDeviceObjects() failed.");
		}
		Log("RestoreDeviceObjects() done.");
	}

	// setup renderstate
	g_pSavedStateBlock->Capture();
	g_pDrawTextStateBlock->Apply();

	// render
	dev->BeginScene();

	if (g_mystate.bNowRecording) dev->SetStreamSource( 0, g_pVBrec, 0, sizeof(CUSTOMVERTEX) );
	else if (g_mystate.bSystemWideMode) dev->SetStreamSource( 0, g_pVBswHook, 0, sizeof(CUSTOMVERTEX) );
	else dev->SetStreamSource( 0, g_pVBidle, 0, sizeof(CUSTOMVERTEX) );
	dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

	dev->SetStreamSource( 0, g_pVBborder, 0, sizeof(CUSTOMVERTEX) );
	dev->DrawPrimitive( D3DPT_LINESTRIP, 0, 4 );

	dev->EndScene();

	// restore the modified renderstates
	g_pSavedStateBlock->Apply();
}

/* Copies pixel's RGB data into dest buffer. Alpha information is discarded. */
HRESULT JuceGetRGB(IDirect3DDevice9* d3dDevice, 
IDirect3DSurface9* srcSurf, RECT* rect, D3DFORMAT format, BYTE* destBuf, INT destPitch)
{
	//Log("JuceGetRGB: called.");

	// check dest.surface format
	if (!(format == D3DFMT_R8G8B8 || format == D3DFMT_A8R8G8B8 || format == D3DFMT_X8R8G8B8 ||
	      format == D3DFMT_R5G6B5 || format == D3DFMT_X1R5G5B5))
	{
		Log("JuceGetRGB: surface format not supported.");
		return E_FAIL;
	}

	D3DLOCKED_RECT lockedSrcRect;
	int width = rect->right - rect->left;
	int height = rect->bottom - rect->top;

	// copy rect to a new surface and lock the rect there.
	// NOTE: apparently, this works faster than locking a rect on the back buffer itself.
	// LOOKS LIKE: reading from the backbuffer is faster via GetRenderTargetData, but writing
	// is faster if done directly.
	IDirect3DSurface9* surf = NULL;
	if (FAILED(d3dDevice->CreateOffscreenPlainSurface(
					width, height, format, D3DPOOL_SYSTEMMEM, &surf, NULL)))
	{
		Log("JuceGetRGB: Unable to create image surface.");
		return E_FAIL;
	}
	POINT points[] = {{0, 0}};
	//Log("JuceGetRGB: created image surface.");
	//LogWithNumber("JuceGetRGB: d3dDevice = %d", (DWORD)d3dDevice);
	//LogWithNumber("JuceGetRGB: srcSurf = %d", (DWORD)srcSurf);
	//LogWithNumber("JuceGetRGB: rect = %d", (DWORD)rect);
	//LogWithNumber("JuceGetRGB: imgSurf = %d", (DWORD)imgSurf);
	//LogWithNumber("JuceGetRGB: points = %d", (DWORD)points);
	if (FAILED(d3dDevice->GetRenderTargetData(srcSurf, surf)))
	{
		Log("JuceGetRGB: GetRenderTargetData() failed for image surface.");
		return E_FAIL;
	}
	//Log("JuceGetRGB: copied rects.");
	
	//DWORD start = GetTickCount();
	RECT newRect = {0, 0, width, height};
	if (FAILED(surf->LockRect(&lockedSrcRect, &newRect, 0)))
	{
		Log("JuceGetRGB: Unable to lock source rect.");
		return E_FAIL;
	}
	//Log("JuceGetRGB: locked rect.");
	//DWORD stop = GetTickCount();
	//LogWithNumber("JuceGetRGB: LockRect = %d ticks", (stop-start));

	// copy data
	//start = GetTickCount();
	BYTE* pSrcRow = (BYTE*)lockedSrcRect.pBits;
	INT srcPitch = lockedSrcRect.Pitch;

	//LogWithNumber("width = %d", width);
	//LogWithNumber("height = %d", width);
	//LogWithNumber("srcPitch = %d", srcPitch);
	//LogWithNumber("destPitch = %d", destPitch);
	
	int i, j, k;
	BYTE b0, b1;
	switch (format)
	{
		case D3DFMT_R8G8B8:
			Log("JuceGetRGB: source format = D3DFMT_R8G8B8");
			// 24-bit: straight copy
			for (i=0, k=height-1; i<height, k>=0; i++, k--)
			{
				memcpy(destBuf + i*destPitch, pSrcRow + k*srcPitch, 3*width);
			}
			break;

		case D3DFMT_X1R5G5B5:
			Log("JuceGetRGB: source format = D3DFMT_X1R5G5B5");
			// 16-bit: some conversion needed.
			for (i=0, k=height-1; i<height, k>=0; i++, k--)
			{
				for (j=0; j<width; j++)
				{
					b0 = pSrcRow[k*srcPitch + j*2];
					b1 = pSrcRow[k*srcPitch + j*2 + 1];

					//  memory layout 16 bit:
					//  ---------------------------------
					//  Lo            Hi Lo            Hi
					//  b3b4b5b6b7g3g4g5 g6g7r3r4r5r6r700
					//  ---------------------------------
					//    blue      green      red
					//
					//                turns into:->
					//
					//  memory layout 24 bit:
					//  --------------------------------------------------
					//  Lo            Hi Lo            Hi Lo            Hi
					//  000000b3b4b5b6b7 000000g3g4g5g6g7 000000r3r4r5r6r7
					//  --------------------------------------------------
					//       blue            green              red
						
					destBuf[i*destPitch + j*3] = (b0 << 3) & 0xf8;
					destBuf[i*destPitch + j*3 + 1] = ((b0 >> 2) & 0x38) | ((b1 << 6) & 0xc0);
					destBuf[i*destPitch + j*3 + 2] = (b1 << 1) & 0xf8;
				}
			}
			break;

		case D3DFMT_R5G6B5:
			Log("JuceGetRGB: source format = D3DFMT_R5G6B5");
			// 16-bit: some conversion needed.
			for (i=0, k=height-1; i<height, k>=0; i++, k--)
			{
				for (j=0; j<width; j++)
				{
					b0 = pSrcRow[k*srcPitch + j*2];
					b1 = pSrcRow[k*srcPitch + j*2 + 1];

					//  memory layout 16 bit:
					//  ---------------------------------
					//  Lo            Hi Lo            Hi
					//  b3b4b5b6b7g2g3g4 g5g6g7r3r4r5r6r7
					//  ---------------------------------
					//    blue      green      red
					//
					//                turns into:->
					//
					//  memory layout 24 bit:
					//  --------------------------------------------------
					//  Lo            Hi Lo            Hi Lo            Hi
					//  000000b3b4b5b6b7 000000g3g4g5g6g7 000000r3r4r5r6r7
					//  --------------------------------------------------
					//       blue            green              red
						
					destBuf[i*destPitch + j*3] = (b0 << 3) & 0xf8;
					destBuf[i*destPitch + j*3 + 1] = ((b0 >> 3) & 0x1c) | ((b1 << 5) & 0xe0);
					destBuf[i*destPitch + j*3 + 2] = b1 & 0xf8;
				}
			}
			break;

		case D3DFMT_A8R8G8B8:
		case D3DFMT_X8R8G8B8:
			Log("JuceGetRGB: source format = D3DFMT_A8R8G8B8 or D3DFMT_X8R8G8B8");
			// 32-bit entries: discard alpha
			for (i=0, k=height-1; i<height, k>=0; i++, k--)
			{
				for (j=0; j<width; j++)
				{
					destBuf[i*destPitch + j*3] = pSrcRow[k*srcPitch + j*4];
					destBuf[i*destPitch + j*3 + 1] = pSrcRow[k*srcPitch + j*4 + 1];
					destBuf[i*destPitch + j*3 + 2] = pSrcRow[k*srcPitch + j*4 + 2];
				}
			}
			break;
	}

	//stop = GetTickCount();
	//LogWithNumber("JuceGetRGB: copy data = %d ticks", (stop-start));

	// unlock rect
	//start = GetTickCount();
	surf->UnlockRect();
	//stop = GetTickCount();
	//LogWithNumber("JuceGetRGB: UnlockRect = %d ticks", (stop-start));
	
	// release surface
	surf->Release();

	return S_OK;
}

/* Copies pixel's RGB data into dest buffer. Alpha information is discarded. */
HRESULT JuceGetHalfSizeRGB(IDirect3DDevice9* d3dDevice, 
IDirect3DSurface9* srcSurf, RECT* rect, D3DFORMAT format, BYTE* destBuf, INT destPitch)
{
	//Log("JuceGetHalfSizeRGB: called.");

	// check dest.surface format
	if (!(format == D3DFMT_R8G8B8 || format == D3DFMT_A8R8G8B8 || format == D3DFMT_X8R8G8B8 ||
	      format == D3DFMT_R5G6B5 || format == D3DFMT_X1R5G5B5))
	{
		Log("JuceGetHalfSizeRGB: surface format not supported.");
		return E_FAIL;
	}

	D3DLOCKED_RECT lockedSrcRect;
	int width = rect->right - rect->left;
	int height = rect->bottom - rect->top;
	int dHeight = height/2;
	int dWidth = width/2;

	// copy rect to a new surface and lock the rect there.
	// NOTE: apparently, this works faster than locking a rect on the back buffer itself.
	// LOOKS LIKE: reading from the backbuffer is faster via GetRenderTargetData, but writing
	// is faster if done directly.
	IDirect3DSurface9* surf = NULL;
	if (FAILED(d3dDevice->CreateOffscreenPlainSurface(
					width, height, format, D3DPOOL_SYSTEMMEM, &surf, NULL)))
	{
		Log("JuceGetHalfSizeRGB: Unable to create image surface.");
		return E_FAIL;
	}
	POINT points[] = {{0, 0}};
	if (FAILED(d3dDevice->GetRenderTargetData(srcSurf, surf)))
	{
		Log("JuceGetHalfSizeRGB: GetRenderTargetData() failed for image surface.");
		return E_FAIL;
	}
	//Log("JuceGetHalfSizeRGB: copied rects.");
	
	RECT newRect = {0, 0, width, height};
	if (FAILED(surf->LockRect(&lockedSrcRect, &newRect, 0)))
	{
		Log("JuceGetHalfSizeRGB: Unable to lock source rect.");
		return E_FAIL;
	}
	//Log("JuceGetHalfSizeRGB: locked rect.");

	// copy data
	BYTE* pSrcRow = (BYTE*)lockedSrcRect.pBits;
	INT srcPitch = lockedSrcRect.Pitch;

	//LogWithNumber("width = %d", width);
	//LogWithNumber("height = %d", width);
	//LogWithNumber("srcPitch = %d", srcPitch);
	//LogWithNumber("destPitch = %d", destPitch);
	
	int sbpp = 0;
	int i, j, k, m;
	BYTE b0, b1;
	BYTE b00, b01, b02, b10, b11, b12, b20, b21, b22, b30, b31, b32;
	UINT temp;

	switch (format)
	{
		case D3DFMT_X1R5G5B5:
			//Log("JuceGetHalfSizeRGB: source format = D3DFMT_X1R5G5B5");
			// 16-bit: some conversion needed.
			for (i=0, k=height-2; i<dHeight, k>=0; i++, k-=2)
			{
				for (j=0, m=0; j<dWidth, m<width-1; j++, m+=2)
				{
					//  memory layout 16 bit:
					//  ---------------------------------
					//  Lo            Hi Lo            Hi
					//  b3b4b5b6b7g3g4g5 g6g7r3r4r5r6r700
					//  ---------------------------------
					//    blue      green      red
					//
					//                turns into:->
					//
					//  memory layout 24 bit:
					//  --------------------------------------------------
					//  Lo            Hi Lo            Hi Lo            Hi
					//  000000b3b4b5b6b7 000000g3g4g5g6g7 000000r3r4r5r6r7
					//  --------------------------------------------------
					//       blue            green              red
						
					/*
					// fetch 1 pixel: nearest neigbour
					b0 = pSrcRow[k*srcPitch + m*2];
					b1 = pSrcRow[k*srcPitch + m*2 + 1];

					destBuf[i*destPitch + j*3] = (b0 << 3) & 0xf8;
					destBuf[i*destPitch + j*3 + 1] = ((b0 >> 2) & 0x38) | ((b1 << 6) & 0xc0);
					destBuf[i*destPitch + j*3 + 2] = (b1 << 1) & 0xf8;
					*/

					// fetch 4 pixels: bilinear interpolation
					b0 = pSrcRow[k*srcPitch + m*2];
					b1 = pSrcRow[k*srcPitch + m*2 + 1];
					b00 = (b0 << 3) & 0xf8;
					b01 = ((b0 >> 2) & 0x38) | ((b1 << 6) & 0xc0);
					b02 = (b1 << 1) & 0xf8;
					b0 = pSrcRow[k*srcPitch + (m+1)*2];
					b1 = pSrcRow[k*srcPitch + (m+1)*2 + 1];
					b10 = (b0 << 3) & 0xf8;
					b11 = ((b0 >> 2) & 0x38) | ((b1 << 6) & 0xc0);
					b12 = (b1 << 1) & 0xf8;
					b0 = pSrcRow[(k+1)*srcPitch + m*2];
					b1 = pSrcRow[(k+1)*srcPitch + m*2 + 1];
					b20 = (b0 << 3) & 0xf8;
					b21 = ((b0 >> 2) & 0x38) | ((b1 << 6) & 0xc0);
					b22 = (b1 << 1) & 0xf8;
					b0 = pSrcRow[(k+1)*srcPitch + (m+1)*2];
					b1 = pSrcRow[(k+1)*srcPitch + (m+1)*2 + 1];
					b30 = (b0 << 3) & 0xf8;
					b31 = ((b0 >> 2) & 0x38) | ((b1 << 6) & 0xc0);
					b32 = (b1 << 1) & 0xf8;

					temp = (b00 + b10 + b20 + b30) >> 2;
					destBuf[i*destPitch + j*3] = (BYTE)temp;
					temp = (b01 + b11 + b21 + b31) >> 2;
					destBuf[i*destPitch + j*3 + 1] = (BYTE)temp; 
					temp = (b02 + b12 + b22 + b32) >> 2;
					destBuf[i*destPitch + j*3 + 2] = (BYTE)temp;
				}
			}
			break;

		case D3DFMT_R5G6B5:
			//Log("JuceGetHalfSizeRGB: source format = D3DFMT_R5G6B5");
			// 16-bit: some conversion needed.
			for (i=0, k=height-2; i<dHeight, k>=0; i++, k-=2)
			{
				for (j=0, m=0; j<dWidth, m<width-1; j++, m+=2)
				{
					//  memory layout 16 bit:
					//  ---------------------------------
					//  Lo            Hi Lo            Hi
					//  b3b4b5b6b7g2g3g4 g5g6g7r3r4r5r6r7
					//  ---------------------------------
					//    blue      green      red
					//
					//                turns into:->
					//
					//  memory layout 24 bit:
					//  --------------------------------------------------
					//  Lo            Hi Lo            Hi Lo            Hi
					//  000000b3b4b5b6b7 000000g3g4g5g6g7 000000r3r4r5r6r7
					//  --------------------------------------------------
					//       blue            green              red
						
					/*
					// fetch 1 pixel: nearest neigbour
					b0 = pSrcRow[k*srcPitch + m*2];
					b1 = pSrcRow[k*srcPitch + m*2 + 1];

					destBuf[i*destPitch + j*3] = (b0 << 3) & 0xf8;
					destBuf[i*destPitch + j*3 + 1] = ((b0 >> 2) & 0x38) | ((b1 << 6) & 0xc0);
					destBuf[i*destPitch + j*3 + 2] = (b1 << 1) & 0xf8;
					*/

					// fetch 4 pixels: bilinear interpolation
					b0 = pSrcRow[k*srcPitch + m*2];
					b1 = pSrcRow[k*srcPitch + m*2 + 1];
					b00 = (b0 << 3) & 0xf8;
					b01 = ((b0 >> 3) & 0x1c) | ((b1 << 5) & 0xe0);
					b02 = b1 & 0xf8;
					b0 = pSrcRow[k*srcPitch + (m+1)*2];
					b1 = pSrcRow[k*srcPitch + (m+1)*2 + 1];
					b10 = (b0 << 3) & 0xf8;
					b11 = ((b0 >> 3) & 0x1c) | ((b1 << 5) & 0xe0);
					b12 = b1 & 0xf8;
					b0 = pSrcRow[(k+1)*srcPitch + m*2];
					b1 = pSrcRow[(k+1)*srcPitch + m*2 + 1];
					b20 = (b0 << 3) & 0xf8;
					b21 = ((b0 >> 3) & 0x1c) | ((b1 << 5) & 0xe0);
					b22 = b1 & 0xf8;
					b0 = pSrcRow[(k+1)*srcPitch + (m+1)*2];
					b1 = pSrcRow[(k+1)*srcPitch + (m+1)*2 + 1];
					b30 = (b0 << 3) & 0xf8;
					b31 = ((b0 >> 3) & 0x1c) | ((b1 << 5) & 0xe0);
					b32 = b1 & 0xf8;

					temp = (b00 + b10 + b20 + b30) >> 2;
					destBuf[i*destPitch + j*3] = (BYTE)temp;
					temp = (b01 + b11 + b21 + b31) >> 2;
					destBuf[i*destPitch + j*3 + 1] = (BYTE)temp; 
					temp = (b02 + b12 + b22 + b32) >> 2;
					destBuf[i*destPitch + j*3 + 2] = (BYTE)temp;
				}
			}
			break;

		case D3DFMT_R8G8B8:
		case D3DFMT_A8R8G8B8:
		case D3DFMT_X8R8G8B8:
			//Log("JuceGetHalfSizeRGB: source format = D3DFMT_R8G8B or D3DFMT_A8R8G8B8 or D3DFMT_X8R8G8B8");
			if (format == D3DFMT_R8G8B8) sbpp = 3; else sbpp = 4;
			for (i=0, k=height-2; i<dHeight, k>=0; i++, k-=2)
			{
				for (j=0, m=0; j<dWidth, m<width-1; j++, m+=2)
				{
					/*
					// fetch 1 pixel: nearest neigbour
					destBuf[i*destPitch + j*3] = pSrcRow[k*srcPitch + m*sbpp];
					destBuf[i*destPitch + j*3 + 1] = pSrcRow[k*srcPitch + m*sbpp + 1];
					destBuf[i*destPitch + j*3 + 2] = pSrcRow[k*srcPitch + m*sbpp + 2];
					*/

					// fetch 4 pixels: bilinear interpolation
					temp = (pSrcRow[k*srcPitch + m*sbpp] + 
						    pSrcRow[k*srcPitch + (m+1)*sbpp] + 
						    pSrcRow[(k+1)*srcPitch + m*sbpp] +
						    pSrcRow[(k+1)*srcPitch + (m+1)*sbpp]) >> 2;
					destBuf[i*destPitch + j*3] = (BYTE)temp;
					temp = (pSrcRow[k*srcPitch + m*sbpp + 1] + 
						    pSrcRow[k*srcPitch + (m+1)*sbpp + 1] + 
						    pSrcRow[(k+1)*srcPitch + m*sbpp + 1] +
						    pSrcRow[(k+1)*srcPitch + (m+1)*sbpp + 1]) >> 2;
					destBuf[i*destPitch + j*3 + 1] = (BYTE)temp;
					temp = (pSrcRow[k*srcPitch + m*sbpp + 2] + 
						    pSrcRow[k*srcPitch + (m+1)*sbpp + 2] + 
						    pSrcRow[(k+1)*srcPitch + m*sbpp + 2] +
						    pSrcRow[(k+1)*srcPitch + (m+1)*sbpp + 2]) >> 2;
					destBuf[i*destPitch + j*3 + 2] = (BYTE)temp;
				}
			}
			break;
	}

	// unlock rect
	surf->UnlockRect();
	//Log("Rect unlocked.");
	
	// release surface
	surf->Release();

	return S_OK;
}

void InvalidateAndDelete9()
{
	InvalidateDeviceObjects(g_dev, true);
	Log("InvalidateDeviceObjects done.");
	DeleteDeviceObjects(g_dev);
	Log("DeleteDeviceObjects done.");

	/* release bufffer */
	if (g_rgbBuf != NULL)
	{
		HeapFree(procHeap, 0, g_rgbBuf);
		g_rgbBuf = NULL;
		Log("g_rgbBuf freed.");
	}
}

void InitHooksD9()
{
	//MasterLog(g_myinfo.logfile, g_myinfo.processfile, " uses Direct3D9");
	
	// set function pointer to capture frame for video
	GetPixels = (g_config.fullSizeVideo) ? JuceGetRGB : JuceGetHalfSizeRGB;

	if (!g_initDone)
	{
		/* determine my directory */
		ZeroMemory(g_myinfo.dir, BUFLEN);
		ZeroMemory(g_myinfo.file, BUFLEN);
		GetModuleFileName(hInst, g_myinfo.file, MAX_FILEPATH);
		lstrcpy(g_myinfo.dir, g_myinfo.file);
		char *q = g_myinfo.dir + lstrlen(g_myinfo.dir);
		while ((q != g_myinfo.dir) && (*q != '\\')) { *q = '\0'; q--; }

		/* open log file, specific for this process */
		ZeroMemory(logName, BUFLEN);
		lstrcpy(logName, g_myinfo.dir);
		lstrcat(logName, g_myinfo.shortProcessfileNoExt); 
		lstrcat(logName, ".log");

		OpenLog(logName);

		Log("Log started.");
		LogWithNumber("g_config.debug = %d", g_config.debug);
		LogWithNumber("g_config.useDirectInput = %d", g_config.useDirectInput);
		LogWithNumber("g_config.startupModeSystemWide = %d", g_config.startupModeSystemWide);
		LogWithNumber("sg_hGetMsgHook = %d", (DWORD)sg_hGetMsgHook);
		LogWithNumber("sg_bHookInstalled = %d", sg_bHookInstalled);
		LogWithNumber("sg_bUnhookDevice = %d", sg_bUnhookDevice);
		LogWithNumber("sg_bExiting = %d", sg_bExiting);

		/* verify that capture directory is set. If not, then set to g_myinfo.dir. */
		if (lstrlen(g_config.captureDir)==0) SetCaptureDir(g_myinfo.dir);

		LARGE_INTEGER freq = {0, 0};
		if (QueryPerformanceFrequency(&freq)) Log("Performance counter exists.");
		LogWithNumber("Performance counter frequency (hi): %u", freq.HighPart);
		LogWithNumber("Performance counter frequency (lo): %u", freq.LowPart);

		g_freq = freq.LowPart;

		/* determine frame capturing algorithm */
		g_algorithm = SearchCustomConfig(g_myinfo.processfile, g_myinfo.dir, &customConfig) ? 
			CUSTOM : ADAPTIVE;
		if (customConfig)
		{
			g_customFrameRate = customConfig->frameRate;
			g_customFrameWeight = customConfig->frameWeight;
		}

		g_initDone = TRUE;
	}

	// save handle to process' heap
	procHeap = GetProcessHeap();

	/* hook Present() and Reset() */
	HookDeviceMethods9(hD3D9);
}

// This function is called by the main DLL thread, when
// the DLL is about to detach. All the memory clean-ups should be done here.
void GraphicsCleanup9()
{
	if (customConfig) HeapFree(GetProcessHeap(), 0, customConfig);

	// restore IDirect3D9Device methods
	if (g_D3D9_Present != NULL && g_codeFragment_p9[0] != 0)
	{
		try 
		{
			memcpy(g_D3D9_Present, g_codeFragment_p9, 5);
			VirtualProtect(g_D3D9_Present, 8, g_savedProtection_p9, &g_savedProtection_p9);
		}
		catch (...)
		{
			TRACE("GraphicsCleanup9: problem restoring Present code fragment.");
		}
	}
	if (g_D3D9_Reset != NULL && g_codeFragment_r9[0] != 0)
	{
		try 
		{
			memcpy(g_D3D9_Reset, g_codeFragment_r9, 5);
			VirtualProtect(g_D3D9_Reset, 8, g_savedProtection_r9, &g_savedProtection_r9);
		}
		catch (...)
		{
			TRACE("GraphicsCleanup9: problem restoring Reset code fragment.");
		}
	}
	if (g_D3D9_AddRef != NULL && pAddRefPtr != NULL)
	{
		*pAddRefPtr = (DWORD)g_D3D9_AddRef;
	}
	if (g_D3D9_Release != NULL && pReleasePtr != NULL)
	{
		*pReleasePtr = (DWORD)g_D3D9_Release;
	}

	// uninstall keyboard hook (if still installed).
	UninstallKeyboardHook();

	TRACE("GraphicsCleanup9: done.");
}

// It looks like at certain points, vtable entries get restored to its original values.
// If that happens, we need to re-assign them to our functions again.
// NOTE: we don't want blindly re-assign, because there can be other programs
// hooking on the same methods. Therefore, we only re-assign if we see that
// original addresses are restored by the system.
void VerifyHooks(IDirect3DDevice9* dev)
{
	DWORD* vtable = (DWORD*)(*((DWORD*)dev));
	if (vtable[ADDREF_POS] == (DWORD)g_D3D9_AddRef)
	{
		vtable[ADDREF_POS] = (DWORD)JuceAddRef9;
		TRACE("VerifyHooks: dev->AddRef() re-hooked.");
	}
	if (vtable[RELEASE_POS] == (DWORD)g_D3D9_Release)
	{
		vtable[RELEASE_POS] = (DWORD)JuceRelease9;
		TRACE("VerifyHooks: dev->Release() re-hooked.");
	}
}

// verifies that JMP-implants are still in place.
void VerifyD9()
{
	HookDeviceMethods9(GetModuleHandle("d3d9.dll"));
}

