#include "app_messages.h"
#include "config.h"
#include "dpi.h"
#include "filter_dlg.h"
#include "help_dlg.h"
#include "hotkey.h"
#include "icon_load.h"
#include "lang.h"
#include "panel.h"
#include "path_util.h"
#include "resource.h"
#include "theme.h"
#include "tray.h"
#include "watcher.h"

#include <commctrl.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <windows.h>

#include <string>
#include <vector>

namespace {

constexpr wchar_t kGitHubRepoUrl[] = L"https://github.com/hotjk/ToFolder";
constexpr UINT_PTR kDirChangeTimerId = 1;
// Bursts of change notifications (e.g. a file being copied) collapse into one refresh.
constexpr UINT kDirChangeCoalesceMs = 200;

HWND g_hwnd = nullptr;
UINT g_taskbarCreatedMsg = 0;
TrayIcon g_tray;
PanelWindow g_panel;
DirectoryWatcher g_watcher;
config::Settings g_settings;
ULONGLONG g_panelHiddenAt = 0;
bool g_dirChangePending = false;
bool g_dirChangeHasAdded = false;

void SaveSettings() {
  config::Save(g_settings);
}

// Persists g_settings and pushes it to the panel (which refreshes itself if visible).
void CommitSettings() {
  SaveSettings();
  g_panel.ApplySettings(g_settings);
}

bool HasWatchFolder() {
  return DirectoryWatcher::PathExists(g_settings.watchPath);
}

void UpdateTrayTip() {
  if (g_settings.watchPath.empty()) {
    g_tray.SetTip(lang::T(lang::Id::TipNoFolder));
    return;
  }
  std::wstring tip = lang::T(lang::Id::TipPrefix);
  tip += g_settings.watchPath;
  if (tip.size() > 120) {
    tip = tip.substr(0, 117) + L"...";
  }
  g_tray.SetTip(tip);
}

void RestartWatcher() {
  g_watcher.Stop();
  if (g_settings.watchEnabled && HasWatchFolder()) {
    g_watcher.Start(g_settings.watchPath);
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

void SetWatchPath(const std::wstring& path) {
  g_settings.watchPath = path;
  g_tray.SetUnread(false);
  UpdateTrayTip();
  RestartWatcher();
  CommitSettings();
}

void SetLanguage(lang::Language language) {
  lang::Set(language);
  g_settings.language = language;
  g_settings.languageConfigured = true;
  UpdateTrayTip();
  CommitSettings();
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

// ---------------------------------------------------------------------------
// Start with Windows (HKCU Run). The value name embeds the exe path hash so renamed
// copies, which run as separate instances, each keep their own entry.

constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kLegacyRunValueName[] = L"ToFolder";

std::wstring RunValueName() {
  return L"ToFolder_" + config::GetExePathHash();
}

std::wstring QuotedExePath() {
  return L"\"" + config::GetExePath() + L"\"";
}

bool ReadRunValue(HKEY key, const wchar_t* name, std::wstring& out) {
  wchar_t buf[2048] = {};
  DWORD type = 0;
  DWORD size = sizeof(buf) - sizeof(wchar_t);
  if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<LPBYTE>(buf), &size) !=
          ERROR_SUCCESS ||
      type != REG_SZ) {
    return false;
  }
  out = buf;
  return true;
}

// Older builds registered every copy under one shared name; only treat it as ours if it
// points at this exe.
bool LegacyRunValueIsOurs(HKEY key) {
  std::wstring value;
  return ReadRunValue(key, kLegacyRunValueName, value) &&
         path_util::EqualsNoCase(value, QuotedExePath());
}

bool IsStartWithWindows() {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_READ, &key) != ERROR_SUCCESS) {
    return false;
  }
  std::wstring value;
  const bool enabled = (ReadRunValue(key, RunValueName().c_str(), value) && !value.empty()) ||
                       LegacyRunValueIsOurs(key);
  RegCloseKey(key);
  return enabled;
}

