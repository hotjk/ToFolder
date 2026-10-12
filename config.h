#pragma once

#include <string>

namespace config {

std::wstring GetExeDirectory();
std::wstring GetConfigPath();
std::wstring LoadWatchPath();
bool SaveWatchPath(const std::wstring& path);

}  // namespace config
