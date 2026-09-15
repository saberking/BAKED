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
#include <../clap/include/clap/ext/context-menu.h>
#include <windows.h>
#include <commctrl.h> // For SetWindowSubclass API
#pragma comment(lib, "comctl32.lib")


START_NAMESPACE_DISTRHO

//for right click autmoaiton clip
static UINT WM_TRIGGER_CLAP_MENU = 0;
struct AsyncMenuPayload {//for right click autmoaiton clip
    const clap_host_t* host;
    int32_t screenX;
    int32_t screenY;
};

class ImGuiPluginUI : public UI, public FileDropReceiver
{
    float fSpeed = 1.f;
    ResizeHandle fResizeHandle;
    char sampleFilePath[MAX_FILE_PATH_LENGTH];
    SampleEditor *editor=NULL;
    MyOleDropTarget *oleDropTarget=NULL;
    bool editorWasOpen=true;
public:

    ImGuiPluginUI()
        : UI(),
        fResizeHandle(this)
    {
        //create unique id for automation clip window
        WM_TRIGGER_CLAP_MENU = ::RegisterWindowMessageA("MyUniquePlugin_ClapContextMenu_TriggerMsg");
        HWND hwnd = (HWND)getWindow().getNativeWindowHandle();
        ::SetWindowSubclass(hwnd, SubclassMenuProc, reinterpret_cast<UINT_PTR>(this), 0);
        const double scaleFactor = getScaleFactor();
        setGeometryConstraints(DISTRHO_UI_DEFAULT_WIDTH * scaleFactor, DISTRHO_UI_DEFAULT_HEIGHT * scaleFactor);


        if (isResizable())
            fResizeHandle.hide();
        oleDropTarget=new MyOleDropTarget(this);
        editor=new SampleEditor("Sample Editor", getPluginDPSPointer()->modules[0], getWindow(), [this](const char* path) {
            this->setDroppedFilePath(path);
        });

        editor->show();
    }

    void stateChanged(const char* key, const char* value){
        strcpy(sampleFilePath, value);
    };

    ImGuiPluginDSP* getPluginDPSPointer(){
        auto* plugin = static_cast<ImGuiPluginDSP*>(getPluginInstancePointer());
        return plugin;
    }

    void setDroppedFilePath(const char* path) override {
        getPluginDPSPointer()->modules[0]->sample->loadWavFile(path);
        setState("sampleFilePath" ,path);
        strcpy(sampleFilePath, path);
        editor->isMono=(getPluginDPSPointer()->modules[0]->sample->channels==1);
        editor->calculateFFT();
    }

    Window& getWindow() const override {
        return UI::getWindow();
    }

    static LRESULT CALLBACK SubclassMenuProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                             UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
    {
        if (uMsg == WM_TRIGGER_CLAP_MENU)
        {
            // Safely extract the heap data passed through the OS message queue
            auto* payload = reinterpret_cast<AsyncMenuPayload*>(wParam);
            if (payload)
            {
                auto* menuExt = (const clap_host_context_menu_t*)payload->host->get_extension(payload->host, CLAP_EXT_CONTEXT_MENU);
                if (menuExt && menuExt->popup)
                {
                    clap_context_menu_target_t target;
                    target.kind = CLAP_CONTEXT_MENU_TARGET_KIND_PARAM;
                    target.id = kParamSpeed;

                    // Open the menu cleanly outside of the active ImGui/DPF render cycle.
                    // This un-freezes both windows and eliminates the multi-instance crash!
                    menuExt->popup(payload->host, &target, 0, payload->screenX, payload->screenY);
                }

                // Delete the temporary payload allocation immediately after use
                delete payload;
            }
            return 0;
        }

        // Pass every other standard OS window message safely back to DPF
        return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }

protected:
    void parameterChanged(uint32_t index, float value) override {
        fSpeed = value;
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
            ImGui::Text("Filepath: %s", sampleFilePath);
            // if (ImGui::CollapsingHeader("Sample Editor"))
            // {
            //     ImGui::Indent();
            //     editor->render();

            //     ImGui::Unindent();
            // }
            if (!editor->isVisible()&&ImGui::Button("Edit sample"))
            {
                editor->show();
            }
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::SliderFloat("Speed", &fSpeed, 0.f, 1.f))
            {
                if (ImGui::IsItemActivated())
                    editParameter(kParamSpeed, true);

                setParameterValue(kParamSpeed, fSpeed);

            }
            if(ImGui::IsItemHovered()&& ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                const clap_host_t* host=static_cast<const clap_host_t*>(getPluginDPSPointer()->host);
                HWND hwnd = reinterpret_cast<HWND>(getWindow().getNativeWindowHandle());

                if(host){
                    // Query DAW for the context menu extension
                    auto* menuExt = (const clap_host_context_menu_t*)host->get_extension(host, CLAP_EXT_CONTEXT_MENU);
                    std::cout<<"menuExt"<<std::endl;
                    if (menuExt && menuExt->popup)
                    {
                        std::cout<<"pop"<<std::endl;


                        ImVec2 mousePos = ImGui::GetMousePos();

                        auto* payload = new AsyncMenuPayload();
                        payload->host = host;
                        payload->screenX = mousePos.x;
                        payload->screenY = mousePos.y;

                        ::PostMessage(hwnd, WM_TRIGGER_CLAP_MENU, reinterpret_cast<WPARAM>(payload), 0);
                    }
                }

            }
            if (ImGui::IsItemDeactivated())
            {
                editParameter(kParamSpeed, false);
            }
        }
        ImGui::End();

    }

    ~ImGuiPluginUI(){
        HWND hwnd = (HWND)getWindow().getNativeWindowHandle();

        ::RemoveWindowSubclass(hwnd, SubclassMenuProc, reinterpret_cast<UINT_PTR>(this));
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
