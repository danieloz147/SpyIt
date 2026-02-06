# Screen Enumeration BOF - Step by Step Guide

Building a Beacon Object File (BOF) for adaptixC2/Cobalt Strike that enumerates display monitors. Start simple with C code, then convert to BOF format.

## 📁 Project Structure

```
SpyIt/
├── Enum-Screens/
│   ├── Step1_Basic_C_usage/
│   │   ├── enumerate_screens.c      # Simple C program
│   │   └── compile.bat              # Build EXE
│   └── Step2_BOF-Conversion/
│       ├── enumerate_screens.c      # BOF source (BeaconPrintf)
│       ├── compile.bat              # Build BOF (x86+x64)
│       ├── enum-screens.axs         # Adaptix AxScript command
│       └── beacon.h                 # Adaptix beacon header
├── .gitignore
└── README.md
```

---

## 🚀 Step 1: Simple C Program

**Location:** `Enum-Screens/Step1_Basic_C_usage/`

### What it does:
- ✅ Enumerates all display monitors using Windows API
- ✅ Shows resolution, position, and primary status
- ✅ Standalone executable (no BOF yet)

### Files:
- `enumerate_screens.c` - Source code
- `compile.bat` - Build script

### How to build:
```cmd
cd "Enum-Screens/Step1_Basic_C_usage"
compile.bat
```

### How to run:
```cmd
enumerate_screens.exe
```

### Example output:
```
[?] Enumerating display monitors...
[+] Monitor 0 [PRIMARY]: Resolution: 1707x960
[+] Monitor 1          : Resolution: 2293x960

[!] Total screens detected: 2
```

### Key Windows API:
- `EnumDisplayMonitors()` - Enumerate all monitors
- `GetMonitorInfo()` - Get monitor details (resolution, position)
- `MONITORINFO` structure - Store monitor data

### Compiler Setup:
Uses **MSVC** (Visual Studio 2017) with:
- `user32.lib` - Window and monitor APIs
- `gdi32.lib` - Graphics device interface

---

## ✅ Step 2: BOF Conversion (Current)

**Location:** `Enum-Screens/Step2_BOF-Conversion/`

### What it does:
- BOF version using `BeaconPrintf`
- Adaptix AxScript command (`enum-screens`)
- Builds both x64 and x86 object files

### How to build:
```cmd
cd "Enum-Screens/Step2_BOF-Conversion"
compile.bat
```

### Output files:
- `enumerate_screens.x64.o`
- `enumerate_screens.x86.o` (if x86 toolchain is available)

### How to load in Adaptix:
Load `enum-screens.axs` via AxScript → Script Manager, then run:
```
enum-screens
```

---

## 🛠️ Requirements

### Compiler:
- **Visual Studio 2017+** (or Build Tools)
- MSVC C/C++ compiler
- Windows 10/11 SDK

### Libraries:
- `user32.lib` - For monitor enumeration
- `gdi32.lib` - For device context

---

## 📚 Learning Path

1. **Step 1**: Understand Windows API basics
   - See how `EnumDisplayMonitors()` works
   - Test locally and observe output

2. **Step 2** (Current): Learn Beacon API
   - Replace standard output with C2 callbacks
   - Load and execute in Adaptix

---

## 🐛 Troubleshooting

### Compilation fails with "windows.h not found"
Run the vcvars setup first:
```cmd
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
```

Or use the provided `compile.bat` which handles this automatically.

### Missing DLL error (libgcc_s_dw2-1.dll)
This project uses MSVC, not GCC - make sure you're using the right compiler.

---

## 📖 References

- [Windows API - EnumDisplayMonitors](https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumdisplaymonitors)
- [Windows API - MONITORINFO](https://docs.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-monitorinfo)
- [Beacon Object Files - Cobalt Strike](https://www.cobaltstrike.com/help-beacon-object-files)
- [adaptixC2](https://adaptix.dev)

---

## 📝 Notes

- BOFs run in-process with minimal overhead
- No spawned processes = better stealth
- `.gitignore` excludes build artifacts (`*.exe`, `*.obj`, `*.dll`)
- Limited to BeaconAPI functions (no full stdlib)
- Must be x64 or x86 compiled (match target architecture)

Happy coding! 🚀