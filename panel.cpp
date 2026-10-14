#include "panel.h"

#include "app_messages.h"
#include "dpi.h"
#include "lang.h"
#include "resource.h"
#include "theme.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <oleidl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <uxtheme.h>
#include <windowsx.h>

#include <cmath>
#include <new>
#include <string>
#include <unordered_set>
#include <vector>

#ifndef LVS_EX_NOHSCROLL
#define LVS_EX_NOHSCROLL 0x10000000
#endif

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_DEFAULT 0
#define DWMWCP_DONOTROUND 1
#define DWMWCP_ROUND 2
#define DWMWCP_ROUNDSMALL 3
#endif
#ifndef DWMWA_TRANSITIONS_FORCEDISABLED
#define DWMWA_TRANSITIONS_FORCEDISABLED 3
#endif

namespace {

constexpr int kPanelWidth96 = 360;
constexpr int kPanelMargin96 = 12;
constexpr int kPanelPadding96 = 12;
constexpr int kRowHeight96 = 28;
constexpr int kChipHeight96 = 20;
constexpr int kChipPadX96 = 6;
constexpr int kChipGapX96 = 10;
constexpr int kChipGapY96 = 6;
constexpr int kFilterListGap96 = 6;  // space between chip block and list
constexpr int kPanelMinHeight96 = 160;
constexpr int kSlideMs = 160;

std::wstring ExtLower(const std::wstring& fileName) {
  const size_t dot = fileName.find_last_of(L'.');
  if (dot == std::wstring::npos || dot + 1 >= fileName.size()) {
    return {};
  }
  std::wstring ext = fileName.substr(dot + 1);
  for (wchar_t& ch : ext) {
    ch = static_cast<wchar_t>(towlower(ch));
  }
  return ext;
}

std::unordered_set<std::wstring> SplitExtSet(const std::wstring& extensions) {
  std::unordered_set<std::wstring> set;
  size_t start = 0;
  while (start <= extensions.size()) {
    size_t comma = extensions.find(L',', start);
    if (comma == std::wstring::npos) {
      comma = extensions.size();
    }
    std::wstring part = extensions.substr(start, comma - start);
    size_t b = 0;
    while (b < part.size() && iswspace(part[b])) {
      ++b;
    }
    size_t e = part.size();
    while (e > b && iswspace(part[e - 1])) {
      --e;
    }
    if (e > b) {
      set.insert(part.substr(b, e - b));
    }
    if (comma >= extensions.size()) {
      break;
    }
    start = comma + 1;
  }
  return set;
}

class SimpleDropSource final : public IDropSource {
 public:
  SimpleDropSource() = default;

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
    if (!ppv) {
      return E_POINTER;
    }
    if (riid == IID_IUnknown || riid == IID_IDropSource) {
      *ppv = static_cast<IDropSource*>(this);
      AddRef();
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override {
    return static_cast<ULONG>(InterlockedIncrement(&ref_));
  }

  ULONG STDMETHODCALLTYPE Release() override {
    const LONG ref = InterlockedDecrement(&ref_);
    if (ref == 0) {
      delete this;
    }
    return static_cast<ULONG>(ref);
  }

  HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escapePressed, DWORD keyState) override {
    if (escapePressed) {
      return DRAGDROP_S_CANCEL;
    }
    if ((keyState & MK_LBUTTON) == 0) {
      return DRAGDROP_S_DROP;
    }
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD /*dwEffect*/) override {
    return DRAGDROP_S_USEDEFAULTCURSORS;
  }

