// config.cpp

#define BUFLEN 4096

#include <stdio.h>
#include <string.h>
#include "hook.h"
#include "config.h"

extern FILE* log;

/**
 * Looks up a custom config in a list. If not found return NULL
 */
TAXI_CUSTOM_CONFIG* LookupExistingCustomConfig(TAXI_CONFIG* config, char* appId)
{
	if (config == NULL) return NULL;

	LOG(log, (log, "Looking up existing custom config <%s>...\n", appId));
	TAXI_CUSTOM_CONFIG* p = config->customList;

	// lookup
	while (p != NULL)
	{
		if (lstrcmp(p->appId, appId)==0) return p;
		p = p->next;
	}

	LOG(log, (log, "<%s> NOT FOUND.\n", appId));
	return NULL;
}

/**
 * Looks up a custom config in a list. If not found, creates one
 * and inserts at the beginning of the list.
 */
TAXI_CUSTOM_CONFIG* LookupCustomConfig(TAXI_CONFIG* config, char* appId)
{
	if (config == NULL) return NULL;

	TAXI_CUSTOM_CONFIG* p = config->customList;

	// lookup
	while (p != NULL)
	{
		if (lstrcmp(p->appId, appId)==0) return p;
		p = p->next;
	}

	// not found. Therefore, insert a new one
	p = (TAXI_CUSTOM_CONFIG*)HeapAlloc(
			GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(TAXI_CUSTOM_CONFIG));
	if (p == NULL) return NULL;

	strncpy(p->appId, appId, BUFLEN-1);
	p->next = config->customList;
	config->customList = p;
	return p;
}

/**
 * Deletes a custom config from the list and frees its memory. 
 * If not found, nothing is done.
 */
VOID DeleteCustomConfig(TAXI_CONFIG* config, char* appId)
{
	if (config == NULL) return;

	TAXI_CUSTOM_CONFIG* p = config->customList;
	if (p == NULL) return;

	// check first element
	if (lstrcmp(p->appId, appId)==0)
	{
		config->customList = p->next;
		HeapFree(GetProcessHeap(), 0, p);
	}
	else
	{
		// lookup in the rest of the list
		TAXI_CUSTOM_CONFIG* q = p->next; 
		while (q != NULL)
		{
			if (lstrcmp(q->appId, appId)==0) 
			{
				p->next = q->next;
				HeapFree(GetProcessHeap(), 0, q);
				q = p->next;
			}
			else 
			{
				p = q;
				q = q->next;
			}
		}
	}
}

/**
 * Free memory taken by custom configs
 */
VOID FreeCustomConfigs(TAXI_CONFIG* config)
{
	if (config == NULL) return;

	TAXI_CUSTOM_CONFIG* p = config->customList;
	TAXI_CUSTOM_CONFIG* q = NULL;

	while (p != NULL)
	{
		q = p->next;
		HeapFree(GetProcessHeap(), 0, p);
		p = q;
	}

	config->customList = NULL;
}

/**
 * Returns true if successful.
 */
BOOL ReadConfig(TAXI_CONFIG* config)
{
	return ReadConfigFromDir(config, NULL);
}

/**
 * Returns true if successful.
 */
