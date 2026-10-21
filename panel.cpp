#include "panel.h"

#include "app_messages.h"
#include "clipboard.h"
#include "dpi.h"
#include "drop_source.h"
#include "path_util.h"
#include "resource.h"
#include "slide_anim.h"
#include "str_util.h"
#include "theme.h"
#include "ui_util.h"
#include "win_compat.h"

#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>
#include <new>
#include <string>
#include <vector>

namespace {

// Indexed by config::SizeIndex(): Small / Medium / Large.
constexpr int kPanelWidth96[] = {360, 440, 520};
constexpr int kRowHeight96[] = {28, 32, 38};
constexpr int kChipHeight96[] = {20, 24, 28};

constexpr int kPanelMargin96 = 12;
constexpr int kPanelPadding96 = 12;
constexpr int kChipPadX96 = 6;
constexpr int kChipGapX96 = 10;
constexpr int kChipGapY96 = 6;
constexpr int kChipMinWidth96 = 28;
constexpr int kFilterListGap96 = 6;  // space between chip block and list
constexpr int kPinSize96 = 20;
constexpr int kPinGap96 = 8;  // gap between chips and pin
constexpr int kPanelMinHeight96 = 160;
constexpr int kOffscreenGap96 = 32;  // clear of the DWM shadow when parked off-screen
constexpr UINT_PTR kPinTooltipId = 1;
// Segoe MDL2 Assets glyphs
constexpr wchar_t kPinGlyph[] = L"\xE718";
constexpr wchar_t kPinSolidGlyph[] = L"\xE840";
constexpr wchar_t kClassName[] = L"ToFolderPanelClass";

struct MenuEntry {
  UINT id;  // 0 = separator
  lang::Id text;
};

constexpr MenuEntry kItemMenu[] = {
    {IDM_LIST_OPEN, lang::Id::ListOpen},
    {IDM_LIST_OPEN_LOCATION, lang::Id::ListOpenLocation},
    {0, lang::Id::Count},
    {IDM_LIST_CUT, lang::Id::ListCut},
    {IDM_LIST_COPY, lang::Id::ListCopy},
    {IDM_LIST_COPY_NAME, lang::Id::ListCopyName},
    {IDM_LIST_COPY_PATH, lang::Id::ListCopyPath},
    {0, lang::Id::Count},
    {IDM_LIST_DELETE, lang::Id::ListDelete},
};

void ApplyWin11Chrome(HWND hwnd) {
  const DWORD corner = DWMWCP_ROUND;
  DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

  // Prevent DWM from adding its own show/hide morph (looks like scaling).
  const BOOL noTransition = TRUE;
  DwmSetWindowAttribute(hwnd, DWMWA_TRANSITIONS_FORCEDISABLED, &noTransition, sizeof(noTransition));
}

std::wstring DisplayName(const FileEntry& entry, bool isParentRow) {
  return entry.isDirectory && !isParentRow ? entry.name + L"\\" : entry.name;
}

void SelectOnly(HWND list, int index) {
  ListView_SetItemState(list, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
  ListView_SetItemState(list, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}

}  // namespace

PanelWindow::PanelWindow() = default;

PanelWindow::~PanelWindow() {
  Destroy();
}

bool PanelWindow::Create(HWND owner) {
  owner_ = owner;

  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wc.lpszClassName = kClassName;
  RegisterClassExW(&wc);

  const RECT wa = ui_util::WorkAreaAtCursor();
  hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, kClassName, L"ToFolder",
                          WS_POPUP | WS_CLIPCHILDREN, wa.right, wa.top, PanelWidthPx(),
                          wa.bottom - wa.top, owner, nullptr, GetModuleHandleW(nullptr), this);
  if (!hwnd_) {
    return false;
  }

  ApplyWin11Chrome(hwnd_);

  created_ = true;
  EnsureFont();
  EnsureList();
  EnsureTooltip();
  ApplyTheme();
  Layout();
  return true;
}

void PanelWindow::Destroy() {
  if (tooltip_) {
    DestroyWindow(tooltip_);
    tooltip_ = nullptr;
  }
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
  listView_ = nullptr;
  for (HFONT* font : {&listFont_, &pinFont_}) {
    if (*font) {
      DeleteObject(*font);
      *font = nullptr;
    }
  }
  fontDpi_ = 0;
  if (bgBrush_) {
    DeleteObject(bgBrush_);
    bgBrush_ = nullptr;
  }
  created_ = false;
}

bool PanelWindow::IsVisible() const {
  return hwnd_ && IsWindowVisible(hwnd_);
}

// ---------------------------------------------------------------------------
// Settings

void PanelWindow::ApplySettings(const config::Settings& settings) {
  const bool watchChanged = settings.watchPath != settings_.watchPath;
  settings_ = settings;
  config::PruneActiveFilters(settings_.filters, settings_.activeFilters);
  if (watchChanged || !settings_.expandFolders) {
    browsePath_ = settings_.watchPath;
  }
  if (watchChanged) {
    seenPaths_.clear();
    newFileTracking_ = false;
  }
  RebuildFilterMatcher();
  chipLayoutDirty_ = true;
  if (created_) {
    EnsureFont();
  }
  if (IsVisible()) {
    Refresh();
  }
}

void PanelWindow::RebuildFilterMatcher() {
  filteringActive_ = settings_.showFilters && !settings_.activeFilters.empty();
  matchAllFiles_ = false;
  matchFolders_ = false;
  allowedExt_.clear();
  if (!filteringActive_) {
    return;
  }
  for (const auto& filter : settings_.filters) {
    if (!IsFilterActive(filter.name)) {
      continue;
    }
    if (config::IsFilesFilter(filter)) {
      matchAllFiles_ = true;
    } else if (config::IsFoldersFilter(filter)) {
      matchFolders_ = true;
    } else {
      for (auto& ext : str::SplitTrim(filter.extensions, L',')) {
        allowedExt_.insert(std::move(ext));
      }
    }
  }
}

bool PanelWindow::MatchesFilters(const FileEntry& entry) const {
  if (entry.isDirectory) {
    return matchFolders_;
  }
  if (matchAllFiles_) {
    return true;
  }
  if (allowedExt_.empty()) {
    return false;
  }
  const std::wstring ext = path_util::ExtensionLower(entry.name);
  return !ext.empty() && allowedExt_.count(ext) != 0;
}

// ---------------------------------------------------------------------------
// Metrics

UINT PanelWindow::CurrentDpi() const {
  if (hwnd_) {
    return dpi::ForWindow(hwnd_);
  }
  POINT pt = {};
  GetCursorPos(&pt);
  return dpi::ForPoint(pt);
}

int PanelWindow::Px(int value96) const {
  return dpi::Scale(value96, CurrentDpi());
}

int PanelWindow::PanelWidthPx() const {
  return Px(kPanelWidth96[config::SizeIndex(settings_.panelSize)]);
}

int PanelWindow::RowHeightPx() const {
  if (listView_) {
    RECT itemRc = {};
    if (ListView_GetItemRect(listView_, 0, &itemRc, LVIR_BOUNDS)) {
      const int h = itemRc.bottom - itemRc.top;
      if (h > 0) {
        return h;
      }
    }
  }
  return Px(kRowHeight96[config::SizeIndex(settings_.fontSize)]);
}

int PanelWindow::PinSizePx() const {
  return Px(kPinSize96);
}

RECT PanelWindow::PinRect() const {
  RECT rc = {};
  if (hwnd_) {
    GetClientRect(hwnd_, &rc);
  }
  const int pad = Px(kPanelPadding96);
  const int size = PinSizePx();
  RECT pin = {};
  pin.right = rc.right - pad;
  pin.left = pin.right - size;
  pin.top = pad;
  pin.bottom = pad + size;
  return pin;
}

int PanelWindow::TopChromeHeightPx() const {
  const int pinRow = PinSizePx() + Px(kFilterListGap96);
  if (!HasFilterBar()) {
    return pinRow;
  }
  UpdateChipLayout();
  return (std::max)(chipBarHeight_, pinRow);
}

int PanelWindow::PanelHeightPx(const RECT& workArea) const {
  const int margin = Px(kPanelMargin96);
  const int pad = Px(kPanelPadding96);
  const int maxH = (workArea.bottom - workArea.top) - margin * 2;
  const int minH = Px(kPanelMinHeight96);
  const int rows = items_.empty() ? 1 : static_cast<int>(items_.size());
  const int h = pad * 2 + TopChromeHeightPx() + RowHeightPx() * rows;
  return (std::min)((std::max)(h, minH), maxH);
}

// ---------------------------------------------------------------------------
// Child windows, fonts, theme

void PanelWindow::EnsureFont() {
  const UINT dpi = CurrentDpi();
  if (listFont_ && fontDpi_ == dpi && fontSizeBuilt_ == settings_.fontSize) {
    return;
  }
  for (HFONT* font : {&listFont_, &pinFont_}) {
    if (*font) {
      DeleteObject(*font);
      *font = nullptr;
    }
  }
  listFont_ = dpi::CreateUiFont(dpi, settings_.fontSize);

  LOGFONTW lf = {};
  lf.lfHeight = -(dpi::Scale(kPinSize96, dpi) - dpi::Scale(2, dpi));
  lf.lfWeight = FW_NORMAL;
  lf.lfCharSet = DEFAULT_CHARSET;
  lf.lfQuality = CLEARTYPE_QUALITY;
  wcscpy_s(lf.lfFaceName, L"Segoe MDL2 Assets");
  pinFont_ = CreateFontIndirectW(&lf);

  fontDpi_ = dpi;
  fontSizeBuilt_ = settings_.fontSize;
  chipLayoutDirty_ = true;
  if (listView_ && listFont_) {
    SendMessageW(listView_, WM_SETFONT, reinterpret_cast<WPARAM>(listFont_), TRUE);
  }
}

void PanelWindow::EnsureList() {
  INITCOMMONCONTROLSEX icc = {};
  icc.dwSize = sizeof(icc);
  icc.dwICC = ICC_LISTVIEW_CLASSES;
  InitCommonControlsEx(&icc);

  listView_ = CreateWindowExW(
      0, WC_LISTVIEWW, L"",
      WS_CHILD | WS_VISIBLE | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS |
          LVS_NOCOLUMNHEADER | LVS_OWNERDATA,
      0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_FILE_LIST)),
      GetModuleHandleW(nullptr), nullptr);

  // Double-buffer + no hscroll; INFOTIP works with OWNERDATA (LABELTIP does not).
  ListView_SetExtendedListViewStyle(listView_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER |
                                                   LVS_EX_INFOTIP | LVS_EX_NOHSCROLL);

  LVCOLUMNW col = {};
  col.mask = LVCF_WIDTH | LVCF_SUBITEM;
  col.cx = 100;
  col.iSubItem = 0;
  ListView_InsertColumn(listView_, 0, &col);

  EnsureListIcons();
  EnsureListInfoTipTopMost();

  if (listFont_) {
    SendMessageW(listView_, WM_SETFONT, reinterpret_cast<WPARAM>(listFont_), TRUE);
  }
}

