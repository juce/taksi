#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include "GL/gl.h"
#include "glext.h"

#define _COMPILING_MYDLL
#include "mydll.h"

#include "config.h"
#include "log.h"
#include "dinp.h"
#include "shared.h"
#include "util.h"
#include "imageutil.h"
#include "graphx_ogl.h"

extern HINSTANCE hInst;
HMODULE hOpenGL = NULL;
extern HMODULE hDI;

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

// frame rate calculators
extern float g_frameRate;
extern DWORD g_skipped;
extern DWORD g_lastOverhead;
extern DWORD g_freq;
extern DWORD g_customFrameRate;
extern float g_customFrameWeight;

extern DWORD* g_pLoadLibraryPtr;

// custom config (if any exists for current process)
static TAXI_CUSTOM_CONFIG* customConfig = NULL;

static HANDLE procHeap = NULL;
// name of the process-specific log
static char logName[BUFLEN];

// video capture parameters
extern ALGORITHM_TYPE g_algorithm;

static BOOL okToHook = false;

typedef __declspec(dllexport) BOOL (APIENTRY *SWAPBUFFERS)(HDC);
typedef __declspec(dllexport) void (APIENTRY *GLUTSWAPBUFFERS)(void);

static SWAPBUFFERS orgSwapBuffers = NULL;
static SWAPBUFFERS orgWglSwapBuffers = NULL;
DWORD* pIatEntry = NULL;

// boolean flag to indicate whether to use keyboard hook procedure
static bool g_bUseKeyboardHook = true;

// Handle of application window
HWND g_hWnd;

extern HANDLE videoFile;

// back-buffer dimensions
extern UINT g_bbWidth;
extern UINT g_bbHeight;

// buffer to keep current frame 
static BYTE* g_rgbBuf = NULL;
static INT g_rgbSize = 0;
static INT g_Pitch = 0;

// helper buffer for g_rgbBuf
static BYTE* g_rgbBigBuf = NULL;
static INT g_rgbBigSize = 0;

// indicator dimensions/position
#define IWIDTH 16
#define IHEIGHT 16
#define INDX 8
#define INDY 8

static void MakeScreenShot(BYTE* rgbBuf, SIZE_T size, INT pitch);
static void MakeSmallScreenShot(BYTE* rgbBuf, SIZE_T size, INT pitch);
static void GetFullSizeFrame(BYTE* rgbBuf, SIZE_T size, INT pitch);
static void GetHalfSizeFrame(BYTE* rgbBuf, SIZE_T size, INT pitch);
static void GetHalfSizeImage(BYTE* srcBuf, int width, int height, BYTE* destBuf);
void HookSwapBuffers(HMODULE hMod);

__declspec(dllexport) BOOL APIENTRY JuceSwapBuffers(HDC  hdc);
__declspec(dllexport) BOOL APIENTRY JuceWglSwapBuffers(HDC  hdc);

static BOOL hasOpenGLInfo = FALSE;
PFNGLACTIVETEXTUREARBPROC glActiveTextureARB = NULL;
static GLint g_maxTexUnits = 0;
static GLenum g_textureUnitNums[32] = {
	GL_TEXTURE0_ARB,  GL_TEXTURE1_ARB,  GL_TEXTURE2_ARB,  GL_TEXTURE3_ARB,
	GL_TEXTURE4_ARB,  GL_TEXTURE5_ARB,  GL_TEXTURE6_ARB,  GL_TEXTURE7_ARB,
	GL_TEXTURE8_ARB,  GL_TEXTURE9_ARB,  GL_TEXTURE10_ARB, GL_TEXTURE11_ARB,
	GL_TEXTURE12_ARB, GL_TEXTURE13_ARB, GL_TEXTURE14_ARB, GL_TEXTURE15_ARB,
	GL_TEXTURE16_ARB, GL_TEXTURE17_ARB, GL_TEXTURE18_ARB, GL_TEXTURE19_ARB,
	GL_TEXTURE20_ARB, GL_TEXTURE21_ARB, GL_TEXTURE22_ARB, GL_TEXTURE23_ARB,
	GL_TEXTURE24_ARB, GL_TEXTURE25_ARB, GL_TEXTURE26_ARB, GL_TEXTURE27_ARB,
	GL_TEXTURE28_ARB, GL_TEXTURE29_ARB, GL_TEXTURE30_ARB, GL_TEXTURE31_ARB,
};

DWORD g_addr = 0;
BYTE g_codeFragment[5] = {0,0,0,0,0};
BYTE g_jmp[5] = {0,0,0,0,0};
DWORD g_savedProtection = 0;

