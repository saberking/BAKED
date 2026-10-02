#ifndef PLUGINDSP_HPP
#define PLUGINDSP_HPP

#include "DistrhoPlugin.hpp"
#include "Parameters.hpp"
#include "WinConsoleOutput.hpp"
#include "AudioData.hpp"
#include "external/base64.h"
#include "Undo.hpp"

START_NAMESPACE_DISTRHO

class ImGuiPluginDSP : public Plugin
{
    float fSpeed = 1.0f;
    float fVelocitySensitivity=1.f;
    float fNoteSensitivity=1.f;
    bool consoleAttached=false;
public:
    std::atomic<bool> isReleaseEnabled=true;

    std::atomic<InterpolationMode> interpolationMode;

    UndoItem *undoItems[MAX_UNDO_DEPTH];
    int nextUndoIndex=0;
    int undoCount=0,redoCount=0;

    std::vector<std::atomic<float>> *convolver;

    int convolverLength=200;

    std::vector<std::atomic<float>> releaseCurve;

    std::vector<Module *> modules;//pointless to have more than one.
                                // polyphonic effects should be baked in and monophonic can be separate plugins
    /**
      Plugin class constructor.@n
      You must set all parameter values to their defaults, matching ParameterRanges::def.
    */
    ImGuiPluginDSP()
        : Plugin(kParamCount, 0, 1) // parameters, programs, states
    {
        if (DEBUG&&!GetConsoleWindow()) {
            initConsoleOutput();
            consoleAttached=true;
        }
        std::vector<float *>levels;
        levels.push_back(&fSpeed);
        modules.push_back(new Module(levels, &fSpeed, &fSpeed, &isReleaseEnabled, &fVelocitySensitivity,
                                     &fNoteSensitivity, &interpolationMode, releaseCurve));
        for(int i=0;i<MAX_UNDO_DEPTH;i++)
        {
            undoItems[i]=NULL;
        }

        for(int i=0;i<MAX_UNDO_DEPTH;i++)
        {
            undoItems[i]=NULL;
        }


        convolver=new std::vector<std::atomic<float>>(MAX_SAMPLE_LENGTH);
        for(int i=0;i<MAX_SAMPLE_LENGTH;i++)
        {
            (*convolver)[i]=0.f;
        }

        releaseCurve=std::vector<std::atomic<float>>(ENVELOPE_LENGTH);

        for(int i=0;i<ENVELOPE_LENGTH;i++)
        {
            releaseCurve[i].store(1.f,std::memory_order_relaxed);
        }
        interpolationMode.store(interpModeLinear,std::memory_order_relaxed);

    }
    ~ImGuiPluginDSP(){
        for(int i=0;i<modules.size();i++){
            delete(modules[i]);
        }
        delete convolver;
        for(int i=0;i<MAX_UNDO_DEPTH;i++)
        {
            if(undoItems[i])delete undoItems[i];
        }
    }

protected:


    /**
      Initialize the parameter @a index.@n
      This function will be called once, shortly after the plugin is created.
    */
    void initParameter(uint32_t index, Parameter& parameter) override
    {
        if(index==kParamSpeed)
        {
            parameter.ranges.min = 0.f;
            parameter.ranges.max = 1.f;
            parameter.ranges.def = 1.f;
            parameter.name = "Speed";
            parameter.symbol = "speed";
            parameter.hints=kParameterIsAutomatable;
        }
        if(index==kParamVelocitySensitivity)
        {
            parameter.ranges.min = 0.f;
            parameter.ranges.max = 1.f;
            parameter.ranges.def = 1.f;
            parameter.name = "Velocity";
            parameter.symbol = "velocity";
            parameter.hints=kParameterIsAutomatable;
        }
        if(index==kParamNoteSensitivity)
        {
            parameter.ranges.min = -1.f;
            parameter.ranges.max = 1.f;
            parameter.ranges.def = 1.f;
            parameter.name = "Midi note";
            parameter.symbol = "midi note";
            parameter.hints=kParameterIsAutomatable;
        }


    }

