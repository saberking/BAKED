#ifndef EDITOR_HPP
#define EDITOR_HPP
#include "src/DistrhoDefines.h"
#include "AudioData.hpp"
#include "external/implot.h"
#include "external/implot_internal.h"


START_NAMESPACE_DISTRHO
#define MAX_FILE_PATH_LENGTH 256

class Editor
{
public:
    char name[MAX_FILE_PATH_LENGTH];
    Editor(char *_name){
        strcpy(name, _name);
    }
};

class SampleEditor : Editor
{
public:
    AudioData *data;
    float xAxis[MAX_SAMPLE_LENGTH];
    SampleEditor(char *_name, AudioData *_data=NULL) : Editor(_name ){
        data=_data;
    }

    // A custom callback function that ImPlot uses to grab values safely
    static ImPlotPoint AtomicVectorGetter(int idx, void* data_ptr) {
        // Cast the void pointer back to our atomic vector reference
        auto& vec = *static_cast<std::vector<std::atomic<float>>*>(data_ptr);

        // Safely read the atomic float value and return it as an ImPlot coordinate
        float y_val = vec[idx].load(std::memory_order_relaxed);
        return ImPlotPoint(idx, y_val); // x = idx, y = y_val
    }

    void render(){
        if (ImGui::Begin(name, nullptr, ImGuiWindowFlags_NoResize))
        {
            if(data)
            {
                for(int i=0;i< data->channels;i++){
                    if (ImPlot::BeginPlot("My Plot")) {
                        ImPlot::PlotLineG("My Line Plot", AtomicVectorGetter, &data->sampleData[i], data->sampleData[i].size());
                        ImPlot::EndPlot();
                    }
                }
            }


        }
        ImGui::End();
    }
};

class EnvelopeEditor : Editor
{
};

END_NAMESPACE_DISTRHO
#endif // EDITOR_HPP
