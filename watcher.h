#pragma once

#include "config.h"

#include <windows.h>

#include <functional>
#include <string>
#include <vector>

struct FileEntry {
  std::wstring name;
  std::wstring fullPath;
  FILETIME lastWrite = {};
  bool isDirectory = false;
  int iconIndex = -1;  // SHGFI_SYSICONINDEX into the shell system image list
};

class DirectoryWatcher {
 public:
  // Invoked on the watcher thread. `hasAdded` is true when entries were added or renamed
  // in, or when the change buffer overflowed and the exact changes are unknown.
  using Callback = std::function<void(bool hasAdded)>;

  DirectoryWatcher();
  ~DirectoryWatcher();

  DirectoryWatcher(const DirectoryWatcher&) = delete;
  DirectoryWatcher& operator=(const DirectoryWatcher&) = delete;

  // Must be set before Start(); the callback is read from the watcher thread.
  void SetCallback(Callback cb);
  bool Start(const std::wstring& path);
  void Stop();

  // UI thread only (uses an unsynchronized icon cache).
  static std::vector<FileEntry> Enumerate(const std::wstring& path,
                                          config::SortMode sortMode = config::SortMode::ByTime,
                                          bool showHidden = false);
  // Small system icon index for a path, resolved from its name only (no disk access).
  static int IconIndexFor(const std::wstring& name, bool isDirectory);
  static bool PathExists(const std::wstring& path);

 private:
  static DWORD WINAPI ThreadProc(LPVOID param);
  void WatchLoop();

  std::wstring path_;
  Callback callback_;
  HANDLE thread_ = nullptr;
  HANDLE stopEvent_ = nullptr;
};
