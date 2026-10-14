@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "ROOT=%~dp0"
cd /d "%ROOT%"

where cl >nul 2>&1
if errorlevel 1 (
  set "VCVARS="
  if exist "%ProgramFiles%\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
  ) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
  ) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
      if exist "%%i\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
    )
  )
  if not defined VCVARS (
    echo [ERROR] Visual Studio C++ tools not found. Install VS Build Tools with MSVC.
    exit /b 1
  )
  call "!VCVARS!" >nul
)

if not exist "out" mkdir out

rem Regenerate .ico only when one is missing or older than its .svg (or with "build.bat icons").
set "NEED_ICONS="
if /i "%~1"=="icons" set "NEED_ICONS=1"
if not defined NEED_ICONS (
  powershell -NoProfile -Command "$stale = Get-ChildItem assets\*.svg | Where-Object { $ico = [IO.Path]::ChangeExtension($_.FullName, '.ico'); -not (Test-Path $ico) -or (Get-Item $ico).LastWriteTime -lt $_.LastWriteTime }; if ($stale) { exit 1 }"
  if errorlevel 1 set "NEED_ICONS=1"
)
if defined NEED_ICONS (
  echo Generating multi-size icons ...
  python assets\gen_icons.py
  if errorlevel 1 (
    echo [ERROR] Icon generation failed. Python + pymupdf + pillow required.
    exit /b 1
  )
) else (
  echo Icons up to date, skipping generation.
)

echo Compiling resources ...
rc /nologo /fo out\resource.res resource.rc
if errorlevel 1 (
  echo [ERROR] Resource compile failed.
  exit /b 1
)

echo Building ToFolder.exe ...
cl /nologo /EHsc /std:c++17 /utf-8 /permissive- /O2 /GL /Gy /W4 /DUNICODE /D_UNICODE ^
  /Fe:out\ToFolder.exe ^
  /Fo:out\ ^
  main.cpp config.cpp watcher.cpp tray.cpp panel.cpp dpi.cpp icon_load.cpp theme.cpp hotkey.cpp filter_dlg.cpp help_dlg.cpp lang.cpp ^
  str_util.cpp ui_util.cpp path_util.cpp clipboard.cpp slide_anim.cpp ^
  out\resource.res ^
  /link /SUBSYSTEM:WINDOWS /MANIFEST:NO /LTCG /OPT:REF /OPT:ICF ^
  user32.lib gdi32.lib shell32.lib ole32.lib comdlg32.lib comctl32.lib advapi32.lib shcore.lib dwmapi.lib uxtheme.lib

if errorlevel 1 (
  echo [ERROR] Build failed.
  exit /b 1
)

copy /Y ToFolder.ini out\ToFolder.ini >nul
copy /Y assets\folder.ico out\folder.ico >nul
copy /Y assets\folder2-open.ico out\folder2-open.ico >nul
copy /Y assets\folder-fill.ico out\folder-fill.ico >nul

echo.
where python >nul 2>&1
if not errorlevel 1 (
  echo Verifying icons embedded in EXE ...
  python assets\verify_exe_icons.py out\ToFolder.exe
)

echo Refreshing shell icon cache ...
ie4uinit.exe -show >nul 2>&1
powershell -NoProfile -Command "Add-Type -AssemblyName System.Runtime.InteropServices; $sig='[DllImport(\"shell32.dll\")]public static extern void SHChangeNotify(int e,uint f,IntPtr i,IntPtr d);'; $t=Add-Type -MemberDefinition $sig -Name S -Namespace N -PassThru; $t::SHChangeNotify(0x8000000,0x1000,[IntPtr]::Zero,[IntPtr]::Zero)" >nul 2>&1

echo.
echo Build OK: %ROOT%out\ToFolder.exe
echo If Explorer still shows a blurry file icon, restart Explorer or rename the exe once to bypass cache.
exit /b 0