 private:
  LONG ref_ = 1;
};

void ApplyWin11Chrome(HWND hwnd) {
  const DWORD corner = DWMWCP_ROUND;
  DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

  // Prevent DWM from adding its own show/hide morph (looks like scaling).
  const BOOL noTransition = TRUE;
  DwmSetWindowAttribute(hwnd, DWMWA_TRANSITIONS_FORCEDISABLED, &noTransition, sizeof(noTransition));
}

constexpr DWORD kMoveFlags = SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER;

// Windows toast–like: decelerate into place / accelerate away — no overshoot.
float EaseOutCubic(float t) {
  const float u = 1.0f - t;
  return 1.0f - u * u * u;
}

float EaseInCubic(float t) {
  return t * t * t;
}

void RunSlide(HWND hwnd, int fromX, int toX, int y, float (*ease)(float)) {
  int steps = kSlideMs * 60 / 1000;
  if (steps < 16) {
    steps = 16;
  }

  int lastX = fromX;
  for (int i = 1; i <= steps; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(steps);
    const float e = ease(t);
    const int x =
        fromX + static_cast<int>(std::lround(static_cast<double>(toX - fromX) * e));
    if (x != lastX) {
      SetWindowPos(hwnd, nullptr, x, y, 0, 0, kMoveFlags);
      lastX = x;
      DwmFlush();
    }
  }
  if (lastX != toX) {
    SetWindowPos(hwnd, nullptr, toX, y, 0, 0, kMoveFlags);
    DwmFlush();
  }
}

void SetAlpha(HWND hwnd, BYTE alpha) {
  SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
}

// Off-screen windows are not composed with content (only an empty DWM frame).
// Paint at the final rect while fully transparent, jump off-screen, then fade in
// and translate — size never changes.
void SlideIn(HWND hwnd, int fromX, int toX, int y) {
  const LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
  SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);
  SetWindowPos(
      hwnd,
      nullptr,
      0,
      0,
      0,
      0,
      SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
  // Alpha 0: on-screen paint without a visible flash (off-screen paint is skipped by DWM).
  SetAlpha(hwnd, 0);

  SetWindowPos(
      hwnd,
      nullptr,
      toX,
      y,
      0,
      0,
      kMoveFlags | SWP_SHOWWINDOW);
  RedrawWindow(
      hwnd,
      nullptr,
      nullptr,
      RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_ERASE);
  DwmFlush();

  // Content is in the redirection surface; jump off-screen while still invisible.
  SetWindowPos(hwnd, nullptr, fromX, y, 0, 0, kMoveFlags);
  SetAlpha(hwnd, 255);
  DwmFlush();

  RunSlide(hwnd, fromX, toX, y, EaseOutCubic);
}

void SlideOut(HWND hwnd, int fromX, int toX, int y) {
  SetWindowPos(hwnd, nullptr, fromX, y, 0, 0, kMoveFlags | SWP_SHOWWINDOW);
  RunSlide(hwnd, fromX, toX, y, EaseInCubic);
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
  wc.lpszClassName = L"ToFolderPanelClass";
  RegisterClassExW(&wc);

  const RECT wa = WorkAreaRect();
  hwnd_ = CreateWindowExW(
      WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
      L"ToFolderPanelClass",
      L"ToFolder",
      WS_POPUP | WS_CLIPCHILDREN,
      wa.right,
      wa.top,
      PanelWidthPx(),
      wa.bottom - wa.top,
      owner,
      nullptr,
      GetModuleHandleW(nullptr),
      this);

  if (!hwnd_) {
    return false;
  }

  ApplyWin11Chrome(hwnd_);

  created_ = true;
  EnsureFont();
  EnsureList();
  ApplyTheme();
  Layout();
  return true;
}

void PanelWindow::Destroy() {
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
  listView_ = nullptr;
  if (listFont_) {
    DeleteObject(listFont_);
    listFont_ = nullptr;
  }
  if (bgBrush_) {
    DeleteObject(bgBrush_);
    bgBrush_ = nullptr;
  }
  created_ = false;
}

bool PanelWindow::IsVisible() const {
  return hwnd_ && IsWindowVisible(hwnd_);
}

UINT PanelWindow::CurrentDpi() const {
  if (hwnd_) {
    return dpi::ForWindow(hwnd_);
  }
  POINT pt = {};
  GetCursorPos(&pt);
  return dpi::ForPoint(pt);
}

int PanelWindow::PanelWidthPx() const {
  return dpi::Scale(kPanelWidth96, CurrentDpi());
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
  return dpi::Scale(kRowHeight96, CurrentDpi());
}

bool PanelWindow::HasFilterBar() const {
  return showFilters_ && !filters_.empty();
}

int PanelWindow::FilterBarHeightPx() const {
  if (!HasFilterBar()) {
    return 0;
  }
  int barH = 0;
  ComputeFilterChipLayout(nullptr, &barH);
  return barH;
}

int PanelWindow::PanelHeightPx(const RECT& workArea) const {
  const int margin = dpi::Scale(kPanelMargin96, CurrentDpi());
  const int pad = dpi::Scale(kPanelPadding96, CurrentDpi());
  const int maxH = (workArea.bottom - workArea.top) - margin * 2;
  const int minH = dpi::Scale(kPanelMinHeight96, CurrentDpi());
  const int rows = items_.empty() ? 1 : static_cast<int>(items_.size());
  int h = pad * 2 + FilterBarHeightPx() + RowHeightPx() * rows;
  if (h < minH) {
    h = minH;
  }
  if (h > maxH) {
    h = maxH;
  }
  return h;
}

RECT PanelWindow::WorkAreaRect() const {
  POINT pt = {};
  GetCursorPos(&pt);
  HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  if (GetMonitorInfoW(mon, &mi)) {
    return mi.rcWork;
  }
  RECT wa = {};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
  return wa;
}

void PanelWindow::EnsureFont() {
  if (listFont_) {
    DeleteObject(listFont_);
    listFont_ = nullptr;
  }
  listFont_ = dpi::CreateUiFont(CurrentDpi());
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
      0,
      WC_LISTVIEWW,
      L"",
      WS_CHILD | WS_VISIBLE | WS_VSCROLL | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS |
          LVS_NOCOLUMNHEADER | LVS_OWNERDATA,
      0,
      0,
      0,
      0,
      hwnd_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_FILE_LIST)),
      GetModuleHandleW(nullptr),
      nullptr);

  // Double-buffer + no hscroll for smooth scrolling without horizontal bar
  ListView_SetExtendedListViewStyle(listView_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER |
                                                   LVS_EX_LABELTIP | LVS_EX_NOHSCROLL);

  LVCOLUMNW col = {};
  col.mask = LVCF_WIDTH | LVCF_SUBITEM;
  col.cx = 100;
  col.iSubItem = 0;
  ListView_InsertColumn(listView_, 0, &col);

  EnsureListIcons();

  if (listFont_) {
    SendMessageW(listView_, WM_SETFONT, reinterpret_cast<WPARAM>(listFont_), TRUE);
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
  HIMAGELIST himl = reinterpret_cast<HIMAGELIST>(SHGetFileInfoW(
      L"C:\\",
      0,
      &sfi,
      sizeof(sfi),
      SHGFI_SYSICONINDEX | SHGFI_SMALLICON));
  if (himl) {
    ListView_SetImageList(listView_, himl, LVSIL_SMALL);
  }
}

