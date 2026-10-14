#include "slide_anim.h"

#include <dwmapi.h>

#include <cmath>

namespace slide_anim {
namespace {

constexpr double kSlideMs = 160.0;
constexpr DWORD kMoveFlags = SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER;

// Windows toast–like: decelerate into place / accelerate away — no overshoot.
double EaseOutCubic(double t) {
  const double u = 1.0 - t;
  return 1.0 - u * u * u;
}

double EaseInCubic(double t) {
  return t * t * t;
}

// Frame pacing via DwmFlush, but progress comes from elapsed time so the duration
// does not depend on the monitor refresh rate.
void RunSlide(HWND hwnd, int fromX, int toX, int y, double (*ease)(double)) {
  LARGE_INTEGER freq = {};
  LARGE_INTEGER start = {};
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&start);
  const double durationTicks = static_cast<double>(freq.QuadPart) * kSlideMs / 1000.0;

  int lastX = fromX;
  for (;;) {
    LARGE_INTEGER now = {};
    QueryPerformanceCounter(&now);
    double t = static_cast<double>(now.QuadPart - start.QuadPart) / durationTicks;
    if (t > 1.0) {
      t = 1.0;
    }
    const int x = fromX + static_cast<int>(std::lround((toX - fromX) * ease(t)));
    if (x != lastX) {
      SetWindowPos(hwnd, nullptr, x, y, 0, 0, kMoveFlags);
      lastX = x;
    }
    if (t >= 1.0) {
      break;
    }
    if (FAILED(DwmFlush())) {
      Sleep(1);
    }
  }
  DwmFlush();
}

void SetAlpha(HWND hwnd, BYTE alpha) {
  SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
}

}  // namespace

// Off-screen windows are not composed with content (only an empty DWM frame).
// Paint at the final rect while fully transparent, jump off-screen, then fade in
// and translate — size never changes.
void SlideIn(HWND hwnd, int fromX, int toX, int y) {
  const LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
  SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);
  SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
  // Alpha 0: on-screen paint without a visible flash (off-screen paint is skipped by DWM).
  SetAlpha(hwnd, 0);

  SetWindowPos(hwnd, nullptr, toX, y, 0, 0, kMoveFlags | SWP_SHOWWINDOW);
  RedrawWindow(hwnd, nullptr, nullptr,
               RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_ERASE);
  DwmFlush();

  // Content is in the redirection surface; jump off-screen while still invisible.
  SetWindowPos(hwnd, nullptr, fromX, y, 0, 0, kMoveFlags);
  SetAlpha(hwnd, 255);
  DwmFlush();

  RunSlide(hwnd, fromX, toX, y, EaseOutCubic);
}

void SlideOut(HWND hwnd, int fromX, int toX, int y) {
  SetWindowPos(hwnd, nullptr, fromX, y, 0, 0, kMoveFlags | SWP_SHOWWINDOW);
  RunSlide(hwnd, fromX, toX, y, EaseInCubic);
}

}  // namespace slide_anim
