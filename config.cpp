#include "config.h"

#include "str_util.h"

#include <windows.h>

#include <string>
#include <vector>

namespace config {
namespace {

constexpr int kMaxFilters = 64;

// Content of the config file as last loaded or written; Save() skips identical writes.
std::string g_lastFileContent;

SortMode ParseSortMode(const std::wstring& value) {
  return str::EqualsNoCase(value, L"name") ? SortMode::ByName : SortMode::ByTime;
}

const wchar_t* SortModeToString(SortMode mode) {
  return mode == SortMode::ByName ? L"name" : L"time";
}

// Shared by FontSize and PanelSize, which both use Small / Medium / Large.
template <typename SizeEnum>
SizeEnum ParseSizeLevel(const std::wstring& value) {
  if (str::EqualsNoCase(value, L"medium") || str::EqualsNoCase(value, L"mid")) {
    return SizeEnum::Medium;
  }
  if (str::EqualsNoCase(value, L"large") || str::EqualsNoCase(value, L"big")) {
    return SizeEnum::Large;
  }
  return SizeEnum::Small;
}

template <typename SizeEnum>
const wchar_t* SizeLevelToString(SizeEnum size) {
  constexpr const wchar_t* kNames[] = {L"small", L"medium", L"large"};
  return kNames[SizeIndex(size)];
}

bool ParseBool(const std::wstring& value, bool defaultValue) {
  for (const wchar_t* t : {L"1", L"true", L"yes", L"on"}) {
    if (str::EqualsNoCase(value, t)) {
      return true;
    }
  }
  for (const wchar_t* f : {L"0", L"false", L"no", L"off"}) {
    if (str::EqualsNoCase(value, f)) {
      return false;
    }
  }
  return defaultValue;
}

const wchar_t* BoolToString(bool value) {
  return value ? L"true" : L"false";
}

lang::Language ParseLanguage(const std::wstring& value) {
  if (str::EqualsNoCase(value, L"zh") || str::EqualsNoCase(value, L"zh-CN") ||
      str::EqualsNoCase(value, L"chinese")) {
    return lang::Language::Chinese;
  }
  return lang::Language::English;
}

// "Filter<N>Name" / "Filter<N>Ext" -> 0-based slot and field suffix.
bool ParseFilterKey(const std::wstring& key, size_t& slot, std::wstring& field) {
  constexpr size_t kPrefixLen = 6;
  if (key.compare(0, kPrefixLen, L"Filter") != 0) {
    return false;
  }
  size_t i = kPrefixLen;
  int index = 0;
  while (i < key.size() && key[i] >= L'0' && key[i] <= L'9' && index <= kMaxFilters) {
    index = index * 10 + (key[i] - L'0');
    ++i;
  }
  if (i == kPrefixLen || index < 1 || index > kMaxFilters) {
    return false;
  }
  slot = static_cast<size_t>(index - 1);
  field = key.substr(i);
  return true;
}

bool ReadFileBytes(const std::wstring& path, std::string& out) {
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return false;
  }
  LARGE_INTEGER size = {};
  bool ok = GetFileSizeEx(file, &size) && size.QuadPart < (1 << 20);
  if (ok) {
    out.resize(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    ok = out.empty() ||
         (ReadFile(file, out.data(), static_cast<DWORD>(out.size()), &read, nullptr) &&
          read == out.size());
  }
  CloseHandle(file);
  return ok;
}

bool WriteFileBytes(const std::wstring& path, const std::string& bytes) {
  // Write a sibling temp file and swap it in so a failed write never truncates the config.
  const std::wstring temp = path + L".tmp";
  HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return false;
  }
  DWORD written = 0;
  const bool ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written,
                            nullptr) &&
                  written == bytes.size();
  CloseHandle(file);
  if (!ok || !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
    DeleteFileW(temp.c_str());
    return false;
  }
  return true;
}

