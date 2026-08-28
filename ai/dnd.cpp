#include "DistrhoUI.hpp"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h> // Required for DefSubclassProc and SetWindowSubclass
#pragma comment(lib, "comctl32.lib")

// 1. Your custom Windows message listener
LRESULT CALLBACK MyPluginWindowSubclass(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, 
                                        UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    // External ImGui handler for window sizing/inputs
    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
        return true;

    switch (uMsg)
    {
        case WM_DROPFILES:
        {
            HDROP hDrop = reinterpret_cast<HDROP>(wParam);
            char droppedFilePath[MAX_PATH];
            
            // Extract the first dropped file path
            if (DragQueryFileA(hDrop, 0, droppedFilePath, MAX_PATH))
            {
                // Access your UI class instance pointer passed via dwRefData
                auto* myUI = reinterpret_cast<MyPluginUI*>(dwRefData);
                myUI->setDroppedFilePath(droppedFilePath);
            }
            
            DragFinish(hDrop);
            return 0; // Tell Windows we handled this message
        }
        
        case WM_NCDESTROY:
            // Clean up the subclass hook when the window dies
            RemoveWindowSubclass(hwnd, MyPluginWindowSubclass, uIdSubclass);
            break;
    }

    // Pass all other messages (mouse clicks, movement) straight to DPF/Pugl!
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}
#endif

START_NAMESPACE_DISTRHO

class MyPluginUI : public UI
{
private:
    std::string currentFilePath = "";

public:
    MyPluginUI() : UI()
    {
#ifdef _WIN32
        HWND pluginHwnd = (HWND)getNativeWindowHandle();
        if (pluginHwnd != NULL)
        {
            // 2. Tell the OS this window accepts file drops
            DragAcceptFiles(pluginHwnd, TRUE);

            // 3. Inject our subclass listener over DPF, passing 'this' UI pointer as reference data
            SetWindowSubclass(pluginHwnd, MyPluginWindowSubclass, 1, (DWORD_PTR)this);
            
            // 4. Safely initialize Dear ImGui on top of the raw handle
            ImGui_ImplWin32_Init(pluginHwnd);
            // (Initialize your DX11 context here as well...)
        }
#endif
    }

    void setDroppedFilePath(const char* path)
    {
        currentFilePath = path;
    }

    // ... Your standard DPF UI rendering loop where you call your ImGui frames
};

END_NAMESPACE_DISTRHO
