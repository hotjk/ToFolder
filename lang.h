#pragma once

#include <windows.h>

namespace lang {

enum class Language {
  English = 0,
  Chinese = 1,
};

enum class Id {
#define LANG_STRING(id, en, zh) id,
#include "lang_strings.inc"
#undef LANG_STRING
  Count,
};

Language Current();
void Set(Language language);
Language DetectOs();

const wchar_t* T(Id id);

}  // namespace lang
