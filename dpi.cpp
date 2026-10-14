#include "dpi.h"

#include <shellscalingapi.h>

#include <cstdlib>

#pragma comment(lib, "shcore.lib")

namespace dpi {
namespace {

using SetDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
using GetDpiForSystemFn = UINT(WINAPI*)();
using SystemParametersInfoForDpiFn = BOOL(WINAPI*)(UINT, UINT, PVOID, UINT, UINT);
using GetSystemMetricsForDpiFn = int(WINAPI*)(int, UINT);

// Resolved once: these are called many times per layout pass.
template <typename Fn>
Fn User32Proc(const char* name) {
  return reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleW(L"user32.dll"), name));
}

}  // namespace

bool EnablePerMonitorV2() {
  static const auto setCtx = User32Proc<SetDpiAwarenessContextFn>("SetProcessDpiAwarenessContext");
  if (setCtx && setCtx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
    return true;
  }
  const HRESULT hr = SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
  return SUCCEEDED(hr) || hr == E_ACCESSDENIED;
}

UINT ForWindow(HWND hwnd) {
  static const auto forWindow = User32Proc<GetDpiForWindowFn>("GetDpiForWindow");
  static const auto forSystem = User32Proc<GetDpiForSystemFn>("GetDpiForSystem");
  if (forWindow && hwnd) {
    const UINT d = forWindow(hwnd);
    if (d) {
      return d;
    }
  }
  if (forSystem) {
    const UINT d = forSystem();
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
  static const auto metricsForDpi = User32Proc<GetSystemMetricsForDpiFn>("GetSystemMetricsForDpi");
  if (metricsForDpi) {
    const int cx = metricsForDpi(SM_CXSMICON, dpi);
    if (cx > 0) {
      return cx;
    }
  }
  return Scale(16, dpi);
}

HFONT CreateUiFont(UINT dpi, config::FontSize size) {
  // Relative to the system message font (Small = current default).
  constexpr int kPercent[] = {100, 125, 150};
  const int percent = kPercent[config::SizeIndex(size)];

  NONCLIENTMETRICSW ncm = {};
  ncm.cbSize = sizeof(ncm);

  static const auto spiForDpi =
      User32Proc<SystemParametersInfoForDpiFn>("SystemParametersInfoForDpi");
  BOOL ok = FALSE;
  if (spiForDpi) {
    ok = spiForDpi(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0, dpi);
  }
  if (!ok) {
    ok = SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
  }
  if (ok) {
    LOGFONTW lf = ncm.lfMessageFont;
    const int base = lf.lfHeight == 0 ? Scale(9, dpi) : abs(lf.lfHeight);
    const int scaled = MulDiv(base, percent, 100);
    lf.lfHeight = lf.lfHeight < 0 ? -scaled : scaled;
    return CreateFontIndirectW(&lf);
  }

  const int pt = MulDiv(9, percent, 100);
  const int px = -Scale(pt, dpi);
  return CreateFontW(px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                     DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

}  // namespace dpi
