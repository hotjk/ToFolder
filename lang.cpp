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
    {L"Font size", L"字体大小"},
    {L"Small", L"小"},
    {L"Medium", L"中"},
    {L"Large", L"大"},
    {L"Panel size", L"面板大小"},
    {L"Small", L"小"},
    {L"Medium", L"中"},
    {L"Large", L"大"},
    {L"Hotkey...", L"热键..."},
    {L"Filters...", L"过滤..."},
    {L"Help...", L"帮助..."},
    {L"About...", L"关于..."},
    {L"Exit", L"退出"},
    {L"Could not update the Windows startup setting.", L"无法更新开机自启动设置。"},
    {L"ToFolder\n\nGitHub:\nhttps://github.com/hotjk/ToFolder\n\nOpen the repository in your browser?",
     L"ToFolder\n\nGitHub:\nhttps://github.com/hotjk/ToFolder\n\n是否在浏览器中打开仓库？"},
    {L"About ToFolder", L"关于 ToFolder"},
    {L"Open", L"打开"},
    {L"Open containing folder", L"打开所在文件夹"},
    {L"Cut", L"剪切"},
    {L"Copy", L"复制"},
    {L"Copy name", L"复制文件名"},
    {L"Copy path", L"复制完整路径"},
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
    {L"Help", L"帮助"},
    {L"ToFolder — quick help\r\n"
     L"\r\n"
     L"Basics\r\n"
     L"• Left-click the tray icon to open/close the side panel.\r\n"
     L"• Choose a watch folder from the tray menu (or on first click).\r\n"
     L"• New files can mark the tray icon as unread when watching is on.\r\n"
     L"\r\n"
     L"Panel\r\n"
     L"• Double-click / Enter opens files; with Expand folders, directories open inside the panel.\r\n"
     L"• Pin (top-right) keeps the panel open when clicking outside.\r\n"
     L"• Esc or tray click still closes it.\r\n"
     L"• Drag items out to Explorer (Move; Ctrl = Copy).\r\n"
     L"• Right-click: Open, location, Cut/Copy/name/path, Delete.\r\n"
     L"\r\n"
     L"Filters\r\n"
     L"• Enable and edit groups in Filters… (file/folder/image/zip/doc by default).\r\n"
     L"• Multi-select chips use OR matching.\r\n"
     L"\r\n"
     L"Settings\r\n"
     L"• Font size / Panel size, language, hotkey, Start with Windows — all in the tray menu.\r\n"
     L"• Options are saved next to the exe in config.ini.",
     L"ToFolder — 快速帮助\r\n"
     L"\r\n"
     L"基本\r\n"
     L"• 左键托盘图标：打开/关闭右侧面板。\r\n"
     L"• 在托盘菜单中选择监控文件夹（首次点击也可选择）。\r\n"
     L"• 开启监控后，新增文件可将托盘标为未读。\r\n"
     L"\r\n"
     L"面板\r\n"
     L"• 双击 / Enter 打开文件；开启「展开文件夹」后，目录在面板内进入。\r\n"
     L"• 右上角固定：固定后点击外部不关闭。\r\n"
     L"• Esc 或再点托盘仍可关闭。\r\n"
     L"• 可拖出到资源管理器（默认移动，Ctrl 复制）。\r\n"
     L"• 右键：打开、所在位置、剪切/复制/文件名/路径、删除。\r\n"
     L"\r\n"
     L"过滤\r\n"
     L"• 在「过滤…」中启用并编辑过滤组（默认 file/folder/image/zip/doc）。\r\n"
     L"• 顶部芯片可多选，按 OR（并集）匹配。\r\n"
     L"\r\n"
     L"设置\r\n"
     L"• 字体大小 / 面板大小、语言、热键、开机自启等均在托盘菜单。\r\n"
     L"• 选项保存在程序同目录的 config.ini。"},
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
