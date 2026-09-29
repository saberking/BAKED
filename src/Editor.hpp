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
#include "fonts/font_data.h"


START_NAMESPACE_DISTRHO
#define LL_ICON     "\x36"
#define LR_ICON     "\x37"
#define RL_ICON     "\x38"
#define RR_ICON     "\x39"
#define HAND_ICON   "\x3a"
#define LINE_ICON   "\x3b"
#define PENCIL_ICON "\x3c"
#define UNDO_ICON   "\x3d"
#define COPY_ICON   "\x3e"
#define PASTE_ICON  "\x3f"
#define PLAYSTART_ICON "\x40"
#define ERASER_ICON "\x41"
#define ZERO_ICON   "\x42"
#define REDO_ICON   "\x43"

#define MAX_UNDO_DEPTH 12
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
    struct UndoItem
    {
        std::vector<float> data;
        std::vector<float> *dataPtr;
        std::vector<std::atomic<float>>*atomicDataPtr;
        bool isAtomic=false;
        bool shouldContinue;
        bool isHead=true;
        UndoItem(std::vector<float> *_data, bool _shouldContinue=false)
        {
            data.reserve(MAX_SAMPLE_LENGTH);
            data=*_data;
            dataPtr=_data;
            shouldContinue=_shouldContinue;
        }
        UndoItem(std::vector<std::atomic<float>> *_data, bool _shouldContinue=false)
        {
            data.reserve(MAX_SAMPLE_LENGTH);
            for(int i=0;i<_data->size();i++)
                data.push_back((*_data)[i].load(std::memory_order_relaxed));
            atomicDataPtr=_data;
            isAtomic=true;
            shouldContinue=_shouldContinue;
        }
        void apply()
        {
            if(isAtomic)
            {
                for(int i=0;i<data.size();i++)
                {
                    (*atomicDataPtr)[i].store(data[i],std::memory_order_relaxed);
                }

            }else{
                *dataPtr=data;
            }

        }
    };
    UndoItem *undoItems[MAX_UNDO_DEPTH];
    int nextUndoIndex=0;
    int undoCount=0,redoCount=0;
    AudioData *data=NULL;
    Module *module=NULL;
    ImPlotSpec spec;
    ImPlotContext** imPlotContext;
    std::vector<std::complex<float>> *spectrum[2] ;
    std::vector<float> *convolver;

    std::vector<std::complex<float>> *convolverSpectrum ;
    bool isSpectrumChanged[2];
    bool isLiveUpdate=true;
    bool isWaveformChanged[2];
    std::function<void(const char*)> fileDropped;
    std::function<void()> setDirty;
    std::function<void(int, bool)> editParameter;
    std::function<void(int, float)> setParameterValue;
    int length;
    bool isDragging=false;
    bool isDragSavedForUndo=false;
    float  dragStartY, dragEndY;
    int dragStartX, dragEndX;
    std::function<void(int,float,int)> dragCallback;
    int dragChannel;
    double spectrumXMax, spectrumXMin, waveformXMax, waveformXMin;
    float fSpeed = 1.f;
    ImGuiPluginDSP *dspPointer;
    Window *parentWindow;
    enum ToolbarButtons{
        toolbarButtonsHand,
        toolbarButtonsPencil,
        toolbarButtonsLine,
        toolbarButtonsEraser,
        toolbarButtonsCopy,
        toolbarButtonsPaste,
        toolbarButtonsCount
    };
    ToolbarButtons selectedButtonIndex=toolbarButtonsHand;
    static constexpr const char* toolbarIcons[6] = { HAND_ICON, PENCIL_ICON, LINE_ICON, ERASER_ICON,COPY_ICON,PASTE_ICON };

    enum DataType{
        dataTypeEnvelope,
        dataTypeConvolver,
        dataTypeWaveL,
        dataTypeWaveR,
        dataTypeSpectrumL,
        dataTypeSpectrumR,
        dataTypePhaseL,
        dataTypePhaseR
    };
    DataType dragDataType;
    ImFont *iconFontLarge,*iconFontRegular;


    SampleEditor(const char *_name, Module *_module, Window& _window,
                 std::function<void(const char*)> _fileDropped, std::function<void()> _setDirty,
                 ImPlotContext* _imPlotContext [NO_OF_PLOT_CONTEXTS],
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

        convolver=new std::vector<float>(MAX_SAMPLE_LENGTH);
        for(int i=0;i<MAX_SAMPLE_LENGTH;i++)
        {
            (*convolver)[i]=0.f;
        }

        for(int channel=0;channel<2;channel++)
        {
            spectrum[channel] = new std::vector<std::complex<float>>  ( MAX_SAMPLE_LENGTH/2+1 );
            for(int j=0;j<MAX_SAMPLE_LENGTH/2+1;j++)
            {
                (*spectrum[channel])[j]=std::complex(0,0);
            }

        }
        convolverSpectrum=new std::vector<std::complex<float>> (MAX_SAMPLE_LENGTH/2+1);
        for(int j=0;j<MAX_SAMPLE_LENGTH/2+1;j++)
        {
            (*convolverSpectrum)[j]=std::complex(0,0);
        }

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

        for(int i=0;i<MAX_UNDO_DEPTH;i++)
        {
            undoItems[i]=NULL;
        }

        setupFonts();
    }

    void setupFonts()
    {
        ImGuiIO& io = ImGui::GetIO();

        // 1. Load default font first (Assigns to slot 0)
        io.Fonts->AddFontDefault();

        // 2. Load your custom font INDEPENDENTLY (Do NOT set config.MergeMode = true)
        ImFontConfig config;
        config.PixelSnapH = true;
        config.FontDataOwnedByAtlas = false; // Prevents global array memory freeing crash

        // Save the returned pointer to a global or class variable (e.g., ImFont* m_IconFont)
        iconFontRegular = io.Fonts->AddFontFromMemoryTTF(
            (void*)Untitled1_ttf,
            sizeof(Untitled1_ttf),
            22.0f,
            &config
            );

        iconFontLarge = io.Fonts->AddFontFromMemoryTTF(
            (void*)Untitled1_ttf,
            sizeof(Untitled1_ttf),
            24.0f,
            &config
            );

        io.Fonts->Build();
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




    void calculateFFT(int j){//j=2 for convolver
        pocketfft::shape_t shape_in{1};                                              // dimensions of the input shape
        pocketfft::stride_t stride_in{1};                    // must have the size of each element. Must have size() equal to shape_in.size()
        pocketfft::stride_t stride_out{1}; // must have the size of each element. Must have size() equal to shape_in.size()
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
            if(j<2){
                data_in[i]=data->sampleData[j][i].load(std::memory_order_relaxed);
            }else{
                data_in[i]=(*convolver)[i];
            }
        }

        pocketfft::r2c (
            shape_in,
            stride_in,
            stride_out,
            axes,
            forward,
            data_in.data(),
            j<2?spectrum[j]->data():convolverSpectrum->data(),
            fct
            );

        if(j<2){
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
        }else{
            (*convolverSpectrum)[0]/=2;
            (*convolverSpectrum)[shape_in[0]/2]/=2;
        }


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

    void setInputMap(){
        ImPlotInputMap &inputMap=ImPlot::GetInputMap();
        if (selectedButtonIndex)
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
    void setConvolver(int x, float y)
    {
        if(x<0||x>=ENVELOPE_LENGTH) return;
        (*convolver)[x]=std::max(0.f,std::min(1.f,y));
    }
    void erase(std::vector<float> &vec, int start, int end)
    {
        auto move_target = vec.begin() + std::min(start, end);
        auto move_source = move_target + std::abs(start-end);
        int target_idx = std::distance(vec.begin(), move_target);
        int source_idx = std::distance(vec.begin(), move_source);

        std::cout << "Erasong!!!!!!!!!!!!!!" << target_idx << ", " << source_idx << std::endl;

        std::move(move_source, vec.end(), move_target);

        auto pad_start = vec.end() - std::abs(start-end);
        std::fill(pad_start, vec.end(), 0.f);
    }
    void erase(std::vector<std::atomic<float>> &vec, int start, int end)
    {
        int start_idx = std::min(start, end);
        int end_idx = std::max(start, end);
        int count = end_idx - start_idx;

        if (count <= 0 || start_idx >= vec.size()) return;

        // 1. Shift everything after the deleted chunk to the left (your std::move replacement)
        int write_ptr = start_idx;
        int read_ptr = end_idx;
        int total_elements = static_cast<int>(vec.size());

        while (read_ptr < total_elements) {
            float value_to_move = vec[read_ptr].load(std::memory_order_relaxed);
            vec[write_ptr].store(value_to_move, std::memory_order_relaxed);
            write_ptr++;
            read_ptr++;
        }

        // 2. Pad the end of the vector with zeros (your std::fill replacement)
        while (write_ptr < total_elements) {
            vec[write_ptr].store(0.0f, std::memory_order_relaxed);
            write_ptr++;
        }
    }
    void eraseWaveform(int x, float y, int channel)
    {
        int start=std::max(0,std::min(MAX_SAMPLE_LENGTH-1,dragStartX));
        int end=std::max(0,std::min(MAX_SAMPLE_LENGTH-1,x));

        erase(module->sample->sampleData[channel], start, end);

    }
    void eraseEnvelope(int x, float y)
    {
        int start=std::max(0,std::min(ENVELOPE_LENGTH-1,dragStartX));
        int end=std::max(0,std::min(ENVELOPE_LENGTH-1,x));
        erase(module->envelope,start,end);
    }
    void eraseConvolver(int x, float y)
    {
        int start=std::max(0,std::min(MAX_SAMPLE_LENGTH-1,dragStartX));
        int end=std::max(0,std::min(MAX_SAMPLE_LENGTH-1,x));

        erase(*convolver, start, end);
    }
    void startDrag(int x, float y, std::function<void(int, float, int)> callback, int channel, DataType type)
    {
        std::cout<<"starting drag"<<isDragSavedForUndo<<std::endl;
        isDragging=true;
        dragStartX=x;
        dragStartY=y;
        dragCallback=callback;
        dragChannel=channel;
        dragDataType=type;
        if(!isDragSavedForUndo)
        {
            isDragSavedForUndo=true;
            std::cout<<"Adding drag undo in startdraf"<<std::endl;
            addDragUndoItem(type);
        }
    }
    void endDrag()
    {
        isDragSavedForUndo=false;
        if(isDragging)
        {
            std::cout<<"ending dra "<<(int)selectedButtonIndex<<std::endl;
            if(selectedButtonIndex==toolbarButtonsLine)
            {

                drawLine(dragEndX,dragEndY);
                if(isLiveUpdate)
                {
                    for(int i=0;i<2;i++)
                    {
                        if(isSpectrumChanged[i])
                        {
                            calculateWaveform(i);
                        }
                        if(isWaveformChanged[i])
                        {
                            calculateFFT(i);
                        }

                    }
                    module->process();
                }
            }
            if(selectedButtonIndex==toolbarButtonsEraser)
            {
                dragCallback(dragEndX,0.f, dragChannel);
            }
            isDragging=false;



        }
    }
    void addDragUndoItem(DataType type)
    {
        if(type==dataTypeWaveL)
            addUndoItem(&(module->sample->sampleData[0]));
        if(type==dataTypeWaveR)
            addUndoItem(&(module->sample->sampleData[1]));
        if(type==dataTypeEnvelope)
            addUndoItem(&(module->envelope));
        if(type==dataTypeConvolver)
            addUndoItem(convolver);
        addLiveUpdateUndoItem();
    }
    void addLiveUpdateUndoItem()
    {
        if(dragDataType==dataTypeSpectrumL||dragDataType==dataTypePhaseL) addUndoItem(&(module->sample->sampleData[0]));
        if(dragDataType==dataTypeSpectrumR||dragDataType==dataTypePhaseR) addUndoItem(&(module->sample->sampleData[1]));
    }
    void drawLine(int x, float y)
    {


        int xStep = x>dragStartX?1:-1;
        int noOfSteps=std::abs(x-dragStartX)+1;
        float yStep =(y-dragStartY)/noOfSteps;
        for(int index=0;index<noOfSteps;index++)
        {
            dragCallback(dragStartX+index*xStep, dragStartY+index*yStep, dragChannel);
        }
    }
    void updateDragEnd(int x, float y)
    {
        dragEndX=x;dragEndY=y;
    }
    void handleDrag(int x, float y, std::function<void(int, float, int)> callback, DataType type, int channel=0)
    {
        if(selectedButtonIndex==toolbarButtonsPencil)
        {
            if(!isDragSavedForUndo)
            {
                std::cout<<"Adding drag undo in handldrag"<<std::endl;

                isDragSavedForUndo=true;
                addDragUndoItem(type);
            }

            if(isDragging){
                drawLine(x,y);
            }else{
                callback(x, y, channel);
            }
            startDrag(x,y, callback, channel, type );

        }else if(selectedButtonIndex==toolbarButtonsLine||selectedButtonIndex==toolbarButtonsEraser){
            if(!isDragging)
                startDrag(x,y, callback, channel, type);
        }
        updateDragEnd(x,y);
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

    void compress()
    {
        for (int j=0;j<2;j++)
        {
            addUndoItem(&(module->sample->sampleData[j]),j==1);
            for (int i=0;i<module->sample->length.load(std::memory_order_relaxed);i++)
            {
                float sample=module->sample->sampleData[j][i].load(std::memory_order_relaxed);
                float cuberoot=std::cbrt(sample);
                module->sample->sampleData[j][i].store(0.9*sample+0.1*std::copysign(cuberoot*cuberoot,sample));
            }
            isWaveformChanged[j]=true;
        }

        if(isLiveUpdate)
        {
            module->process();
            for(int j=0;j<2;j++)
            {
                calculateFFT(j);

            }
        }

    }
    void filter()
    {
        for (int j=0;j<2;j++)
        {
            if (isWaveformChanged[j]) calculateFFT(j);
            int length=module->sample->length.load(std::memory_order_relaxed)/2+1;
            float multiplier=std::powf(0.000001,4/3*length);
            float exponential=1.f;
            for (int i=length/4;i<length;i++)
            {
                exponential*=multiplier;
                (*spectrum[j])[i]=(*spectrum[j])[i]*exponential;
            }
            addUndoItem(&(module->sample->sampleData[j]),j==1);

            calculateWaveform(j);
        }
    }
    void normalise()
    {
        for(int j=0;j<2;j++)
        {
            addUndoItem(&(module->sample->sampleData[j]),j==1);

            float max=0.000001f;
            for(int i=0;i<module->sample->length.load(std::memory_order_relaxed);i++)
                max=std::max(max,std::abs(module->sample->sampleData[j][i].load(std::memory_order_relaxed)));
            float recip=1/max;
            for(int i=0;i<module->sample->length.load(std::memory_order_relaxed);i++)
                module->sample->sampleData[j][i].store(module->sample->sampleData[j][i].load(std::memory_order_relaxed)*recip);
            isWaveformChanged[j]=true;
            if(isLiveUpdate) calculateFFT(j);
        }

    }
    void convolve ()
    {
        for(int j=0;j<2;j++)
        {
            addUndoItem(&(module->sample->sampleData[j]),j==1);

            if(isSpectrumChanged[j]){

                calculateWaveform(j);
            }
        }
        int sampleLength=module->sample->length.load(std::memory_order_relaxed);
        int totalLength=std::min(MAX_SAMPLE_LENGTH,sampleLength+ENVELOPE_LENGTH-1);
        module->sample->length.store(totalLength,std::memory_order_relaxed);
        calculateFFT(2);//convolver

        for(int j=0;j<2;j++)
        {
            for(int i=sampleLength;i<totalLength;i++)
            {
                module->sample->sampleData[j][i].store(0.f,std::memory_order_relaxed);

            }
            calculateFFT(j);
            for(int i=0;i<totalLength/2+1;i++){
                (*spectrum[j])[i]*=(*convolverSpectrum)[i]*(float)totalLength/(float)2;
            }
            calculateWaveform(j);


        }



    }
    UndoItem *copyUndoItem(UndoItem *item)
    {
        UndoItem *tempRedoPtr;
        if(item->isAtomic)
            tempRedoPtr=new UndoItem(item->atomicDataPtr);
        else  tempRedoPtr=new UndoItem(item->dataPtr);
        tempRedoPtr->isHead=item->isHead;
        tempRedoPtr->shouldContinue=item->shouldContinue;
        return tempRedoPtr;
    }
    void undo()
    {
        undoCount=std::max(0,undoCount-1);
        redoCount=std::min(MAX_UNDO_DEPTH-2,redoCount+1);

        std::cout<<"undo "<<nextUndoIndex<<std::endl;
        int currentIndex=(nextUndoIndex-1+MAX_UNDO_DEPTH)%MAX_UNDO_DEPTH;
        int lastIndex=(currentIndex-1+MAX_UNDO_DEPTH)%MAX_UNDO_DEPTH;

        bool shouldContinue=(!(undoItems[lastIndex]&&undoItems[lastIndex]->isHead)&&undoItems[currentIndex]->shouldContinue);
        nextUndoIndex=currentIndex;

        UndoItem *tempRedoPtr=copyUndoItem(undoItems[nextUndoIndex]);

        undoItems[nextUndoIndex]->apply();

        delete(undoItems[nextUndoIndex]);
        undoItems[nextUndoIndex]=tempRedoPtr;
        for(int i=0;i<2;i++)
        {
            calculateFFT(i);
        }
        module->process();
        if(shouldContinue) undo();
        std::cout<<"finished undoing"<<nextUndoIndex<<std::endl;
    }
    void redo()
    {
        undoCount=std::min(MAX_UNDO_DEPTH-2,undoCount+1);
        redoCount=std::max(0,redoCount-1);

        std::cout<<"redo"<<nextUndoIndex<<std::endl;
        int nextIndex=(nextUndoIndex+1)%MAX_UNDO_DEPTH;
        bool shouldContinue=(!undoItems[nextUndoIndex]->isHead&&undoItems[nextIndex]&&undoItems[nextIndex]->shouldContinue);
        UndoItem *tempRedoPtr=copyUndoItem(undoItems[nextUndoIndex]);


        undoItems[nextUndoIndex]->apply();
        delete(undoItems[nextUndoIndex]);
        undoItems[nextUndoIndex]=tempRedoPtr;
        nextUndoIndex=nextIndex;
        for(int i=0;i<2;i++)
        {
            calculateFFT(i);
        }
        module->process();
        if(shouldContinue) redo();
    }
    template <typename T>
    void addUndoItem(std::vector<T>* data, bool shouldContinue = false)
    {
        undoCount=std::min(MAX_UNDO_DEPTH-2,undoCount+1);
        redoCount=std::max(0,redoCount-1);
        std::cout<<"addundoitem "<<nextUndoIndex<<std::endl;
        int currentIndex=(nextUndoIndex-1+MAX_UNDO_DEPTH)%MAX_UNDO_DEPTH;
        bool isHead=true;
        if(undoItems[currentIndex])
        {
            if(undoItems[currentIndex]->isHead) undoItems[currentIndex]->isHead=false;
            else isHead=false;
        }
        if(undoItems[nextUndoIndex])
        {
            isHead=undoItems[nextUndoIndex]->isHead;
            if(undoItems[nextUndoIndex]->isHead)isHead=true;
            delete(undoItems[nextUndoIndex]);
        }
        undoItems[nextUndoIndex]=new UndoItem(data, shouldContinue);
        undoItems[nextUndoIndex]->isHead=isHead;
        nextUndoIndex=(nextUndoIndex+1)%MAX_UNDO_DEPTH;
        if(undoItems[nextUndoIndex]&&undoItems[nextUndoIndex]->shouldContinue)
        {
            undoItems[nextUndoIndex]->shouldContinue=false;
        }
    }
    void displayButtonSelector(const char* const*labels,int length,int &selectedIndex, bool large=false)
    {
        if(large)ImGui::PushFont(iconFontLarge);
        else ImGui::PushFont(iconFontRegular);

        for (int i = 0; i < length; i++) {
            bool is_selected = (selectedIndex == i);

            if (is_selected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
            }

            if (ImGui::Button(labels[i])) {
                selectedIndex = i; // Update selection state on click
            }

            if (is_selected) {
                ImGui::PopStyleColor(2);
            }

            if (i < length-1) {
                ImGui::SameLine();
            }
        }
        ImGui::PopFont();

    }
    void displaySpeakerConnections()
    {
        const char *labels[]={LL_ICON,LR_ICON,RL_ICON,RR_ICON};
        int temp=module->speakerConnections.load(std::memory_order_relaxed);
        displayButtonSelector(labels,4,temp);
        module->speakerConnections.store(static_cast<SpeakerConnections>( temp),std::memory_order_relaxed);
    }
    void displayToolbar()
    {
        if (ImGui::BeginTable("Toolbar Table", 3, ImGuiTableFlags_SizingStretchSame| ImGuiTableFlags_SizingFixedFit)){
            ImGui::TableSetupColumn("Left Empty", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Toolbar Column", ImGuiTableColumnFlags_WidthFixed, 0.0f);
            ImGui::TableSetupColumn("Right Empty", ImGuiTableColumnFlags_WidthStretch); // (Optional mixed layout)

            ImGui::TableNextColumn();ImGui::TableNextColumn();


            // if(ImGui::BeginChild("Toolbar", ImVec2(0.f, 0.f), ImGuiChildFlags_Border|ImGuiChildFlags_AutoResizeY))
            // {
            ImGui::PushFont(iconFontLarge);
            int currentIndex=(nextUndoIndex+MAX_UNDO_DEPTH-1)%MAX_UNDO_DEPTH;
            bool redoDataAvailable=(undoItems[nextUndoIndex]!=NULL);
            bool isHead=(undoItems[currentIndex]&&undoItems[currentIndex]->isHead);
            //bool canRedo=(redoDataAvailable&&!isHead);
            bool canRedo=(bool)redoCount;
            bool canUndo=(undoItems[currentIndex]&&undoCount);
            ImGui::BeginDisabled(!canUndo);
            if(ImGui::Button(UNDO_ICON)&&canUndo)
            {

                undo();
            }
            ImGui::EndDisabled();
            ImGui::SameLine();

            ImGui::BeginDisabled(!canRedo);


            if(ImGui::Button(REDO_ICON)&&canRedo)
            {

                redo();
            }
            ImGui::EndDisabled();

            ImGui::PopFont();
            ImGui::SameLine();
            int selected=static_cast<int>(selectedButtonIndex);
            displayButtonSelector(toolbarIcons,toolbarButtonsCount,selected, true);
            selectedButtonIndex=static_cast<ToolbarButtons>(selected);

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
                    showUpdateButton=(showUpdateButton||isSpectrumChanged[j]||isWaveformChanged[j]);
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

            ImGui::EndTable();
        }
    }
    void displayPlaybackControls()
    {
        ImGui::SetNextItemWidth(-100.f);
        if (ImGui::SliderFloat("Speed", &fSpeed, 0.f, 1.f))
        {
            if (ImGui::IsItemActivated())
            {
                    editParameter(kParamSpeed, true);
            }
            fSpeed=std::max(0.f,std::min(1.f,fSpeed));

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
                        // int xOffset=0,yOffset=0;
                        // if (isVisible())getEmbeddedSubwindowOffset(xOffset,yOffset);
                        // payload->screenX = mousePos.x+xOffset;
                        // payload->screenY = mousePos.y+yOffset;

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

    void configureSmallGraph()
    {
        setInputMap();

        ImPlot::SetupAxis(ImAxis_Y1, "", ImPlotAxisFlags_Lock|ImPlotAxisFlags_NoGridLines);
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
    }

    void displayEnvelope(){
        ImPlot::SetCurrentContext(imPlotContext[6]);
        if(ImPlot::BeginPlot("Envelope",ImVec2(-1.0f, 200.0f))){
            configureSmallGraph();
            ImPlot::PlotScatterG("Envelope", envelopeGetter, &dspPointer->modules[0]->envelope, ENVELOPE_LENGTH, spec);
            if (ImPlot::IsPlotHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                if(selectedButtonIndex==toolbarButtonsPencil||selectedButtonIndex==toolbarButtonsLine||selectedButtonIndex==toolbarButtonsEraser) {
                    ImPlotPoint current_pos = ImPlot::GetPlotMousePos();

                    handleDrag(
                        (int)current_pos.x,current_pos.y,
                        [this](int x, float y, int dummy){
                            this->selectedButtonIndex==toolbarButtonsEraser?this->eraseEnvelope(x,y):this->setEnvelope(x,y);
                        },
                        dataTypeEnvelope
                        );
                    // if (isLiveUpdate)
                    // {
                    //     module->process();
                    // }
                }
            }
            ImPlot::EndPlot();

        }
    }

    void displayConvolver()
    {

        ImPlot::SetCurrentContext(imPlotContext[6]);
        if(ImPlot::BeginPlot("Convolver##convolverplot",ImVec2(-1.0f, 200.0f))){
            configureSmallGraph();
            std::vector<float> xValues;
            for(int i=0;i<ENVELOPE_LENGTH;i++)
            {
                xValues.push_back(i);
            }
            ImPlot::PlotScatter("Convolver", xValues.data(), (*convolver).data(), ENVELOPE_LENGTH, spec);
            if (ImPlot::IsPlotHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                if(selectedButtonIndex==toolbarButtonsPencil||selectedButtonIndex==toolbarButtonsLine||selectedButtonIndex==toolbarButtonsEraser)
                {
                    ImPlotPoint current_pos = ImPlot::GetPlotMousePos();

                    handleDrag(
                        (int)current_pos.x,current_pos.y,
                        [this](int x, float y, int dummy){
                            this->selectedButtonIndex==toolbarButtonsEraser?this->eraseConvolver(x,y):this->setConvolver(x,y);
                        },
                        dataTypeConvolver
                        );

                }
            }
            ImPlot::EndPlot();

        }
        if(ImGui::Button("Convolve"))
        {
            convolve();
        }

    }



    void displayProcessing()
    {

        displayConvolver();
        ImGui::Separator();
        if(ImGui::Button("Compress"))
        {
            compress();
        }
        ImGui::Separator();

        if(ImGui::Button("Filter"))
        {
            filter();
        }
        ImGui::Separator();
        if(ImGui::Button("Normalise"))
        {
            normalise();
        }



    }


    void displaySample()
    {
        int plotIndex=0;

        length=data->length.load(std::memory_order_relaxed);


        displaySpeakerConnections();
        SpeakerConnections connections=module->speakerConnections.load(std::memory_order_relaxed);
        bool isMono=(connections==speakerLL||connections==speakerRR);
        ImGui::SameLine();



        float tempLength=length;
        ImGui::SetNextItemWidth(-150);
        ImGui::SliderFloat ("Length",&tempLength, 1,MAX_SAMPLE_LENGTH, "%.0f", ImGuiSliderFlags_Logarithmic);
        length=std::max(1, std::min(MAX_SAMPLE_LENGTH,(int)tempLength));
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


        ImVec2 plotSize(-1,260);
        if (ImGui::BeginTable("SampleTable", isMono?1:2, ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_Resizable))
        {
            for(int channel=0;channel<1||!isMono&&channel<2;channel++){
                ImGui::TableNextColumn();
                if(connections==speakerRR)channel=1;
                ImPlot::SetCurrentContext(imPlotContext[plotIndex++]);
                if(ImPlot::BeginPlot(channel?"Waveform R":"Waveform L", plotSize)){
                    setInputMap();

                    ImPlot::SetupAxis(ImAxis_Y1, "", ImPlotAxisFlags_Lock);
                    ImPlot::SetupAxisLimits(ImAxis_Y1, -1.1, 1.1, ImPlotCond_Always);

                    ImPlot::SetupAxis(ImAxis_X1, "", ImPlotAxisFlags_None);
                    ImPlot::SetupAxisLinks(ImAxis_X1, &waveformXMin, &waveformXMax);
                    ImPlot::SetupAxisLimits(ImAxis_X1, -1.f, 200.f, ImPlotCond_Once);
                    ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, -100, MAX_SAMPLE_LENGTH+1000);
                    ImPlot::SetupAxisZoomConstraints(ImAxis_X1, 40, MAX_SAMPLE_LENGTH+2000);


                    PlotAudioContext plotAudioContext { data, channel };
                    ImPlot::PlotScatterG("Waveform", AtomicVectorGetter, &plotAudioContext, data->length.load(std::memory_order_relaxed), spec);
                    if (ImPlot::IsPlotHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    {
                        if(selectedButtonIndex==toolbarButtonsPencil||selectedButtonIndex==toolbarButtonsLine||selectedButtonIndex==toolbarButtonsEraser)
                        {
                            if(isSpectrumChanged[channel])
                            {
                                addUndoItem(&(module->sample->sampleData[channel]));

                                calculateWaveform(channel);
                            }
                            ImPlotPoint current_pos = ImPlot::GetPlotMousePos();

                            handleDrag(
                                (int)current_pos.x,current_pos.y,
                                [this]( int x, float y, int channel){
                                    this->selectedButtonIndex==toolbarButtonsEraser?this->eraseWaveform(x,y, channel):this->setWaveformSample(x,y,channel);
                                },
                                channel?dataTypeWaveR:dataTypeWaveL,
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
                    }
                    showPlayhead();
                    ImPlot::EndPlot();

                }

                ImPlot::SetCurrentContext(imPlotContext[plotIndex++]);

                if (ImPlot::BeginPlot(channel?"Spectrum R":"Spectrum L",plotSize)){
                    setInputMap();
                    ImPlot::SetupAxis(ImAxis_Y1, "", ImPlotAxisFlags_Lock);
                    ImPlot::SetupAxisLimits(ImAxis_Y1, -0.001, 1.1, ImPlotCond_Always);
                    ImPlot::SetupAxisScale(ImAxis_Y1, TransformForward_Sqrt, TransformInverse_Sqrt);

                    ImPlot::SetupAxis(ImAxis_X1, "", ImPlotAxisFlags_None);
                    ImPlot::SetupAxisLinks(ImAxis_X1, &(spectrumXMin), &(spectrumXMax));

                    ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, -50, MAX_SAMPLE_LENGTH/2+500);
                    ImPlot::SetupAxisZoomConstraints(ImAxis_X1, 20, MAX_SAMPLE_LENGTH/2+1000);

                    ImPlot::PlotScatterG("Spectrum", SpectrumGetter, spectrum[channel], data->length.load(std::memory_order_relaxed)/2+1, spec);
                    if (ImPlot::IsPlotHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    {
                        if(selectedButtonIndex==toolbarButtonsPencil||selectedButtonIndex==toolbarButtonsLine)
                        {
                            if(isWaveformChanged[channel])calculateFFT(channel);
                            ImPlotPoint current_pos = ImPlot::GetPlotMousePos();
                            //(*spectrum[channel])[current_pos.x+1]=std::complex(0.f,0.f);

                            handleDrag(
                                (int)current_pos.x,current_pos.y,
                                [this](int x, float y, int channel){
                                    this->setSpectrumAmplitude(x,y,channel);
                                },
                                channel?dataTypeSpectrumR:dataTypeSpectrumL,
                                channel
                                );

                            // int newLength=std::max(
                            //     data->length.load(std::memory_order_relaxed),
                            //     std::max(0,std::min(MAX_SAMPLE_LENGTH, (int)current_pos.x*2))
                            //     );

                            // data->length.store(newLength, std::memory_order_relaxed);

                            if(isLiveUpdate&&isSpectrumChanged[channel])
                            {

                                calculateWaveform(channel);
                            }

                        }
                    }
                    ImPlot::EndPlot();
                }



                ImPlot::SetCurrentContext(imPlotContext[plotIndex++]);
                if (ImPlot::BeginPlot(channel?"Phase R":"Phase L", plotSize)){
                    setInputMap();
                    ImPlot::SetupAxis(ImAxis_Y1, "", ImPlotAxisFlags_Lock);
                    ImPlot::SetupAxisLimits(ImAxis_Y1, -0.3,2*M_PI+0.15, ImPlotCond_Always);

                    ImPlot::SetupAxis(ImAxis_X1, "", ImPlotAxisFlags_None);
                    ImPlot::SetupAxisLinks(ImAxis_X1, &(spectrumXMin), &(spectrumXMax));
                    ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, -50, MAX_SAMPLE_LENGTH/2+500);
                    ImPlot::SetupAxisZoomConstraints(ImAxis_X1, 20, MAX_SAMPLE_LENGTH/2+1000);

                    ImPlot::PlotScatterG("Phase", PhaseGetter, spectrum[channel], data->length.load(std::memory_order_relaxed)/2+1, spec);
                    if (ImPlot::IsPlotHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    {
                        if(selectedButtonIndex==toolbarButtonsPencil||selectedButtonIndex==toolbarButtonsLine)
                        {
                            if(isWaveformChanged[channel])calculateFFT(channel);

                            ImPlotPoint current_pos = ImPlot::GetPlotMousePos();

                            handleDrag(
                                (int)current_pos.x,current_pos.y,
                                [this](int x, float y, int channel){
                                    this->setSpectrumPhase(x,y,channel);
                                },
                                channel?dataTypePhaseR:dataTypePhaseL,
                                channel
                                );
                            if(isLiveUpdate&&isSpectrumChanged[channel])
                            {

                                calculateWaveform(channel);
                            }
                        }
                    }
                    ImPlot::EndPlot();
                }


            }

            ImGui::EndTable();
        }




    }

    void display()
    {
        displayToolbar();
        float separator_width = ImGui::GetContentRegionAvail().x;
        //separator_width-=ImGui::GetStyle().ScrollbarSize;
        ImGui::BeginChild("ShortSeparator", ImVec2(separator_width, 2), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImGui::Separator();

        ImGui::EndChild();
        if(ImGui::BeginTable("Main Table", 2, ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("Master Column", ImGuiTableColumnFlags_WidthStretch, 2.f);
            ImGui::TableSetupColumn("Second Column", ImGuiTableColumnFlags_WidthStretch, 1.f);

            ImGui::TableNextColumn();
            if (ImGui::BeginChild("LeftCol", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar))
            {
                displaySample();

            }
            // Save the height calculated by the first column this frame
            float columnOneHeight = ImGui::GetWindowHeight();
            ImGui::EndChild();

            ImGui::TableNextColumn();

            if (ImGui::BeginChild("ScrollableRegion", ImVec2(0, columnOneHeight), ImGuiChildFlags_None, ImGuiWindowFlags_None))
            {
                float rigid_width = ImGui::GetContentRegionAvail().x;

                // if(!ImGui::GetCurrentWindow()->ScrollbarY)
                //     rigid_width-= ImGui::GetStyle().ScrollbarSize;

                if (ImGui::BeginChild("RigidContentBox", ImVec2(rigid_width, 0), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar))
                {

                    displayProcessing();
                    ImGui::Separator();
                    displayEnvelope();
                    ImGui::Separator();
                    displayPlaybackControls();
                }
                ImGui::EndChild();

            }
            ImGui::EndChild(); // End the scrollable region
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
        delete convolverSpectrum;
        delete convolver;
        HWND hwnd = (HWND)parentWindow->getNativeWindowHandle();
        ::RemoveWindowSubclass(hwnd, SubclassMenuProc, reinterpret_cast<UINT_PTR>(this));
        for(int i=0;i<MAX_UNDO_DEPTH;i++)
        {
            if(undoItems[i])delete undoItems[i];
        }

    }
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleEditor)

};


END_NAMESPACE_DISTRHO
#endif // EDITOR_HPP
