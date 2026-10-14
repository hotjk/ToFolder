#include "hotkey.h"

#include "lang.h"
#include "resource.h"
#include "theme.h"

#include <commctrl.h>

#include <string>
#include <vector>

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

namespace hotkey {
namespace {

bool EqualsIgnoreCase(const std::wstring& a, const wchar_t* b) {
  return _wcsicmp(a.c_str(), b) == 0;
}

bool IsModifierVk(UINT vk) {
  return vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_SHIFT ||
         vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
         vk == VK_LWIN || vk == VK_RWIN;
}

bool VkFromToken(const std::wstring& token, UINT& vk) {
  if (token.size() == 1) {
    const wchar_t ch = token[0];
    if (ch >= L'A' && ch <= L'Z') {
      vk = static_cast<UINT>(ch);
      return true;
    }
    if (ch >= L'a' && ch <= L'z') {
      vk = static_cast<UINT>(ch - L'a' + L'A');
      return true;
    }
    if (ch >= L'0' && ch <= L'9') {
      vk = static_cast<UINT>(ch);
      return true;
    }
  }
  if (EqualsIgnoreCase(token, L"Space")) {
    vk = VK_SPACE;
    return true;
  }
  if (EqualsIgnoreCase(token, L"Tab")) {
    vk = VK_TAB;
    return true;
  }
  if (EqualsIgnoreCase(token, L"Esc") || EqualsIgnoreCase(token, L"Escape")) {
    vk = VK_ESCAPE;
    return true;
  }
  if (EqualsIgnoreCase(token, L"Delete") || EqualsIgnoreCase(token, L"Del")) {
    vk = VK_DELETE;
    return true;
  }
  if (EqualsIgnoreCase(token, L"Insert") || EqualsIgnoreCase(token, L"Ins")) {
    vk = VK_INSERT;
    return true;
  }
  if (EqualsIgnoreCase(token, L"Home")) {
    vk = VK_HOME;
    return true;
  }
  if (EqualsIgnoreCase(token, L"End")) {
    vk = VK_END;
    return true;
  }
  if (EqualsIgnoreCase(token, L"PageUp") || EqualsIgnoreCase(token, L"PgUp")) {
    vk = VK_PRIOR;
    return true;
  }
  if (EqualsIgnoreCase(token, L"PageDown") || EqualsIgnoreCase(token, L"PgDn")) {
    vk = VK_NEXT;
    return true;
  }
  if (EqualsIgnoreCase(token, L"Left")) {
    vk = VK_LEFT;
    return true;
  }
  if (EqualsIgnoreCase(token, L"Right")) {
    vk = VK_RIGHT;
    return true;
  }
  if (EqualsIgnoreCase(token, L"Up")) {
    vk = VK_UP;
    return true;
  }
  if (EqualsIgnoreCase(token, L"Down")) {
    vk = VK_DOWN;
    return true;
  }
  if (token.size() >= 2 && (token[0] == L'F' || token[0] == L'f')) {
    bool digits = true;
    for (size_t i = 1; i < token.size(); ++i) {
      if (token[i] < L'0' || token[i] > L'9') {
        digits = false;
        break;
      }
    }
    if (digits) {
      const int n = _wtoi(token.c_str() + 1);
      if (n >= 1 && n <= 12) {
        vk = VK_F1 + static_cast<UINT>(n - 1);
        return true;
      }
    }
  }
  return false;
}

std::wstring TokenFromVk(UINT vk) {
  if (vk >= L'A' && vk <= L'Z') {
    return std::wstring(1, static_cast<wchar_t>(vk));
  }
  if (vk >= L'0' && vk <= L'9') {
    return std::wstring(1, static_cast<wchar_t>(vk));
  }
  if (vk >= VK_F1 && vk <= VK_F12) {
    return L"F" + std::to_wstring(vk - VK_F1 + 1);
  }
  switch (vk) {
    case VK_SPACE:
      return L"Space";
    case VK_TAB:
      return L"Tab";
    case VK_ESCAPE:
      return L"Esc";
    case VK_DELETE:
      return L"Delete";
    case VK_INSERT:
      return L"Insert";
    case VK_HOME:
      return L"Home";
    case VK_END:
      return L"End";
    case VK_PRIOR:
      return L"PageUp";
    case VK_NEXT:
      return L"PageDown";
    case VK_LEFT:
      return L"Left";
    case VK_RIGHT:
      return L"Right";
    case VK_UP:
      return L"Up";
    case VK_DOWN:
      return L"Down";
    default:
      return {};
  }
}

struct DialogState {
  HWND hotkeyHwnd = nullptr;
  bool enabled = false;
  Chord chord = {};
  Chord previousChord = {};
  bool previousEnabled = false;
};

DialogState* StateFromDlg(HWND dlg) {
  return reinterpret_cast<DialogState*>(GetWindowLongPtrW(dlg, DWLP_USER));
}

void UpdateHotkeyEdit(HWND dlg, const Chord& chord) {
  const std::wstring text = Format(chord);
  SetDlgItemTextW(dlg, IDC_HOTKEY_EDIT,
                   text.empty() ? lang::T(lang::Id::HotkeyPressShortcut) : text.c_str());
}

void LocalizeHotkeyDialog(HWND dlg) {
  SetWindowTextW(dlg, lang::T(lang::Id::HotkeyDialogTitle));
  SetDlgItemTextW(dlg, IDC_HOTKEY_ENABLE, lang::T(lang::Id::HotkeyEnable));
  SetDlgItemTextW(dlg, IDC_HOTKEY_LABEL, lang::T(lang::Id::HotkeyShortcutLabel));
  SetDlgItemTextW(dlg, IDC_HOTKEY_HINT, lang::T(lang::Id::HotkeyHint));
  SetDlgItemTextW(dlg, IDOK, lang::T(lang::Id::HotkeyOk));
  SetDlgItemTextW(dlg, IDCANCEL, lang::T(lang::Id::HotkeyCancel));
}

// Owner is a hidden 0x0 tray window — center on the monitor under the cursor.
void CenterDialog(HWND dlg) {
  RECT rc = {};
  GetWindowRect(dlg, &rc);
  const int w = rc.right - rc.left;
  const int h = rc.bottom - rc.top;

  POINT pt = {};
  GetCursorPos(&pt);
  HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  RECT wa = {};
  if (GetMonitorInfoW(mon, &mi)) {
    wa = mi.rcWork;
  } else {
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
  }

  const int x = wa.left + (wa.right - wa.left - w) / 2;
  const int y = wa.top + (wa.bottom - wa.top - h) / 2;
  SetWindowPos(dlg, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT CALLBACK HotkeyEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR /*id*/,
                                DWORD_PTR refData) {
  auto* state = reinterpret_cast<DialogState*>(refData);
  if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
    const UINT vk = static_cast<UINT>(wParam);
    if (vk == VK_ESCAPE) {
      SetFocus(GetDlgItem(GetParent(hwnd), IDOK));
      return 0;
    }
    if (!IsModifierVk(vk) && state) {
      const Chord chord = FromKeyMessage(wParam, lParam);
      if (chord.vk != 0) {
        state->chord = chord;
        UpdateHotkeyEdit(GetParent(hwnd), chord);
      }
    }
    return 0;
  }
  if (msg == WM_CHAR || msg == WM_SYSCHAR || msg == WM_KEYUP || msg == WM_SYSKEYUP) {
    return 0;
  }
  return DefSubclassProc(hwnd, msg, wParam, lParam);
}

INT_PTR CALLBACK HotkeyDlgProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_INITDIALOG: {
      auto* state = reinterpret_cast<DialogState*>(lParam);
      SetWindowLongPtrW(dlg, DWLP_USER, reinterpret_cast<LONG_PTR>(state));
      theme::AllowDarkForWindow(dlg, !theme::AppsUseLightTheme());
      LocalizeHotkeyDialog(dlg);
      CheckDlgButton(dlg, IDC_HOTKEY_ENABLE, state->enabled ? BST_CHECKED : BST_UNCHECKED);
      UpdateHotkeyEdit(dlg, state->chord);
      HWND edit = GetDlgItem(dlg, IDC_HOTKEY_EDIT);
      SetWindowSubclass(edit, HotkeyEditProc, 1, reinterpret_cast<DWORD_PTR>(state));
      CenterDialog(dlg);
      SetFocus(edit);
      return FALSE;
    }
    case WM_COMMAND: {
      const int id = LOWORD(wParam);
      if (id == IDOK) {
        auto* state = StateFromDlg(dlg);
        if (!state) {
          return TRUE;
        }
        const bool wantEnabled = IsDlgButtonChecked(dlg, IDC_HOTKEY_ENABLE) == BST_CHECKED;
        if (wantEnabled && state->chord.vk == 0) {
          MessageBoxW(dlg, lang::T(lang::Id::HotkeyNeedSet), lang::T(lang::Id::HotkeyDialogTitle),
                      MB_OK | MB_ICONWARNING);
          return TRUE;
        }
        if (!Apply(state->hotkeyHwnd, wantEnabled, state->chord)) {
          MessageBoxW(dlg, lang::T(lang::Id::HotkeyConflict), lang::T(lang::Id::HotkeyDialogTitle),
                      MB_OK | MB_ICONERROR);
          Apply(state->hotkeyHwnd, state->previousEnabled, state->previousChord);
          return TRUE;
        }
        state->enabled = wantEnabled;
        EndDialog(dlg, IDOK);
        return TRUE;
      }
      if (id == IDCANCEL) {
        EndDialog(dlg, IDCANCEL);
        return TRUE;
      }
      break;
    }
    case WM_DESTROY: {
      HWND edit = GetDlgItem(dlg, IDC_HOTKEY_EDIT);
      if (edit) {
        RemoveWindowSubclass(edit, HotkeyEditProc, 1);
      }
      break;
    }
  }
  return FALSE;
}

}  // namespace