bool FileExists(const std::wstring& path) {
  const DWORD attrs = GetFileAttributesW(path.c_str());
  return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::wstring Serialize(const Settings& settings) {
  std::wstring out;
  auto put = [&out](const std::wstring& key, const std::wstring& value) {
    out += key;
    out += L'=';
    out += value;
    out += L"\r\n";
  };
  put(L"WatchPath", settings.watchPath);
  put(L"SortMode", SortModeToString(settings.sortMode));
  put(L"FontSize", SizeLevelToString(settings.fontSize));
  put(L"PanelSize", SizeLevelToString(settings.panelSize));
  put(L"ShowHidden", BoolToString(settings.showHidden));
  put(L"ExpandFolders", BoolToString(settings.expandFolders));
  put(L"WatchEnabled", BoolToString(settings.watchEnabled));
  put(L"ShowFilters", BoolToString(settings.showFilters));
  put(L"HotkeyEnabled", BoolToString(settings.hotkeyEnabled));
  put(L"Hotkey", settings.hotkey);
  put(L"Language", settings.language == lang::Language::Chinese ? L"zh" : L"en");
  put(L"FilterCount", std::to_wstring(settings.filters.size()));
  for (size_t i = 0; i < settings.filters.size(); ++i) {
    const std::wstring prefix = L"Filter" + std::to_wstring(i + 1);
    put(prefix + L"Name", settings.filters[i].name);
    put(prefix + L"Ext", settings.filters[i].extensions);
  }
  put(L"ActiveFilters", str::Join(settings.activeFilters, L','));
  return out;
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
  const std::wstring trimmed = str::Trim(raw);
  if (trimmed == L"*" || str::EqualsNoCase(trimmed, L":file") ||
      str::EqualsNoCase(trimmed, L":file:")) {
    return L"*";
  }
  if (trimmed == L"/" || str::EqualsNoCase(trimmed, L":folder") ||
      str::EqualsNoCase(trimmed, L":folder:")) {
    return L"/";
  }

  std::vector<std::wstring> cleaned;
  for (std::wstring part : str::SplitTrim(raw, L',')) {
    const size_t firstNonDot = part.find_first_not_of(L'.');
    part = firstNonDot == std::wstring::npos ? std::wstring() : part.substr(firstNonDot);
    part = str::ToLower(str::Trim(part));
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
  return str::Join(cleaned, L',');
}

void PruneActiveFilters(const std::vector<FilterItem>& filters,
                        std::vector<std::wstring>& activeFilters) {
  // Keep only names that exist (using the definition's casing), deduplicated, in order.
  std::vector<std::wstring> kept;
  for (const auto& name : activeFilters) {
    const FilterItem* match = nullptr;
    for (const auto& f : filters) {
      if (_wcsicmp(f.name.c_str(), name.c_str()) == 0) {
        match = &f;
        break;
      }
    }
    if (!match) {
      continue;
    }
    bool dup = false;
    for (const auto& k : kept) {
      if (_wcsicmp(k.c_str(), match->name.c_str()) == 0) {
        dup = true;
        break;
      }
    }
    if (!dup) {
      kept.push_back(match->name);
    }
  }
  activeFilters = std::move(kept);
}

std::wstring GetExePath() {
  std::wstring buf(MAX_PATH, L'\0');
  for (;;) {
    const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    if (n == 0) {
      return L"ToFolder.exe";
    }
    if (n < buf.size()) {
      buf.resize(n);
      return buf;
    }
    buf.resize(buf.size() * 2);
  }
}

std::wstring GetExeDirectory() {
  const std::wstring path = GetExePath();
  size_t pos = path.find_last_of(L"\\/");
  if (pos == std::wstring::npos) {
    return L".";
  }
  return path.substr(0, pos);
}

std::wstring GetConfigPath() {
  // Same directory and base name as the executable, with .ini extension.
  const std::wstring exe = GetExePath();
  const size_t slash = exe.find_last_of(L"\\/");
  const size_t dot = exe.find_last_of(L'.');
  if (dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash)) {
    return exe.substr(0, dot) + L".ini";
  }
  return exe + L".ini";
}

std::wstring GetExePathHash() {
  // FNV-1a of the lowercased exe path: stable per path, differs for renamed copies.
  const std::wstring exe = GetExePath();
  unsigned int hash = 2166136261u;
  for (wchar_t ch : exe) {
    hash ^= static_cast<unsigned int>(towlower(ch));
    hash *= 16777619u;
  }
  wchar_t text[16] = {};
  swprintf_s(text, L"%08X", hash);
  return text;
}

std::wstring GetSingleInstanceMutexName() {
  // Mutex names cannot contain '\' beyond the namespace prefix, hence the hash.
  return L"Local\\ToFolder_Exe_" + GetExePathHash();
}

Settings Load() {
  Settings settings;
  settings.filters = DefaultFilters();

  const std::wstring configPath = GetConfigPath();
  std::wstring path = configPath;
  // Legacy fallback: older builds used config.ini next to the exe.
  if (!FileExists(path)) {
    const std::wstring legacy = GetExeDirectory() + L"\\config.ini";
    if (FileExists(legacy)) {
      path = legacy;
    }
  }
  std::string bytes;
  if (!ReadFileBytes(path, bytes)) {
    return settings;
  }
  if (path == configPath) {
    g_lastFileContent = bytes;
  }

  bool filterCountSeen = false;
  std::vector<FilterItem> loadedFilters;
  std::wstring activeRaw;

  const std::wstring text = str::DecodeText(bytes);
  size_t lineStart = 0;
  while (lineStart < text.size()) {
    size_t lineEnd = text.find(L'\n', lineStart);
    if (lineEnd == std::wstring::npos) {
      lineEnd = text.size();
    }
    const std::wstring line = text.substr(lineStart, lineEnd - lineStart);
    lineStart = lineEnd + 1;

    const size_t eq = line.find(L'=');
    if (eq == std::wstring::npos) {
      continue;
    }
    const std::wstring key = str::Trim(line.substr(0, eq));
    const std::wstring value = str::Trim(line.substr(eq + 1));

    size_t slot = 0;
    std::wstring field;
    if (key == L"WatchPath") {
      settings.watchPath = value;
    } else if (key == L"SortMode") {
      settings.sortMode = ParseSortMode(value);
    } else if (key == L"FontSize") {
      settings.fontSize = ParseSizeLevel<FontSize>(value);
    } else if (key == L"PanelSize") {
      settings.panelSize = ParseSizeLevel<PanelSize>(value);
    } else if (key == L"ShowHidden") {
      settings.showHidden = ParseBool(value, settings.showHidden);
    } else if (key == L"ExpandFolders") {
      settings.expandFolders = ParseBool(value, settings.expandFolders);
    } else if (key == L"WatchEnabled") {
      settings.watchEnabled = ParseBool(value, settings.watchEnabled);
    } else if (key == L"ShowFilters") {
      settings.showFilters = ParseBool(value, settings.showFilters);
    } else if (key == L"HotkeyEnabled") {
      settings.hotkeyEnabled = ParseBool(value, settings.hotkeyEnabled);
    } else if (key == L"Hotkey") {
      if (!value.empty()) {
        settings.hotkey = value;
      }
    } else if (key == L"Language") {
      settings.languageConfigured = true;
      settings.language = ParseLanguage(value);
    } else if (key == L"FilterCount") {
      filterCountSeen = true;
      int count = _wtoi(value.c_str());
      count = count < 0 ? 0 : (count > kMaxFilters ? kMaxFilters : count);
      loadedFilters.resize(static_cast<size_t>(count));
    } else if (key == L"ActiveFilters") {
      activeRaw = value;
    } else if (ParseFilterKey(key, slot, field)) {
      if (slot >= loadedFilters.size()) {
        loadedFilters.resize(slot + 1);
      }
      if (field == L"Name") {
        loadedFilters[slot].name = value;
      } else if (field == L"Ext") {
        loadedFilters[slot].extensions = NormalizeExtensions(value);
      }
    }
  }

  if (filterCountSeen) {
    settings.filters.clear();
    for (auto& item : loadedFilters) {
      if (!item.name.empty()) {
        settings.filters.push_back(std::move(item));
      }
    }
  }

  settings.activeFilters = str::SplitTrim(activeRaw, L',');
  PruneActiveFilters(settings.filters, settings.activeFilters);
  return settings;
}

bool Save(const Settings& settings) {
  Settings normalized = settings;
  PruneActiveFilters(normalized.filters, normalized.activeFilters);
  const std::string bytes = str::WideToUtf8(Serialize(normalized));
  if (bytes == g_lastFileContent) {
    return true;
  }
  if (!WriteFileBytes(GetConfigPath(), bytes)) {
    return false;
  }
  g_lastFileContent = bytes;
  return true;
}

}  // namespace config
