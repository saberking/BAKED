#ifndef UTIL_HPP
#define UTIL_HPP
#include "src/DistrhoDefines.h"
#include "external/AudioFile.h"

START_NAMESPACE_DISTRHO

class AudioData
{
    unsigned int channels = 1;
    std::vector<float> sampleData[2];
    AudioData(AudioData * copyFrom=NULL){
        if(copyFrom){
            channels=copyFrom->channels;
            sampleData[0]=copyFrom->sampleData[0];
            sampleData[1]=copyFrom->sampleData[1];
        }
    }
    void loadWavFile ( const char *filePath)
    {
        audioFile.load ( filePath );
        channels = audioFile.getNumChannels();
        sampleData[0] = audioFile.samples[0];
        if ( channels>=2 )
        {
            sampleData[1] = audioFile.samples[1];
        }

    }
    void makeMono(){
        channels=1;
    }
    void makeStereo(){
        if(channels==1){
            sampleData[1]=sampleData[0];
            channels=2;
        }
    }
};



END_NAMESPACE_DISTRHO

#endif // UTIL_HPP