/* function pointers to allow explicit run-time linking. */
PFNGLBEGINPROC GlBegin = NULL;
PFNGLDISABLEPROC GlDisable = NULL;
PFNGLBINDTEXTUREPROC GlBindTexture = NULL;
PFNGLCOLOR3FPROC GlColor3f = NULL;
PFNGLENABLEPROC GlEnable = NULL;
PFNGLENDPROC GlEnd = NULL;
PFNGLGETINTEGERVPROC GlGetIntegerv = NULL;
PFNGLGETSTRINGPROC GlGetString = NULL;
PFNGLLOADIDENTITYPROC GlLoadIdentity = NULL;
PFNGLMATRIXMODEPROC GlMatrixMode = NULL;
PFNGLORTHOPROC GlOrtho = NULL;
PFNGLPOPATTRIBPROC GlPopAttrib = NULL;
PFNGLPOPMATRIXPROC GlPopMatrix = NULL;
PFNGLPUSHATTRIBPROC GlPushAttrib = NULL;
PFNGLPUSHMATRIXPROC GlPushMatrix = NULL;
PFNGLREADBUFFERPROC GlReadBuffer = NULL;
PFNGLREADPIXELSPROC GlReadPixels = NULL;
PFNGLSHADEMODELPROC GlShadeModel = NULL;
PFNGLVERTEX2IPROC GlVertex2i = NULL;
PFNGLVIEWPORTPROC GlViewport = NULL;
PFNWGLGETCURRENTDCPROC WglGetCurrentDC = NULL;
PFNWGLGETPROCADDRESSPROC WglGetProcAddress = NULL;

// function pointer for grabbing RGB-data
typedef void (*PFNGETFRAMEPROC)(BYTE*, SIZE_T, INT);
static PFNGETFRAMEPROC GetFrame;


///////////////////////////////////////
// FUNCTIONS
///////////////////////////////////////

/* Restore original OpenGL functions */
EXTERN_C _declspec(dllexport) void RestoreOpenGLFunctions()
{
	Log("RestoreOpenGLFunctions: done.");
}

/**
 * Search through all used DLLs to see if any of them exports
 * a function with specified name.
 */
/*
BOOL LookupExportedFunction(char* functionName, DWORD *func, HMODULE* hMod)
{
	HANDLE hModuleSnap = INVALID_HANDLE_VALUE; 
	MODULEENTRY32 me32; 

	//  Take a snapshot of all modules in the specified process. 
	hModuleSnap = CreateToolhelp32Snapshot( TH32CS_SNAPMODULE, GetCurrentProcessId() ); 
	if( hModuleSnap == INVALID_HANDLE_VALUE ) 
	{ 
		return( FALSE ); 
	} 

	//  Set the size of the structure before using it. 
	me32.dwSize = sizeof( MODULEENTRY32 ); 

	//  Retrieve information about the first module, 
	//  and exit if unsuccessful 
	if( !Module32First( hModuleSnap, &me32 ) ) 
	{ 
		CloseHandle( hModuleSnap );     // Must clean up the snapshot object! 
		return( FALSE ); 
	} 

	//  Now walk the module list of the process 
	do 
	{ 
		DWORD addr = (DWORD)GetProcAddress((HMODULE)me32.modBaseAddr, functionName);
		if (addr != 0)
		{
			*func = addr;
			*hMod = (HMODULE)me32.modBaseAddr;

			TRACE4("Found exported symbol in module: %s", me32.szModule);

			CloseHandle( hModuleSnap ); 
			return TRUE;
		}

	} while( Module32Next( hModuleSnap, &me32 ) ); 

	CloseHandle( hModuleSnap ); 
	return FALSE;
}
*/

/**
 * Search through IATs for all modules of this process to see
 * if any of the entries contains a given function address.
 */
