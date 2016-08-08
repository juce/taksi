#ifndef _JUCE_GRAPHX_DX8_
#define _JUCE_GRAPHX_DX8_

#include "d3d8types.h"
#include "d3d8.h"

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) if (p) { if (p->Release() == 0) p = NULL; }
#endif

#ifndef SAFE_RELEASE2
#define SAFE_RELEASE2(p, c) if (p) { c--; if (p->Release() == 0) p = NULL; }
#endif

#ifndef HEAP_SEARCH_INTERVAL
#define HEAP_SEARCH_INTERVAL 1000  // in msec
#define DLLMAP_PAUSE 500           // in msec
#endif

void InvalidateAndDelete8();
EXTERN_C _declspec(dllexport) void RestoreDeviceMethods8();
//VOID HookerFunc8(LPVOID lpParam); // hooker thread function
//BOOL GraphicsLoopHooked8();
void InitHooksD8();
void GraphicsCleanup8();
void VerifyD8();

#endif
