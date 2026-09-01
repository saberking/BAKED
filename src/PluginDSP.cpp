/*
 * ImGui plugin example
 * Copyright (C) 2021 Jean Pierre Cimalando <jp-dev@inbox.ru>
 * Copyright (C) 2021-2025 Filipe Coelho <falktx@falktx.com>
 * SPDX-License-Identifier: ISC
 */

#include "DistrhoPlugin.hpp"
#include "Parameters.hpp"
#include "external/AudioFile.h"
#include "WinConsoleOutput.hpp"


START_NAMESPACE_DISTRHO

#define MAX_POLY 128
#define MAX_FILE_PATH_LENGTH 256

// --------------------------------------------------------------------------------------------------------------------
class SamplePlaybackEngine
{

};

class ImGuiPluginDSP : public Plugin
{
    float fRelease = 0.0f;
    char sampleFilePath[MAX_FILE_PATH_LENGTH];
    std::vector<float> sampleLeft, sampleRight;
    bool loaded = false;
    AudioFile<float> audioFile;
    long playhead[MAX_POLY], releasePoint[MAX_POLY], midiNote[MAX_POLY],velocity[MAX_POLY];

public:
   /**
      Plugin class constructor.@n
      You must set all parameter values to their defaults, matching ParameterRanges::def.
    */
    ImGuiPluginDSP()
        : Plugin(kParamCount, 0, 0) // parameters, programs, states
    {
        if (!GetConsoleWindow()) {
            initConsoleOutput();
        }

        strcpy(sampleFilePath, "Drop Audio File Here");
        for(int i=0;i<MAX_POLY;i++){
            playhead[i]=-1;//not playing
            releasePoint[i]=-1;//not released
        }
    }
    ~ImGuiPluginDSP(){
        FreeConsole();
    }

protected:
    // ----------------------------------------------------------------------------------------------------------------
    // Information

   /**
      Get the plugin label.@n
      This label is a short restricted name consisting of only _, a-z, A-Z and 0-9 characters.
    */
    const char* getLabel() const noexcept override
    {
        return "BAKED";
    }

   /**
      Get an extensive comment/description about the plugin.@n
      Optional, returns nothing by default.
    */
    const char* getDescription() const override
    {
        return "Sampler with precomputed effects";
    }

   /**
      Get the plugin author/maker.
    */
    const char* getMaker() const noexcept override
    {
        return "Jean Pierre Cimalando, falkTX, Saber";
    }

   /**
      Get the plugin license (a single line of text or a URL).@n
      For commercial plugins this should return some short copyright information.
    */
    const char* getLicense() const noexcept override
    {
        return "ISC";
    }

   /**
      Get the plugin version, in hexadecimal.
      @see d_version()
    */
    uint32_t getVersion() const noexcept override
    {
        return d_version(1, 0, 0);
    }

