// Stream.c  (DXGI Desktop Duplication -> H.264 (Media Foundation) -> output.mp4 + prints output Mbps)
// Build (VS2017 x64):
// cl /EHsc /W4 /DUNICODE /D_UNICODE Stream.c user32.lib gdi32.lib dxgi.lib d3d11.lib mfplat.lib mfreadwrite.lib mfuuid.lib ole32.lib

#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define INITGUID
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <initguid.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wincodec.h>
#include <stdio.h>
#include <stdint.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define HR(x) do { HRESULT _hr=(x); if (FAILED(_hr)) { \
  fprintf(stderr, "\nHRESULT 0x%08X at %s:%d\n", (unsigned)_hr, __FILE__, __LINE__); \
  goto cleanup; } } while(0)

static volatile LONG g_stop = 0;
static volatile LONG g_capture_stop = 0;
static volatile LONG g_pending_screen = -1;
static volatile LONG g_current_screen = 0;
static volatile LONG g_screen_count = 0;
static volatile LONG g_logs_enabled = 0;

static BOOL WINAPI on_ctrl(DWORD type) {
  if (type == CTRL_C_EVENT || type == CTRL_CLOSE_EVENT || type == CTRL_BREAK_EVENT) {
    InterlockedExchange(&g_stop, 1);
    return TRUE;
  }
  return FALSE;
}

