#include "path_util.h"

#include "str_util.h"

#include <cwchar>

namespace path_util {
namespace {

bool IsSeparator(wchar_t ch) {
  return ch == L'\\' || ch == L'/';
}

std::wstring StripTrailingSeparators(std::wstring p) {
  while (!p.empty() && IsSeparator(p.back())) {
    p.pop_back();
  }
  return p;
}

}  // namespace

std::wstring Parent(const std::wstring& path) {
  const std::wstring p = StripTrailingSeparators(path);
  const size_t pos = p.find_last_of(L"\\/");
  if (pos == std::wstring::npos) {
    return p;
  }
  if (pos == 2 && p[1] == L':') {
    return p.substr(0, 3);
  }
  return p.substr(0, pos);
}

bool EqualsNoCase(const std::wstring& a, const std::wstring& b) {
  return _wcsicmp(a.c_str(), b.c_str()) == 0;
}

bool IsSameOrUnder(const std::wstring& path, const std::wstring& root) {
  if (root.empty() || path.empty()) {
    return false;
  }
  if (EqualsNoCase(path, root)) {
    return true;
  }
  const std::wstring base = StripTrailingSeparators(root);
  if (path.size() <= base.size() || _wcsnicmp(path.c_str(), base.c_str(), base.size()) != 0) {
    return false;
  }
  return IsSeparator(path[base.size()]);
}

std::wstring ExtensionLower(const std::wstring& fileName) {
  const size_t dot = fileName.find_last_of(L'.');
  if (dot == std::wstring::npos || dot + 1 >= fileName.size()) {
    return {};
  }
  return str::ToLower(fileName.substr(dot + 1));
}

}  // namespace path_util
