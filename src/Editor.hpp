#ifndef EDITOR_HPP
#define EDITOR_HPP
#include "src/DistrhoDefines.h"
#include "DistrhoUI.hpp"

#include "AudioData.hpp"
#include "external/implot.h"
#include "external/implot_internal.h"
#include "external/pocketfft_hdronly.h"
#include "DragAndDrop.hpp"
#include <../clap/include/clap/ext/context-menu.h>
#include <../clap/include/clap/ext/state.h>
#include <../clap/include/clap/ext/params.h>
#include <windows.h>
#include <commctrl.h> // For SetWindowSubclass API
#include "PluginDSP.hpp"


START_NAMESPACE_DISTRHO

struct PlotAudioContext {
    AudioData* audioData;
    int channel;
};

static UINT WM_TRIGGER_CLAP_MENU = 0;
struct AsyncMenuPayload {//for right click autmoaiton clip
    const clap_host_t* host;
    int32_t screenX;
    int32_t screenY;
};


class SampleEditor //: public DGL::ImGuiStandaloneWindow, public FileDropReceiver
{
public:
    AudioData *data=NULL;
    Module *module=NULL;
    ImPlotSpec spec;
    bool editMode=false;
    bool isMono;
    ImPlotContext** imPlotContext;
    std::vector<std::complex<float>> *spectrum[2] ;
    bool isSpectrumChanged[2];
    bool isLiveUpdate=true;
    bool isWaveformChanged[2];
    std::function<void(const char*)> fileDropped;
    std::function<void()> setDirty;
    std::function<void(int, bool)> editParameter;
    std::function<void(int, float)> setParameterValue;
    int length;
    bool isDragging=false;
    float  dragStartY;
    int dragStartX;
    double spectrumXMax, spectrumXMin, waveformXMax, waveformXMin;
    float fSpeed = 1.f;
    ImGuiPluginDSP *dspPointer;
    Window *parentWindow;

