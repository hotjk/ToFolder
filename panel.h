#pragma once

#include "config.h"
#include "watcher.h"

#include <windows.h>

#include <string>
#include <vector>

class PanelWindow {
 public:
  PanelWindow();
  ~PanelWindow();

  PanelWindow(const PanelWindow&) = delete;
  PanelWindow& operator=(const PanelWindow&) = delete;

  bool Create(HWND owner);
  void Destroy();
  HWND Hwnd() const { return hwnd_; }
  bool IsVisible() const;
  void Show();
  void Hide();
  void Toggle();
  void SetWatchPath(const std::wstring& path);
  void SetSortMode(config::SortMode mode);
  config::SortMode SortMode() const { return sortMode_; }
  void Refresh();
  bool ContainsPoint(POINT pt) const;

 private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
  void Layout();
  void UpdateColumnWidth();
  void PopulateList();
  void OpenAt(int index);
  void OpenLocationAt(int index);
  bool DeleteAt(int index);
  void ShowItemContextMenu(int index, POINT screenPt);
  void EnsureList();
  void EnsureListIcons();
  void EnsureFont();
  RECT WorkAreaRect() const;
  UINT CurrentDpi() const;
  int PanelWidthPx() const;
  int RowHeightPx() const;
  int PanelHeightPx(const RECT& workArea) const;
  int HitTestItem(POINT clientPt) const;

  HWND hwnd_ = nullptr;
  HWND owner_ = nullptr;
  HWND listView_ = nullptr;
  HFONT listFont_ = nullptr;
  std::wstring watchPath_;
  config::SortMode sortMode_ = config::SortMode::ByTime;
  std::vector<FileEntry> items_;
  bool created_ = false;
};
