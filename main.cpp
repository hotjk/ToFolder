#include "app_messages.h"
#include "config.h"
#include "dpi.h"
#include "hotkey.h"
#include "icon_load.h"
#include "lang.h"
#include "panel.h"
#include "resource.h"
#include "theme.h"
#include "tray.h"
#include "watcher.h"

#include <commctrl.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <windows.h>

#include <string>

namespace {

constexpr wchar_t kGitHubRepoUrl[] = L"https://github.com/hotjk/ToFolder";

HWND g_hwnd = nullptr;
TrayIcon g_tray;
PanelWindow g_panel;
DirectoryWatcher g_watcher;
std::wstring g_watchPath;
config::SortMode g_sortMode = config::SortMode::ByTime;
bool g_showHidden = false;
bool g_expandFolders = false;
bool g_watchEnabled = true;
bool g_hotkeyEnabled = false;
std::wstring g_hotkeyText = L"Ctrl+Shift+F";
ULONGLONG g_panelHiddenAt = 0;

void UpdateTrayTip() {
  if (g_watchPath.empty()) {
    g_tray.SetTip(lang::T(lang::Id::TipNoFolder));
  } else {
    std::wstring tip = lang::T(lang::Id::TipPrefix);
    tip += g_watchPath;
    if (tip.size() > 120) {
      tip = tip.substr(0, 117) + L"...";
    }
    g_tray.SetTip(tip);
  }
}

void ApplyLanguage(lang::Language language, bool save) {
  lang::Set(language);
  if (save) {
    config::SaveLanguage(language);
  }
  UpdateTrayTip();
  if (g_panel.IsVisible()) {
    g_panel.Refresh();
  }
}

void OnDirChanged(bool hasAdded) {
  PostMessageW(g_hwnd, WM_DIR_CHANGED, hasAdded ? 1 : 0, 0);
}

bool ChooseFolder(HWND owner, std::wstring& outPath) {
  IFileDialog* dialog = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog));
  if (FAILED(hr)) {
    return false;
  }

  DWORD options = 0;
  dialog->GetOptions(&options);
  dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
  dialog->SetTitle(lang::T(lang::Id::SelectFolderTitle));

  hr = dialog->Show(owner);
  bool ok = false;
  if (SUCCEEDED(hr)) {
    IShellItem* item = nullptr;
    if (SUCCEEDED(dialog->GetResult(&item)) && item) {
      PWSTR path = nullptr;
      if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
        outPath = path;
        ok = true;
        CoTaskMemFree(path);
      }
      item->Release();
    }
  }
  dialog->Release();
  return ok;
}

void ApplyWatchPath(const std::wstring& path, bool save) {
  g_watchPath = path;
  if (save) {
    config::SaveWatchPath(g_watchPath);
  }
  g_tray.SetUnread(false);
  g_panel.SetWatchPath(g_watchPath);
  g_panel.SetSortMode(g_sortMode);
  g_panel.SetShowHidden(g_showHidden);
  g_panel.SetExpandFolders(g_expandFolders);
  UpdateTrayTip();
  g_watcher.Stop();
  if (g_watchEnabled && DirectoryWatcher::PathExists(g_watchPath)) {
    g_watcher.Start(g_watchPath);
  }
  if (g_panel.IsVisible()) {
    g_panel.Refresh();
  }
}

void ApplyWatchEnabled(bool enabled, bool save) {
  g_watchEnabled = enabled;
  if (save) {
    config::SaveWatchEnabled(g_watchEnabled);
  }
  g_watcher.Stop();
  if (g_watchEnabled && DirectoryWatcher::PathExists(g_watchPath)) {
    g_watcher.Start(g_watchPath);
  }
  if (!g_watchEnabled) {
    g_tray.SetUnread(false);
  }
}

void ApplySortMode(config::SortMode mode, bool save) {
  g_sortMode = mode;
  g_panel.SetSortMode(g_sortMode);
  if (save) {
    config::SaveSortMode(g_sortMode);
  }
  if (g_panel.IsVisible()) {
    g_panel.Refresh();
  }
}

void ApplyShowHidden(bool showHidden, bool save) {
  g_showHidden = showHidden;
  g_panel.SetShowHidden(g_showHidden);
  if (save) {
    config::SaveShowHidden(g_showHidden);
  }
  if (g_panel.IsVisible()) {
    g_panel.Refresh();
  }
}

