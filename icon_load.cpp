#include "icon_load.h"

#include "dpi.h"

#include <commctrl.h>

HICON LoadAppIcon(HINSTANCE inst, int resId, int cx, int cy) {
  if (cx <= 0) {
    cx = 16;
  }
  if (cy <= 0) {
    cy = cx;
  }

  HICON icon = nullptr;
  // Prefer high-quality scale from the best matching image in the .ico group
  if (SUCCEEDED(LoadIconWithScaleDown(inst, MAKEINTRESOURCEW(resId), cx, cy, &icon)) && icon) {
    return icon;
  }

  icon = static_cast<HICON>(
      LoadImageW(inst, MAKEINTRESOURCEW(resId), IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR));
  if (icon) {
    return icon;
  }

  return static_cast<HICON>(
      LoadImageW(inst, MAKEINTRESOURCEW(resId), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR));
}

int TrayIconSizePx(HWND hwnd) {
  POINT pt = {};
  GetCursorPos(&pt);
  const UINT d = hwnd ? dpi::ForWindow(hwnd) : dpi::ForPoint(pt);
  return dpi::TrayIconPx(d);
}
