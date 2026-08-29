#include "DragAndDrop.hpp"

START_NAMESPACE_DISTRHO


MyOleDropTarget::MyOleDropTarget(ImGuiPluginUI* ui) : m_ui(ui) {}

// Standard COM Plumbing Functions
STDMETHODIMP MyOleDropTarget::QueryInterface(REFIID riid, void** ppvObj) {
    if (riid == IID_IUnknown || riid == IID_IDropTarget) {
        *ppvObj = static_cast<IDropTarget*>(this);
        AddRef();
        return S_OK;
    }
    *ppvObj = NULL; return E_NOINTERFACE;
}
STDMETHODIMP_(ULONG) MyOleDropTarget::AddRef() { return InterlockedIncrement(&m_refCount); }
STDMETHODIMP_(ULONG) MyOleDropTarget::Release() {
    ULONG res = InterlockedDecrement(&m_refCount);
    if (res == 0) delete this; return res;
}

// This forces FL Studio/Wine to change the cursor to a "Copy File" symbol
STDMETHODIMP MyOleDropTarget::DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    *pdwEffect = DROPEFFECT_COPY; return S_OK;
}
STDMETHODIMP MyOleDropTarget::DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    *pdwEffect = DROPEFFECT_COPY; return S_OK;
}
STDMETHODIMP MyOleDropTarget::DragLeave() { return S_OK; }

// THE MAGIC MOMENT: This runs when you let go of the mouse!



STDMETHODIMP MyOleDropTarget::Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
    FORMATETC fmt = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stg;

    // Extract the file block from the rich OLE Data Object
    if (pDataObj->GetData(&fmt, &stg) == S_OK)
    {
        HDROP hDrop = (HDROP)GlobalLock(stg.hGlobal);
        char droppedPath[MAX_PATH];

        if (DragQueryFileA(hDrop, 0, droppedPath, MAX_PATH))
        {
            // Send the path straight to your ImGui variable!
            m_ui->setDroppedFilePath(droppedPath);
        }

        GlobalUnlock(stg.hGlobal);
        ReleaseStgMedium(&stg);
    }

    *pdwEffect = DROPEFFECT_COPY;
    return S_OK;
}
END_NAMESPACE_DISTRHO
