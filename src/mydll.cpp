/* mydll */

/**************************************
 * Visual indicators meaning:         *
 * GREEN: exclusive hooked mode       *
 * BLUE: system-wide hooked mode      *
 * RED: video recording mode          *
 **************************************/

using namespace std;

#include <windows.h>
#include <stdio.h>
#include <limits.h>
#include <list>

#include "shared.h"
#include "config.h"
#include "log.h"
#include "graphx_ogl.h"
#include "util.h"

#define _COMPILING_MYDLL
#include "mydll.h"

#define LOG "taksi-dll.log"


/**************************************
Shared by all processes variables
**************************************/
#pragma data_seg(".HKT")
HWND   sg_hServerWnd            = NULL;
HHOOK  sg_hGetMsgHook           = NULL;
BOOL   sg_bHookInstalled        = false;
BOOL   sg_bExiting              = false;
BOOL   sg_bIgnoreOverhead       = false;
BOOL   sg_bUnhookDevice         = false;
DWORD  sg_reconfCounter         = 0;
TAXI_CONFIG g_config = {
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
DXPOINTERS g_dxInfo = {0, 0, 0, 0};
#pragma data_seg()
#pragma comment(linker, "/section:.HKT,rws")

/************************** 
End of shared section 
**************************/

DWORD* g_pLoadLibraryPtr = NULL;
DWORD g_msgCount = 0;
BOOL g_D8_hooked = FALSE;
BOOL g_D9_hooked = FALSE;
BOOL g_OGL_hooked = FALSE;
BOOL g_initDone = FALSE;

// if set to TRUE, then GetMsgProc should
// not take any action at all.
BOOL g_ignoreAllMessages = FALSE;

/* DEBUGGING TOOLS */
int g_clockIndent = 0;
list<DWORD> g_debugClock;

DWORD g_reconfCounter = 0;

HANDLE g_hooker = NULL;
//BOOL safeToHook = false;
/*
HANDLE event_hookerStop8 = NULL;
HANDLE event_hookerStop9 = NULL;
HANDLE event_hookerStopOGL = NULL;
HANDLE event_hookerFuncDone8 = NULL;
HANDLE event_hookerFuncDone9 = NULL;
HANDLE event_hookerFuncDoneOGL = NULL;
*/

BOOL g_unloadD8 = FALSE;
BOOL g_unloadD9 = FALSE;
BOOL g_unloadOGL = FALSE;

int totalFrames = 0;
DWORD moviChunkSize = 4;
BITMAPINFOHEADER bitmapHeader;

ALGORITHM_TYPE g_algorithm;
DWORD g_customFrameRate = 1;
float g_customFrameWeight = 0.0;

// frame rate calculators
DWORD g_lastCount = 0;
float g_extraWeight = 0.0;
float g_frameRate = 0.0;
DWORD g_penalty = 0;
DWORD g_skipped = 1;
DWORD g_lastOverhead = 0;
DWORD g_freq = 0;

// AVI structures

#define MOVILIST_OFFSET 0x800

typedef struct {
	WORD left;
	WORD top;
	WORD right;
	WORD bottom;
} RECT8;

typedef struct {
    DWORD dwMicroSecPerFrame;
    DWORD dwMaxBytesPerSec;
    DWORD dwReserved1;
    DWORD dwFlags;
    DWORD dwTotalFrames;
    DWORD dwInitialFrames;
    DWORD dwStreams;
    DWORD dwSuggestedBufferSize;
    DWORD dwWidth;
    DWORD dwHeight;
    DWORD dwReserved[4];
} MainAVIHeader;

typedef struct {
    FOURCC fccType;
    FOURCC fccHandler;
    DWORD  dwFlags;
    DWORD  dwPriority;
    DWORD  dwInitialFrames;
    DWORD  dwScale;
    DWORD  dwRate;
    DWORD  dwStart;
    DWORD  dwLength;
    DWORD  dwSuggestedBufferSize;
    DWORD  dwQuality;
    DWORD  dwSampleSize;
    RECT8  rcFrame;
} AVIStreamHeader;

// Header structure for an AVI file with no audio stream, 
// and one video stream, which uses bitmaps without palette.
typedef struct _AVIFILEHEADER_VO
{
	FOURCC fccRiff;                  //  should be "RIFF"
	DWORD  dwRiffChunkSize;          
	FOURCC fccForm;                  //  should be "AVI "
	FOURCC list_0;                   //  should be "LIST"
	DWORD  list_0_size;
	FOURCC fccHdrl;                  //  should be "hdrl"
	FOURCC fccAvih;                  //  should be "avih"
	DWORD  dwAvihChunkSize;
	MainAVIHeader aviHeader;
	FOURCC list_1;                   //  should be "LIST"
	DWORD  list_1_size;
	FOURCC fccStrl;                  //  should be "strl"
	FOURCC fccStrh;                  //  should be "strh"
	DWORD  dwStrhChunkSize;
	AVIStreamHeader streamHeader;
	FOURCC fccStrf;                  //  should be "strf"
	DWORD  dwStrfChunkSize;
	BITMAPINFOHEADER biHeader;

} AVIFILEHEADER_VO;

/* Flags for index */
#define AVIIF_LIST          0x00000001L // chunk is a 'LIST'
#define AVIIF_KEYFRAME      0x00000010L // this frame is a key frame.

#define AVIIF_NOTIME	    0x00000100L // this frame doesn't take any time
#define AVIIF_COMPUSE       0x0FFF0000L // these bits are for compressor use

typedef struct
{
    DWORD		ckid;
    DWORD		dwFlags;
    DWORD		dwChunkOffset;		// Position of chunk
    DWORD		dwChunkLength;		// Length of chunk
} AVIINDEXENTRY;

// end of AVI structures

// Linked-list of indicies
#define AVIINDEX_NUMENTRIES 1024
#define AVIINDEX_SIZE sizeof(AVIINDEXENTRY)*AVIINDEX_NUMENTRIES

typedef struct _AVIIndex {
	AVIINDEXENTRY* index;
	struct _AVIIndex* next;
} AVIIndex;

AVIIndex* aviIndex = NULL;
AVIIndex* currIndex = NULL;
DWORD aviIndexChunkSize = 0;
DWORD currChunkOffset = 0;
DWORD indexedFrames = 0;

HINSTANCE hInst, hProc;
HANDLE videoFile;
// window handle of the process
HWND hProcWnd = NULL;

extern HMODULE hDI;
extern HMODULE hOpenGL;

// information about the process
MYINFO g_myinfo;
// information about my state
MYSTATE g_mystate = 
{
	TRUE,  // BOOL bIndicate;
	FALSE, // BOOL bKeyboardInitDone;
	FALSE, // BOOL bMakeScreenShot;
	FALSE, // BOOL bMakeSmallScreenShot;
	FALSE, // BOOL bStartRecording;
	FALSE, // BOOL bNowRecording;
	TRUE,  // BOOL bSystemWideMode;
	FALSE, // BOOL bStopRecording;
	TRUE,  // BOOL bUnhookGetMsgProc;
};

char buf[BUFLEN];

// keyboard hook handle
HHOOK g_hKeyboardHook = NULL;

// back-buffer dimensions in pixels
UINT g_bbWidth = 0;
UINT g_bbHeight = 0;

void CheckAndHookModules();

//////////////////////////////////////////////////////////
// FUNCTIONS
//////////////////////////////////////////////////////////

DWORD QPC()
{
	LARGE_INTEGER count;
	QueryPerformanceCounter(&count);
	return count.LowPart;
}

/* install dummy keyboard hooks to all threads of this process. */
void InstallDummyKeyboardHook(DWORD tid)
{
	g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD, DummyKeyboardProc, hInst, tid);
	LogWithNumber("Installed dummy keyboard hook: %08x", (DWORD)g_hKeyboardHook);
}

