#ifndef EDITOR_HPP
#define EDITOR_HPP
#include "src/DistrhoDefines.h"
#include "SamplerDefines.hpp"
#include "AudioData.hpp"
#include "external/ImPlot.hpp"
START_NAMESPACE_DISTRHO

class Editor
{
    char name[MAX_FILE_PATH_LENGTH];
    Editor(char *_name){
        strcpy(name, _name);
    }
};

class SampleEditor : Editor
{
    AudioData *data;
    float xAxis[MAX_SAMPLE_LENGTH];
    SampleEditor(char *_name, AudioData *_data=NULL) : Editor(_name ){
        data=_data;
    }

    render(){
        if (ImGui::Begin(name, nullptr, ImGuiWindowFlags_NoResize))
        {
            for(int i=0;i< data->channels;i++){
                if (ImPlot::BeginPlot("My Plot")) {
                    ImPlot::PlotLine("My Line Plot", xAxis, data->sampleData[i], sizeof(data->sampleData[i]));
                    ImPlot::EndPlot();
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
