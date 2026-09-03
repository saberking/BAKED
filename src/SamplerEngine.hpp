#ifndef SAMPLERENGINE_HPP
#define SAMPLERENGINE_HPP
#include "src/DistrhoDefines.h"
#include "AudioData.hpp"

START_NAMESPACE_DISTRHO

#define MAX_POLY 128


// --------------------------------------------------------------------------------------------------------------------
class SamplePlaybackEngineMonophonic
{
public:
    long playhead=0, releaseStage=0;
    int midiNote, velocity;
    bool playing=false, released=false;
    float getReleaseValue(AudioData *releaseCurve){
        return releaseCurve->sampleData[0][(long)releaseStage*MAX_SAMPLE_LENGTH].load(std::memory_order_relaxed);
    }
    void noteOn(int _midiNote, int _velocity){
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
        outputs[0]=sample->sampleData[0][playhead].load(std::memory_order_relaxed);
        outputs[1]=sample->sampleData[1][playhead].load(std::memory_order_relaxed);
        //std::cout<<"foo"<<"\n\n"<<outputs<<"\n\n";
        if(++playhead>=sample->length){
            playing=false;
        }
        if(released){
            float releaseValue=getReleaseValue(releaseCurve);
            outputs[0]*=releaseValue;outputs[1]*=releaseValue;
            releaseStage+=1/(releaseTime*MAX_SAMPLE_LENGTH);
            if(releaseStage>=1){
                playing=false;
            }
        }

    }
};

class SamplePlaybackEnginePolyphonic
{
public:
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

    void run(AudioData *sample, float releaseTime, AudioData *releaseCurve, float outputs[2]){
        outputs[0]=outputs[1]=0;
        float tempOutputs[2];
        for(int i=0;i<MAX_POLY;i++)
        {
            engines[i]->run(sample,releaseTime,releaseCurve, tempOutputs);
            outputs[0]+=tempOutputs[0];outputs[1]+=tempOutputs[1];
        }
    }


};

END_NAMESPACE_DISTRHO
#endif // SAMPLERENGINE_HPP