/*
BOOL FindEntryInIAT(DWORD procAddr, DWORD** pFunction)
{
	HANDLE hModuleSnap = INVALID_HANDLE_VALUE; 
	MODULEENTRY32 me32; 

	//  Take a snapshot of all modules in the specified process. 
	hModuleSnap = CreateToolhelp32Snapshot( TH32CS_SNAPMODULE, GetCurrentProcessId() ); 
	if( hModuleSnap == INVALID_HANDLE_VALUE ) 
	{ 
		return( FALSE ); 
	} 

	//  Set the size of the structure before using it. 
	me32.dwSize = sizeof( MODULEENTRY32 ); 

	//  Retrieve information about the first module, 
	//  and exit if unsuccessful 
	if( !Module32First( hModuleSnap, &me32 ) ) 
	{ 
		CloseHandle( hModuleSnap );     // Must clean up the snapshot object! 
		return( FALSE ); 
	} 

	//  Now walk the module list of the process 
	do 
	{ 
		LogWithString("Searching for usage in: %s", me32.szModule);

		HMODULE hMod = (HMODULE)me32.modBaseAddr;
		IMAGE_IMPORT_DESCRIPTOR* imports = GetModuleImportDescriptors(hMod);
		if (imports == NULL) continue;

		// go through each IMAGE_IMPORT_DESCRIPTOR and check if its IAT
		// has an address of the function we're looking for (procAddr)
		int k=0;
		while (imports[k].Characteristics != 0)
		{
			IMAGE_THUNK_DATA* func = (IMAGE_THUNK_DATA*)(me32.modBaseAddr + imports[k].FirstThunk);
			TRACE4("  Imported module: %s", (char*)(me32.modBaseAddr + imports[k].Name));

			int j=0;
			while (func[j].u1.Function != 0)
			{
				//TRACE4("    f: %08x", (DWORD)func[j].u1.Function);

				if ((DWORD)func[j].u1.Function == procAddr)
				{
					TRACE4("Found usage in module: %s", me32.szModule);

					*pFunction = (DWORD*)&(func[j].u1.Function);
					CloseHandle(hModuleSnap);
					return TRUE;
				}
				j++;
			}
			TRACE2("    Looked through %d functions.", j);
			k++;
		}

	} while( Module32Next( hModuleSnap, &me32 ) ); 

	CloseHandle( hModuleSnap ); 
	return FALSE;
}
*/
 
/* Hooks multiple functions */
void HookSwapBuffers(HMODULE hMod)
{
	// hook wglSwapBuffers, using code modifications at run-time.
	// ALGORITHM: we overwrite the beginning of real wglSwapBuffers
	// with a JMP instruction to our routine (JuceWglSwapBuffers).
	// When our routine gets called, first thing that it does - it restores 
	// the original bytes of wglSwapBuffers, then performs its pre-call tasks, 
	// then calls wglSwapBuffers, then does post-call tasks, then writes
	// the JMP instruction back into the beginning of wglSwapBuffers, and
	// returns.
	
	if (!hMod)
	{
		Log("HookSwapBuffers: opengl32 is not loaded at this moment. No hooking performed.");
		return;
	}
  	
	orgWglSwapBuffers = (SWAPBUFFERS)GetProcAddress(hMod, "wglSwapBuffers");
	if (orgWglSwapBuffers)
	{
		TRACE("HookSwapBuffers: checking JMP-implant...");
		if (memcmp(orgWglSwapBuffers, g_jmp, 5)==0)
		{
			TRACE("HookSwapBuffers: wglSwapBuffers already has JMP-implant.");
		}
		else
		{
			TRACE2("HookSwapBuffers: orgWglSwapBuffers = %08x", (DWORD)orgWglSwapBuffers);
			TRACE2("HookSwapBuffers: JuceWglSwapBuffers = %08x", (DWORD)JuceWglSwapBuffers);

			// unconditional JMP to relative address is 5 bytes.
			g_jmp[0] = 0xe9;
			DWORD addr = (DWORD)JuceWglSwapBuffers - (DWORD)orgWglSwapBuffers - 5;
			TRACE2("JMP %08x", addr);
			memcpy(g_jmp + 1, &addr, sizeof(DWORD));

			memcpy(g_codeFragment, orgWglSwapBuffers, 5);
			DWORD newProtection = PAGE_EXECUTE_READWRITE;
			if (VirtualProtect(orgWglSwapBuffers, 8, newProtection, &g_savedProtection))
			{
				memcpy(orgWglSwapBuffers, g_jmp, 5);
				TRACE("HookSwapBuffers: JMP-hook planted.");
			}
		}
	}

	TRACE("HookSwapBuffers: done.");
}

/* Determine HWND for our app window, and window dimensions. */
void GetWindowInfoAndDims()
{
	HDC hdc = WglGetCurrentDC();
	hProcWnd = WindowFromDC(hdc);

	RECT rect;
	GetClientRect(hProcWnd, &rect);

	g_bbWidth = rect.right;
	g_bbHeight = rect.bottom;
}