void ApplyExpandFolders(bool expandFolders, bool save) {
  g_expandFolders = expandFolders;
  g_panel.SetExpandFolders(g_expandFolders);
  if (save) {
    config::SaveExpandFolders(g_expandFolders);
  }
  if (g_panel.IsVisible()) {
    g_panel.Refresh();
  }
}

bool HasWatchFolder() {
  return DirectoryWatcher::PathExists(g_watchPath);
}

void ShowPanel() {
  if (!HasWatchFolder()) {
    return;
  }
  g_tray.SetUnread(false);
  g_panel.Show();
  g_tray.SetPanelOpen(true);
}

void HidePanel() {
  if (!g_panel.IsVisible()) {
    return;
  }
  g_panel.Hide();
  g_tray.SetPanelOpen(false);
  g_panelHiddenAt = GetTickCount64();
}

constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValueName[] = L"ToFolder";

bool IsStartWithWindows() {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_READ, &key) != ERROR_SUCCESS) {
    return false;
  }
  wchar_t buf[MAX_PATH + 4] = {};
  DWORD type = 0;
  DWORD size = sizeof(buf);
  const LSTATUS st =
      RegQueryValueExW(key, kRunValueName, nullptr, &type, reinterpret_cast<LPBYTE>(buf), &size);
  RegCloseKey(key);
  return st == ERROR_SUCCESS && type == REG_SZ && buf[0] != L'\0';
}

bool SetStartWithWindows(bool enable) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
    return false;
  }
  bool ok = false;
  if (enable) {
    wchar_t exePath[MAX_PATH] = {};
    const DWORD n = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
      std::wstring value = L"\"";
      value += exePath;
      value += L"\"";
      ok = RegSetValueExW(key, kRunValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                          static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    }
  } else {
    const LSTATUS st = RegDeleteValueW(key, kRunValueName);
    ok = (st == ERROR_SUCCESS || st == ERROR_FILE_NOT_FOUND);
  }
  RegCloseKey(key);
  return ok;
}

