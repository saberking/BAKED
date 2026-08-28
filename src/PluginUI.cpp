/*
 * ImGui plugin example
 * Copyright (C) 2021 Jean Pierre Cimalando <jp-dev@inbox.ru>
 * Copyright (C) 2021-2022 Filipe Coelho <falktx@falktx.com>
 * SPDX-License-Identifier: ISC
 */

#include "DistrhoUI.hpp"
#include "ResizeHandle.hpp"
#include "Parameters.hpp"

#include <windows.h>
#include <shellapi.h>
#include <commctrl.h> // Required for subclassing functions
START_NAMESPACE_DISTRHO

class ImGuiPluginUI;


// --------------------------------------------------------------------------------------------------------------------

class ImGuiPluginUI : public UI
{
    float fRelease = 0.0f;
    char sampleFilePath[256];

    ResizeHandle fResizeHandle;

    // ----------------------------------------------------------------------------------------------------------------

public:
   /**
      UI class constructor.
      The UI should be initialized to a default state that matches the plugin side.
    */
    ImGuiPluginUI();
    void setDroppedFilePath(const char* path)
    {
        strcpy(sampleFilePath, path);
    }

protected:
    // ----------------------------------------------------------------------------------------------------------------
    // DSP/Plugin Callbacks

   /**
      A parameter has changed on the plugin side.@n
      This is called by the host to inform the UI about parameter changes.
    */
    void parameterChanged(uint32_t index, float value) override
    {
        //DISTRHO_SAFE_ASSERT_RETURN(index == 0,);

        fRelease = value;
        repaint();
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Widget Callbacks

   /**
      ImGui specific onDisplay function.
    */
    void onImGuiDisplay() override;


    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImGuiPluginUI)
};
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
            ImGuiPluginUI* myUI = reinterpret_cast<ImGuiPluginUI*>(dwRefData);

            // Pass the path directly to our plugin variable
            myUI->setDroppedFilePath(droppedPath);
        }

        DragFinish(hDrop);
        return 0; // Tell Windows we handled the file drop safely
    }

    // Let the template plugin handle all other inputs (mouse, keys) normally!
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

// --------------------------------------------------------------------------------------------------------------------
ImGuiPluginUI::ImGuiPluginUI()
    : UI(),
      fResizeHandle(this)
{
    const double scaleFactor = getScaleFactor();
    setGeometryConstraints(DISTRHO_UI_DEFAULT_WIDTH * scaleFactor, DISTRHO_UI_DEFAULT_HEIGHT * scaleFactor);
    strcpy(sampleFilePath, "Drop Audio File Here");

    // hide handle if UI is resizable
    if (isResizable())
        fResizeHandle.hide();
    HWND pluginHwnd = (HWND)getWindow().getNativeWindowHandle();

    if (pluginHwnd != NULL)
    {
        // Tell Windows this window is allowed to receive file drops
        DragAcceptFiles(pluginHwnd, TRUE);

        // Hook our message guard, passing 'this' specific instance as the reference data
        SetWindowSubclass(pluginHwnd, MyFileDropSubclass, 1, (DWORD_PTR)this);
    }

}
void ImGuiPluginUI::onImGuiDisplay(){
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

                ImGui::Text("%s", sampleFilePath);

            if (ImGui::IsItemDeactivated())
            {
                editParameter(kParamRelease, false);
            }
        }
        ImGui::End();
    }
}

UI* createUI()
{
    return new ImGuiPluginUI();
}

// --------------------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