/* Draws indicator in the top-left corner. */
void DrawIndicator()
{
	//TRACE("Drawing indicator...");

	GlPushAttrib(GL_ALL_ATTRIB_BITS);

	GlViewport(0, g_bbHeight-32, 32, 32);
	GlMatrixMode(GL_PROJECTION);
	GlPushMatrix();
	GlLoadIdentity();
	GlOrtho(0.0, 32.0, 0.0, 32.0, -1.0, 1.0);
	GlMatrixMode(GL_MODELVIEW);
	GlPushMatrix();
	GlLoadIdentity();

	GlDisable(GL_LIGHTING);
	GlDisable(GL_TEXTURE_1D);
	if (glActiveTextureARB)
	{
		for (int i=0; i<g_maxTexUnits; i++)
		{
			//TRACE2("disabling GL_TEXTURE%d_ARB...", i);
			glActiveTextureARB(g_textureUnitNums[i]);
			GlDisable(GL_TEXTURE_2D);
		}
	}
	else
	{
		GlDisable(GL_TEXTURE_2D);
	}
    //GlBindTexture(GL_TEXTURE_2D, 0);
    //GlDisable(GL_TEXTURE_2D);
    //GlDisable(GL_TEXTURE_CUBE_MAP);

	GlDisable(GL_DEPTH_TEST);
	//GlDisable(GL_STENCIL_TEST);
	GlShadeModel(GL_FLAT);

	// set appropriate color for the indicator
	if (g_mystate.bNowRecording)
	{
		GlColor3f(1.0, 0.26, 0.0); // RED: recording mode
	}
	else if (g_mystate.bSystemWideMode)
	{
		GlColor3f(0.26, 0.53, 1.0); // BLUE: system wide hooking mode
	}
	else
	{
		GlColor3f(0.53, 1.0, 0.0); // GREEN: normal mode
	}
	GlBegin(GL_POLYGON);
		GlVertex2i(INDX, INDY);
		GlVertex2i(INDX, INDY + IHEIGHT);
		GlVertex2i(INDX + IWIDTH, INDY + IHEIGHT);
		GlVertex2i(INDX + IWIDTH, INDY);
	GlEnd();
	// black outline
	GlColor3f(0.0, 0.0, 0.0);
	GlBegin(GL_LINE_LOOP);
		GlVertex2i(INDX, INDY);
		GlVertex2i(INDX, INDY + IHEIGHT);
		GlVertex2i(INDX + IWIDTH, INDY + IHEIGHT);
		GlVertex2i(INDX + IWIDTH, INDY);
	GlEnd();

	GlMatrixMode(GL_PROJECTION);
	GlPopMatrix();
	GlMatrixMode(GL_MODELVIEW);
	GlPopMatrix();

	GlPopAttrib();
}

/* Get OpenGL version and extensions info */
void GetVersionAndExtensionsInfo()
{
	char* ver = (char*)GlGetString(GL_VERSION);
	TRACE4("OpenGL Version: %s", ver);
	char* ext = (char*)GlGetString(GL_EXTENSIONS);
	TRACE4("OpenGL Extensions: %s", ext);
	if (ext && strstr(ext, "GL_ARB_multitexture") != 0)
	{
		glActiveTextureARB = (PFNGLACTIVETEXTUREARBPROC)WglGetProcAddress("glActiveTextureARB");
		TRACE2("glActiveTextureARB = %08x", (DWORD)glActiveTextureARB);

		GlGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &g_maxTexUnits);
		TRACE2("Maximum texture units: %d", g_maxTexUnits);
	}
	if (ver && ext) hasOpenGLInfo = TRUE;
}

/* Swap 'R' and 'B' components for each pixel. */
/* NOTE: image width (in pixels) must be a multiple of 4. */
void SwapColorComponents(BYTE* rgbBuf, int g_bbWidth, int g_bbHeight)
{
	TRACE("SwapColorComponents: CALLED.");
	for (int i=0; i<g_bbHeight; i++)
	{
		for (int j=0; j<g_bbWidth; j++)
		{
			int i0 = i*g_bbWidth*3 + j*3;
			int i2 = i0 + 2;
			BYTE b0 = rgbBuf[i0];
			BYTE b2 = rgbBuf[i2];
			rgbBuf[i0] = b2;
			rgbBuf[i2] = b0;
		}
	}
}

