#ifndef AUDIO_DATA_HPP
#define AUDIO_DATA_HPP
#include "DistrhoPlugin.hpp"
#include "Defines.hpp"
#include "src/DistrhoDefines.h"

#include "external/dr_wav.h"
#include <atomic>
#include <vector>
#include <iostream>

START_NAMESPACE_DISTRHO
#define MAX_SAMPLE_LENGTH 960000
#define MAX_POLY 128

class AudioData
{
public:
    std::atomic<int> channels = 1;
    const int maxChannels;
    std::vector<std::atomic<float>> sampleData[2];
    std::atomic<int> length=0;
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

    void loadWavFile(const char* filePath)
    {
        if (filePath == nullptr) return;
        std::cout << "load " << filePath << std::endl;

        drwav wav;
        // Open the WAV file safely
        if (!drwav_init_file(&wav, filePath, nullptr)) {
            std::cout << "ERRRRRRRRRRRR (Failed to open file via dr_wav)" << std::endl;
            return;
        }
        std::cout << "loaded" << std::endl;

        // dr_wav reads totalPCMFrameCount (the sample length of a single channel)
        int audioFileChannels = wav.channels;
        int totalFrames = (int)wav.totalPCMFrameCount;
        std::cout << audioFileChannels << " channels" << std::endl;

        channels.store(std::max(1, std::min(2, audioFileChannels)), std::memory_order_relaxed);
        std::cout << "stored channels" << std::endl;

        // Bind buffer limit sizes safely
        int tempLength = std::min(totalFrames, MAX_SAMPLE_LENGTH);
        length.store(tempLength, std::memory_order_relaxed);

        // Allocate an intermediate heap buffer to hold the interleaved float data
        // Size is frames * channels because dr_wav reads channels consecutively
        size_t totalSamplesToRead = (size_t)tempLength * audioFileChannels;
        std::vector<float> pcmData(totalSamplesToRead);

        // Read the file data converted into native 32-bit floats automatically
        drwav_read_pcm_frames_f32(&wav, tempLength, pcmData.data());

        // Close the file handle immediately after reading into system memory
        drwav_uninit(&wav);

        float temp;
        for (int i = 0; i < tempLength; i++)
        {
            // Calculate the interleaved stride index positions
            int ch0Index = i * audioFileChannels;
            int ch1Index = (audioFileChannels >= 2) ? (ch0Index + 1) : ch0Index;

            if (maxChannels == 2) {
                sampleData[0][i].store(pcmData[ch0Index], std::memory_order_relaxed);
                sampleData[1][i].store(pcmData[ch1Index], std::memory_order_relaxed);
            } else {
                temp = pcmData[ch0Index]; // Used for loading sample as release curve
                if (audioFileChannels >= 2) {
                    temp = (temp + pcmData[ch1Index]) / 2.0f;
                }
                sampleData[0][i].store(temp, std::memory_order_relaxed);
            }
        }

        std::cout << "length: " << length.load(std::memory_order_relaxed) << "\n\n";
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioData)
};
struct Module;
struct SamplePlaybackEngineMonophonic {
    Module *module=NULL;
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
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplePlaybackEngineMonophonic)

};
struct Module {
    AudioData *sample=NULL;
    std::vector<float*> levels;
    AudioData *releaseCurve=NULL;
    float *releaseSpeed=NULL;
    float *speed=NULL;
    bool *releaseEnabled=NULL;
    char sampleFilePath[MAX_FILE_PATH_LENGTH];
    SamplePlaybackEngineMonophonic * playbackData[MAX_POLY];

    Module(std::vector<float *> _levels, float *_releaseSpeed, float *_speed, bool *_releaseEnabled):
        levels(_levels),
        releaseSpeed(_releaseSpeed),
        speed(_speed),
        releaseEnabled(_releaseEnabled)
    {
        strcpy(sampleFilePath, "Drop sample here...");
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
            if(playbackData[i]->playing)
            {
                playbackData[i]->run(tempOuts);
                outputs[0]+=tempOuts[0];outputs[1]+=tempOuts[1];
            }
        }
    }
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Module)

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
        if(playhead>module->sample->length.load(std::memory_order_relaxed)-1){
            stop(); return;
        }
        if(released){
            releasePlayhead+=*(module->releaseSpeed);
            if(releasePlayhead>module->releaseCurve->length.load(std::memory_order_relaxed)-1){
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
    if(playhead>module->sample->length.load(std::memory_order_relaxed)-1){
        stop();
        return;
    }
    outputs[0]=outputs[1]=module->sample->sampleData[0][(int)playhead].load(std::memory_order_relaxed)*getReleaseValue();
    if(module->sample->channels.load(std::memory_order_relaxed)==2){
        outputs[1]=module->sample->sampleData[1][(int)playhead].load(std::memory_order_relaxed)*getReleaseValue();
    }
    timeStep();
}
END_NAMESPACE_DISTRHO

#endif // UTIL_HPP
