#pragma once

#include <string>

namespace path_util {

// "C:\a\b" -> "C:\a", "C:\a" -> "C:\"; trailing separators are ignored.
std::wstring Parent(const std::wstring& path);

bool EqualsNoCase(const std::wstring& a, const std::wstring& b);

// True if `path` equals `root` or lies anywhere beneath it.
bool IsSameOrUnder(const std::wstring& path, const std::wstring& root);

// Lowercase extension without the dot; empty if none.
std::wstring ExtensionLower(const std::wstring& fileName);

}  // namespace path_util