static void log_line(const char* fmt, ...) {
  if (InterlockedCompareExchange(&g_logs_enabled, 0, 0) == 0) return;
  FILE* f = NULL;
  fopen_s(&f, "stream.log", "a");
  if (!f) return;
  SYSTEMTIME st;
  GetLocalTime(&st);
  fprintf(f, "%04u-%02u-%02u %02u:%02u:%02u ",
          st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(f, fmt, ap);
  va_end(ap);
  fprintf(f, "\n");
  fclose(f);
}

static LONG WINAPI on_unhandled_exception(EXCEPTION_POINTERS* e) {
  log_line("Unhandled exception: code=0x%08X addr=%p",
           (unsigned)e->ExceptionRecord->ExceptionCode,
           e->ExceptionRecord->ExceptionAddress);
  return EXCEPTION_EXECUTE_HANDLER;
}

static LONG log_exception(EXCEPTION_POINTERS* e, const char* tag) {
  log_line("%s exception: code=0x%08X addr=%p",
           tag,
           (unsigned)e->ExceptionRecord->ExceptionCode,
           e->ExceptionRecord->ExceptionAddress);
  return EXCEPTION_EXECUTE_HANDLER;
}

typedef struct SharedFrame {
  SRWLOCK lock;
  uint8_t* bgra;       // CPU image buffer
  uint32_t stride;     // bytes per row
  uint32_t w, h;
  uint64_t updates;    // number of NEW frames captured
} SharedFrame;

typedef struct CapCtx {
  ID3D11Device* dev;
  ID3D11DeviceContext* ctx;
  IDXGIOutputDuplication* dupl;
  ID3D11Texture2D* staging;
  SharedFrame* shared;
  BYTE* pointerShape;
  UINT pointerShapeSize;
  DXGI_OUTDUPL_POINTER_SHAPE_INFO pointerInfo;
  POINT pointerPos;
  BOOL pointerVisible;
  RECT desktopRect;
} CapCtx;
static void overlay_cursor_bgra(uint8_t* dst, uint32_t dstStride, uint32_t dstW, uint32_t dstH,
                                const DXGI_OUTDUPL_POINTER_SHAPE_INFO* info, const BYTE* shape,
                                POINT pos, BOOL visible, RECT desktopRect) {
  if (!visible || !shape || !info) return;

  int x0 = (pos.x - desktopRect.left) - (int)info->HotSpot.x;
  int y0 = (pos.y - desktopRect.top) - (int)info->HotSpot.y;

  UINT w = info->Width;
  UINT h = info->Height;
  if (info->Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME) {
    h = (h / 2); // AND + XOR masks stacked
  }
  UINT pitch = info->Pitch;

  if (info->Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME) {
    for (UINT y = 0; y < h; y++) {
      int dy = y0 + (int)y;
      if (dy < 0 || dy >= (int)dstH) continue;

      const BYTE* andRow = shape + (SIZE_T)y * pitch;
      const BYTE* xorRow = shape + (SIZE_T)(y + h) * pitch;
      uint8_t* dstRow = dst + (SIZE_T)dy * dstStride;

      for (UINT x = 0; x < w; x++) {
        int dx = x0 + (int)x;
        if (dx < 0 || dx >= (int)dstW) continue;

        BYTE andBit = (andRow[x / 8] >> (7 - (x % 8))) & 1;
        BYTE xorBit = (xorRow[x / 8] >> (7 - (x % 8))) & 1;

        uint8_t* p = dstRow + (SIZE_T)dx * 4;
        if (andBit && !xorBit) {
          // no change
        } else if (!andBit && !xorBit) {
          p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 0xFF;
        } else if (andBit && xorBit) {
          p[0] = 0xFF; p[1] = 0xFF; p[2] = 0xFF; p[3] = 0xFF;
        } else { // !andBit && xorBit -> invert
          p[0] = 0xFF - p[0];
          p[1] = 0xFF - p[1];
          p[2] = 0xFF - p[2];
        }
      }
    }
    return;
  }

  // COLOR or MASKED_COLOR
  for (UINT y = 0; y < h; y++) {
    int dy = y0 + (int)y;
    if (dy < 0 || dy >= (int)dstH) continue;

    const BYTE* srcRow = shape + (SIZE_T)y * pitch;
    uint8_t* dstRow = dst + (SIZE_T)dy * dstStride;

    for (UINT x = 0; x < w; x++) {
      int dx = x0 + (int)x;
      if (dx < 0 || dx >= (int)dstW) continue;

      const BYTE* s = srcRow + (SIZE_T)x * 4;
      uint8_t* d = dstRow + (SIZE_T)dx * 4;

      const uint8_t a = s[3];
      if (info->Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR) {
        if (a == 0) {
          d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255;
        } else if (a == 255) {
          d[0] = 0xFF - d[0];
          d[1] = 0xFF - d[1];
          d[2] = 0xFF - d[2];
        } else {
          d[0] = (uint8_t)((s[0] * (255 - a) + d[0] * a) / 255);
          d[1] = (uint8_t)((s[1] * (255 - a) + d[1] * a) / 255);
          d[2] = (uint8_t)((s[2] * (255 - a) + d[2] * a) / 255);
          d[3] = 255;
        }
      } else {
        if (a == 0) {
          continue;
        } else if (a == 255) {
          d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255;
        } else {
          d[0] = (uint8_t)((s[0] * a + d[0] * (255 - a)) / 255);
          d[1] = (uint8_t)((s[1] * a + d[1] * (255 - a)) / 255);
          d[2] = (uint8_t)((s[2] * a + d[2] * (255 - a)) / 255);
          d[3] = 255;
        }
      }
    }
  }
}

typedef struct HttpCtx {
  SharedFrame* shared;
  UINT32 w;
  UINT32 h;
  int port;
  SRWLOCK lock;
  RECT desktopRect;
} HttpCtx;

static void overlay_cursor_gdi(HDC memdc, UINT32 w, UINT32 h, RECT desktopRect) {
  CURSORINFO ci;
  ICONINFO ii;
  int x, y;

  ci.cbSize = sizeof(ci);
  if (!GetCursorInfo(&ci)) return;
  if (!(ci.flags & CURSOR_SHOWING)) return;

  if (!GetIconInfo(ci.hCursor, &ii)) return;

  x = (int)ci.ptScreenPos.x - desktopRect.left - (int)ii.xHotspot;
  y = (int)ci.ptScreenPos.y - desktopRect.top - (int)ii.yHotspot;

  if (ii.hbmMask) DeleteObject(ii.hbmMask);
  if (ii.hbmColor) DeleteObject(ii.hbmColor);

  if (x >= (int)w || y >= (int)h || x <= -(int)w || y <= -(int)h) return;

  DrawIconEx(memdc, x, y, ci.hCursor, 0, 0, 0, NULL, DI_NORMAL);
}

static void blit_bgra(uint8_t* dst, uint32_t dstStride, const uint8_t* src, uint32_t srcStride,
                      uint32_t w, uint32_t h, BOOL rotate180, BOOL mirror) {
  for (uint32_t y = 0; y < h; y++) {
    const uint32_t srcY = rotate180 ? (h - 1 - y) : y;
    const uint8_t* srcRow = src + (SIZE_T)srcY * srcStride;
    uint8_t* dstRow = dst + (SIZE_T)y * dstStride;

    for (uint32_t x = 0; x < w; x++) {
      uint32_t srcX = rotate180 ? (w - 1 - x) : x;
      if (mirror) {
        srcX = (w - 1 - srcX);
      }

      const uint8_t* p = srcRow + (SIZE_T)srcX * 4;
      uint8_t* q = dstRow + (SIZE_T)x * 4;
      q[0] = p[0];
      q[1] = p[1];
      q[2] = p[2];
      q[3] = p[3];
    }
  }
}


static void print_usage(void) {
  wprintf(L"Usage: Stream.exe [-m N|--monitor N] [--port N] [--logs-enable] [--no-detach]\n");
  wprintf(L"  -m, --monitor N   Select monitor/output index (default 0)\n");
  wprintf(L"  --port N          HTTP listen port on 127.0.0.1 (default 40484)\n");
  wprintf(L"  --logs-enable     Enable stream.log diagnostics\n");
  wprintf(L"  --no-detach       Keep console attached (default: detach)\n");
}

static int relaunch_detached_if_needed(int argc, wchar_t** argv) {
  BOOL noDetach = FALSE;
  for (int i = 1; i < argc; i++) {
    if (wcscmp(argv[i], L"--no-detach") == 0) {
      noDetach = TRUE;
      break;
    }
  }

  if (noDetach) return 0;

  if (GetConsoleWindow() == NULL) return 0;

  // build new command line: "<exe>" <args...> --no-detach
  wchar_t exePath[MAX_PATH];
  DWORD n = GetModuleFileNameW(NULL, exePath, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return 0;

  size_t total = wcslen(exePath) + 32;
  for (int i = 1; i < argc; i++) total += wcslen(argv[i]) + 3;

  wchar_t* cmd = (wchar_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, total * sizeof(wchar_t));
  if (!cmd) return 0;

  wcscat_s(cmd, total, L"\"");
  wcscat_s(cmd, total, exePath);
  wcscat_s(cmd, total, L"\"");
  for (int i = 1; i < argc; i++) {
    wcscat_s(cmd, total, L" ");
    if (wcschr(argv[i], L' ') || wcschr(argv[i], L'\t')) {
      wcscat_s(cmd, total, L"\"");
      wcscat_s(cmd, total, argv[i]);
      wcscat_s(cmd, total, L"\"");
    } else {
      wcscat_s(cmd, total, argv[i]);
    }
  }
  wcscat_s(cmd, total, L" --no-detach");

  STARTUPINFOW si;
  PROCESS_INFORMATION pi;
  ZeroMemory(&si, sizeof(si));
  ZeroMemory(&pi, sizeof(pi));
  si.cb = sizeof(si);

  if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE,
                     CREATE_NO_WINDOW | DETACHED_PROCESS,
                     NULL, NULL, &si, &pi)) {
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    HeapFree(GetProcessHeap(), 0, cmd);
    return 1;
  }

  HeapFree(GetProcessHeap(), 0, cmd);
  return 0;
}

