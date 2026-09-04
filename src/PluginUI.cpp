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
#include "Editor.hpp"
#include "PluginDSP.hpp"


START_NAMESPACE_DISTRHO

class ImGuiPluginUI : public UI, public FileDropReceiver
{
    float fRelease = 0.0f;
    ResizeHandle fResizeHandle;
    char sampleFilePath[MAX_FILE_PATH_LENGTH];
    SampleEditor *editor;

public:
    ImGuiPluginUI()
        : UI(),
        fResizeHandle(this)
    {
        const double scaleFactor = getScaleFactor();
        setGeometryConstraints(DISTRHO_UI_DEFAULT_WIDTH * scaleFactor, DISTRHO_UI_DEFAULT_HEIGHT * scaleFactor);


        if (isResizable())
            fResizeHandle.hide();


        strcpy(sampleFilePath, "Drop sample here...");
        // Create our custom OLE interceptor instance
        new MyOleDropTarget(this);
        editor=new SampleEditor("Sample Editor", getPluginDPSPointer()->sample, getPluginDPSPointer()->engine, getWindow());
        if (editor) {
            editor->show();  // Tells the OS to make the window visible
        }

    }

    ImGuiPluginDSP* getPluginDPSPointer(){
        auto* plugin = static_cast<ImGuiPluginDSP*>(getPluginInstancePointer());
        return plugin;
    }

    void setDroppedFilePath(const char* path) override {
        getPluginDPSPointer()->sample->loadWavFile(path);
        strcpy(sampleFilePath, path);
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
            // if (ImGui::CollapsingHeader("Sample Editor"))
            // {
            //     ImGui::Indent();
            //     editor->render();

            //     ImGui::Unindent();
            // }

            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::SliderFloat("Release", &fRelease, 0.f, 1.f))
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

    ~ImGuiPluginUI(){
        ImPlot::DestroyContext();
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImGuiPluginUI)
};

// This must stay outside the class because it is a global framework entry point
UI* createUI()
{
    return new ImGuiPluginUI();
}

END_NAMESPACE_DISTRHO
