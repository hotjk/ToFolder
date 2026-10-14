// X-macro list of UI strings: LANG_STRING(Id, English, Chinese).
// Included by lang.h (to build lang::Id) and lang.cpp (to build the string table).

LANG_STRING(TipNoFolder, L"ToFolder - No folder selected", L"ToFolder - 未选择文件夹")
LANG_STRING(TipPrefix, L"ToFolder - ", L"ToFolder - ")
LANG_STRING(SelectFolderTitle, L"Select folder to watch", L"选择要监控的文件夹")
LANG_STRING(MenuOpenPanel, L"Open panel", L"打开面板")
LANG_STRING(MenuOpenFolder, L"Open folder", L"打开文件夹")
LANG_STRING(MenuChooseFolder, L"Choose folder...", L"选择文件夹...")
LANG_STRING(MenuWatchNewFiles, L"Watch for new files", L"监控新增文件")
LANG_STRING(MenuSortByTime, L"Sort by time", L"按时间排序")
LANG_STRING(MenuSortByName, L"Sort by name", L"按名称排序")
LANG_STRING(MenuShowHidden, L"Show hidden files", L"显示隐藏文件")
LANG_STRING(MenuExpandFolders, L"Expand folders", L"展开文件夹")
LANG_STRING(MenuStartWithWindows, L"Start with Windows", L"开机自启动")
LANG_STRING(MenuLanguage, L"Language", L"语言")
LANG_STRING(MenuLangEnglish, L"English", L"English")
LANG_STRING(MenuLangChinese, L"中文", L"中文")
LANG_STRING(MenuFontSize, L"Font size", L"字体大小")
LANG_STRING(MenuFontSmall, L"Small", L"小")
LANG_STRING(MenuFontMedium, L"Medium", L"中")
LANG_STRING(MenuFontLarge, L"Large", L"大")
LANG_STRING(MenuPanelSize, L"Panel size", L"面板大小")
LANG_STRING(MenuPanelSmall, L"Small", L"小")
LANG_STRING(MenuPanelMedium, L"Medium", L"中")
LANG_STRING(MenuPanelLarge, L"Large", L"大")
LANG_STRING(MenuHotkey, L"Hotkey...", L"热键...")
LANG_STRING(MenuFilters, L"Filters...", L"过滤...")
LANG_STRING(MenuHelp, L"Help...", L"帮助...")
LANG_STRING(MenuAbout, L"About...", L"关于...")
LANG_STRING(MenuExit, L"Exit", L"退出")
LANG_STRING(StartupUpdateFailed, L"Could not update the Windows startup setting.",
            L"无法更新开机自启动设置。")
LANG_STRING(AboutBody,
            L"ToFolder\n\nGitHub:\nhttps://github.com/hotjk/ToFolder\n\nOpen the repository in your browser?",
            L"ToFolder\n\nGitHub:\nhttps://github.com/hotjk/ToFolder\n\n是否在浏览器中打开仓库？")
LANG_STRING(AboutTitle, L"About ToFolder", L"关于 ToFolder")
LANG_STRING(ListOpen, L"Open", L"打开")
LANG_STRING(ListOpenLocation, L"Open containing folder", L"打开所在文件夹")
LANG_STRING(ListCut, L"Cut", L"剪切")
LANG_STRING(ListCopy, L"Copy", L"复制")
LANG_STRING(ListCopyName, L"Copy name", L"复制文件名")
LANG_STRING(ListCopyPath, L"Copy path", L"复制完整路径")
LANG_STRING(ListDelete, L"Delete", L"删除")
LANG_STRING(DeleteFailed, L"Failed to delete the item.", L"删除失败。")
LANG_STRING(EmptyFolder, L"(Empty folder)", L"（空文件夹）")
LANG_STRING(ChooseFolderHint, L"(Choose a folder from the tray menu)", L"（请从托盘菜单选择文件夹）")
LANG_STRING(PathInvalid, L"(Path is invalid)", L"（路径无效）")
LANG_STRING(HotkeyDialogTitle, L"Hotkey", L"热键")
LANG_STRING(HotkeyEnable, L"Enable hotkey", L"启用热键")
LANG_STRING(HotkeyShortcutLabel, L"Shortcut:", L"快捷键：")
LANG_STRING(HotkeyHint, L"Click the box, then press a key combination.",
            L"点击输入框，然后按下组合键。")
LANG_STRING(HotkeyPressShortcut, L"(press a shortcut)", L"（请按下快捷键）")
LANG_STRING(HotkeyOk, L"OK", L"确定")
LANG_STRING(HotkeyCancel, L"Cancel", L"取消")
LANG_STRING(HotkeyNeedSet, L"Please set a hotkey first.", L"请先设置热键。")
LANG_STRING(HotkeyConflict,
            L"Could not register that hotkey. It may already be in use by another program.",
            L"无法注册该热键，可能已被其他程序占用。")
LANG_STRING(FiltersDialogTitle, L"Filters", L"过滤项")
LANG_STRING(FiltersEnable, L"Enable filters", L"启用过滤")
LANG_STRING(FiltersListLabel, L"Filters:", L"过滤项：")
LANG_STRING(FiltersNameLabel, L"Name:", L"名称：")
LANG_STRING(FiltersExtLabel, L"Extensions:", L"扩展名：")
LANG_STRING(FiltersHint, L"Comma-separated exts (jpg,png), or * for all files, / for folders.",
            L"逗号分隔扩展名（jpg,png），或 * 表示全部文件，/ 表示文件夹。")
LANG_STRING(FiltersAdd, L"Add", L"添加")
LANG_STRING(FiltersRemove, L"Remove", L"删除")
LANG_STRING(FiltersUpdate, L"Update", L"更新")
LANG_STRING(FiltersNameRequired, L"Please enter a filter name.", L"请输入过滤项名称。")
LANG_STRING(FiltersNameDuplicate, L"A filter with that name already exists.", L"已存在同名过滤项。")
LANG_STRING(TipPin, L"Pin panel", L"固定面板")
LANG_STRING(TipUnpin, L"Unpin panel", L"取消固定")
LANG_STRING(HelpDialogTitle, L"Help", L"帮助")
LANG_STRING(HelpBody,
            L"ToFolder — quick help\r\n"
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
            L"• Options are saved next to the exe as <exe-name>.ini.",
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
            L"• 选项保存在程序同目录的同名 .ini 文件（如 ToFolder.ini）。")
