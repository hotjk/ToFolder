#pragma once

#include <windows.h>

namespace dpi {

bool EnablePerMonitorV2();
UINT ForWindow(HWND hwnd);
UINT ForPoint(POINT pt);
int Scale(int value96, UINT dpi);
int TrayIconPx(UINT dpi);
HFONT CreateUiFont(UINT dpi);

}  // namespace dpi
