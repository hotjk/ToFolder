#pragma once

#include "config.h"

#include <windows.h>

namespace dpi {

bool EnablePerMonitorV2();
UINT ForWindow(HWND hwnd);
UINT ForPoint(POINT pt);
int Scale(int value96, UINT dpi);
int TrayIconPx(UINT dpi);
HFONT CreateUiFont(UINT dpi, config::FontSize size = config::FontSize::Small);

}  // namespace dpi
