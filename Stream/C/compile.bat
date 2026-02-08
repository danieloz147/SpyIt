@echo off
cd /d "%~dp0"

call "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars64.bat" x64
if %errorlevel% neq 0 (
	echo [!] vcvars64.bat failed
	exit /b 1
)

cl.exe /EHsc /W4 /DUNICODE /D_UNICODE Stream.c user32.lib gdi32.lib dxgi.lib d3d11.lib windowscodecs.lib ws2_32.lib ole32.lib oleaut32.lib