void PanelWindow::UpdateColumnWidth() {
  if (!listView_) {
    return;
  }
  RECT rc = {};
  GetClientRect(listView_, &rc);
  int width = rc.right - rc.left;
  if (width <= 0) {
    return;
  }

  // Subtract vertical scrollbar width so the column fits and no HSCROLL appears
  SCROLLBARINFO sbi = {};
  sbi.cbSize = sizeof(sbi);
  if (GetScrollBarInfo(listView_, OBJID_VSCROLL, &sbi) &&
      (sbi.rgstate[0] & STATE_SYSTEM_INVISIBLE) == 0) {
    width -= (sbi.rcScrollBar.right - sbi.rcScrollBar.left);
  }

  if (width < 1) {
    width = 1;
  }
  ListView_SetColumnWidth(listView_, 0, width);
  ShowScrollBar(listView_, SB_HORZ, FALSE);
}

void PanelWindow::ComputeFilterChipLayout(std::vector<RECT>* outRects, int* outBarH) const {
  if (outRects) {
    outRects->clear();
  }
  if (outBarH) {
    *outBarH = 0;
  }
  if (!HasFilterBar() || !hwnd_ || filters_.empty()) {
    return;
  }

  RECT rc = {};
  GetClientRect(hwnd_, &rc);
  const UINT dpi = CurrentDpi();
  const int pad = dpi::Scale(kPanelPadding96, dpi);
  const int chipH = dpi::Scale(kChipHeight96, dpi);
  const int chipPadX = dpi::Scale(kChipPadX96, dpi);
  const int gapX = dpi::Scale(kChipGapX96, dpi);
  const int gapY = dpi::Scale(kChipGapY96, dpi);
  const int listGap = dpi::Scale(kFilterListGap96, dpi);
  const int maxRight = rc.right - pad;
  const int minChipW = dpi::Scale(28, dpi);

  HDC hdc = GetDC(hwnd_);
  HFONT oldFont = nullptr;
  if (listFont_) {
    oldFont = static_cast<HFONT>(SelectObject(hdc, listFont_));
  }

  int x = pad;
  int y = pad;
  int lastBottom = pad + chipH;

  for (const auto& filter : filters_) {
    SIZE sz = {};
    GetTextExtentPoint32W(hdc, filter.name.c_str(), static_cast<int>(filter.name.size()), &sz);
    int chipW = sz.cx + chipPadX * 2;
    if (chipW < minChipW) {
      chipW = minChipW;
    }
    // Wrap if this chip would overflow (keep at least one chip per row).
    if (x > pad && x + chipW > maxRight) {
      x = pad;
      y += chipH + gapY;
    }
    // Cap width on very narrow panels so a single long name still fits.
    if (chipW > maxRight - pad) {
      chipW = maxRight - pad;
      if (chipW < 1) {
        chipW = 1;
      }
    }
    RECT chip = {x, y, x + chipW, y + chipH};
    if (outRects) {
      outRects->push_back(chip);
    }
    lastBottom = chip.bottom;
    x += chipW + gapX;
  }

  if (oldFont) {
    SelectObject(hdc, oldFont);
  }
  ReleaseDC(hwnd_, hdc);

  if (outBarH) {
    // Height of chip block below the top padding, plus gap before the list.
    *outBarH = (lastBottom - pad) + listGap;
  }
}

void PanelWindow::LayoutFilterChips() {
  filterChipRects_.clear();
  if (!HasFilterBar()) {
    return;
  }
  ComputeFilterChipLayout(&filterChipRects_, nullptr);
}

