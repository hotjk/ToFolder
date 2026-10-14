#pragma once

#include "lang.h"

#include <string>
#include <vector>

namespace config {

constexpr wchar_t kDefaultHotkey[] = L"Ctrl+Shift+F";

enum class SortMode {
  ByTime = 0,
  ByName = 1,
};

enum class FontSize {
  Small = 0,   // current default
  Medium = 1,
  Large = 2,
};

enum class PanelSize {
  Small = 0,   // current default width
  Medium = 1,
  Large = 2,
};

// Index into Small/Medium/Large lookup tables; out-of-range values map to Small.
template <typename SizeEnum>
constexpr int SizeIndex(SizeEnum size) {
  const int i = static_cast<int>(size);
  return i >= 0 && i <= 2 ? i : 0;
}

// Special FilterItem::extensions values (not real extensions):
//   "*"  — match all files
//   "/"  — match folders
struct FilterItem {
  std::wstring name;
  std::wstring extensions;  // "*", "/", or lowercase comma-separated exts (no dots)
};

inline bool IsFilesFilter(const FilterItem& item) {
  return item.extensions == L"*";
}
inline bool IsFoldersFilter(const FilterItem& item) {
  return item.extensions == L"/";
}

struct Settings {
  std::wstring watchPath;
  SortMode sortMode = SortMode::ByTime;
  FontSize fontSize = FontSize::Small;
  PanelSize panelSize = PanelSize::Small;
  bool showHidden = false;
  bool expandFolders = true;
  bool watchEnabled = true;  // monitor folder for new files
  bool showFilters = true;   // show filter chips on the panel
  bool hotkeyEnabled = false;
  std::wstring hotkey = kDefaultHotkey;
  lang::Language language = lang::Language::English;
  bool languageConfigured = false;  // false until Language= written once
  std::vector<FilterItem> filters;
  std::vector<std::wstring> activeFilters;
};

std::wstring GetExePath();
std::wstring GetExeDirectory();
std::wstring GetConfigPath();
// Short hex hash of the exe path; identifies this copy of the executable.
std::wstring GetExePathHash();
// Mutex name unique per executable path (allows multiple renamed copies).
std::wstring GetSingleInstanceMutexName();

// The config file is UTF-8. Save() does nothing when the content is unchanged.
Settings Load();
bool Save(const Settings& settings);

std::vector<FilterItem> DefaultFilters();
std::wstring NormalizeExtensions(const std::wstring& raw);
void PruneActiveFilters(const std::vector<FilterItem>& filters,
                        std::vector<std::wstring>& activeFilters);

}  // namespace config
