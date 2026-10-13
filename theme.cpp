#include "theme.h"

#include <dwmapi.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

namespace theme {
namespace {

enum class PreferredAppMode {
  Default = 0,
  AllowDark = 1,
  ForceDark = 2,
  ForceLight = 3,
  Max = 4,
};

using FnAllowDarkModeForWindow = BOOL(WINAPI*)(HWND, BOOL);
using FnSetPreferredAppMode = PreferredAppMode(WINAPI*)(PreferredAppMode);
using FnFlushMenuThemes = void(WINAPI*)();
using FnRefreshImmersiveColorPolicyState = void(WINAPI*)();

FnAllowDarkModeForWindow g_allowDarkModeForWindow = nullptr;
FnSetPreferredAppMode g_setPreferredAppMode = nullptr;
FnFlushMenuThemes g_flushMenuThemes = nullptr;
FnRefreshImmersiveColorPolicyState g_refreshImmersiveColorPolicyState = nullptr;
bool g_darkApisReady = false;

bool ReadPersonalizeDword(const wchar_t* valueName, DWORD defaultValue) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0,
                    KEY_READ, &key) != ERROR_SUCCESS) {
    return defaultValue != 0;
  }
  DWORD value = defaultValue;
  DWORD type = 0;
  DWORD size = sizeof(value);
  const LSTATUS st =
      RegQueryValueExW(key, valueName, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size);
  RegCloseKey(key);
  if (st != ERROR_SUCCESS || type != REG_DWORD) {
    return defaultValue != 0;
  }
  return value != 0;
}

}  // namespace

bool AppsUseLightTheme() {
  return ReadPersonalizeDword(L"AppsUseLightTheme", 1);
}

bool SystemUsesLightTheme() {
  return ReadPersonalizeDword(L"SystemUsesLightTheme", 1);
}

void InitDarkModeSupport() {
  if (g_darkApisReady) {
    return;
  }

  HMODULE uxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!uxtheme) {
    return;
  }

  // Ordinals used by Win10 1903+ / Win11 (undocumented but widely used).
  g_allowDarkModeForWindow =
      reinterpret_cast<FnAllowDarkModeForWindow>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(133)));
  g_setPreferredAppMode =
      reinterpret_cast<FnSetPreferredAppMode>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(135)));
  g_flushMenuThemes =
      reinterpret_cast<FnFlushMenuThemes>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(136)));
  g_refreshImmersiveColorPolicyState = reinterpret_cast<FnRefreshImmersiveColorPolicyState>(
      GetProcAddress(uxtheme, MAKEINTRESOURCEA(104)));

  if (g_setPreferredAppMode) {
    g_setPreferredAppMode(PreferredAppMode::AllowDark);
  }
  if (g_refreshImmersiveColorPolicyState) {
    g_refreshImmersiveColorPolicyState();
  }

  g_darkApisReady = g_allowDarkModeForWindow || g_setPreferredAppMode || g_flushMenuThemes;
}

void AllowDarkForWindow(HWND hwnd, bool allow) {
  if (!hwnd || !g_allowDarkModeForWindow) {
    return;
  }
  g_allowDarkModeForWindow(hwnd, allow ? TRUE : FALSE);
}

void FlushMenusForTheme() {
  if (g_flushMenuThemes) {
    g_flushMenuThemes();
  }
}

void PreparePopupMenu(HWND hwnd) {
  // AllowDark alone often keeps the first theme for the process lifetime; force
  // light/dark from the current AppsUseLightTheme each time a popup opens.
  const bool light = AppsUseLightTheme();
  if (g_refreshImmersiveColorPolicyState) {
    g_refreshImmersiveColorPolicyState();
  }
  if (g_setPreferredAppMode) {
    g_setPreferredAppMode(light ? PreferredAppMode::ForceLight : PreferredAppMode::ForceDark);
  }
  AllowDarkForWindow(hwnd, !light);
  FlushMenusForTheme();
}

void ApplyImmersiveDarkFrame(HWND hwnd, bool dark) {
  if (!hwnd) {
    return;
  }
  BOOL useDark = dark ? TRUE : FALSE;
  // Attribute 20 is current; 19 was used on some older Insider builds.
  if (FAILED(DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark)))) {
    DwmSetWindowAttribute(hwnd, 19, &useDark, sizeof(useDark));
  }
}

}  // namespace theme
