#include "config.h"

#include <windows.h>

#include <fstream>
#include <string>
#include <vector>

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

FontSize ParseFontSize(const std::wstring& value) {
  const std::wstring v = Trim(value);
  if (_wcsicmp(v.c_str(), L"medium") == 0 || _wcsicmp(v.c_str(), L"mid") == 0) {
    return FontSize::Medium;
  }
  if (_wcsicmp(v.c_str(), L"large") == 0 || _wcsicmp(v.c_str(), L"big") == 0) {
    return FontSize::Large;
  }
  return FontSize::Small;
}

const wchar_t* FontSizeToString(FontSize size) {
  switch (size) {
    case FontSize::Medium:
      return L"medium";
    case FontSize::Large:
      return L"large";
    case FontSize::Small:
    default:
      return L"small";
  }
}

PanelSize ParsePanelSize(const std::wstring& value) {
  const std::wstring v = Trim(value);
  if (_wcsicmp(v.c_str(), L"medium") == 0 || _wcsicmp(v.c_str(), L"mid") == 0) {
    return PanelSize::Medium;
  }
  if (_wcsicmp(v.c_str(), L"large") == 0 || _wcsicmp(v.c_str(), L"big") == 0) {
    return PanelSize::Large;
  }
  return PanelSize::Small;
}

const wchar_t* PanelSizeToString(PanelSize size) {
  switch (size) {
    case PanelSize::Medium:
      return L"medium";
    case PanelSize::Large:
      return L"large";
    case PanelSize::Small:
    default:
      return L"small";
  }
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

std::vector<std::wstring> SplitComma(const std::wstring& raw) {
  std::vector<std::wstring> parts;
  size_t start = 0;
  while (start <= raw.size()) {
    size_t comma = raw.find(L',', start);
    if (comma == std::wstring::npos) {
      comma = raw.size();
    }
    std::wstring part = Trim(raw.substr(start, comma - start));
    if (!part.empty()) {
      parts.push_back(std::move(part));
    }
    if (comma >= raw.size()) {
      break;
    }
    start = comma + 1;
  }
  return parts;
}

std::wstring JoinComma(const std::vector<std::wstring>& parts) {
  std::wstring out;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i > 0) {
      out += L',';
    }
    out += parts[i];
  }
  return out;
}

std::wstring ToLowerCopy(std::wstring s) {
  for (wchar_t& ch : s) {
    ch = static_cast<wchar_t>(towlower(ch));
  }
  return s;
}

bool StartsWithKey(const std::wstring& line, const wchar_t* key, std::wstring& valueOut) {
  const size_t keyLen = wcslen(key);
  if (line.compare(0, keyLen, key) != 0) {
    return false;
  }
  valueOut = Trim(line.substr(keyLen));
  return true;
}

}  // namespace

std::vector<FilterItem> DefaultFilters() {
  return {
      {L"file", L"*"},
      {L"folder", L"/"},
      {L"image",
       L"jpg,jpeg,png,gif,webp,bmp,ico,tif,tiff,heic,svg"},
      {L"zip", L"zip,7z,rar,tar,gz,bz2,xz,tgz"},
      {L"doc",
       L"doc,docx,xls,xlsx,ppt,pptx,pdf,txt,md,rtf,odt,ods,odp"},
  };
}

std::wstring NormalizeExtensions(const std::wstring& raw) {
  const std::wstring trimmed = Trim(raw);
  if (trimmed == L"*" || _wcsicmp(trimmed.c_str(), L":file") == 0 ||
      _wcsicmp(trimmed.c_str(), L":file:") == 0) {
    return L"*";
  }
  if (trimmed == L"/" || _wcsicmp(trimmed.c_str(), L":folder") == 0 ||
      _wcsicmp(trimmed.c_str(), L":folder:") == 0) {
    return L"/";
  }

  std::vector<std::wstring> parts = SplitComma(raw);
  std::vector<std::wstring> cleaned;
  cleaned.reserve(parts.size());
  for (std::wstring part : parts) {
    while (!part.empty() && part.front() == L'.') {
      part.erase(part.begin());
    }
    part = ToLowerCopy(Trim(part));
    if (part.empty() || part == L"*" || part == L"/") {
      continue;
    }
    bool dup = false;
    for (const auto& existing : cleaned) {
      if (existing == part) {
        dup = true;
        break;
      }
    }
    if (!dup) {
      cleaned.push_back(std::move(part));
    }
  }
  return JoinComma(cleaned);
}

