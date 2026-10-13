#pragma once

#include "config.h"

#include <windows.h>

#include <cstdint>
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
  using Callback = std::function<void(bool hasAdded)>;

  DirectoryWatcher();
  ~DirectoryWatcher();

  DirectoryWatcher(const DirectoryWatcher&) = delete;
  DirectoryWatcher& operator=(const DirectoryWatcher&) = delete;

  void SetCallback(Callback cb);
  bool Start(const std::wstring& path);
  void Stop();
  std::wstring GetPath() const;

  static std::vector<FileEntry> Enumerate(const std::wstring& path,
                                          config::SortMode sortMode = config::SortMode::ByTime);
  static bool PathExists(const std::wstring& path);

 private:
  static DWORD WINAPI ThreadProc(LPVOID param);
  void WatchLoop();

  std::wstring path_;
  Callback callback_;
  HANDLE thread_ = nullptr;
  HANDLE stopEvent_ = nullptr;
  HANDLE dirHandle_ = nullptr;
  volatile LONG running_ = 0;
};