void PanelWindow::Layout() {
  if (!hwnd_ || !listView_) {
    return;
  }
  RECT rc = {};
  GetClientRect(hwnd_, &rc);
  const int pad = dpi::Scale(kPanelPadding96, CurrentDpi());
  const int barH = FilterBarHeightPx();
  LayoutFilterChips();
  int x = pad;
  int y = pad + barH;
  int w = (rc.right - rc.left) - pad * 2;
  int h = (rc.bottom - rc.top) - pad * 2 - barH;
  if (w < 1) {
    w = 1;
  }
  if (h < 1) {
    h = 1;
  }
  SetWindowPos(listView_, nullptr, x, y, w, h, SWP_NOZORDER);
  UpdateColumnWidth();
  if (HasFilterBar()) {
    RECT chipRc = {0, 0, rc.right, pad + barH};
    InvalidateRect(hwnd_, &chipRc, TRUE);
  }
}

void PanelWindow::SetWatchPath(const std::wstring& path) {
  watchPath_ = path;
  browsePath_ = path;
}

void PanelWindow::SetSortMode(config::SortMode mode) {
  sortMode_ = mode;
}

void PanelWindow::SetShowHidden(bool showHidden) {
  showHidden_ = showHidden;
}

void PanelWindow::SetExpandFolders(bool expandFolders) {
  expandFolders_ = expandFolders;
  if (!expandFolders_ && browsePath_ != watchPath_) {
    browsePath_ = watchPath_;
    if (IsVisible()) {
      Refresh();
    }
  }
}

void PanelWindow::SetShowFilters(bool showFilters) {
  showFilters_ = showFilters;
}

void PanelWindow::SetFilters(const std::vector<config::FilterItem>& filters) {
  filters_ = filters;
  config::PruneActiveFilters(filters_, activeFilters_);
}

void PanelWindow::SetActiveFilters(const std::vector<std::wstring>& activeFilters) {
  activeFilters_ = activeFilters;
  config::PruneActiveFilters(filters_, activeFilters_);
}

bool PanelWindow::IsFilterActive(const std::wstring& name) const {
  for (const auto& active : activeFilters_) {
    if (_wcsicmp(active.c_str(), name.c_str()) == 0) {
      return true;
    }
  }
  return false;
}

void PanelWindow::NotifyFiltersChanged() {
  if (owner_) {
    PostMessageW(owner_, WM_FILTERS_CHANGED, 0, 0);
  }
}

void PanelWindow::ToggleFilterChip(int index) {
  if (index < 0 || index >= static_cast<int>(filters_.size())) {
    return;
  }
  const std::wstring& name = filters_[index].name;
  bool found = false;
  for (auto it = activeFilters_.begin(); it != activeFilters_.end(); ++it) {
    if (_wcsicmp(it->c_str(), name.c_str()) == 0) {
      activeFilters_.erase(it);
      found = true;
      break;
    }
  }
  if (!found) {
    activeFilters_.push_back(name);
  }
  PopulateList();
  ApplyVisibleSize(false);
  InvalidateRect(hwnd_, nullptr, TRUE);
  NotifyFiltersChanged();
}

