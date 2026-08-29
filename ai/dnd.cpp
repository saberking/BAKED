The Minimal OLE Drop Target Code (No Framework Needed)This is standard C++ Windows code that implements Microsoft's IDropTarget interface. It will capture the modern file stream that Dolphin throws, extract the path, and hand it to your template plugin.Paste this code directly into the top of your PluginUI.cpp:cpp

#include <windows.h>
#include <shellapi.h>
#include <ole2.h> // Required for OLE Drag and Drop

    START_NAMESPACE_DISTRHO

    class ImGuiPluginUI;

// --------------------------------------------------------------------------------------------------------------------
// A clean, lightweight OLE Interceptor that forces Windows to extract files for us
class MyOleDropTarget : public IDropTarget
{
private:
    ULONG m_refCount = 1;
    ImGuiPluginUI* m_ui = nullptr;

public:
    MyOleDropTarget(ImGuiPluginUI* ui) : m_ui(ui) {}

    // Standard COM Plumbing Functions
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) {
        if (riid == IID_IUnknown || riid == IID_IDropTarget) {
            *ppvObj = static_cast<IDropTarget*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObj = NULL; return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_refCount); }
    STDMETHODIMP_(ULONG) Release() {
        ULONG res = InterlockedDecrement(&m_refCount);
        if (res == 0) delete this; return res;
    }

    // This forces FL Studio/Wine to change the cursor to a "Copy File" symbol
    STDMETHODIMP DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
        *pdwEffect = DROPEFFECT_COPY; return S_OK;
    }
    STDMETHODIMP DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
        *pdwEffect = DROPEFFECT_COPY; return S_OK;
    }
    STDMETHODIMP DragLeave() { return S_OK; }

    // THE MAGIC MOMENT: This runs when you let go of the mouse!
    STDMETHODIMP Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
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
};
Use code with caution.Step 2: Update Your Constructor to Use the OLE TargetNow, delete your old SetWindowSubclass loops entirely. Inside your ImGuiPluginUI::ImGuiPluginUI() constructor, initialize the OLE library and bind our new target straight onto the template window handle:cpp
ImGuiPluginUI::ImGuiPluginUI()
    : UI(),
    fResizeHandle(this)
{
    const double scaleFactor = getScaleFactor();
    setGeometryConstraints(DISTRHO_UI_DEFAULT_WIDTH * scaleFactor, DISTRHO_UI_DEFAULT_HEIGHT * scaleFactor);

    strcpy(sampleFilePath, "Drop Audio File Here");

    if (isResizable())
        fResizeHandle.hide();

    // 1. Initialize Windows OLE engine (Crucial for Drag & Drop!)
    OleInitialize(NULL);

    // 2. Grab the window handle from the DPF template context
    HWND pluginHwnd = (HWND)getWindow().getNativeWindowHandle();

    if (pluginHwnd != NULL)
    {
        // 3. Create our custom OLE interceptor instance
        MyOleDropTarget* dropTarget = new MyOleDropTarget(this);

        // 4. Force the operating system to map our interceptor onto the plugin view window
        RegisterDragDrop(pluginHwnd, dropTarget);
    }
}