void ShowContextMenu(HWND hwnd) {
  POINT pt = {};
  GetCursorPos(&pt);
  HMENU menu = CreatePopupMenu();
  AppendMenuW(menu, MF_STRING, IDM_OPEN_PANEL, lang::T(lang::Id::MenuOpenPanel));
  AppendMenuW(menu, MF_STRING, IDM_OPEN_FOLDER, lang::T(lang::Id::MenuOpenFolder));
  if (!HasWatchFolder()) {
    EnableMenuItem(menu, IDM_OPEN_PANEL, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(menu, IDM_OPEN_FOLDER, MF_BYCOMMAND | MF_GRAYED);
  }
  AppendMenuW(menu, MF_STRING, IDM_CHOOSE_FOLDER, lang::T(lang::Id::MenuChooseFolder));
  AppendMenuW(
      menu,
      MF_STRING | (g_watchEnabled ? MF_CHECKED : MF_UNCHECKED),
      IDM_WATCH_ENABLED,
      lang::T(lang::Id::MenuWatchNewFiles));
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(
      menu,
      MF_STRING | (g_sortMode == config::SortMode::ByTime ? MF_CHECKED : MF_UNCHECKED),
      IDM_SORT_TIME,
      lang::T(lang::Id::MenuSortByTime));
  AppendMenuW(
      menu,
      MF_STRING | (g_sortMode == config::SortMode::ByName ? MF_CHECKED : MF_UNCHECKED),
      IDM_SORT_NAME,
      lang::T(lang::Id::MenuSortByName));
  AppendMenuW(
      menu,
      MF_STRING | (g_showHidden ? MF_CHECKED : MF_UNCHECKED),
      IDM_SHOW_HIDDEN,
      lang::T(lang::Id::MenuShowHidden));
  AppendMenuW(
      menu,
      MF_STRING | (g_expandFolders ? MF_CHECKED : MF_UNCHECKED),
      IDM_EXPAND_FOLDERS,
      lang::T(lang::Id::MenuExpandFolders));
  AppendMenuW(
      menu,
      MF_STRING | (IsStartWithWindows() ? MF_CHECKED : MF_UNCHECKED),
      IDM_START_WITH_WINDOWS,
      lang::T(lang::Id::MenuStartWithWindows));
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

  HMENU langMenu = CreatePopupMenu();
  const bool langEn = lang::Current() == lang::Language::English;
  AppendMenuW(langMenu, MF_STRING | (langEn ? MF_CHECKED : MF_UNCHECKED), IDM_LANG_EN,
              lang::T(lang::Id::MenuLangEnglish));
  AppendMenuW(langMenu, MF_STRING | (!langEn ? MF_CHECKED : MF_UNCHECKED), IDM_LANG_ZH,
              lang::T(lang::Id::MenuLangChinese));
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(langMenu), lang::T(lang::Id::MenuLanguage));

  AppendMenuW(menu, MF_STRING, IDM_HOTKEY, lang::T(lang::Id::MenuHotkey));
  AppendMenuW(menu, MF_STRING, IDM_ABOUT, lang::T(lang::Id::MenuAbout));
  AppendMenuW(menu, MF_STRING, IDM_EXIT, lang::T(lang::Id::MenuExit));

  theme::PreparePopupMenu(hwnd);
  SetForegroundWindow(hwnd);
  const UINT cmd =
      TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd,
                     nullptr);
  DestroyMenu(menu);
  PostMessageW(hwnd, WM_NULL, 0, 0);

  switch (cmd) {
    case IDM_OPEN_PANEL:
      if (!g_panel.IsVisible()) {
        ShowPanel();
      }
      break;
    case IDM_OPEN_FOLDER:
      if (HasWatchFolder()) {
        ShellExecuteW(hwnd, L"open", g_watchPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
      }
      break;
    case IDM_CHOOSE_FOLDER: {
      std::wstring path;
      if (ChooseFolder(hwnd, path)) {
        ApplyWatchPath(path, true);
      }
      break;
    }
    case IDM_WATCH_ENABLED:
      ApplyWatchEnabled(!g_watchEnabled, true);
      break;
    case IDM_SORT_TIME:
      ApplySortMode(config::SortMode::ByTime, true);
      break;
    case IDM_SORT_NAME:
      ApplySortMode(config::SortMode::ByName, true);
      break;
    case IDM_SHOW_HIDDEN:
      ApplyShowHidden(!g_showHidden, true);
      break;
    case IDM_EXPAND_FOLDERS:
      ApplyExpandFolders(!g_expandFolders, true);
      break;
    case IDM_START_WITH_WINDOWS:
      if (!SetStartWithWindows(!IsStartWithWindows())) {
        MessageBoxW(hwnd, lang::T(lang::Id::StartupUpdateFailed), L"ToFolder",
                    MB_OK | MB_ICONERROR);
      }
      break;
    case IDM_LANG_EN:
      ApplyLanguage(lang::Language::English, true);
      break;
    case IDM_LANG_ZH:
      ApplyLanguage(lang::Language::Chinese, true);
      break;
    case IDM_HOTKEY: {
      bool enabled = g_hotkeyEnabled;
      std::wstring text = g_hotkeyText;
      if (hotkey::ShowSettingsDialog(hwnd, enabled, text)) {
        g_hotkeyEnabled = enabled;
        g_hotkeyText = text;
        config::SaveHotkeySettings(g_hotkeyEnabled, g_hotkeyText);
        // Dialog already registered via Apply; keep state in sync.
        hotkey::Apply(hwnd, g_hotkeyEnabled, g_hotkeyText);
      }
      break;
    }
    case IDM_ABOUT: {
      const int choice = MessageBoxW(hwnd, lang::T(lang::Id::AboutBody),
                                     lang::T(lang::Id::AboutTitle),
                                     MB_YESNO | MB_ICONINFORMATION | MB_DEFBUTTON1);
      if (choice == IDYES) {
        ShellExecuteW(hwnd, L"open", kGitHubRepoUrl, nullptr, nullptr, SW_SHOWNORMAL);
      }
      break;
    }
    case IDM_EXIT:
      DestroyWindow(hwnd);
      break;
  }
}

