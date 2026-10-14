#pragma once

#include <windows.h>

namespace lang {

enum class Language {
  English = 0,
  Chinese = 1,
};

enum class Id {
  TipNoFolder,
  TipPrefix,
  SelectFolderTitle,
  MenuOpenPanel,
  MenuOpenFolder,
  MenuChooseFolder,
  MenuWatchNewFiles,
  MenuSortByTime,
  MenuSortByName,
  MenuShowHidden,
  MenuExpandFolders,
  MenuStartWithWindows,
  MenuLanguage,
  MenuLangEnglish,
  MenuLangChinese,
  MenuHotkey,
  MenuFilters,
  MenuAbout,
  MenuExit,
  StartupUpdateFailed,
  AboutBody,
  AboutTitle,
  ListOpen,
  ListOpenLocation,
  ListDelete,
  DeleteFailed,
  EmptyFolder,
  ChooseFolderHint,
  PathInvalid,
  HotkeyDialogTitle,
  HotkeyEnable,
  HotkeyShortcutLabel,
  HotkeyHint,
  HotkeyPressShortcut,
  HotkeyOk,
  HotkeyCancel,
  HotkeyNeedSet,
  HotkeyConflict,
  FiltersDialogTitle,
  FiltersEnable,
  FiltersListLabel,
  FiltersNameLabel,
  FiltersExtLabel,
  FiltersHint,
  FiltersAdd,
  FiltersRemove,
  FiltersUpdate,
  FiltersNameRequired,
  FiltersNameDuplicate,
  Count,
};

Language Current();
void Set(Language language);
Language DetectOs();

const wchar_t* T(Id id);

}  // namespace lang
