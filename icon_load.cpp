#include "icon_load.h"

#include "dpi.h"

#include <commctrl.h>

#include <cstring>
#include <vector>

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

HICON RecolorIconForDarkTaskbar(HICON src) {
  if (!src) {
    return nullptr;
  }

  ICONINFO ii = {};
  if (!GetIconInfo(src, &ii)) {
    return nullptr;
  }

  BITMAP bm = {};
  if (!ii.hbmColor || !GetObjectW(ii.hbmColor, sizeof(bm), &bm) || bm.bmBitsPixel != 32) {
    if (ii.hbmColor) {
      DeleteObject(ii.hbmColor);
    }
    if (ii.hbmMask) {
      DeleteObject(ii.hbmMask);
    }
    return nullptr;
  }

  const int width = bm.bmWidth;
  const int height = bm.bmHeight;
  const size_t bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;

  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width;
  bmi.bmiHeader.biHeight = -height;  // top-down
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  HDC hdc = GetDC(nullptr);
  std::vector<BYTE> pixels(bytes);
  const int got = GetDIBits(hdc, ii.hbmColor, 0, height, pixels.data(), &bmi, DIB_RGB_COLORS);
  if (got != height) {
    ReleaseDC(nullptr, hdc);
    DeleteObject(ii.hbmColor);
    if (ii.hbmMask) {
      DeleteObject(ii.hbmMask);
    }
    return nullptr;
  }

  for (size_t i = 0; i + 3 < bytes; i += 4) {
    const BYTE b = pixels[i];
    const BYTE g = pixels[i + 1];
    const BYTE r = pixels[i + 2];
    const BYTE a = pixels[i + 3];
    if (a == 0) {
      continue;
    }
    // Near-black glyph strokes → near-white; keep soft anti-alias by mapping luminance.
    const int y = (r * 299 + g * 587 + b * 114) / 1000;
    if (y < 200) {
      const BYTE v = static_cast<BYTE>(255 - y);
      pixels[i] = v;
      pixels[i + 1] = v;
      pixels[i + 2] = v;
    }
  }

  void* bits = nullptr;
  HBITMAP hbmColor = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  ReleaseDC(nullptr, hdc);
  if (!hbmColor || !bits) {
    if (hbmColor) {
      DeleteObject(hbmColor);
    }
    DeleteObject(ii.hbmColor);
    if (ii.hbmMask) {
      DeleteObject(ii.hbmMask);
    }
    return nullptr;
  }
  memcpy(bits, pixels.data(), bytes);

  // 32bpp icons: mask can be empty; create a 1bpp all-zero mask of the same size.
  HBITMAP hbmMask = CreateBitmap(width, height, 1, 1, nullptr);
  if (hbmMask) {
    HDC mem = CreateCompatibleDC(nullptr);
    HGDIOBJ old = SelectObject(mem, hbmMask);
    PatBlt(mem, 0, 0, width, height, BLACKNESS);
    SelectObject(mem, old);
    DeleteDC(mem);
  }

  DeleteObject(ii.hbmColor);
  if (ii.hbmMask) {
    DeleteObject(ii.hbmMask);
  }

  ICONINFO outIi = {};
  outIi.fIcon = TRUE;
  outIi.hbmMask = hbmMask;
  outIi.hbmColor = hbmColor;
  HICON result = CreateIconIndirect(&outIi);
  DeleteObject(hbmColor);
  if (hbmMask) {
    DeleteObject(hbmMask);
  }
  return result;
}

int TrayIconSizePx(HWND hwnd) {
  POINT pt = {};
  GetCursorPos(&pt);
  const UINT d = hwnd ? dpi::ForWindow(hwnd) : dpi::ForPoint(pt);
  return dpi::TrayIconPx(d);
}