bool SetStartWithWindows(bool enable) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key) !=
      ERROR_SUCCESS) {
    return false;
  }
  if (LegacyRunValueIsOurs(key)) {
    RegDeleteValueW(key, kLegacyRunValueName);
  }
  const std::wstring name = RunValueName();
  bool ok = false;
  if (enable) {
    const std::wstring value = QuotedExePath();
    ok = RegSetValueExW(key, name.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                        static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
  } else {
    const LSTATUS st = RegDeleteValueW(key, name.c_str());
    ok = (st == ERROR_SUCCESS || st == ERROR_FILE_NOT_FOUND);
  }
  RegCloseKey(key);
  return ok;
}

// ---------------------------------------------------------------------------
// Tray menu

void AppendItem(HMENU menu, UINT id, lang::Id text, bool checked = false, bool enabled = true) {
  UINT flags = MF_STRING;
  flags |= checked ? MF_CHECKED : MF_UNCHECKED;
  flags |= enabled ? MF_ENABLED : MF_GRAYED;
  AppendMenuW(menu, flags, id, lang::T(text));
}

void AppendSeparator(HMENU menu) {
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
}

HMENU AppendSubmenu(HMENU menu, lang::Id text) {
  HMENU sub = CreatePopupMenu();
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(sub), lang::T(text));
  return sub;
}

// Small / Medium / Large submenu whose command ids are consecutive from firstId.
void AppendSizeSubmenu(HMENU menu, lang::Id title, UINT firstId, const lang::Id (&labels)[3],
                       int currentIndex) {
  HMENU sub = AppendSubmenu(menu, title);
  for (int i = 0; i < 3; ++i) {
    AppendItem(sub, firstId + i, labels[i], i == currentIndex);
  }
}

HMENU BuildTrayMenu() {
  const bool hasFolder = HasWatchFolder();
  HMENU menu = CreatePopupMenu();
  AppendItem(menu, IDM_OPEN_PANEL, lang::Id::MenuOpenPanel, false, hasFolder);
  AppendItem(menu, IDM_OPEN_FOLDER, lang::Id::MenuOpenFolder, false, hasFolder);
  AppendItem(menu, IDM_CHOOSE_FOLDER, lang::Id::MenuChooseFolder);
  AppendItem(menu, IDM_WATCH_ENABLED, lang::Id::MenuWatchNewFiles, g_settings.watchEnabled);
  AppendSeparator(menu);
  AppendItem(menu, IDM_SORT_TIME, lang::Id::MenuSortByTime,
             g_settings.sortMode == config::SortMode::ByTime);
  AppendItem(menu, IDM_SORT_NAME, lang::Id::MenuSortByName,
             g_settings.sortMode == config::SortMode::ByName);
  AppendItem(menu, IDM_SHOW_HIDDEN, lang::Id::MenuShowHidden, g_settings.showHidden);
  AppendItem(menu, IDM_EXPAND_FOLDERS, lang::Id::MenuExpandFolders, g_settings.expandFolders);
  AppendItem(menu, IDM_START_WITH_WINDOWS, lang::Id::MenuStartWithWindows, IsStartWithWindows());
  AppendSeparator(menu);

  HMENU langMenu = AppendSubmenu(menu, lang::Id::MenuLanguage);
  const bool zh = lang::Current() == lang::Language::Chinese;
  AppendItem(langMenu, IDM_LANG_EN, lang::Id::MenuLangEnglish, !zh);
  AppendItem(langMenu, IDM_LANG_ZH, lang::Id::MenuLangChinese, zh);

  AppendSizeSubmenu(menu, lang::Id::MenuFontSize, IDM_FONT_SMALL,
                    {lang::Id::MenuFontSmall, lang::Id::MenuFontMedium, lang::Id::MenuFontLarge},
                    config::SizeIndex(g_settings.fontSize));
  AppendSizeSubmenu(menu, lang::Id::MenuPanelSize, IDM_PANEL_SMALL,
                    {lang::Id::MenuPanelSmall, lang::Id::MenuPanelMedium, lang::Id::MenuPanelLarge},
                    config::SizeIndex(g_settings.panelSize));

  AppendItem(menu, IDM_HOTKEY, lang::Id::MenuHotkey);
  AppendItem(menu, IDM_FILTERS, lang::Id::MenuFilters);
  AppendItem(menu, IDM_HELP, lang::Id::MenuHelp);
  AppendItem(menu, IDM_ABOUT, lang::Id::MenuAbout);
  AppendItem(menu, IDM_EXIT, lang::Id::MenuExit);
  return menu;
}

