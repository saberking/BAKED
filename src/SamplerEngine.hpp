#ifndef SAMPLERENGINE_HPP
#define SAMPLERENGINE_HPP
#include "src/DistrhoDefines.h"
#include "AudioData.hpp"

START_NAMESPACE_DISTRHO



// --------------------------------------------------------------------------------------------------------------------
class SamplePlaybackEngineMonophonic
{
    long playhead=0, releaseStage=0;
    int midiNote, velocity;
    bool playing=false, released=false;
    float getReleaseValue(float releaseCurve[MAX_SAMPLE_LENGTH]){
        return releaseCurve[(long)releaseStage*MAX_SAMPLE_LENGTH];
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

    void run(AudioData *sample, float releaseTime, AudioData *releaseCurve, float outputs[2]){

        if(!playing){
            outputs[0]=outputs[1]=0;
            return;
        }
        output[0]=sample->sampleData[0][playhead].load(std::memory_order_relaxed);
        output[1]=sample->sampleData[1][playhead].load(std::memory_order_relaxed);
        if(++playhead>=sample->length){
            playing=false;
        }
        if(released){
            float releaseValue=getReleaseValue(releaseCurve);
            output[0]*=releaseValue;output[1]*=releaseValue;
            releaseStage+=1/(releaseTime*MAX_SAMPLE_LENGTH);
            if(releaseStage>=1){
                playing=false;
            }
        }

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

    float run(AudioData *sample, float releaseTime, AudioData *releaseCurve, float outputs[2]){
        float output=0;
        for(int i=0;i<MAX_POLY;i++)
        {
            if(engines[i]->playing)
            {
                output+=engines[i]->run(sampleData,releaseTime,releaseCurve, outputs);
            }
        }
        return output;
    }


};

END_NAMESPACE_DISTRHO
#endif // SAMPLERENGINE_HPP
