#pragma once

#include "config.h"

#include <windows.h>

#include <string>
#include <vector>

namespace filter_dlg {

// Modal editor for filter enable + definitions. On OK, updates all three outs.
bool ShowFiltersDialog(HWND owner, bool& enabled, std::vector<config::FilterItem>& filters,
                       std::vector<std::wstring>& activeFilters);

}  // namespace filter_dlg