/* Makes a screen shot from back buffer. */
void MakeScreenShot(BYTE* rgbBuf, SIZE_T size, INT pitch)
{
	// select back buffer
	GlReadBuffer(GL_BACK);

	// read the pixels data
	GlReadPixels(0, 0, g_bbWidth, g_bbHeight, GL_RGB, GL_UNSIGNED_BYTE, rgbBuf);

	// Adjust pixel encodings: RGB -> BGR
	SwapColorComponents(rgbBuf, g_bbWidth, g_bbHeight);

	// save image as bitmap
	if (FAILED(SaveAsBMP(NULL, rgbBuf, size, g_bbWidth, g_bbHeight, 3)))
	{
		Log("MakeScreenShot: SaveAsBMP failed.");
	}

	return;
}

/* Makes a small screen shot from back buffer. */
void MakeSmallScreenShot(BYTE* rgbBuf, SIZE_T size, INT pitch)
{
	// select back buffer
	GlReadBuffer(GL_BACK);

	// read the pixels data
	GlReadPixels(0, 0, g_bbWidth, g_bbHeight, GL_RGB, GL_UNSIGNED_BYTE, rgbBuf);

	int width = g_bbWidth/2;
	int height = g_bbHeight/2;
	int smallBufSize = width * height * 3;
	BYTE* destBuf = (BYTE*)HeapAlloc(procHeap, HEAP_ZERO_MEMORY, smallBufSize);
	if (destBuf == NULL)
	{
		Log("MakeSmallScreenShot: unable to allocate buffer.");
		return;
	}
	
	// minimize the image
	GetHalfSizeImage(rgbBuf, g_bbWidth, g_bbHeight, destBuf);

	// Adjust pixel encodings: RGB -> BGR
	SwapColorComponents(destBuf, width, height);

	// save image as bitmap
	if (FAILED(SaveAsBMP(NULL, destBuf, smallBufSize, width, height, 3)))
	{
		Log("MakeSmallScreenShot: SaveAsBMP failed.");
		return;
	}

	HeapFree(procHeap, 0, destBuf);
	return;
}

/* Makes a minimized image (1/2 size in each direction). */
void GetHalfSizeImage(BYTE* srcBuf, int width, int height, BYTE* destBuf)
{
	BYTE temp;
	int i,j,k,m;

	int dWidth = width/2;
	int dHeight = height/2;
	int sbpp = 3; // source bytes-per-pixel

	int srcPitch = width * sbpp;
	int destPitch = dWidth * sbpp;

	for (i=0, k=0; i<dHeight, k<height-1; i++, k+=2)
	{
		for (j=0, m=0; j<dWidth, m<width-1; j++, m+=2)
		{
			/*
			// fetch 1 pixel: nearest neigbour
			destBuf[i*destPitch + j*3] = srcBuf[k*srcPitch + m*sbpp];
			destBuf[i*destPitch + j*3 + 1] = srcBuf[k*srcPitch + m*sbpp + 1];
			destBuf[i*destPitch + j*3 + 2] = srcBuf[k*srcPitch + m*sbpp + 2];
			*/

			// fetch 4 pixels: bilinear interpolation
			temp = (srcBuf[k*srcPitch + m*sbpp] + 
					srcBuf[k*srcPitch + (m+1)*sbpp] + 
					srcBuf[(k+1)*srcPitch + m*sbpp] +
					srcBuf[(k+1)*srcPitch + (m+1)*sbpp]) >> 2;
			destBuf[i*destPitch + j*3] = (BYTE)temp;
			temp = (srcBuf[k*srcPitch + m*sbpp + 1] + 
					srcBuf[k*srcPitch + (m+1)*sbpp + 1] + 
					srcBuf[(k+1)*srcPitch + m*sbpp + 1] +
					srcBuf[(k+1)*srcPitch + (m+1)*sbpp + 1]) >> 2;
			destBuf[i*destPitch + j*3 + 1] = (BYTE)temp;
			temp = (srcBuf[k*srcPitch + m*sbpp + 2] + 
					srcBuf[k*srcPitch + (m+1)*sbpp + 2] + 
					srcBuf[(k+1)*srcPitch + m*sbpp + 2] +
					srcBuf[(k+1)*srcPitch + (m+1)*sbpp + 2]) >> 2;
			destBuf[i*destPitch + j*3 + 2] = (BYTE)temp;
		}
	}
}

/* Gets current frame to be stored in video file. */
void GetFullSizeFrame(BYTE* rgbBuf, SIZE_T size, INT pitch)
{
	// select back buffer
	GlReadBuffer(GL_BACK);

	// read the pixels data
	GlReadPixels(0, 0, g_bbWidth, g_bbHeight, GL_RGB, GL_UNSIGNED_BYTE, rgbBuf);

	// Adjust pixel encodings: RGB -> BGR
	SwapColorComponents(rgbBuf, g_bbWidth, g_bbHeight);
}