/* install keyboard hooks to all threads of this process. */
void InstallKeyboardHook(DWORD tid)
{
	g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, hInst, tid);
	LogWithNumber("Installed keyboard hook: %08x", (DWORD)g_hKeyboardHook);
}

/* remove keyboard hooks */
void UninstallKeyboardHook(void)
{
	if (g_hKeyboardHook != NULL)
	{
		UnhookWindowsHookEx( g_hKeyboardHook );
		LogWithNumber("Keyboard hook %08x uninstalled.", (DWORD)g_hKeyboardHook);
		g_hKeyboardHook = NULL;
	}
}

/* Loads a BMP file */
/* params: [out] address of the pointer to RGB data         */
/*         [out] address of the pointer to BITMAPINFOHEADER */
HRESULT LoadFromBMP(char* filename, HANDLE heap, BYTE** rgbBuf, BITMAPINFOHEADER** biInfoHeader)
{
	DWORD wbytes;
	BITMAPFILEHEADER fhdr;
	BITMAPINFOHEADER* infoheader;

	HANDLE hFile = CreateFile(filename,            // file to open 
					 GENERIC_READ,                 // open for writing 
					 0,                            // do not share 
					 NULL,                         // default security 
					 OPEN_EXISTING,                // open existing 
					 FILE_ATTRIBUTE_NORMAL,        // normal file 
					 NULL);                        // no attr. template 

	if (hFile == NULL) 
	{
		Log("LoadFromBMP: unable to open file for reading.");
		return E_FAIL;
	}
	DWORD fileSize = GetFileSize(hFile, NULL);

	// read the BITMAPFILEHEADER
	ZeroMemory(&fhdr, sizeof(fhdr));
	ReadFile(hFile, &fhdr, sizeof(fhdr), (LPDWORD)&wbytes, NULL);
	if (fhdr.bfType != 0x4d42 || // "BM"
	    fhdr.bfSize != fileSize ||
		fhdr.bfReserved1 != 0 ||
		fhdr.bfReserved2 != 0)
	{
		Log("LoadFromBMP: wrong file format.");
		return E_FAIL;
	}

	// read the BITMAPINFOHEADER
	infoheader = (BITMAPINFOHEADER*)HeapAlloc(heap, HEAP_ZERO_MEMORY, sizeof(BITMAPINFOHEADER));
	if (infoheader == NULL)
	{
		Log("LoadFromBMP: failed to allocate memory for bitmap infoheader");
		return E_FAIL;
	}
	ZeroMemory(infoheader, sizeof(infoheader));
	ReadFile(hFile, infoheader, sizeof(BITMAPINFOHEADER), (LPDWORD)&wbytes, NULL);
	// check bit-count
	if (infoheader->biBitCount < 24)
	{
		Log("LoadFromBMP: only bitmaps of 24 bpp are supported.");
		return E_FAIL;
	}

	// read the RGB data
	INT bytes = infoheader->biWidth * infoheader->biBitCount / 8;
	INT rem = bytes % sizeof(ULONG);
	INT pitch = rem == 0 ? bytes : bytes + sizeof(ULONG) - rem;
	SIZE_T size = infoheader->biWidth * infoheader->biHeight * infoheader->biBitCount / 8; 

	BYTE* rgbData = (BYTE*)HeapAlloc(heap, HEAP_ZERO_MEMORY, size);
	if (rgbData == NULL)
	{
		Log("LoadFromBMP: failed to allocate memory for RGB data");
		return E_FAIL;
	}
	ZeroMemory(infoheader, sizeof(infoheader));
	SetFilePointer(hFile, fhdr.bfOffBits, NULL, FILE_BEGIN);
	ReadFile(hFile, rgbData, size, (LPDWORD)&wbytes, NULL);
	if (wbytes != size)
	{
		Log("LoadFromBMP: failed to read the whole RGB data");
		return E_FAIL;
	}
	CloseHandle(hFile);

	*rgbBuf = rgbData;
	*biInfoHeader = infoheader;
	
	return S_OK;
}

