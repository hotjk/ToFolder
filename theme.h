#pragma once

#include <windows.h>

namespace theme {

// App chrome / menus follow AppsUseLightTheme.
bool AppsUseLightTheme();

// Taskbar / tray glyph contrast follows SystemUsesLightTheme.
bool SystemUsesLightTheme();

// Load uxtheme dark-mode helpers and prefer AllowDark. Safe no-op on failure.
void InitDarkModeSupport();

void AllowDarkForWindow(HWND hwnd, bool allow);
void FlushMenusForTheme();
// Re-read AppsUseLightTheme and force menu chrome before TrackPopupMenu.
void PreparePopupMenu(HWND hwnd);
void ApplyImmersiveDarkFrame(HWND hwnd, bool dark);

}  // namespace theme
