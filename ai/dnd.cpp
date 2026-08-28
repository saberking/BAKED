#include "DistrhoUI.hpp"
#include "imgui.h"

// 1. Lock all Windows-specific code inside an ifdef shield
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h> // Required for subclassing functions
#endif

START_NAMESPACE_DISTRHO

// Forward declaration of our class so the Windows function knows it exists
class MyPluginUI;

#ifdef _WIN32
// 2. The custom Windows message guard function
LRESULT CALLBACK MyFileDropSubclass(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    if (uMsg == WM_DROPFILES)
    {
        HDROP hDrop = reinterpret_cast<HDROP>(wParam);
        char droppedPath[MAX_PATH];

        // Extract the file path string from the drop event
        if (DragQueryFileA(hDrop, 0, droppedPath, MAX_PATH))
        {
            // Cast 'dwRefData' back into our specific C++ plugin instance
            MyPluginUI* myUI = reinterpret_cast<MyPluginUI*>(dwRefData);

            // Pass the path directly to our plugin variable
            myUI->setDroppedFilePath(droppedPath);
        }

        DragFinish(hDrop);
        return 0; // Tell Windows we handled the file drop safely
    }

    // Let the template plugin handle all other inputs (mouse, keys) normally!
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}
#endif

// 3. Your Main DPF UI Class
class MyPluginUI : public UI
{
private:
    std::string currentFilePath = ""; // Variable to store our loaded file path

public:
    // This is your UI constructor (runs once when the plugin window opens)
    MyPluginUI() : UI()
    {
        #ifdef _WIN32
        // Get the blank window handle that DPF already opened for us
        HWND pluginHwnd = (HWND)getNativeWindowHandle();

        if (pluginHwnd != NULL)
        {
            // Tell Windows this window is allowed to receive file drops
            DragAcceptFiles(pluginHwnd, TRUE);

            // Hook our message guard, passing 'this' specific instance as the reference data
            SetWindowSubclass(pluginHwnd, MyFileDropSubclass, 1, (DWORD_PTR)this);
        }
        #endif
    }

    // A helper function to let the Windows code update our C++ string variable
    void setDroppedFilePath(const char* path)
    {
        currentFilePath = path;
    }

    // 4. This is your standard template drawing loop
    // (Note: Depending on your template version, this might be called onImGuiDisplay)
    void postDisplay() override
    {
        // NO DIRECTX CODE NEEDED! Just use standard ImGui commands:
        ImGui::Begin("My Sample Plugin");

        if (!currentFilePath.empty())
        {
            ImGui::Text("Loaded Sample Path:");
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", currentFilePath.c_str());
        }
        else
        {
            ImGui::Text("Drag and drop a .wav file anywhere on this window...");
        }

        ImGui::End();
    }
};

UI* createUI() {
    return new MyPluginUI();
}

END_NAMESPACE_DISTRHO