/* Writes a BMP file */
HRESULT SaveAsBMP(char* filename, BYTE* rgbBuf, SIZE_T size, LONG width, LONG height, int bpp)
{
	BITMAPFILEHEADER fhdr;
	BITMAPINFOHEADER infoheader;

	// fill in the headers
	fhdr.bfType = 0x4D42; // "BM"
	fhdr.bfSize = sizeof(fhdr) + sizeof(infoheader) + size;
	fhdr.bfReserved1 = 0;
	fhdr.bfReserved2 = 0;
	fhdr.bfOffBits = sizeof(fhdr) + sizeof(infoheader);

	infoheader.biSize = sizeof(infoheader);
	infoheader.biWidth = width;
	infoheader.biHeight = height;
	infoheader.biPlanes = 1;
	infoheader.biBitCount = bpp*8;
	infoheader.biCompression = BI_RGB;
	infoheader.biSizeImage = 0;
	infoheader.biXPelsPerMeter = 0;
	infoheader.biYPelsPerMeter = 0;
	infoheader.biClrUsed = 0;
	infoheader.biClrImportant = 0;

	// prepare filename
	char name[BUFLEN];
	if (filename == NULL)
	{
		SYSTEMTIME time;
		GetLocalTime(&time);
		ZeroMemory(name, BUFLEN);
		sprintf(name, "%s%s-%d%02d%02d-%02d%02d%02d%03d.bmp", g_config.captureDir, g_myinfo.shortProcessfileNoExt, 
				time.wYear, time.wMonth, time.wDay,
				time.wHour, time.wMinute, time.wSecond, time.wMilliseconds); 
		filename = name;
	}

	// save to file
	DWORD wbytes;
	HANDLE hFile = CreateFile(filename,            // file to create 
					 GENERIC_WRITE,                // open for writing 
					 0,                            // do not share 
					 NULL,                         // default security 
					 OPEN_ALWAYS,                  // overwrite existing 
					 FILE_ATTRIBUTE_NORMAL,        // normal file 
					 NULL);                        // no attr. template 

	if (hFile != INVALID_HANDLE_VALUE) 
	{
		WriteFile(hFile, &fhdr, sizeof(fhdr), (LPDWORD)&wbytes, NULL);
		WriteFile(hFile, &infoheader, sizeof(infoheader), (LPDWORD)&wbytes, NULL);
		WriteFile(hFile, rgbBuf, size, (LPDWORD)&wbytes, NULL);
		CloseHandle(hFile);
	}
	else 
	{
		Log("SaveAsBMP: failed to save to file.");
		return E_FAIL;
	}
	return S_OK;
}