int PanelWindow::HitTestFilterChip(POINT clientPt) const {
  for (size_t i = 0; i < filterChipRects_.size(); ++i) {
    if (PtInRect(&filterChipRects_[i], clientPt)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void PanelWindow::PaintFilterBar(HDC hdc) {
  if (!HasFilterBar()) {
    return;
  }
  LayoutFilterChips();
  const bool dark = !appsLight_;
  const COLORREF textIdle = dark ? RGB(160, 160, 160) : RGB(110, 110, 110);
  const COLORREF textActive = dark ? RGB(240, 240, 240) : RGB(20, 20, 20);
  const COLORREF accent = GetSysColor(COLOR_HOTLIGHT);
  const int underline = dpi::Scale(2, CurrentDpi());

  HFONT oldFont = nullptr;
  if (listFont_) {
    oldFont = static_cast<HFONT>(SelectObject(hdc, listFont_));
  }
  SetBkMode(hdc, TRANSPARENT);

  for (size_t i = 0; i < filterChipRects_.size() && i < filters_.size(); ++i) {
    const RECT& r = filterChipRects_[i];
    const bool on = IsFilterActive(filters_[i].name);
    SetTextColor(hdc, on ? textActive : textIdle);
    DrawTextW(hdc, filters_[i].name.c_str(), -1, const_cast<RECT*>(&r),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

    if (on) {
      HPEN pen = CreatePen(PS_SOLID, underline, accent);
      HGDIOBJ oldPen = SelectObject(hdc, pen);
      const int y = r.bottom - underline;
      const int inset = dpi::Scale(2, CurrentDpi());
      MoveToEx(hdc, r.left + inset, y, nullptr);
      LineTo(hdc, r.right - inset, y);
      SelectObject(hdc, oldPen);
      DeleteObject(pen);
    }
  }

  if (oldFont) {
    SelectObject(hdc, oldFont);
  }
}

bool PanelWindow::IsAtWatchRoot() const {
  if (watchPath_.empty() || browsePath_.empty()) {
    return true;
  }
  return _wcsicmp(browsePath_.c_str(), watchPath_.c_str()) == 0;
}

std::wstring PanelWindow::ParentPath(const std::wstring& path) const {
  if (path.empty()) {
    return path;
  }
  std::wstring p = path;
  while (!p.empty() && (p.back() == L'\\' || p.back() == L'/')) {
    p.pop_back();
  }
  const size_t pos = p.find_last_of(L"\\/");
  if (pos == std::wstring::npos) {
    return p;
  }
  if (pos == 2 && p.size() >= 3 && p[1] == L':') {
    // "C:\foo" -> "C:\"
    return p.substr(0, 3);
  }
  return p.substr(0, pos);
}

bool PanelWindow::IsPathUnderWatch(const std::wstring& path) const {
  if (watchPath_.empty() || path.empty()) {
    return false;
  }
  if (_wcsicmp(path.c_str(), watchPath_.c_str()) == 0) {
    return true;
  }
  std::wstring root = watchPath_;
  while (!root.empty() && (root.back() == L'\\' || root.back() == L'/')) {
    root.pop_back();
  }
  if (path.size() <= root.size()) {
    return false;
  }
  if (_wcsnicmp(path.c_str(), root.c_str(), root.size()) != 0) {
    return false;
  }
  const wchar_t sep = path[root.size()];
  return sep == L'\\' || sep == L'/';
}

bool PanelWindow::IsParentRow(int index) const {
  return index >= 0 && index < static_cast<int>(items_.size()) && items_[index].name == L"..";
}

void PanelWindow::ApplyVisibleSize(bool allowShrink) {
  if (!hwnd_ || !IsWindowVisible(hwnd_)) {
    return;
  }

  const RECT wa = WorkAreaRect();
  const int margin = dpi::Scale(kPanelMargin96, CurrentDpi());
  const int width = PanelWidthPx();
  int height = PanelHeightPx(wa);
  if (!allowShrink && retainedHeightPx_ > height) {
    height = retainedHeightPx_;
  }
  retainedHeightPx_ = height;

  const int x = wa.right - width - margin;
  const int y = wa.bottom - margin - height;
  SetWindowPos(
      hwnd_,
      HWND_TOPMOST,
      x,
      y,
      width,
      height,
      SWP_NOACTIVATE | SWP_NOCOPYBITS);
  Layout();
  EnsureSelection();
}

void PanelWindow::NavigateTo(const std::wstring& path) {
  if (path.empty() || !DirectoryWatcher::PathExists(path) || !IsPathUnderWatch(path)) {
    return;
  }
  browsePath_ = path;
  PopulateList();
  // Entering another folder: grow if needed, never shrink.
  ApplyVisibleSize(false);
}

void PanelWindow::NavigateUp() {
  if (!expandFolders_ || IsAtWatchRoot()) {
    return;
  }
  const std::wstring parent = ParentPath(browsePath_);
  if (!IsPathUnderWatch(parent)) {
    browsePath_ = watchPath_;
  } else {
    browsePath_ = parent;
  }
  PopulateList();
  ApplyVisibleSize(false);
}

void PanelWindow::PopulateList() {
  if (!listView_) {
    return;
  }

  if (browsePath_.empty()) {
    browsePath_ = watchPath_;
  }
  if (!browsePath_.empty() && !DirectoryWatcher::PathExists(browsePath_)) {
    browsePath_ = watchPath_;
  }

  const int top = ListView_GetTopIndex(listView_);
  items_ = DirectoryWatcher::Enumerate(browsePath_, sortMode_, showHidden_);

  if (showFilters_ && !activeFilters_.empty()) {
    bool matchAllFiles = false;
    bool matchFolders = false;
    std::unordered_set<std::wstring> allowedExt;
    for (const auto& filter : filters_) {
      if (!IsFilterActive(filter.name)) {
        continue;
      }
      if (config::IsFilesFilter(filter)) {
        matchAllFiles = true;
      } else if (config::IsFoldersFilter(filter)) {
        matchFolders = true;
      } else {
        auto part = SplitExtSet(filter.extensions);
        allowedExt.insert(part.begin(), part.end());
      }
    }
    std::vector<FileEntry> filtered;
    filtered.reserve(items_.size());
    for (auto& entry : items_) {
      bool keep = false;
      if (entry.isDirectory) {
        keep = matchFolders;
      } else if (matchAllFiles) {
        keep = true;
      } else if (!allowedExt.empty()) {
        const std::wstring ext = ExtLower(entry.name);
        keep = !ext.empty() && allowedExt.find(ext) != allowedExt.end();
      }
      if (keep) {
        filtered.push_back(std::move(entry));
      }
    }
    items_ = std::move(filtered);
  }

  if (expandFolders_ && !IsAtWatchRoot()) {
    FileEntry up;
    up.name = L"..";
    up.fullPath = ParentPath(browsePath_);
    up.isDirectory = true;
    up.iconIndex = I_IMAGENONE;
    SHFILEINFOW sfi = {};
    if (SHGetFileInfoW(
            up.fullPath.c_str(),
            FILE_ATTRIBUTE_DIRECTORY,
            &sfi,
            sizeof(sfi),
            SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)) {
      up.iconIndex = sfi.iIcon;
    }
    items_.insert(items_.begin(), std::move(up));
  }

  // Virtual list: text comes from LVN_GETDISPINFO (smooth with many files)
  int count = static_cast<int>(items_.size());
  if (count == 0) {
    count = 1;  // placeholder row
  }
  ListView_SetItemCountEx(listView_, count, LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL);

  UpdateColumnWidth();

  if (top > 0 && !items_.empty() && top < static_cast<int>(items_.size())) {
    ListView_EnsureVisible(listView_, top, FALSE);
  }
  InvalidateRect(listView_, nullptr, FALSE);
}

void PanelWindow::Refresh() {
  PopulateList();
  // Watcher/config refresh may grow for new files, but keep size when list shrinks.
  // ApplyVisibleSize -> Layout (filter chips measured once there).
  ApplyVisibleSize(false);
}

void PanelWindow::Show() {
  if (!hwnd_) {
    return;
  }

  EnsureFont();
  ApplyWin11Chrome(hwnd_);
  SetWindowRgn(hwnd_, nullptr, FALSE);
  retainedHeightPx_ = 0;

  const RECT wa = WorkAreaRect();
  const int margin = dpi::Scale(kPanelMargin96, CurrentDpi());
  const int width = PanelWidthPx();
  const int toX = wa.right - width - margin;
  // Fully past the right edge (clear of DWM shadow) — same size as final, translate only.
  const int fromX = wa.right + dpi::Scale(32, CurrentDpi());

  // Size list tall enough to measure a row, populate, then shrink to content.
  const int maxH = (wa.bottom - wa.top) - margin * 2;
  SetWindowPos(
      hwnd_,
      HWND_TOPMOST,
      toX,
      wa.bottom - margin - maxH,
      width,
      maxH,
      SWP_NOACTIVATE | SWP_HIDEWINDOW);
  Layout();
  PopulateList();
  const int height = PanelHeightPx(wa);
  retainedHeightPx_ = height;
  const int y = wa.bottom - margin - height;
  SetWindowPos(
      hwnd_,
      HWND_TOPMOST,
      toX,
      y,
      width,
      height,
      SWP_NOACTIVATE | SWP_HIDEWINDOW);
  Layout();
  if (listView_) {
    UpdateColumnWidth();
  }

  SlideIn(hwnd_, fromX, toX, y);
  EnsureSelection();
  SetForegroundWindow(hwnd_);
  if (listView_) {
    SetFocus(listView_);
  }
}

void PanelWindow::Hide() {
  if (!hwnd_ || !IsWindowVisible(hwnd_)) {
    return;
  }

  // Avoid re-entrancy from WM_ACTIVATE(WA_INACTIVE) while sliding out.
  suppressDeactivateHide_ = true;
  ApplyWin11Chrome(hwnd_);

  RECT rc = {};
  GetWindowRect(hwnd_, &rc);
  const int y = rc.top;
  const int fromX = rc.left;
  const RECT wa = WorkAreaRect();
  const int toX = wa.right + dpi::Scale(32, CurrentDpi());

  SlideOut(hwnd_, fromX, toX, y);
  ShowWindow(hwnd_, SW_HIDE);
  retainedHeightPx_ = 0;
  suppressDeactivateHide_ = false;
}

void PanelWindow::Toggle() {
  if (IsVisible()) {
    Hide();
  } else {
    Show();
  }
}

bool PanelWindow::ContainsPoint(POINT pt) const {
  if (!hwnd_ || !IsWindowVisible(hwnd_)) {
    return false;
  }
  RECT rc = {};
  GetWindowRect(hwnd_, &rc);
  return PtInRect(&rc, pt) != FALSE;
}

int PanelWindow::HitTestItem(POINT clientPt) const {
  if (!listView_ || items_.empty()) {
    return -1;
  }
  LVHITTESTINFO hit = {};
  hit.pt = clientPt;
  const int index =
      static_cast<int>(SendMessageW(listView_, LVM_HITTEST, 0, reinterpret_cast<LPARAM>(&hit)));
  if (index < 0 || index >= static_cast<int>(items_.size())) {
    return -1;
  }
  if ((hit.flags & LVHT_ONITEM) == 0) {
    return -1;
  }
  return index;
}

int PanelWindow::SelectedIndex() const {
  if (!listView_ || items_.empty()) {
    return -1;
  }
  const int index = ListView_GetNextItem(listView_, -1, LVNI_SELECTED);
  if (index < 0 || index >= static_cast<int>(items_.size())) {
    return -1;
  }
  return index;
}

void PanelWindow::EnsureSelection() {
  if (!listView_ || items_.empty()) {
    return;
  }
  int index = SelectedIndex();
  if (index < 0) {
    index = 0;
  }
  ListView_SetItemState(listView_, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
  ListView_SetItemState(listView_, index, LVIS_SELECTED | LVIS_FOCUSED,
                        LVIS_SELECTED | LVIS_FOCUSED);
  ListView_EnsureVisible(listView_, index, FALSE);
}

void PanelWindow::OpenAt(int index) {
  // Placeholder rows have no real items_
  if (items_.empty()) {
    return;
  }
  if (index < 0 || index >= static_cast<int>(items_.size())) {
    return;
  }

  const FileEntry& entry = items_[index];
  if (expandFolders_ && entry.isDirectory) {
    if (IsParentRow(index)) {
      NavigateUp();
    } else {
      NavigateTo(entry.fullPath);
    }
    return;
  }

  ShellExecuteW(hwnd_, L"open", entry.fullPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
  if (owner_) {
    PostMessageW(owner_, WM_REQUEST_HIDE, 0, 0);
  }
}

void PanelWindow::OpenLocationAt(int index) {
  if (items_.empty() || index < 0 || index >= static_cast<int>(items_.size())) {
    return;
  }
  if (IsParentRow(index)) {
    return;
  }

  PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(items_[index].fullPath.c_str());
  if (pidl) {
    SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
    ILFree(pidl);
    if (owner_) {
      PostMessageW(owner_, WM_REQUEST_HIDE, 0, 0);
    }
    return;
  }

  // Fallback: open parent folder
  std::wstring path = items_[index].fullPath;
  const size_t pos = path.find_last_of(L"\\/");
  if (pos != std::wstring::npos) {
    path.resize(pos);
    ShellExecuteW(hwnd_, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (owner_) {
      PostMessageW(owner_, WM_REQUEST_HIDE, 0, 0);
    }
  }
}

bool PanelWindow::DeleteAt(int index) {
  if (items_.empty() || index < 0 || index >= static_cast<int>(items_.size())) {
    return false;
  }
  if (IsParentRow(index)) {
    return false;
  }

  const std::wstring& path = items_[index].fullPath;

  // Double-null-terminated path list for SHFileOperation
  std::wstring from = path;
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
  if (items_.empty() || index < 0 || index >= static_cast<int>(items_.size())) {
    return;
  }
  if (IsParentRow(index)) {
    return;
  }

  const std::wstring& path = items_[index].fullPath;
  IShellItem* item = nullptr;
  HRESULT hr = SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&item));
  if (FAILED(hr) || !item) {
    return;
  }

  IDataObject* dataObj = nullptr;
  hr = item->BindToHandler(nullptr, BHID_DataObject, IID_PPV_ARGS(&dataObj));
  item->Release();
  if (FAILED(hr) || !dataObj) {
    return;
  }

  IDropSource* dropSource = new (std::nothrow) SimpleDropSource();
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

  const HWND fg = GetForegroundWindow();
  const bool panelIsForeground = fg == hwnd_ || (fg != nullptr && IsChild(hwnd_, fg));
  if (!panelIsForeground) {
    if (owner_) {
      PostMessageW(owner_, WM_REQUEST_HIDE, 0, 0);
    } else {
      Hide();
    }
  } else if (listView_) {
    SetFocus(listView_);
  }

  Refresh();
}

void PanelWindow::ShowItemContextMenu(int index, POINT screenPt) {
  if (items_.empty() || index < 0 || index >= static_cast<int>(items_.size())) {
    return;
  }
  if (IsParentRow(index)) {
    return;
  }

  ListView_SetItemState(listView_, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
  ListView_SetItemState(listView_, index, LVIS_SELECTED | LVIS_FOCUSED,
                        LVIS_SELECTED | LVIS_FOCUSED);

  HMENU menu = CreatePopupMenu();
  AppendMenuW(menu, MF_STRING, IDM_LIST_OPEN, lang::T(lang::Id::ListOpen));
  AppendMenuW(menu, MF_STRING, IDM_LIST_OPEN_LOCATION, lang::T(lang::Id::ListOpenLocation));
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, IDM_LIST_DELETE, lang::T(lang::Id::ListDelete));

  // Popup menus deactivate the owner; don't treat that as an outside dismiss.
  suppressDeactivateHide_ = true;
  theme::PreparePopupMenu(hwnd_);
  SetForegroundWindow(hwnd_);
  const UINT cmd =
      TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, screenPt.x, screenPt.y,
                     0, hwnd_, nullptr);
  DestroyMenu(menu);
  suppressDeactivateHide_ = false;

  auto requestHide = [this]() {
    if (owner_) {
      PostMessageW(owner_, WM_REQUEST_HIDE, 0, 0);
    } else {
      Hide();
    }
  };

  auto panelIsForeground = [this]() -> bool {
    const HWND fg = GetForegroundWindow();
    return fg == hwnd_ || (fg != nullptr && IsChild(hwnd_, fg));
  };

  // Clicking outside dismisses the menu and activates another window. That WA_INACTIVE
  // was suppressed above — if we steal focus back, later outside clicks never deactivate
  // us again and the panel gets stuck open.
  if (cmd == 0) {
    if (!panelIsForeground()) {
      requestHide();
    } else if (listView_) {
      SetFocus(listView_);
    }
    return;
  }

  if (!panelIsForeground()) {
    SetForegroundWindow(hwnd_);
  }
  if (listView_) {
    SetFocus(listView_);
  }

  switch (cmd) {
    case IDM_LIST_OPEN:
      OpenAt(index);
      break;
    case IDM_LIST_OPEN_LOCATION:
      OpenLocationAt(index);
      break;
    case IDM_LIST_DELETE:
      DeleteAt(index);
      break;
  }
}

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
      EndPaint(hwnd_, &ps);
      return 0;
    }
    case WM_LBUTTONDOWN: {
      POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
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
        if (owner_) {
          PostMessageW(owner_, WM_REQUEST_HIDE, 0, 0);
        }
      }
      return 0;
    case WM_KEYDOWN:
      if (wParam == VK_ESCAPE) {
        if (owner_) {
          PostMessageW(owner_, WM_REQUEST_HIDE, 0, 0);
        } else {
          Hide();
        }
        return 0;
      }
      if (wParam == VK_BACK) {
        NavigateUp();
        return 0;
      }
      break;
    case WM_NOTIFY: {
      auto* nmhdr = reinterpret_cast<LPNMHDR>(lParam);
      if (nmhdr->idFrom != IDC_FILE_LIST) {
        break;
      }
      if (nmhdr->code == LVN_KEYDOWN) {
        const auto* kd = reinterpret_cast<LPNMLVKEYDOWN>(lParam);
        if (kd->wVKey == VK_RETURN) {
          OpenAt(SelectedIndex());
          return 0;
        }
        if (kd->wVKey == VK_BACK) {
          NavigateUp();
          return 0;
        }
        if (kd->wVKey == VK_ESCAPE) {
          if (owner_) {
            PostMessageW(owner_, WM_REQUEST_HIDE, 0, 0);
          } else {
            Hide();
          }
          return 0;
        }
        break;
      }
      if (nmhdr->code == LVN_GETDISPINFOW) {
        auto* di = reinterpret_cast<NMLVDISPINFOW*>(lParam);
        const int index = di->item.iItem;

        if (di->item.mask & LVIF_IMAGE) {
          if (items_.empty() || index < 0 || index >= static_cast<int>(items_.size())) {
            di->item.iImage = I_IMAGENONE;
          } else {
            di->item.iImage = items_[index].iconIndex;
          }
        }

        if ((di->item.mask & LVIF_TEXT) && di->item.pszText && di->item.cchTextMax > 0) {
          di->item.pszText[0] = L'\0';

          if (items_.empty()) {
            const wchar_t* msg = lang::T(lang::Id::EmptyFolder);
            if (watchPath_.empty()) {
              msg = lang::T(lang::Id::ChooseFolderHint);
            } else if (!DirectoryWatcher::PathExists(watchPath_)) {
              msg = lang::T(lang::Id::PathInvalid);
            }
            wcsncpy_s(di->item.pszText, di->item.cchTextMax, msg, _TRUNCATE);
          } else if (index >= 0 && index < static_cast<int>(items_.size())) {
            const FileEntry& e = items_[index];
            std::wstring name = e.name;
            if (e.isDirectory && e.name != L"..") {
              name += L"\\";
            }
            wcsncpy_s(di->item.pszText, di->item.cchTextMax, name.c_str(), _TRUNCATE);
          }
        }
        return 0;
      }
      if (nmhdr->code == LVN_BEGINDRAG) {
        const auto* lv = reinterpret_cast<LPNMLISTVIEW>(lParam);
        BeginDragAt(lv->iItem);
        return 0;
      }
      if (nmhdr->code == NM_DBLCLK) {
        auto* item = reinterpret_cast<LPNMITEMACTIVATE>(lParam);
        OpenAt(item->iItem);
        return 0;
      }
      break;
    }
    case WM_CONTEXTMENU: {
      // Handle only WM_CONTEXTMENU (not also NM_RCLICK) so the menu is shown once and closes cleanly.
      HWND src = reinterpret_cast<HWND>(wParam);
      if (src != listView_ && src != hwnd_ && GetParent(src) != hwnd_) {
        break;
      }
      POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      int index = -1;
      if (pt.x == -1 && pt.y == -1) {
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
      return 0;
    }
    case WM_CLOSE:
      if (owner_) {
        PostMessageW(owner_, WM_REQUEST_HIDE, 0, 0);
      } else {
        Hide();
      }
      return 0;
    case WM_DESTROY:
      hwnd_ = nullptr;
      return 0;
  }
  return DefWindowProcW(hwnd_, msg, wParam, lParam);
}
