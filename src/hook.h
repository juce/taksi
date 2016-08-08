#ifndef _HOOK_MAIN_
#define _HOOK_MAIN_

#ifdef MYDLL_RELEASE_BUILD
#define TAXI_WINDOW_TITLE "Taksi"
#else
#define TAXI_WINDOW_TITLE "Taksi (debug build)"
#endif
#define CREDITS "About Taksi: Version 0.5 (11/2004) by Juce."
#define MAINLOGFILE "taksi-main.log"

#define LOG(f,x) if (f != NULL) fprintf x

#endif

