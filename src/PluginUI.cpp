/*
 * ImGui plugin example
 * Copyright (C) 2021 Jean Pierre Cimalando <jp-dev@inbox.ru>
 * Copyright (C) 2021-2022 Filipe Coelho <falktx@falktx.com>
 * SPDX-License-Identifier: ISC
 */

// 1. CRUCIAL: Tell MinGW to unlock modern Windows OLE API features
#ifndef WINVER
#define WINVER 0x0601
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include "DistrhoUI.hpp"
#include "ResizeHandle.hpp"
#include "Parameters.hpp"

#include <windows.h>
#include <shellapi.h>
#include <ole2.h> // Required for OLE Drag and Drop

START_NAMESPACE_DISTRHO

    class ImGuiPluginUI;

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
    STDMETHODIMP Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
};

// --------------------------------------------------------------------------------------------------------------------

class ImGuiPluginUI : public UI
{
    float fRelease = 0.0f;
    char sampleFilePath[256];
    ResizeHandle fResizeHandle;

public:
    ImGuiPluginUI();

    void setDroppedFilePath(const char* path)
    {
        strcpy(sampleFilePath, path);
    }

protected:
    void parameterChanged(uint32_t index, float value) override
    {
        fRelease = value;
        repaint();
    }

    void onImGuiDisplay() override;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImGuiPluginUI)
};

// NOTE: Old MyFileDropSubclass and HookAllChildWindows functions have been removed.
// They are no longer needed because OLE RegisterDragDrop does all the work!

// --------------------------------------------------------------------------------------------------------------------
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

void ImGuiPluginUI::onImGuiDisplay()
{
    const float width = getWidth();
    const float height = getHeight();
    const float margin = 20.0f * getScaleFactor();

    ImGui::SetNextWindowPos(ImVec2(margin, margin));
    ImGui::SetNextWindowSize(ImVec2(width - 2 * margin, height - 2 * margin));

    if (ImGui::Begin("BAKED", nullptr, ImGuiWindowFlags_NoResize))
    {
        if (ImGui::SliderFloat("Release", &fRelease, 0.f, 4000.f))
        {
            if (ImGui::IsItemActivated())
                editParameter(kParamRelease, true);

            setParameterValue(kParamRelease, fRelease);
        }

        if (ImGui::IsItemDeactivated())
        {
            editParameter(kParamRelease, false);
        }

        // UI Separation Line
        ImGui::Separator();
        ImGui::Spacing();

        // Print our sample path string live inside the ImGui framework
        ImGui::Text("File Status: %s", sampleFilePath);
    }
    ImGui::End();
}

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

UI* createUI()
{
    return new ImGuiPluginUI();
}

END_NAMESPACE_DISTRHO
