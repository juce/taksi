#include <windows.h>
#include <stdio.h>
#include "d3d9.h"

typedef IDirect3D9* (STDMETHODCALLTYPE *DIRECT3DCREATE9)(UINT);

#define LOG(f,x) if (f != NULL) fprintf x
extern FILE* log;

#define RESET9_OFFSET 16
#define PRESENT9_OFFSET 17

BOOL GetDevice9Methods(HWND hWnd, DWORD* present9, DWORD* reset9)
{
	// step 1: Load d3d9.dll
	HMODULE hD3D9 = LoadLibrary("d3d9");
	if (hD3D9 == NULL) 
	{
		LOG(log, (log, "Failed to load d3d9.dll\n"));
		return false;
	}

	// step 2: Get IDirect3D9
	DIRECT3DCREATE9 Direct3DCreate9 = (DIRECT3DCREATE9)GetProcAddress(hD3D9, "Direct3DCreate9");
	if (Direct3DCreate9 == NULL) 
	{
		LOG(log, (log, "Unable to find Direct3DCreate9\n"));
		return false;
	}
	IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
	if (d3d == NULL)
	{
		LOG(log, (log, "Failed to get IDirect3D9 %d %x\n", 10, 20));
		return false;
	}

	// step 3: Get IDirect3DDevice9
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

	IDirect3DDevice9* d3dDevice = NULL;
    if (FAILED(d3d->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
								  D3DCREATE_SOFTWARE_VERTEXPROCESSING,
								  &d3dpp, &d3dDevice)))
    {
		LOG(log, (log, "CreateDevice failed.\n"));
        return false;
    }
	LOG(log, (log, "Got IDirect3DDevice9: %08x\n", (DWORD)d3dDevice));

	// step 4: store method addresses in out vars
	DWORD* vtablePtr = (DWORD*)(*((DWORD*)d3dDevice));
	*present9 = vtablePtr[PRESENT9_OFFSET] - (DWORD)hD3D9;
	*reset9 = vtablePtr[RESET9_OFFSET] - (DWORD)hD3D9;
	
	// step 5: release device
	d3dDevice->Release();

	// step 6: release d3d
	d3d->Release();
	
	// step 7: Unload d3d9.dll
	FreeLibrary(hD3D9);

	return true;
}

