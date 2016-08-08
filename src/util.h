#ifndef _JUCE_UTIL_
#define _JUCE_UTIL_

// BMP operations
HRESULT LoadFromBMP(char* filename, HANDLE heap, BYTE** rgbBuf, BITMAPINFOHEADER** biInfoHeader);
HRESULT SaveAsBMP(char* filename, BYTE* rgbBuf, SIZE_T size, LONG width, LONG height, int bpp);

// AVI operations
HANDLE OpenAVIFile();
DWORD WriteAVIFrame(HANDLE hFile, BYTE *rgbBuf, SIZE_T size, DWORD times);
void CloseAVIFile(HANDLE);

#endif

