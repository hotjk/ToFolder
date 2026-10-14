#pragma once

#include <oleidl.h>
#include <windows.h>

// Minimal IDropSource for DoDragDrop: drop on left-button release, cancel on Esc.
class SimpleDropSource final : public IDropSource {
 public:
  SimpleDropSource() = default;

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
      delete this;
    }
    return static_cast<ULONG>(ref);
  }

  HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escapePressed, DWORD keyState) override {
    if (escapePressed) {
      return DRAGDROP_S_CANCEL;
    }
    if ((keyState & MK_LBUTTON) == 0) {
      return DRAGDROP_S_DROP;
    }
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD /*dwEffect*/) override {
    return DRAGDROP_S_USEDEFAULTCURSORS;
  }

 private:
  LONG ref_ = 1;
};
