#include "clipboard.h"

#include <oleidl.h>
#include <shlobj.h>

#include <cstring>

namespace clipboard {

bool CopyText(HWND owner, const std::wstring& text) {
  if (!OpenClipboard(owner)) {
    return false;
  }
  EmptyClipboard();
  const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
  HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (!mem) {
    CloseClipboard();
    return false;
  }
  void* locked = GlobalLock(mem);
  if (!locked) {
    GlobalFree(mem);
    CloseClipboard();
    return false;
  }
  memcpy(locked, text.c_str(), bytes);
  GlobalUnlock(mem);
  if (!SetClipboardData(CF_UNICODETEXT, mem)) {
    GlobalFree(mem);
    CloseClipboard();
    return false;
  }
  CloseClipboard();
  return true;
}

bool CopyFileItems(HWND owner, const std::vector<std::wstring>& paths, bool cut) {
  if (paths.empty()) {
    return false;
  }
  // DROPFILES + wide paths, each NUL-terminated, plus an extra NUL.
  size_t chars = 1;
  for (const std::wstring& path : paths) {
    chars += path.size() + 1;
  }
  const size_t pathBytes = chars * sizeof(wchar_t);
  const size_t total = sizeof(DROPFILES) + pathBytes;
  HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, total);
  if (!mem) {
    return false;
  }
  auto* drop = static_cast<DROPFILES*>(GlobalLock(mem));
  if (!drop) {
    GlobalFree(mem);
    return false;
  }
  drop->pFiles = sizeof(DROPFILES);
  drop->fWide = TRUE;
  wchar_t* dest = reinterpret_cast<wchar_t*>(reinterpret_cast<BYTE*>(drop) + sizeof(DROPFILES));
  for (const std::wstring& path : paths) {
    const size_t bytes = (path.size() + 1) * sizeof(wchar_t);
    memcpy(dest, path.c_str(), bytes);
    dest += path.size() + 1;
  }
  GlobalUnlock(mem);

  HGLOBAL effectMem = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
  if (!effectMem) {
    GlobalFree(mem);
    return false;
  }
  auto* effect = static_cast<DWORD*>(GlobalLock(effectMem));
  if (!effect) {
    GlobalFree(effectMem);
    GlobalFree(mem);
    return false;
  }
  *effect = cut ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
  GlobalUnlock(effectMem);

  if (!OpenClipboard(owner)) {
    GlobalFree(effectMem);
    GlobalFree(mem);
    return false;
  }
  EmptyClipboard();
  if (!SetClipboardData(CF_HDROP, mem)) {
    GlobalFree(effectMem);
    GlobalFree(mem);
    CloseClipboard();
    return false;
  }
  const UINT fmtEffect = RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
  if (!fmtEffect || !SetClipboardData(fmtEffect, effectMem)) {
    GlobalFree(effectMem);
    // CF_HDROP already on clipboard; still usable as copy.
  }
  CloseClipboard();
  return true;
}

}  // namespace clipboard
