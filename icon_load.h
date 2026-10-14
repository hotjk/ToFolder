#pragma once

#include <windows.h>

// High-quality icon load sized for the current DPI / tray metrics.
HICON LoadAppIcon(HINSTANCE inst, int resId, int cx, int cy);

// Recolor near-black strokes to white (keeps alpha). Returns a new HICON; caller destroys.
// On failure returns nullptr (caller should keep the dark icon).
HICON RecolorIconForDarkTaskbar(HICON src);

int TrayIconSizePx(HWND hwnd);
