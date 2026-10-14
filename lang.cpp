#include "lang.h"

namespace lang {
namespace {

Language g_language = Language::English;

struct Strings {
  const wchar_t* en;
  const wchar_t* zh;
};

// Keep order in sync with Id.
constexpr Strings kStrings[static_cast<int>(Id::Count)] = {
    {L"ToFolder - No folder selected", L"ToFolder - 未选择文件夹"},
    {L"ToFolder - ", L"ToFolder - "},
    {L"Select folder to watch", L"选择要监控的文件夹"},
    {L"Open panel", L"打开面板"},
    {L"Open folder", L"打开文件夹"},
    {L"Choose folder...", L"选择文件夹..."},
    {L"Watch for new files", L"监控新增文件"},
    {L"Sort by time", L"按时间排序"},
    {L"Sort by name", L"按名称排序"},
    {L"Show hidden files", L"显示隐藏文件"},
    {L"Expand folders", L"展开文件夹"},
    {L"Start with Windows", L"开机自启动"},
    {L"Language", L"语言"},
    {L"English", L"English"},
    {L"中文", L"中文"},
    {L"Hotkey...", L"热键..."},
    {L"Filters...", L"过滤..."},
    {L"About...", L"关于..."},
    {L"Exit", L"退出"},
    {L"Could not update the Windows startup setting.", L"无法更新开机自启动设置。"},
    {L"ToFolder\n\nGitHub:\nhttps://github.com/hotjk/ToFolder\n\nOpen the repository in your browser?",
     L"ToFolder\n\nGitHub:\nhttps://github.com/hotjk/ToFolder\n\n是否在浏览器中打开仓库？"},
    {L"About ToFolder", L"关于 ToFolder"},
    {L"Open", L"打开"},
    {L"Open containing folder", L"打开所在文件夹"},
    {L"Delete", L"删除"},
    {L"Failed to delete the item.", L"删除失败。"},
    {L"(Empty folder)", L"（空文件夹）"},
    {L"(Choose a folder from the tray menu)", L"（请从托盘菜单选择文件夹）"},
    {L"(Path is invalid)", L"（路径无效）"},
    {L"Hotkey", L"热键"},
    {L"Enable hotkey", L"启用热键"},
    {L"Shortcut:", L"快捷键："},
    {L"Click the box, then press a key combination.", L"点击输入框，然后按下组合键。"},
    {L"(press a shortcut)", L"（请按下快捷键）"},
    {L"OK", L"确定"},
    {L"Cancel", L"取消"},
    {L"Please set a hotkey first.", L"请先设置热键。"},
    {L"Could not register that hotkey. It may already be in use by another program.",
     L"无法注册该热键，可能已被其他程序占用。"},
    {L"Filters", L"过滤项"},
    {L"Enable filters", L"启用过滤"},
    {L"Filters:", L"过滤项："},
    {L"Name:", L"名称："},
    {L"Extensions:", L"扩展名："},
    {L"Comma-separated exts (jpg,png), or * for all files, / for folders.",
     L"逗号分隔扩展名（jpg,png），或 * 表示全部文件，/ 表示文件夹。"},
    {L"Add", L"添加"},
    {L"Remove", L"删除"},
    {L"Update", L"更新"},
    {L"Please enter a filter name.", L"请输入过滤项名称。"},
    {L"A filter with that name already exists.", L"已存在同名过滤项。"},
    {L"Pin panel", L"固定面板"},
    {L"Unpin panel", L"取消固定"},
};

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
