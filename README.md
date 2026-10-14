# ToFolder

English | [简体中文](README.zh-CN.md)

A Windows tray utility that watches a folder and slides a file list in from the right edge of the screen. Pure Win32 / C++ — no .NET or other runtime.

## Features

- No main window; lives in the system tray (single instance)
- Watches a configurable folder (top-level files and subfolders; not recursive by default)
- Left-click the tray icon to slide in the file list panel (~160 ms ease in/out)
- System file icons; sort by modified time or name (default: time; folders after files)
- New files switch the tray to an “unread” icon; opening the panel clears it
- Open-folder tray icon while the panel is visible
- Double-click or Enter to open; with **Expand folders**, directories navigate inside the panel, otherwise Explorer opens them
- Item context menu: Open / Open containing folder / Delete (Recycle Bin)
- Drag items to the desktop or Explorer: move by default, Ctrl to copy
- Keyboard: ↑↓ select, Enter open, Backspace go up (expand mode), Esc close
- Click outside or lose focus to dismiss the panel (unless pinned)
- If no watch folder is set, left-click opens a folder picker
- Tray glyphs follow taskbar light/dark (runtime recolor); menus and panel follow app light/dark
- Optional global hotkey to toggle the panel (**Hotkey...** in the tray menu)
- File type filters (default file / folder / image / zip / doc): multi-select OR chips on the panel; configure via **Filters...**
- Optional **Start with Windows** (off by default; HKCU Run)

## Tray menu

| Item | Description |
|------|-------------|
| Open panel | Show the panel (disabled if no folder is set) |
| Open folder | Open the watch folder in Explorer |
| Choose folder... | Pick the folder to watch |
| Watch for new files | Monitor the folder and mark unread on new items (on by default) |
| Sort by time / name | List sort order (check marks current) |
| Show hidden files | Include hidden items |
| Expand folders | Navigate into subfolders in the panel (`..` and Backspace) |
| Start with Windows | Launch at sign-in (off by default) |
| Language | English / 中文 (default English; first run follows OS UI language) |
| Hotkey... | Enable / change the global hotkey (toggle panel) |
| Filters... | Enable filters and edit filter groups |
| About... | Open the GitHub repository |
| Exit | Quit |

## Panel (when Expand folders is on)

- Double-click / Enter a directory to enter it in the panel
- Top `..` row: go up (not above the watch root)
- Backspace: go up
- Panel height grows with content and does not shrink while open
- Top-right **pin** keeps the panel open when clicking outside (Esc / tray click still closes)

## Filters

- When filter groups exist, chips appear at the top of the panel
- Click chips to toggle; multiple chips use OR (union of matches)
- No chip selected = show everything
- `file` (`*`) = all files; `folder` (`/`) = folders; others match extensions
- Defaults: `file`, `folder`, `image`, `zip`, `doc` (editable in **Filters...**)
- `..` navigation row is always kept when Expand folders is on

## Configuration

`config.ini` next to the executable:

```ini
WatchPath=
SortMode=time
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

| Key | Values | Description |
|-----|--------|-------------|
| `WatchPath` | path | Folder to watch; empty if unset |
| `SortMode` | `time` / `name` | List sort |
| `ShowHidden` | `true` / `false` | Show hidden files |
| `ExpandFolders` | `true` / `false` | In-panel folder navigation (default on) |
| `WatchEnabled` | `true` / `false` | Watch for new files (default on) |
| `ShowFilters` | `true` / `false` | Enable filters (default on; set in **Filters...**); off hides chips and disables filtering |
| `HotkeyEnabled` | `true` / `false` | Register the global hotkey |
| `Hotkey` | e.g. `Ctrl+Shift+F` | Chord (Ctrl/Alt/Shift/Win + key) |
| `Language` | `en` / `zh` | UI language (written on first run from OS if unset) |
| `FilterCount` | integer | Number of filter groups (defaults seeded if missing) |
| `FilterNName` / `FilterNExt` | name / exts | Group name; exts, or `*` (all files), `/` (folders) |
| `ActiveFilters` | `file,image` | Currently toggled-on filter names (OR); empty = no filter |

Tray menu changes are written back to this file.

## Light / dark theme

- **Tray icons**: follow Windows mode (`SystemUsesLightTheme`) — black strokes on a light taskbar, white on dark
- **Tray menus / panel**: follow app mode (`AppsUseLightTheme`); menus refresh from the current mode when opened

## Build

Requirements:

- Visual Studio 2022/2026 (MSVC C++ tools)
- Python 3 (icon generation: `pymupdf`, `pillow`)

```bat
pip install pymupdf pillow
build.bat
```

Output:

- `out\ToFolder.exe`
- `out\config.ini`

## Usage

1. Run `out\ToFolder.exe`
2. Left-click the tray icon to choose a watch folder (or use **Choose folder...**)
3. Left-click again to open/close the side panel
4. Double-click or Enter to open files; watch the tray icon for new arrivals
5. Enable **Expand folders** in the tray menu to browse subdirectories

Only one ToFolder process runs at a time.

## Project layout

```
main.cpp          Entry, tray messages, menus, theme refresh
tray.cpp          Tray icon and light/dark variants
panel.cpp         Slide-out panel, list, navigation, context menu
watcher.cpp       Directory watch and enumeration/sort
config.cpp        config.ini read/write
theme.cpp         App/taskbar theme and dark menu APIs
hotkey.cpp        Global hotkey registration and settings dialog
filter_dlg.cpp    Filter group editor dialog
lang.cpp          English / Chinese UI strings
dpi.cpp           Per-Monitor V2 DPI
icon_load.cpp     High-quality icon loading
assets/           Bootstrap Icons SVGs and compact ICO scripts
resource.rc       Manifest, icons, dialogs
build.bat         One-shot build
```

## Icons

Tray and app icons are based on [Bootstrap Icons](https://icons.getbootstrap.com/) (MIT):

- Idle: `folder` (16/32/48/256 PNG ICO; shared as the exe icon)
- Panel open: `folder2-open`
- Unread: `folder-fill`
- Dark taskbar: black strokes are recolored to white at runtime (no duplicate light ICOs)

## License

Use the code as you like for this project. Icons follow the Bootstrap Icons MIT license.
