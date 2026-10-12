#include "panel.h"

#include "app_messages.h"
#include "dpi.h"
#include "resource.h"

#include <commctrl.h>
#include <shellapi.h>

#include <string>

#ifndef LVS_EX_NOHSCROLL
#define LVS_EX_NOHSCROLL 0x10000000
#endif

namespace {

constexpr int kPanelWidth96 = 360;

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
      WS_POPUP | WS_BORDER | WS_CLIPCHILDREN,
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
  SetWindowPos(listView_, nullptr, 0, 0, rc.right, rc.bottom, SWP_NOZORDER);
  UpdateColumnWidth();
}

void PanelWindow::SetWatchPath(const std::wstring& path) {
  watchPath_ = path;
}

void PanelWindow::PopulateList() {
  if (!listView_) {
    return;
  }

  const int top = ListView_GetTopIndex(listView_);
  items_ = DirectoryWatcher::Enumerate(watchPath_);

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
}

void PanelWindow::Show() {
  if (!hwnd_) {
    return;
  }

  EnsureFont();

  const RECT wa = WorkAreaRect();
  const int width = PanelWidthPx();
  const int height = wa.bottom - wa.top;
  const int x = wa.right - width;
  const int y = wa.top;

  SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_HIDEWINDOW);
  Layout();
  Refresh();

  AnimateWindow(hwnd_, 180, AW_SLIDE | AW_HOR_NEGATIVE);
  SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void PanelWindow::Hide() {
  if (!hwnd_ || !IsWindowVisible(hwnd_)) {
    return;
  }
  AnimateWindow(hwnd_, 150, AW_SLIDE | AW_HOR_POSITIVE | AW_HIDE);
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
    case WM_CLOSE:
      Hide();
      return 0;
    case WM_DESTROY:
      hwnd_ = nullptr;
      return 0;
  }
  return DefWindowProcW(hwnd_, msg, wParam, lParam);
}
