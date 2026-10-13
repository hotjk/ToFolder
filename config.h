#pragma once

#include <string>

namespace config {

enum class SortMode {
  ByTime = 0,
  ByName = 1,
};

struct Settings {
  std::wstring watchPath;
  SortMode sortMode = SortMode::ByTime;
  bool showHidden = false;
  bool expandFolders = false;
};

std::wstring GetExeDirectory();
std::wstring GetConfigPath();
Settings Load();
bool Save(const Settings& settings);

// Convenience wrappers
std::wstring LoadWatchPath();
bool SaveWatchPath(const std::wstring& path);
SortMode LoadSortMode();
bool SaveSortMode(SortMode mode);
bool LoadShowHidden();
bool SaveShowHidden(bool showHidden);
bool LoadExpandFolders();
bool SaveExpandFolders(bool expandFolders);

}  // namespace config