void PruneActiveFilters(const std::vector<FilterItem>& filters,
                        std::vector<std::wstring>& activeFilters) {
  std::vector<std::wstring> kept;
  for (const auto& name : activeFilters) {
    bool found = false;
    for (const auto& f : filters) {
      if (_wcsicmp(f.name.c_str(), name.c_str()) == 0) {
        kept.push_back(f.name);  // canonical casing from definition
        found = true;
        break;
      }
    }
    if (!found) {
      continue;
    }
  }
  // Deduplicate while preserving order
  std::vector<std::wstring> unique;
  for (const auto& name : kept) {
    bool dup = false;
    for (const auto& u : unique) {
      if (_wcsicmp(u.c_str(), name.c_str()) == 0) {
        dup = true;
        break;
      }
    }
    if (!dup) {
      unique.push_back(name);
    }
  }
  activeFilters = std::move(unique);
}

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
  settings.filters = DefaultFilters();

  const std::wstring path = GetConfigPath();
  std::wifstream in(path);
  if (!in) {
    return settings;
  }

  bool filterCountSeen = false;
  int filterCount = 0;
  std::vector<FilterItem> loadedFilters;
  std::wstring activeRaw;

  std::wstring line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == L'\r') {
      line.pop_back();
    }
    std::wstring value;
    if (StartsWithKey(line, L"WatchPath=", value)) {
      settings.watchPath = value;
      continue;
    }
    if (StartsWithKey(line, L"SortMode=", value)) {
      settings.sortMode = ParseSortMode(value);
      continue;
    }
    if (StartsWithKey(line, L"FontSize=", value)) {
      settings.fontSize = ParseFontSize(value);
      continue;
    }
    if (StartsWithKey(line, L"PanelSize=", value)) {
      settings.panelSize = ParsePanelSize(value);
      continue;
    }
    if (StartsWithKey(line, L"ShowHidden=", value)) {
      settings.showHidden = ParseBool(value, false);
      continue;
    }
    if (StartsWithKey(line, L"ExpandFolders=", value)) {
      settings.expandFolders = ParseBool(value, false);
      continue;
    }
    if (StartsWithKey(line, L"WatchEnabled=", value)) {
      settings.watchEnabled = ParseBool(value, true);
      continue;
    }
    if (StartsWithKey(line, L"ShowFilters=", value)) {
      settings.showFilters = ParseBool(value, true);
      continue;
    }
    if (StartsWithKey(line, L"HotkeyEnabled=", value)) {
      settings.hotkeyEnabled = ParseBool(value, false);
      continue;
    }
    if (StartsWithKey(line, L"Hotkey=", value)) {
      if (!value.empty()) {
        settings.hotkey = value;
      }
      continue;
    }
    if (StartsWithKey(line, L"Language=", value)) {
      settings.languageConfigured = true;
      if (_wcsicmp(value.c_str(), L"zh") == 0 || _wcsicmp(value.c_str(), L"zh-CN") == 0 ||
          _wcsicmp(value.c_str(), L"chinese") == 0) {
        settings.language = lang::Language::Chinese;
      } else {
        settings.language = lang::Language::English;
      }
      continue;
    }
    if (StartsWithKey(line, L"FilterCount=", value)) {
      filterCountSeen = true;
      filterCount = _wtoi(value.c_str());
      if (filterCount < 0) {
        filterCount = 0;
      }
      if (filterCount > 64) {
        filterCount = 64;
      }
      loadedFilters.assign(static_cast<size_t>(filterCount), FilterItem{});
      continue;
    }
    if (StartsWithKey(line, L"ActiveFilters=", value)) {
      activeRaw = value;
      continue;
    }
    // FilterNName= / FilterNExt=
    if (line.compare(0, 6, L"Filter") == 0) {
      size_t i = 6;
      if (i >= line.size() || line[i] < L'0' || line[i] > L'9') {
        continue;
      }
      int index = 0;
      while (i < line.size() && line[i] >= L'0' && line[i] <= L'9') {
        index = index * 10 + (line[i] - L'0');
        ++i;
      }
      if (index < 1 || index > 64) {
        continue;
      }
      const size_t slot = static_cast<size_t>(index - 1);
      if (slot >= loadedFilters.size()) {
        loadedFilters.resize(slot + 1);
      }
      if (line.compare(i, 5, L"Name=") == 0) {
        loadedFilters[slot].name = Trim(line.substr(i + 5));
      } else if (line.compare(i, 4, L"Ext=") == 0) {
        loadedFilters[slot].extensions = NormalizeExtensions(line.substr(i + 4));
      }
    }
  }

  if (filterCountSeen) {
    settings.filters.clear();
    for (auto& item : loadedFilters) {
      item.name = Trim(item.name);
      item.extensions = NormalizeExtensions(item.extensions);
      if (!item.name.empty()) {
        settings.filters.push_back(std::move(item));
      }
    }
  }

  settings.activeFilters = SplitComma(activeRaw);
  PruneActiveFilters(settings.filters, settings.activeFilters);
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
  out << L"FontSize=" << FontSizeToString(settings.fontSize) << L"\n";
  out << L"PanelSize=" << PanelSizeToString(settings.panelSize) << L"\n";
  out << L"ShowHidden=" << (settings.showHidden ? L"true" : L"false") << L"\n";
  out << L"ExpandFolders=" << (settings.expandFolders ? L"true" : L"false") << L"\n";
  out << L"WatchEnabled=" << (settings.watchEnabled ? L"true" : L"false") << L"\n";
  out << L"ShowFilters=" << (settings.showFilters ? L"true" : L"false") << L"\n";
  out << L"HotkeyEnabled=" << (settings.hotkeyEnabled ? L"true" : L"false") << L"\n";
  out << L"Hotkey=" << settings.hotkey << L"\n";
  out << L"Language=" << (settings.language == lang::Language::Chinese ? L"zh" : L"en") << L"\n";

  out << L"FilterCount=" << settings.filters.size() << L"\n";
  for (size_t i = 0; i < settings.filters.size(); ++i) {
    out << L"Filter" << (i + 1) << L"Name=" << settings.filters[i].name << L"\n";
    out << L"Filter" << (i + 1) << L"Ext=" << settings.filters[i].extensions << L"\n";
  }
  out << L"ActiveFilters=" << JoinComma(settings.activeFilters) << L"\n";
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

