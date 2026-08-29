#ifndef DRAGANDDROP_HPP
#define DRAGANDDROP_HPP





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
#include <ole2.h>
#include "PluginUI.hpp"

START_NAMESPACE_DISTRHO

class MyOleDropTarget : public IDropTarget
{
private:
    ULONG m_refCount = 1;
    ImGuiPluginUI* m_ui = nullptr;

public:
    MyOleDropTarget(ImGuiPluginUI* ui);

    // Standard COM Plumbing Functions
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();
    // This forces FL Studio/Wine to change the cursor to a "Copy File" symbol
    STDMETHODIMP DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
    STDMETHODIMP DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
    STDMETHODIMP DragLeave();
    // THE MAGIC MOMENT: This runs when you let go of the mouse!
    STDMETHODIMP Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
};
END_NAMESPACE_DISTRHO
#endif // DRAGANDDROP_HPP
