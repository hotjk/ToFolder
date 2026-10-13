#include "app_messages.h"
#include "config.h"
#include "dpi.h"
#include "icon_load.h"
#include "panel.h"
#include "resource.h"
#include "tray.h"
#include "watcher.h"

#include <commctrl.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <windows.h>

#include <string>

namespace {

HWND g_hwnd = nullptr;
TrayIcon g_tray;
PanelWindow g_panel;
DirectoryWatcher g_watcher;
HHOOK g_mouseHook = nullptr;
HHOOK g_keyboardHook = nullptr;
std::wstring g_watchPath;

void UpdateTrayTip() {
  if (g_watchPath.empty()) {
    g_tray.SetTip(L"ToFolder - No folder selected");
  } else {
    std::wstring tip = L"ToFolder - " + g_watchPath;
    if (tip.size() > 120) {
      tip = tip.substr(0, 117) + L"...";
    }
    g_tray.SetTip(tip);
  }
}

void UninstallDismissHooks() {
  if (g_mouseHook) {
    UnhookWindowsHookEx(g_mouseHook);
    g_mouseHook = nullptr;
  }
  if (g_keyboardHook) {
    UnhookWindowsHookEx(g_keyboardHook);
    g_keyboardHook = nullptr;
  }
}

bool PointInTrayIcon(POINT pt) {
  NOTIFYICONIDENTIFIER id = {};
  id.cbSize = sizeof(id);
  id.hWnd = g_hwnd;
  id.uID = 1;
  RECT rc = {};
  if (FAILED(Shell_NotifyIconGetRect(&id, &rc))) {
    return false;
  }
  InflateRect(&rc, 4, 4);
  return PtInRect(&rc, pt) != FALSE;
}

LRESULT CALLBACK LowLevelMouseProc(int code, WPARAM wParam, LPARAM lParam) {
  if (code == HC_ACTION && g_panel.IsVisible()) {
    if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN) {
      const auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
      const POINT pt = info->pt;
      if (!g_panel.ContainsPoint(pt) && !PointInTrayIcon(pt)) {
        PostMessageW(g_hwnd, WM_REQUEST_HIDE, 0, 0);
      }
    }
  }
  return CallNextHookEx(g_mouseHook, code, wParam, lParam);
}

LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
  if (code == HC_ACTION && g_panel.IsVisible()) {
    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
      const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
      if (info->vkCode == VK_ESCAPE) {
        PostMessageW(g_hwnd, WM_REQUEST_HIDE, 0, 0);
        return 1;  // swallow so other apps don't also see Esc
      }
    }
  }
  return CallNextHookEx(g_keyboardHook, code, wParam, lParam);
}

void InstallDismissHooks() {
  HMODULE mod = GetModuleHandleW(nullptr);
  if (!g_mouseHook) {
    g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, mod, 0);
  }
  if (!g_keyboardHook) {
    g_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, mod, 0);
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
  dialog->SetTitle(L"Select folder to watch");

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
  UpdateTrayTip();
  g_watcher.Stop();
  if (DirectoryWatcher::PathExists(g_watchPath)) {
    g_watcher.Start(g_watchPath);
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
  InstallDismissHooks();
}

void HidePanel() {
  g_panel.Hide();
  g_tray.SetPanelOpen(false);
  UninstallDismissHooks();
}

void ShowContextMenu(HWND hwnd) {
  POINT pt = {};
  GetCursorPos(&pt);
  HMENU menu = CreatePopupMenu();
  AppendMenuW(menu, MF_STRING, IDM_OPEN_PANEL, L"Open panel");
  AppendMenuW(menu, MF_STRING, IDM_OPEN_FOLDER, L"Open folder");
  if (!HasWatchFolder()) {
    EnableMenuItem(menu, IDM_OPEN_PANEL, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(menu, IDM_OPEN_FOLDER, MF_BYCOMMAND | MF_GRAYED);
  }
  AppendMenuW(menu, MF_STRING, IDM_CHOOSE_FOLDER, L"Choose folder...");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, IDM_EXIT, L"Exit");

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

    case WM_DESTROY:
      UninstallDismissHooks();
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

  HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\ToFolderSingleInstance");
  if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
    if (mutex) {
      CloseHandle(mutex);
    }
    return 0;
  }

  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

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

  g_watcher.SetCallback(OnDirChanged);
  g_panel.Create(g_hwnd);
  g_tray.Add(g_hwnd, WM_TRAYICON);

  ApplyWatchPath(config::LoadWatchPath(), false);

  MSG msg = {};
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  CoUninitialize();
  CloseHandle(mutex);
  return static_cast<int>(msg.wParam);
}
