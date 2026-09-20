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
#include "PluginDSP.hpp"
#include <../clap/include/clap/ext/context-menu.h>
#include <../clap/include/clap/ext/state.h>
#include <../clap/include/clap/ext/params.h>
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
    ImPlotContext* imPlotContext[8];
    bool editEnvelope=true;

public:

    ImGuiPluginUI()
        : UI(),
        fResizeHandle(this)
    {
        //create unique id for automation clip window
        WM_TRIGGER_CLAP_MENU = ::RegisterWindowMessageA("MyUniquePlugin_ClapContextMenu_TriggerMsg");
        HWND hwnd = (HWND)getWindow().getNativeWindowHandle();
        const uint32_t activeFormat = getPluginFormat();

        if (activeFormat == 1)
        {
            std::cout<<"setwindowsublcass\n";
            ::SetWindowSubclass(hwnd, SubclassMenuProc, reinterpret_cast<UINT_PTR>(this), 0);
        }
        //const double scaleFactor = getScaleFactor();
        //setGeometryConstraints(DISTRHO_UI_DEFAULT_WIDTH * scaleFactor, DISTRHO_UI_DEFAULT_HEIGHT * scaleFactor);
        setSize(1600,980);

        if (isResizable())
            fResizeHandle.hide();

        for(int i=0;i<8;i++){
            imPlotContext[i]=ImPlot::CreateContext();
        }

        oleDropTarget=new MyOleDropTarget(this);
        editor=new SampleEditor("Sample Editor", getPluginDPSPointer()->modules[0], getWindow(),
            [this](const char* path) {this->setDroppedFilePath(path);},
            [this](){this->setDirty();},
            imPlotContext
            );

    }

    bool checkIfClapAtRuntime()
    {
        char fileBuffer[MAX_PATH] = {0};
        HMODULE hModule = NULL;

        // 🟢 Create a dummy static variable. It lives inside your plugin library's binary memory space.
        static const int dummyAnchor = 0;

        // 🟢 Pass the address of the dummy anchor variable instead of the member function pointer
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&dummyAnchor), &hModule);

        if (hModule) {
            GetModuleFileNameA(hModule, fileBuffer, sizeof(fileBuffer));
            std::string binaryPath(fileBuffer);

            std::transform(binaryPath.begin(), binaryPath.end(), binaryPath.begin(), ::tolower);

            std::string target = ".clap";
            if (binaryPath.length() >= target.length()) {
                return (binaryPath.compare(binaryPath.length() - target.length(), target.length(), target) == 0);
            }
        }
        return false; // 🔵 Fallback (VST3, etc.)
    }

    int getPluginFormat()
    {   if(checkIfClapAtRuntime())        return 1;
        return 0;
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

        }
        setDirty();
    }

    void setDirty(){

        const uint32_t activeFormat = getPluginFormat();

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

    static LRESULT CALLBACK SubclassMenuProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                             UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
    {
        if (uMsg == WM_TRIGGER_CLAP_MENU)
        {
            // Safely extract the heap data passed through the OS message queue
            auto* payload = reinterpret_cast<AsyncMenuPayload*>(wParam);
            if (payload)
            {
                if(payload->host)
                {
                    std::cout<<"LRESULT CALLBACK"<<std::endl;
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

    void displayPlaybackControls()
    {


        // 3. Set the slider width to stretch up to that text boundary
        ImGui::SetNextItemWidth(-100.f);        if (ImGui::SliderFloat("Speed", &fSpeed, 0.f, 1.f))
        {
            if (ImGui::IsItemActivated())
                editParameter(kParamSpeed, true);

            setParameterValue(kParamSpeed, fSpeed);

        }
        const uint32_t activeFormat = getPluginFormat();
        if (activeFormat == 1)
        {
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
                ImGuiIO& io = ImGui::GetIO();
                io.MouseClicked[ImGuiMouseButton_Right] = false;
                io.MouseDown[ImGuiMouseButton_Right] = false;

            }
        }
        if (ImGui::IsItemDeactivated())
        {
            editParameter(kParamSpeed, false);
        }
    }
    static ImPlotPoint envelopeGetter(int idx, void* data_ptr) {
        auto* vec_ptr = static_cast<std::vector<std::atomic<float>>*>(data_ptr);
        float y_val = (*vec_ptr)[idx].load(std::memory_order_relaxed);
        return ImPlotPoint(idx, y_val);
    }


    void setEnvelope(int x, float y)
    {
        if(x<0||x>=ENVELOPE_LENGTH) return;
        getPluginDPSPointer()->modules[0]->envelope[x].store(std::max(0.f,std::min(1.f,y)), std::memory_order_relaxed);
        setDirty();
    }
    void displayEnvelope(){
        ImPlot::SetCurrentContext(imPlotContext[6]);
        if(ImPlot::BeginPlot("Envelope",ImVec2(-1.0f, 200.0f))){
            editor->setInputMap(editEnvelope);

            ImPlot::SetupAxis(ImAxis_Y1, "Amplitude", ImPlotAxisFlags_Lock|ImPlotAxisFlags_NoGridLines);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -0.001, 1.18, ImPlotCond_Always);
            ImPlot::SetupAxisScale(ImAxis_Y1, editor->TransformForward_Sqrt, editor->TransformInverse_Sqrt);

            // Allow the X-axis to scroll and zoom normally
            ImPlot::SetupAxis(ImAxis_X1, "", ImPlotAxisFlags_NoGridLines|ImPlotAxisFlags_NoTickLabels|ImPlotAxisFlags_NoTickMarks);
            ImPlot::SetupAxisLimits(ImAxis_X1, -10.f, 209.f, ImPlotCond_Once);
            ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, -10.0, 209.0);
            ImPlot::SetupAxisZoomConstraints(ImAxis_X1, 20, 219.0);



            ImPlotSpec bound_spec;
            bound_spec.LineColor = ImVec4(0.5f, 0.5f, 0.5f, 0.5f);
            bound_spec.LineWeight = 1.5f;

            // --- VERTICAL BOUNDS (From Y=0 to Y=1) ---
            double v_line_y[] = { 0.0, 1.0 };
            double v_line_x0[] = { 0.0, 0.0 };
            double v_line_x200[] = { 200.0, 200.0 };

            // Left vertical edge at X=0
            ImPlot::PlotLine("##Vert0", v_line_x0, v_line_y, 2, bound_spec);

            // Right vertical edge at X=200
            ImPlot::PlotLine("##Vert200", v_line_x200, v_line_y, 2, bound_spec);


            // --- HORIZONTAL BOUNDS (From X=0 to X=200) ---
            double h_line_x[] = { 0.0, 200.0 };
            double h_line_y0[] = { 0.0, 0.0 };
            double h_line_y1[] = { 1.0, 1.0 };

            // Bottom horizontal edge at Y=0
            ImPlot::PlotLine("##Horiz0", h_line_x, h_line_y0, 2, bound_spec);

            // Top horizontal edge at Y=1
            ImPlot::PlotLine("##Horiz1", h_line_x, h_line_y1, 2, bound_spec);



            ImPlot::PlotScatterG("Envelope", envelopeGetter, &getPluginDPSPointer()->modules[0]->envelope, ENVELOPE_LENGTH, editor->spec);
            if (ImPlot::IsPlotHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&editEnvelope) {
                ImPlotPoint current_pos = ImPlot::GetPlotMousePos();

                editor->handleDrag(
                    (int)current_pos.x,current_pos.y,
                    [this](int x, float y, int channelDummy){this->setEnvelope(x,y);}
                    );

            }
            ImPlot::EndPlot();

        }
    }

    void onImGuiDisplay() override {


        const float width = getWidth();
        const float height = getHeight();
        //const float margin = 20.0f * getScaleFactor();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(width , height ));

        if (ImGui::Begin("BAKED", nullptr, ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoTitleBar))
        {

            // if (ImGui::Button("Edit sample"))
            // {
            //     editor->show();
            //     editor->focus();
            // }
            // ImGui::Separator();
            // ImGui::Spacing();

            if (ImGui::BeginTable("my_resizable_table", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit))
            {
                // Provide an initial width (e.g., 150.0f). The user can drag to resize it.
                ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthFixed, 1225.0f);
                ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableNextColumn();
                editor->display();

                ImGui::TableNextColumn();


                displayPlaybackControls();
                displayEnvelope();

                ImGui::EndTable();
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
