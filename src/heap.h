// heap.h
#include <tlhelp32.h>

DWORD* SearchHeaps(DWORD* data, DWORD dataSize);

DWORD* SearchHeapNT(HANDLE heap, DWORD* data, DWORD dataSize);
DWORD* SearchHeap9x(HEAPLIST32* heapInfo, DWORD* data, DWORD dataSize);
