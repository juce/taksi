@echo off
echo Setting kitserver compile environment
@call "c:\program files\microsoft visual studio 12.0\vc\vcvarsall.bat"
set INCLUDE=%INCLUDE%;.\mfc
echo Environment set

