#include "dpi.h"

#include <shellscalingapi.h>

#pragma comment(lib, "shcore.lib")

namespace dpi {
namespace {

using SetDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
using GetDpiForSystemFn = UINT(WINAPI*)();
using SystemParametersInfoForDpiFn = BOOL(WINAPI*)(UINT, UINT, PVOID, UINT, UINT);
using GetSystemMetricsForDpiFn = int(WINAPI*)(int, UINT);

HMODULE User32() {
  return GetModuleHandleW(L"user32.dll");
}

}  // namespace

bool EnablePerMonitorV2() {
  auto* setCtx = reinterpret_cast<SetDpiAwarenessContextFn>(
      GetProcAddress(User32(), "SetProcessDpiAwarenessContext"));
  if (setCtx && setCtx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
    return true;
  }
  const HRESULT hr = SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
  return SUCCEEDED(hr) || hr == E_ACCESSDENIED;
}

UINT ForWindow(HWND hwnd) {
  auto* fn = reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(User32(), "GetDpiForWindow"));
  if (fn && hwnd) {
    const UINT d = fn(hwnd);
    if (d) {
      return d;
    }
  }
  auto* sys = reinterpret_cast<GetDpiForSystemFn>(GetProcAddress(User32(), "GetDpiForSystem"));
  if (sys) {
    const UINT d = sys();
    if (d) {
      return d;
    }
  }
  HDC hdc = GetDC(nullptr);
  const UINT d = static_cast<UINT>(GetDeviceCaps(hdc, LOGPIXELSX));
  ReleaseDC(nullptr, hdc);
  return d ? d : 96;
}

UINT ForPoint(POINT pt) {
  HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
  UINT dpiX = 96;
  UINT dpiY = 96;
  if (SUCCEEDED(GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)) && dpiX) {
    return dpiX;
  }
  return ForWindow(nullptr);
}

int Scale(int value96, UINT dpi) {
  return MulDiv(value96, static_cast<int>(dpi), 96);
}

int TrayIconPx(UINT dpi) {
  auto* fn =
      reinterpret_cast<GetSystemMetricsForDpiFn>(GetProcAddress(User32(), "GetSystemMetricsForDpi"));
  if (fn) {
    const int cx = fn(SM_CXSMICON, dpi);
    if (cx > 0) {
      return cx;
    }
  }
  return Scale(16, dpi);
}

HFONT CreateUiFont(UINT dpi) {
  NONCLIENTMETRICSW ncm = {};
  ncm.cbSize = sizeof(ncm);

  auto* spiDpi = reinterpret_cast<SystemParametersInfoForDpiFn>(
      GetProcAddress(User32(), "SystemParametersInfoForDpi"));
  BOOL ok = FALSE;
  if (spiDpi) {
    ok = spiDpi(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0, dpi);
  }
  if (!ok) {
    ok = SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
  }
  if (ok) {
    return CreateFontIndirectW(&ncm.lfMessageFont);
  }

  const int px = -Scale(9, dpi);
  return CreateFontW(px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                     DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

}  // namespace dpi
