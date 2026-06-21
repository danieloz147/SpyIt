@echo off
cd /d "%~dp0"

call "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars64.bat" x64
if %errorlevel% neq 0 (
	echo [!] vcvars64.bat failed
	exit /b 1
)

cl.exe /D_WIN64 /D_WIN32 /D_M_AMD64 /D_AMD64_ enumerate_screens.c /link user32.lib gdi32.lib