BOOL ReadConfigFromDir(TAXI_CONFIG* config, char* dir)
{
	if (config == NULL) return false;

	char configFilename[BUFLEN];
	ZeroMemory(configFilename, BUFLEN);
	if (dir != NULL) lstrcpy(configFilename, dir);
	lstrcat(configFilename, CONFIG_FILE);

	FILE* cfg = fopen(configFilename, "rt");
	if (cfg == NULL) return false;

	char str[BUFLEN];
	char name[BUFLEN];
	int value = 0;
	float dvalue = 0.0f;

	char *pName = NULL, *pValue = NULL, *comment = NULL;
	while (true)
	{
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN-1, cfg);
		if (feof(cfg)) break;

		// skip comments
		comment = strstr(str, "#");
		if (comment != NULL) comment[0] = '\0';

		// parse the line
		pName = pValue = NULL;
		ZeroMemory(name, BUFLEN); value = 0;
		char* eq = strstr(str, "=");
		if (eq == NULL || eq[1] == '\0') continue;

		eq[0] = '\0';
		pName = str; pValue = eq + 1;

		ZeroMemory(name, NULL); 
		sscanf(pName, "%s", name);

		if (lstrcmp(name, "debug")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LOG(log, (log, "ReadConfig: debug = (%d)\n", value));
			config->debug = value;
		}
		else if (lstrcmp(name, "capture.dir")==0)
		{
			char* startQuote = strstr(pValue, "\"");
			if (startQuote == NULL) continue;
			char* endQuote = strstr(startQuote + 1, "\"");
			if (endQuote == NULL) continue;

			char buf[BUFLEN];
			ZeroMemory(buf, BUFLEN);
			memcpy(buf, startQuote + 1, endQuote - startQuote - 1);

			LOG(log, (log, "ReadConfig: captureDir = \"%s\"\n", buf));
			ZeroMemory(config->captureDir, BUFLEN);
			strncpy(config->captureDir, buf, BUFLEN-1);
		}
		else if (lstrcmp(name, "movie.frameRate.target")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LOG(log, (log, "ReadConfig: targetFrameRate = %d\n", value));
			config->targetFrameRate = value;
		}
		else if (lstrcmp(name, "vKey.indicatorToggle")==0)
		{
			if (sscanf(pValue, "%x", &value)!=1) continue;
			LOG(log, (log, "ReadConfig: vKeyIndicator = 0x%02x\n", value));
			config->vKeyIndicator = value;
		}
		else if (lstrcmp(name, "vKey.hookModeToggle")==0)
		{
			if (sscanf(pValue, "%x", &value)!=1) continue;
			LOG(log, (log, "ReadConfig: vKeyHookMode = 0x%02x\n", value));
			config->vKeyHookMode = value;
		}
		else if (lstrcmp(name, "vKey.smallScreenShot")==0)
		{
			if (sscanf(pValue, "%x", &value)!=1) continue;
			LOG(log, (log, "ReadConfig: vKeySmallScreenShot = 0x%02x\n", value));
			config->vKeySmallScreenShot = value;
		}
		else if (lstrcmp(name, "vKey.screenShot")==0)
		{
			if (sscanf(pValue, "%x", &value)!=1) continue;
			LOG(log, (log, "ReadConfig: vKeyScreenShot = 0x%02x\n", value));
			config->vKeyScreenShot = value;
		}
		else if (lstrcmp(name, "vKey.videoCapture")==0)
		{
			if (sscanf(pValue, "%x", &value)!=1) continue;
			LOG(log, (log, "ReadConfig: vKeyVideoCapture = 0x%02x\n", value));
			config->vKeyVideoCapture = value;
		}
		else if (lstrcmp(name, "keyboard.useDirectInput")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LOG(log, (log, "ReadConfig: useDirectInput = %d\n", value));
			config->useDirectInput = (value != 0);
		}
		else if (lstrcmp(name, "startup.hookMode.systemWide")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LOG(log, (log, "ReadConfig: startupModeSystemWide = %d\n", value));
			config->startupModeSystemWide = (value != 0);
		}
		else if (lstrcmp(name, "movie.fullSize")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LOG(log, (log, "ReadConfig: fullSizeVideo = %d\n", value));
			config->fullSizeVideo = (value != 0);
		}

		else if (strstr(name, "custom.") == name)
		{
			// it's a custom setting.
			// STEP 1: determine appId and setting
			char* appId = name + lstrlen("custom.");
			char* dot2 = strstr(appId, ".");
			if (dot2 == NULL || dot2[1] == '\0') continue; else dot2[0] = '\0';
			char* setting = dot2 + 1;

			// STEP 2: lookup/add custom config
			TAXI_CUSTOM_CONFIG* cfg = LookupCustomConfig(config, appId);
			
			// STEP 3: set the setting
			if (lstrcmp(setting, "pattern")==0)
			{
				char* startQuote = strstr(pValue, "\"");
				if (startQuote == NULL) continue;
				char* endQuote = strstr(startQuote + 1, "\"");
				if (endQuote == NULL) continue;

				char buf[BUFLEN];
				ZeroMemory(buf, BUFLEN);
				memcpy(buf, startQuote + 1, endQuote - startQuote - 1);

				LOG(log, (log, "ReadConfig: <%s> pattern = \"%s\"\n", appId, buf));
				ZeroMemory(cfg->pattern, BUFLEN);
				strncpy(cfg->pattern, buf, BUFLEN-1);
			}
			else if (lstrcmp(setting, "frameRate")==0)
			{
				if (sscanf(pValue, "%d", &value)!=1) continue;
				LOG(log, (log, "ReadConfig: <%s> frameRate = %d\n", appId, value));
				cfg->frameRate = value;
			}
			else if (lstrcmp(setting, "frameWeight")==0)
			{
				if (sscanf(pValue, "%f", &dvalue)!=1) continue;
				LOG(log, (log, "ReadConfig: <%s> frameWeight = %f\n", appId, dvalue));
				cfg->frameWeight = dvalue;
			}
		}
	}
	fclose(cfg);

	return true;
}

/**
 * Returns true if successful.
 */
