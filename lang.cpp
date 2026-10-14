#include "lang.h"

namespace lang {
namespace {

Language g_language = Language::English;

struct Strings {
  const wchar_t* en;
  const wchar_t* zh;
};

constexpr Strings kStrings[] = {
#define LANG_STRING(id, en, zh) {en, zh},
#include "lang_strings.inc"
#undef LANG_STRING
};
static_assert(sizeof(kStrings) / sizeof(kStrings[0]) == static_cast<size_t>(Id::Count),
              "string table out of sync with lang::Id");

}  // namespace

Language Current() {
  return g_language;
}

void Set(Language language) {
  g_language = language;
}

Language DetectOs() {
  const LANGID id = GetUserDefaultUILanguage();
  if (PRIMARYLANGID(id) == LANG_CHINESE) {
    return Language::Chinese;
  }
  return Language::English;
}

const wchar_t* T(Id id) {
  const int index = static_cast<int>(id);
  if (index < 0 || index >= static_cast<int>(Id::Count)) {
    return L"";
  }
  return g_language == Language::Chinese ? kStrings[index].zh : kStrings[index].en;
}

}  // namespace lang
