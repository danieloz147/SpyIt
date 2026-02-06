# Screen Enumeration BOF - Step by Step Guide

Building a Beacon Object File (BOF) for adaptixC2/Cobalt Strike that enumerates display monitors. Start simple with C code, then convert to BOF format.

## 📁 Project Structure

```
SpyIt/
├── Enum-Screens/
│   └── Step1 Basic C usage/
│       ├── enumerate_screens.c      # Simple C program
│       └── compile.bat               # Build script
├── .gitignore
└── README.md
```

---

## 🚀 Step 1: Simple C Program (Current)

**Location:** `Enum-Screens/Step1 Basic C usage/`

### What it does:
- ✅ Enumerates all display monitors using Windows API
- ✅ Shows resolution, position, and primary status
- ✅ Standalone executable (no BOF yet)

### Files:
- `enumerate_screens.c` - Source code
- `compile.bat` - Build script

### How to build:
```cmd
cd "Enum-Screens/Step1 Basic C usage"
compile.bat
```

### How to run:
```cmd
enumerate_screens.exe
```

### Example output:
```
[+] Enumerating display monitors...
[*] Monitor 1:
    Resolution: 1920x1080
    Position: (0, 0)
    Primary: Yes

[+] Total screens detected: 1
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

## 📝 Next Steps (Coming Soon [TBD])

### Step 2: BOF Conversion
- Replace `printf()` with Beacon API (`BeaconPrintf`, `BeaconOutput`)
- Buffer management for remote output
- Requires `beacon.h` from adaptixC2 SDK

### Step 3: Production BOF
- Optimized Beacon API integration
- Function pointer resolution
- Ready for adaptixC2 deployment
- Supports x64 and x86

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

1. **Step 1** (Current): Understand Windows API basics
   - See how `EnumDisplayMonitors()` works
   - Test locally and observe output

2. **Step 2** (Next): Learn Beacon API
   - Replace standard output with C2 callbacks
   - Buffer management for remote execution

3. **Step 3** (Final): Production BOF
   - Optimize for C2 framework
   - Deploy to adaptixC2

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
- BOFs run in-process with minimal overhead
- No spawned processes means better stealth
- Limited to BeaconAPI functions (no full stdlib)
- Must be x64 or x86 compiled (match target architecture)

Happy coding! 🚀