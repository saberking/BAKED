#ifndef EDITOR_HPP
#define EDITOR_HPP
#include "src/DistrhoDefines.h"
#include "DistrhoUI.hpp"

#include "AudioData.hpp"
#include "external/implot.h"
#include "external/implot_internal.h"
#include "external/pocketfft_hdronly.h"


START_NAMESPACE_DISTRHO


class SampleEditor : public DGL::ImGuiStandaloneWindow
{
public:
    AudioData *data=NULL;
    Module *module=NULL;
    bool isRelease=false;
    char name[MAX_FILE_PATH_LENGTH];
    ImPlotSpec spec;
    bool editMode=false;
    bool isSpectrum=false;
    ImPlotContext* imPlotContext = nullptr;
    std::vector<std::complex<float>> *spectrum ;
    bool spectrumReady=false;


    SampleEditor(const char *_name, Module *_module, Window& window):
        DGL::ImGuiStandaloneWindow(window.getApp(), window) {
        strcpy(name, _name);
        module=_module;
        if(!isRelease)data=module->sample;
        spec.Flags = ImPlotFlags_CanvasOnly;
        setResizable(true);
        imPlotContext=ImPlot::CreateContext();
        spectrum = new std::vector<std::complex<float>>  ( MAX_SAMPLE_LENGTH );
    }

    static ImPlotPoint AtomicVectorGetter(int idx, void* data_ptr) {
        auto* vec_ptr = static_cast<AudioData*>(data_ptr);
        float y_val = vec_ptr->sampleData[0][idx].load(std::memory_order_relaxed);
        return ImPlotPoint(idx, y_val);
    }

    static ImPlotPoint SpectrumGetter(int idx, void* data_ptr){
        auto* vec_ptr = static_cast<std::vector<std::complex<float>>*>(data_ptr);
        float y_val = std::abs((*vec_ptr)[idx]);
        return ImPlotPoint(idx, y_val);
    }

    void calculateFFT(){
        std::vector<std::complex<float>> data_in ( data->length );
        pocketfft::shape_t shape_in{1};                                              // dimensions of the input shape
        pocketfft::stride_t stride_in{1};                    // must have the size of each element. Must have size() equal to shape_in.size()
        pocketfft::stride_t stride_out{1}; // must have the size of each element. Must have size() equal to shape_in.size()
        stride_in[0]=stride_out[0]=sizeof ( std::complex<float> );
        //  pocketfft::shape_t axes{1};                                                  // 0 to shape.size()-1 inclusive
        bool forward{ pocketfft::FORWARD };                                            // FORWARD or BACKWARD
        // input data (reals)
        // output data (FFT(input))
        float fct{ 1.0f /data->length};    // scaling factor
        shape_in[0]=data->length;
        pocketfft::shape_t axes;

        axes.push_back ( 0 );
        int i;
        for ( i=0; i<data->length; i++ )
        {
            data_in[i]=std::complex<float> ( data->sampleData[0][i].load(std::memory_order_relaxed),0.f );
        }
        // while(i<MAX_SAMPLE_LENGTH){
        //     data_in[i++]=std::polar<float> ( 0.f, 0.f );
        // }


        pocketfft::c2c (
            shape_in,
            stride_in,
            stride_out,
            axes,
            forward,
            data_in.data(),
            spectrum->data(),
            fct
            );
        spectrumReady=true;
    }


    void onImGuiDisplay() override{

        ImPlot::SetCurrentContext(imPlotContext);
        ImGui::PushID(this);

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(getWidth(), getHeight()));



        if (ImGui::Begin("Waveform Analysis", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)){
            if (data&&ImPlot::BeginPlot(isSpectrum?"Spectrum":"Waveform")) {
                ImPlotInputMap& input_map = ImPlot::GetInputMap();
                if (editMode)
                {
                    input_map.Pan = ImGuiMouseButton_Right;

                    input_map.SelectMod = ImGuiMod_Ctrl ;//zoom
                }
                else
                {
                    input_map.Pan = ImGuiMouseButton_Left;

                    input_map.SelectMod = ImGuiMod_None;//zoom
                }

                ImPlot::SetupAxis(ImAxis_Y1, "Amplitude", ImPlotAxisFlags_Lock);
                ImPlot::SetupAxisLimits(ImAxis_Y1, -1.0, 1.0, ImPlotCond_Always);

                // Allow the X-axis to scroll and zoom normally
                ImPlot::SetupAxis(ImAxis_X1, "Samples", ImPlotAxisFlags_None);
                    ImPlot::SetupAxisLimits(ImAxis_X1, -1.f, 200.f, ImPlotCond_Once);
                if(isSpectrum&&spectrumReady){
                    ImPlot::PlotScatterG("Spectrum###DataSlot", SpectrumGetter, spectrum, data->length/2, spec);

                }else{
                    ImPlot::PlotScatterG("My Line###DataSlot", AtomicVectorGetter, data, data->length, spec);

                }

                if (ImPlot::IsPlotHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&editMode) {
                    ImPlotPoint current_pos = ImPlot::GetPlotMousePos();
                    if(current_pos.x>=0&&current_pos.x<MAX_SAMPLE_LENGTH){
                        data->sampleData[0][current_pos.x]=current_pos.y;
                        if(data->length<(int)current_pos.x+1){
                            data->length=(int)current_pos.x+1;
                        }
                    }
                }

                for(int i=0;i<MAX_POLY;i++){
                    if(module->playbackData[i]->playing){
                        double playhead = module->playbackData[i]->playhead;
                        ImPlot::DragLineX(0, &playhead, ImVec4(0.5,0.5,0.5,0.5), 0.5f, ImPlotDragToolFlags_NoInputs);
                    }
                }
                ImPlot::EndPlot();

            }
            ImGui::Checkbox("Edit", &editMode);
            if(ImGui::Checkbox("Show Spectrum", &isSpectrum)&&isSpectrum){
                calculateFFT();
            }
        }

        ImGui::End();
        ImGui::PopID();
    }
    ~SampleEditor(){
        ImPlot::DestroyContext(imPlotContext);
        delete spectrum;
    }
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleEditor)

};


END_NAMESPACE_DISTRHO
#endif // EDITOR_HPP
