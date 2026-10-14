#include "help_dlg.h"

#include "lang.h"
#include "resource.h"
#include "theme.h"
#include "ui_util.h"

namespace help_dlg {
namespace {

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
      ui_util::CenterOnCursorMonitor(dlg);
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
