// log.cpp
#include <windows.h>
#include <stdio.h>

#include "log.h"
#include "config.h"

extern TAXI_CONFIG g_config;
static HANDLE mylog = INVALID_HANDLE_VALUE;

//////////////////////////////////////////////////
// FUNCTIONS
//////////////////////////////////////////////////

// Log a message into main (mydll) log file.
void MasterLog(char* logfile, char* procfile, char* msg)
{
	if (g_config.debug < 1) return;
	DWORD wbytes;
	char buf[BUFLEN];

	ZeroMemory(buf, BUFLEN);
	lstrcpy(buf, "process: ");
	lstrcat(buf, procfile);
	lstrcat(buf, msg);
	lstrcat(buf, "\r\n");

	HANDLE hFile = CreateFile(logfile,  // file to create 
				 GENERIC_WRITE,         // open for writing 
				 0,                     // do not share 
				 NULL,                  // default security 
				 OPEN_ALWAYS,           // append to existing
				 FILE_ATTRIBUTE_NORMAL, // normal file 
				 NULL);                 // no attr. template 

	if (hFile != INVALID_HANDLE_VALUE) 
	{
		SetFilePointer(hFile, 0, NULL, FILE_END);
		WriteFile(hFile, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
		CloseHandle(hFile);
	}
}

// Creates log file
void OpenLog(char* logName)
{
	if (g_config.debug > 0)
		mylog = CreateFile(logName,                    // file to create 
					 GENERIC_WRITE,                    // open for writing 
					 FILE_SHARE_READ | FILE_SHARE_DELETE,   // do not share 
					 NULL,                             // default security 
					 CREATE_ALWAYS,                    // overwrite existing 
					 FILE_ATTRIBUTE_NORMAL,            // normal file 
					 NULL);                            // no attr. template 
}

// Closes log file
void CloseLog()
{
	if (mylog != INVALID_HANDLE_VALUE) CloseHandle(mylog);
}

// Simple logger
void Log(char *msg)
{
	if (g_config.debug < 1) return;
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		WriteFile(mylog, (LPVOID)msg, lstrlen(msg), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple logger 2
void LogWithNumber(char *msg, DWORD number)
{
	if (g_config.debug < 1) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, number);
		WriteFile(mylog, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple logger 3
void LogWithDouble(char *msg, double number)
{
	if (g_config.debug < 1) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, number);
		WriteFile(mylog, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple logger 4
void LogWithString(char *msg, char* str)
{
	if (g_config.debug < 1) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, str);
		WriteFile(mylog, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple debugging logger
void Debug(char *msg)
{
	if (g_config.debug < 2) return;
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		WriteFile(mylog, (LPVOID)msg, lstrlen(msg), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple debugging logger 2
void DebugWithNumber(char *msg, DWORD number)
{
	if (g_config.debug < 2) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, number);
		WriteFile(mylog, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple debugging logger 3
void DebugWithDouble(char *msg, double number)
{
	if (g_config.debug < 2) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, number);
		WriteFile(mylog, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple debugging logger 4
void DebugWithString(char *msg, char* str)
{
	if (g_config.debug < 2) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, str);
		WriteFile(mylog, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

