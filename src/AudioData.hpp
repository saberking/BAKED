#ifndef UTIL_HPP
#define UTIL_HPP
#include "src/DistrhoDefines.h"
#include "external/AudioFile.h"

START_NAMESPACE_DISTRHO
#define MAX_SAMPLE_LENGTH 960000

class AudioData
{
    unsigned int channels = 1;
    std::vector<std::atomic<float>> sampleData[2][MAX_SAMPLE_LENGTH];
    unsigned long length=0;
    AudioData(){
        for(int i=0;i<2;i++)
        {
            for(int j=0;j<MAX_SAMPLE_LENGTH;j++)
            {
                sampleData[i][j].store(0, std::memory_order_relaxed)
            }
        }
    }
    void loadWavFile ( const char *filePath)
    {
        AudioFile<float> audioFile;
        audioFile.load ( filePath );
        channels = audioFile.getNumChannels();
        for(long i=0;i<sizeof(audioFile.samples[0]);i++)
        {
            sampleData[0][i].store(audioFile.samples[0][i], std::memory_order_relaxed);
            sampleData[1][i].store(audioFile.samples[channels>=2?1:0][i],std::memory_order_relaxed);
        }
        length=sizeof(audioFile.samples[0]);

        sampleLoaded=true;

    }
    void makeMono(){
        channels=1;
    }
    void makeStereo(){
        for(long i=0;i<length;i++)
        {
            sampleData[1][i].store( sampleData[0][i].load(std::memory_order_relaxed),std::memory_order_relaxed);
        }
        channels=2;
    }
};



END_NAMESPACE_DISTRHO

#endif // UTIL_HPP