   /**
      Get the plugin unique Id.@n
      This value is used by LADSPA, DSSI and VST plugin formats.
      @see d_cconst()
    */
    int64_t getUniqueId() const noexcept override
    {
        return d_cconst('B', 'A', 'K', 'D');
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Init

   /**
      Initialize the parameter @a index.@n
      This function will be called once, shortly after the plugin is created.
    */
    void initParameter(uint32_t index, Parameter& parameter) override
    {
        DISTRHO_SAFE_ASSERT_RETURN(index == 0,);

        parameter.ranges.min = 0.f;
        parameter.ranges.max = 4000.f;
        parameter.ranges.def = 0.f;
        parameter.name = "Release";
        parameter.shortName = "Release";
        parameter.symbol = "release";
        parameter.unit = "ms";
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Internal data

   /**
      Get the current value of a parameter.@n
      The host may call this function from any context, including realtime processing.
    */
    float getParameterValue(uint32_t index) const override
    {
        if(index==kParamRelease){
            return fRelease;
        }
    }

   /**
      Change a parameter value.@n
      The host may call this function from any context, including realtime processing.@n
      When a parameter is marked as automatable, you must ensure no non-realtime operations are performed.
      @note This function will only be called for parameter inputs.
    */
    void setParameterValue(uint32_t index, float value) override
    {
        fRelease = value;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Audio/MIDI Processing

   /**
      Activate this plugin.
    */
    void activate() override
    {
    }

    String getState(const char* key) const override {

        return (String) sampleFilePath;
    }

    void setState(const char* key, const char* value) override {
        std::cout<<value<<"\n\n";
        std::cout << std::flush;
        loadWavFile(value);
    }

    void resetSampler(){
        for(int i=0;i<MAX_POLY;i++){
            playhead[i]=releasePoint[i]=-1;
        }
    }

    void loadWavFile ( const char *value )
    {
        if(strcmp(sampleFilePath, value)==0){
            return;
        }
        strcpy(sampleFilePath, value);
        audioFile.load ( value );
        int numChannels = audioFile.getNumChannels();
        std::cout<<numChannels;
        sampleLeft = audioFile.samples[0];
        if ( numChannels>=2 )
        {
            sampleRight = audioFile.samples[1];

        }
        else
        {
            sampleRight = audioFile.samples[0];
        }
        loaded=true;
        resetSampler();
    }

    void noteOn(int _midiNote, int _velocity){
        for(int i=0;i<MAX_POLY;i++){
            if(playhead[i]==-1){
                playhead[i]=0;//start playing
                midiNote[i]=_midiNote;
                velocity[i]=_velocity;
                return;
            }
        }
    }

    void noteOff(int _midiNote){
        for(int i=0;i<MAX_POLY;i++){
            if(playhead[i]!=-1&&midiNote[i]==_midiNote){
                releasePoint[i]=playhead[i];
            }
        }
    }

    float envelope(int voiceIndex){
        if(releasePoint[voiceIndex]==-1){
            return 1.f;
        }else if(fRelease==0){
            return -0.01f;
        }else{
            return 1 - (playhead[voiceIndex]-releasePoint[voiceIndex])/(fRelease*getSampleRate()/1000);
        }
    }

    void run ( const float **inputs, float **outputs, uint32_t frames,
             const MidiEvent *midiEvents, // MIDI pointer
             uint32_t midiEventCount      // Number of MIDI events in block
             ) override
    {

        float *const outL = outputs[0];
        float *const outR = outputs[1];



        int curEventIndex =0;
        for ( uint32_t i = 0; i < frames; i++ )
        {
            while ( curEventIndex < midiEventCount && i == midiEvents[curEventIndex].frame )
            {

                int status = midiEvents[curEventIndex].data[0]; // midi status
                int midi_message = status & 0xF0;
                int midi_data1 = midiEvents[curEventIndex].data[1];
                int midi_data2 = midiEvents[curEventIndex].data[2];
                //midiNote=midi_data1;

                switch ( midi_message )
                {
                case 0x80: // note_off
                    noteOff(midi_data1);
                    break;
                case 0x90: // note_on
                    noteOn(midi_data1, midi_data2);
                    break;
                default:
                    break;
                }
                curEventIndex++;

            }
            outL[i]=outR[i]=0.0f;
            if ( loaded )
            {
                for(int j=0;j<MAX_POLY;j++){
                    if(playhead[j]!=-1){
                        if(envelope(j)<0||playhead[j]>=sampleLeft.size()){
                            playhead[j]=-1;
                            releasePoint[j]=-1;
                        }else{
                            outL[i]+=sampleLeft[playhead[j]]*envelope(j);
                            outR[i]+=sampleRight[playhead[j]]*envelope(j);
                            playhead[j]++;
                        }
                    }
                }

            }


        }

    }


    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImGuiPluginDSP)
};

// --------------------------------------------------------------------------------------------------------------------

Plugin* createPlugin()
{
    return new ImGuiPluginDSP();
}

// --------------------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
