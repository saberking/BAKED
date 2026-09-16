#ifndef EDITOR_HPP
#define EDITOR_HPP
#include "src/DistrhoDefines.h"
#include "DistrhoUI.hpp"

#include "AudioData.hpp"
#include "external/implot.h"
#include "external/implot_internal.h"
#include "external/pocketfft_hdronly.h"
#include "DragAndDrop.hpp"


START_NAMESPACE_DISTRHO

struct PlotAudioContext {
    AudioData* audioData;
    int channel;
};

class SampleEditor : public DGL::ImGuiStandaloneWindow, public FileDropReceiver
{
public:
    AudioData *data=NULL;
    Module *module=NULL;
    ImPlotSpec spec;
    bool editMode=false;
    bool isMono;
    ImPlotContext* imPlotContext[6];
    std::vector<std::complex<float>> *spectrum[2] ;
    bool isSpectrumChanged=false;
    bool isLiveUpdate=true;
    bool isWaveformChanged=false;
    std::function<void(const char*)> fileDropped;
    std::function<void()> setDirty;
    int length;
    bool isDragging=false;
    float  dragStartY;
    int dragStartX;

    SampleEditor(const char *_name, Module *_module, Window& window,
                 std::function<void(const char*)> _fileDropped, std::function<void()> _setDirty
        ):
        DGL::ImGuiStandaloneWindow(window.getApp(), window) {
        module=_module;
        fileDropped=_fileDropped;
        setDirty=_setDirty;
        data=module->sample;
        spec.Flags = ImPlotFlags_CanvasOnly;
        setResizable(true);
        setSize(1400,970);
        for(int i=0;i<6;i++){
            imPlotContext[i]=ImPlot::CreateContext();
        }
        for(int channel=0;channel<2;channel++)
        {
            spectrum[channel] = new std::vector<std::complex<float>>  ( MAX_SAMPLE_LENGTH );
            for(int j=0;j<MAX_SAMPLE_LENGTH;j++)
            {
                (*spectrum[channel])[j]=std::complex(0,0);
            }

        }
        calculateFFT();
        isMono=(data->channels.load()==1);
    }

    Window& getWindow() const override {
        return DGL::ImGuiStandaloneWindow::getWindow();
    }
    void setDroppedFilePath(const char* path) override {
        fileDropped(path);
    }
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

    void calculateFFT(){
        for(int j=0;j<data->maxChannels;j++){
            //std::vector<std::complex<float>> data_in ( data->length );
            std::vector<float> data_in ( data->length.load(std::memory_order_relaxed) );
            pocketfft::shape_t shape_in{1};                                              // dimensions of the input shape
            pocketfft::stride_t stride_in{1};                    // must have the size of each element. Must have size() equal to shape_in.size()
            pocketfft::stride_t stride_out{1}; // must have the size of each element. Must have size() equal to shape_in.size()
            //stride_in[0]=stride_out[0]=sizeof ( std::complex<float> );
            stride_in[0]=sizeof ( float );
            stride_out[0]=sizeof ( std::complex<float> );
            bool forward{ pocketfft::FORWARD };                                            // FORWARD or BACKWARD

            float fct{ 2.0f /data->length.load(std::memory_order_relaxed)};    // scaling factor
            shape_in[0]=data->length.load(std::memory_order_relaxed);
            pocketfft::shape_t axes;

            axes.push_back ( 0 );
            for (int i=0; i<data->length.load(std::memory_order_relaxed); i++ )
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
            for(int i=0;i<data->length.load(std::memory_order_relaxed)/2;i++)
            {
                if(std::abs((*spectrum[j])[i])<0.000001)
                {
                    (*spectrum[j])[i]=std::complex(0.f,0.f);
                }
            }
        }
        isWaveformChanged=false;

    }

    void calculateWaveform(){
        std::vector<float> *waveform[2] ;

        for(int j=0;j<data->maxChannels;j++){
            waveform[j] = new std::vector<float> ( MAX_SAMPLE_LENGTH );

            std::vector<std::complex<float>> data_in ( data->length.load(std::memory_order_relaxed)/2+1 );
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
            for (int i=0; i<data->length.load(std::memory_order_relaxed)/2+1; i++ )
            {
                data_in[i]= (*spectrum[j])[i] ;
            }


            pocketfft::c2r (
                shape_in,
                stride_in,
                stride_out,
                axes,
                forward,
                data_in.data(),
                waveform[j]->data(),
                fct
                );
            for(int k=0;k<data->length.load(std::memory_order_relaxed);k++){
                data->sampleData[j][k].store((*waveform[j])[k]);
            }
            delete waveform[j];
            isSpectrumChanged=false;
            setDirty();
        }

    }