void OnTrayLeftClick() {
  if (g_panel.IsVisible()) {
    HidePanel();
    return;
  }
  // Clicking the tray deactivates the panel first; ignore the reopen from that same click.
  if (GetTickCount64() - g_panelHiddenAt < 250) {
    return;
  }
  if (!HasWatchFolder()) {
    std::wstring path;
    if (ChooseFolder(g_hwnd, path)) {
      ApplyWatchPath(path, true);
      ShowPanel();
    }
    return;
  }
  ShowPanel();
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE:
      return 0;

    case WM_TRAYICON: {
      const UINT notify = LOWORD(lParam);
      if (notify == NIN_SELECT || notify == NIN_KEYSELECT) {
        OnTrayLeftClick();
      } else if (notify == WM_CONTEXTMENU) {
        ShowContextMenu(hwnd);
      }
      return 0;
    }

    case WM_REQUEST_HIDE:
      if (g_panel.IsVisible()) {
        HidePanel();
      }
      return 0;

    case WM_DIR_CHANGED: {
      const bool hasAdded = wParam != 0;
      if (g_panel.IsVisible()) {
        g_panel.Refresh();
      } else if (hasAdded) {
        g_tray.SetUnread(true);
      }
      return 0;
    }

    case WM_HOTKEY:
      if (static_cast<int>(wParam) == hotkey::kHotkeyId) {
        OnTrayLeftClick();
      }
      return 0;

    case WM_SETTINGCHANGE:
      if (lParam == 0 ||
          _wcsicmp(reinterpret_cast<LPCWSTR>(lParam), L"ImmersiveColorSet") == 0) {
        theme::AllowDarkForWindow(hwnd, !theme::AppsUseLightTheme());
        g_tray.RefreshTheme();
        g_panel.RefreshTheme();
        theme::FlushMenusForTheme();
      }
      return 0;

    case WM_DESTROY:
      hotkey::Unregister(hwnd);
      g_watcher.Stop();
      g_tray.Remove();
      g_panel.Destroy();
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
  dpi::EnablePerMonitorV2();
  theme::InitDarkModeSupport();

  HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\ToFolderSingleInstance");
  if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
    if (mutex) {
      CloseHandle(mutex);
    }
    return 0;
  }

  if (FAILED(OleInitialize(nullptr))) {
    return 1;
  }

  INITCOMMONCONTROLSEX icc = {};
  icc.dwSize = sizeof(icc);
  icc.dwICC = ICC_WIN95_CLASSES;
  InitCommonControlsEx(&icc);

  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = MainWndProc;
  wc.hInstance = hInstance;
  {
    POINT pt = {};
    GetCursorPos(&pt);
    const UINT screenDpi = dpi::ForPoint(pt);
    const int cxLarge = dpi::Scale(32, screenDpi);
    const int cxSmall = dpi::TrayIconPx(screenDpi);
    wc.hIcon = LoadAppIcon(hInstance, IDI_APP, cxLarge, cxLarge);
    wc.hIconSm = LoadAppIcon(hInstance, IDI_APP, cxSmall, cxSmall);
  }
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.lpszClassName = L"ToFolderHiddenClass";
  RegisterClassExW(&wc);

  g_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, L"ToFolderHiddenClass", L"ToFolder", WS_POPUP, 0, 0, 0,
                           0, nullptr, nullptr, hInstance, nullptr);
  if (!g_hwnd) {
    return 1;
  }
  theme::AllowDarkForWindow(g_hwnd, !theme::AppsUseLightTheme());

  g_watcher.SetCallback(OnDirChanged);
  g_panel.Create(g_hwnd);
  g_tray.Add(g_hwnd, WM_TRAYICON);

  const config::Settings settings = config::Load();
  g_sortMode = settings.sortMode;
  g_showHidden = settings.showHidden;
  g_expandFolders = settings.expandFolders;
  g_watchEnabled = settings.watchEnabled;
  g_hotkeyEnabled = settings.hotkeyEnabled;
  g_hotkeyText = settings.hotkey.empty() ? L"Ctrl+Shift+F" : settings.hotkey;
  if (settings.languageConfigured) {
    lang::Set(settings.language);
  } else {
    // First launch: follow OS UI language, then persist (default English otherwise).
    ApplyLanguage(lang::DetectOs(), true);
  }
  g_panel.SetSortMode(g_sortMode);
  g_panel.SetShowHidden(g_showHidden);
  g_panel.SetExpandFolders(g_expandFolders);
  ApplyWatchPath(settings.watchPath, false);
  if (!hotkey::Apply(g_hwnd, g_hotkeyEnabled, g_hotkeyText) && g_hotkeyEnabled) {
    // Keep running without the hotkey if it conflicts at startup.
    g_hotkeyEnabled = false;
  }

  MSG msg = {};
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  OleUninitialize();
  CloseHandle(mutex);
  return static_cast<int>(msg.wParam);
}
