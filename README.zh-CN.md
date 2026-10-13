# ToFolder

[English](README.md) | 简体中文

Windows 托盘驻留小工具：监控指定文件夹，从屏幕右侧滑出面板浏览文件。纯 Win32 / C++ 实现，无 .NET 或其他运行时依赖。

## 功能

- 无主窗口，系统托盘驻留；单实例运行
- 监控可配置文件夹（顶层文件与子文件夹；默认不递归）
- 左键托盘图标：从右侧滑出文件列表面板（约 160ms 缓动滑入/滑出）
- 列表显示系统文件图标；按修改时间或名称排序（默认时间，文件夹排在文件之后）
- 新增文件时托盘切换为「有未读」图标；打开面板后清除
- 面板打开时托盘切换为打开文件夹图标
- 双击或 Enter 打开；目录在「展开文件夹」开启时在面板内进入，否则用资源管理器打开
- 右键列表项：打开 / 打开所在文件夹 / 删除（移入回收站）
- 拖出列表项到桌面或资源管理器：默认移动，按住 Ctrl 为复制
- 键盘：↑↓ 选择，Enter 打开，Backspace 返回上级（展开模式下），Esc 关闭
- 点击面板外或失去焦点关闭面板
- 未选择监控文件夹时，左键点击托盘会弹出「选择文件夹」对话框
- 托盘图标随任务栏深浅色自动切换黑线/白线；菜单与面板随应用深浅色主题
- 可选全局热键切换面板（托盘菜单 **Hotkey...** 设置）
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
| Hotkey... | 开启/设置全局热键（切换面板） |
| About... | 打开 GitHub 仓库页面 |
| Exit | 退出程序 |

## 面板交互（展开文件夹开启时）

- 双击 / Enter 目录：在面板内进入该目录
- 列表顶部 `..`：返回上级（不超过监控根目录）
- Backspace：返回上级
- 面板高度按内容增高，打开期间不因文件变少而缩短

## 配置

程序同目录下的 `config.ini`：

```ini
WatchPath=
SortMode=time
ShowHidden=false
ExpandFolders=false
WatchEnabled=true
HotkeyEnabled=false
Hotkey=Ctrl+Shift+F
```

| 键 | 取值 | 说明 |
|----|------|------|
| `WatchPath` | 路径 | 监控文件夹；空表示未选择 |
| `SortMode` | `time` / `name` | 列表排序 |
| `ShowHidden` | `true` / `false` | 显示隐藏文件 |
| `ExpandFolders` | `true` / `false` | 面板内展开进入子目录 |
| `WatchEnabled` | `true` / `false` | 监控新增文件（默认开启） |
| `HotkeyEnabled` | `true` / `false` | 是否注册全局热键 |
| `Hotkey` | 如 `Ctrl+Shift+F` | 热键组合（Ctrl/Alt/Shift/Win + 主键） |

也可通过托盘菜单修改，选项会写回该文件。

## 深浅色主题

- **托盘图标**：跟随「Windows 模式」（任务栏 `SystemUsesLightTheme`），浅色任务栏用黑线图标，深色用白线
- **托盘菜单 / 面板**：跟随「应用模式」（`AppsUseLightTheme`）；每次弹出菜单前按当前模式刷新

## 构建

依赖：

- Visual Studio 2022/2026（含 MSVC C++ 工具集）
- Python 3（构建时生成图标：需 `pymupdf`、`pillow`）

```bat
pip install pymupdf pillow
build.bat
```

产物：

- `out\ToFolder.exe`
- `out\config.ini`

## 使用

1. 运行 `out\ToFolder.exe`
2. 左键托盘图标选择监控文件夹（或右键 **Choose folder...**）
3. 再次左键打开/关闭右侧面板
4. 双击或 Enter 打开文件；有新文件时注意托盘图标变化
5. 需要浏览子目录时，在托盘菜单勾选 **Expand folders**

单实例运行：同时只会有一个 ToFolder 进程。

## 项目结构

```
main.cpp          入口、托盘消息、菜单、主题切换
tray.cpp          托盘图标与深浅色状态
panel.cpp         右侧滑出面板、列表、导航与右键菜单
watcher.cpp       目录监控与枚举排序
config.cpp        读写 config.ini
theme.cpp         应用/任务栏主题与暗色菜单 API
hotkey.cpp        全局热键注册与设置对话框
dpi.cpp           Per-Monitor V2 DPI
icon_load.cpp     高质量图标加载
assets/           Bootstrap Icons SVG、黑/白 ICO 生成脚本
resource.rc       清单、图标与热键对话框
build.bat         一键编译
```

## 图标来源

托盘与应用程序图标基于 [Bootstrap Icons](https://icons.getbootstrap.com/)（MIT）：

- 正常：`folder`（另有白线 `folder-light` 用于深色任务栏）
- 面板打开：`folder2-open` / `folder2-open-light`
- 有新文件：`folder-fill` / `folder-fill-light`

## 许可

代码可按项目需要自行使用。图标遵循 Bootstrap Icons 的 MIT 许可。
