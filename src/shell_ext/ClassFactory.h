#pragma once
#include <windows.h>
#include <unknwn.h>
#include <new>

extern long g_serverLocks;

template <class T>
class ClassFactory : public IClassFactory {
public:
    ClassFactory() : m_refCount(1) {
        InterlockedIncrement(&g_serverLocks);
    }
    virtual ~ClassFactory() {
        InterlockedDecrement(&g_serverLocks);
    }

    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override {
        return InterlockedIncrement(&m_refCount);
    }

    STDMETHODIMP_(ULONG) Release() override {
        ULONG count = InterlockedDecrement(&m_refCount);
        if (count == 0) delete this;
        return count;
    }

    STDMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (pUnkOuter != nullptr) return CLASS_E_NOAGGREGATION;

        T *pObj = new (std::nothrow) T();
        if (!pObj) return E_OUTOFMEMORY;

        HRESULT hr = pObj->QueryInterface(riid, ppv);
        pObj->Release();
        return hr;
    }

    STDMETHODIMP LockServer(BOOL fLock) override {
        if (fLock) InterlockedIncrement(&g_serverLocks);
        else InterlockedDecrement(&g_serverLocks);
        return S_OK;
    }

private:
    long m_refCount;
};
