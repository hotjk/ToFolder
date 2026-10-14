#pragma once

#include <string>
#include <vector>

namespace str {

std::wstring Trim(const std::wstring& s);
std::wstring ToLower(std::wstring s);
bool EqualsNoCase(const std::wstring& a, const wchar_t* b);

// Splits on `sep`, trims each part, and drops empty parts.
std::vector<std::wstring> SplitTrim(const std::wstring& s, wchar_t sep);
std::wstring Join(const std::vector<std::wstring>& parts, wchar_t sep);

std::string WideToUtf8(const std::wstring& s);
// Decodes UTF-8 (with or without BOM); falls back to the ANSI code page if the
// bytes are not valid UTF-8.
std::wstring DecodeText(const std::string& bytes);

}  // namespace str
