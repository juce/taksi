#include <windows.h>
#include <stdio.h>
#include "d3d8.h"

typedef IDirect3D8* (STDMETHODCALLTYPE *DIRECT3DCREATE8)(UINT);

#define LOG(f,x) if (f != NULL) fprintf x
extern FILE* log;

#define RESET8_OFFSET 14
#define PRESENT8_OFFSET 15

BOOL GetDevice8Methods(HWND hWnd, DWORD* present8, DWORD* reset8)
{
	// step 1: Load d3d8.dll
	HMODULE hD3D8 = LoadLibrary("d3d8");
	if (hD3D8 == NULL) 
	{
		LOG(log, (log, "Failed to load d3d8.dll\n"));
		return false;
	}

	// step 2: Get IDirect3D8
	DIRECT3DCREATE8 Direct3DCreate8 = (DIRECT3DCREATE8)GetProcAddress(hD3D8, "Direct3DCreate8");
	if (Direct3DCreate8 == NULL) 
	{
		LOG(log, (log, "Unable to find Direct3DCreate8\n"));
		return false;
	}
	IDirect3D8* d3d = Direct3DCreate8(D3D_SDK_VERSION);
	if (d3d == NULL)
	{
		LOG(log, (log, "Failed to get IDirect3D8 %d %x\n", 10, 20));
		return false;
	}

	// step 3: Get IDirect3DDevice8
    D3DDISPLAYMODE d3ddm;
    if (FAILED(d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm )))
	{
		LOG(log, (log, "GetAdapterDisplayMode failed.\n"));
        return false;
	}
    D3DPRESENT_PARAMETERS d3dpp; 
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = d3ddm.Format;

	IDirect3DDevice8* d3dDevice = NULL;
    if (FAILED(d3d->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
								  D3DCREATE_SOFTWARE_VERTEXPROCESSING,
								  &d3dpp, &d3dDevice)))
    {
		LOG(log, (log, "CreateDevice failed.\n"));
        return false;
    }
	LOG(log, (log, "Got IDirect3DDevice8: %08x\n", (DWORD)d3dDevice));

	// step 4: store method addresses in out vars
	DWORD* vtablePtr = (DWORD*)(*((DWORD*)d3dDevice));
	*present8 = vtablePtr[PRESENT8_OFFSET] - (DWORD)hD3D8;
	*reset8 = vtablePtr[RESET8_OFFSET] - (DWORD)hD3D8;
	
	// step 5: release device
	d3dDevice->Release();

	// step 6: release d3d
	d3d->Release();
	
	// step 7: Unload d3d8.dll
	FreeLibrary(hD3D8);

	return true;
}

