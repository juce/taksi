#include <windows.h>
#include <tlhelp32.h>
#include "log.h"
#include "heap.h"

/* Searches specified all process heaps for certain data */
DWORD* SearchHeaps(DWORD* data, DWORD dataSize)
{
	// Due to Win32 API inconsistencies, we need to use different API: 
	// depending on the version of the operating system, we're running on
	//     HeapWalk on WinNT/2000/XP, 
	//     Heap32First/Heap32Last on Win95/98/Me
	OSVERSIONINFO osInfo;
	osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osInfo);

	TRACE2("OS Major version: %d", osInfo.dwMajorVersion);
	TRACE2("OS Minor version: %d", osInfo.dwMinorVersion);
	TRACE2("OS Platform Id: %d", osInfo.dwPlatformId);

	TRACE2("Default process heap: %08x", (DWORD)GetProcessHeap());

	if (osInfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
	{
		// get handles for heaps (up to 1000)
		HANDLE heaps[1000];
		int numHeaps = GetProcessHeaps(1000, heaps);
		TRACE2("Process uses %d heaps.", numHeaps);

		for (int i=0; i<numHeaps; i++)
		{
			TRACE2("Searching heap #%d...", i);
			HANDLE heap = heaps[i];
			DWORD* p = SearchHeapNT(heap, data, dataSize);
			if (p != NULL) return p;
		}
		// didn't find the data
		return NULL;
	}
	else
	{
		// NOTE about Win95/98/Me:
		//
		// Ideally, a straight-forward and simple approach would be used
		// to search the application's heaps using Toolhelp32 API, since
		// HeapWalk function is not available on those platforms.
		// However, experiments show that Heap32ListFirst/Heap32ListNext
		// functions unfortunately do NOT enumerate application's default
		// heap, when run on WinXP using Win95/98/Me compatibility modes.
		// (I have not run this test on actual systems themselves - as
		// opposed to using compatibility mode on XP). So far, I have 
		// found no explanation of this effect, although this may change
		// in the future.
		//
		// Therefore, we implement the heap search as a 2-step process:
		// First, a flat search of the application's whole address space 
		// (0x1000 - 0x7ffffff) is performed, starting at the default
		// heap location, wrapping around, and searching up to 
		// application's HMODULE address. If that procedure fails, then
		// as a "double-check" action, we search the heaps, enumerated
		// by Heap32ListFirst/Heap32ListNext.

		// STEP 1: flat search of the application's address space.
		TRACE("Starting flat search of address space...");

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		TRACE2("Memory page size: %d bytes", sysInfo.dwPageSize);

		HANDLE heap = GetProcessHeap();
		DWORD lastAddr = 0x80000000 - dataSize;
		for (DWORD* p = (DWORD*)heap; p < (DWORD*)lastAddr; p++)
		{
			if ((DWORD)p == (DWORD)data) continue; // don't compare with itself
			try
			{
				if (memcmp(p, data, dataSize) == 0) return p;
				p++;
			}
			catch (...)
			{
				// apparently, we cannot read this page
				// skip to the next page then.
				TRACE2("Memory read failed at %08x. Skipping to next page", (DWORD)p);
				p = (DWORD*)(((DWORD)p % sysInfo.dwPageSize + 1) * sysInfo.dwPageSize);
			}
		}

		// wrap-around and search from the beginning
		HMODULE hMod = GetModuleHandle(NULL);
		for (p = (DWORD*)0x1000; p < (DWORD*)hMod; p++)
		{
			if ((DWORD)p == (DWORD)data) continue; // don't compare with itself
			try
			{
				if (memcmp(p, data, dataSize) == 0) return p;
				p++;

			}
			catch (...)
			{
				// apparently, we cannot read this page
				// skip to the next page then.
				TRACE2("Memory read failed at %08x. Skipping to next page", (DWORD)p);
				p = (DWORD*)(((DWORD)p % sysInfo.dwPageSize + 1) * sysInfo.dwPageSize);
			}
		}

		TRACE("Flat address space search failed.");

		// STEP 2: use Toolhelp API.
		DWORD processId = GetCurrentProcessId();
		HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPHEAPLIST, processId);

		int i=0;
		HEAPLIST32 heapInfo;
		ZeroMemory(&heapInfo, sizeof(HEAPLIST32));
		heapInfo.dwSize = sizeof(HEAPLIST32);

		if (Heap32ListFirst(snap, &heapInfo))
		{
			do
			{
				TRACE2("Searching heap #%d...", i);
				TRACE2("Heap flags:  %08x", heapInfo.dwFlags);
				if (heapInfo.dwFlags & HF32_DEFAULT) TRACE("(Default process heap)");
				DWORD* p = SearchHeap9x(&heapInfo, data, dataSize);
				if (p != NULL) return p;
				i++;
			}
			while (Heap32ListNext(snap, &heapInfo));
		}
		// didn't find the data
		return NULL;
	}
}

/* Searches specified heap for certain data: (NT/2000/XP) version */
DWORD* SearchHeapNT(HANDLE heap, DWORD* data, DWORD dataSize)
{
	PROCESS_HEAP_ENTRY mem;
	mem.lpData = NULL;

	TRACE("Starting search using HeapWalk...");
	HeapLock(heap);
	while (HeapWalk(heap, &mem))
	{
		if (mem.wFlags & PROCESS_HEAP_UNCOMMITTED_RANGE) continue;
		//TRACE2("heap element size: %d", mem.cbData);

		if (mem.cbData < dataSize) continue;
		int count = (mem.cbData - dataSize)/sizeof(DWORD) + 1;

		DWORD* p = (DWORD*)mem.lpData;
		for (int i=0; i<count; i++)
		{
			if (memcmp(p, data, dataSize) == 0) 
			{
				HeapUnlock(heap);
				return p;
			}
			p++;
		}
	}
	// data not found
	HeapUnlock(heap);
	return NULL;
}

/* Searches specified heap for certain data: (95/98/Me) version */
DWORD* SearchHeap9x(HEAPLIST32* heapInfo, DWORD* data, DWORD dataSize) 
{
	HEAPENTRY32 mem;
	ZeroMemory(&mem, sizeof(HEAPENTRY32));
	mem.dwSize = sizeof(HEAPENTRY32);

	TRACE("Starting search using Heap32First/Heap32Last...");
	if (Heap32First(&mem, heapInfo->th32ProcessID, heapInfo->th32HeapID))
	{
		do
		{
			if (mem.dwFlags & LF32_FREE) continue;
			//TRACE2("heap element size: %d", mem.dwBlockSize);
			//TRACE2("heap element addr: %08x", mem.dwAddress);

			//TRACE2("Heap handle: %08x", (DWORD)mem.hHandle);

			if (mem.dwBlockSize < dataSize) continue;
			int count = (mem.dwBlockSize - dataSize)/sizeof(DWORD) + 1;

			DWORD* p = (DWORD*)mem.dwAddress;
			for (int i=0; i<count; i++)
			{
				if (memcmp(p, data, dataSize) == 0) return p;
				p++;
			}
		}
		while (Heap32Next(&mem));
	}
	return NULL;
}

