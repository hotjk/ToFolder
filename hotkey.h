#pragma once

#include <windows.h>

#include <string>

namespace hotkey {

constexpr int kHotkeyId = 1;

struct Chord {
  UINT modifiers = 0;  // MOD_CONTROL | MOD_SHIFT | ...
  UINT vk = 0;
};

bool Parse(const std::wstring& text, Chord& out);
std::wstring Format(const Chord& chord);
Chord FromKeyMessage(WPARAM vk, LPARAM lParam);

void Unregister(HWND hwnd);
bool Apply(HWND hwnd, bool enabled, const Chord& chord);
bool Apply(HWND hwnd, bool enabled, const std::wstring& text);

// Modal settings dialog. Returns true if the user saved successfully.
bool ShowSettingsDialog(HWND owner, bool& enabled, std::wstring& text);

}  // namespace hotkey