static int parse_query_int(const char* req, const char* key, int defVal) {
  const char* p = strstr(req, key);
  if (!p) return defVal;
  p += strlen(key);
  int sign = 1;
  if (*p == '-') { sign = -1; p++; }
  int v = 0;
  int any = 0;
  while (*p >= '0' && *p <= '9') {
    v = v * 10 + (*p - '0');
    p++;
    any = 1;
  }
  return any ? (v * sign) : defVal;
}

static int send_all(SOCKET s, const char* buf, int len) {
  int sent = 0;
  while (sent < len) {
    int n = send(s, buf + sent, len - sent, 0);
    if (n <= 0) return 0;
    sent += n;
  }
  return 1;
}

static HRESULT encode_jpeg(IWICImagingFactory* factory, const uint8_t* bgra, UINT w, UINT h, UINT stride,
                            int quality, BYTE** outData, DWORD* outSize) {
  IWICBitmap* bitmap = NULL;
  IWICFormatConverter* converter = NULL;
  IWICBitmapEncoder* encoder = NULL;
  IWICBitmapFrameEncode* frame = NULL;
  IPropertyBag2* props = NULL;
  IStream* stream = NULL;
  HRESULT hr = S_OK;

  *outData = NULL;
  *outSize = 0;

  HR(IWICImagingFactory_CreateBitmapFromMemory(factory, w, h, &GUID_WICPixelFormat32bppBGRA,
                                               stride, stride * h, (BYTE*)bgra, &bitmap));

  HR(IWICImagingFactory_CreateFormatConverter(factory, &converter));
  HR(IWICFormatConverter_Initialize(converter, (IWICBitmapSource*)bitmap, &GUID_WICPixelFormat24bppBGR,
                                    WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom));

  HR(CreateStreamOnHGlobal(NULL, TRUE, &stream));
  HR(IWICImagingFactory_CreateEncoder(factory, &GUID_ContainerFormatJpeg, NULL, &encoder));
  HR(IWICBitmapEncoder_Initialize(encoder, stream, WICBitmapEncoderNoCache));
  HR(IWICBitmapEncoder_CreateNewFrame(encoder, &frame, &props));

  if (props) {
    PROPBAG2 option = {0};
    VARIANT var = {0};
    option.pstrName = L"ImageQuality";
    VariantInit(&var);
    var.vt = VT_R4;
    var.fltVal = (quality < 1) ? 0.01f : (quality > 100) ? 1.0f : (float)quality / 100.0f;
    IPropertyBag2_Write(props, 1, &option, &var);
    VariantClear(&var);
  }

  HR(IWICBitmapFrameEncode_Initialize(frame, props));
  HR(IWICBitmapFrameEncode_SetSize(frame, w, h));

  {
    WICPixelFormatGUID pf = GUID_WICPixelFormat24bppBGR;
    HR(IWICBitmapFrameEncode_SetPixelFormat(frame, &pf));
  }

  HR(IWICBitmapFrameEncode_WriteSource(frame, (IWICBitmapSource*)converter, NULL));
  HR(IWICBitmapFrameEncode_Commit(frame));
  HR(IWICBitmapEncoder_Commit(encoder));

  {
    HGLOBAL hg = NULL;
    STATSTG st = {0};
    HR(IStream_Stat(stream, &st, STATFLAG_NONAME));
    HR(GetHGlobalFromStream(stream, &hg));
    *outSize = (DWORD)st.cbSize.QuadPart;
    *outData = (BYTE*)HeapAlloc(GetProcessHeap(), 0, *outSize);
    if (!*outData) { hr = E_OUTOFMEMORY; goto cleanup; }
    void* p = GlobalLock(hg);
    if (!p) { hr = E_FAIL; goto cleanup; }
    memcpy(*outData, p, *outSize);
    GlobalUnlock(hg);
  }

cleanup:
  if (frame) IWICBitmapFrameEncode_Release(frame);
  if (props) IPropertyBag2_Release(props);
  if (encoder) IWICBitmapEncoder_Release(encoder);
  if (stream) IStream_Release(stream);
  if (converter) IWICFormatConverter_Release(converter);
  if (bitmap) IWICBitmap_Release(bitmap);
  return hr;
}

