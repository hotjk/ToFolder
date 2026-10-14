#include "ui_util.h"

namespace ui_util {

RECT WorkAreaAtCursor() {
  POINT pt = {};
  GetCursorPos(&pt);
  HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  if (GetMonitorInfoW(mon, &mi)) {
    return mi.rcWork;
  }
  RECT wa = {};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
  return wa;
}

void CenterOnCursorMonitor(HWND hwnd) {
  RECT rc = {};
  GetWindowRect(hwnd, &rc);
  const int w = rc.right - rc.left;
  const int h = rc.bottom - rc.top;
  const RECT wa = WorkAreaAtCursor();
  const int x = wa.left + (wa.right - wa.left - w) / 2;
  const int y = wa.top + (wa.bottom - wa.top - h) / 2;
  SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

}  // namespace ui_util
