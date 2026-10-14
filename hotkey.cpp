#include "hotkey.h"

#include "config.h"
#include "lang.h"
#include "resource.h"
#include "str_util.h"
#include "theme.h"
#include "ui_util.h"
#include "win_compat.h"

#include <commctrl.h>

#include <string>
#include <vector>

namespace hotkey {
namespace {

struct NamedKey {
  const wchar_t* name;  // first entry per vk is the canonical name used by Format()
  UINT vk;
};

constexpr NamedKey kNamedKeys[] = {
    {L"Space", VK_SPACE},     {L"Tab", VK_TAB},          {L"Esc", VK_ESCAPE},
    {L"Escape", VK_ESCAPE},   {L"Delete", VK_DELETE},    {L"Del", VK_DELETE},
    {L"Insert", VK_INSERT},   {L"Ins", VK_INSERT},       {L"Home", VK_HOME},
    {L"End", VK_END},         {L"PageUp", VK_PRIOR},     {L"PgUp", VK_PRIOR},
    {L"PageDown", VK_NEXT},   {L"PgDn", VK_NEXT},        {L"Left", VK_LEFT},
    {L"Right", VK_RIGHT},     {L"Up", VK_UP},            {L"Down", VK_DOWN},
};

struct NamedModifier {
  const wchar_t* name;  // first entry per flag is canonical
  UINT flag;
};

// Order matters: Format() emits modifiers in this order.
constexpr NamedModifier kModifiers[] = {
    {L"Ctrl", MOD_CONTROL}, {L"Control", MOD_CONTROL}, {L"Alt", MOD_ALT},
    {L"Shift", MOD_SHIFT},  {L"Win", MOD_WIN},         {L"Windows", MOD_WIN},
};

bool IsModifierVk(UINT vk) {
  return vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_SHIFT ||
         vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
         vk == VK_LWIN || vk == VK_RWIN;
}

bool VkFromToken(const std::wstring& token, UINT& vk) {
  if (token.size() == 1) {
    const wchar_t ch = static_cast<wchar_t>(towupper(token[0]));
    if ((ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9')) {
      vk = static_cast<UINT>(ch);
      return true;
    }
  }
  for (const NamedKey& key : kNamedKeys) {
    if (str::EqualsNoCase(token, key.name)) {
      vk = key.vk;
      return true;
    }
  }
  if (token.size() >= 2 && towupper(token[0]) == L'F' &&
      token.find_first_not_of(L"0123456789", 1) == std::wstring::npos) {
    const int n = _wtoi(token.c_str() + 1);
    if (n >= 1 && n <= 12) {
      vk = VK_F1 + static_cast<UINT>(n - 1);
      return true;
    }
  }
  return false;
}

std::wstring TokenFromVk(UINT vk) {
  if ((vk >= L'A' && vk <= L'Z') || (vk >= L'0' && vk <= L'9')) {
    return std::wstring(1, static_cast<wchar_t>(vk));
  }
  if (vk >= VK_F1 && vk <= VK_F12) {
    return L"F" + std::to_wstring(vk - VK_F1 + 1);
  }
  for (const NamedKey& key : kNamedKeys) {
    if (key.vk == vk) {
      return key.name;
    }
  }
  return {};
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
      ui_util::CenterOnCursorMonitor(dlg);
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

  const std::vector<std::wstring> parts = str::SplitTrim(s, L'+');
  if (parts.empty()) {
    return false;
  }

  UINT mods = 0;
  UINT vk = 0;
  for (const std::wstring& p : parts) {
    bool isModifier = false;
    for (const NamedModifier& mod : kModifiers) {
      if (str::EqualsNoCase(p, mod.name)) {
        mods |= mod.flag;
        isModifier = true;
        break;
      }
    }
    if (isModifier) {
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
  UINT emitted = 0;
  for (const NamedModifier& mod : kModifiers) {
    if ((chord.modifiers & mod.flag) && !(emitted & mod.flag)) {
      text += mod.name;
      text += L'+';
      emitted |= mod.flag;
    }
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
    Parse(config::kDefaultHotkey, state.chord);
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
    text = config::kDefaultHotkey;
  }
  return true;
}

}  // namespace hotkey