static void handle_http_client(SOCKET client, HttpCtx* ctx) {
  char req[1024] = {0};
  int n = recv(client, req, sizeof(req) - 1, 0);
  if (n <= 0) return;

  if (strncmp(req, "GET /terminate.js", 17) == 0) {
    InterlockedExchange(&g_stop, 1);
    const char* body = "window.__terminateAck && window.__terminateAck(true);\n";
    char hdr[256];
    int hdrLen = _snprintf_s(hdr, sizeof(hdr), _TRUNCATE,
                             "HTTP/1.1 200 OK\r\nContent-Type: application/javascript\r\nContent-Length: %d\r\n\r\n",
                             (int)strlen(body));
    send_all(client, hdr, hdrLen);
    send_all(client, body, (int)strlen(body));
  } else if (strncmp(req, "GET /terminate", 14) == 0) {
    InterlockedExchange(&g_stop, 1);
    const char* body = "OK\n";
    char hdr[256];
    int hdrLen = _snprintf_s(hdr, sizeof(hdr), _TRUNCATE,
                             "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n",
                             (int)strlen(body));
    send_all(client, hdr, hdrLen);
    send_all(client, body, (int)strlen(body));
  } else if (strncmp(req, "GET /screens.js", 15) == 0) {
    LONG count = InterlockedCompareExchange(&g_screen_count, 0, 0);
    char body[128];
    int bodyLen = _snprintf_s(body, sizeof(body), _TRUNCATE,
                              "window.__screens && window.__screens({count:%ld});\n",
                              count);
    char hdr[256];
    int hdrLen = _snprintf_s(hdr, sizeof(hdr), _TRUNCATE,
                             "HTTP/1.1 200 OK\r\nContent-Type: application/javascript\r\nContent-Length: %d\r\n\r\n",
                             bodyLen);
    send_all(client, hdr, hdrLen);
    send_all(client, body, bodyLen);
  } else if (strncmp(req, "GET /switch.js", 14) == 0) {
    int idx = parse_query_int(req, "screen=", -1);
    if (idx >= 0) InterlockedExchange(&g_pending_screen, idx);
    const char* body = "window.__switchAck && window.__switchAck(true);\n";
    char hdr[256];
    int hdrLen = _snprintf_s(hdr, sizeof(hdr), _TRUNCATE,
                             "HTTP/1.1 200 OK\r\nContent-Type: application/javascript\r\nContent-Length: %d\r\n\r\n",
                             (int)strlen(body));
    send_all(client, hdr, hdrLen);
    send_all(client, body, (int)strlen(body));
  } else if (strncmp(req, "GET /switch", 11) == 0) {
    int idx = parse_query_int(req, "screen=", -1);
    if (idx >= 0) InterlockedExchange(&g_pending_screen, idx);
    const char* body = "OK\n";
    char hdr[256];
    int hdrLen = _snprintf_s(hdr, sizeof(hdr), _TRUNCATE,
                             "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n",
                             (int)strlen(body));
    send_all(client, hdr, hdrLen);
    send_all(client, body, (int)strlen(body));
  } else if (strncmp(req, "GET /stream", 11) == 0) {
    IWICImagingFactory* factory = NULL;
    HDC memdc = NULL;
    HBITMAP dib = NULL;
    uint8_t* dibBits = NULL;
    UINT32 curW = 0, curH = 0;

    if (FAILED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IWICImagingFactory, (void**)&factory))) {
      return;
    }

    {
      BITMAPINFO bmi;
      ZeroMemory(&bmi, sizeof(bmi));
      bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bmi.bmiHeader.biWidth = (LONG)ctx->w;
      bmi.bmiHeader.biHeight = -(LONG)ctx->h; // top-down
      bmi.bmiHeader.biPlanes = 1;
      bmi.bmiHeader.biBitCount = 32;
      bmi.bmiHeader.biCompression = BI_RGB;

      memdc = CreateCompatibleDC(NULL);
      if (memdc) {
        dib = CreateDIBSection(memdc, &bmi, DIB_RGB_COLORS, (void**)&dibBits, NULL, 0);
        if (dib) {
          SelectObject(memdc, dib);
        } else {
          DeleteDC(memdc);
          memdc = NULL;
        }
      }
    }


    const char* hdr =
      "HTTP/1.1 200 OK\r\n"
      "Connection: close\r\n"
      "Cache-Control: no-cache\r\n"
      "Pragma: no-cache\r\n"
      "Access-Control-Allow-Origin: *\r\n"
      "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
    if (!send_all(client, hdr, (int)strlen(hdr))) goto stream_cleanup;

    while (InterlockedCompareExchange(&g_stop, 0, 0) == 0) {
      UINT32 w = 0, h = 0;
      RECT desktopRect;
      int fps = 30;

      AcquireSRWLockShared(&ctx->lock);
      w = ctx->w;
      h = ctx->h;
      desktopRect = ctx->desktopRect;
      ReleaseSRWLockShared(&ctx->lock);

      if (w == 0 || h == 0) { Sleep(5); continue; }

      if (w != curW || h != curH) {
        if (dib) { DeleteObject(dib); dib = NULL; dibBits = NULL; }
        if (memdc) { DeleteDC(memdc); memdc = NULL; }

        BITMAPINFO bmi;
        ZeroMemory(&bmi, sizeof(bmi));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = (LONG)w;
        bmi.bmiHeader.biHeight = -(LONG)h; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        memdc = CreateCompatibleDC(NULL);
        if (memdc) {
          dib = CreateDIBSection(memdc, &bmi, DIB_RGB_COLORS, (void**)&dibBits, NULL, 0);
          if (dib) {
            SelectObject(memdc, dib);
            curW = w; curH = h;
          } else {
            DeleteDC(memdc);
            memdc = NULL;
          }
        }
      }
      uint8_t* snapshot = NULL;
      uint32_t stride = 0;

      AcquireSRWLockShared(&ctx->shared->lock);
      if (ctx->shared->bgra) {
        stride = ctx->shared->stride;
        snapshot = (uint8_t*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)stride * h);
        if (snapshot) memcpy(snapshot, ctx->shared->bgra, (SIZE_T)stride * h);
      }
      ReleaseSRWLockShared(&ctx->shared->lock);

      if (!snapshot) { Sleep(5); continue; }

      const uint32_t outStride = w * 4;
      uint8_t* temp = dibBits;
      if (!temp) {
        temp = (uint8_t*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)outStride * h);
        if (!temp) { HeapFree(GetProcessHeap(), 0, snapshot); break; }
      }

      blit_bgra(temp, outStride, snapshot, stride, w, h, FALSE, FALSE);

      if (memdc && dibBits) {
        overlay_cursor_gdi(memdc, w, h, desktopRect);
      }

      BYTE* jpg = NULL;
      DWORD jpgSize = 0;
      if (SUCCEEDED(encode_jpeg(factory, temp, w, h, outStride, 75, &jpg, &jpgSize)) && jpg) {
        char partHdr[256];
        int hdrLen = _snprintf_s(partHdr, sizeof(partHdr), _TRUNCATE,
                                 "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %lu\r\n\r\n",
                                 (unsigned long)jpgSize);
        if (!send_all(client, partHdr, hdrLen) ||
            !send_all(client, (const char*)jpg, (int)jpgSize) ||
            !send_all(client, "\r\n", 2)) {
          HeapFree(GetProcessHeap(), 0, jpg);
          HeapFree(GetProcessHeap(), 0, temp);
          HeapFree(GetProcessHeap(), 0, snapshot);
          goto stream_cleanup;
        }
        HeapFree(GetProcessHeap(), 0, jpg);
      }

        if (!dibBits) {
        HeapFree(GetProcessHeap(), 0, temp);
      }
      HeapFree(GetProcessHeap(), 0, snapshot);

      if (fps <= 0) fps = 30;
      Sleep(1000 / fps);
    }

  stream_cleanup:
    if (dib) DeleteObject(dib);
    if (memdc) DeleteDC(memdc);
    if (factory) IWICImagingFactory_Release(factory);
  } else {
    const char* html =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "Access-Control-Allow-Origin: *\r\n\r\n"
      "<!doctype html><html><head><meta charset='utf-8'><title>Stream</title></head>"
      "<body style='margin:0;background:#000;display:flex;align-items:center;justify-content:center;'>"
      "<img src='/stream' style='max-width:100vw;max-height:100vh;'/>"
      "</body></html>";
    send_all(client, html, (int)strlen(html));
  }
}

