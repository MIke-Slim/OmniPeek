#pragma once
#include <windows.h>
#include <unknwn.h>
#include "webview2/WebView2.h"

template <typename F>
class CoreWebView2EnvironmentCompletedHandler : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
    F m_func;
    long m_ref;
public:
    CoreWebView2EnvironmentCompletedHandler(F f) : m_func(f), m_ref(1) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG c = InterlockedDecrement(&m_ref);
        if (c == 0) delete this;
        return c;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT res, ICoreWebView2Environment* env) override {
        return m_func(res, env);
    }
};

template <typename F>
class CoreWebView2ControllerCompletedHandler : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
    F m_func;
    long m_ref;
public:
    CoreWebView2ControllerCompletedHandler(F f) : m_func(f), m_ref(1) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG c = InterlockedDecrement(&m_ref);
        if (c == 0) delete this;
        return c;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT res, ICoreWebView2Controller* controller) override {
        return m_func(res, controller);
    }
};

template <typename F>
CoreWebView2EnvironmentCompletedHandler<F>* MakeEnvHandler(F f) {
    return new CoreWebView2EnvironmentCompletedHandler<F>(f);
}

template <typename F>
CoreWebView2ControllerCompletedHandler<F>* MakeControllerHandler(F f) {
    return new CoreWebView2ControllerCompletedHandler<F>(f);
}
