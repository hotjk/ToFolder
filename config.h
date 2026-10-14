#pragma once

#include "lang.h"

#include <string>
#include <vector>

namespace config {

enum class SortMode {
  ByTime = 0,
  ByName = 1,
};

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
  bool showHidden = false;
  bool expandFolders = true;
  bool watchEnabled = true;  // monitor folder for new files
  bool showFilters = true;   // show filter chips on the panel
  bool hotkeyEnabled = false;
  std::wstring hotkey = L"Ctrl+Shift+F";
  lang::Language language = lang::Language::English;
  bool languageConfigured = false;  // false until Language= written once
  std::vector<FilterItem> filters;
  std::vector<std::wstring> activeFilters;
};

std::wstring GetExeDirectory();
std::wstring GetConfigPath();
Settings Load();
bool Save(const Settings& settings);

std::vector<FilterItem> DefaultFilters();
std::wstring NormalizeExtensions(const std::wstring& raw);
void PruneActiveFilters(const std::vector<FilterItem>& filters,
                        std::vector<std::wstring>& activeFilters);

// Convenience wrappers
std::wstring LoadWatchPath();
bool SaveWatchPath(const std::wstring& path);
SortMode LoadSortMode();
bool SaveSortMode(SortMode mode);
bool LoadShowHidden();
bool SaveShowHidden(bool showHidden);
bool LoadExpandFolders();
bool SaveExpandFolders(bool expandFolders);
bool LoadWatchEnabled();
bool SaveWatchEnabled(bool enabled);
bool LoadShowFilters();
bool SaveShowFilters(bool showFilters);
bool LoadHotkeyEnabled();
bool SaveHotkeySettings(bool enabled, const std::wstring& hotkey);
bool SaveLanguage(lang::Language language);
bool SaveFilters(const std::vector<FilterItem>& filters,
                 const std::vector<std::wstring>& activeFilters);

}  // namespace config
