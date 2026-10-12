#pragma once

#include <windows.h>

// High-quality icon load sized for the current DPI / tray metrics.
HICON LoadAppIcon(HINSTANCE inst, int resId, int cx, int cy);
int TrayIconSizePx(HWND hwnd);
