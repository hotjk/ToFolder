#include "tray.h"

#include "icon_load.h"
#include "resource.h"

#include <shellapi.h>

#ifndef NIF_SHOWTIP
#define NIF_SHOWTIP 0x00000080
#endif

namespace {

void CopyTip(NOTIFYICONDATAW& nid, const std::wstring& tip) {
  const wchar_t* src = tip.empty() ? L"ToFolder" : tip.c_str();
  wcsncpy_s(nid.szTip, _countof(nid.szTip), src, _TRUNCATE);
}

}  // namespace

TrayIcon::TrayIcon() = default;

TrayIcon::~TrayIcon() {
  Remove();
  DestroyIcons();
}

void TrayIcon::DestroyIcons() {
  if (iconNormal_) {
    DestroyIcon(iconNormal_);
    iconNormal_ = nullptr;
  }
  if (iconOpen_) {
    DestroyIcon(iconOpen_);
    iconOpen_ = nullptr;
  }
  if (iconPlus_) {
    DestroyIcon(iconPlus_);
    iconPlus_ = nullptr;
  }
}

bool TrayIcon::LoadIcons() {
  DestroyIcons();
  HINSTANCE inst = GetModuleHandleW(nullptr);
  // Exact tray-slot pixels: shell will not re-scale a huge 256px icon (which looked blurry).
  const int cx = TrayIconSizePx(hwnd_);
  iconNormal_ = LoadAppIcon(inst, IDI_FOLDER, cx, cx);
  iconOpen_ = LoadAppIcon(inst, IDI_FOLDER_OPEN, cx, cx);
  iconPlus_ = LoadAppIcon(inst, IDI_FOLDER_FILL, cx, cx);
  return iconNormal_ && iconOpen_ && iconPlus_;
}

HICON TrayIcon::CurrentIcon() const {
  if (panelOpen_ && iconOpen_) {
    return iconOpen_;
  }
  if (hasUnread_ && iconPlus_) {
    return iconPlus_;
  }
  return iconNormal_ ? iconNormal_ : LoadIconW(nullptr, IDI_APPLICATION);
}

bool TrayIcon::Add(HWND hwnd, UINT callbackMessage) {
  hwnd_ = hwnd;
  callbackMessage_ = callbackMessage;

  if (!LoadIcons()) {
    return false;
  }

  ZeroMemory(&nid_, sizeof(nid_));
  nid_.cbSize = sizeof(NOTIFYICONDATAW);
  nid_.hWnd = hwnd_;
  nid_.uID = 1;
  nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
  nid_.uCallbackMessage = callbackMessage_;
  nid_.hIcon = CurrentIcon();
  CopyTip(nid_, tip_);

  added_ = Shell_NotifyIconW(NIM_ADD, &nid_) != FALSE;
  if (added_) {
    nid_.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &nid_);
  }
  return added_;
}

void TrayIcon::Remove() {
  if (added_) {
    Shell_NotifyIconW(NIM_DELETE, &nid_);
    added_ = false;
  }
}

void TrayIcon::SetTip(const std::wstring& tip) {
  tip_ = tip;
  if (!added_) {
    return;
  }
  nid_.uFlags = NIF_TIP | NIF_SHOWTIP;
  CopyTip(nid_, tip_);
  Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayIcon::ApplyIcon() {
  if (!added_) {
    return;
  }
  nid_.uFlags = NIF_ICON | NIF_TIP | NIF_SHOWTIP;
  nid_.hIcon = CurrentIcon();
  CopyTip(nid_, tip_);
  Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayIcon::SetUnread(bool unread) {
  if (hasUnread_ == unread) {
    return;
  }
  hasUnread_ = unread;
  ApplyIcon();
}

void TrayIcon::SetPanelOpen(bool open) {
  if (panelOpen_ == open) {
    return;
  }
  panelOpen_ = open;
  ApplyIcon();
}
