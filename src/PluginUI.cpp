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
#define DR_WAV_IMPLEMENTATION

#include "Editor.hpp"
#include <../clap/include/clap/ext/context-menu.h>
#include <../clap/include/clap/ext/state.h>
#include <../clap/include/clap/ext/params.h>
#include <windows.h>
#include <commctrl.h> // For SetWindowSubclass API
#pragma comment(lib, "comctl32.lib")


START_NAMESPACE_DISTRHO



//for right click autmoaiton clip
class ImGuiPluginUI : public UI, public FileDropReceiver
{
    ResizeHandle fResizeHandle;
    char sampleFilePath[MAX_FILE_PATH_LENGTH];
    SampleEditor *editor=NULL;
    MyOleDropTarget *oleDropTarget=NULL;
    ImPlotContext* imPlotContext[NO_OF_PLOT_CONTEXTS];
    bool editEnvelope=true;
public:

    ImGuiPluginUI()
        : UI(),
        fResizeHandle(this)
    {
        //create unique id for automation clip window

        // const double scaleFactor = getScaleFactor();
        //setGeometryConstraints(1650, 900);
        setSize(1335,890);

        if (isResizable())
            fResizeHandle.hide();

        for(int i=0;i<NO_OF_PLOT_CONTEXTS;i++){
            imPlotContext[i]=ImPlot::CreateContext();
        }

        oleDropTarget=new MyOleDropTarget(this);
        editor=new SampleEditor("Sample Editor", getPluginDPSPointer()->modules[0], getWindow(),
            [this](const char* path) {this->setDroppedFilePath(path);},
            [this](){this->setDirty();},
            imPlotContext,
            [this](int parameter,bool editing){this->editParameter(parameter, editing);},
            [this](int parameter,float value){this->setParameterValue(parameter, value);},
            getPluginDPSPointer()
            );


    }

    void stateChanged(const char* key, const char* value){

    }

    ImGuiPluginDSP* getPluginDPSPointer(){
        auto* plugin = static_cast<ImGuiPluginDSP*>(getPluginInstancePointer());
        return plugin;
    }

    void setDroppedFilePath(const char* path) override {
        getPluginDPSPointer()->modules[0]->sample->loadWavFile(path);
        strcpy(sampleFilePath, path);
        editor->isMono=(getPluginDPSPointer()->modules[0]->sample->channels.load(std::memory_order_relaxed)==1);
        for(int i =0;i<2;i++)
        {
            editor->calculateFFT(i);
            getPluginDPSPointer()->modules[0]->process();
        }
        setDirty();
    }

    void setDirty(){

        const uint32_t activeFormat = editor->getPluginFormat();

        if (activeFormat == 1)
        {
            auto* clapPointer=reinterpret_cast<const clap_host_t*>(getPluginDPSPointer()->host);
            if(!clapPointer)return;
            auto* hostState = reinterpret_cast<const clap_host_state_t*>(clapPointer->get_extension(clapPointer, CLAP_EXT_STATE));
            if (hostState != nullptr && hostState->mark_dirty != nullptr) {


                hostState->mark_dirty(clapPointer);
            }
        }
    }

    Window& getWindow() const override {
        return UI::getWindow();
    }



protected:
    void parameterChanged(uint32_t index, float value) override {
        editor->fSpeed = value;
        repaint();
    }



    void onImGuiDisplay() override {
        const float height = getHeight();
        const float width = getWidth();

        //const float margin = 20.0f * getScaleFactor();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(width , height ));
        // ImGuiStyle& style = ImGui::GetStyle();
        // float padding_x = style.WindowPadding.x * 2.0f;
        // float padding_y = style.WindowPadding.y * 2.0f;
        // // bool horizontal_scrollbar_needed = (width - padding_x) < 750.0f;
        // // float scrollbar_offset_y = horizontal_scrollbar_needed ? style.ScrollbarSize : 0.0f;

        // ImGui::SetNextWindowContentSize(ImVec2(
        //     std::max(800.0f, width - padding_x),
        //     0.f
        //     ));
        if (ImGui::Begin("BAKED", nullptr, ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoTitleBar))
        {
            editor->display();
        }
        if(!ImGui::IsMouseDown(ImGuiMouseButton_Left)) editor->endDrag();

        ImGui::End();
    }

    ~ImGuiPluginUI(){

        delete(oleDropTarget);
        delete(editor);
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImGuiPluginUI)
};

// This must stay outside the class because it is a global framework entry point
UI* createUI()
{
    return new ImGuiPluginUI();
}

END_NAMESPACE_DISTRHO