    void setInputMap(){
        ImPlotInputMap &inputMap=ImPlot::GetInputMap();
        if (editMode)
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
                ImPlot::DragLineX(0, &playhead, ImVec4(0.5,0.5,0.5,0.5), 0.5f, ImPlotDragToolFlags_NoInputs);
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

    void setWaveformSample(int channel, int x, float y)
    {
        data->sampleData[channel][x].store(clip(y));
        isWaveformChanged=true;
        setDirty();
    }

    void setSpectrumAmplitude(int channel, int x, float y)
    {
        (*spectrum[channel])[x]=std::polar(
            (float)std::max(y,0.f),
            (float)(std::abs((*spectrum[channel])[x])?std::arg((*spectrum[channel])[x]):-M_PI/2)
            );
        isSpectrumChanged=true;
    }

    void setSpectrumPhase(int channel, int x, float y)
    {
        (*spectrum[channel])[x]=std::polar(
            std::abs((*spectrum[channel])[x]),
            (float)(std::max(y,0.f)-M_PI/2)
            );
        isSpectrumChanged=true;
    }

    void handleDrag(int channel, int x, float y, std::function<void(int, int, float)> callback)
    {
        if(isDragging&&x!=dragStartX){
            int xStep = x>dragStartX?1:-1;
            int noOfSteps=std::abs(x-dragStartX);
            float yStep =(y-dragStartY)/noOfSteps;
            for(int index=1;index<noOfSteps;index++)
            {
                callback(channel,dragStartX+index*xStep, dragStartY+index*yStep);
            }
        }
        isDragging=true;
        dragStartX=x;
        dragStartY=y;
    }

    void onImGuiDisplay() override{

        ImGui::PushID(this);

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(getWidth(), getHeight()));
        int plotIndex=0;

        if(!data)return;
        length=data->length.load(std::memory_order_relaxed);
        if (ImGui::Begin("Waveform Analysis", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)){


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
                        if(isSpectrumChanged)calculateWaveform();
                        if(isWaveformChanged)calculateFFT();
                    }
                }
                if(!isLiveUpdate&&(isSpectrumChanged||isWaveformChanged))
                {
                    ImGui::SameLine();
                    if(ImGui::Button("Apply changes"))
                    {
                        if(isSpectrumChanged)calculateWaveform();
                        if(isWaveformChanged)calculateFFT();
                    }
                }
                float tempLength=length;
                ImGui::SliderFloat ("Length",&tempLength, 0,MAX_SAMPLE_LENGTH, "%.0f", ImGuiSliderFlags_Logarithmic);
                length=(int)tempLength;
                if(length!=data->length.load(std::memory_order_relaxed))
                {
                    data->length.store(length, std::memory_order_relaxed);
                    setDirty();
                    isWaveformChanged=true;
                    if(isLiveUpdate)
                    {
                        calculateFFT();
                    }
                }

            }


            if (ImGui::BeginTable("My2ColumnTable", isMono?1:2))
            {
                for(int channel=0;channel<1||!isMono&&channel<2;channel++){
                    ImGui::TableNextColumn();

                    ImPlot::SetCurrentContext(imPlotContext[plotIndex++]);
                    if(ImPlot::BeginPlot(channel?"Waveform R":"Waveform L")){
                        setInputMap();

                        ImPlot::SetupAxis(ImAxis_Y1, "Amplitude", ImPlotAxisFlags_Lock);
                        ImPlot::SetupAxisLimits(ImAxis_Y1, -1.1, 1.1, ImPlotCond_Always);

                        // Allow the X-axis to scroll and zoom normally
                        ImPlot::SetupAxis(ImAxis_X1, "Sample", ImPlotAxisFlags_None);
                        ImPlot::SetupAxisLimits(ImAxis_X1, -1.f, 200.f, ImPlotCond_Once);
                        PlotAudioContext plotAudioContext { data, channel };
                        ImPlot::PlotScatterG("Waveform", AtomicVectorGetter, &plotAudioContext, data->length.load(std::memory_order_relaxed), spec);
                        if (ImPlot::IsPlotHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&editMode) {
                            if(isSpectrumChanged)calculateWaveform();
                            ImPlotPoint current_pos = ImPlot::GetPlotMousePos();
                            if(current_pos.x>=0&&current_pos.x<MAX_SAMPLE_LENGTH){
                                setWaveformSample(channel, current_pos.x, current_pos.y);
                                if(data->length.load(std::memory_order_relaxed)<(int)current_pos.x+1){
                                    data->length.store((int)current_pos.x+1, std::memory_order_relaxed);
                                }
                            }
                            handleDrag(
                                channel, std::max(-1,std::min(MAX_SAMPLE_LENGTH,(int)current_pos.x)),current_pos.y,
                                [this](int channel, int x, float y){this->setWaveformSample(channel,x,y);}
                                );
                            if(isLiveUpdate&&isWaveformChanged)calculateFFT();
                        }
                        showPlayhead();
                        ImPlot::EndPlot();

                    }
                }

                for(int channel=0;channel<1||!isMono&&channel<2;channel++){
                    ImGui::TableNextColumn();

                    ImPlot::SetCurrentContext(imPlotContext[plotIndex++]);

                    if (ImPlot::BeginPlot(channel?"Spectrum R":"Spectrum L")){
                        setInputMap();
                        ImPlot::SetupAxis(ImAxis_Y1, "Amplitude", ImPlotAxisFlags_Lock);
                        ImPlot::SetupAxisLimits(ImAxis_Y1, -0.001, 1.0, ImPlotCond_Always);
                        ImPlot::SetupAxisScale(ImAxis_Y1, TransformForward_Sqrt, TransformInverse_Sqrt);

                        // Allow the X-axis to scroll and zoom normally
                        ImPlot::SetupAxis(ImAxis_X1, "Partial", ImPlotAxisFlags_None);
                        ImPlot::SetupAxisLimits(ImAxis_X1, -1.f, 100.f, ImPlotCond_Once);
                        ImPlot::PlotScatterG("Spectrum", SpectrumGetter, spectrum[channel], data->length.load(std::memory_order_relaxed)/2, spec);
                        if (ImPlot::IsPlotHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&editMode) {
                            if(isWaveformChanged)calculateFFT();
                            ImPlotPoint current_pos = ImPlot::GetPlotMousePos();
                            if(current_pos.x>=0&&current_pos.x<MAX_SAMPLE_LENGTH/2-1){
                                setSpectrumAmplitude(channel,current_pos.x,current_pos.y);
                                if(data->length.load(std::memory_order_relaxed)<(int)current_pos.x*2+2){
                                    data->length.store((int)current_pos.x*2+2, std::memory_order_relaxed);
                                    (*spectrum[channel])[current_pos.x+1]=std::complex(0.f,0.f);
                                }
                            }
                            handleDrag(
                                channel, std::max(-1,std::min(MAX_SAMPLE_LENGTH/2,(int)current_pos.x)),current_pos.y,
                                [this](int channel, int x, float y){this->setSpectrumAmplitude(channel,x,y);}
                                );

                            if(isLiveUpdate&&isSpectrumChanged)calculateWaveform();
                        }
                        ImPlot::EndPlot();
                    }

                }

                for(int channel=0;channel<1||!isMono&&channel<2;channel++){
                    ImGui::TableNextColumn();

                    ImPlot::SetCurrentContext(imPlotContext[plotIndex++]);
                    if (ImPlot::BeginPlot(channel?"Phase R":"Phase L")){
                        setInputMap();
                        ImPlot::SetupAxis(ImAxis_Y1, "Phase", ImPlotAxisFlags_Lock);
                        ImPlot::SetupAxisLimits(ImAxis_Y1, -0.3,2*M_PI+0.15, ImPlotCond_Always);

                        // Allow the X-axis to scroll and zoom normally
                        ImPlot::SetupAxis(ImAxis_X1, "Partial", ImPlotAxisFlags_None);
                        ImPlot::SetupAxisLimits(ImAxis_X1, -1.f, 100.f, ImPlotCond_Once);
                        ImPlot::PlotScatterG("Phase", PhaseGetter, spectrum[channel], data->length.load(std::memory_order_relaxed)/2, spec);
                        if (ImPlot::IsPlotHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&editMode) {
                            if(isWaveformChanged)calculateFFT();

                            ImPlotPoint current_pos = ImPlot::GetPlotMousePos();
                            if(current_pos.x>=0&&current_pos.x<data->length.load(std::memory_order_relaxed)/2-1&&std::abs((*spectrum[channel])[current_pos.x])){
                                setSpectrumPhase(channel, current_pos.x,current_pos.y);
                            }
                            handleDrag(
                                channel, std::max(-1,std::min(data->length.load(std::memory_order_relaxed)/2,(int)current_pos.x)),current_pos.y,
                                [this](int channel, int x, float y){this->setSpectrumPhase(channel,x,y);}
                                );
                            if(isLiveUpdate&&isSpectrumChanged)calculateWaveform();
                        }
                        ImPlot::EndPlot();
                    }


                }
                ImGui::EndTable();

            }
            if(!ImGui::IsMouseDown(ImGuiMouseButton_Left))isDragging=false;

        }
        ImGui::End();
        ImGui::PopID();

    }
    ~SampleEditor(){
        for(int i=0;i<4;i++){
            ImPlot::DestroyContext(imPlotContext[i]);
        }
        delete spectrum[0];delete spectrum[1];
    }
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleEditor)

};


END_NAMESPACE_DISTRHO
#endif // EDITOR_HPP
