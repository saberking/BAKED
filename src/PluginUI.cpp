/*
 * ImGui plugin example
 * Copyright (C) 2021 Jean Pierre Cimalando <jp-dev@inbox.ru>
 * Copyright (C) 2021-2022 Filipe Coelho <falktx@falktx.com>
 * SPDX-License-Identifier: ISC
 */

#include "DragAndDrop.hpp"

START_NAMESPACE_DISTRHO



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

void ImGuiPluginUI::setDroppedFilePath(const char* path)
{
    strcpy(sampleFilePath, path);
}

void ImGuiPluginUI::parameterChanged(uint32_t index, float value)
{
    fRelease = value;
    repaint();
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




UI* createUI()
{
    return new ImGuiPluginUI();
}

// --------------------------------------------------------------------------------------------------------------------




END_NAMESPACE_DISTRHO
