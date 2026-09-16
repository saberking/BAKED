#ifndef PLUGINDSP_HPP
#define PLUGINDSP_HPP

#include "DistrhoPlugin.hpp"
#include "Parameters.hpp"
#include "WinConsoleOutput.hpp"
#include "AudioData.hpp"
#include "external/base64.h"

START_NAMESPACE_DISTRHO

class ImGuiPluginDSP : public Plugin
{
    float fSpeed = 1.0f;
    bool fReleaseEnabled=false;
    bool consoleAttached=false;
public:
    std::vector<Module *> modules;//pointless to have more than one.
                                // polyphonic effects should be baked in and monophonic can be separate plugins
    /**
      Plugin class constructor.@n
      You must set all parameter values to their defaults, matching ParameterRanges::def.
    */
    ImGuiPluginDSP()
        : Plugin(kParamCount, 0, 1) // parameters, programs, states
    {
        if (!GetConsoleWindow()) {
            initConsoleOutput();
            consoleAttached=true;
        }
        std::vector<float *>levels;
        levels.push_back(&fSpeed);
        modules.push_back(new Module(levels, &fSpeed, &fSpeed, &fReleaseEnabled));

    }
    ~ImGuiPluginDSP(){
        for(int i=0;i<modules.size();i++){
            delete(modules[i]);
        }
    }

protected:


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
        parameter.name = "Speed";
        parameter.shortName = "Speed";
        parameter.symbol = "speed";
        parameter.unit = "";
        parameter.hints=kParameterIsAutomatable;

    }

    float getParameterValue(uint32_t index) const override
    {
        if(index==kParamSpeed){
            return fSpeed;
        }
    }


    void setParameterValue(uint32_t index, float value) override
    {
        fSpeed = value;
    }

    void activate() override
    {
    }

    void initState(uint32_t index, String& key, String& defaultValue) override
    {
        if (index == 0) {
            key = "sampleData";
            defaultValue = "";
        }
    }

    String getState(const char* key) const override {

        uint32_t length = static_cast<uint32_t>(modules[0]->sample->length.load(std::memory_order_relaxed));
        if (length == 0) return String("");

        // 1. Pack your sizes and channels sequentially into a simple local raw byte array
        size_t headerSize = sizeof(uint32_t);
        size_t channelDataSize = MAX_SAMPLE_LENGTH * sizeof(float);
        size_t totalBytes = headerSize + (channelDataSize * 2);

        std::vector<uint8_t> rawBinaryBuffer(totalBytes);

        // Copy length header
        std::memcpy(rawBinaryBuffer.data(), &length, headerSize);

        float* leftDest = reinterpret_cast<float*>(rawBinaryBuffer.data() + headerSize);
        for (uint32_t i = 0; i < MAX_SAMPLE_LENGTH; ++i) {
            leftDest[i] = modules[0]->sample->sampleData[0][i].load(std::memory_order_relaxed);
        }

        float* rightDest = reinterpret_cast<float*>(rawBinaryBuffer.data() + headerSize + channelDataSize);
        for (uint32_t i = 0; i < MAX_SAMPLE_LENGTH; ++i) {
            rightDest[i] = modules[0]->sample->sampleData[1][i].load(std::memory_order_relaxed);
        }

        // 3. Convert to base64 text string safely
        std::string encodedText = base64_encode(rawBinaryBuffer.data(), rawBinaryBuffer.size());
        return String(encodedText.c_str());

    }

    void setState(const char *key, const char * value){
        if (strlen(value) == 0 ) {
            return;
        }

        std::string decodedBytes = base64_decode(std::string(value));

        const uint8_t* rawData = reinterpret_cast<const uint8_t*>(decodedBytes.data());

        // 2. Read the sample length header out of the first 4 bytes
        uint32_t length;
        std::memcpy(&length, rawData, sizeof(uint32_t));


        modules[0]->sample->length.store(length, std::memory_order_relaxed);

        // 4. Extract data directly out of the remaining decoded data stream
        size_t headerSize = sizeof(uint32_t);
        size_t channelDataSize = MAX_SAMPLE_LENGTH * sizeof(float);


        // Create temporary pointers pointing to the raw decoded byte stream
        const float* leftSrc = reinterpret_cast<const float*>(rawData + headerSize);
        const float* rightSrc = reinterpret_cast<const float*>(rawData + headerSize + channelDataSize);

        // Safely write the standard floats back into your std::atomic<float> vectors
        for (uint32_t i = 0; i < MAX_SAMPLE_LENGTH; ++i) {
            modules[0]->sample->sampleData[0][i].store(leftSrc[i], std::memory_order_relaxed);
        }
        for (uint32_t i = 0; i < MAX_SAMPLE_LENGTH; ++i) {
            modules[0]->sample->sampleData[1][i].store(rightSrc[i], std::memory_order_relaxed);
        }


    }

    void noteOn(int midiNote, int velocity){
        for(int i=0;i<modules.size();i++){
            modules[i]->noteOn(midiNote,velocity);
        }
    }

    void noteOff(int midiNote){
        for(int i=0;i<modules.size();i++){
            modules[i]->noteOff(midiNote);
        }
    }

    void handleMidi(const MidiEvent *midiEvent){
        int status = midiEvent->data[0]; // midi status
        int midi_message = status & 0xF0;
        int midi_data1 = midiEvent->data[1];
        int midi_data2 = midiEvent->data[2];

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

                handleMidi(&(midiEvents[curEventIndex++]));

            }
            float tempOut[2];
            outputs[0][i]=outputs[1][i]=0;
            for(int j=0;j<modules.size();j++){
                modules[j]->run(tempOut);
                outputs[0][i]+=tempOut[0];outputs[1][i]+=tempOut[1];
            }

        }

    }

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
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImGuiPluginDSP)
};

END_NAMESPACE_DISTRHO

#endif // PLUGINUI_HPP
