#pragma once

#include <oleidl.h>
#include <shlobj.h>
#include <windows.h>

// Minimal IDropSource for DoDragDrop: drop on left-button release, cancel on Esc.
// Ctrl selects copy; otherwise the preferred effect is move.
class SimpleDropSource final : public IDropSource {
 public:
  explicit SimpleDropSource(IDataObject* data) : data_(data) {
    if (data_) {
      data_->AddRef();
    }
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
    if (!ppv) {
      return E_POINTER;
    }
    if (riid == IID_IUnknown || riid == IID_IDropSource) {
      *ppv = static_cast<IDropSource*>(this);
      AddRef();
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override {
    return static_cast<ULONG>(InterlockedIncrement(&ref_));
  }

  ULONG STDMETHODCALLTYPE Release() override {
    const LONG ref = InterlockedDecrement(&ref_);
    if (ref == 0) {
      if (data_) {
        data_->Release();
        data_ = nullptr;
      }
      delete this;
    }
    return static_cast<ULONG>(ref);
  }

  HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escapePressed, DWORD keyState) override {
    if (escapePressed) {
      return DRAGDROP_S_CANCEL;
    }
    SetPreferredEffect((keyState & MK_CONTROL) != 0 ? DROPEFFECT_COPY : DROPEFFECT_MOVE);
    if ((keyState & MK_LBUTTON) == 0) {
      return DRAGDROP_S_DROP;
    }
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD /*dwEffect*/) override {
    return DRAGDROP_S_USEDEFAULTCURSORS;
  }

 private:
  void SetPreferredEffect(DWORD effect) {
    if (!data_ || effect == preferred_) {
      return;
    }
    FORMATETC fmt = {};
    fmt.cfFormat = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT));
    fmt.dwAspect = DVASPECT_CONTENT;
    fmt.lindex = -1;
    fmt.tymed = TYMED_HGLOBAL;

    STGMEDIUM medium = {};
    medium.tymed = TYMED_HGLOBAL;
    medium.hGlobal = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
    if (!medium.hGlobal) {
      return;
    }
    auto* value = static_cast<DWORD*>(GlobalLock(medium.hGlobal));
    if (!value) {
      GlobalFree(medium.hGlobal);
      return;
    }
    *value = effect;
    GlobalUnlock(medium.hGlobal);
    if (SUCCEEDED(data_->SetData(&fmt, &medium, TRUE))) {
      preferred_ = effect;
    }
  }

  IDataObject* data_ = nullptr;
  DWORD preferred_ = DROPEFFECT_NONE;
  LONG ref_ = 1;
};
