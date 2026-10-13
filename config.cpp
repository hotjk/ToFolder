#include "config.h"

#include <windows.h>

#include <fstream>
#include <string>

namespace config {
namespace {

std::wstring Trim(const std::wstring& s) {
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

SortMode ParseSortMode(const std::wstring& value) {
  const std::wstring v = Trim(value);
  if (_wcsicmp(v.c_str(), L"name") == 0) {
    return SortMode::ByName;
  }
  return SortMode::ByTime;
}

const wchar_t* SortModeToString(SortMode mode) {
  return mode == SortMode::ByName ? L"name" : L"time";
}

bool ParseBool(const std::wstring& value, bool defaultValue) {
  const std::wstring v = Trim(value);
  if (v.empty()) {
    return defaultValue;
  }
  if (_wcsicmp(v.c_str(), L"1") == 0 || _wcsicmp(v.c_str(), L"true") == 0 ||
      _wcsicmp(v.c_str(), L"yes") == 0 || _wcsicmp(v.c_str(), L"on") == 0) {
    return true;
  }
  if (_wcsicmp(v.c_str(), L"0") == 0 || _wcsicmp(v.c_str(), L"false") == 0 ||
      _wcsicmp(v.c_str(), L"no") == 0 || _wcsicmp(v.c_str(), L"off") == 0) {
    return false;
  }
  return defaultValue;
}

}  // namespace

std::wstring GetExeDirectory() {
  wchar_t buf[MAX_PATH] = {};
  DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return L".";
  }
  std::wstring path(buf, n);
  size_t pos = path.find_last_of(L"\\/");
  if (pos == std::wstring::npos) {
    return L".";
  }
  return path.substr(0, pos);
}

std::wstring GetConfigPath() {
  return GetExeDirectory() + L"\\config.ini";
}

Settings Load() {
  Settings settings;
  const std::wstring path = GetConfigPath();
  std::wifstream in(path);
  if (!in) {
    return settings;
  }
  std::wstring line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == L'\r') {
      line.pop_back();
    }
    const std::wstring watchKey = L"WatchPath=";
    if (line.compare(0, watchKey.size(), watchKey) == 0) {
      settings.watchPath = Trim(line.substr(watchKey.size()));
      continue;
    }
    const std::wstring sortKey = L"SortMode=";
    if (line.compare(0, sortKey.size(), sortKey) == 0) {
      settings.sortMode = ParseSortMode(line.substr(sortKey.size()));
      continue;
    }
    const std::wstring hiddenKey = L"ShowHidden=";
    if (line.compare(0, hiddenKey.size(), hiddenKey) == 0) {
      settings.showHidden = ParseBool(line.substr(hiddenKey.size()), false);
      continue;
    }
    const std::wstring expandKey = L"ExpandFolders=";
    if (line.compare(0, expandKey.size(), expandKey) == 0) {
      settings.expandFolders = ParseBool(line.substr(expandKey.size()), false);
    }
  }
  return settings;
}

bool Save(const Settings& settings) {
  const std::wstring configPath = GetConfigPath();
  std::wofstream out(configPath, std::ios::trunc);
  if (!out) {
    return false;
  }
  out << L"WatchPath=" << settings.watchPath << L"\n";
  out << L"SortMode=" << SortModeToString(settings.sortMode) << L"\n";
  out << L"ShowHidden=" << (settings.showHidden ? L"true" : L"false") << L"\n";
  out << L"ExpandFolders=" << (settings.expandFolders ? L"true" : L"false") << L"\n";
  return static_cast<bool>(out);
}

std::wstring LoadWatchPath() {
  return Load().watchPath;
}

bool SaveWatchPath(const std::wstring& path) {
  Settings settings = Load();
  settings.watchPath = path;
  return Save(settings);
}

SortMode LoadSortMode() {
  return Load().sortMode;
}

bool SaveSortMode(SortMode mode) {
  Settings settings = Load();
  settings.sortMode = mode;
  return Save(settings);
}

bool LoadShowHidden() {
  return Load().showHidden;
}

bool SaveShowHidden(bool showHidden) {
  Settings settings = Load();
  settings.showHidden = showHidden;
  return Save(settings);
}

bool LoadExpandFolders() {
  return Load().expandFolders;
}

bool SaveExpandFolders(bool expandFolders) {
  Settings settings = Load();
  settings.expandFolders = expandFolders;
  return Save(settings);
}

}  // namespace config
