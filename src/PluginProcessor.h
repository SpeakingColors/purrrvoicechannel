#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "Presets.h"

class PurrVoiceProcessor : public juce::AudioProcessor
{
public:
    PurrVoiceProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Purrr Voice Channel"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return (int) factory.size(); }
    int getCurrentProgram() override { return juce::jmax (0, currentFactory); }
    void setCurrentProgram (int i) override { loadFactory (i); }
    const juce::String getProgramName (int i) override { return juce::isPositiveAndBelow (i, (int) factory.size()) ? factory[(size_t) i].name : juce::String(); }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // presets: factory list (read only) plus the user's own files in <app data>/Studio 87/Purrr Voice Channel/presets
    static juce::File presetDir();
    juce::StringArray userPresetNames() const;
    void loadFactory (int index);
    bool loadUser (const juce::String& name);
    bool saveUser (const juce::String& name);
    bool deleteUser (const juce::String& name);
    bool exportTo (const juce::File&);
    bool importFrom (const juce::File&);
    void resetToDefaults();

    juce::AudioProcessorValueTreeState apvts;
    std::vector<pvc::Preset> factory = pvc::factoryPresets();
    juce::String presetName { "Init (clean start)" }; int currentFactory = 0;
    juce::String notes; int editorWidth = 0; bool bubbles = true;

    // live values for the editor (written on the audio thread, read on the message thread)
    struct Live { std::atomic<float> inL { 0 }, inR { 0 }, outL { 0 }, outR { 0 }, rms { 0 }, gr { 0 }, eg { 0 }, dgr { 0 }, tubeK { 0 }, lvIn { -120 }, mk { 0 }; std::atomic<int> gopen { 1 }; } live;
    // output samples for the spectrum in the EQ view (single producer, single consumer)
    static constexpr int specN = 8192;
    std::array<float, specN> specBuf {}; std::atomic<int> specW { 0 };
    double sampleRateNow() const { return sr; }

private:
    void applyValues (const std::vector<std::pair<juce::String, float>>& v);
    juce::ValueTree stateTree() const;
    std::unique_ptr<purrvc::VoiceDsp> dsp; double sr = 48000;
    juce::AudioBuffer<float> dryDelay; int dryPos = 0;
    bool jumpVoltage = true;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PurrVoiceProcessor)
};