    float getParameterValue(uint32_t index) const override
    {
        if(index==kParamSpeed){
            return fSpeed;
        }
        if(index==kParamVelocitySensitivity){
            return fVelocitySensitivity;
        }
        if(index==kParamNoteSensitivity){
            return fNoteSensitivity;
        }
    }


    void setParameterValue(uint32_t index, float value) override
    {
        if(index==kParamSpeed){
            fSpeed=value;
        }
        if(index==kParamVelocitySensitivity){
            fVelocitySensitivity=value;
        }
        if(index==kParamNoteSensitivity){
            fNoteSensitivity=value;
        }
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
        if (!strcmp(key,"sampleData"))
        {

            // 1. Pack your sizes and channels sequentially into a simple local raw byte array
            size_t lengthSize = sizeof(uint32_t);
            size_t channelDataSize = MAX_SAMPLE_LENGTH * sizeof(float);
            size_t envelopeSize=ENVELOPE_LENGTH*sizeof(float);
            size_t convolverLengthSize=sizeof(float);
            size_t convolverSize=MAX_SAMPLE_LENGTH*sizeof(float);
            size_t stereoSize=sizeof(SpeakerConnections);
            size_t interpolateSize=sizeof(InterpolationMode);
            size_t releaseSize=sizeof(float)*ENVELOPE_LENGTH;
            size_t totalBytes = lengthSize + (channelDataSize * 2) +envelopeSize+convolverLengthSize+convolverSize+
                                stereoSize+interpolateSize+releaseSize;

            std::vector<uint8_t> rawBinaryBuffer(totalBytes);
            auto *incrementalPointer=rawBinaryBuffer.data();

            uint32_t length = static_cast<uint32_t>(modules[0]->sample->length.load(std::memory_order_relaxed));
            std::memcpy(rawBinaryBuffer.data(), &length, lengthSize);
            incrementalPointer += lengthSize;

            float* leftDest = reinterpret_cast<float*>(incrementalPointer);
            for (uint32_t i = 0; i < MAX_SAMPLE_LENGTH; ++i) {
                leftDest[i] = modules[0]->sample->sampleData[0][i].load(std::memory_order_relaxed);
            }
            incrementalPointer+=channelDataSize;

            float* rightDest = reinterpret_cast<float*>(incrementalPointer);
            for (uint32_t i = 0; i < MAX_SAMPLE_LENGTH; ++i) {
                rightDest[i] = modules[0]->sample->sampleData[1][i].load(std::memory_order_relaxed);
            }
            incrementalPointer+=channelDataSize;

            float *envelopeDest=reinterpret_cast<float*>(incrementalPointer);
            for (uint32_t i = 0; i < ENVELOPE_LENGTH; ++i) {
                envelopeDest[i] = modules[0]->envelope[i].load(std::memory_order_relaxed);
            }
            incrementalPointer+=envelopeSize;

            std::memcpy(incrementalPointer, &convolverLength, convolverLengthSize);
            incrementalPointer+=convolverLengthSize;

            float *convolverDest=reinterpret_cast<float*>(incrementalPointer);
            for (uint32_t i = 0; i < MAX_SAMPLE_LENGTH; ++i) {
                convolverDest[i] = (*convolver)[i].load(std::memory_order_relaxed);
            }
            incrementalPointer+=convolverSize;

            InterpolationMode tempMode=interpolationMode.load(std::memory_order_relaxed);
            std::memcpy(incrementalPointer, &tempMode, interpolateSize);
            incrementalPointer+=interpolateSize;

            SpeakerConnections tempConnections=modules[0]->speakerConnections.load(std::memory_order_relaxed);
            std::memcpy(incrementalPointer, &tempConnections, stereoSize);
            incrementalPointer+=stereoSize;

            float *releaseDest=reinterpret_cast<float*>(incrementalPointer);
            for (uint32_t i = 0; i < ENVELOPE_LENGTH; ++i) {
                releaseDest[i] = releaseCurve[i].load(std::memory_order_relaxed);
            }
            incrementalPointer+=releaseSize;

            // 3. Convert to base64 text string safely
            std::string encodedText = base64_encode(rawBinaryBuffer.data(), rawBinaryBuffer.size());
            return String(encodedText.c_str());
        }else return String("");

    }