void PanelWindow::EnsureListInfoTipTopMost() {
  if (!listView_) {
    return;
  }
  // Panel is WS_EX_TOPMOST; the ListView's own tooltip is not, so it draws underneath.
  HWND tip = ListView_GetToolTips(listView_);
  if (!tip) {
    tip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                          WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
                          CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, GetModuleHandleW(nullptr),
                          nullptr);
    if (!tip) {
      return;
    }
    ListView_SetToolTips(listView_, tip);
  }
  SetWindowPos(tip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  // Keep tip above the topmost panel after each show/activate.
  LONG_PTR ex = GetWindowLongPtrW(tip, GWL_EXSTYLE);
  if ((ex & WS_EX_TOPMOST) == 0) {
    SetWindowLongPtrW(tip, GWL_EXSTYLE, ex | WS_EX_TOPMOST);
  }
}

void PanelWindow::ApplyTheme() {
  if (!hwnd_) {
    return;
  }

  appsLight_ = theme::AppsUseLightTheme();
  const bool dark = !appsLight_;

  theme::AllowDarkForWindow(hwnd_, dark);
  theme::ApplyImmersiveDarkFrame(hwnd_, dark);

  if (bgBrush_) {
    DeleteObject(bgBrush_);
    bgBrush_ = nullptr;
  }

  COLORREF bk = GetSysColor(COLOR_WINDOW);
  COLORREF text = GetSysColor(COLOR_WINDOWTEXT);
  if (dark) {
    bk = RGB(32, 32, 32);
    text = RGB(240, 240, 240);
  }
  bgBrush_ = CreateSolidBrush(bk);

  if (listView_) {
    theme::AllowDarkForWindow(listView_, dark);
    SetWindowTheme(listView_, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    ListView_SetBkColor(listView_, bk);
    ListView_SetTextBkColor(listView_, bk);
    ListView_SetTextColor(listView_, text);
    InvalidateRect(listView_, nullptr, TRUE);
  }

  InvalidateRect(hwnd_, nullptr, TRUE);
}

void PanelWindow::RefreshTheme() {
  if (!created_) {
    return;
  }
  const bool light = theme::AppsUseLightTheme();
  if (light == appsLight_ && bgBrush_) {
    return;
  }
  ApplyTheme();
}

void PanelWindow::EnsureListIcons() {
  if (!listView_) {
    return;
  }

  // Return value is the shell system image list — do not ImageList_Destroy it.
  SHFILEINFOW sfi = {};
  HIMAGELIST himl = reinterpret_cast<HIMAGELIST>(
      SHGetFileInfoW(L"C:\\", 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON));
  if (himl) {
    ListView_SetImageList(listView_, himl, LVSIL_SMALL);
  }
}

void PanelWindow::EnsureTooltip() {
  if (tooltip_ || !hwnd_) {
    return;
  }
  tooltip_ = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                             WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT,
                             CW_USEDEFAULT, CW_USEDEFAULT, hwnd_, nullptr, GetModuleHandleW(nullptr),
                             nullptr);
  if (!tooltip_) {
    return;
  }
  SetWindowPos(tooltip_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  SendMessageW(tooltip_, TTM_SETMAXTIPWIDTH, 0, 240);

  TOOLINFOW ti = {};
  ti.cbSize = sizeof(ti);
  ti.uFlags = TTF_SUBCLASS;
  ti.hwnd = hwnd_;
  ti.uId = kPinTooltipId;
  ti.rect = PinRect();
  ti.lpszText = const_cast<wchar_t*>(lang::T(lang::Id::TipPin));
  SendMessageW(tooltip_, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
}

void PanelWindow::UpdatePinTooltip() {
  if (!tooltip_ || !hwnd_) {
    return;
  }
  TOOLINFOW ti = {};
  ti.cbSize = sizeof(ti);
  ti.hwnd = hwnd_;
  ti.uId = kPinTooltipId;
  ti.rect = PinRect();
  ti.lpszText = const_cast<wchar_t*>(lang::T(pinned_ ? lang::Id::TipUnpin : lang::Id::TipPin));
  SendMessageW(tooltip_, TTM_SETTOOLINFOW, 0, reinterpret_cast<LPARAM>(&ti));
}

// ---------------------------------------------------------------------------
// Layout

void PanelWindow::UpdateColumnWidth() {
  if (!listView_) {
    return;
  }
  // The client rect already excludes a visible vertical scrollbar.
  RECT rc = {};
  GetClientRect(listView_, &rc);
  const int width = rc.right - rc.left;
  if (width <= 0) {
    return;
  }

  // The Explorer theme draws a column divider on the column's last pixel; one extra pixel
  // pushes it just past the client edge. LVS_EX_NOHSCROLL keeps that from adding a scrollbar.
  ListView_SetColumnWidth(listView_, 0, width + 1);
  ShowScrollBar(listView_, SB_HORZ, FALSE);
}

void PanelWindow::Layout() {
  if (!hwnd_ || !listView_) {
    return;
  }
  RECT rc = {};
  GetClientRect(hwnd_, &rc);
  const int pad = Px(kPanelPadding96);
  const int chromeH = TopChromeHeightPx();
  const int w = (std::max)(static_cast<int>(rc.right - rc.left) - pad * 2, 1);
  const int h = (std::max)(static_cast<int>(rc.bottom - rc.top) - pad * 2 - chromeH, 1);
  SetWindowPos(listView_, nullptr, pad, pad + chromeH, w, h, SWP_NOZORDER);
  UpdateColumnWidth();
  UpdatePinTooltip();
  RECT chromeRc = {0, 0, rc.right, pad + chromeH};
  InvalidateRect(hwnd_, &chromeRc, TRUE);
}

void PanelWindow::ApplyVisibleSize(bool allowShrink) {
  if (!IsVisible()) {
    return;
  }

  const RECT wa = ui_util::WorkAreaAtCursor();
  const int margin = Px(kPanelMargin96);
  const int width = PanelWidthPx();
  int height = PanelHeightPx(wa);
  if (!allowShrink && retainedHeightPx_ > height) {
    height = retainedHeightPx_;
  }
  retainedHeightPx_ = height;

  const int x = wa.right - width - margin;
  const int y = wa.bottom - margin - height;
  SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_NOCOPYBITS);
  Layout();
  EnsureSelection();
}

// ---------------------------------------------------------------------------
// Filter chips and pin

bool PanelWindow::HasFilterBar() const {
  return settings_.showFilters && !settings_.filters.empty();
}

void PanelWindow::UpdateChipLayout() const {
  RECT rc = {};
  if (hwnd_) {
    GetClientRect(hwnd_, &rc);
  }
  const UINT dpi = CurrentDpi();
  if (!chipLayoutDirty_ && chipLayoutWidth_ == rc.right && chipLayoutDpi_ == dpi) {
    return;
  }
  chipLayoutDirty_ = false;
  chipLayoutWidth_ = rc.right;
  chipLayoutDpi_ = dpi;
  chipRects_.clear();
  chipBarHeight_ = 0;
  if (!HasFilterBar() || !hwnd_) {
    return;
  }

  const int pad = dpi::Scale(kPanelPadding96, dpi);
  const int chipH = dpi::Scale(kChipHeight96[config::SizeIndex(settings_.fontSize)], dpi);
  const int chipPadX = dpi::Scale(kChipPadX96, dpi);
  const int gapX = dpi::Scale(kChipGapX96, dpi);
  const int gapY = dpi::Scale(kChipGapY96, dpi);
  const int pinReserve = dpi::Scale(kPinSize96, dpi) + dpi::Scale(kPinGap96, dpi);
  const int maxRight = rc.right - pad - pinReserve;
  const int minChipW = dpi::Scale(kChipMinWidth96, dpi);

  HDC hdc = GetDC(hwnd_);
  HGDIOBJ oldFont = listFont_ ? SelectObject(hdc, listFont_) : nullptr;

  int x = pad;
  int y = pad;
  int lastBottom = pad + chipH;
  for (const auto& filter : settings_.filters) {
    SIZE sz = {};
    GetTextExtentPoint32W(hdc, filter.name.c_str(), static_cast<int>(filter.name.size()), &sz);
    int chipW = (std::max)(static_cast<int>(sz.cx) + chipPadX * 2, minChipW);
    // Wrap if this chip would overflow (keep at least one chip per row).
    if (x > pad && x + chipW > maxRight) {
      x = pad;
      y += chipH + gapY;
    }
    // Cap width on very narrow panels so a single long name still fits.
    chipW = (std::max)((std::min)(chipW, maxRight - pad), 1);
    const RECT chip = {x, y, x + chipW, y + chipH};
    chipRects_.push_back(chip);
    lastBottom = chip.bottom;
    x += chipW + gapX;
  }

  if (oldFont) {
    SelectObject(hdc, oldFont);
  }
  ReleaseDC(hwnd_, hdc);

  // Height of chip block below the top padding, plus gap before the list.
  chipBarHeight_ = (lastBottom - pad) + dpi::Scale(kFilterListGap96, dpi);
}

bool PanelWindow::IsFilterActive(const std::wstring& name) const {
  for (const auto& active : settings_.activeFilters) {
    if (_wcsicmp(active.c_str(), name.c_str()) == 0) {
      return true;
    }
  }
  return false;
}

void PanelWindow::ToggleFilterChip(int index) {
  if (index < 0 || index >= static_cast<int>(settings_.filters.size())) {
    return;
  }
  const std::wstring& name = settings_.filters[index].name;
  auto& active = settings_.activeFilters;
  const auto it = std::find_if(active.begin(), active.end(), [&name](const std::wstring& a) {
    return _wcsicmp(a.c_str(), name.c_str()) == 0;
  });
  if (it != active.end()) {
    active.erase(it);
  } else {
    active.push_back(name);
  }
  RebuildFilterMatcher();
  PopulateList();
  ApplyVisibleSize(false);
  InvalidateRect(hwnd_, nullptr, TRUE);
  if (owner_) {
    PostMessageW(owner_, WM_FILTERS_CHANGED, 0, 0);
  }
}

void PanelWindow::TogglePin() {
  pinned_ = !pinned_;
  UpdatePinTooltip();
  const RECT pin = PinRect();
  InvalidateRect(hwnd_, &pin, TRUE);
}

bool PanelWindow::HitTestPin(POINT clientPt) const {
  const RECT pin = PinRect();
  return PtInRect(&pin, clientPt) != FALSE;
}

int PanelWindow::HitTestFilterChip(POINT clientPt) const {
  if (!HasFilterBar()) {
    return -1;
  }
  UpdateChipLayout();
  for (size_t i = 0; i < chipRects_.size(); ++i) {
    if (PtInRect(&chipRects_[i], clientPt)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void PanelWindow::PaintFilterBar(HDC hdc) {
  if (!HasFilterBar()) {
    return;
  }
  UpdateChipLayout();
  const bool dark = !appsLight_;
  const COLORREF textIdle = dark ? RGB(160, 160, 160) : RGB(110, 110, 110);
  const COLORREF textActive = dark ? RGB(240, 240, 240) : RGB(20, 20, 20);
  const int underline = Px(2);
  const int inset = Px(2);

  HGDIOBJ oldFont = listFont_ ? SelectObject(hdc, listFont_) : nullptr;
  SetBkMode(hdc, TRANSPARENT);
  HPEN accentPen = CreatePen(PS_SOLID, underline, GetSysColor(COLOR_HOTLIGHT));

  for (size_t i = 0; i < chipRects_.size() && i < settings_.filters.size(); ++i) {
    RECT r = chipRects_[i];
    const bool on = IsFilterActive(settings_.filters[i].name);
    SetTextColor(hdc, on ? textActive : textIdle);
    DrawTextW(hdc, settings_.filters[i].name.c_str(), -1, &r,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

    if (on && accentPen) {
      HGDIOBJ oldPen = SelectObject(hdc, accentPen);
      const int y = r.bottom - underline;
      MoveToEx(hdc, r.left + inset, y, nullptr);
      LineTo(hdc, r.right - inset, y);
      SelectObject(hdc, oldPen);
    }
  }

  if (accentPen) {
    DeleteObject(accentPen);
  }
  if (oldFont) {
    SelectObject(hdc, oldFont);
  }
}

void PanelWindow::PaintPin(HDC hdc) {
  RECT pin = PinRect();
  const bool dark = !appsLight_;
  const COLORREF color =
      pinned_ ? GetSysColor(COLOR_HOTLIGHT) : (dark ? RGB(200, 200, 200) : RGB(80, 80, 80));

  bool drewGlyph = false;
  if (pinFont_) {
    HGDIOBJ oldFont = SelectObject(hdc, pinFont_);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    const wchar_t* glyph = pinned_ ? kPinSolidGlyph : kPinGlyph;
    drewGlyph =
        DrawTextW(hdc, glyph, -1, &pin, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX) > 0;
    SelectObject(hdc, oldFont);
  }
  if (drewGlyph) {
    return;
  }

  // Fallback geometric pin if MDL2 font is unavailable.
  const int cx = (pin.left + pin.right) / 2;
  const int cy = (pin.top + pin.bottom) / 2;
  const int r = Px(4);
  const int lift = Px(2);
  HPEN pen = CreatePen(PS_SOLID, Px(2), color);
  HBRUSH brush =
      pinned_ ? CreateSolidBrush(color) : static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
  HGDIOBJ oldPen = SelectObject(hdc, pen);
  HGDIOBJ oldBrush = SelectObject(hdc, brush);
  Ellipse(hdc, cx - r, cy - r - lift, cx + r, cy + r - lift);
  MoveToEx(hdc, cx, cy + r - lift, nullptr);
  LineTo(hdc, cx, pin.bottom - lift);
  SelectObject(hdc, oldPen);
  SelectObject(hdc, oldBrush);
  DeleteObject(pen);
  if (pinned_) {
    DeleteObject(brush);
  }
}

// ---------------------------------------------------------------------------
// Listing and navigation

bool PanelWindow::IsAtWatchRoot() const {
  if (settings_.watchPath.empty() || browsePath_.empty()) {
    return true;
  }
  return path_util::EqualsNoCase(browsePath_, settings_.watchPath);
}

bool PanelWindow::IsParentRow(int index) const {
  const FileEntry* entry = ItemAt(index);
  return entry && entry->name == L"..";
}

const FileEntry* PanelWindow::ItemAt(int index) const {
  if (index < 0 || index >= static_cast<int>(items_.size())) {
    return nullptr;
  }
  return &items_[index];
}

const FileEntry* PanelWindow::ActionableItemAt(int index) const {
  return IsParentRow(index) ? nullptr : ItemAt(index);
}

void PanelWindow::NavigateTo(const std::wstring& path) {
  if (!DirectoryWatcher::PathExists(path) ||
      !path_util::IsSameOrUnder(path, settings_.watchPath)) {
    return;
  }
  browsePath_ = path;
  PopulateList();
  // Entering another folder: grow if needed, never shrink.
  ApplyVisibleSize(false);
}

void PanelWindow::NavigateUp() {
  if (!settings_.expandFolders || IsAtWatchRoot()) {
    return;
  }
  const std::wstring parent = path_util::Parent(browsePath_);
  browsePath_ =
      path_util::IsSameOrUnder(parent, settings_.watchPath) ? parent : settings_.watchPath;
  PopulateList();
  ApplyVisibleSize(false);
}

void PanelWindow::PopulateList() {
  if (!listView_) {
    return;
  }

  if (browsePath_.empty() || !DirectoryWatcher::PathExists(browsePath_)) {
    browsePath_ = settings_.watchPath;
  }

  const int top = ListView_GetTopIndex(listView_);
  items_ = DirectoryWatcher::Enumerate(browsePath_, settings_.sortMode, settings_.showHidden);

  if (filteringActive_) {
    items_.erase(std::remove_if(items_.begin(), items_.end(),
                                [this](const FileEntry& e) { return !MatchesFilters(e); }),
                 items_.end());
  }

  if (settings_.expandFolders && !IsAtWatchRoot()) {
    FileEntry up;
    up.name = L"..";
    up.fullPath = path_util::Parent(browsePath_);
    up.isDirectory = true;
    up.iconIndex = DirectoryWatcher::IconIndexFor(up.fullPath, true);
    items_.insert(items_.begin(), std::move(up));
  }

  if (items_.empty()) {
    // Resolved here rather than in LVN_GETDISPINFO, which runs on every repaint.
    if (settings_.watchPath.empty()) {
      emptyTextId_ = lang::Id::ChooseFolderHint;
    } else if (!DirectoryWatcher::PathExists(settings_.watchPath)) {
      emptyTextId_ = lang::Id::PathInvalid;
    } else {
      emptyTextId_ = lang::Id::EmptyFolder;
    }
  }

  // Highlight watch-root entries that appeared since the user last closed the panel.
  const bool markNew = newFileTracking_ && IsAtWatchRoot();
  for (FileEntry& entry : items_) {
    if (entry.name == L"..") {
      entry.isNew = false;
      continue;
    }
    entry.isNew =
        markNew && seenPaths_.find(str::ToLower(entry.fullPath)) == seenPaths_.end();
  }

  // Virtual list: text comes from LVN_GETDISPINFO (smooth with many files)
  const int count = items_.empty() ? 1 : static_cast<int>(items_.size());  // 1 = placeholder
  ListView_SetItemCountEx(listView_, count, LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL);

  UpdateColumnWidth();

  if (top > 0 && top < static_cast<int>(items_.size())) {
    ListView_EnsureVisible(listView_, top, FALSE);
  }
  InvalidateRect(listView_, nullptr, FALSE);
}

void PanelWindow::MarkListedPathsSeen() {
  for (const FileEntry& entry : items_) {
    if (entry.name == L".." || entry.fullPath.empty()) {
      continue;
    }
    seenPaths_.insert(str::ToLower(entry.fullPath));
  }
}

void PanelWindow::Refresh() {
  PopulateList();
  // Watcher/config refresh may grow for new files, but keep size when list shrinks.
  ApplyVisibleSize(false);
}

// ---------------------------------------------------------------------------
// Show / hide

void PanelWindow::Show() {
  if (!hwnd_) {
    return;
  }

  ApplyWin11Chrome(hwnd_);
  SetWindowRgn(hwnd_, nullptr, FALSE);
  retainedHeightPx_ = 0;

  const RECT wa = ui_util::WorkAreaAtCursor();
  const int margin = Px(kPanelMargin96);
  const int width = PanelWidthPx();
  const int toX = wa.right - width - margin;
  // Fully past the right edge — same size as final, translate only.
  const int fromX = wa.right + Px(kOffscreenGap96);

  // Size list tall enough to measure a row, populate, then shrink to content.
  const int maxH = (wa.bottom - wa.top) - margin * 2;
  SetWindowPos(hwnd_, HWND_TOPMOST, toX, wa.bottom - margin - maxH, width, maxH,
               SWP_NOACTIVATE | SWP_HIDEWINDOW);
  // After the move the window has the target monitor's DPI; rebuilds only if it changed.
  EnsureFont();
  Layout();
  PopulateList();
  const int height = PanelHeightPx(wa);
  retainedHeightPx_ = height;
  const int y = wa.bottom - margin - height;
  SetWindowPos(hwnd_, HWND_TOPMOST, toX, y, width, height, SWP_NOACTIVATE | SWP_HIDEWINDOW);
  Layout();

  slide_anim::SlideIn(hwnd_, fromX, toX, y);
  EnsureListInfoTipTopMost();
  EnsureSelection();
  // First open: treat the current listing as already seen so only later arrivals highlight.
  if (!newFileTracking_) {
    MarkListedPathsSeen();
    newFileTracking_ = true;
  }
  SetForegroundWindow(hwnd_);
  if (listView_) {
    SetFocus(listView_);
  }
}

void PanelWindow::Hide() {
  if (!IsVisible()) {
    return;
  }

  // Absorb highlighted rows so they no longer count as new next time.
  MarkListedPathsSeen();

  // Avoid re-entrancy from WM_ACTIVATE(WA_INACTIVE) while sliding out.
  suppressDeactivateHide_ = true;
  ApplyWin11Chrome(hwnd_);

  RECT rc = {};
  GetWindowRect(hwnd_, &rc);
  const RECT wa = ui_util::WorkAreaAtCursor();
  slide_anim::SlideOut(hwnd_, rc.left, wa.right + Px(kOffscreenGap96), rc.top);
  ShowWindow(hwnd_, SW_HIDE);
  retainedHeightPx_ = 0;
  suppressDeactivateHide_ = false;
}

void PanelWindow::RequestHide() {
  if (owner_) {
    PostMessageW(owner_, WM_REQUEST_HIDE, 0, 0);
  } else {
    Hide();
  }
}

void PanelWindow::HideUnlessPinned() {
  if (!pinned_) {
    RequestHide();
  }
}

bool PanelWindow::PanelIsForeground() const {
  const HWND fg = GetForegroundWindow();
  return fg == hwnd_ || (fg != nullptr && IsChild(hwnd_, fg));
}

// ---------------------------------------------------------------------------
// Selection and hit testing

int PanelWindow::HitTestItem(POINT clientPt) const {
  if (!listView_ || items_.empty()) {
    return -1;
  }
  LVHITTESTINFO hit = {};
  hit.pt = clientPt;
  const int index =
      static_cast<int>(SendMessageW(listView_, LVM_HITTEST, 0, reinterpret_cast<LPARAM>(&hit)));
  if (!ItemAt(index) || (hit.flags & LVHT_ONITEM) == 0) {
    return -1;
  }
  return index;
}

int PanelWindow::SelectedIndex() const {
  if (!listView_) {
    return -1;
  }
  const int index = ListView_GetNextItem(listView_, -1, LVNI_SELECTED);
  return ItemAt(index) ? index : -1;
}

int PanelWindow::FocusedIndex() const {
  if (!listView_) {
    return -1;
  }
  const int index = ListView_GetNextItem(listView_, -1, LVNI_FOCUSED);
  return ItemAt(index) ? index : SelectedIndex();
}

bool PanelWindow::IsItemSelected(int index) const {
  if (!listView_ || !ItemAt(index)) {
    return false;
  }
  return (ListView_GetItemState(listView_, index, LVIS_SELECTED) & LVIS_SELECTED) != 0;
}

void PanelWindow::SelectAll() {
  if (!listView_ || items_.empty()) {
    return;
  }
  ListView_SetItemState(listView_, -1, LVIS_SELECTED, LVIS_SELECTED);
}

std::vector<int> PanelWindow::SelectedActionable() const {
  std::vector<int> indices;
  if (!listView_) {
    return indices;
  }
  for (int index = -1; (index = ListView_GetNextItem(listView_, index, LVNI_SELECTED)) != -1;) {
    if (ActionableItemAt(index)) {
      indices.push_back(index);
    }
  }
  return indices;
}

std::vector<int> PanelWindow::ActionTargets(int index) const {
  if (IsItemSelected(index)) {
    const std::vector<int> selected = SelectedActionable();
    if (!selected.empty()) {
      return selected;
    }
  }
  if (ActionableItemAt(index)) {
    return {index};
  }
  return {};
}

void PanelWindow::EnsureSelection() {
  if (!listView_ || items_.empty() || SelectedIndex() >= 0) {
    return;
  }
  SelectOnly(listView_, 0);
  ListView_EnsureVisible(listView_, 0, FALSE);
}

// ---------------------------------------------------------------------------
// Item actions

void PanelWindow::OpenAt(int index) {
  const FileEntry* entry = ItemAt(index);
  if (!entry) {
    return;
  }
  // Expanding a folder navigates this panel; do not also open the rest of the selection.
  if (settings_.expandFolders && entry->isDirectory) {
    if (IsParentRow(index)) {
      NavigateUp();
    } else {
      NavigateTo(entry->fullPath);
    }
    return;
  }

  const std::vector<int> targets = ActionTargets(index);
  if (targets.empty()) {
    return;
  }
  for (int target : targets) {
    const FileEntry* item = ActionableItemAt(target);
    if (item) {
      ShellExecuteW(hwnd_, L"open", item->fullPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
  }
  HideUnlessPinned();
}

void PanelWindow::OpenLocationAt(int index) {
  const FileEntry* entry = ActionableItemAt(index);
  if (!entry) {
    return;
  }

  PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(entry->fullPath.c_str());
  if (pidl) {
    SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
    ILFree(pidl);
    HideUnlessPinned();
    return;
  }

  // Fallback: open parent folder
  const size_t pos = entry->fullPath.find_last_of(L"\\/");
  if (pos != std::wstring::npos) {
    const std::wstring parent = entry->fullPath.substr(0, pos);
    ShellExecuteW(hwnd_, L"open", parent.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    HideUnlessPinned();
  }
}

bool PanelWindow::DeleteItems(const std::vector<int>& indices) {
  // Double-null-terminated path list for SHFileOperation.
  std::wstring from;
  for (int index : indices) {
    const FileEntry* entry = ActionableItemAt(index);
    if (!entry) {
      continue;
    }
    from += entry->fullPath;
    from.push_back(L'\0');
  }
  if (from.empty()) {
    return false;
  }
  from.push_back(L'\0');

  SHFILEOPSTRUCTW op = {};
  op.hwnd = hwnd_;
  op.wFunc = FO_DELETE;
  op.pFrom = from.c_str();
  op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
  const int result = SHFileOperationW(&op);
  if (result != 0 || op.fAnyOperationsAborted) {
    MessageBoxW(hwnd_, lang::T(lang::Id::DeleteFailed), L"ToFolder", MB_ICONERROR | MB_OK);
    return false;
  }

  Refresh();
  return true;
}

void PanelWindow::BeginDragAt(int index) {
  const std::vector<int> targets = ActionTargets(index);
  std::vector<PCIDLIST_ABSOLUTE> pidls;
  pidls.reserve(targets.size());
  for (int target : targets) {
    const FileEntry* entry = ActionableItemAt(target);
    if (!entry) {
      continue;
    }
    if (PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(entry->fullPath.c_str())) {
      pidls.push_back(pidl);
    }
  }
  if (pidls.empty()) {
    return;
  }

  IShellItemArray* items = nullptr;
  HRESULT hr = SHCreateShellItemArrayFromIDLists(static_cast<UINT>(pidls.size()), pidls.data(),
                                                 &items);
  for (PCIDLIST_ABSOLUTE pidl : pidls) {
    ILFree(const_cast<PIDLIST_ABSOLUTE>(pidl));
  }
  if (FAILED(hr) || !items) {
    return;
  }

  IDataObject* dataObj = nullptr;
  hr = items->BindToHandler(nullptr, BHID_DataObject, IID_PPV_ARGS(&dataObj));
  items->Release();
  if (FAILED(hr) || !dataObj) {
    return;
  }

  IDropSource* dropSource = new (std::nothrow) SimpleDropSource(dataObj);
  if (!dropSource) {
    dataObj->Release();
    return;
  }

  // Drag activates other windows; don't treat that as an outside dismiss.
  suppressDeactivateHide_ = true;
  DWORD effect = DROPEFFECT_NONE;
  DoDragDrop(dataObj, dropSource, DROPEFFECT_MOVE | DROPEFFECT_COPY, &effect);
  dropSource->Release();
  dataObj->Release();
  suppressDeactivateHide_ = false;

  if (!PanelIsForeground() && !pinned_) {
    RequestHide();
  } else if (listView_) {
    SetFocus(listView_);
  }

  Refresh();
}

void PanelWindow::ShowItemContextMenu(int index, POINT screenPt) {
  if (!ActionableItemAt(index)) {
    return;
  }
  // Right-click on an unselected row replaces the selection; a selected row keeps the group.
  if (!IsItemSelected(index)) {
    SelectOnly(listView_, index);
  }
  const std::vector<int> targets = ActionTargets(index);
  if (targets.empty()) {
    return;
  }

  HMENU menu = CreatePopupMenu();
  for (const MenuEntry& item : kItemMenu) {
    if (item.id == 0) {
      AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    } else {
      AppendMenuW(menu, MF_STRING, item.id, lang::T(item.text));
    }
  }

  // Popup menus deactivate the owner; don't treat that as an outside dismiss.
  suppressDeactivateHide_ = true;
  theme::PreparePopupMenu(hwnd_);
  SetForegroundWindow(hwnd_);
  const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                                  screenPt.x, screenPt.y, 0, hwnd_, nullptr);
  DestroyMenu(menu);
  suppressDeactivateHide_ = false;

  // Clicking outside dismisses the menu and activates another window. That WA_INACTIVE
  // was suppressed above — if we steal focus back, later outside clicks never deactivate
  // us again and the panel gets stuck open.
  if (cmd == 0) {
    if (!PanelIsForeground()) {
      HideUnlessPinned();
    } else if (listView_) {
      SetFocus(listView_);
    }
    return;
  }

  if (!PanelIsForeground()) {
    SetForegroundWindow(hwnd_);
  }
  if (listView_) {
    SetFocus(listView_);
  }

  std::vector<std::wstring> paths;
  std::wstring names;
  std::wstring fullPaths;
  paths.reserve(targets.size());
  for (int target : targets) {
    const FileEntry* item = ActionableItemAt(target);
    if (!item) {
      continue;
    }
    paths.push_back(item->fullPath);
    if (!names.empty()) {
      names += L"\r\n";
      fullPaths += L"\r\n";
    }
    names += item->name;
    fullPaths += item->fullPath;
  }

  switch (cmd) {
    case IDM_LIST_OPEN:
      OpenAt(index);
      break;
    case IDM_LIST_OPEN_LOCATION:
      OpenLocationAt(index);
      break;
    case IDM_LIST_CUT:
      clipboard::CopyFileItems(hwnd_, paths, true);
      break;
    case IDM_LIST_COPY:
      clipboard::CopyFileItems(hwnd_, paths, false);
      break;
    case IDM_LIST_COPY_NAME:
      clipboard::CopyText(hwnd_, names);
      break;
    case IDM_LIST_COPY_PATH:
      clipboard::CopyText(hwnd_, fullPaths);
      break;
    case IDM_LIST_DELETE:
      DeleteItems(targets);
      break;
  }
}

// ---------------------------------------------------------------------------
// Message handling

LRESULT CALLBACK PanelWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  PanelWindow* self = nullptr;
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
    self = static_cast<PanelWindow*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  } else {
    self = reinterpret_cast<PanelWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }
  if (!self) {
    return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
  return self->HandleMessage(msg, wParam, lParam);
}

bool PanelWindow::HandleNavigationKey(UINT vk) {
  switch (vk) {
    case 'A':
      if (GetKeyState(VK_CONTROL) < 0) {
        SelectAll();
        return true;
      }
      return false;
    case VK_RETURN:
      OpenAt(FocusedIndex());
      return true;
    case VK_BACK:
      NavigateUp();
      return true;
    case VK_ESCAPE:
      RequestHide();
      return true;
    default:
      return false;
  }
}

void PanelWindow::OnGetDispInfo(NMLVDISPINFOW* di) const {
  const int index = di->item.iItem;
  const FileEntry* entry = ItemAt(index);

  if (di->item.mask & LVIF_IMAGE) {
    di->item.iImage = entry ? entry->iconIndex : I_IMAGENONE;
  }

  if ((di->item.mask & LVIF_TEXT) && di->item.pszText && di->item.cchTextMax > 0) {
    wchar_t* out = di->item.pszText;
    const int cap = di->item.cchTextMax;
    out[0] = L'\0';
    if (items_.empty()) {
      wcsncpy_s(out, cap, lang::T(emptyTextId_), _TRUNCATE);
    } else if (entry) {
      wcsncpy_s(out, cap, entry->name.c_str(), _TRUNCATE);
      if (entry->isDirectory && !IsParentRow(index)) {
        wcsncat_s(out, cap, L"\\", _TRUNCATE);
      }
    }
  }
}

void PanelWindow::OnGetInfoTip(NMLVGETINFOTIPW* tip) const {
  if (!tip->pszText || tip->cchTextMax <= 0) {
    return;
  }
  tip->pszText[0] = L'\0';
  const FileEntry* entry = ItemAt(tip->iItem);
  if (!entry) {
    return;
  }
  const std::wstring name = DisplayName(*entry, IsParentRow(tip->iItem));

  // Only show tip when the label is truncated in the column.
  RECT labelRc = {};
  if (!ListView_GetItemRect(listView_, tip->iItem, &labelRc, LVIR_LABEL)) {
    return;
  }
  HDC hdc = GetDC(listView_);
  if (!hdc) {
    return;
  }
  HGDIOBJ oldFont = listFont_ ? SelectObject(hdc, listFont_) : nullptr;
  SIZE sz = {};
  GetTextExtentPoint32W(hdc, name.c_str(), static_cast<int>(name.size()), &sz);
  if (oldFont) {
    SelectObject(hdc, oldFont);
  }
  ReleaseDC(listView_, hdc);

  if (sz.cx > labelRc.right - labelRc.left) {
    wcsncpy_s(tip->pszText, tip->cchTextMax, name.c_str(), _TRUNCATE);
  }
}

bool PanelWindow::OnListCustomDraw(NMLVCUSTOMDRAW* cd, LRESULT* result) {
  switch (cd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
      *result = CDRF_NOTIFYITEMDRAW;
      return true;
    case CDDS_ITEMPREPAINT: {
      const FileEntry* entry = ItemAt(static_cast<int>(cd->nmcd.dwItemSpec));
      if (entry && entry->isNew) {
        // Soft amber: readable in both Themes without competing with selection.
        cd->clrTextBk = appsLight_ ? RGB(255, 244, 196) : RGB(72, 62, 28);
      }
      *result = CDRF_DODEFAULT;
      return true;
    }
    default:
      return false;
  }
}

bool PanelWindow::OnNotify(const NMHDR* nmhdr, LRESULT* result) {
  if (nmhdr->idFrom != IDC_FILE_LIST) {
    return false;
  }
  *result = 0;
  switch (nmhdr->code) {
    case LVN_KEYDOWN:
      return HandleNavigationKey(reinterpret_cast<const NMLVKEYDOWN*>(nmhdr)->wVKey);
    case LVN_GETDISPINFOW:
      OnGetDispInfo(reinterpret_cast<NMLVDISPINFOW*>(const_cast<NMHDR*>(nmhdr)));
      return true;
    case LVN_GETINFOTIPW:
      OnGetInfoTip(reinterpret_cast<NMLVGETINFOTIPW*>(const_cast<NMHDR*>(nmhdr)));
      return true;
    case LVN_BEGINDRAG:
      BeginDragAt(reinterpret_cast<const NMLISTVIEW*>(nmhdr)->iItem);
      return true;
    case NM_CUSTOMDRAW:
      return OnListCustomDraw(reinterpret_cast<NMLVCUSTOMDRAW*>(const_cast<NMHDR*>(nmhdr)),
                              result);
    case NM_DBLCLK:
      OpenAt(reinterpret_cast<const NMITEMACTIVATE*>(nmhdr)->iItem);
      return true;
    default:
      return false;
  }
}

bool PanelWindow::OnContextMenu(HWND source, POINT pt) {
  // Handle only WM_CONTEXTMENU (not also NM_RCLICK) so the menu is shown once and closes cleanly.
  if (source != listView_ && source != hwnd_ && GetParent(source) != hwnd_) {
    return false;
  }
  int index = -1;
  if (pt.x == -1 && pt.y == -1) {
    // Keyboard invocation: anchor below the selected row.
    index = ListView_GetNextItem(listView_, -1, LVNI_SELECTED);
    RECT rc = {};
    if (index >= 0 && ListView_GetItemRect(listView_, index, &rc, LVIR_BOUNDS)) {
      pt.x = rc.left;
      pt.y = rc.bottom;
      ClientToScreen(listView_, &pt);
    } else {
      GetCursorPos(&pt);
    }
  } else {
    POINT client = pt;
    ScreenToClient(listView_, &client);
    index = HitTestItem(client);
  }
  ShowItemContextMenu(index, pt);
  return true;
}

LRESULT PanelWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_ERASEBKGND: {
      HDC hdc = reinterpret_cast<HDC>(wParam);
      RECT rc = {};
      GetClientRect(hwnd_, &rc);
      HBRUSH brush = bgBrush_ ? bgBrush_ : reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
      FillRect(hdc, &rc, brush);
      return 1;
    }
    case WM_PAINT: {
      PAINTSTRUCT ps = {};
      HDC hdc = BeginPaint(hwnd_, &ps);
      PaintFilterBar(hdc);
      PaintPin(hdc);
      EndPaint(hwnd_, &ps);
      return 0;
    }
    case WM_LBUTTONDOWN: {
      const POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      if (HitTestPin(pt)) {
        TogglePin();
        return 0;
      }
      const int chip = HitTestFilterChip(pt);
      if (chip >= 0) {
        ToggleFilterChip(chip);
        return 0;
      }
      break;
    }
    case WM_DPICHANGED: {
      const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
      EnsureFont();
      EnsureListIcons();
      if (listView_) {
        InvalidateRect(listView_, nullptr, TRUE);
      }
      if (IsVisible() && suggested) {
        SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        Layout();
      }
      return 0;
    }
    case WM_SIZE:
      Layout();
      return 0;
    case WM_ACTIVATE:
      if (LOWORD(wParam) == WA_INACTIVE && !suppressDeactivateHide_) {
        HideUnlessPinned();
      }
      return 0;
    case WM_KEYDOWN:
      if (HandleNavigationKey(static_cast<UINT>(wParam))) {
        return 0;
      }
      break;
    case WM_NOTIFY: {
      LRESULT result = 0;
      if (OnNotify(reinterpret_cast<const NMHDR*>(lParam), &result)) {
        return result;
      }
      break;
    }
    case WM_CONTEXTMENU:
      if (OnContextMenu(reinterpret_cast<HWND>(wParam),
                        POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)})) {
        return 0;
      }
      break;
    case WM_CLOSE:
      RequestHide();
      return 0;
    case WM_DESTROY:
      hwnd_ = nullptr;
      return 0;
  }
  return DefWindowProcW(hwnd_, msg, wParam, lParam);
}