/* Opens a new file, and writes the AVI header */
HANDLE OpenAVIFile()
{
	// prepare filename
	SYSTEMTIME time;
	GetLocalTime(&time);
	char filename[BUFLEN];
	ZeroMemory(filename, BUFLEN);
	sprintf(filename, "%s%s-%d%02d%02d-%02d%02d%02d%03d.avi", g_config.captureDir, g_myinfo.shortProcessfileNoExt, 
			time.wYear, time.wMonth, time.wDay,
			time.wHour, time.wMinute, time.wSecond, time.wMilliseconds); 
	Log("OpenAVIFile: filename:");
	Log(filename);
	
	HANDLE hFile = CreateFile(filename, // file to create 
		 GENERIC_READ | GENERIC_WRITE,  // open for reading and writing 
		 0,                             // do not share 
		 NULL,                          // default security 
		 CREATE_ALWAYS,                 // overwrite existing 
		 FILE_ATTRIBUTE_NORMAL,         // normal file 
		 NULL);                         // no attr. template 

	if (hFile == NULL) return NULL;

	AVIFILEHEADER_VO afh;
	ZeroMemory(&afh, sizeof(AVIFILEHEADER_VO));

	afh.fccRiff = 0x46464952; // "RIFF"
	afh.fccForm = 0x20495641; // "AVI "
	afh.list_0  = 0x5453494c; // "LIST"
	afh.fccHdrl = 0x6c726468; // "hdrl"
	afh.fccAvih = 0x68697661; // "avih"
	afh.dwAvihChunkSize = sizeof(MainAVIHeader);
	afh.list_1  = 0x5453494c; // "LIST"
	afh.fccStrl = 0x6c727473; // "strl"
	afh.fccStrh = 0x68727473; // "strh"
	afh.dwStrhChunkSize = sizeof(AVIStreamHeader);
	afh.fccStrf = 0x66727473; // "strf"
	afh.dwStrfChunkSize = sizeof(BITMAPINFOHEADER);

	// add "JUNK" chunk to get the 2K-alignment
	HANDLE heap = GetProcessHeap();
	int junkChunkSize = MOVILIST_OFFSET - sizeof(afh);
	BYTE* junkChunk = (BYTE*)HeapAlloc(heap, HEAP_ZERO_MEMORY, junkChunkSize);
	memcpy(junkChunk, "JUNK", 4);
	DWORD* sizePtr = (DWORD*)(junkChunk + 4);
	*sizePtr = junkChunkSize - 8;

	afh.list_1_size = 
		sizeof(FOURCC) + sizeof(FOURCC) + sizeof(DWORD) + sizeof(AVIStreamHeader) +
		sizeof(FOURCC) + sizeof(DWORD) + sizeof(BITMAPINFOHEADER) + junkChunkSize;
	afh.list_0_size = 
		sizeof(FOURCC) + sizeof(FOURCC) + sizeof(DWORD) + sizeof(MainAVIHeader) +
		sizeof(FOURCC) + sizeof(DWORD) + afh.list_1_size;

	DWORD wbytes = 0;
	WriteFile(hFile, (LPVOID)&afh, sizeof(afh), (LPDWORD)&wbytes, NULL);
	WriteFile(hFile, (LPVOID)junkChunk, junkChunkSize, (LPDWORD)&wbytes, NULL);

	char movi[] = "LIST\0\0\0\0movi";
	WriteFile(hFile, (LPVOID)movi, 12, (LPDWORD)&wbytes, NULL);

	// reset frames counter and chunk size
	totalFrames = 0;
	indexedFrames = 0;
	currChunkOffset = MOVILIST_OFFSET + 12;
	moviChunkSize = 4;

	// create index
	aviIndex = (AVIIndex*)HeapAlloc(heap, HEAP_ZERO_MEMORY, sizeof(AVIIndex));
	aviIndex->index = (AVIINDEXENTRY*)HeapAlloc(heap, HEAP_ZERO_MEMORY, AVIINDEX_SIZE);
	aviIndex->next = NULL;
	aviIndexChunkSize = AVIINDEX_SIZE;

	// initialize index pointer
	currIndex = aviIndex;

	return hFile;
}

void CloseAVIFile(HANDLE hFile)
{
	// flush the buffers
	FlushFileBuffers(hFile);

	// set frame rate for the newly recorded movie
	double frameRate = g_config.targetFrameRate;
	if (g_algorithm == CUSTOM) frameRate = g_customFrameRate;

	LogWithNumber("Movie frame rate is set to %u", (DWORD)frameRate);

	AVIFILEHEADER_VO afh;
	DWORD wbytes = 0;

	// read the avi-header
	SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
	ReadFile(hFile, (LPVOID)&afh, sizeof(afh), (LPDWORD)&wbytes, NULL);

	// update RIFF chunk size
	afh.dwRiffChunkSize = MOVILIST_OFFSET + moviChunkSize + 8 + aviIndexChunkSize;

	int BPP = 3; // always write 24-bit AVIs.

	// calculate correct dimensions
	int divider = (g_config.fullSizeVideo) ? 1 : 2;

	// fill-in MainAVIHeader
	afh.aviHeader.dwMicroSecPerFrame = 1000000/frameRate;
	afh.aviHeader.dwMaxBytesPerSec = BPP * (g_bbWidth/divider * g_bbHeight/divider) * frameRate;
	afh.aviHeader.dwTotalFrames = totalFrames;
	afh.aviHeader.dwStreams = 1;
	afh.aviHeader.dwFlags = 0x10; // uses index
	afh.aviHeader.dwSuggestedBufferSize = BPP * (g_bbWidth/divider * g_bbHeight/divider);
	afh.aviHeader.dwWidth = g_bbWidth/2;
	afh.aviHeader.dwHeight = g_bbHeight/2;
	
	// fill-in AVIStreamHeader
	afh.streamHeader.fccType = 0x73646976; // "vids"
	afh.streamHeader.fccHandler = 0x20424944; // "DIB "
	afh.streamHeader.dwScale = 1;
	afh.streamHeader.dwRate = frameRate;
	afh.streamHeader.dwLength = totalFrames;
	afh.streamHeader.dwQuality = 0;
	afh.streamHeader.dwSuggestedBufferSize = afh.aviHeader.dwSuggestedBufferSize;
	afh.streamHeader.rcFrame.right = g_bbWidth/divider;
	afh.streamHeader.rcFrame.bottom = g_bbHeight/divider;

	// fill in bitmap header
	afh.biHeader.biSize = sizeof(BITMAPINFOHEADER);
	afh.biHeader.biWidth = g_bbWidth/divider;
	afh.biHeader.biHeight = g_bbHeight/divider;
	afh.biHeader.biPlanes = 1;
	afh.biHeader.biBitCount = BPP*8;
	afh.biHeader.biCompression = BI_RGB;
	afh.biHeader.biSizeImage = BPP * (g_bbWidth/divider *g_bbHeight/divider);
	
	// save update header
	SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
	WriteFile(hFile, (LPVOID)&afh, sizeof(afh), (LPDWORD)&wbytes, NULL);

	// update movi chunk size
	SetFilePointer(hFile, MOVILIST_OFFSET + 4, NULL, FILE_BEGIN);
	WriteFile(hFile, (LPVOID)&moviChunkSize, sizeof(DWORD), (LPDWORD)&wbytes, NULL);

	// write index
	SetFilePointer(hFile, 0, NULL, FILE_END); 
	WriteFile(hFile, (LPVOID)"idx1", 4, (LPDWORD)&wbytes, NULL);
	WriteFile(hFile, (LPVOID)&aviIndexChunkSize, 4, (LPDWORD)&wbytes, NULL);

	// release memory that index took
	HANDLE heap = GetProcessHeap();
	AVIIndex *p = aviIndex, *q; 
	while (p != NULL)
	{
		WriteFile(hFile, (LPVOID)p->index, AVIINDEX_SIZE, (LPDWORD)&wbytes, NULL);
		q = p->next;
		HeapFree(heap, 0, p->index); // release index
		HeapFree(heap, 0, p); // release index
		p = q;
	}

	// close file
	CloseHandle(hFile);
}