    void setState(const char *key, const char * value){
        if(!strcmp("sampleData", key))
        {
            if (strlen(value) == 0 ) {
                return;
            }            
            // 4. Extract data directly out of the remaining decoded data stream
            size_t lengthSize = sizeof(uint32_t);
            size_t channelDataSize = MAX_SAMPLE_LENGTH * sizeof(float);
            size_t envelopeSize=ENVELOPE_LENGTH*sizeof(float);
            size_t convolverLengthSize=sizeof(uint32_t);
            size_t convolverSize=MAX_SAMPLE_LENGTH*sizeof(float);
            size_t stereoSize=sizeof(SpeakerConnections);
            size_t interpolateSize=sizeof(InterpolationMode);
            size_t releaseSize=MAX_SAMPLE_LENGTH*sizeof(float);

            std::string decodedBytes = base64_decode(std::string(value));

            const uint8_t* incrementalPointer = reinterpret_cast<const uint8_t*>(decodedBytes.data());

            // 2. Read the sample length header out of the first 4 bytes
            uint32_t length;
            std::memcpy(&length, incrementalPointer, sizeof(uint32_t));
            modules[0]->sample->length.store(length, std::memory_order_relaxed);
            incrementalPointer+=lengthSize;




            const float* leftSrc = reinterpret_cast<const float*>(incrementalPointer);
            for (uint32_t i = 0; i < MAX_SAMPLE_LENGTH; ++i) {
                modules[0]->sample->sampleData[0][i].store(leftSrc[i], std::memory_order_relaxed);
            }
            incrementalPointer+=channelDataSize;

            const float* rightSrc = reinterpret_cast<const float*>(incrementalPointer);
            for (uint32_t i = 0; i < MAX_SAMPLE_LENGTH; ++i) {
                modules[0]->sample->sampleData[1][i].store(rightSrc[i], std::memory_order_relaxed);
            }
            incrementalPointer+=channelDataSize;

            const float *envelopeSrc=reinterpret_cast<const float*>(incrementalPointer);
            for (uint32_t i = 0; i < ENVELOPE_LENGTH; ++i) {
                modules[0]->envelope[i].store(envelopeSrc[i], std::memory_order_relaxed);
            }
            incrementalPointer+=envelopeSize;

            std::memcpy(&convolverLength, incrementalPointer, convolverLengthSize);
            incrementalPointer+=convolverLengthSize;

            const float *convolverSrc=reinterpret_cast<const float*>(incrementalPointer);
            for (uint32_t i = 0; i < MAX_SAMPLE_LENGTH; ++i) {
                (*convolver)[i].store(convolverSrc[i], std::memory_order_relaxed);
            }
            incrementalPointer+=convolverSize;

            InterpolationMode tempMode;
            std::memcpy(&tempMode, incrementalPointer, interpolateSize);
            interpolationMode.store(tempMode,std::memory_order_relaxed);
            incrementalPointer+=interpolateSize;

            SpeakerConnections tempConnections;
            std::memcpy(&tempConnections, incrementalPointer , stereoSize);
            modules[0]->speakerConnections.store(tempConnections,std::memory_order_relaxed);
            incrementalPointer+=stereoSize;

            const float *releaseSrc=reinterpret_cast<const float*>(incrementalPointer);
            for (uint32_t i = 0; i < ENVELOPE_LENGTH; ++i) {
                releaseCurve[i].store(releaseSrc[i], std::memory_order_relaxed);
            }
            incrementalPointer+=releaseSize;

            modules[0]->process();
        }
    }

    void noteOn(int midiNote, float velocity){
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
        float velocity=(float)midi_data2;
        velocity/=128.f;
        switch ( midi_message )
        {
        case 0x80: // note_off
            noteOff(midi_data1);
            break;
        case 0x90: // note_on
            noteOn(midi_data1, velocity);
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
        return "Sampler with spectral editing";
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
