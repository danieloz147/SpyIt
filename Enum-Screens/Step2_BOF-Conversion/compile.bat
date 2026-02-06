@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo [+] Compiling AdaptixC2 BOF: enumerate_screens.c



REM x64 BUILD
echo [*] Building x64...
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

if not exist "beacon.h" (
    echo [!] ERROR: beacon.h missing!
    pause
    exit /b 1
)

cl.exe /nologo /O2 /Gs999999 /GR- /EHs- /EHa- /Zl /guard:cf- /Oi- /I. /c enumerate_screens.c /Fo:enumerate_screens.x64.o /GS-

if %errorlevel% equ 0 (
    for %%F in (enumerate_screens.x64.o) do set size=%%~zF
    echo [+] SUCCESS! enumerate_screens.x64.o ^(%size% bytes^)
) else (
    echo [!] x64 BUILD FAILED!
)

echo.
REM x86 BUILD
echo [*] Building x86...
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars32.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
    cl.exe /nologo /O2 /Gs999999 /GR- /EHs- /EHa- /Zl /guard:cf- /Oi- /I. /c enumerate_screens.c /Fo:enumerate_screens.x86.o /GS-
    if %errorlevel% equ 0 (
        for %%F in (enumerate_screens.x86.o) do set size=%%~zF
        echo [+] SUCCESS! enumerate_screens.x86.o ^(%size% bytes^)
    ) else (
        echo [!] x86 BUILD FAILED!
    )
) else (
    echo [!] vcvars32.bat not found - skipping x86 build
)

echo.
echo ========================================
echo ADAPTIXC2:
echo   execute bof enumerate_screens.x64.o
echo   execute bof enumerate_screens.x86.o
echo ========================================

pause