DWORD WriteAVIFrame(HANDLE hFile, BYTE *rgbBuf, SIZE_T size, DWORD times)
{
	TRACE2("WriteAVIFrame: called. writing frame %d time(s)", times);
	DWORD wbytes = 0;

	for (int i=0; i<times; i++)
	{
		// write frame
		char chunkHeader[] = "00db\0\0\0\0";
		*(SIZE_T*)(chunkHeader+4) = size;
		WriteFile(hFile, (LPVOID)chunkHeader, 8, (LPDWORD)&wbytes, NULL);
		WriteFile(hFile, (LPVOID)rgbBuf, size, (LPDWORD)&wbytes, NULL);
		
		// update index
		if (indexedFrames == AVIINDEX_NUMENTRIES)
		{
			// allocate new index, if this one is full.
			HANDLE heap = GetProcessHeap();
			AVIIndex* q = (AVIIndex*)HeapAlloc(heap, HEAP_ZERO_MEMORY, sizeof(AVIIndex));
			q->index = (AVIINDEXENTRY*)HeapAlloc(heap, HEAP_ZERO_MEMORY, AVIINDEX_SIZE);
			q->next = NULL;
			currIndex->next = q;
			currIndex = q;

			aviIndexChunkSize += AVIINDEX_SIZE;
			indexedFrames = 0;
		}
		currIndex->index[indexedFrames].ckid = 0x62643030; // "00db"
		currIndex->index[indexedFrames].dwFlags = AVIIF_KEYFRAME;
		currIndex->index[indexedFrames].dwChunkOffset = currChunkOffset;
		currIndex->index[indexedFrames].dwChunkLength = size;

		currChunkOffset += 8 + size;
		moviChunkSize += 8 + size;
		indexedFrames++;
		totalFrames++;
	}

	return wbytes;
}

// determine whether this frame needs to be grabbed when recording.
DWORD GetFrameDups()
{
	DWORD frameDups = 0;
	DWORD count = QPC();
	switch (g_algorithm)
	{
		case ADAPTIVE:
			if (g_lastCount > 0)
			{
				TRACE2("g_lastCount = %u", g_lastCount);
				TRACE2("count = %u", count);
				TRACE2("g_lastOverhead = %u", g_lastOverhead);
				TRACE2("real ticks = %d", count - g_lastCount);
				DWORD ticks = count - g_lastCount;

				if (g_lastOverhead > 0) 
				{
					g_penalty = g_lastOverhead / 2;
					ticks -= (g_lastOverhead - g_penalty);
					g_lastOverhead = 0;
				}
				else
				{
					g_penalty = g_penalty / 2;
					ticks += g_penalty;
				}
				TRACE2("adjusted ticks = %d", ticks);

				float currFrameRate = (float)g_freq / ticks;
				TRACE3("currentFrameRate = %f", currFrameRate);

				if (currFrameRate > 0)
				{
					float frameWeight = g_config.targetFrameRate / currFrameRate + g_extraWeight;
					frameDups = (DWORD)(frameWeight);
					g_extraWeight = frameWeight - frameDups;
					TRACE2("frameDups = %d", frameDups);
					TRACE3("g_extraWeight = %f", g_extraWeight);
				}
			}
			g_lastCount = count;
			break;

		case CUSTOM:
			float frameWeight = g_customFrameWeight + g_extraWeight;
			frameDups = (DWORD)(frameWeight);
			g_extraWeight = frameWeight - frameDups;
			TRACE2("frameDups = %d", frameDups);
			TRACE3("g_extraWeight = %f", g_extraWeight);
			break;
	}
	return frameDups;
}

/* This functions should be called when DLL is mapped into an application
 * to check if this is one of the special apps, that we shouldn't do any
 * graphics API hooking.
 */
static BOOL CheckSpecialApps()
{
	// First, test if we are mapped into ourselves.
	if (lstrcmpi(g_myinfo.shortProcessfile, TAKSI_EXE)==0) return TRUE;

	// Check if it's Windows Explorer. We don't want to hook it either.
	char explorer[BUFLEN];
	ZeroMemory(explorer, BUFLEN);
	GetWindowsDirectory(explorer, BUFLEN);
	lstrcat(explorer, "\\Explorer.EXE");

	if (lstrcmpi(g_myinfo.processfile, explorer)==0) return TRUE;

	return FALSE;
}


