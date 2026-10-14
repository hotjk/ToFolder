#include "help_dlg.h"

#include "lang.h"
#include "resource.h"
#include "theme.h"

namespace help_dlg {
namespace {

void CenterDialog(HWND dlg) {
  RECT rc = {};
  GetWindowRect(dlg, &rc);
  const int w = rc.right - rc.left;
  const int h = rc.bottom - rc.top;

  POINT pt = {};
  GetCursorPos(&pt);
  HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  RECT wa = {};
  if (GetMonitorInfoW(mon, &mi)) {
    wa = mi.rcWork;
  } else {
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
  }

  const int x = wa.left + (wa.right - wa.left - w) / 2;
  const int y = wa.top + (wa.bottom - wa.top - h) / 2;
  SetWindowPos(dlg, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

INT_PTR CALLBACK HelpDlgProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM /*lParam*/) {
  switch (msg) {
    case WM_INITDIALOG: {
      theme::AllowDarkForWindow(dlg, !theme::AppsUseLightTheme());
      SetWindowTextW(dlg, lang::T(lang::Id::HelpDialogTitle));
      SetDlgItemTextW(dlg, IDC_HELP_BODY, lang::T(lang::Id::HelpBody));
      SetDlgItemTextW(dlg, IDOK, lang::T(lang::Id::HotkeyOk));
      // Prefer top of text for long help.
      SendDlgItemMessageW(dlg, IDC_HELP_BODY, EM_SETSEL, 0, 0);
      SendDlgItemMessageW(dlg, IDC_HELP_BODY, EM_SCROLLCARET, 0, 0);
      CenterDialog(dlg);
      return TRUE;
    }
    case WM_COMMAND: {
      if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
        EndDialog(dlg, IDOK);
        return TRUE;
      }
      break;
    }
  }
  return FALSE;
}

}  // namespace

void ShowHelpDialog(HWND owner) {
  DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_HELP), owner, HelpDlgProc, 0);
}

}  // namespace help_dlg