/* Gets current frame to be stored in video file. */
void GetHalfSizeFrame(BYTE* rgbBuf, SIZE_T size, INT pitch)
{
	if (g_rgbBigBuf == NULL)
	{
		// allocate big buffer for pixel data
		g_rgbBigSize = g_bbWidth * g_bbHeight * 3;
		g_rgbBigBuf = (BYTE*)HeapAlloc(procHeap, HEAP_ZERO_MEMORY, g_rgbBigSize);
	}

	// select back buffer
	GlReadBuffer(GL_BACK);

	// read the pixels data
	GlReadPixels(0, 0, g_bbWidth, g_bbHeight, GL_RGB, GL_UNSIGNED_BYTE, g_rgbBigBuf);

	// minimize the image
	GetHalfSizeImage(g_rgbBigBuf, g_bbWidth, g_bbHeight, rgbBuf);

	// Adjust pixel encodings: RGB -> BGR
	SwapColorComponents(rgbBuf, g_bbWidth/2, g_bbHeight/2);
}

/* New SwapBuffers function */
__declspec(dllexport) BOOL APIENTRY JuceSwapBuffers(HDC  hdc)
{
	TRACE("JuceSwapBuffers: CALLED.");

	// if haven't done yet, query for version info and extensions
	if (!hasOpenGLInfo) GetVersionAndExtensionsInfo();

	// draw indicator
	DrawIndicator();

	// call original function
	return orgSwapBuffers(hdc);
}

