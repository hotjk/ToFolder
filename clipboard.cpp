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

bool CopyFileItem(HWND owner, const std::wstring& path, bool cut) {
  // DROPFILES + wide path list terminated by an extra NUL.
  const size_t pathBytes = (path.size() + 1) * sizeof(wchar_t);
  const size_t total = sizeof(DROPFILES) + pathBytes + sizeof(wchar_t);
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
  memcpy(dest, path.c_str(), pathBytes);
  dest[path.size()] = L'\0';
  dest[path.size() + 1] = L'\0';
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
