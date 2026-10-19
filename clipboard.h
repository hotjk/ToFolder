#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace clipboard {

bool CopyText(HWND owner, const std::wstring& text);

// Puts files on the clipboard as CF_HDROP; `cut` marks them for move on paste.
bool CopyFileItems(HWND owner, const std::vector<std::wstring>& paths, bool cut);

}  // namespace clipboard