bool Parse(const std::wstring& text, Chord& out) {
  out = {};
  std::wstring s = text;
  for (wchar_t& ch : s) {
    if (ch == L' ') {
      ch = L'+';
    }
  }

  std::vector<std::wstring> parts;
  size_t start = 0;
  while (start < s.size()) {
    size_t pos = s.find(L'+', start);
    if (pos == std::wstring::npos) {
      pos = s.size();
    }
    std::wstring part = s.substr(start, pos - start);
    while (!part.empty() && iswspace(part.front())) {
      part.erase(part.begin());
    }
    while (!part.empty() && iswspace(part.back())) {
      part.pop_back();
    }
    if (!part.empty()) {
      parts.push_back(part);
    }
    start = pos + 1;
  }

  if (parts.empty()) {
    return false;
  }

  UINT mods = 0;
  UINT vk = 0;
  for (const std::wstring& p : parts) {
    if (EqualsIgnoreCase(p, L"Ctrl") || EqualsIgnoreCase(p, L"Control")) {
      mods |= MOD_CONTROL;
      continue;
    }
    if (EqualsIgnoreCase(p, L"Alt")) {
      mods |= MOD_ALT;
      continue;
    }
    if (EqualsIgnoreCase(p, L"Shift")) {
      mods |= MOD_SHIFT;
      continue;
    }
    if (EqualsIgnoreCase(p, L"Win") || EqualsIgnoreCase(p, L"Windows")) {
      mods |= MOD_WIN;
      continue;
    }
    if (vk != 0) {
      return false;
    }
    if (!VkFromToken(p, vk)) {
      return false;
    }
  }

  if (vk == 0) {
    return false;
  }
  out.modifiers = mods;
  out.vk = vk;
  return true;
}

