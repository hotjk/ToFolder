#pragma once

#include "config.h"
#include "lang.h"
#include "watcher.h"

#include <windows.h>

#include <commctrl.h>

#include <string>
#include <unordered_set>
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
  // Takes the display-related fields of `settings`; refreshes the list if visible.
  void ApplySettings(const config::Settings& settings);
  // Chip toggles change these; the owner is told via WM_FILTERS_CHANGED.
  const std::vector<std::wstring>& ActiveFilters() const { return settings_.activeFilters; }
  bool IsPinned() const { return pinned_; }
  void Refresh();
  void RefreshTheme();

 private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
  bool OnNotify(const NMHDR* nmhdr, LRESULT* result);
  void OnGetDispInfo(NMLVDISPINFOW* di) const;
  void OnGetInfoTip(NMLVGETINFOTIPW* tip) const;
  bool OnContextMenu(HWND source, POINT screenPt);
  bool HandleNavigationKey(UINT vk);

  // Window / child setup
  void EnsureList();
  void EnsureListIcons();
  void EnsureFont();
  void EnsureTooltip();
  void EnsureListInfoTipTopMost();
  void UpdatePinTooltip();
  void ApplyTheme();

  // Layout and metrics
  void Layout();
  void UpdateColumnWidth();
  void ApplyVisibleSize(bool allowShrink);
  void EnsureSelection();
  UINT CurrentDpi() const;
  int Px(int value96) const;
  int PanelWidthPx() const;
  int RowHeightPx() const;
  int PinSizePx() const;
  RECT PinRect() const;
  int TopChromeHeightPx() const;
  int PanelHeightPx(const RECT& workArea) const;
  int HitTestItem(POINT clientPt) const;
  int SelectedIndex() const;
  int FocusedIndex() const;
  bool IsItemSelected(int index) const;
  void SelectAll();
  // Selected rows that file actions apply to, in list order (skips ".." and placeholders).
  std::vector<int> SelectedActionable() const;
  // Whole selection when `index` is already selected; otherwise just that row.
  std::vector<int> ActionTargets(int index) const;

  // Filter chips and pin
  bool HasFilterBar() const;
  void UpdateChipLayout() const;
  void PaintFilterBar(HDC hdc);
  void PaintPin(HDC hdc);
  int HitTestFilterChip(POINT clientPt) const;
  bool HitTestPin(POINT clientPt) const;
  bool IsFilterActive(const std::wstring& name) const;
  void ToggleFilterChip(int index);
  void TogglePin();
  void RebuildFilterMatcher();
  bool MatchesFilters(const FileEntry& entry) const;

  // Listing and navigation
  void PopulateList();
  void NavigateTo(const std::wstring& path);
  void NavigateUp();
  bool IsAtWatchRoot() const;
  bool IsParentRow(int index) const;
  const FileEntry* ItemAt(int index) const;
  // Like ItemAt, but excludes the ".." row, which file actions do not apply to.
  const FileEntry* ActionableItemAt(int index) const;

  // Item actions
  void OpenAt(int index);
  void OpenLocationAt(int index);
  bool DeleteItems(const std::vector<int>& indices);
  void BeginDragAt(int index);
  void ShowItemContextMenu(int index, POINT screenPt);

  void RequestHide();
  void HideUnlessPinned();
  bool PanelIsForeground() const;

  HWND hwnd_ = nullptr;
  HWND owner_ = nullptr;
  HWND listView_ = nullptr;
  HWND tooltip_ = nullptr;
  HFONT listFont_ = nullptr;
  HFONT pinFont_ = nullptr;
  UINT fontDpi_ = 0;
  config::FontSize fontSizeBuilt_ = config::FontSize::Small;
  HBRUSH bgBrush_ = nullptr;
  bool appsLight_ = true;

  config::Settings settings_;
  std::wstring browsePath_;
  bool pinned_ = false;

  // Derived from settings_ by RebuildFilterMatcher().
  bool filteringActive_ = false;
  bool matchAllFiles_ = false;
  bool matchFolders_ = false;
  std::unordered_set<std::wstring> allowedExt_;

  // Chip layout cache; recomputed when dirty or when width / DPI change.
  mutable std::vector<RECT> chipRects_;
  mutable int chipBarHeight_ = 0;
  mutable int chipLayoutWidth_ = -1;
  mutable UINT chipLayoutDpi_ = 0;
  mutable bool chipLayoutDirty_ = true;

  std::vector<FileEntry> items_;
  lang::Id emptyTextId_ = lang::Id::EmptyFolder;  // placeholder row when items_ is empty
  int retainedHeightPx_ = 0;  // grow-only while panel stays open
  bool created_ = false;
  bool suppressDeactivateHide_ = false;
};
