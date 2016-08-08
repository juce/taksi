CC=cl
CFLAGS=/nologo $(EXTRA_CFLAGS) -EHsc
LINK=link
LFLAGS=/NOLOGO
LIBS=user32.lib gdi32.lib

all: taksi.dll taksi.exe
release:
	$(MAKE) EXTRA_CFLAGS="/DMYDLL_RELEASE_BUILD"
perf:
	$(MAKE) EXTRA_CFLAGS="/DMYDLL_CLOCK_STATS"

taksi.exe: hook.obj win32gui.obj config.obj hook.res taksi.dll
	$(LINK) $(LFLAGS) /out:taksi.exe hook.obj win32gui.obj config.obj taksi.lib hook.res $(LIBS)
hook.obj: hook.cpp hook.h win32gui.h shared.h config.h 
win32gui.obj: win32gui.cpp win32gui.h
config.obj: config.cpp config.h
log.obj: log.cpp log.h
configutil.obj: configutil.cpp config.h
graphx_ogl.obj: graphx_ogl.cpp graphx_ogl.h shared.h log.h config.h
imageutil.obj: imageutil.cpp imageutil.h
heap.obj: heap.cpp heap.h log.h

hook.res: 
	rc -r -fo hook.res hook.rc

mydll.res: 
	rc -r -fo mydll.res mydll.rc

taksi.lib: mydll.obj configutil.obj config.obj log.obj graphx_ogl.obj imageutil.obj
taksi.dll: taksi.lib mydll.res
	$(LINK) $(LFLAGS) /out:taksi.dll /DLL mydll.obj configutil.obj config.obj log.obj graphx_ogl.obj imageutil.obj mydll.res $(LIBS)
mydll.obj: mydll.cpp mydll.h shared.h config.h log.h

.cpp.obj:
	$(CC) $(CFLAGS) -c $<

clean:
	del /Q /F *.exp *.lib *.obj *.res

clean-all:
	del /Q /F *.exp *.lib *.obj *.res *.log *~
