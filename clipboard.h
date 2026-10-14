#pragma once

#include <windows.h>

#include <string>

namespace clipboard {

bool CopyText(HWND owner, const std::wstring& text);

// Puts a single file on the clipboard as CF_HDROP; `cut` marks it for move on paste.
bool CopyFileItem(HWND owner, const std::wstring& path, bool cut);

}  // namespace clipboard
