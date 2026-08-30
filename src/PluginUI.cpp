/*
 * ImGui plugin example
 * Copyright (C) 2021 Jean Pierre Cimalando <jp-dev@inbox.ru>
 * Copyright (C) 2021-2022 Filipe Coelho <falktx@falktx.com>
 * SPDX-License-Identifier: ISC
 */

#include "DistrhoUI.hpp"
#include "ResizeHandle.hpp"
#include "Parameters.hpp"
#include "DragAndDrop.hpp"

START_NAMESPACE_DISTRHO

class ImGuiPluginUI : public UI, public FileDropReceiver
{
    float fRelease = 0.0f;
    char sampleFilePath[256];
    ResizeHandle fResizeHandle;

public:
    ImGuiPluginUI()
        : UI(),
        fResizeHandle(this)
    {
        const double scaleFactor = getScaleFactor();
        setGeometryConstraints(DISTRHO_UI_DEFAULT_WIDTH * scaleFactor, DISTRHO_UI_DEFAULT_HEIGHT * scaleFactor);

        strcpy(sampleFilePath, "Drop Audio File Here");

        if (isResizable())
            fResizeHandle.hide();

        // Create our custom OLE interceptor instance
        new MyOleDropTarget(this);
    }

    void setDroppedFilePath(const char* path) override {
        strcpy(sampleFilePath, path);
        setState("sampleFilePath", sampleFilePath);
    }

    Window& getWindow() const override {
        return UI::getWindow();
    }

protected:
    void parameterChanged(uint32_t index, float value) override {
        fRelease = value;
        repaint();
    }

    void onImGuiDisplay() override {
        const float width = getWidth();
        const float height = getHeight();
        const float margin = 20.0f * getScaleFactor();

        ImGui::SetNextWindowPos(ImVec2(margin, margin));
        ImGui::SetNextWindowSize(ImVec2(width - 2 * margin, height - 2 * margin));

        if (ImGui::Begin("BAKED", nullptr, ImGuiWindowFlags_NoResize))
        {
            ImGui::Text("File Status: %s", sampleFilePath);

            ImGui::Separator();
            ImGui::Spacing();

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
        }
        ImGui::End();
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImGuiPluginUI)
};

// This must stay outside the class because it is a global framework entry point
UI* createUI()
{
    return new ImGuiPluginUI();
}

END_NAMESPACE_DISTRHO
