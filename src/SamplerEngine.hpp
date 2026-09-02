#ifndef SAMPLERENGINE_HPP
#define SAMPLERENGINE_HPP
#include "src/DistrhoDefines.h"
#include "SamplerDefines.hpp"

START_NAMESPACE_DISTRHO



// --------------------------------------------------------------------------------------------------------------------
class SamplePlaybackEngineMonophonic
{
    long playhead=0, releaseStage=0;
    int midiNote, velocity;
    bool playing=false, released=false;
    float getReleaseValue(float releaseCurve[MAX_SAMPLE_LENGTH], long sampleLength){
        return releaseCurve[(long)releaseStage*sampleLength];
    }
    void noteOn(int _midiNote, int velocity){
        midiNote=_midiNote;
        velocity=_velocity;
        playhead=0;
        playing=true;
        released=false;
    }
    void noteOff(){
        released=true;
        releaseStage=0;
    }

    float run(std::vector<float> sampleData, float releaseTime, float releaseCurve[MAX_SAMPLE_LENGTH]){

        if(!playing) return 0;
        float output=sampleData[playhead];
        if(++playhead>=sizeof(sampleData)){
            playing=false;
        }
        if(released){
            output*=getReleaseValue(releaseCurve, sizeof(sampleData));
            releaseStage+=1/(releaseTime*sizeof(sampleData);
            if(releaseStage>=1){
                playing=false;
            }
        }
        return output;

    }
};

class SamplePlaybackEnginePolyphonic
{
    SamplePlaybackEngineMonophonic *engines[MAX_POLY];
    SamplePlaybackEnginePolyphonic(){
        for(int i=0;i<MAX_POLY;i++){
            engines[i]=new SamplePlaybackEngineMonophonic();
        }
    }
    void noteOn(int midiNote, int velocity){
        for(int i=0;i<MAX_POLY;i++){
            if(!engines[i]->playing){
                engines[i]->noteOn(midiNote, velocity);
                return;
            }
        }
    }

    void noteOff(int midiNote){
        for(int i=0;i<MAX_POLY;i++){
            if(engines[i]->playing&&engines[i]->midiNote==midiNote){
                engines[i]->noteOff();
            }
        }
    }

    float run(std::vector<float> sampleData, float releaseTime, float releaseCurve[MAX_SAMPLE_LENGTH]){
        float output=0;
        for(int i=0;i<MAX_POLY;i++)
        {
            if(engines[i]->playing)
            {
                output+=engines[i]->run(sampleData,releaseTime,releaseCurve);
            }
        }
        return output;
    }


};

END_NAMESPACE_DISTRHO
#endif // SAMPLERENGINE_HPP