BOOL WriteConfig(TAXI_CONFIG* config)
{
	char* buf = NULL;
	DWORD size = 0;

	// first read all lines
	HANDLE oldCfg = CreateFile(CONFIG_FILE, GENERIC_READ, 0, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (oldCfg != INVALID_HANDLE_VALUE)
	{
		size = GetFileSize(oldCfg, NULL);
		buf = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
		if (buf == NULL) return false;

		DWORD dwBytesRead = 0;
		ReadFile(oldCfg, buf, size, &dwBytesRead, NULL);
		if (dwBytesRead != size) 
		{
			HeapFree(GetProcessHeap(), 0, buf);
			return false;
		}
		CloseHandle(oldCfg);
	}

	// create new file
	FILE* cfg = fopen(CONFIG_FILE, "wt");

	// loop over every line from the old file, and overwrite it in the new file
	// if necessary. Otherwise - copy the old line.
	
	BOOL bWrittenCaptureDir = false; BOOL bWrittenTargetFrameRate = false;
	BOOL bWrittenVKeyIndicator = false; BOOL bWrittenVKeyHookMode = false;
	BOOL bWrittenVKeySmallScreenShot = false; BOOL bWrittenVKeyScreenShot = false;
	BOOL bWrittenVKeyVideoCapture = false;
	BOOL bWrittenUseDirectInput = false;
	BOOL bWrittenStartupModeSystemWide = false;
	BOOL bWrittenFullSizeVideo = false;

	char* line = buf; BOOL done = false;
	char* comment = NULL;
	if (buf != NULL) while (!done && line < buf + size)
	{
		char* endline = strstr(line, "\r\n");
		if (endline != NULL) endline[0] = '\0'; else done = true;
		char* comment = strstr(line, "#");
		char* setting = NULL;

		if ((setting = strstr(line, "capture.dir")) && 
			(comment == NULL || setting < comment))
		{
			fprintf(cfg, "capture.dir = \"%s\"\n", config->captureDir);
			bWrittenCaptureDir = true;
		}
		else if ((setting = strstr(line, "movie.frameRate.target")) && 
				 (comment == NULL || setting < comment))
		{
			fprintf(cfg, "movie.frameRate.target = %d\n", config->targetFrameRate);
			bWrittenTargetFrameRate = true;
		}
		else if ((setting = strstr(line, "vKey.indicatorToggle")) && 
				 (comment == NULL || setting < comment))
		{
			fprintf(cfg, "vKey.indicatorToggle = 0x%02x\n", config->vKeyIndicator);
			bWrittenVKeyIndicator = true;
		}
		else if ((setting = strstr(line, "vKey.hookModeToggle")) && 
				 (comment == NULL || setting < comment))
		{
			fprintf(cfg, "vKey.hookModeToggle = 0x%02x\n", config->vKeyHookMode);
			bWrittenVKeyHookMode = true;
		}
		else if ((setting = strstr(line, "vKey.smallScreenShot")) && 
				 (comment == NULL || setting < comment))
		{
			fprintf(cfg, "vKey.smallScreenShot = 0x%02x\n", config->vKeySmallScreenShot);
			bWrittenVKeySmallScreenShot = true;
		}
		else if ((setting = strstr(line, "vKey.screenShot")) && 
				 (comment == NULL || setting < comment))
		{
			fprintf(cfg, "vKey.screenShot = 0x%02x\n", config->vKeyScreenShot);
			bWrittenVKeyScreenShot = true;
		}
		else if ((setting = strstr(line, "vKey.videoCapture")) && 
				 (comment == NULL || setting < comment))
		{
			fprintf(cfg, "vKey.videoCapture = 0x%02x\n", config->vKeyVideoCapture);
			bWrittenVKeyVideoCapture = true;
		}
		else if ((setting = strstr(line, "keyboard.useDirectInput")) && 
				 (comment == NULL || setting < comment))
		{
			fprintf(cfg, "keyboard.useDirectInput = %d\n", config->useDirectInput ? 1 : 0);
			bWrittenUseDirectInput = true;
		}
		else if ((setting = strstr(line, "startup.hookMode.systemWide")) && 
				 (comment == NULL || setting < comment))
		{
			fprintf(cfg, "startup.hookMode.systemWide = %d\n", config->startupModeSystemWide ? 1 : 0);
			bWrittenStartupModeSystemWide = true;
		}
		else if ((setting = strstr(line, "movie.fullSize")) && 
				 (comment == NULL || setting < comment))
		{
			fprintf(cfg, "movie.fullSize = %d\n", config->fullSizeVideo ? 1 : 0);
			bWrittenFullSizeVideo = true;
		}

		else if ((setting = strstr(line, "custom.")) &&
				 (comment == NULL || setting < comment))
		{
			// it's a custom setting line.
			// STEP 1: determine appId and setting
			char* appId = setting + lstrlen("custom.");
			char* dot2 = strstr(appId, ".");
			if (dot2 == NULL || dot2[1] == '\0') continue; else dot2[0] = '\0';
			char* customSetting = dot2 + 1;

			// STEP 2: lookup custom config
			TAXI_CUSTOM_CONFIG* conf = LookupExistingCustomConfig(config, appId);
			if (conf == NULL) continue; // this line effectively doesn't make it to new cfg-file.
			
			// STEP 3: set the setting
			if (strstr(customSetting, "pattern") == customSetting)
			{
				if (conf->flags & CUSTOM_PATTERN_SAVED) continue;
				fprintf(cfg, "custom.%s.pattern = \"%s\"\n", conf->appId, conf->pattern);
				conf->flags |= CUSTOM_PATTERN_SAVED;
			}
			else if (strstr(customSetting, "frameRate") == customSetting)
			{
				if (conf->flags & CUSTOM_FRAME_RATE_SAVED) continue;
				fprintf(cfg, "custom.%s.frameRate = %d\n", conf->appId, conf->frameRate);
				conf->flags |= CUSTOM_FRAME_RATE_SAVED;
			}
			else if (strstr(customSetting, "frameWeight") == customSetting)
			{
				if (conf->flags & CUSTOM_FRAME_WEIGHT_SAVED) continue;
				fprintf(cfg, "custom.%s.frameWeight = %f\n", conf->appId, conf->frameWeight);
				conf->flags |= CUSTOM_FRAME_WEIGHT_SAVED;
			}
		}
		else
		{
			// take the old line
			fprintf(cfg, "%s\n", line);
		}

		if (endline != NULL) { endline[0] = '\r'; line = endline + 2; }
	}

	// if something wasn't written, make sure we write it.
	if (!bWrittenCaptureDir) 
		fprintf(cfg, "capture.dir = \"%s\"\n", config->captureDir);
	if (!bWrittenTargetFrameRate)
		fprintf(cfg, "movie.frameRate.target = %d\n", config->targetFrameRate);
	if (!bWrittenVKeyIndicator)
		fprintf(cfg, "vKey.indicatorToggle = 0x%02x\n", config->vKeyIndicator);
	if (!bWrittenVKeyHookMode)
		fprintf(cfg, "vKey.hookModeToggle = 0x%02x\n", config->vKeyHookMode);
	if (!bWrittenVKeySmallScreenShot)
		fprintf(cfg, "vKey.smallScreenShot = 0x%02x\n", config->vKeySmallScreenShot);
	if (!bWrittenVKeyScreenShot)
		fprintf(cfg, "vKey.screenShot = 0x%02x\n", config->vKeyScreenShot);
	if (!bWrittenVKeyVideoCapture)
		fprintf(cfg, "vKey.videoCapture = 0x%02x\n", config->vKeyVideoCapture);
	if (!bWrittenUseDirectInput)
		fprintf(cfg, "keyboard.useDirectInput = %d\n", config->useDirectInput ? "1" : "0");
	if (!bWrittenStartupModeSystemWide)
		fprintf(cfg, "startup.hookMode.systemWide = %d\n", config->startupModeSystemWide ? "1" : "0");
	if (!bWrittenFullSizeVideo)
		fprintf(cfg, "movie.fullSize = %d\n", config->fullSizeVideo ? "1" : "0");

	// same goes for custom configs
	TAXI_CUSTOM_CONFIG* p = config->customList;
	while (p != NULL)
	{
		if (!(p->flags & CUSTOM_PATTERN_SAVED))
		{
			fprintf(cfg, "custom.%s.pattern = \"%s\"\n", p->appId, p->pattern);
			p->flags |= CUSTOM_PATTERN_SAVED;
		}
		if (!(p->flags & CUSTOM_FRAME_RATE_SAVED))
		{
			fprintf(cfg, "custom.%s.frameRate = %d\n", p->appId, p->frameRate);
			p->flags |= CUSTOM_FRAME_RATE_SAVED;
		}
		if (!(p->flags & CUSTOM_FRAME_WEIGHT_SAVED))
		{
			fprintf(cfg, "custom.%s.frameWeight = %f\n", p->appId, p->frameWeight);
			p->flags |= CUSTOM_FRAME_WEIGHT_SAVED;
		}
		p = p->next;
	}

	// now clear the flags so that next save will work too
	p = config->customList;
	while (p != NULL) { p->flags = 0; p = p->next; }

	// release buffer
	HeapFree(GetProcessHeap(), 0, buf);

	fclose(cfg);

	return true;
}