std::wstring Format(const Chord& chord) {
  if (chord.vk == 0) {
    return {};
  }
  std::wstring text;
  if (chord.modifiers & MOD_CONTROL) {
    text += L"Ctrl+";
  }
  if (chord.modifiers & MOD_ALT) {
    text += L"Alt+";
  }
  if (chord.modifiers & MOD_SHIFT) {
    text += L"Shift+";
  }
  if (chord.modifiers & MOD_WIN) {
    text += L"Win+";
  }
  const std::wstring key = TokenFromVk(chord.vk);
  if (key.empty()) {
    return {};
  }
  text += key;
  return text;
}

Chord FromKeyMessage(WPARAM vkWparam, LPARAM /*lParam*/) {
  Chord chord;
  const UINT vk = static_cast<UINT>(vkWparam);
  if (IsModifierVk(vk)) {
    return chord;
  }
  if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
    chord.modifiers |= MOD_CONTROL;
  }
  if ((GetKeyState(VK_MENU) & 0x8000) != 0) {
    chord.modifiers |= MOD_ALT;
  }
  if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) {
    chord.modifiers |= MOD_SHIFT;
  }
  if ((GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0) {
    chord.modifiers |= MOD_WIN;
  }
  chord.vk = vk;
  return chord;
}

void Unregister(HWND hwnd) {
  if (hwnd) {
    UnregisterHotKey(hwnd, kHotkeyId);
  }
}

bool Apply(HWND hwnd, bool enabled, const Chord& chord) {
  Unregister(hwnd);
  if (!enabled) {
    return true;
  }
  if (!hwnd || chord.vk == 0) {
    return false;
  }
  const UINT mods = chord.modifiers | MOD_NOREPEAT;
  return RegisterHotKey(hwnd, kHotkeyId, mods, chord.vk) != FALSE;
}

bool Apply(HWND hwnd, bool enabled, const std::wstring& text) {
  Chord chord;
  if (enabled) {
    if (!Parse(text, chord)) {
      return false;
    }
  }
  return Apply(hwnd, enabled, chord);
}

bool ShowSettingsDialog(HWND owner, bool& enabled, std::wstring& text) {
  DialogState state;
  state.hotkeyHwnd = owner;
  state.enabled = enabled;
  state.previousEnabled = enabled;
  if (!Parse(text, state.chord) || state.chord.vk == 0) {
    Parse(L"Ctrl+Shift+F", state.chord);
  }
  if (!Parse(text, state.previousChord)) {
    state.previousChord = state.chord;
  }

  const INT_PTR result = DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_HOTKEY),
                                         owner, HotkeyDlgProc, reinterpret_cast<LPARAM>(&state));
  if (result != IDOK) {
    return false;
  }

  enabled = state.enabled;
  text = Format(state.chord);
  if (text.empty()) {
    text = L"Ctrl+Shift+F";
  }
  return true;
}

}  // namespace hotkey
