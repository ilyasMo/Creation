#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "GainStage.h"
#include "DelayEngine.h"

class AfterglowProcessor final : public juce::AudioProcessor, private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit AfterglowProcessor(juce::File presetDirectory = {});
    ~AfterglowProcessor() override;
    juce::Point<int> editorSize() const noexcept;
    void rememberEditorWidth(int width, bool notifyHost=true) noexcept;
    juce::String currentPresetName() const;
    juce::String presetDisplayName() const;
    juce::Array<juce::File> userPresets() const;
    void loadDefaultPreset();
    juce::Result savePresetAs(const juce::String& name);
    juce::Result loadPreset(const juce::File& file);
    const juce::String getName() const override { return JucePlugin_Name; }
    void prepareToPlay(double sampleRate, int maximumBlockSize) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    juce::AudioProcessorValueTreeState& getParameterState() noexcept { return parameters; }
    double getTailLengthSeconds() const override;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int sizeInBytes) override;

private:
    void applyGain(juce::AudioBuffer<float>&, float linearGain);
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* outputGain = nullptr;
    std::atomic<float>* inputGain = nullptr;
    afterglow::GainStage inputGainStage;
    const juce::File presetDirectory;
    std::atomic<int> editorWidth { 1165 };
    mutable juce::CriticalSection presetMetadataLock;
    juce::String presetName { "Default" };
    std::atomic<bool> presetDirty { false }, applyingPreset { false };
    void parameterChanged(const juce::String&, float) override;
    void setPresetIdentity(const juce::String&, bool dirty);
    bool parametersAreDefault() const;
    juce::Result applyPresetParameters(const juce::ValueTree&, const juce::String& name);
    afterglow::GainStage gainStage;
    afterglow::DelayEngine delayEngine;
    afterglow::DelaySettings readDelaySettings() const noexcept;
    void updateTempo();
    std::atomic<double> hostTempo { 120.0 };
    struct DelayParameters
    {
        std::atomic<float> *enabled = nullptr, *sync = nullptr, *tapeEnabled = nullptr, *eqEnabled = nullptr, *time = nullptr,
            *feedback = nullptr, *lowCut = nullptr, *highCut = nullptr,
            *wow = nullptr, *flutter = nullptr,
            *pingPong = nullptr, *mix = nullptr, *division = nullptr, *timing = nullptr;
    } delayParameters;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AfterglowProcessor)
};
