#ifndef PLUGINDSP_HPP
#define PLUGINDSP_HPP

#include "DistrhoPlugin.hpp"
#include "Parameters.hpp"
#include "WinConsoleOutput.hpp"
#include "AudioData.hpp"
#include "SamplerEngine.hpp"
START_NAMESPACE_DISTRHO

    class ImGuiPluginDSP : public Plugin
{
    float fRelease = 0.0f;


public:
    AudioData *sample;
    AudioData *releaseCurve;
    SamplePlaybackEnginePolyphonic *engine;

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

        engine=new SamplePlaybackEnginePolyphonic();
        sample= new AudioData();
        releaseCurve=new AudioData();

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
        parameter.ranges.max = 1.f;
        parameter.ranges.def = 1.f;
        parameter.name = "Release";
        parameter.shortName = "Release";
        parameter.symbol = "release";
        parameter.unit = "";
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

        return (String) "";
    }

    void setState(const char *key, const char * value){
        if(strcmp(key, "loadWavFile")==0)
        {
            sample->loadWavFile(value);
        }
    }




    void run ( const float **inputs, float **outputs, uint32_t frames,
             const MidiEvent *midiEvents, // MIDI pointer
             uint32_t midiEventCount      // Number of MIDI events in block
             ) override
    {





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
                    engine->noteOff(midi_data1);
                    break;
                case 0x90: // note_on
                    engine->noteOn(midi_data1, midi_data2);
                    break;
                default:
                    break;
                }
                curEventIndex++;

            }
            float tempOut[2];
            engine->run(sample, fRelease, releaseCurve, tempOut);
            outputs[0][i]=tempOut[0];outputs[1][i]=tempOut[1];



        }

    }


    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImGuiPluginDSP)
};

END_NAMESPACE_DISTRHO

#endif // PLUGINUI_HPP
