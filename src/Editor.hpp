#ifndef EDITOR_HPP
#define EDITOR_HPP
#include "src/DistrhoDefines.h"
#include "AudioData.hpp"
#include "external/implot.h"
#include "external/implot_internal.h"


START_NAMESPACE_DISTRHO
#define MAX_FILE_PATH_LENGTH 256



class SampleEditor
{
public:
    AudioData *data;
    char name[MAX_FILE_PATH_LENGTH];
    ImPlotSpec spec;


    SampleEditor(const char *_name, AudioData *_data=NULL) {
        strcpy(name, _name);
        data=_data;
        spec.Flags = ImPlotFlags_CanvasOnly;
    }

    // A custom callback function that ImPlot uses to grab values safely
    static ImPlotPoint AtomicVectorGetter(int idx, void* data_ptr) {
        auto* vec_ptr = static_cast<AudioData*>(data_ptr);
        float y_val=0;
        // 2. Safely read using the arrow operator -> directly into the vector array index
        // This tells the compiler: "Stay at this vector address, look 'idx' floats deep inside it"

        y_val = vec_ptr->sampleData[0][idx].load(std::memory_order_relaxed);
        return ImPlotPoint(idx, y_val);

        // Alternative cleaner syntax that does the exact same safe lookup:
        // float y_val = (*vec_ptr)[idx].load(std::memory_order_relaxed);

    }

    void render(){

        if(data)
        {
            if (ImPlot::BeginPlot("My Plot")) {
                ImPlot::SetupAxis(ImAxis_Y1, "Amplitude", ImPlotAxisFlags_Lock);
                ImPlot::SetupAxisLimits(ImAxis_Y1, -1.0, 1.0, ImPlotCond_Always);

                // Allow the X-axis to scroll and zoom normally
                ImPlot::SetupAxis(ImAxis_X1, "Samples", ImPlotAxisFlags_None);
                ImPlot::PlotScatterG("My Line", AtomicVectorGetter, data, data->length, spec);
                ImPlot::EndPlot();

            }

        }
    }
};


END_NAMESPACE_DISTRHO
#endif // EDITOR_HPP
