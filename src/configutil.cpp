// configutil.cpp

#include <stdio.h>
#include <string.h>
#include "config.h"
#include "log.h"

// this is needed to satisfy config module
FILE* log = NULL;

/**
 * Re-read the configuration file and try to match any custom config with the
 * specified processfile. If successfull, return true.
 * Parameter: [in out] cfg  - gets assigned to the pointer to new custom config.
 * NOTE: this function must be called from within the DLL - not the main Taksi app.
 */
BOOL SearchCustomConfig(char* processfile, char* dir, TAXI_CUSTOM_CONFIG** cfg)
{
	TAXI_CONFIG config = {
		DEFAULT_DEBUG,
		DEFAULT_CAPTURE_DIR,
		DEFAULT_TARGET_FRAME_RATE,
		DEFAULT_VKEY_INDICATOR, DEFAULT_VKEY_HOOK_MODE, 
		DEFAULT_VKEY_SMALL_SCREENSHOT, DEFAULT_VKEY_SCREENSHOT,
		DEFAULT_VKEY_VIDEO_CAPTURE,
		DEFAULT_USE_DIRECT_INPUT,
		DEFAULT_STARTUP_MODE_SYSTEM_WIDE,
		DEFAULT_FULL_SIZE_VIDEO,
		DEFAULT_CUSTOM_LIST
	};

	if (!ReadConfigFromDir(&config, dir)) return false;
	TAXI_CUSTOM_CONFIG* p = config.customList;

	TRACE4("SearchCustomConfig: processfile = \"%s\"", processfile);

	// lookup
	while (p != NULL)
	{
		// try to match the pattern with processfile
		TRACE4("SearchCustomConfig: comparing pattern \"%s\" ...", p->pattern);
			
		if (strstr(processfile, p->pattern)) 
		{
			TRACE4("SearchCustomConfig: Matched pattern in: %s", strstr(processfile, p->pattern));

			// make a copy of custom config object
			TAXI_CUSTOM_CONFIG* newCfg = 
				(TAXI_CUSTOM_CONFIG*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(TAXI_CUSTOM_CONFIG));
			if (!newCfg)
			{
				Log("SearchCustomConfig: Unable to allocate new custom config");
				return false;
			}
			lstrcpy(newCfg->appId, p->appId);
			lstrcpy(newCfg->pattern, p->pattern);
			newCfg->frameRate = p->frameRate;
			newCfg->frameWeight = p->frameWeight;
			newCfg->flags = p->flags;
			newCfg->next = NULL;

			// free the old custom config, if one existed
			if (*cfg) HeapFree(GetProcessHeap(), 0, *cfg);

			// save the pointer in output parameter
			*cfg = newCfg;

			LogWithNumber("Using custom frame rate: %u FPS", (*cfg)->frameRate);
			LogWithDouble("Using custom frame weight: %0.4f", (*cfg)->frameWeight);

			// Free memory which was allocated for read custom configs
			FreeCustomConfigs(&config);
			return true;
		}

		// not matched: go to next one
		p = p->next;
	}

	// Free memory which was allocated for read custom configs
	FreeCustomConfigs(&config);

	Log("SearchCustomConfig: No custom config match.");
	*cfg = NULL;
	return false;
}

