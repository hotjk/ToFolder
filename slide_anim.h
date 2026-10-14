#pragma once

#include <windows.h>

namespace slide_anim {

// Shows `hwnd` at (toX, y) by sliding it in horizontally from fromX (ease-out).
void SlideIn(HWND hwnd, int fromX, int toX, int y);

// Slides `hwnd` from fromX to toX (ease-in); the caller hides it afterwards.
void SlideOut(HWND hwnd, int fromX, int toX, int y);

}  // namespace slide_anim
