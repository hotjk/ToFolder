#include "watcher.h"

#include <algorithm>
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

std::wstring DirectoryWatcher::GetPath() const {
  return path_;
}

bool DirectoryWatcher::PathExists(const std::wstring& path) {
  if (path.empty()) {
    return false;
  }
  const DWORD attrs = GetFileAttributesW(path.c_str());
  return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

std::vector<FileEntry> DirectoryWatcher::Enumerate(const std::wstring& path,
                                                   config::SortMode sortMode) {
  std::vector<FileEntry> items;
  if (!PathExists(path)) {
    return items;
  }

  const std::wstring pattern = path + L"\\*";
  WIN32_FIND_DATAW fd = {};
  HANDLE find = FindFirstFileW(pattern.c_str(), &fd);
  if (find == INVALID_HANDLE_VALUE) {
    return items;
  }

  do {
    if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) {
      continue;
    }
    FileEntry entry;
    entry.name = fd.cFileName;
    entry.fullPath = path + L"\\" + fd.cFileName;
    entry.lastWrite = fd.ftLastWriteTime;
    entry.isDirectory = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    items.push_back(std::move(entry));
  } while (FindNextFileW(find, &fd));

  FindClose(find);

  // Files first, folders always at the bottom; then apply the chosen sort within each group.
  if (sortMode == config::SortMode::ByName) {
    std::sort(items.begin(), items.end(), [](const FileEntry& a, const FileEntry& b) {
      if (a.isDirectory != b.isDirectory) {
        return !a.isDirectory && b.isDirectory;
      }
      return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
    });
  } else {
    std::sort(items.begin(), items.end(), [](const FileEntry& a, const FileEntry& b) {
      if (a.isDirectory != b.isDirectory) {
        return !a.isDirectory && b.isDirectory;
      }
      const LONG cmp = CompareFileTime(&a.lastWrite, &b.lastWrite);
      if (cmp != 0) {
        return cmp > 0;
      }
      return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
    });
  }

  return items;
}

bool DirectoryWatcher::Start(const std::wstring& path) {
  Stop();
  path_ = path;
  if (!PathExists(path_)) {
    return false;
  }

  ResetEvent(stopEvent_);
  InterlockedExchange(&running_, 1);
  thread_ = CreateThread(nullptr, 0, ThreadProc, this, 0, nullptr);
  return thread_ != nullptr;
}

void DirectoryWatcher::Stop() {
  InterlockedExchange(&running_, 0);
  if (stopEvent_) {
    SetEvent(stopEvent_);
  }
  if (dirHandle_ && dirHandle_ != INVALID_HANDLE_VALUE) {
    CancelIoEx(dirHandle_, nullptr);
  }
  if (thread_) {
    WaitForSingleObject(thread_, 5000);
    CloseHandle(thread_);
    thread_ = nullptr;
  }
  if (dirHandle_ && dirHandle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(dirHandle_);
    dirHandle_ = nullptr;
  }
}

DWORD WINAPI DirectoryWatcher::ThreadProc(LPVOID param) {
  auto* self = static_cast<DirectoryWatcher*>(param);
  self->WatchLoop();
  return 0;
}

void DirectoryWatcher::WatchLoop() {
  dirHandle_ = CreateFileW(
      path_.c_str(),
      FILE_LIST_DIRECTORY,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      nullptr,
      OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
      nullptr);

  if (dirHandle_ == INVALID_HANDLE_VALUE) {
    dirHandle_ = nullptr;
    return;
  }

  alignas(DWORD) BYTE buffer[64 * 1024];
  OVERLAPPED overlapped = {};
  overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!overlapped.hEvent) {
    return;
  }

  const DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                       FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE |
                       FILE_NOTIFY_CHANGE_ATTRIBUTES;

  while (InterlockedCompareExchange(&running_, 1, 1) == 1) {
    ResetEvent(overlapped.hEvent);
    DWORD bytesReturned = 0;
    const BOOL ok = ReadDirectoryChangesW(
        dirHandle_,
        buffer,
        sizeof(buffer),
        FALSE,
        filter,
        &bytesReturned,
        &overlapped,
        nullptr);

    if (!ok) {
      const DWORD err = GetLastError();
      if (err != ERROR_IO_PENDING) {
        break;
      }
    }

    HANDLE waits[2] = {stopEvent_, overlapped.hEvent};
    const DWORD wait = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
    if (wait == WAIT_OBJECT_0) {
      CancelIoEx(dirHandle_, &overlapped);
      break;
    }
    if (wait != WAIT_OBJECT_0 + 1) {
      break;
    }

    if (!GetOverlappedResult(dirHandle_, &overlapped, &bytesReturned, FALSE)) {
      break;
    }
    if (bytesReturned == 0) {
      continue;
    }

    bool hasAdded = false;
    BYTE* ptr = buffer;
    for (;;) {
      auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(ptr);
      if (info->Action == FILE_ACTION_ADDED ||
          info->Action == FILE_ACTION_RENAMED_NEW_NAME) {
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
  if (dirHandle_) {
    CloseHandle(dirHandle_);
    dirHandle_ = nullptr;
  }
}