typedef struct ClientCtx {
  SOCKET client;
  HttpCtx* http;
} ClientCtx;

static DWORD WINAPI http_client_thread(LPVOID p) {
  ClientCtx* cc = (ClientCtx*)p;
  if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) {
    shutdown(cc->client, SD_BOTH);
    closesocket(cc->client);
    HeapFree(GetProcessHeap(), 0, cc);
    return 1;
  }

  __try {
    handle_http_client(cc->client, cc->http);
  } __except (log_exception(GetExceptionInformation(), "http_client")) {
  }
  shutdown(cc->client, SD_BOTH);
  closesocket(cc->client);
  CoUninitialize();
  HeapFree(GetProcessHeap(), 0, cc);
  return 0;
}

static DWORD WINAPI http_thread(LPVOID p) {
  HttpCtx* ctx = (HttpCtx*)p;
  WSADATA wsa;
  SOCKET listenSock = INVALID_SOCKET;

  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;
  if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) { WSACleanup(); return 1; }

  listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listenSock == INVALID_SOCKET) goto done;

  {
    SOCKADDR_IN addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((u_short)ctx->port);
    if (bind(listenSock, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR) goto done;
    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) goto done;
  }

  (void)ctx->port;

  while (InterlockedCompareExchange(&g_stop, 0, 0) == 0) {
    fd_set set;
    TIMEVAL tv;
    FD_ZERO(&set);
    FD_SET(listenSock, &set);
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    int r = select(0, &set, NULL, NULL, &tv);
    if (r > 0 && FD_ISSET(listenSock, &set)) {
      SOCKET client = accept(listenSock, NULL, NULL);
      if (client != INVALID_SOCKET) {
        ClientCtx* cc = (ClientCtx*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ClientCtx));
        if (cc) {
          cc->client = client;
          cc->http = ctx;
          HANDLE th = CreateThread(NULL, 0, http_client_thread, cc, 0, NULL);
          if (th) CloseHandle(th);
        } else {
          shutdown(client, SD_BOTH);
          closesocket(client);
        }
      }
    }
  }

done:
  if (listenSock != INVALID_SOCKET) closesocket(listenSock);
  CoUninitialize();
  WSACleanup();
  return 0;
}

