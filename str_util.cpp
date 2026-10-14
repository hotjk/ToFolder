#include "str_util.h"

#include <windows.h>

#include <cwctype>

namespace str {

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

std::wstring ToLower(std::wstring s) {
  for (wchar_t& ch : s) {
    ch = static_cast<wchar_t>(towlower(ch));
  }
  return s;
}

bool EqualsNoCase(const std::wstring& a, const wchar_t* b) {
  return _wcsicmp(a.c_str(), b) == 0;
}

std::vector<std::wstring> SplitTrim(const std::wstring& s, wchar_t sep) {
  std::vector<std::wstring> parts;
  size_t start = 0;
  while (start <= s.size()) {
    size_t pos = s.find(sep, start);
    if (pos == std::wstring::npos) {
      pos = s.size();
    }
    std::wstring part = Trim(s.substr(start, pos - start));
    if (!part.empty()) {
      parts.push_back(std::move(part));
    }
    start = pos + 1;
  }
  return parts;
}

std::wstring Join(const std::vector<std::wstring>& parts, wchar_t sep) {
  std::wstring out;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i > 0) {
      out += sep;
    }
    out += parts[i];
  }
  return out;
}

std::string WideToUtf8(const std::wstring& s) {
  if (s.empty()) {
    return {};
  }
  const int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0,
                                    nullptr, nullptr);
  std::string out(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n, nullptr,
                      nullptr);
  return out;
}

std::wstring DecodeText(const std::string& bytes) {
  const char* data = bytes.data();
  int size = static_cast<int>(bytes.size());
  if (size >= 3 && static_cast<unsigned char>(data[0]) == 0xEF &&
      static_cast<unsigned char>(data[1]) == 0xBB && static_cast<unsigned char>(data[2]) == 0xBF) {
    data += 3;
    size -= 3;
  }
  if (size == 0) {
    return {};
  }
  UINT codePage = CP_UTF8;
  int n = MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, data, size, nullptr, 0);
  if (n == 0) {
    codePage = CP_ACP;
    n = MultiByteToWideChar(codePage, 0, data, size, nullptr, 0);
  }
  std::wstring out(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(codePage, 0, data, size, out.data(), n);
  return out;
}

}  // namespace str
