#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "GainStage.h"
#include "DelayEngine.h"

class AfterglowProcessor final : public juce::AudioProcessor
{
public:
    AfterglowProcessor();
    const juce::String getName() const override { return JucePlugin_Name; }
    void prepareToPlay(double sampleRate, int maximumBlockSize) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
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
    afterglow::GainStage gainStage;
    afterglow::DelayEngine delayEngine;
    afterglow::DelaySettings readDelaySettings() const noexcept;
    void updateTempo();
    std::atomic<double> hostTempo { 120.0 };
    struct DelayParameters
    {
        std::atomic<float> *enabled = nullptr, *sync = nullptr, *time = nullptr,
            *feedback = nullptr, *tone = nullptr, *drive = nullptr,
            *pingPong = nullptr, *mix = nullptr, *division = nullptr, *timing = nullptr;
    } delayParameters;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AfterglowProcessor)
};
