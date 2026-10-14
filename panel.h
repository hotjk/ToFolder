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
  void SetShowHidden(bool showHidden);
  bool ShowHidden() const { return showHidden_; }
  void SetExpandFolders(bool expandFolders);
  bool ExpandFolders() const { return expandFolders_; }
  void SetShowFilters(bool showFilters);
  bool ShowFilters() const { return showFilters_; }
  void SetFilters(const std::vector<config::FilterItem>& filters);
  void SetActiveFilters(const std::vector<std::wstring>& activeFilters);
  const std::vector<config::FilterItem>& Filters() const { return filters_; }
  const std::vector<std::wstring>& ActiveFilters() const { return activeFilters_; }
  bool IsPinned() const { return pinned_; }
  void Refresh();
  void RefreshTheme();
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
  void BeginDragAt(int index);
  void EnsureList();
  void EnsureListIcons();
  void EnsureFont();
  void EnsureSelection();
  void EnsureTooltip();
  void UpdatePinTooltip();
  void ApplyTheme();
  void NavigateTo(const std::wstring& path);
  void NavigateUp();
  void ApplyVisibleSize(bool allowShrink);
  bool IsAtWatchRoot() const;
  bool IsParentRow(int index) const;
  std::wstring ParentPath(const std::wstring& path) const;
  bool IsPathUnderWatch(const std::wstring& path) const;
  RECT WorkAreaRect() const;
  UINT CurrentDpi() const;
  int PanelWidthPx() const;
  int RowHeightPx() const;
  int PinSizePx() const;
  int PinReservePx() const;
  RECT PinRect() const;
  int TopChromeHeightPx() const;
  int FilterBarHeightPx() const;
  int PanelHeightPx(const RECT& workArea) const;
  int HitTestItem(POINT clientPt) const;
  int SelectedIndex() const;
  bool HasFilterBar() const;
  void ComputeFilterChipLayout(std::vector<RECT>* outRects, int* outBarH) const;
  void LayoutFilterChips();
  void PaintFilterBar(HDC hdc);
  void PaintPin(HDC hdc);
  int HitTestFilterChip(POINT clientPt) const;
  bool HitTestPin(POINT clientPt) const;
  bool IsFilterActive(const std::wstring& name) const;
  void ToggleFilterChip(int index);
  void TogglePin();
  void NotifyFiltersChanged();

  HWND hwnd_ = nullptr;
  HWND owner_ = nullptr;
  HWND listView_ = nullptr;
  HWND tooltip_ = nullptr;
  HFONT listFont_ = nullptr;
  HBRUSH bgBrush_ = nullptr;
  bool appsLight_ = true;
  std::wstring watchPath_;
  std::wstring browsePath_;
  config::SortMode sortMode_ = config::SortMode::ByTime;
  bool showHidden_ = false;
  bool expandFolders_ = false;
  bool showFilters_ = true;
  bool pinned_ = false;
  std::vector<config::FilterItem> filters_;
  std::vector<std::wstring> activeFilters_;
  std::vector<RECT> filterChipRects_;
  std::vector<FileEntry> items_;
  int retainedHeightPx_ = 0;  // grow-only while panel stays open
  bool created_ = false;
  bool suppressDeactivateHide_ = false;
};
