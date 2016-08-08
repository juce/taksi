@echo off
echo Setting kitserver compile environment
@call "c:\program files\microsoft visual studio 12.0\vc\vcvarsall.bat"
set DXSDK=c:\dxsdk_jun2010
set DXSDK8=c:\dxsdk81
set INCLUDE=%INCLUDE%;%DXSDK%\include;%DXSDK8%\include;.\mfc
set LIB=%LIB%;%DXSDK%\lib\x86;%DXSDK8%\lib
echo Environment set

