#ifndef _JUCE_GRAPHX_DX9_
#define _JUCE_GRAPHX_DX9_

#include "d3d9types.h"
#include "d3d9.h"

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

void InvalidateAndDelete9();
EXTERN_C _declspec(dllexport) void RestoreDeviceMethods9();
//VOID HookerFunc9(LPVOID lpParam); // hooker thread function
//BOOL GraphicsLoopHooked9();
void InitHooksD9();
void GraphicsCleanup9();
void VerifyD9();

#endif
