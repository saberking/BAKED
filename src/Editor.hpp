#ifndef EDITOR_HPP
#define EDITOR_HPP
#include "src/DistrhoDefines.h"
#include "DistrhoUI.hpp"

#include "AudioData.hpp"
#include "external/implot.h"
#include "external/implot_internal.h"
#include "external/pocketfft_hdronly.h"


START_NAMESPACE_DISTRHO

struct PlotAudioContext {
    AudioData* audioData;
    int channel;
};

enum EditorViews {
    ev_waveform=0,
    ev_partialAmplitude,
    ev_partialPhase,
    ev_count
};

const char *editorViewNames[] = {"Waveform", "Partial amplitude", "Partial phase"};

class SampleEditor : public DGL::ImGuiStandaloneWindow
{
public:
    AudioData *data=NULL;
    Module *module=NULL;
    bool isRelease=false;
    char name[MAX_FILE_PATH_LENGTH];
    ImPlotSpec spec;
    bool editMode=false;
    bool isMono;
    ImPlotContext* imPlotContext[4];
    std::vector<std::complex<float>> *spectrum[2] ;
    EditorViews currentView=ev_waveform;
    bool isSpectrumChanged=false;
    bool isLiveUpdate=true;
    bool warp=false;

    SampleEditor(const char *_name, Module *_module, Window& window):
        DGL::ImGuiStandaloneWindow(window.getApp(), window) {
        strcpy(name, _name);
        module=_module;
        if(!isRelease)data=module->sample;
        spec.Flags = ImPlotFlags_CanvasOnly;
        setResizable(true);
        setSize(900,666);
        for(int i=0;i<4;i++){
            imPlotContext[i]=ImPlot::CreateContext();
        }
        for(int channel=0;channel<1;channel++)
        {
            spectrum[channel] = new std::vector<std::complex<float>>  ( MAX_SAMPLE_LENGTH );
            for(int j=0;j<MAX_SAMPLE_LENGTH;j++)
            {
                (*spectrum[channel])[j]=std::complex(0,0);
            }

        }
        spectrum[1] = new std::vector<std::complex<float>>  ( MAX_SAMPLE_LENGTH );
        isMono=(data->channels==1);
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
            std::vector<float> data_in ( data->length );
            pocketfft::shape_t shape_in{1};                                              // dimensions of the input shape
            pocketfft::stride_t stride_in{1};                    // must have the size of each element. Must have size() equal to shape_in.size()
            pocketfft::stride_t stride_out{1}; // must have the size of each element. Must have size() equal to shape_in.size()
            //stride_in[0]=stride_out[0]=sizeof ( std::complex<float> );
            stride_in[0]=sizeof ( float );
            stride_out[0]=sizeof ( std::complex<float> );
            bool forward{ pocketfft::FORWARD };                                            // FORWARD or BACKWARD

            float fct{ 2.0f /data->length};    // scaling factor
            shape_in[0]=data->length;
            pocketfft::shape_t axes;

            axes.push_back ( 0 );
            for (int i=0; i<data->length; i++ )
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
            for(int i=0;i<data->length/2;i++)
            {
                if(std::abs((*spectrum[j])[i])<0.000001)
                {
                    (*spectrum[j])[i]=std::complex(0.f,0.f);
                }
            }
        }


    }

    void calculateWaveform(){
        std::vector<float> *waveform[2] ;

        for(int j=0;j<data->maxChannels;j++){
            waveform[j] = new std::vector<float> ( MAX_SAMPLE_LENGTH );

            std::vector<std::complex<float>> data_in ( data->length/2+1 );
            pocketfft::shape_t shape_in{1};                                              // dimensions of the input shape
            pocketfft::stride_t stride_in{1};                    // must have the size of each element. Must have size() equal to shape_in.size()
            pocketfft::stride_t stride_out{1}; // must have the size of each element. Must have size() equal to shape_in.size()
            stride_in[0]=sizeof ( std::complex<float> );
            stride_out[0]=sizeof ( float );

            bool forward{ pocketfft::BACKWARD };                                            // FORWARD or BACKWARD

            float fct{ 0.5f};    // scaling factor
            shape_in[0]=warp?data->length-1:data->length;
            pocketfft::shape_t axes;

            axes.push_back ( 0 );
            for (int i=0; i<data->length/2+1; i++ )
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
            for(int k=0;k<data->length;k++){
                data->sampleData[j][k].store((*waveform[j])[k]);
            }
            delete waveform[j];
            isSpectrumChanged=false;
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





    void onImGuiDisplay() override{

        ImGui::PushID(this);

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(getWidth(), getHeight()));
        int plotIndex=0;

        if(!data)return;
        if (ImGui::Begin("Waveform Analysis", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)){

            ImGui::SetNextItemWidth(150);
            if (ImGui::BeginCombo(" ", editorViewNames[currentView])) {

                for (int n = 0; n < ev_count; n++) {

                    if (ImGui::Selectable(editorViewNames[n], (currentView == n))) {
                        EditorViews temp = static_cast<EditorViews>(n);
                        if(currentView==ev_waveform){
                            if(temp!=currentView)calculateFFT();
                        }else {
                            if(temp==ev_waveform)calculateWaveform();
                        }
                        currentView=temp;
                    }

                    if (currentView == n) {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }
            ImGui::SameLine();
            if(ImGui::Checkbox("Mono", &isMono)){
                if(isMono)data->channels.store(1, std::memory_order_relaxed);
                else data->channels.store(2, std::memory_order_relaxed);
            }
            ImGui::SameLine();
            ImGui::Checkbox("Edit", &editMode);

            if(currentView!=ev_waveform&&editMode)
            {   //ImGui::SameLine();
                //ImGui::Checkbox("Warp", &warp);
                ImGui::SameLine();
                ImGui::Checkbox("Live update", &isLiveUpdate);

                if(!isLiveUpdate&&isSpectrumChanged)
                {
                    ImGui::SameLine();
                    if(ImGui::Button("Apply changes"))
                    {
                        calculateWaveform();
                    }
                }
            }



            for(int channel=0;channel<1||!isMono&&channel<2;channel++){
                ImPlot::SetCurrentContext(imPlotContext[plotIndex++]);

                if(currentView==ev_partialAmplitude){
                    if (ImPlot::BeginPlot(channel?"Spectrum R":"Spectrum L")){
                        setInputMap();
                        ImPlot::SetupAxis(ImAxis_Y1, "Amplitude", ImPlotAxisFlags_Lock);
                        ImPlot::SetupAxisLimits(ImAxis_Y1, -0.001, 1.0, ImPlotCond_Always);
                        ImPlot::SetupAxisScale(ImAxis_Y1, TransformForward_Sqrt, TransformInverse_Sqrt);

                        // Allow the X-axis to scroll and zoom normally
                        ImPlot::SetupAxis(ImAxis_X1, "Partial", ImPlotAxisFlags_None);
                        ImPlot::SetupAxisLimits(ImAxis_X1, -1.f, 200.f, ImPlotCond_Once);
                    }
                    ImPlot::PlotScatterG("Spectrum", SpectrumGetter, spectrum[channel], data->length/2, spec);
                    if (ImPlot::IsPlotHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&editMode) {
                        ImPlotPoint current_pos = ImPlot::GetPlotMousePos();
                        if(current_pos.x>=0&&current_pos.x<MAX_SAMPLE_LENGTH/2-1){
                            (*spectrum[channel])[current_pos.x]=std::polar(
                                current_pos.y>0?(float)current_pos.y:0.f,
                                (float)(
                                    std::abs((*spectrum[channel])[current_pos.x])?std::arg((*spectrum[channel])[current_pos.x]):-M_PI/2
                                )
                            );
                            isSpectrumChanged=true;
                            if(data->length<(int)current_pos.x*2+2){
                                data->length=(int)current_pos.x*2+2;
                                (*spectrum[channel])[current_pos.x+1]=std::complex(0.f,0.f);
                            }
                            if(isLiveUpdate)calculateWaveform();

                        }
                    }

                }else if(currentView==ev_waveform){
                    if(ImPlot::BeginPlot(channel?"Waveform R":"Waveform L")){
                        setInputMap();

                        ImPlot::SetupAxis(ImAxis_Y1, "Amplitude", ImPlotAxisFlags_Lock);
                        ImPlot::SetupAxisLimits(ImAxis_Y1, -1.0, 1.0, ImPlotCond_Always);

                        // Allow the X-axis to scroll and zoom normally
                        ImPlot::SetupAxis(ImAxis_X1, "Sample", ImPlotAxisFlags_None);
                        ImPlot::SetupAxisLimits(ImAxis_X1, -1.f, 200.f, ImPlotCond_Once);
                        PlotAudioContext plotAudioContext { data, channel };
                        ImPlot::PlotScatterG("Waveform", AtomicVectorGetter, &plotAudioContext, data->length, spec);
                        if (ImPlot::IsPlotHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&editMode) {
                            ImPlotPoint current_pos = ImPlot::GetPlotMousePos();
                            if(current_pos.x>=0&&current_pos.x<MAX_SAMPLE_LENGTH){
                                data->sampleData[channel][current_pos.x].store(current_pos.y);
                                if(data->length<(int)current_pos.x+1){
                                    data->length=(int)current_pos.x+1;
                                }
                            }
                        }
                        showPlayhead();

                    }
                }else{
                    if (ImPlot::BeginPlot(channel?"Phase R":"Phase L")){
                        setInputMap();
                        ImPlot::SetupAxis(ImAxis_Y1, "Phase", ImPlotAxisFlags_Lock);
                        ImPlot::SetupAxisLimits(ImAxis_Y1, -0.3,2*M_PI+0.15, ImPlotCond_Always);

                        // Allow the X-axis to scroll and zoom normally
                        ImPlot::SetupAxis(ImAxis_X1, "Partial", ImPlotAxisFlags_None);
                        ImPlot::SetupAxisLimits(ImAxis_X1, -1.f, 200.f, ImPlotCond_Once);
                    }
                    ImPlot::PlotScatterG("Phase", PhaseGetter, spectrum[channel], data->length/2, spec);
                    if (ImPlot::IsPlotHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&editMode) {
                        ImPlotPoint current_pos = ImPlot::GetPlotMousePos();
                        if(current_pos.x>=0&&current_pos.x<data->length/2&&std::abs((*spectrum[channel])[current_pos.x])){
                            (*spectrum[channel])[current_pos.x]=std::polar(std::abs((*spectrum[channel])[current_pos.x]), (float)(std::max(current_pos.y,0.d)-M_PI/2));
                            isSpectrumChanged=true;
                            if(isLiveUpdate)calculateWaveform();
                        }
                    }
                }
                ImPlot::EndPlot();
            }
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
