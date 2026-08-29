#ifndef DRAGANDDROP_HPP
#define DRAGANDDROP_HPP

#ifndef WINVER
#define WINVER 0x0601
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <shellapi.h>
#include <ole2.h>
#include "DistrhoUI.hpp"

START_NAMESPACE_DISTRHO

class FileDropReceiver {
public:
    virtual ~FileDropReceiver() = default;
    virtual void setDroppedFilePath(const char* path) = 0;
    virtual Window& getWindow() const = 0;
};

class MyOleDropTarget : public IDropTarget
{
private:
    ULONG m_refCount = 1;
    FileDropReceiver* m_receiver = nullptr;

public:
    MyOleDropTarget(FileDropReceiver* receiver) : m_receiver(receiver) {
        OleInitialize(NULL);

        // Grab the window handle from the DPF template context
        HWND pluginHwnd = (HWND)receiver->getWindow().getNativeWindowHandle();
        // Force the operating system to map our interceptor onto the plugin view window
        RegisterDragDrop(pluginHwnd, this);
    }

    // Standard COM Plumbing Functions
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) {
        if (riid == IID_IUnknown || riid == IID_IDropTarget) {
            *ppvObj = static_cast<IDropTarget*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObj = NULL;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() {
        return InterlockedIncrement(&m_refCount);
    }

    STDMETHODIMP_(ULONG) Release() {
        ULONG res = InterlockedDecrement(&m_refCount);
        if (res == 0) delete this;
        return res;
    }

    STDMETHODIMP DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
        *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
    }

    STDMETHODIMP DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
        *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
    }

    STDMETHODIMP DragLeave() {
        return S_OK;
    }

    STDMETHODIMP Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
        FORMATETC fmt = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM stg;

        if (pDataObj->GetData(&fmt, &stg) == S_OK)
        {
            HDROP hDrop = (HDROP)GlobalLock(stg.hGlobal);
            char droppedPath[MAX_PATH];

            if (DragQueryFileA(hDrop, 0, droppedPath, MAX_PATH))
            {
                // Send the path straight through the interface!
                if (m_receiver != nullptr) {
                    m_receiver->setDroppedFilePath(droppedPath);
                }
            }

            GlobalUnlock(stg.hGlobal);
            ReleaseStgMedium(&stg);
        }

        *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
    }
};

END_NAMESPACE_DISTRHO
#endif // DRAGANDDROP_HPP
