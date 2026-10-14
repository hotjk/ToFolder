# ToFolder

[English](README.md) | 简体中文

Windows 托盘驻留小工具：监控指定文件夹，从屏幕右侧滑出面板浏览文件。纯 Win32 / C++ 实现，无 .NET 或其他运行时依赖。

## 功能

- 无主窗口，系统托盘驻留；同一可执行文件路径单实例，不同 exe 副本可并行
- 监控可配置文件夹（顶层文件与子文件夹；默认不递归）
- 左键托盘图标：从右侧滑出文件列表面板（约 160ms 缓动滑入/滑出）
- 列表显示系统文件图标；按修改时间或名称排序（默认时间，文件夹排在文件之后）
- 新增文件时托盘切换为「有未读」图标；打开面板后清除
- 面板打开时托盘切换为打开文件夹图标
- 双击或 Enter 打开；目录在「展开文件夹」开启时在面板内进入，否则用资源管理器打开
- 右键列表项：打开 / 打开所在文件夹 / 剪切 / 复制 / 复制文件名 / 复制完整路径 / 删除（移入回收站）
- 拖出列表项到桌面或资源管理器：默认移动，按住 Ctrl 为复制
- 键盘：↑↓ 选择，Enter 打开，Backspace 返回上级（展开模式下），Esc 关闭
- 点击面板外或失去焦点关闭面板（已固定时除外）
- 未选择监控文件夹时，左键点击托盘会弹出「选择文件夹」对话框
- 托盘图标随任务栏深浅色自动切换黑线/白线；菜单与面板随应用深浅色主题
- 可选全局热键切换面板（托盘菜单 **Hotkey...** 设置）
- 文件类型过滤（默认 file / folder / image / zip / doc）：面板顶部多选 OR 芯片；托盘 **过滤...** 配置
- 可选 **Start with Windows** 开机自启（默认关闭；写入 HKCU Run）

## 托盘菜单

| 菜单项 | 说明 |
|--------|------|
| Open panel | 打开面板（未设置文件夹时禁用） |
| Open folder | 在资源管理器中打开监控文件夹 |
| Choose folder... | 选择要监控的文件夹 |
| Watch for new files | 监控文件夹新增并标记未读（默认开启） |
| Sort by time / name | 列表排序方式（勾选表示当前） |
| Show hidden files | 是否显示隐藏文件 |
| Expand folders | 在面板内进入子文件夹（含 `..` 与 Backspace） |
| Start with Windows | 登录时自动启动（默认关闭） |
| Language | English / 中文（默认英文；首次启动跟随系统 UI 语言） |
| 字体大小 | 小 / 中 / 大（默认小，即当前字号） |
| 面板大小 | 小 / 中 / 大（默认小，即当前宽度） |
| Hotkey... | 开启/设置全局热键（切换面板） |
| 过滤... | 启用过滤并编辑过滤组 |
| 帮助... | 简要功能介绍 |
| About... | 打开 GitHub 仓库页面 |
| Exit | 退出程序 |

## 面板交互（展开文件夹开启时）

- 双击 / Enter 目录：在面板内进入该目录
- 列表顶部 `..`：返回上级（不超过监控根目录）
- Backspace：返回上级
- 面板高度按内容增高，打开期间不因文件变少而缩短
- 右上角 **固定** 按钮：固定后点击外部不关闭（Esc / 托盘点击仍可关闭）

## 过滤

- 配置了过滤组时，面板顶部显示可切换芯片
- 可多选；匹配任一选中组即显示（OR）
- 未勾选任何芯片 = 显示全部
- `file`（`*`）= 全部文件；`folder`（`/`）= 文件夹；其余按扩展名
- 默认：`file`、`folder`、`image`、`zip`、`doc`（可在 **过滤...** 中编辑）
- 开启「展开文件夹」时，`..` 导航行始终保留

## 配置

程序同目录下的同名 `.ini`（如 `ToFolder.ini`）：

```ini
WatchPath=
SortMode=time
FontSize=small
PanelSize=small
ShowHidden=false
ExpandFolders=true
WatchEnabled=true
ShowFilters=true
HotkeyEnabled=false
Hotkey=Ctrl+Shift+F
FilterCount=5
Filter1Name=file
Filter1Ext=*
Filter2Name=folder
Filter2Ext=/
Filter3Name=image
Filter3Ext=jpg,jpeg,png,gif,webp,bmp,ico,tif,tiff,heic,svg
Filter4Name=zip
Filter4Ext=zip,7z,rar,tar,gz,bz2,xz,tgz
Filter5Name=doc
Filter5Ext=doc,docx,xls,xlsx,ppt,pptx,pdf,txt,md,rtf,odt,ods,odp
ActiveFilters=
```

