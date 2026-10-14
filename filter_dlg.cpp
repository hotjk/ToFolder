#include "filter_dlg.h"

#include "lang.h"
#include "resource.h"
#include "theme.h"

#include <commctrl.h>

#include <string>
#include <vector>

namespace filter_dlg {
namespace {

struct DialogState {
  bool enabled = true;
  std::vector<config::FilterItem> filters;
  std::vector<std::wstring> activeFilters;
  int selected = -1;
  bool suppressSelChange = false;
};

DialogState* StateFromDlg(HWND dlg) {
  return reinterpret_cast<DialogState*>(GetWindowLongPtrW(dlg, DWLP_USER));
}

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

void LocalizeDialog(HWND dlg) {
  SetWindowTextW(dlg, lang::T(lang::Id::FiltersDialogTitle));
  SetDlgItemTextW(dlg, IDC_FILTER_ENABLE, lang::T(lang::Id::FiltersEnable));
  SetDlgItemTextW(dlg, IDC_FILTER_LIST_LABEL, lang::T(lang::Id::FiltersListLabel));
  SetDlgItemTextW(dlg, IDC_FILTER_NAME_LABEL, lang::T(lang::Id::FiltersNameLabel));
  SetDlgItemTextW(dlg, IDC_FILTER_EXT_LABEL, lang::T(lang::Id::FiltersExtLabel));
  SetDlgItemTextW(dlg, IDC_FILTER_HINT, lang::T(lang::Id::FiltersHint));
  SetDlgItemTextW(dlg, IDC_FILTER_ADD, lang::T(lang::Id::FiltersAdd));
  SetDlgItemTextW(dlg, IDC_FILTER_REMOVE, lang::T(lang::Id::FiltersRemove));
  SetDlgItemTextW(dlg, IDC_FILTER_UPDATE, lang::T(lang::Id::FiltersUpdate));
  SetDlgItemTextW(dlg, IDOK, lang::T(lang::Id::HotkeyOk));
  SetDlgItemTextW(dlg, IDCANCEL, lang::T(lang::Id::HotkeyCancel));
}

void SetEditorEnabled(HWND dlg, bool enabled) {
  const int ids[] = {
      IDC_FILTER_LIST_LABEL, IDC_FILTER_LIST,      IDC_FILTER_NAME_LABEL, IDC_FILTER_NAME,
      IDC_FILTER_EXT_LABEL,  IDC_FILTER_EXT,       IDC_FILTER_HINT,       IDC_FILTER_ADD,
      IDC_FILTER_UPDATE,     IDC_FILTER_REMOVE,
  };
  for (int id : ids) {
    EnableWindow(GetDlgItem(dlg, id), enabled ? TRUE : FALSE);
  }
}

void RefreshList(HWND dlg, DialogState* state, int selectIndex) {
  HWND list = GetDlgItem(dlg, IDC_FILTER_LIST);
  state->suppressSelChange = true;
  SendMessageW(list, LB_RESETCONTENT, 0, 0);
  for (const auto& item : state->filters) {
    SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.name.c_str()));
  }
  if (selectIndex >= 0 && selectIndex < static_cast<int>(state->filters.size())) {
    SendMessageW(list, LB_SETCURSEL, selectIndex, 0);
    state->selected = selectIndex;
  } else if (!state->filters.empty()) {
    SendMessageW(list, LB_SETCURSEL, 0, 0);
    state->selected = 0;
  } else {
    state->selected = -1;
  }
  state->suppressSelChange = false;
}

void LoadSelectedIntoEdits(HWND dlg, DialogState* state) {
  if (state->selected < 0 || state->selected >= static_cast<int>(state->filters.size())) {
    SetDlgItemTextW(dlg, IDC_FILTER_NAME, L"");
    SetDlgItemTextW(dlg, IDC_FILTER_EXT, L"");
    return;
  }
  const auto& item = state->filters[state->selected];
  SetDlgItemTextW(dlg, IDC_FILTER_NAME, item.name.c_str());
  SetDlgItemTextW(dlg, IDC_FILTER_EXT, item.extensions.c_str());
}

std::wstring GetDlgText(HWND dlg, int id) {
  const int len = GetWindowTextLengthW(GetDlgItem(dlg, id));
  std::wstring text(static_cast<size_t>(len) + 1, L'\0');
  GetDlgItemTextW(dlg, id, text.data(), len + 1);
  text.resize(wcslen(text.c_str()));
  return text;
}

std::wstring TrimLocal(const std::wstring& s) {
  size_t begin = 0;
  while (begin < s.size() && iswspace(s[begin])) {
    ++begin;
  }
  size_t end = s.size();
  while (end > begin && iswspace(s[end - 1])) {
    --end;
  }
  return s.substr(begin, end - begin);
}

bool NameExists(const DialogState* state, const std::wstring& name, int exceptIndex) {
  for (int i = 0; i < static_cast<int>(state->filters.size()); ++i) {
    if (i == exceptIndex) {
      continue;
    }
    if (_wcsicmp(state->filters[i].name.c_str(), name.c_str()) == 0) {
      return true;
    }
  }
  return false;
}