/* New wglSwapBuffers function */
__declspec(dllexport) BOOL APIENTRY JuceWglSwapBuffers(HDC  hdc)
{
	TRACE("--------------------------------");
	TRACE("JuceWglSwapBuffers: called.");
	BYTE* dest = (BYTE*)orgWglSwapBuffers;

	// put back saved code fragment
	dest[0] = g_codeFragment[0];
	*((DWORD*)(dest + 1)) = *((DWORD*)(g_codeFragment + 1));

	TRACE2("LoadLibrary used in Import Table here: %08x", (DWORD)g_pLoadLibraryPtr);

	// determine whether this frame needs to be grabbed when recording.
	DWORD frameDups = GetFrameDups();

	// if haven't done yet, query for version info and extensions
	if (!hasOpenGLInfo) GetVersionAndExtensionsInfo();

	if (hProcWnd == NULL) GetWindowInfoAndDims();

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
				TRACE("JuceWglSwapBuffers: KEYBOARD INIT: using DirectInput.");
				GetDirectInputCreator();

				// install dummy keyboard hook so that we don't get unmapped,
				// when going to exclusive mode (i.e. unhooking GetMsgProc)
				DWORD tid = GetWindowThreadProcessId(hProcWnd, NULL);
				InstallDummyKeyboardHook(tid);

				g_mystate.bKeyboardInitDone = TRUE;
			}
			else
			{
				TRACE("JuceWglSwapBuffers: KEYBOARD INIT: Unable to load dinput8.dll.");
			}
		}

		// if we're not done at this point, use keyboard hook
		if (!g_mystate.bKeyboardInitDone)
		{
			TRACE("JuceWglSwapBuffers: KEYBOARD INIT: using keyboard hook.");

			/* install keyboard hook */
			DWORD tid = GetWindowThreadProcessId(hProcWnd, NULL);
			InstallKeyboardHook(tid);
		}

		// Goto "exclusive" mode (i.e. unhook GetMsgProc)
		// if specified so in the g_mystate
		if (g_mystate.bUnhookGetMsgProc)
		{
			Log("JuceWglSwapBuffers: Signaling to unhook GetMsgProc.");
			if (UninstallTheHook())
			{
				Log("JuceWglSwapBuffers: GetMsgProc unhooked.");
				// change indicator to "green"
			}
			g_mystate.bUnhookGetMsgProc = FALSE;
		}

		// keyboard configuration done
		g_mystate.bKeyboardInitDone = TRUE;
	}

	// Process DirectInput input
	if (g_config.useDirectInput) ProcessDirectInput(&g_config);

	// process video recording toggles
	if (g_mystate.bStartRecording)
	{
		TRACE("JuceWglSwapBuffers: calling OpenAVIFile.");
		videoFile = OpenAVIFile();
		g_mystate.bStartRecording = false;
		Log("Recording started.");
		g_mystate.bNowRecording = true;
	}
	// Close AVI file, if asked so.
	else if (g_mystate.bStopRecording)
	{
		TRACE("JuceWglSwapBuffers: calling CloseAVIFile.");
		CloseAVIFile(videoFile);
		g_mystate.bStopRecording = false;
		Log("Recording stopped.");
		g_mystate.bNowRecording = false;
	}

	/* make screen shot, if requested so. */
	else if (g_mystate.bMakeScreenShot)
	{
		// allocate buffer for pixel data
		INT bytes = g_bbWidth * 3;
		INT rem = bytes % sizeof(ULONG);
		INT pitch = (rem == 0) ? bytes : bytes + sizeof(ULONG) - rem;
		SIZE_T size = pitch * g_bbHeight;
		BYTE* rgbBuf = (BYTE*)HeapAlloc(procHeap, HEAP_ZERO_MEMORY, size);

		// shoot
		MakeScreenShot(rgbBuf, size, pitch);

		// free the buffer
		HeapFree(procHeap, 0, rgbBuf);

		g_mystate.bMakeScreenShot = false;
	}

	/* make small screen shot, if requested so. */
	else if (g_mystate.bMakeSmallScreenShot)
	{
		// allocate buffer for pixel data
		INT bytes = g_bbWidth * 3;
		INT rem = bytes % sizeof(ULONG);
		INT pitch = (rem == 0) ? bytes : bytes + sizeof(ULONG) - rem;
		SIZE_T size = pitch * g_bbHeight;
		BYTE* rgbBuf = (BYTE*)HeapAlloc(procHeap, HEAP_ZERO_MEMORY, size);

		// shoot
		MakeSmallScreenShot(rgbBuf, size, pitch);

		// free the buffer
		HeapFree(procHeap, 0, rgbBuf);

		g_mystate.bMakeSmallScreenShot = false;
	}

	//TRACE2("g_mystate.bNowRecording = %d", g_mystate.bNowRecording);
	// write AVI-file, if in recording mode. 
	if (g_mystate.bNowRecording)
	{
		if (frameDups > 0)
		{
			DWORD start = QPC();
			
			if (g_rgbBuf == NULL)
			{
				INT divider = (g_config.fullSizeVideo) ? 1 : 2;

				// allocate buffer for pixel data
				g_Pitch = g_bbWidth / divider * 3;
				g_rgbSize = g_Pitch * g_bbHeight / divider;
				g_rgbBuf = (BYTE*)HeapAlloc(procHeap, HEAP_ZERO_MEMORY, g_rgbSize);
			}

			// shoot a frame
			CLOCK_START(g_clockIndent, g_debugClock);
			CLOCK_START(g_clockIndent, g_debugClock);

			GetFrame(g_rgbBuf, g_rgbSize, g_Pitch);

			CLOCK_STOP_AND_LOG("GetFrame time:      %10d", g_clockIndent, g_debugClock);
			CLOCK_START(g_clockIndent, g_debugClock);

			WriteAVIFrame(videoFile, g_rgbBuf, g_rgbSize, frameDups);

			CLOCK_STOP_AND_LOG("WriteAVIFrame time: %10d", g_clockIndent, g_debugClock);
			CLOCK_STOP_AND_LOG("-------> total frame shot time: %d", g_clockIndent, g_debugClock);

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

	// draw the indicator 
	if (g_mystate.bIndicate || g_mystate.bNowRecording) DrawIndicator();

	// call original function
	BOOL res = orgWglSwapBuffers(hdc);

	/* check unhook flag */
	if (GetUnhookFlag())
	{
		// uninstall keyboard hook
		UninstallKeyboardHook();
	}

	// put JMP instruction again
	dest[0] = g_jmp[0];
	*((DWORD*)(dest + 1)) = *((DWORD*)(g_jmp + 1));

	return res;
}

void InitHooksOGL()
{
	//MasterLog(g_myinfo.logfile, g_myinfo.processfile, " uses OpenGL");

	// set function pointer to capture frame for video
	GetFrame = (g_config.fullSizeVideo) ? GetFullSizeFrame : GetHalfSizeFrame;

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

	/* initialize function pointers */
	GlBegin = (PFNGLBEGINPROC)GetProcAddress(hOpenGL, "glBegin");
	GlDisable = (PFNGLDISABLEPROC)GetProcAddress(hOpenGL, "glDisable");
	GlBindTexture = (PFNGLBINDTEXTUREPROC)GetProcAddress(hOpenGL, "glBindTexture");
	GlColor3f = (PFNGLCOLOR3FPROC)GetProcAddress(hOpenGL, "glColor3f");
	GlEnable = (PFNGLENABLEPROC)GetProcAddress(hOpenGL, "glEnable");
	GlEnd = (PFNGLENDPROC)GetProcAddress(hOpenGL, "glEnd");
	GlGetIntegerv = (PFNGLGETINTEGERVPROC)GetProcAddress(hOpenGL, "glGetIntegerv");
	GlGetString = (PFNGLGETSTRINGPROC)GetProcAddress(hOpenGL, "glGetString");
	GlLoadIdentity = (PFNGLLOADIDENTITYPROC)GetProcAddress(hOpenGL, "glLoadIdentity");
	GlMatrixMode = (PFNGLMATRIXMODEPROC)GetProcAddress(hOpenGL, "glMatrixMode");
	GlOrtho = (PFNGLORTHOPROC)GetProcAddress(hOpenGL, "glOrtho");
	GlPopAttrib = (PFNGLPOPATTRIBPROC)GetProcAddress(hOpenGL, "glPopAttrib");
	GlPopMatrix = (PFNGLPOPMATRIXPROC)GetProcAddress(hOpenGL, "glPopMatrix");
	GlPushAttrib = (PFNGLPUSHATTRIBPROC)GetProcAddress(hOpenGL, "glPushAttrib");
	GlPushMatrix = (PFNGLPUSHMATRIXPROC)GetProcAddress(hOpenGL, "glPushMatrix");
	GlReadBuffer = (PFNGLREADBUFFERPROC)GetProcAddress(hOpenGL, "glReadBuffer");
	GlReadPixels = (PFNGLREADPIXELSPROC)GetProcAddress(hOpenGL, "glReadPixels");
	GlShadeModel = (PFNGLSHADEMODELPROC)GetProcAddress(hOpenGL, "glShadeModel");
	GlVertex2i = (PFNGLVERTEX2IPROC)GetProcAddress(hOpenGL, "glVertex2i");
	GlViewport = (PFNGLVIEWPORTPROC)GetProcAddress(hOpenGL, "glViewport");
	WglGetCurrentDC = (PFNWGLGETCURRENTDCPROC)GetProcAddress(hOpenGL, "wglGetCurrentDC");
	WglGetProcAddress = (PFNWGLGETPROCADDRESSPROC)GetProcAddress(hOpenGL, "wglGetProcAddress");

	/* hook wglSwapBuffers */
	HookSwapBuffers(hOpenGL);
}

BOOL GraphicsLoopHookedOGL()
{
	return (orgWglSwapBuffers != NULL);
}

// This function is called by the main DLL thread, when
// the DLL is about to detach. All the memory clean-ups should be done here.
void GraphicsCleanupOGL()
{
	if (customConfig) HeapFree(GetProcessHeap(), 0, customConfig);

	// restore SwapBuffers
	if ((pIatEntry != NULL) && (orgSwapBuffers != NULL))
	{
		DWORD newProtection = PAGE_READWRITE;
		DWORD savedProtection = 0;

		// We need to call VirtualProtect to ensure we can write to the found
		// memory location, in case it is a part of read-only section.
		if (VirtualProtect(pIatEntry, sizeof(DWORD), newProtection, &savedProtection))
		{
			*pIatEntry = (DWORD)orgSwapBuffers;
			VirtualProtect(pIatEntry, sizeof(DWORD), savedProtection, &savedProtection);
		}
		else
		{
			TRACE("GraphicsCleanupOGL: unable to restore IAT entry (SwapBuffers).");
		}
	}

	// restore wglSwapBuffers
	if (orgWglSwapBuffers != NULL && g_codeFragment[0] != 0)
	{
		try 
		{
			memcpy(orgWglSwapBuffers, g_codeFragment, 5);
			VirtualProtect(orgWglSwapBuffers, 8, g_savedProtection, &g_savedProtection);
		}
		catch (...)
		{
			TRACE("GraphicsCleanupOGL: problem restoring wglSwapBuffers code fragment.");
		}
	}

	// release pixel buffers
	if (g_rgbBuf) HeapFree(procHeap, 0, g_rgbBuf);
	if (g_rgbBigBuf) HeapFree(procHeap, 0, g_rgbBigBuf);

	// uninstall keyboard hook (if still installed).
	UninstallKeyboardHook();

	TRACE("GraphicsCleanupOGL: done.");
}

void VerifyOGL()
{
	HookSwapBuffers(GetModuleHandle("opengl32.dll"));
}

