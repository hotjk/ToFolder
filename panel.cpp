#include "panel.h"

#include "app_messages.h"
#include "dpi.h"
#include "resource.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windowsx.h>

#include <cmath>
#include <string>

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
constexpr int kPanelMinHeight96 = 160;
constexpr int kSlideMs = 160;

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
      WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
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

int PanelWindow::PanelHeightPx(const RECT& workArea) const {
  const int margin = dpi::Scale(kPanelMargin96, CurrentDpi());
  const int pad = dpi::Scale(kPanelPadding96, CurrentDpi());
  const int maxH = (workArea.bottom - workArea.top) - margin * 2;
  const int minH = dpi::Scale(kPanelMinHeight96, CurrentDpi());
  const int rows = items_.empty() ? 1 : static_cast<int>(items_.size());
  int h = pad * 2 + RowHeightPx() * rows;
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

  if (listFont_) {
    SendMessageW(listView_, WM_SETFONT, reinterpret_cast<WPARAM>(listFont_), TRUE);
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

void PanelWindow::Layout() {
  if (!hwnd_ || !listView_) {
    return;
  }
  RECT rc = {};
  GetClientRect(hwnd_, &rc);
  const int pad = dpi::Scale(kPanelPadding96, CurrentDpi());
  int x = pad;
  int y = pad;
  int w = (rc.right - rc.left) - pad * 2;
  int h = (rc.bottom - rc.top) - pad * 2;
  if (w < 1) {
    w = 1;
  }
  if (h < 1) {
    h = 1;
  }
  SetWindowPos(listView_, nullptr, x, y, w, h, SWP_NOZORDER);
  UpdateColumnWidth();
}

void PanelWindow::SetWatchPath(const std::wstring& path) {
  watchPath_ = path;
}

void PanelWindow::SetSortMode(config::SortMode mode) {
  sortMode_ = mode;
}

void PanelWindow::PopulateList() {
  if (!listView_) {
    return;
  }

  const int top = ListView_GetTopIndex(listView_);
  items_ = DirectoryWatcher::Enumerate(watchPath_, sortMode_);

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
  if (!hwnd_ || !IsWindowVisible(hwnd_)) {
    return;
  }

  const RECT wa = WorkAreaRect();
  const int margin = dpi::Scale(kPanelMargin96, CurrentDpi());
  const int width = PanelWidthPx();
  const int height = PanelHeightPx(wa);
  const int x = wa.right - width - margin;
  // Anchor to the bottom of the work area (toast-like); full-height still fits.
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
}

void PanelWindow::Show() {
  if (!hwnd_) {
    return;
  }

  EnsureFont();
  ApplyWin11Chrome(hwnd_);
  SetWindowRgn(hwnd_, nullptr, FALSE);

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
}

void PanelWindow::Hide() {
  if (!hwnd_ || !IsWindowVisible(hwnd_)) {
    return;
  }

  ApplyWin11Chrome(hwnd_);

  RECT rc = {};
  GetWindowRect(hwnd_, &rc);
  const int y = rc.top;
  const int fromX = rc.left;
  const RECT wa = WorkAreaRect();
  const int toX = wa.right + dpi::Scale(32, CurrentDpi());

  SlideOut(hwnd_, fromX, toX, y);
  ShowWindow(hwnd_, SW_HIDE);
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

void PanelWindow::OpenAt(int index) {
  // Placeholder rows have no real items_
  if (items_.empty()) {
    return;
  }
  if (index < 0 || index >= static_cast<int>(items_.size())) {
    return;
  }
  ShellExecuteW(hwnd_, L"open", items_[index].fullPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
  if (owner_) {
    PostMessageW(owner_, WM_REQUEST_HIDE, 0, 0);
  }
}

void PanelWindow::OpenLocationAt(int index) {
  if (items_.empty() || index < 0 || index >= static_cast<int>(items_.size())) {
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
    MessageBoxW(hwnd_, L"Failed to delete the item.", L"ToFolder", MB_ICONERROR | MB_OK);
    return false;
  }

  Refresh();
  return true;
}

void PanelWindow::ShowItemContextMenu(int index, POINT screenPt) {
  if (items_.empty() || index < 0 || index >= static_cast<int>(items_.size())) {
    return;
  }

  ListView_SetItemState(listView_, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
  ListView_SetItemState(listView_, index, LVIS_SELECTED | LVIS_FOCUSED,
                        LVIS_SELECTED | LVIS_FOCUSED);

  HMENU menu = CreatePopupMenu();
  AppendMenuW(menu, MF_STRING, IDM_LIST_OPEN, L"Open");
  AppendMenuW(menu, MF_STRING, IDM_LIST_OPEN_LOCATION, L"Open containing folder");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, IDM_LIST_DELETE, L"Delete");

  SetForegroundWindow(hwnd_);
  const UINT cmd =
      TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, screenPt.x, screenPt.y,
                     0, hwnd_, nullptr);
  DestroyMenu(menu);
  // Ensure the popup is visually dismissed before running the action.
  PostMessageW(hwnd_, WM_NULL, 0, 0);
  MSG pump = {};
  while (PeekMessageW(&pump, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&pump);
    DispatchMessageW(&pump);
  }

  if (cmd == 0) {
    return;
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
    case WM_DPICHANGED: {
      const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
      EnsureFont();
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
    case WM_KEYDOWN:
      if (wParam == VK_ESCAPE) {
        Hide();
        return 0;
      }
      break;
    case WM_NOTIFY: {
      auto* nmhdr = reinterpret_cast<LPNMHDR>(lParam);
      if (nmhdr->idFrom != IDC_FILE_LIST) {
        break;
      }
      if (nmhdr->code == LVN_GETDISPINFOW) {
        auto* di = reinterpret_cast<NMLVDISPINFOW*>(lParam);
        if (!(di->item.mask & LVIF_TEXT) || !di->item.pszText || di->item.cchTextMax <= 0) {
          return 0;
        }
        di->item.pszText[0] = L'\0';

        if (items_.empty()) {
          const wchar_t* msg = L"(Empty folder)";
          if (watchPath_.empty()) {
            msg = L"(Choose a folder from the tray menu)";
          } else if (!DirectoryWatcher::PathExists(watchPath_)) {
            msg = L"(Path is invalid)";
          }
          wcsncpy_s(di->item.pszText, di->item.cchTextMax, msg, _TRUNCATE);
          return 0;
        }

        if (di->item.iItem >= 0 && di->item.iItem < static_cast<int>(items_.size())) {
          const FileEntry& e = items_[di->item.iItem];
          std::wstring name = e.isDirectory ? (e.name + L"\\") : e.name;
          wcsncpy_s(di->item.pszText, di->item.cchTextMax, name.c_str(), _TRUNCATE);
        }
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
      Hide();
      return 0;
    case WM_DESTROY:
      hwnd_ = nullptr;
      return 0;
  }
  return DefWindowProcW(hwnd_, msg, wParam, lParam);
}
