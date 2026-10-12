# ToFolder

Windows 托盘驻留小工具：监控指定文件夹，从屏幕右侧滑出面板浏览文件。纯 Win32 / C++ 实现，无 .NET 或其他运行时依赖。

## 功能

- 无主窗口，系统托盘驻留
- 监控可配置文件夹（顶层文件与子文件夹，不递归）
- 点击托盘图标：从右侧滑出文件列表面板（按修改时间倒序）
- 新增文件/文件夹时切换为 `folder-plus` 图标；打开面板后恢复
- 面板打开时使用 `folder2-open` 图标
- 双击列表项用默认程序打开，打开后自动收起面板
- 点击面板外部或按 `Esc` 关闭面板
- 未选择监控文件夹时，左键点击托盘会弹出「选择文件夹」对话框

## 托盘菜单

| 菜单项 | 说明 |
|--------|------|
| Open panel | 打开面板（未设置文件夹时禁用） |
| Open folder | 在资源管理器中打开监控文件夹 |
| Choose folder... | 选择要监控的文件夹 |
| Exit | 退出程序 |

## 配置

程序同目录下的 `config.ini`：

```ini
WatchPath=D:\Your\Folder
```

也可通过托盘右键 **Choose folder...** 选择，路径会写回该文件。

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
- `out\folder*.ico`（多尺寸图标副本，便于查看）

## 使用

1. 运行 `out\ToFolder.exe`
2. 左键托盘图标选择监控文件夹（或右键菜单选择）
3. 再次左键打开/关闭右侧面板
4. 双击文件打开；有新文件时注意托盘图标变化

单实例运行：同时只会有一个 ToFolder 进程。

## 项目结构

```
main.cpp          入口、托盘消息、面板显隐
tray.cpp          托盘图标与状态切换
panel.cpp         右侧滑出面板与文件列表
watcher.cpp       目录监控与枚举
config.cpp        读写 config.ini
dpi.cpp           Per-Monitor DPI
icon_load.cpp     高质量图标加载
assets/           Bootstrap Icons SVG / 生成脚本
resource.rc       清单与图标资源
build.bat         一键编译
```

## 图标来源

托盘与应用程序图标基于 [Bootstrap Icons](https://icons.getbootstrap.com/)（MIT）：

- 正常：`folder`
- 面板打开：`folder2-open`
- 有新文件：`folder-plus`

## 许可

代码可按项目需要自行使用。图标遵循 Bootstrap Icons 的 MIT 许可。
