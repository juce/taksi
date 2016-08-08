#ifndef _JUCE_LOG_
#define _JUCE_LOG_

/* debugging macros */
#ifndef MYDLL_RELEASE_BUILD
#define TRACE(x) Debug(x)
#define TRACE2(x,y) DebugWithNumber(x,y)
#define TRACE3(x,y) DebugWithDouble(x,y)
#define TRACE4(x,y) DebugWithString(x,y)
#define MASTERTRACE(x,y,z) MasterLog(x,y,z)
#define MASTERTRACE2(x,y,z,w) {\
	char buf[BUFLEN]; \
	ZeroMemory(buf, BUFLEN); \
	sprintf(buf, z, w); \
	MasterLog(x, y, buf);\
}
#else
#define TRACE(x) 
#define TRACE2(x,y) 
#define TRACE3(x,y) 
#define TRACE4(x,y) 
#define MASTERTRACE(x,y,z)
#define MASTERTRACE2(x,y,z,w)
#endif

/* macros for gathering time statistics */
#ifdef MYDLL_CLOCK_STATS
#define CLOCK_START(indent, clock) { indent++; clock.push_front(QPC()); }
#define CLOCK_STOP_AND_LOG(str, indent, clock) {\
	indent--;\
	DWORD elapsed = QPC() - clock.front();\
	clock.pop_front();\
	LogWithNumber(str, elapsed);\
}
#else
#define CLOCK_START(indent, clock)
#define CLOCK_STOP_AND_LOG(str, indent, clock)
#endif

void OpenLog(char* logName);
void CloseLog();

void MasterLog(char* logfile, char* procfile, char* msg);
void Log(char* msg);
void LogWithNumber(char* msg, DWORD number);
void LogWithDouble(char* msg, double number);
void LogWithString(char* msg, char* str);

void Debug(char* msg);
void DebugWithNumber(char* msg, DWORD number);
void DebugWithDouble(char* msg, double number);
void DebugWithString(char* msg, char* str);

#endif