bool ReadEdits(HWND dlg, DialogState* state, int exceptIndex, config::FilterItem& out) {
  out.name = TrimLocal(GetDlgText(dlg, IDC_FILTER_NAME));
  out.extensions = config::NormalizeExtensions(GetDlgText(dlg, IDC_FILTER_EXT));
  if (out.name.empty()) {
    MessageBoxW(dlg, lang::T(lang::Id::FiltersNameRequired), lang::T(lang::Id::FiltersDialogTitle),
                MB_OK | MB_ICONWARNING);
    return false;
  }
  if (NameExists(state, out.name, exceptIndex)) {
    MessageBoxW(dlg, lang::T(lang::Id::FiltersNameDuplicate), lang::T(lang::Id::FiltersDialogTitle),
                MB_OK | MB_ICONWARNING);
    return false;
  }
  return true;
}

void RenameActive(DialogState* state, const std::wstring& oldName, const std::wstring& newName) {
  for (auto& name : state->activeFilters) {
    if (_wcsicmp(name.c_str(), oldName.c_str()) == 0) {
      name = newName;
    }
  }
}

INT_PTR CALLBACK FiltersDlgProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_INITDIALOG: {
      auto* state = reinterpret_cast<DialogState*>(lParam);
      SetWindowLongPtrW(dlg, DWLP_USER, reinterpret_cast<LONG_PTR>(state));
      theme::AllowDarkForWindow(dlg, !theme::AppsUseLightTheme());
      LocalizeDialog(dlg);
      CheckDlgButton(dlg, IDC_FILTER_ENABLE, state->enabled ? BST_CHECKED : BST_UNCHECKED);
      RefreshList(dlg, state, state->filters.empty() ? -1 : 0);
      LoadSelectedIntoEdits(dlg, state);
      SetEditorEnabled(dlg, state->enabled);
      CenterDialog(dlg);
      return TRUE;
    }
    case WM_COMMAND: {
      auto* state = StateFromDlg(dlg);
      if (!state) {
        return TRUE;
      }
      const int id = LOWORD(wParam);
      const int code = HIWORD(wParam);
      if (id == IDC_FILTER_ENABLE && code == BN_CLICKED) {
        state->enabled = IsDlgButtonChecked(dlg, IDC_FILTER_ENABLE) == BST_CHECKED;
        SetEditorEnabled(dlg, state->enabled);
        return TRUE;
      }
      if (id == IDC_FILTER_LIST && code == LBN_SELCHANGE) {
        if (state->suppressSelChange) {
          return TRUE;
        }
        state->selected = static_cast<int>(SendDlgItemMessageW(dlg, IDC_FILTER_LIST, LB_GETCURSEL, 0, 0));
        LoadSelectedIntoEdits(dlg, state);
        return TRUE;
      }
      if (id == IDC_FILTER_ADD) {
        config::FilterItem item;
        if (!ReadEdits(dlg, state, -1, item)) {
          return TRUE;
        }
        state->filters.push_back(std::move(item));
        RefreshList(dlg, state, static_cast<int>(state->filters.size()) - 1);
        LoadSelectedIntoEdits(dlg, state);
        return TRUE;
      }
      if (id == IDC_FILTER_UPDATE) {
        if (state->selected < 0 || state->selected >= static_cast<int>(state->filters.size())) {
          return TRUE;
        }
        config::FilterItem item;
        if (!ReadEdits(dlg, state, state->selected, item)) {
          return TRUE;
        }
        const std::wstring oldName = state->filters[state->selected].name;
        state->filters[state->selected] = item;
        if (_wcsicmp(oldName.c_str(), item.name.c_str()) != 0) {
          RenameActive(state, oldName, item.name);
        }
        RefreshList(dlg, state, state->selected);
        LoadSelectedIntoEdits(dlg, state);
        return TRUE;
      }
      if (id == IDC_FILTER_REMOVE) {
        if (state->selected < 0 || state->selected >= static_cast<int>(state->filters.size())) {
          return TRUE;
        }
        state->filters.erase(state->filters.begin() + state->selected);
        config::PruneActiveFilters(state->filters, state->activeFilters);
        const int next = state->filters.empty()
                             ? -1
                             : (state->selected >= static_cast<int>(state->filters.size())
                                    ? static_cast<int>(state->filters.size()) - 1
                                    : state->selected);
        RefreshList(dlg, state, next);
        LoadSelectedIntoEdits(dlg, state);
        return TRUE;
      }
      if (id == IDOK) {
        state->enabled = IsDlgButtonChecked(dlg, IDC_FILTER_ENABLE) == BST_CHECKED;
        config::PruneActiveFilters(state->filters, state->activeFilters);
        EndDialog(dlg, IDOK);
        return TRUE;
      }
      if (id == IDCANCEL) {
        EndDialog(dlg, IDCANCEL);
        return TRUE;
      }
      break;
    }
  }
  return FALSE;
}

}  // namespace

bool ShowFiltersDialog(HWND owner, bool& enabled, std::vector<config::FilterItem>& filters,
                       std::vector<std::wstring>& activeFilters) {
  DialogState state;
  state.enabled = enabled;
  state.filters = filters;
  state.activeFilters = activeFilters;

  const INT_PTR result =
      DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_FILTERS), owner,
                      FiltersDlgProc, reinterpret_cast<LPARAM>(&state));
  if (result != IDOK) {
    return false;
  }

  enabled = state.enabled;
  filters = std::move(state.filters);
  activeFilters = std::move(state.activeFilters);
  config::PruneActiveFilters(filters, activeFilters);
  return true;
}

}  // namespace filter_dlg