FontSize LoadFontSize() {
  return Load().fontSize;
}

bool SaveFontSize(FontSize size) {
  Settings settings = Load();
  settings.fontSize = size;
  return Save(settings);
}

PanelSize LoadPanelSize() {
  return Load().panelSize;
}

bool SavePanelSize(PanelSize size) {
  Settings settings = Load();
  settings.panelSize = size;
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

bool LoadWatchEnabled() {
  return Load().watchEnabled;
}

bool SaveWatchEnabled(bool enabled) {
  Settings settings = Load();
  settings.watchEnabled = enabled;
  return Save(settings);
}

bool LoadShowFilters() {
  return Load().showFilters;
}

bool SaveShowFilters(bool showFilters) {
  Settings settings = Load();
  settings.showFilters = showFilters;
  return Save(settings);
}

bool LoadHotkeyEnabled() {
  return Load().hotkeyEnabled;
}

bool SaveHotkeySettings(bool enabled, const std::wstring& hotkey) {
  Settings settings = Load();
  settings.hotkeyEnabled = enabled;
  if (!hotkey.empty()) {
    settings.hotkey = hotkey;
  }
  return Save(settings);
}

bool SaveLanguage(lang::Language language) {
  Settings settings = Load();
  settings.language = language;
  settings.languageConfigured = true;
  return Save(settings);
}

bool SaveFilters(const std::vector<FilterItem>& filters,
                 const std::vector<std::wstring>& activeFilters) {
  Settings settings = Load();
  settings.filters = filters;
  settings.activeFilters = activeFilters;
  PruneActiveFilters(settings.filters, settings.activeFilters);
  return Save(settings);
}

}  // namespace config