    SampleEditor(const char *_name, Module *_module, Window& _window,
                 std::function<void(const char*)> _fileDropped, std::function<void()> _setDirty,
                 ImPlotContext* _imPlotContext [8],
                 std::function<void(int, bool)> _editParameter,
                 std::function<void(int, float)> _setParameterValue,
                 ImGuiPluginDSP *_dSPPointer
        )
        //:DGL::ImGuiStandaloneWindow(_window.getApp(), _window)
    {
        parentWindow=&_window;
        module=_module;
        fileDropped=_fileDropped;
        setDirty=_setDirty;
        data=module->sample;
        imPlotContext=_imPlotContext;
        editParameter=_editParameter;
        setParameterValue=_setParameterValue;
        dspPointer=_dSPPointer;
        spec.Flags = ImPlotFlags_CanvasOnly;
        // setResizable(true);
        // setSize(1675,1000);

        for(int channel=0;channel<2;channel++)
        {
            spectrum[channel] = new std::vector<std::complex<float>>  ( MAX_SAMPLE_LENGTH/2+1 );
            for(int j=0;j<MAX_SAMPLE_LENGTH/2+1;j++)
            {
                (*spectrum[channel])[j]=std::complex(0,0);
            }

        }
        isMono=(data->channels.load()==1);
        isSpectrumChanged[0]=isSpectrumChanged[1]=false;
        for(int channel=0;channel<2;channel++)
        {
            calculateFFT(channel);
        }
        spectrumXMax=100;
        spectrumXMin=-2;
        waveformXMax=-4;
        waveformXMin=200;

        WM_TRIGGER_CLAP_MENU = ::RegisterWindowMessageA("MyUniquePlugin_ClapContextMenu_TriggerMsg");
        HWND hwnd = (HWND)parentWindow->getNativeWindowHandle();
        const uint32_t activeFormat = getPluginFormat();

        if (activeFormat == 1)
        {
            std::cout<<"setwindowsublcass\n";
            ::SetWindowSubclass(hwnd, SubclassMenuProc, reinterpret_cast<UINT_PTR>(this), 0);
        }
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
    // Window& getWindow() const override {
    //     return DGL::ImGuiStandaloneWindow::getWindow();
    // }
    // void setDroppedFilePath(const char* path) override {
    //     fileDropped(path);
    // }
    static ImPlotPoint AtomicVectorGetter(int idx, void* data_ptr) {
        auto* vec_ptr = static_cast<PlotAudioContext*>(data_ptr);
        float y_val = vec_ptr->audioData->sampleData[vec_ptr->channel][idx].load(std::memory_order_relaxed);
        return ImPlotPoint(idx, y_val);
    }


    static ImPlotPoint SpectrumGetter(int idx, void* data_ptr){
        auto* vec_ptr = static_cast<std::vector<std::complex<float>>*>(data_ptr);
        float y_val = std::abs((*vec_ptr)[idx]);
        return ImPlotPoint(idx, y_val);
    }

    static ImPlotPoint PhaseGetter(int idx, void* data_ptr){
        auto* vec_ptr = static_cast<std::vector<std::complex<float>>*>(data_ptr);
        float y_val = std::arg((*vec_ptr)[idx])+M_PI/2+0.000001;
        if(y_val<0)
        {
            y_val+=2*M_PI;
            //std::cout<<"idx"<<idx<<std::endl<<"yval"<<y_val<<std::endl<<"abs"<<std::abs((*vec_ptr)[idx])<<std::endl;
        }
        if(std::abs((*vec_ptr)[idx])==0)y_val=-999.f;
        return ImPlotPoint(idx, y_val);
    }
    static ImPlotPoint envelopeGetter(int idx, void* data_ptr) {
        auto* vec_ptr = static_cast<std::vector<std::atomic<float>>*>(data_ptr);
        float y_val = (*vec_ptr)[idx].load(std::memory_order_relaxed);
        return ImPlotPoint(idx, y_val);
    }




    void calculateFFT(int j){
        //std::vector<std::complex<float>> data_in ( data->length );
        pocketfft::shape_t shape_in{1};                                              // dimensions of the input shape
        pocketfft::stride_t stride_in{1};                    // must have the size of each element. Must have size() equal to shape_in.size()
        pocketfft::stride_t stride_out{1}; // must have the size of each element. Must have size() equal to shape_in.size()
        //stride_in[0]=stride_out[0]=sizeof ( std::complex<float> );
        stride_in[0]=sizeof ( float );
        stride_out[0]=sizeof ( std::complex<float> );
        bool forward{ pocketfft::FORWARD };                                            // FORWARD or BACKWARD
        shape_in[0]=data->length.load(std::memory_order_relaxed);

        float fct{ 2.0f /shape_in[0]};    // scaling factor
        pocketfft::shape_t axes;

        axes.push_back ( 0 );
        std::vector<float> data_in ( shape_in[0] );

        for (int i=0; i<shape_in[0]; i++ )
        {
            //data_in[i]=std::complex<float> ( data->sampleData[j][i].load(std::memory_order_relaxed),0.f );
            data_in[i]=data->sampleData[j][i].load(std::memory_order_relaxed);
        }

        pocketfft::r2c (
            shape_in,
            stride_in,
            stride_out,
            axes,
            forward,
            data_in.data(),
            spectrum[j]->data(),
            fct
            );
        for(int i=0;i<shape_in[0]/2+1;i++)
        {
            if(std::abs((*spectrum[j])[i])<0.000001)
            {
                (*spectrum[j])[i]=std::complex(0.f,0.f);
            }
        }
        (*spectrum[j])[0]/=2;
        (*spectrum[j])[shape_in[0]/2]/=2;
        isWaveformChanged[j]=false;

    }

    void calculateWaveform(int j){
        std::vector<float> waveform  ( MAX_SAMPLE_LENGTH );



        pocketfft::shape_t shape_in{1};                                              // dimensions of the input shape
        pocketfft::stride_t stride_in{1};                    // must have the size of each element. Must have size() equal to shape_in.size()
        pocketfft::stride_t stride_out{1}; // must have the size of each element. Must have size() equal to shape_in.size()
        stride_in[0]=sizeof ( std::complex<float> );
        stride_out[0]=sizeof ( float );

        bool forward{ pocketfft::BACKWARD };                                            // FORWARD or BACKWARD

        float fct{ 0.5f};    // scaling factor
        shape_in[0]=data->length.load(std::memory_order_relaxed);
        pocketfft::shape_t axes;

        axes.push_back ( 0 );
        std::vector<std::complex<float>> data_in ( shape_in[0]/2+1 );

        for (int i=0; i<shape_in[0]/2+1; i++ )
        {
            data_in[i]= (*spectrum[j])[i] ;
        }
        data_in[shape_in[0]/2]*=2;
        data_in[0]*=2;


        pocketfft::c2r (
            shape_in,
            stride_in,
            stride_out,
            axes,
            forward,
            data_in.data(),
            waveform.data(),
            fct
            );

        float maxVal=1;
        for(int k=0;k<shape_in[0];k++){
            (std::abs(waveform[k])>maxVal)&&(maxVal=std::abs(waveform[k]));
        }
        float multiplier=1/maxVal;
        for(int k=0;k<shape_in[0];k++){
            data->sampleData[j][k].store(waveform[k]*multiplier);

        }
        for(int k=0;k<MAX_SAMPLE_LENGTH/2;k++)
        {
            (*spectrum[j])[k]*=multiplier;
        }

        isSpectrumChanged[j]=false;
        setDirty();
        module->process();
    }

    void setInputMap(bool editing){
        ImPlotInputMap &inputMap=ImPlot::GetInputMap();
        if (editing)
        {
            inputMap.Pan = ImGuiMouseButton_Right;

            inputMap.SelectMod = ImGuiMod_Ctrl ;//zoom
        }
        else
        {
            inputMap.Pan = ImGuiMouseButton_Left;

            inputMap.SelectMod = ImGuiMod_None;//zoom
        }
    }

    void showPlayhead(){
        for(int i=0;i<MAX_POLY;i++){
            if(module->playbackData[i]->playing){
                double playhead = module->playbackData[i]->playhead;
                ImPlot::DragLineX(i, &playhead, ImVec4(0.5,0.5,0.5,0.5), 0.5f, ImPlotDragToolFlags_NoInputs);
            }
        }
    }
    static inline double TransformForward_Sqrt(double v, void*) {
        return  std::cbrt(v);
    }

    static inline double TransformInverse_Sqrt(double v, void*) {
        return v * v*v;
    }


    float clip(float input)
    {
        return std::max(-1.f,std::min(1.f,input));
    }

    void setWaveformSample(int x, float y, int channel)
    {
        if(x<0||x>=MAX_SAMPLE_LENGTH) return;
        data->sampleData[channel][x].store(clip(y));
        isWaveformChanged[channel]=true;
        setDirty();
    }

    void setSpectrumAmplitude(int x, float y, int channel)
    {
        if(x<0||x>data->length.load(std::memory_order_relaxed)/2) return;
        (*spectrum[channel])[x]=std::polar(
            (float)std::min(std::max(y,0.f),1.f),
            (float)(std::abs((*spectrum[channel])[x])?std::arg((*spectrum[channel])[x]):-M_PI/2)
            );
        isSpectrumChanged[channel]=true;
    }

    void setSpectrumPhase(int x, float y, int channel)
    {
        if(x<0||x>data->length.load(std::memory_order_relaxed)/2||!std::abs((*spectrum[channel])[x])) return;
        (*spectrum[channel])[x]=std::polar(
            std::abs((*spectrum[channel])[x]),
            (float)(std::max(y,0.f)-M_PI/2)
            );
        isSpectrumChanged[channel]=true;
    }

    void setEnvelope(int x, float y)
    {
        if(x<0||x>=ENVELOPE_LENGTH) return;
        module->envelope[x].store(std::max(0.f,std::min(1.f,y)), std::memory_order_relaxed);
        setDirty();
    }
    void handleDrag(int x, float y, std::function<void(int, float, int)> callback, int channel=0)
    {
        if(isDragging){
            int xStep = x>dragStartX?1:-1;
            int noOfSteps=std::abs(x-dragStartX)+1;
            float yStep =(y-dragStartY)/noOfSteps;
            for(int index=0;index<noOfSteps;index++)
            {
                callback(dragStartX+index*xStep, dragStartY+index*yStep, channel);
            }
        }else{
            callback(x, y, channel);
        }
        isDragging=true;
        dragStartX=x;
        dragStartY=y;
    }

    // void getEmbeddedSubwindowOffset(int& out_x, int& out_y) {
    //     // 1. Extract the raw Win32 HWND handles out of DPF/DGL
    //     HWND main_hwnd = (HWND)parentWindow->getNativeWindowHandle();
    //     HWND sub_hwnd  = (HWND)getWindow().getNativeWindowHandle();


    //     // 2. Fetch the top-left screen position of the child subwindow
    //     POINT pt = { 0, 0 };
    //     ClientToScreen(sub_hwnd, &pt);

    //     // 3. Map those screen coordinates backwards relative to the main plugin window canvas
    //     ScreenToClient(main_hwnd, &pt);

    //     // 4. Output the precise relative pixel offset bounds!
    //     out_x = pt.x;
    //     out_y = pt.y;
    // }

    void displayPlaybackControls()
    {


        // 3. Set the slider width to stretch up to that text boundary
        ImGui::SetNextItemWidth(-100.f);
        if (ImGui::SliderFloat("Speed", &fSpeed, 0.f, 1.f))
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
                const clap_host_t* host=static_cast<const clap_host_t*>(dspPointer->host);
                HWND hwnd = reinterpret_cast<HWND>(parentWindow->getNativeWindowHandle());

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
                        int xOffset=0,yOffset=0;
                        // if (isVisible())getEmbeddedSubwindowOffset(xOffset,yOffset);
                        payload->screenX = mousePos.x+xOffset;
                        payload->screenY = mousePos.y+yOffset;

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

    void displayEnvelope(){
        ImPlot::SetCurrentContext(imPlotContext[6]);
        if(ImPlot::BeginPlot("Envelope",ImVec2(-1.0f, 200.0f))){
            setInputMap(true);

            ImPlot::SetupAxis(ImAxis_Y1, "Amplitude", ImPlotAxisFlags_Lock|ImPlotAxisFlags_NoGridLines);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -0.001, 1.18, ImPlotCond_Always);
            ImPlot::SetupAxisScale(ImAxis_Y1, TransformForward_Sqrt, TransformInverse_Sqrt);

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



            ImPlot::PlotScatterG("Envelope", envelopeGetter, &dspPointer->modules[0]->envelope, ENVELOPE_LENGTH, spec);
            if (ImPlot::IsPlotHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                ImPlotPoint current_pos = ImPlot::GetPlotMousePos();

                handleDrag(
                    (int)current_pos.x,current_pos.y,
                    [this](int x, float y, int dummy){this->setEnvelope(x,y);}
                    );
                if (isLiveUpdate)
                {
                    module->process();
                }

            }
            ImPlot::EndPlot();

        }
    }


    void displaySample()
    {
        int plotIndex=0;

        length=data->length.load(std::memory_order_relaxed);


        if(ImGui::Checkbox("Mono", &isMono)){
            if(isMono)data->channels.store(1, std::memory_order_relaxed);
            else data->channels.store(2, std::memory_order_relaxed);
        }
        ImGui::SameLine();
        ImGui::Checkbox("Edit", &editMode);

        if(editMode)
        {
            ImGui::SameLine();
            if(ImGui::Checkbox("Live update", &isLiveUpdate))
            {
                if(isLiveUpdate)
                {
                    for(int j=0;j<data->channels.load(std::memory_order_relaxed)  ;j++)
                    {

                        if(isSpectrumChanged[j])calculateWaveform(j);
                        if(isWaveformChanged[j])calculateFFT(j);
                    }
                }
            }
            if(!isLiveUpdate)
            {
                bool showUpdateButton=false;
                for(int j=0;j<data->channels.load(std::memory_order_relaxed);j++)
                {
                    showUpdateButton=showUpdateButton||isSpectrumChanged[j]||isWaveformChanged[j];
                }

                if(showUpdateButton)
                {
                    ImGui::SameLine();
                    if(ImGui::Button("Apply changes"))
                    {
                        for(int j=0;j<data->channels.load(std::memory_order_relaxed)  ;j++)
                        {

                            if(isSpectrumChanged[j])calculateWaveform(j);
                            if(isWaveformChanged[j])
                            {
                                calculateFFT(j);
                                module->process();
                            }
                        }
                    }
                }
            }

            float tempLength=length;
            ImGui::SliderFloat ("Length",&tempLength, 1,MAX_SAMPLE_LENGTH, "%.0f", ImGuiSliderFlags_Logarithmic);
            length=(int)tempLength;
            if(length!=data->length.load(std::memory_order_relaxed))
            {
                data->length.store(length, std::memory_order_relaxed);
                setDirty();
                for(int j=0;j<data->channels.load(std::memory_order_relaxed)  ;j++)
                {
                    isWaveformChanged[j]=true;
                    if(isLiveUpdate)
                    {
                        calculateFFT(j);
                        module->process();
                    }

                }

            }

        }


        if (ImGui::BeginTable("My2ColumnTable", isMono?1:2))
        {
            for(int channel=0;channel<1||!isMono&&channel<2;channel++){
                ImGui::TableNextColumn();

                ImPlot::SetCurrentContext(imPlotContext[plotIndex++]);
                if(ImPlot::BeginPlot(channel?"Waveform R":"Waveform L")){
                    setInputMap(editMode);

                    ImPlot::SetupAxis(ImAxis_Y1, "Amplitude", ImPlotAxisFlags_Lock);
                    ImPlot::SetupAxisLimits(ImAxis_Y1, -1.1, 1.1, ImPlotCond_Always);

                    ImPlot::SetupAxis(ImAxis_X1, "Sample", ImPlotAxisFlags_None);
                    ImPlot::SetupAxisLinks(ImAxis_X1, &waveformXMin, &waveformXMax);
                    ImPlot::SetupAxisLimits(ImAxis_X1, -1.f, 200.f, ImPlotCond_Once);
                    ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, -100, MAX_SAMPLE_LENGTH+1000);
                    ImPlot::SetupAxisZoomConstraints(ImAxis_X1, 40, MAX_SAMPLE_LENGTH+2000);


                    PlotAudioContext plotAudioContext { data, channel };
                    ImPlot::PlotScatterG("Waveform", AtomicVectorGetter, &plotAudioContext, data->length.load(std::memory_order_relaxed), spec);
                    if (ImPlot::IsPlotHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&editMode) {
                        if(isSpectrumChanged[channel])calculateWaveform(channel);
                        ImPlotPoint current_pos = ImPlot::GetPlotMousePos();

                        handleDrag(
                            (int)current_pos.x,current_pos.y,
                            [this]( int x, float y, int channel){this->setWaveformSample(x,y,channel);},
                            channel
                            );
                        int newLength=std::max(
                            data->length.load(std::memory_order_relaxed),
                            std::min(MAX_SAMPLE_LENGTH, (int)current_pos.x+1)
                            );

                        data->length.store(newLength, std::memory_order_relaxed);

                        if(isLiveUpdate&&isWaveformChanged[channel])
                        {
                            calculateFFT(channel);
                            module->process();
                        }
                    }
                    showPlayhead();
                    ImPlot::EndPlot();

                }
            }

            for(int channel=0;channel<1||!isMono&&channel<2;channel++){
                ImGui::TableNextColumn();

                ImPlot::SetCurrentContext(imPlotContext[plotIndex++]);

                if (ImPlot::BeginPlot(channel?"Spectrum R":"Spectrum L")){
                    setInputMap(editMode);
                    ImPlot::SetupAxis(ImAxis_Y1, "Amplitude", ImPlotAxisFlags_Lock);
                    ImPlot::SetupAxisLimits(ImAxis_Y1, -0.001, 1.1, ImPlotCond_Always);
                    ImPlot::SetupAxisScale(ImAxis_Y1, TransformForward_Sqrt, TransformInverse_Sqrt);

                    ImPlot::SetupAxis(ImAxis_X1, "Partial", ImPlotAxisFlags_None);
                    ImPlot::SetupAxisLinks(ImAxis_X1, &(spectrumXMin), &(spectrumXMax));

                    ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, -50, MAX_SAMPLE_LENGTH/2+500);
                    ImPlot::SetupAxisZoomConstraints(ImAxis_X1, 20, MAX_SAMPLE_LENGTH/2+1000);

                    ImPlot::PlotScatterG("Spectrum", SpectrumGetter, spectrum[channel], data->length.load(std::memory_order_relaxed)/2+1, spec);
                    if (ImPlot::IsPlotHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&editMode) {
                        if(isWaveformChanged[channel])calculateFFT(channel);
                        ImPlotPoint current_pos = ImPlot::GetPlotMousePos();
                        //(*spectrum[channel])[current_pos.x+1]=std::complex(0.f,0.f);

                        handleDrag(
                            (int)current_pos.x,current_pos.y,
                            [this](int x, float y, int channel){this->setSpectrumAmplitude(x,y,channel);},
                            channel
                            );

                        // int newLength=std::max(
                        //     data->length.load(std::memory_order_relaxed),
                        //     std::max(0,std::min(MAX_SAMPLE_LENGTH, (int)current_pos.x*2))
                        //     );

                        // data->length.store(newLength, std::memory_order_relaxed);

                        if(isLiveUpdate&&isSpectrumChanged[channel])calculateWaveform(channel);
                    }
                    ImPlot::EndPlot();
                }

            }

            for(int channel=0;channel<1||!isMono&&channel<2;channel++){
                ImGui::TableNextColumn();

                ImPlot::SetCurrentContext(imPlotContext[plotIndex++]);
                if (ImPlot::BeginPlot(channel?"Phase R":"Phase L")){
                    setInputMap(editMode);
                    ImPlot::SetupAxis(ImAxis_Y1, "Phase", ImPlotAxisFlags_Lock);
                    ImPlot::SetupAxisLimits(ImAxis_Y1, -0.3,2*M_PI+0.15, ImPlotCond_Always);

                    ImPlot::SetupAxis(ImAxis_X1, "Partial", ImPlotAxisFlags_None);
                    ImPlot::SetupAxisLinks(ImAxis_X1, &(spectrumXMin), &(spectrumXMax));
                    ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, -50, MAX_SAMPLE_LENGTH/2+500);
                    ImPlot::SetupAxisZoomConstraints(ImAxis_X1, 20, MAX_SAMPLE_LENGTH/2+1000);

                    ImPlot::PlotScatterG("Phase", PhaseGetter, spectrum[channel], data->length.load(std::memory_order_relaxed)/2+1, spec);
                    if (ImPlot::IsPlotHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&editMode) {
                        if(isWaveformChanged[channel])calculateFFT(channel);

                        ImPlotPoint current_pos = ImPlot::GetPlotMousePos();

                        handleDrag(
                            (int)current_pos.x,current_pos.y,
                            [this](int x, float y, int channel){this->setSpectrumPhase(x,y,channel);},
                            channel
                            );
                        if(isLiveUpdate&&isSpectrumChanged[channel])calculateWaveform(channel);
                    }
                    ImPlot::EndPlot();
                }


            }
            ImGui::EndTable();

        }

    }

    void display()
    {
        if (ImGui::BeginTable("my_resizable_table", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthFixed, 1225.0f);
            ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextColumn();
            displaySample();

            ImGui::TableNextColumn();


            displayPlaybackControls();
            displayEnvelope();

            ImGui::EndTable();
        }
    }

    // void onImGuiDisplay() override{

    //     ImGui::PushID(this);

    //     ImGui::SetNextWindowPos(ImVec2(0, 0));
    //     ImGui::SetNextWindowSize(ImVec2(getWidth(), getHeight()));

    //     if(!data)return;
    //     if (ImGui::Begin("Waveform Analysis", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)){
    //         display();


    //     }
    //     if(isVisible()&&!ImGui::IsMouseDown(ImGuiMouseButton_Left)) isDragging=false;

    //     ImGui::End();
    //     ImGui::PopID();

    // }
    ~SampleEditor(){
        for(int i=0;i<4;i++){
            ImPlot::DestroyContext(imPlotContext[i]);
        }
        delete spectrum[0];delete spectrum[1];

        HWND hwnd = (HWND)parentWindow->getNativeWindowHandle();
        ::RemoveWindowSubclass(hwnd, SubclassMenuProc, reinterpret_cast<UINT_PTR>(this));

    }
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleEditor)

};


END_NAMESPACE_DISTRHO
#endif // EDITOR_HPP
