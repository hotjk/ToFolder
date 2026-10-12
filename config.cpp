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

std::wstring LoadWatchPath() {
  const std::wstring path = GetConfigPath();
  std::wifstream in(path);
  if (!in) {
    return L"";
  }
  std::wstring line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == L'\r') {
      line.pop_back();
    }
    const std::wstring key = L"WatchPath=";
    if (line.compare(0, key.size(), key) == 0) {
      return Trim(line.substr(key.size()));
    }
  }
  return L"";
}

bool SaveWatchPath(const std::wstring& path) {
  const std::wstring configPath = GetConfigPath();
  std::wofstream out(configPath, std::ios::trunc);
  if (!out) {
    return false;
  }
  out << L"WatchPath=" << path << L"\n";
  return static_cast<bool>(out);
}

}  // namespace config
