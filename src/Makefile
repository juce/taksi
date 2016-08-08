INC=\DXSDK9\Include
STL=\STL
CC=cl
CFLAGS=/nologo /DMYDLL_RELEASE_BUILD -GX
LINK=link
LFLAGS=/NOLOGO /LIBPATH:\DXSDK9\Lib
LIBS=user32.lib dinput8.lib dxguid.lib gdi32.lib

all: taksi.dll taksi.exe
release:
	$(MAKE) EXTRA_CFLAGS=/DMYDLL_RELEASE_BUILD
perf:
	$(MAKE) EXTRA_CFLAGS=/DMYDLL_CLOCK_STATS

taksi.exe: hook.obj win32gui.obj config.obj det_dx8.obj det_dx9.obj hook.res taksi.dll
	$(LINK) $(LFLAGS) /out:taksi.exe hook.obj win32gui.obj config.obj det_dx8.obj det_dx9.obj taksi.lib hook.res $(LIBS)
hook.obj: hook.cpp hook.h win32gui.h shared.h config.h 
win32gui.obj: win32gui.cpp win32gui.h
config.obj: config.cpp config.h
det_dx8: det_dx8.cpp
det_dx9: det_dx9.cpp
log.obj: log.cpp log.h
configutil.obj: configutil.cpp config.h
graphx_dx8.obj: graphx_dx8.cpp graphx_dx8.h shared.h log.h config.h heap.h 
graphx_dx9.obj: graphx_dx9.cpp graphx_dx9.h shared.h log.h config.h heap.h 
graphx_ogl.obj: graphx_ogl.cpp graphx_ogl.h shared.h log.h config.h
dinp.obj: dinp.cpp dinp.h
imageutil.obj: imageutil.cpp imageutil.h
heap.obj: heap.cpp heap.h log.h

hook.res: 
	rc -r -fo hook.res hook.rc

mydll.res: 
	rc -r -fo mydll.res mydll.rc

taksi.lib: mydll.obj configutil.obj config.obj log.obj graphx_dx8.obj graphx_dx9.obj graphx_ogl.obj dinp.obj imageutil.obj
taksi.dll: taksi.lib mydll.res
	$(LINK) $(LFLAGS) /out:taksi.dll /DLL mydll.obj configutil.obj config.obj log.obj graphx_dx8.obj graphx_dx9.obj graphx_ogl.obj dinp.obj imageutil.obj mydll.res $(LIBS)
mydll.obj: mydll.cpp mydll.h shared.h config.h log.h

.cpp.obj:
	$(CC) $(CFLAGS) -c /I $(INC) /I $(STL) $<

clean:
	del /Q /F *.exp *.lib *.obj *.res

clean-all:
	del /Q /F *.exp *.lib *.obj *.res *.log *~