/*******************/
/* DLL Entry Point */
/*******************/
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	DWORD wbytes, procId; 

	if (dwReason == DLL_PROCESS_ATTACH)
	{
		hInst = hInstance;

		// clear out MYINFO structures
		ZeroMemory(&g_myinfo, sizeof(MYINFO));

		// set reconfCounter
		g_reconfCounter = sg_reconfCounter;

		/* determine logfile full path */
		ZeroMemory(g_myinfo.logfile, MAX_FILEPATH);
		GetModuleFileName(hInst, g_myinfo.logfile, MAX_FILEPATH);
		char *zero = g_myinfo.logfile + lstrlen(g_myinfo.logfile);
		char *p = zero; while ((p != g_myinfo.logfile) && (*p != '\\')) p--;
		if (*p == '\\') p++; 
		g_myinfo.shortLogfile = p;

		ZeroMemory(g_myinfo.shortLogfile, zero - g_myinfo.shortLogfile); 
		lstrcat(p, LOG);

		/* determine process full path */
		ZeroMemory(g_myinfo.processfile, MAX_FILEPATH);
		GetModuleFileName(NULL, g_myinfo.processfile, MAX_FILEPATH);
		zero = g_myinfo.processfile + lstrlen(g_myinfo.processfile);
		p = zero; while ((p != g_myinfo.processfile) && (*p != '\\')) p--;
		if (*p == '\\') p++;
		g_myinfo.shortProcessfile = p;
		
		// save short filename without ".exe" extension.
		ZeroMemory(g_myinfo.shortProcessfileNoExt, BUFLEN);
		char* ext = g_myinfo.shortProcessfile + lstrlen(g_myinfo.shortProcessfile) - 4;
		if (lstrcmpi(ext, ".exe")==0) {
			memcpy(g_myinfo.shortProcessfileNoExt, g_myinfo.shortProcessfile, ext - g_myinfo.shortProcessfile); 
		}
		else {
			lstrcpy(g_myinfo.shortProcessfileNoExt, g_myinfo.shortProcessfile);
		}

		/* determine process handle */
		hProc = GetModuleHandle(NULL);

		/* log information on which process mapped the DLL */
		ZeroMemory(buf, MAX_FILEPATH);
		lstrcpy(buf, "mapped: ");
		lstrcat(buf, g_myinfo.processfile);

		HANDLE hFile = INVALID_HANDLE_VALUE;
		if (g_config.debug > 0)
			hFile = CreateFile(g_myinfo.logfile,// file to create 
						 GENERIC_WRITE,         // open for writing 
						 0,                     // do not share 
						 NULL,                  // default security 
						 OPEN_ALWAYS,           // overwrite existing 
						 FILE_ATTRIBUTE_NORMAL, // normal file 
						 NULL);                 // no attr. template 

		if (hFile != INVALID_HANDLE_VALUE) 
		{
			SetFilePointer(hFile, 0, NULL, FILE_END);
			WriteFile(hFile, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
			WriteFile(hFile, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
			CloseHandle(hFile);
		}

		// adjust g_mystate, based on the g_config
		g_mystate.bUnhookGetMsgProc = !g_config.startupModeSystemWide;

		// do not hook on selected applications.
		// (such as: myself, Explorer.EXE)
		g_ignoreAllMessages = CheckSpecialApps();

		// see if any of supported API DLLs are already loaded.
		if (!g_ignoreAllMessages) CheckAndHookModules();
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log("DLL detaching...");

		if (hOpenGL != NULL)
		{
			/* close AVI file, if we were in recording mode */
			if (g_mystate.bNowRecording)
			{
				g_mystate.bNowRecording = false;
				Log("Recording stopped. (DLL is being unmapped).");
				CloseAVIFile(videoFile);
			}

			/* uninstall keyboard hook */
			UninstallKeyboardHook();

			/* restore original pointers */
			RestoreOpenGLFunctions();

			/* uninstall system-wide hook */
			if (sg_bHookInstalled)
			{
				if (UninstallTheHook())
				{
					Log("GetMsgProc succesffully unhooked.");
				}
			}

			/* if taksi.exe is still running, tell it to re-install the GetMsgProc hook */
			if (!sg_bExiting)
			{
				PostMessage(sg_hServerWnd, WM_APP_REHOOK, (WPARAM)0, (LPARAM)0);
				Log("Posted message for server to re-hook GetMsgProc.");
			}

			// give graphics module a chance to clean up.
			GraphicsCleanupOGL();
		}

		// call FreeLibrary to decrease count
		if (hOpenGL && g_unloadOGL) FreeLibrary(hOpenGL);

		/* close specific log file */
		Log("Closing log.");
		CloseLog();

		/* log information on which process unmapped the DLL */
		ZeroMemory(buf, MAX_FILEPATH);
		lstrcpy(buf, "unmapped: ");
		lstrcat(buf, g_myinfo.processfile);

		HANDLE hFile = INVALID_HANDLE_VALUE;
		if (g_config.debug > 0)
			hFile = CreateFile(g_myinfo.logfile,       // file to create 
						 GENERIC_WRITE,                // open for writing 
						 0,                            // do not share 
						 NULL,                         // default security 
						 OPEN_ALWAYS,                  // overwrite existing 
						 FILE_ATTRIBUTE_NORMAL,        // normal file 
						 NULL);                        // no attr. template 

		if (hFile != INVALID_HANDLE_VALUE) 
		{
			SetFilePointer(hFile, 0, NULL, FILE_END);
			WriteFile(hFile, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
			WriteFile(hFile, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
			CloseHandle(hFile);
		}
	}
	return TRUE;    /* ok */
}

/**************************************************************** 
 * WH_KEYBOARD hook procedure                                   *
 ****************************************************************/ 

EXTERN_C _declspec(dllexport) LRESULT CALLBACK DummyKeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
    // do not process message 
	return CallNextHookEx(g_hKeyboardHook, code, wParam, lParam); 
}

/**************************************************************** 
 * WH_KEYBOARD hook procedure                                   *
 ****************************************************************/ 

EXTERN_C _declspec(dllexport) LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
	//TRACE2("KeyboardProc CALLED. code = %d", code);
	
    if (code < 0) // do not process message 
        return CallNextHookEx(g_hKeyboardHook, code, wParam, lParam); 

	switch (code)
	{
		case HC_ACTION:
			/* process the key events */
			if (lParam & 0x80000000)
			{
				// a key was pressed.
				if (wParam == g_config.vKeyScreenShot)
				{
					Log("KeyboardProc: VKEY_SCREENSHOT pressed.");
					g_mystate.bMakeScreenShot = true;
				}

				else if (wParam == g_config.vKeySmallScreenShot)
				{
					Log("KeyboardProc: VKEY_SMALL_SCREENSHOT pressed.");
					g_mystate.bMakeSmallScreenShot = true;
				}

				else if (wParam == g_config.vKeyHookMode)
				{
					Log("KeyboardProc: VKEY_HOOK_MODE_TOGGLE pressed.");
					if (g_mystate.bNowRecording) break; // don't switch during video capture.
					if (sg_bHookInstalled)
					{
						Log("KeyboardProc: Signaling to unhook GetMsgProc.");
						if (UninstallTheHook())
						{
							Log("KeyboardProc: GetMsgProc unhooked.");
							// change indicator to "green"
						}
					}
					else
					{
						Log("KeyboardProc: Signaling to install GetMsgProc hook.");
						if (InstallTheHook())
						{
							Log("KeyboardProc: GetMsgProc successfully hooked.");
							// change indicator to "blue"
						}
					}
				}

				else if (wParam == g_config.vKeyIndicator)
				{
					Log("KeyboardProc: VKEY_INDICATOR_TOGGLE pressed.");
					g_mystate.bIndicate = !g_mystate.bIndicate;
				}

				else if (wParam == g_config.vKeyVideoCapture)
				{
					Log("KeyboardProc: VKEY_VIDEO_CAPTURE_TOGGLE pressed.");
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
			break;
	}

	// We must pass the all messages on to CallNextHookEx.
	return CallNextHookEx(g_hKeyboardHook, code, wParam, lParam);
}

/* Checks whether an application uses any of supported APIs (D3D8, D3D9, OpenGL).
 * If so, their corresponding buffer-swapping routines are hooked. */
void CheckAndHookModules()
{
	// OpenGL
	if (!g_OGL_hooked) 
	{
		Log("Checking OpenGL usage");
		hOpenGL = GetModuleHandle("opengl32.dll");
		if (!hOpenGL) 
		{
			hOpenGL = LoadLibrary("opengl32");
			g_unloadOGL = TRUE;
		}
		LogWithNumber("hOpenGL = %08x", (DWORD)hOpenGL);
		if (hOpenGL) { InitHooksOGL(); g_OGL_hooked = TRUE; }
	}
}

void VerifyImplants()
{
	VerifyOGL();
}

/**************************************************************** 
 * WH_CBT hook procedure                                        *
 ****************************************************************/ 

EXTERN_C _declspec(dllexport) LRESULT CALLBACK CBTProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code < 0 || g_ignoreAllMessages) // do not process message 
        return CallNextHookEx(sg_hGetMsgHook, code, wParam, lParam); 

	switch (code)
	{
		case HCBT_SETFOCUS:
			MASTERTRACE2(g_myinfo.logfile, g_myinfo.processfile, " code = %08x", (DWORD)code);
			TRACE2("CBTProc: code = %08x", (DWORD)code);
			VerifyImplants();
			break;
	}

	// We must pass the all messages on to CallNextHookEx.
	return CallNextHookEx(sg_hGetMsgHook, code, wParam, lParam);
}

/************************
 * Installer            *
 ************************/
EXTERN_C _declspec(dllexport) BOOL InstallTheHook(void)
{
	if (sg_bHookInstalled) {
		Log("InstallTheHook: already installed.");
		return FALSE;
	}

	sg_hGetMsgHook = SetWindowsHookEx(WH_CBT, CBTProc, hInst, 0);
	sg_bHookInstalled = (sg_hGetMsgHook != NULL);
	g_mystate.bSystemWideMode = sg_bHookInstalled;
	return sg_bHookInstalled;
}

/************************
 * Uninstaller          *
 ************************/
EXTERN_C _declspec(dllexport) BOOL UninstallTheHook(void)
{
	if (!sg_bHookInstalled) {
		Log("UninstallTheHook: already uninstalled.");
		return FALSE;
	}
	sg_bHookInstalled = !UnhookWindowsHookEx( sg_hGetMsgHook );
	g_mystate.bSystemWideMode = sg_bHookInstalled;
	return !sg_bHookInstalled;
}

/* Signal to unhook from IDirect3DDeviceN methods */
EXTERN_C _declspec(dllexport) void SetUnhookFlag(BOOL flag)
{
	//LogWithNumber("Setting sg_bUnhookDevice flag to %d", flag);
	sg_bUnhookDevice = flag;
}

EXTERN_C _declspec(dllexport) BOOL GetUnhookFlag(void)
{
	return sg_bUnhookDevice;
}

/* Set debug flag */
EXTERN_C _declspec(dllexport) void SetDebug(DWORD flag)
{
	//LogWithNumber("Setting g_config.debug flag to %d", flag);
	g_config.debug = flag;
}

EXTERN_C _declspec(dllexport) DWORD GetDebug(void)
{
	return g_config.debug;
}

EXTERN_C _declspec(dllexport) void SetTargetFrameRate(DWORD value)
{
	//LogWithNumber("Setting g_config.targetFrameRate to %d", value);
	g_config.targetFrameRate = value;
}

EXTERN_C _declspec(dllexport) DWORD GetTargetFrameRate(void)
{
	return g_config.targetFrameRate;
}

EXTERN_C _declspec(dllexport) void SetPresent8(DWORD value)
{
	//LogWithNumber("Setting g_config.present8 to %d", value);
	g_dxInfo.present8 = value;
}

EXTERN_C _declspec(dllexport) DWORD GetPresent8(void)
{
	return g_dxInfo.present8;
}

EXTERN_C _declspec(dllexport) void SetReset8(DWORD value)
{
	//LogWithNumber("Setting g_config.reset8 to %d", value);
	g_dxInfo.reset8 = value;
}

EXTERN_C _declspec(dllexport) DWORD GetReset8(void)
{
	return g_dxInfo.reset8;
}

EXTERN_C _declspec(dllexport) void SetPresent9(DWORD value)
{
	//LogWithNumber("Setting g_config.present9 to %d", value);
	g_dxInfo.present9 = value;
}

EXTERN_C _declspec(dllexport) DWORD GetPresent9(void)
{
	return g_dxInfo.present9;
}

EXTERN_C _declspec(dllexport) void SetReset9(DWORD value)
{
	//LogWithNumber("Setting g_config.reset9 to %d", value);
	g_dxInfo.reset9 = value;
}

EXTERN_C _declspec(dllexport) DWORD GetReset9(void)
{
	return g_dxInfo.reset9;
}

/* Signal to unhook from IDirect3DDeviceN methods */
EXTERN_C _declspec(dllexport) void SetKey(int whichKey, int code)
{
	switch (whichKey)
	{
		case VKEY_INDICATOR_TOGGLE: g_config.vKeyIndicator = code; break;
		case VKEY_HOOK_MODE_TOGGLE: g_config.vKeyHookMode = code; break;
		case VKEY_SMALL_SCREENSHOT: g_config.vKeySmallScreenShot = code; break;
		case VKEY_SCREENSHOT: g_config.vKeyScreenShot = code; break;
		case VKEY_VIDEO_CAPTURE: g_config.vKeyVideoCapture = code; break;
	}
}

EXTERN_C _declspec(dllexport) int GetKey(int whichKey)
{
	switch (whichKey)
	{
		case VKEY_INDICATOR_TOGGLE: return g_config.vKeyIndicator;
		case VKEY_HOOK_MODE_TOGGLE: return g_config.vKeyHookMode;
		case VKEY_SMALL_SCREENSHOT: return g_config.vKeySmallScreenShot;
		case VKEY_SCREENSHOT: return g_config.vKeyScreenShot;
		case VKEY_VIDEO_CAPTURE: return g_config.vKeyVideoCapture;
	}
	return -1;
}

EXTERN_C _declspec(dllexport) void SetUseDirectInput(bool useDirectInput)
{
	g_config.useDirectInput = useDirectInput;
}

EXTERN_C _declspec(dllexport) bool GetUseDirectInput(void)
{
	return g_config.useDirectInput;
}

EXTERN_C _declspec(dllexport) void SetStartupModeSystemWide(bool startupModeSystemWide)
{
	g_config.startupModeSystemWide = startupModeSystemWide;
}

EXTERN_C _declspec(dllexport) bool GetStartupModeSystemWide(void)
{
	return g_config.startupModeSystemWide;
}

EXTERN_C _declspec(dllexport) void SetFullSizeVideo(bool fullSizeVideo)
{
	g_config.fullSizeVideo = fullSizeVideo;
}

EXTERN_C _declspec(dllexport) bool GetFullSizeVideo(void)
{
	return g_config.fullSizeVideo;
}

EXTERN_C _declspec(dllexport) void SetExiting(BOOL flag)
{
	sg_bExiting = flag;
}

EXTERN_C _declspec(dllexport) BOOL GetExiting(void)
{
	return sg_bExiting;
}

EXTERN_C _declspec(dllexport) void SetServerWnd(HWND hwnd)
{
	sg_hServerWnd = hwnd;
}

EXTERN_C _declspec(dllexport) HWND GetServerWnd(void)
{
	return sg_hServerWnd;
}

EXTERN_C _declspec(dllexport) void SetCaptureDir(char* path)
{
	ZeroMemory(g_config.captureDir, BUFLEN);
	strncpy(g_config.captureDir, path, BUFLEN-1);
}

EXTERN_C _declspec(dllexport) void GetCaptureDir(char* path)
{
	if (path == NULL || g_config.captureDir == NULL) return;
	strncpy(path, g_config.captureDir, lstrlen(g_config.captureDir));
}

/**
 * Specify whether to ignore the recording overhead, when
 * calculating movie frame rate
 */
EXTERN_C _declspec(dllexport) void SetIgnoreOverhead(BOOL flag)
{
	//LogWithNumber("Setting sg_bIgnoreOverhead flag to %d", flag);
	sg_bIgnoreOverhead = flag;
}

EXTERN_C _declspec(dllexport) BOOL GetIgnoreOverhead(void)
{
	return sg_bIgnoreOverhead;
}

EXTERN_C _declspec(dllexport) void SetReconfCounter(DWORD value)
{
	//LogWithNumber("Setting sg_reconfCounter to %d", value);
	sg_reconfCounter = value;
}

EXTERN_C _declspec(dllexport) DWORD GetReconfCounter(void)
{
	return sg_reconfCounter;
}


