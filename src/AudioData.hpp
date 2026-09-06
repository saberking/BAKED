#ifndef AUDIO_DATA_HPP
#define AUDIO_DATA_HPP
#include "src/DistrhoDefines.h"
#include "external/AudioFile.h"
#include <atomic>

START_NAMESPACE_DISTRHO
#define MAX_SAMPLE_LENGTH 960000
#define MAX_POLY 128

class AudioData
{
public:
    int channels = 1;
    const int maxChannels;
    std::vector<std::atomic<float>> sampleData[2];
    int length=0;
    AudioData(int _maxChannels):maxChannels(_maxChannels){
        for(int i=0;i<maxChannels;i++)
        {
            sampleData[i] = std::vector<std::atomic<float>>(MAX_SAMPLE_LENGTH);
            for(int j=0;j<MAX_SAMPLE_LENGTH;j++)
            {
                sampleData[i][j].store(0, std::memory_order_relaxed);
            }
        }
    }
    void loadWavFile ( const char *filePath)
    {
        AudioFile<float> audioFile;
        audioFile.load ( filePath );
        int audioFileChannels = audioFile.getNumChannels();
        length=audioFile.samples[0].size();
        float temp;
        for(int i=0;i<length&&i<MAX_SAMPLE_LENGTH;i++)
        {
            if(maxChannels==2){
                sampleData[0][i].store(audioFile.samples[0][i], std::memory_order_relaxed);
                sampleData[1][i].store(audioFile.samples[audioFileChannels>=2?1:0][i],std::memory_order_relaxed);
            }else{
                temp=audioFile.samples[0][i];
                if(audioFileChannels>=2){
                    temp=(temp+audioFile.samples[1][i])/2;
                }
                sampleData[0][i].store(temp, std::memory_order_relaxed);
            }

        }
        std::cout<<"length: "<<length<<"\n\n";

    }
    void makeMono(){
        channels=1;
    }
    void makeStereo(){
        // for(long i=0;i<length;i++)
        // {
        //     sampleData[1][i].store( sampleData[0][i].load(std::memory_order_relaxed),std::memory_order_relaxed);
        // }
        channels=2;
    }
};
struct Module;
struct SamplePlaybackEngineMonophonic {
    Module *module;
    float releasePlayhead=0;
    float playhead=0;
    bool playing=false;
    bool released=false;
    int midiNote, velocity;
    inline void timeStep();
    inline void stop();
    inline void noteOn(int _midiNote, int _velocity);
    inline void noteOff(int _midiNote);
    inline void run(float outputs[2]);
    SamplePlaybackEngineMonophonic(Module *_module);
    inline float getReleaseValue();
};
struct Module {
    AudioData *sample;
    std::vector<float*> levels;
    AudioData *releaseCurve;
    float *releaseSpeed;
    float *speed;
    bool *releaseEnabled;
    SamplePlaybackEngineMonophonic * playbackData[MAX_POLY];

    Module(std::vector<float *> _levels, float *_releaseSpeed, float *_speed, bool *_releaseEnabled):
        levels(_levels),
        releaseSpeed(_releaseSpeed),
        speed(_speed),
        releaseEnabled(_releaseEnabled)
    {
        sample=new AudioData(2);
        releaseCurve=new AudioData(1);
        for(int i=0;i<MAX_POLY;i++){
            playbackData[i]=new SamplePlaybackEngineMonophonic(this);
        }
    }
    ~Module(){
        delete(sample);
        delete(releaseCurve);
        for(int i=0;i<MAX_POLY;i++){
            delete(playbackData[i]);
        }
    }

    void noteOn(int midiNote, int velocity){
        for(int i=0;i<MAX_POLY;i++){
            if(!playbackData[i]->playing){
                playbackData[i]->noteOn(midiNote, velocity);
                return;
            }
        }
    }

    void noteOff(int midiNote){
        for(int i=0;i<MAX_POLY;i++){
            playbackData[i]->noteOff(midiNote);
        }
    }

    void run(float outputs[2]){
        outputs[0]=outputs[1]=0;
        float tempOuts[2];
        for(int i=0;i<MAX_POLY;i++){
            playbackData[i]->run(tempOuts);
            outputs[0]+=tempOuts[0];outputs[1]+=tempOuts[1];
        }
    }

};

inline SamplePlaybackEngineMonophonic::  SamplePlaybackEngineMonophonic(Module *_module):
    module(_module){}
inline float SamplePlaybackEngineMonophonic::getReleaseValue(){
    if(!playing) return 0;
    if(!released) return 1;
    return module->releaseCurve->sampleData[0][(int)releasePlayhead].load(std::memory_order_relaxed);
}
inline void SamplePlaybackEngineMonophonic::timeStep(){
    if(playing){
        playhead+=*(module->speed);
        // playhead&&std::cout<<"playhead"<<playhead<<std::endl;
        // playhead&&std::cout<<"length"<<module->sample->length<<std::endl;
        if(playhead>module->sample->length-1){
            stop(); return;
        }
        if(released){
            releasePlayhead+=*(module->releaseSpeed);
            if(releasePlayhead>module->releaseCurve->length-1){
                stop();
            }
        }

    }
}
inline void SamplePlaybackEngineMonophonic::stop(){
    playing=false;
    released=false;
    playhead=0;
    releasePlayhead=0;
}
inline void SamplePlaybackEngineMonophonic::noteOn(int _midiNote, int _velocity){
    playing=true;
    midiNote=_midiNote;
    velocity=_velocity;
}
inline void SamplePlaybackEngineMonophonic::noteOff(int _midiNote){
    if(midiNote==_midiNote&&*(module->releaseEnabled)){
        released=true;
    }
}
inline void SamplePlaybackEngineMonophonic::run(float outputs[2]){
    outputs[0]=outputs[1]=0;
    if(playhead>module->sample->length-1){
        stop();
        return;
    }
    for(int i=0;i<2;i++){
        outputs[i]=module->sample->sampleData[i][(int)playhead].load(std::memory_order_relaxed)*getReleaseValue();
        //int show=((int)playhead)%100-1;
        //!show&&std::cout<<outputs[i]<<std::endl;
    }
    timeStep();
}
END_NAMESPACE_DISTRHO

#endif // UTIL_HPP