static DWORD WINAPI capture_thread(LPVOID p) {
  CapCtx* c = (CapCtx*)p;
  __try {
    while (InterlockedCompareExchange(&g_stop, 0, 0) == 0 &&
           InterlockedCompareExchange(&g_capture_stop, 0, 0) == 0) {
      IDXGIResource* res = NULL;
      ID3D11Texture2D* frame = NULL;
      DXGI_OUTDUPL_FRAME_INFO info;
      ZeroMemory(&info, sizeof(info));

      HRESULT hr = IDXGIOutputDuplication_AcquireNextFrame(c->dupl, 100, &info, &res);
      if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        continue;
      }

      if (hr == DXGI_ERROR_ACCESS_LOST) {
        fprintf(stderr, "\nDXGI_ERROR_ACCESS_LOST\n");
        break;
      }
      if (FAILED(hr)) {
        fprintf(stderr, "\nAcquireNextFrame failed 0x%08X\n", (unsigned)hr);
        break;
      }

    hr = IDXGIResource_QueryInterface(res, &IID_ID3D11Texture2D, (void**)&frame);
    if (FAILED(hr)) {
      fprintf(stderr, "\nQueryInterface(Texture2D) failed 0x%08X\n", (unsigned)hr);
      IDXGIOutputDuplication_ReleaseFrame(c->dupl);
      if (res) IDXGIResource_Release(res);
      continue;
    }

    // update pointer shape/position
    if (info.PointerShapeBufferSize > 0) {
      if (c->pointerShapeSize < info.PointerShapeBufferSize) {
        BYTE* newBuf = (BYTE*)HeapReAlloc(GetProcessHeap(), 0, c->pointerShape, info.PointerShapeBufferSize);
        if (newBuf) {
          c->pointerShape = newBuf;
          c->pointerShapeSize = info.PointerShapeBufferSize;
        }
      }

      if (c->pointerShape && c->pointerShapeSize >= info.PointerShapeBufferSize) {
        UINT bufSize = 0;
        hr = IDXGIOutputDuplication_GetFramePointerShape(c->dupl, c->pointerShapeSize,
                                                         c->pointerShape, &bufSize, &c->pointerInfo);
        if (FAILED(hr) && bufSize > c->pointerShapeSize) {
          BYTE* newBuf = (BYTE*)HeapReAlloc(GetProcessHeap(), 0, c->pointerShape, bufSize);
          if (newBuf) {
            c->pointerShape = newBuf;
            c->pointerShapeSize = bufSize;
            hr = IDXGIOutputDuplication_GetFramePointerShape(c->dupl, c->pointerShapeSize,
                                                             c->pointerShape, &bufSize, &c->pointerInfo);
          }
        }
      }
    }

    // if shape never fetched yet, try to fetch it on first visible frame
    if (!c->pointerShape && info.PointerPosition.Visible) {
      UINT bufSize = 0;
      hr = IDXGIOutputDuplication_GetFramePointerShape(c->dupl, 0, NULL, &bufSize, &c->pointerInfo);
      if (hr == DXGI_ERROR_MORE_DATA && bufSize > 0) {
        BYTE* newBuf = (BYTE*)HeapAlloc(GetProcessHeap(), 0, bufSize);
        if (newBuf) {
          c->pointerShape = newBuf;
          c->pointerShapeSize = bufSize;
          hr = IDXGIOutputDuplication_GetFramePointerShape(c->dupl, c->pointerShapeSize,
                                                           c->pointerShape, &bufSize, &c->pointerInfo);
          if (FAILED(hr)) {
            HeapFree(GetProcessHeap(), 0, c->pointerShape);
            c->pointerShape = NULL;
            c->pointerShapeSize = 0;
          }
        }
      }
    }

    c->pointerVisible = info.PointerPosition.Visible;
    c->pointerPos = info.PointerPosition.Position;

    // GPU -> staging
    ID3D11DeviceContext_CopyResource(c->ctx, (ID3D11Resource*)c->staging, (ID3D11Resource*)frame);

    // staging -> CPU buffer
    D3D11_MAPPED_SUBRESOURCE map;
    hr = ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource*)c->staging, 0, D3D11_MAP_READ, 0, &map);
    if (SUCCEEDED(hr)) {
      AcquireSRWLockExclusive(&c->shared->lock);

      if (!c->shared->bgra) {
        // allocate once, using the mapped RowPitch
        c->shared->stride = (uint32_t)map.RowPitch;
        c->shared->bgra = (uint8_t*)HeapAlloc(GetProcessHeap(), 0,
                          (SIZE_T)c->shared->stride * c->shared->h);
      }

      if (c->shared->bgra) {
        uint8_t* dst = c->shared->bgra;
        const uint8_t* src = (const uint8_t*)map.pData;

        // copy row-by-row: keep destination stride = map.RowPitch
        for (uint32_t y = 0; y < c->shared->h; y++) {
          memcpy(dst + (SIZE_T)y * c->shared->stride,
                 src + (SIZE_T)y * map.RowPitch,
                 c->shared->stride);
        }

        overlay_cursor_bgra(dst, c->shared->stride, c->shared->w, c->shared->h,
          &c->pointerInfo, c->pointerShape, c->pointerPos, c->pointerVisible,
          c->desktopRect);
        c->shared->updates++;
      }

      ReleaseSRWLockExclusive(&c->shared->lock);
      ID3D11DeviceContext_Unmap(c->ctx, (ID3D11Resource*)c->staging, 0);
    }

    IDXGIOutputDuplication_ReleaseFrame(c->dupl);

      if (frame) ID3D11Texture2D_Release(frame);
      if (res) IDXGIResource_Release(res);
    }
  } __except (log_exception(GetExceptionInformation(), "capture")) {
  }

  return 0;
}

static HRESULT select_output_by_index(IDXGIFactory1* factory, int outputIndex,
                                      IDXGIAdapter1** outAdapter, IDXGIOutput** outOutput,
                                      RECT* outDesktopRect) {
  HRESULT hr = S_OK;
  IDXGIAdapter1* curAdapter = NULL;
  IDXGIOutput* curOutput = NULL;
  int curIndex = 0;

  *outAdapter = NULL;
  *outOutput = NULL;

  for (UINT ai = 0; ; ai++) {
    hr = IDXGIFactory1_EnumAdapters1(factory, ai, &curAdapter);
    if (hr == DXGI_ERROR_NOT_FOUND) break;
    HR(hr);

    for (UINT oi = 0; ; oi++) {
      hr = IDXGIAdapter1_EnumOutputs(curAdapter, oi, &curOutput);
      if (hr == DXGI_ERROR_NOT_FOUND) break;
      HR(hr);

      if (curIndex == outputIndex) {
        DXGI_OUTPUT_DESC outDesc;
        HR(IDXGIOutput_GetDesc(curOutput, &outDesc));
        *outAdapter = curAdapter;
        *outOutput = curOutput;
        *outDesktopRect = outDesc.DesktopCoordinates;
        return S_OK;
      }

      curIndex++;
      IDXGIOutput_Release(curOutput);
      curOutput = NULL;
    }

    IDXGIAdapter1_Release(curAdapter);
    curAdapter = NULL;
  }

cleanup:
  if (curOutput) IDXGIOutput_Release(curOutput);
  if (curAdapter) IDXGIAdapter1_Release(curAdapter);
  return DXGI_ERROR_NOT_FOUND;
}

