#include "watcher.h"

#include "path_util.h"

#include <shellapi.h>

#include <algorithm>
#include <unordered_map>
#include <vector>

DirectoryWatcher::DirectoryWatcher() {
  stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
}

DirectoryWatcher::~DirectoryWatcher() {
  Stop();
  if (stopEvent_) {
    CloseHandle(stopEvent_);
    stopEvent_ = nullptr;
  }
}

void DirectoryWatcher::SetCallback(Callback cb) {
  callback_ = std::move(cb);
}

bool DirectoryWatcher::PathExists(const std::wstring& path) {
  if (path.empty()) {
    return false;
  }
  const DWORD attrs = GetFileAttributesW(path.c_str());
  return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

int DirectoryWatcher::IconIndexFor(const std::wstring& name, bool isDirectory) {
  // With SHGFI_USEFILEATTRIBUTES the shell resolves the icon from the extension alone,
  // so one lookup per extension gives identical results. Folders use the "/" key.
  static std::unordered_map<std::wstring, int> cache;
  const std::wstring key = isDirectory ? L"/" : path_util::ExtensionLower(name);
  const auto it = cache.find(key);
  if (it != cache.end()) {
    return it->second;
  }
  const std::wstring probe = isDirectory ? L"folder" : L"file." + key;
  SHFILEINFOW sfi = {};
  int index = -1;
  if (SHGetFileInfoW(probe.c_str(),
                     isDirectory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL, &sfi,
                     sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)) {
    index = sfi.iIcon;
  }
  cache.emplace(key, index);
  return index;
}

std::vector<FileEntry> DirectoryWatcher::Enumerate(const std::wstring& path,
                                                   config::SortMode sortMode,
                                                   bool showHidden) {
  std::vector<FileEntry> items;
  if (!PathExists(path)) {
    return items;
  }

  const std::wstring pattern = path + L"\\*";
  WIN32_FIND_DATAW fd = {};
  HANDLE find = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch,
                                 nullptr, FIND_FIRST_EX_LARGE_FETCH);
  if (find == INVALID_HANDLE_VALUE) {
    return items;
  }

  do {
    if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) {
      continue;
    }
    if (!showHidden && (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)) {
      continue;
    }
    FileEntry entry;
    entry.name = fd.cFileName;
    entry.fullPath = path + L"\\" + fd.cFileName;
    entry.lastWrite = fd.ftLastWriteTime;
    entry.isDirectory = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    entry.iconIndex = IconIndexFor(entry.name, entry.isDirectory);
    items.push_back(std::move(entry));
  } while (FindNextFileW(find, &fd));

  FindClose(find);

  // Files first, folders always at the bottom; then apply the chosen sort within each group.
  const bool byName = sortMode == config::SortMode::ByName;
  std::sort(items.begin(), items.end(), [byName](const FileEntry& a, const FileEntry& b) {
    if (a.isDirectory != b.isDirectory) {
      return !a.isDirectory;
    }
    if (!byName) {
      const LONG cmp = CompareFileTime(&a.lastWrite, &b.lastWrite);
      if (cmp != 0) {
        return cmp > 0;
      }
    }
    return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
  });

  return items;
}

bool DirectoryWatcher::Start(const std::wstring& path) {
  Stop();
  path_ = path;
  if (!stopEvent_ || !PathExists(path_)) {
    return false;
  }
  ResetEvent(stopEvent_);
  thread_ = CreateThread(nullptr, 0, ThreadProc, this, 0, nullptr);
  return thread_ != nullptr;
}

void DirectoryWatcher::Stop() {
  if (!thread_) {
    return;
  }
  // The thread owns the directory handle and cancels its own I/O, so waiting is bounded.
  SetEvent(stopEvent_);
  WaitForSingleObject(thread_, INFINITE);
  CloseHandle(thread_);
  thread_ = nullptr;
}

DWORD WINAPI DirectoryWatcher::ThreadProc(LPVOID param) {
  auto* self = static_cast<DirectoryWatcher*>(param);
  self->WatchLoop();
  return 0;
}

void DirectoryWatcher::WatchLoop() {
  HANDLE dir = CreateFileW(path_.c_str(), FILE_LIST_DIRECTORY,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                           nullptr);
  if (dir == INVALID_HANDLE_VALUE) {
    return;
  }

  OVERLAPPED overlapped = {};
  overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!overlapped.hEvent) {
    CloseHandle(dir);
    return;
  }

  alignas(DWORD) BYTE buffer[64 * 1024];
  const DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                       FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE |
                       FILE_NOTIFY_CHANGE_ATTRIBUTES;

  for (;;) {
    ResetEvent(overlapped.hEvent);
    DWORD bytesReturned = 0;
    if (!ReadDirectoryChangesW(dir, buffer, sizeof(buffer), FALSE, filter, &bytesReturned,
                               &overlapped, nullptr) &&
        GetLastError() != ERROR_IO_PENDING) {
      break;
    }

    HANDLE waits[2] = {stopEvent_, overlapped.hEvent};
    const DWORD wait = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
    if (wait != WAIT_OBJECT_0 + 1) {
      // Stop requested (or wait failed): cancel and wait for the kernel to release `buffer`.
      CancelIoEx(dir, &overlapped);
      GetOverlappedResult(dir, &overlapped, &bytesReturned, TRUE);
      break;
    }
    if (!GetOverlappedResult(dir, &overlapped, &bytesReturned, FALSE)) {
      break;
    }

    // Zero bytes means the change buffer overflowed: something changed, details unknown.
    bool hasAdded = bytesReturned == 0;
    for (BYTE* ptr = buffer; bytesReturned != 0;) {
      auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(ptr);
      if (info->Action == FILE_ACTION_ADDED || info->Action == FILE_ACTION_RENAMED_NEW_NAME) {
        hasAdded = true;
      }
      if (info->NextEntryOffset == 0) {
        break;
      }
      ptr += info->NextEntryOffset;
    }

    if (callback_) {
      callback_(hasAdded);
    }
  }

  CloseHandle(overlapped.hEvent);
  CloseHandle(dir);
}
