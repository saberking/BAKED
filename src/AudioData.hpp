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
#define ENVELOPE_LENGTH 200

enum InterpolationMode{
    interpModeNone,
    interpModeLinear
};

enum SpeakerConnections{
    speakerLL,
    speakerLR,
    speakerRL,
    speakerRR
};
inline float interpolate(float lower, float upper, float position, InterpolationMode mode)
{
    if(mode==interpModeNone)
    {
        return lower;
    }

    float remainder=position-(int)position;
    return lower*(1-remainder)+upper*remainder;
}

class ProcessedData
{
public:
    const int maxChannels;
    std::vector<float> sampleData[2];
    int length=1;
    ProcessedData(int _maxChannels):maxChannels(_maxChannels){
        for(int i=0;i<maxChannels;i++)
        {
            sampleData[i] = std::vector<float>(MAX_SAMPLE_LENGTH);
            for(int j=0;j<MAX_SAMPLE_LENGTH;j++)
            {
                sampleData[i][j]=0.f;
            }
        }
    }
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProcessedData)

};

class AudioData
{
public:
    // std::atomic<int> channels = 1;
    const int maxChannels;
    std::vector<std::atomic<float>> sampleData[2];
    std::atomic<int> length=1;
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


    int loadWavFile(const char* filePath)
    {
        if (filePath == nullptr) return 0;
        std::cout << "load " << filePath << std::endl;

        drwav wav;
        // Open the WAV file safely
        if (!drwav_init_file(&wav, filePath, nullptr)) {
            std::cout << "ERRRRRRRRRRRR (Failed to open file via dr_wav)" << std::endl;
            return 0;
        }
        std::cout << "loaded" << std::endl;

        // dr_wav reads totalPCMFrameCount (the sample length of a single channel)
        int audioFileChannels = wav.channels;
        int totalFrames = (int)wav.totalPCMFrameCount;
        std::cout << audioFileChannels << " channels" << std::endl;

        int noOfChannels=std::max(1, std::min(2, audioFileChannels));
        // channels.store(noOfChannels, std::memory_order_relaxed);

        // std::cout << "stored channels" << std::endl;

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
        return audioFileChannels;
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
    int midiNote;
    float velocity;
    float noteSpeed;
    inline void timeStep(float releaseTimestep, int pIndex);
    inline void stop();
    inline void noteOn(int _midiNote, float _velocity);
    inline void noteOff(int _midiNote);
    inline void run(float outputs[2], float releaseTimestep, int pIndex, InterpolationMode interpolationMode);
    SamplePlaybackEngineMonophonic(Module *_module);
    inline float getReleaseValue();
    // inline float getEnvelopeValue(float recipLength);
    inline float getVelocityValue();

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplePlaybackEngineMonophonic)

};
struct Module {
    AudioData *sample=NULL;
    ProcessedData *processed[2];
    std::atomic<int> processedIndex=0;
    std::vector<float*> levels;
    std::vector<std::atomic<float>> &releaseCurve;
    float *releaseLength=NULL;
    float *speed=NULL;
    std::atomic<bool> *releaseEnabled=NULL;
    float *noteSensitivity=NULL;
    float *velocitySensitivity=NULL;
    std::atomic<InterpolationMode> *interpolationMode=NULL;
    char sampleFilePath[MAX_FILE_PATH_LENGTH];
    std::vector<std::atomic<float>> envelope;
    bool isEnvelopeChanged=false;
    std::atomic<SpeakerConnections> speakerConnections=speakerLL;
    std::atomic<bool> shouldProcess=false;

    SamplePlaybackEngineMonophonic * playbackData[MAX_POLY];

    Module(
        std::vector<float *> _levels,
        float *_releaseLength,
        float *_speed,
        std::atomic<bool> *_releaseEnabled,
        float *_velocitySensitivity,
        float *_noteSensitivity,
        std::atomic<InterpolationMode> *_interpolationMode,
        std::vector<std::atomic<float>>&_releaseCurve):
        levels(_levels),
        releaseLength(_releaseLength),
        speed(_speed),
        releaseEnabled(_releaseEnabled),
        velocitySensitivity(_velocitySensitivity),
        noteSensitivity(_noteSensitivity),
        interpolationMode(_interpolationMode),
        releaseCurve(_releaseCurve)
    {
        strcpy(sampleFilePath, "Drop sample here...");
        sample=new AudioData(2);
        processed[0]=new ProcessedData(2);
        processed[1]=new ProcessedData(2);
        for(int i=0;i<MAX_POLY;i++){
            playbackData[i]=new SamplePlaybackEngineMonophonic(this);
        }
        envelope = std::vector<std::atomic<float>>(ENVELOPE_LENGTH);
        for(int i=0;i<ENVELOPE_LENGTH;i++)
        {
            envelope[i].store(1.f, std::memory_order_relaxed);
        }
    }
    ~Module(){
        delete(sample);
        delete processed[0];
        delete processed[1];
        for(int i=0;i<MAX_POLY;i++){
            delete(playbackData[i]);
        }
    }

    void noteOn(int midiNote, float velocity){
        for(int i=0;i<MAX_POLY;i++){
            if(!playbackData[i]->playing){
                playbackData[i]->noteOn(midiNote,velocity);
                return;
            }
        }
    }

