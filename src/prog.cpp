#include <windows.h>
#include <stdio.h>

typedef BOOL (*INSTALLTHEHOOK)(void);
typedef BOOL (*UNINSTALLTHEHOOK)(void);
typedef BOOL (*TAKESCREENSHOT)(DWORD);
typedef void (*SETDEBUG)(DWORD);
typedef void (*SETCAPTUREDIR)(char *);

INSTALLTHEHOOK InstallTheHook;
UNINSTALLTHEHOOK UninstallTheHook;
TAKESCREENSHOT TakeScreenShot;
SETCAPTUREDIR SetCaptureDir;
SETDEBUG SetDebug;


int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: %s <process-id> [<delay-in-sec>]", argv[0]);
        return 0;
    }

    DWORD processId = atoi(argv[1]);

    char currDir[512];
    memset(currDir, 0, sizeof(currDir));
    GetCurrentDirectory(sizeof(currDir)-1, currDir);
    strcat(currDir,"\\");

    HMODULE hTaksi = LoadLibrary("taksi");
    InstallTheHook = (INSTALLTHEHOOK)GetProcAddress(hTaksi, "InstallTheHook");
    UninstallTheHook = (UNINSTALLTHEHOOK)GetProcAddress(hTaksi, "UninstallTheHook");
    TakeScreenShot = (TAKESCREENSHOT)GetProcAddress(hTaksi, "TakeScreenShot");
    SetCaptureDir = (SETCAPTUREDIR)GetProcAddress(hTaksi, "SetCaptureDir");
    SetDebug = (SETDEBUG)GetProcAddress(hTaksi, "SetDebug");

    SetDebug(0);
    InstallTheHook();
    SetCaptureDir(currDir);

    printf("Make sure your target application (pid=%d) is hooked\n", processId);
    printf("by switching focus to it, for example, and then back here.\n");
    printf("Then, press a key to take a screenshot");
    getchar();

    if (argc > 2) {
        DWORD delay = atoi(argv[2]);
        printf("Waiting %d seconds before taking screenshot ...\n", delay);
        Sleep(delay * 1000);
    }

    if (!TakeScreenShot(processId)) {
        printf("PROBLEM taking screenshot.\n");
    }

    UninstallTheHook();
    return 0;
}