static HRESULT init_capture(IDXGIFactory1* factory, int outputIndex, CapCtx* cap,
                            IDXGIAdapter1** adapter, IDXGIOutput** output0, IDXGIOutput1** output1,
                            ID3D11Device** dev, ID3D11DeviceContext** ctx,
                            IDXGIOutputDuplication** dupl, ID3D11Texture2D** staging,
                            UINT32* outW, UINT32* outH) {
  HRESULT hr = S_OK;
  DXGI_OUTDUPL_DESC duplDesc;

  HR(select_output_by_index(factory, outputIndex, adapter, output0, &cap->desktopRect));
  HR(IDXGIOutput_QueryInterface(*output0, &IID_IDXGIOutput1, (void**)output1));

  {
    D3D_FEATURE_LEVEL fl;
    HR(D3D11CreateDevice((IDXGIAdapter*)*adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL,
                         0, NULL, 0, D3D11_SDK_VERSION, dev, &fl, ctx));
  }

  HR(IDXGIOutput1_DuplicateOutput(*output1, (IUnknown*)*dev, dupl));
  IDXGIOutputDuplication_GetDesc(*dupl, &duplDesc);

  *outW = duplDesc.ModeDesc.Width;
  *outH = duplDesc.ModeDesc.Height;

  {
    D3D11_TEXTURE2D_DESC td;
    ZeroMemory(&td, sizeof(td));
    td.Width = *outW;
    td.Height = *outH;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_STAGING;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    HR(ID3D11Device_CreateTexture2D(*dev, &td, NULL, staging));
  }

  cap->dev = *dev;
  cap->ctx = *ctx;
  cap->dupl = *dupl;
  cap->staging = *staging;
  return S_OK;

cleanup:
  return hr;
}

static void release_capture(IDXGIAdapter1** adapter, IDXGIOutput** output0, IDXGIOutput1** output1,
                            ID3D11Device** dev, ID3D11DeviceContext** ctx,
                            IDXGIOutputDuplication** dupl, ID3D11Texture2D** staging) {
  if (*staging) { ID3D11Texture2D_Release(*staging); *staging = NULL; }
  if (*dupl) { IDXGIOutputDuplication_Release(*dupl); *dupl = NULL; }
  if (*ctx) { ID3D11DeviceContext_Release(*ctx); *ctx = NULL; }
  if (*dev) { ID3D11Device_Release(*dev); *dev = NULL; }
  if (*output1) { IDXGIOutput1_Release(*output1); *output1 = NULL; }
  if (*output0) { IDXGIOutput_Release(*output0); *output0 = NULL; }
  if (*adapter) { IDXGIAdapter1_Release(*adapter); *adapter = NULL; }
}

static BOOL switch_capture(int newIndex, IDXGIFactory1* factory, CapCtx* cap, SharedFrame* shared,
                           HttpCtx* http, HANDLE* th, IDXGIAdapter1** adapter, IDXGIOutput** output0,
                           IDXGIOutput1** output1, ID3D11Device** dev, ID3D11DeviceContext** ctx,
                           IDXGIOutputDuplication** dupl, ID3D11Texture2D** staging,
                           UINT32* W, UINT32* H) {
  InterlockedExchange(&g_capture_stop, 1);
  if (*th) { WaitForSingleObject(*th, INFINITE); CloseHandle(*th); *th = NULL; }
  InterlockedExchange(&g_capture_stop, 0);

  release_capture(adapter, output0, output1, dev, ctx, dupl, staging);

  if (SUCCEEDED(init_capture(factory, newIndex, cap, adapter, output0, output1, dev, ctx, dupl, staging, W, H))) {
    AcquireSRWLockExclusive(&shared->lock);
    if (shared->bgra) { HeapFree(GetProcessHeap(), 0, shared->bgra); shared->bgra = NULL; }
    shared->w = *W;
    shared->h = *H;
    shared->stride = (*W) * 4;
    shared->updates = 0;
    ReleaseSRWLockExclusive(&shared->lock);

    AcquireSRWLockExclusive(&http->lock);
    http->w = *W;
    http->h = *H;
    http->desktopRect = cap->desktopRect;
    ReleaseSRWLockExclusive(&http->lock);

    *th = CreateThread(NULL, 0, capture_thread, cap, 0, NULL);
    InterlockedExchange(&g_current_screen, newIndex);
    return TRUE;
  }

  fwprintf(stderr, L"Monitor index %d not found\n", newIndex);
  return FALSE;
}

static int count_outputs(IDXGIFactory1* factory) {
  int count = 0;
  IDXGIAdapter1* curAdapter = NULL;
  IDXGIOutput* curOutput = NULL;

  for (UINT ai = 0; ; ai++) {
    HRESULT hr = IDXGIFactory1_EnumAdapters1(factory, ai, &curAdapter);
    if (hr == DXGI_ERROR_NOT_FOUND) break;
    if (FAILED(hr)) break;

    for (UINT oi = 0; ; oi++) {
      hr = IDXGIAdapter1_EnumOutputs(curAdapter, oi, &curOutput);
      if (hr == DXGI_ERROR_NOT_FOUND) break;
      if (FAILED(hr)) break;
      count++;
      IDXGIOutput_Release(curOutput);
      curOutput = NULL;
    }

    IDXGIAdapter1_Release(curAdapter);
    curAdapter = NULL;
  }

  if (curOutput) IDXGIOutput_Release(curOutput);
  if (curAdapter) IDXGIAdapter1_Release(curAdapter);
  return count;
}

