#pragma once

#include <windows.h>

namespace ui_util {

// Work area of the monitor under the mouse cursor.
RECT WorkAreaAtCursor();

// Dialogs are owned by a hidden 0x0 tray window, so center on the cursor's monitor instead.
void CenterOnCursorMonitor(HWND hwnd);

}  // namespace ui_util