| 键 | 取值 | 说明 |
|----|------|------|
| `WatchPath` | 路径 | 监控文件夹；空表示未选择 |
| `SortMode` | `time` / `name` | 列表排序 |
| `FontSize` | `small` / `medium` / `large` | 面板列表字号（默认 small） |
| `PanelSize` | `small` / `medium` / `large` | 面板宽度（默认 small = 360px @96dpi） |
| `ShowHidden` | `true` / `false` | 显示隐藏文件 |
| `ExpandFolders` | `true` / `false` | 面板内展开进入子目录（默认开启） |
| `WatchEnabled` | `true` / `false` | 监控新增文件（默认开启） |
| `ShowFilters` | `true` / `false` | 是否启用过滤（默认开启；在 **过滤...** 中设置）；关闭时隐藏芯片且不过滤 |
| `HotkeyEnabled` | `true` / `false` | 是否注册全局热键 |
| `Hotkey` | 如 `Ctrl+Shift+F` | 热键组合（Ctrl/Alt/Shift/Win + 主键） |
| `Language` | `en` / `zh` | 界面语言（首次启动若未配置则按系统语言写入） |
| `FilterCount` | 整数 | 过滤组数量（缺失时写入默认三项） |
| `FilterNName` / `FilterNExt` | 名称 / 扩展名 | 组名；扩展名，或 `*`（全部文件）、`/`（文件夹） |
| `ActiveFilters` | `file,image` | 当前勾选的过滤组（OR）；空表示不过滤 |

也可通过托盘菜单修改，选项会写回该文件。

## 深浅色主题

- **托盘图标**：跟随「Windows 模式」（任务栏 `SystemUsesLightTheme`），浅色任务栏用黑线图标，深色用白线
- **托盘菜单 / 面板**：跟随「应用模式」（`AppsUseLightTheme`）；每次弹出菜单前按当前模式刷新

## 构建

依赖：

- Visual Studio 2022/2026（含 MSVC C++ 工具集）
- Python 3（需 `pymupdf`、`pillow`；仅在 `.ico` 缺失或比 `.svg` 旧时用于生成图标，可用 `build.bat icons` 强制生成）

```bat
pip install pymupdf pillow
build.bat
```

产物：

- `out\ToFolder.exe`
- `out\ToFolder.ini`

## 使用

1. 运行 `out\ToFolder.exe`
2. 左键托盘图标选择监控文件夹（或右键 **Choose folder...**）
3. 再次左键打开/关闭右侧面板
4. 双击或 Enter 打开文件；有新文件时注意托盘图标变化
5. 需要浏览子目录时，在托盘菜单勾选 **Expand folders**

同一可执行文件路径只允许一个进程；复制或重命名后的 exe 可另开实例（各自使用同名 `.ini`）。

## 项目结构

```
main.cpp          入口、托盘消息、菜单、主题切换
tray.cpp          托盘图标与深浅色状态
panel.cpp         右侧滑出面板、列表、导航与右键菜单
watcher.cpp       目录监控与枚举排序
config.cpp        读写与 exe 同名的 .ini（UTF-8）
theme.cpp         应用/任务栏主题与暗色菜单 API
hotkey.cpp        全局热键注册与设置对话框
filter_dlg.cpp    过滤组配置对话框
help_dlg.cpp      帮助对话框（功能简介）
lang.cpp          英/中界面字符串（列表见 lang_strings.inc）
dpi.cpp           Per-Monitor V2 DPI
icon_load.cpp     高质量图标加载
slide_anim.cpp    面板滑入/滑出动画
clipboard.cpp     复制文本 / 剪切复制文件到剪贴板
drop_source.h     拖拽用 IDropSource
path_util.cpp     路径工具（上级目录、包含判断、扩展名）
str_util.cpp      字符串工具（Trim、分割、UTF-8 转换）
ui_util.cpp       工作区与对话框居中
win_compat.h      旧版 SDK 缺失常量
assets/           Bootstrap Icons SVG、黑/白 ICO 生成脚本
resource.rc       清单、图标与对话框
build.bat         一键编译
```

## 图标来源

托盘与应用程序图标基于 [Bootstrap Icons](https://icons.getbootstrap.com/)（MIT）：

- 正常：`folder`（16/32/48/256 PNG ICO；兼作 exe 图标）
- 面板打开：`folder2-open`
- 有新文件：`folder-fill`
- 深色任务栏：运行时将黑线重着色为白线（不再嵌入 light ICO）

## 许可

代码可按项目需要自行使用。图标遵循 Bootstrap Icons 的 MIT 许可。