int wmain(int argc, wchar_t** argv) {
  HRESULT hr = S_OK;
  int outputIndex = 0;
  int port = 40484;
  const int fps = 30;

  if (relaunch_detached_if_needed(argc, argv)) {
    return 0;
  }

  for (int i = 1; i < argc; i++) {
    if (wcscmp(argv[i], L"-m") == 0 || wcscmp(argv[i], L"--monitor") == 0) {
      if (i + 1 >= argc) { print_usage(); return 1; }
      outputIndex = _wtoi(argv[++i]);
    } else if (wcscmp(argv[i], L"--port") == 0) {
      if (i + 1 >= argc) { print_usage(); return 1; }
      port = _wtoi(argv[++i]);
    } else if (wcscmp(argv[i], L"--logs-enable") == 0) {
      InterlockedExchange(&g_logs_enabled, 1);
    } else if (wcscmp(argv[i], L"--no-detach") == 0) {
      // handled in relaunch_detached_if_needed
    } else if (wcscmp(argv[i], L"-h") == 0 || wcscmp(argv[i], L"--help") == 0) {
      print_usage();
      return 0;
    }
  }

  if (outputIndex < 0) {
    fwprintf(stderr, L"Invalid monitor index: %d\n", outputIndex);
    return 1;
  }
  if (port <= 0 || port > 65535) {
    fwprintf(stderr, L"Invalid port: %d\n", port);
    return 1;
  }

  SetUnhandledExceptionFilter(on_unhandled_exception);
  log_line("Starting Stream.exe");
  SetConsoleCtrlHandler(on_ctrl, TRUE);

  // ---------- DXGI / D3D11 ----------
  IDXGIFactory1* factory = NULL;
  IDXGIAdapter1* adapter = NULL;
  IDXGIOutput* output0 = NULL;
  IDXGIOutput1* output1 = NULL;
  ID3D11Device* dev = NULL;
  ID3D11DeviceContext* ctx = NULL;
  IDXGIOutputDuplication* dupl = NULL;
  ID3D11Texture2D* staging = NULL;
  SharedFrame shared = {0};
  CapCtx cap = {0};
  HttpCtx http = {0};
  HANDLE th = NULL;
  HANDLE th_http = NULL;

  HR(CoInitializeEx(NULL, COINIT_MULTITHREADED));
  log_line("COM initialized");

  HR(CreateDXGIFactory1(&IID_IDXGIFactory1, (void**)&factory));
  log_line("DXGI factory created");
  InterlockedExchange(&g_screen_count, count_outputs(factory));

  UINT32 W = 0, H = 0;
  HR(init_capture(factory, outputIndex, &cap, &adapter, &output0, &output1, &dev, &ctx, &dupl, &staging, &W, &H));
  log_line("Capture initialized: screen=%d %ux%u", outputIndex, W, H);

  SetConsoleTitleW(L"Stream (Running)");
  ShowWindow(GetConsoleWindow(), SW_MINIMIZE);

  // shared frame
  InitializeSRWLock(&shared.lock);
  shared.bgra = NULL;
  shared.stride = W * 4;
  shared.w = W;
  shared.h = H;
  shared.updates = 0;

  // start capture thread
  cap.dev = dev;
  cap.ctx = ctx;
  cap.dupl = dupl;
  cap.staging = staging;
  cap.shared = &shared;
  cap.pointerShape = NULL;
  cap.pointerShapeSize = 0;
  ZeroMemory(&cap.pointerInfo, sizeof(cap.pointerInfo));
  cap.pointerPos.x = 0;
  cap.pointerPos.y = 0;
  cap.pointerVisible = FALSE;

  th = CreateThread(NULL, 0, capture_thread, &cap, 0, NULL);

  http.shared = &shared;
  http.w = W;
  http.h = H;
  http.port = port;
  http.desktopRect = cap.desktopRect;
  InitializeSRWLock(&http.lock);

  th_http = CreateThread(NULL, 0, http_thread, &http, 0, NULL);

  InterlockedExchange(&g_current_screen, outputIndex);

  while (InterlockedCompareExchange(&g_stop, 0, 0) == 0) {
    LONG pending = InterlockedExchange(&g_pending_screen, -1);
    if (pending >= 0 && pending != outputIndex) {
      if (switch_capture(pending, factory, &cap, &shared, &http, &th,
                         &adapter, &output0, &output1, &dev, &ctx, &dupl, &staging, &W, &H)) {
        outputIndex = pending;
      }
    }

    if (th_http) {
      DWORD r = WaitForSingleObject(th_http, 10);
      if (r == WAIT_OBJECT_0) {
        CloseHandle(th_http);
        th_http = NULL;
        th_http = CreateThread(NULL, 0, http_thread, &http, 0, NULL);
      }
    } else {
      Sleep(10);
    }
  }

  if (th_http) { WaitForSingleObject(th_http, INFINITE); CloseHandle(th_http); }


cleanup:
  if (th) { WaitForSingleObject(th, 1000); CloseHandle(th); }

  if (shared.bgra) HeapFree(GetProcessHeap(), 0, shared.bgra);
  if (cap.pointerShape) HeapFree(GetProcessHeap(), 0, cap.pointerShape);

  release_capture(&adapter, &output0, &output1, &dev, &ctx, &dupl, &staging);
  if (factory) IDXGIFactory1_Release(factory);

  CoUninitialize();
  return 0;
}
