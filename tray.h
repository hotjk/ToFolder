#pragma once

#include <windows.h>

#include <string>

class TrayIcon {
 public:
  TrayIcon();
  ~TrayIcon();

  TrayIcon(const TrayIcon&) = delete;
  TrayIcon& operator=(const TrayIcon&) = delete;

  bool Add(HWND hwnd, UINT callbackMessage);
  void Remove();
  void SetTip(const std::wstring& tip);
  void SetUnread(bool unread);
  void SetPanelOpen(bool open);
  bool HasUnread() const { return hasUnread_; }
  bool IsPanelOpen() const { return panelOpen_; }

 private:
  bool LoadIcons();
  void DestroyIcons();
  HICON CurrentIcon() const;
  void ApplyIcon();

  HWND hwnd_ = nullptr;
  UINT callbackMessage_ = 0;
  NOTIFYICONDATAW nid_ = {};
  HICON iconNormal_ = nullptr;
  HICON iconOpen_ = nullptr;
  HICON iconPlus_ = nullptr;
  bool added_ = false;
  bool hasUnread_ = false;
  bool panelOpen_ = false;
  std::wstring tip_;
};