    void noteOff(int midiNote){
        if(releaseEnabled->load(std::memory_order_relaxed))
            for(int i=0;i<MAX_POLY;i++){
                playbackData[i]->noteOff(midiNote);
            }
    }

    void process()
    {
        //shouldProcess.store(true,std::memory_order_relaxed);
        applyProcess();
    }

    void applyProcess()
    {
        int index=1-processedIndex.load(std::memory_order_acquire);
        for(int i=0;i<sample->length.load(std::memory_order_relaxed);i++)
        {
            float envelopeIndex=(float)i*(float)ENVELOPE_LENGTH/sample->length.load(std::memory_order_relaxed);
            int lower=(int)envelopeIndex;
            int upper=std::min(ENVELOPE_LENGTH-1,lower+1);
            float envelopeValue=interpolate(envelope[lower],envelope[upper],envelopeIndex, interpModeLinear);

            for(int j=0;j<2;j++)
            {
                processed[index]->sampleData[j][i]=sample->sampleData[j][i].load(std::memory_order_relaxed)*envelopeValue;

            }
            processed[index]->length=sample->length.load(std::memory_order_relaxed);
        }
        isEnvelopeChanged=false;
        //shouldProcess.store(false,std::memory_order_relaxed);
        processedIndex.store(index,std::memory_order_release);
    }

    void run(float outputs[2], int pIndex){
        float recipLength=((float)ENVELOPE_LENGTH)/((float)std::max(1,sample->length.load(std::memory_order_relaxed)));

        float releaseTimestep=recipLength/ *releaseLength;

        outputs[0]=outputs[1]=0;
        float tempOuts[2], tempOutsSum[]={0,0};
        for(int i=0;i<MAX_POLY;i++){
            if(playbackData[i]->playing)
            {
                playbackData[i]->run(tempOuts, releaseTimestep, pIndex, interpolationMode->load(std::memory_order_relaxed));
                tempOutsSum[0]+=tempOuts[0];tempOutsSum[1]+=tempOuts[1];
            }
        }
        SpeakerConnections connections=speakerConnections.load(std::memory_order_relaxed);
        if(connections==speakerLL||connections==speakerLR)outputs[0]=tempOutsSum[0];
        else outputs[0]=tempOutsSum[1];
        if(connections==speakerLR||connections==speakerRR)outputs[1]=tempOutsSum[1];
        else outputs[1]=tempOutsSum[0];
    }
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Module)

};

inline SamplePlaybackEngineMonophonic::  SamplePlaybackEngineMonophonic(Module *_module):
    module(_module){}
inline float SamplePlaybackEngineMonophonic::getReleaseValue(){
    if(!playing) return 0;
    if(!released) return 1;
    return module->releaseCurve[(int)releasePlayhead].load(std::memory_order_relaxed);
}
inline float SamplePlaybackEngineMonophonic::getVelocityValue(){
    return  1-*(module->velocitySensitivity)+*(module->velocitySensitivity)*velocity;
}
// inline float SamplePlaybackEngineMonophonic::getEnvelopeValue(float recipLength){
//     if(!playing.load(std::memory_order_relaxed)) return 0;
//     return module->envelope[(int)(playhead*recipLength)].load(std::memory_order_relaxed);
// }
inline void SamplePlaybackEngineMonophonic::timeStep(float releaseTimestep, int pIndex){
    if(playing){
        float newPlayhead=playhead+*(module->speed)*noteSpeed;
        if(newPlayhead>module->processed[pIndex]->length-1){
            stop(); return;
        }
        playhead=newPlayhead;

        if(released){
            float newReleasePlayhead=releasePlayhead+releaseTimestep;
            if(newReleasePlayhead>module->releaseCurve.size()-1){
                stop(); return;
            }
            releasePlayhead=newReleasePlayhead;

        }

    }
}
inline void SamplePlaybackEngineMonophonic::stop(){
    released=false;
    playhead=0;
    releasePlayhead=0;
    playing=false;

}
inline void SamplePlaybackEngineMonophonic::noteOn(int _midiNote, float _velocity){
    midiNote=_midiNote;
    velocity=_velocity;

    noteSpeed=pow(2,(_midiNote-60)**(module->noteSensitivity)/12);
    released=false;
    playing=true;

}
inline void SamplePlaybackEngineMonophonic::noteOff(int _midiNote){
    if(midiNote==_midiNote){
        released=true;
    }
}
inline void SamplePlaybackEngineMonophonic::run(float outputs[2],float releaseTimestep, int pIndex, InterpolationMode interpolationMode){
    outputs[0]=outputs[1]=0;
    if(playhead>module->processed[pIndex]->length-1){
        stop();
        return;
    }
    float volume=getVelocityValue()*getReleaseValue();
    int lower=(int)playhead;
    int upper=std::min(MAX_SAMPLE_LENGTH-1,lower+1);
    float interpolated=interpolate(
        module->processed[pIndex]->sampleData[0][lower],
        module->processed[pIndex]->sampleData[0][upper],
        playhead,
        interpolationMode
        );
    outputs[0]=interpolated*volume;

    interpolated=interpolate(
        module->processed[pIndex]->sampleData[1][lower],
        module->processed[pIndex]->sampleData[1][upper],
        playhead,
        interpolationMode
        );
    outputs[1]=interpolated*volume;

    timeStep(releaseTimestep,pIndex);
}
END_NAMESPACE_DISTRHO

#endif // UTIL_HPP