void OnTrayCommand(HWND hwnd, UINT cmd) {
  if (cmd >= IDM_FONT_SMALL && cmd <= IDM_FONT_LARGE) {
    g_settings.fontSize = static_cast<config::FontSize>(cmd - IDM_FONT_SMALL);
    CommitSettings();
    return;
  }
  if (cmd >= IDM_PANEL_SMALL && cmd <= IDM_PANEL_LARGE) {
    g_settings.panelSize = static_cast<config::PanelSize>(cmd - IDM_PANEL_SMALL);
    CommitSettings();
    return;
  }

  switch (cmd) {
    case IDM_OPEN_PANEL:
      if (!g_panel.IsVisible()) {
        ShowPanel();
      }
      break;
    case IDM_OPEN_FOLDER:
      if (HasWatchFolder()) {
        ShellExecuteW(hwnd, L"open", g_settings.watchPath.c_str(), nullptr, nullptr,
                      SW_SHOWNORMAL);
      }
      break;
    case IDM_CHOOSE_FOLDER: {
      std::wstring path;
      if (ChooseFolder(hwnd, path)) {
        SetWatchPath(path);
      }
      break;
    }
    case IDM_WATCH_ENABLED:
      g_settings.watchEnabled = !g_settings.watchEnabled;
      SaveSettings();
      RestartWatcher();
      if (!g_settings.watchEnabled) {
        g_tray.SetUnread(false);
      }
      break;
    case IDM_SORT_TIME:
    case IDM_SORT_NAME:
      g_settings.sortMode =
          cmd == IDM_SORT_NAME ? config::SortMode::ByName : config::SortMode::ByTime;
      CommitSettings();
      break;
    case IDM_SHOW_HIDDEN:
      g_settings.showHidden = !g_settings.showHidden;
      CommitSettings();
      break;
    case IDM_EXPAND_FOLDERS:
      g_settings.expandFolders = !g_settings.expandFolders;
      CommitSettings();
      break;
    case IDM_START_WITH_WINDOWS:
      if (!SetStartWithWindows(!IsStartWithWindows())) {
        MessageBoxW(hwnd, lang::T(lang::Id::StartupUpdateFailed), L"ToFolder",
                    MB_OK | MB_ICONERROR);
      }
      break;
    case IDM_LANG_EN:
      SetLanguage(lang::Language::English);
      break;
    case IDM_LANG_ZH:
      SetLanguage(lang::Language::Chinese);
      break;
    case IDM_HOTKEY: {
      bool enabled = g_settings.hotkeyEnabled;
      std::wstring text = g_settings.hotkey;
      // The dialog registers the chosen hotkey itself (and restores the old one on conflict).
      if (hotkey::ShowSettingsDialog(hwnd, enabled, text)) {
        g_settings.hotkeyEnabled = enabled;
        g_settings.hotkey = text;
        SaveSettings();
      }
      break;
    }
    case IDM_FILTERS: {
      bool enabled = g_settings.showFilters;
      auto filters = g_settings.filters;
      auto active = g_settings.activeFilters;
      if (filter_dlg::ShowFiltersDialog(hwnd, enabled, filters, active)) {
        g_settings.showFilters = enabled;
        g_settings.filters = std::move(filters);
        g_settings.activeFilters = std::move(active);
        CommitSettings();
      }
      break;
    }
    case IDM_HELP:
      help_dlg::ShowHelpDialog(hwnd);
      break;
    case IDM_ABOUT: {
      const int choice =
          MessageBoxW(hwnd, lang::T(lang::Id::AboutBody), lang::T(lang::Id::AboutTitle),
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

void ShowContextMenu(HWND hwnd) {
  POINT pt = {};
  GetCursorPos(&pt);
  HMENU menu = BuildTrayMenu();
  theme::PreparePopupMenu(hwnd);
  SetForegroundWindow(hwnd);
  const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x,
                                  pt.y, 0, hwnd, nullptr);
  DestroyMenu(menu);
  PostMessageW(hwnd, WM_NULL, 0, 0);
  OnTrayCommand(hwnd, cmd);
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
      SetWatchPath(path);
      ShowPanel();
    }
    return;
  }
  ShowPanel();
}

void FlushDirChange() {
  KillTimer(g_hwnd, kDirChangeTimerId);
  g_dirChangePending = false;
  if (g_panel.IsVisible()) {
    g_panel.Refresh();
  } else if (g_dirChangeHasAdded) {
    g_tray.SetUnread(true);
  }
  g_dirChangeHasAdded = false;
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (msg == g_taskbarCreatedMsg && g_taskbarCreatedMsg != 0) {
    g_tray.Recreate();
    UpdateTrayTip();
    return 0;
  }

  switch (msg) {
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
      HidePanel();
      return 0;

    case WM_FILTERS_CHANGED:
      g_settings.activeFilters = g_panel.ActiveFilters();
      SaveSettings();
      return 0;

    case WM_DIR_CHANGED:
      // Throttle rather than debounce so a continuous stream still refreshes periodically.
      g_dirChangeHasAdded = g_dirChangeHasAdded || wParam != 0;
      if (!g_dirChangePending) {
        g_dirChangePending = true;
        SetTimer(hwnd, kDirChangeTimerId, kDirChangeCoalesceMs, nullptr);
      }
      return 0;

    case WM_TIMER:
      if (wParam == kDirChangeTimerId) {
        FlushDirChange();
      }
      return 0;

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
      KillTimer(hwnd, kDirChangeTimerId);
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

  // One instance per executable path; different exe copies may run together.
  const std::wstring mutexName = config::GetSingleInstanceMutexName();
  HANDLE mutex = CreateMutexW(nullptr, TRUE, mutexName.c_str());
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

  g_taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");
  g_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, L"ToFolderHiddenClass", L"ToFolder", WS_POPUP, 0, 0, 0,
                           0, nullptr, nullptr, hInstance, nullptr);
  if (!g_hwnd) {
    return 1;
  }
  theme::AllowDarkForWindow(g_hwnd, !theme::AppsUseLightTheme());

  g_settings = config::Load();
  if (!g_settings.languageConfigured) {
    // First launch: follow OS UI language, then persist (default English otherwise).
    g_settings.language = lang::DetectOs();
    g_settings.languageConfigured = true;
  }
  lang::Set(g_settings.language);

  g_watcher.SetCallback(OnDirChanged);
  g_panel.Create(g_hwnd);
  g_tray.Add(g_hwnd, WM_TRAYICON);
  UpdateTrayTip();
  RestartWatcher();
  // Also writes defaults (e.g. filters) into the exe-named .ini when they were missing.
  CommitSettings();
  // Keep running without the hotkey if it conflicts at startup.
  hotkey::Apply(g_hwnd, g_settings.hotkeyEnabled, g_settings.hotkey);

  MSG msg = {};
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  OleUninitialize();
  CloseHandle(mutex);
  return static_cast<int>(msg.wParam);
}
