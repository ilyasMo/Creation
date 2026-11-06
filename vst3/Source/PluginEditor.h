#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>
#include <vector>
class AfterglowProcessor;

class AfterglowEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AfterglowEditor(AfterglowProcessor&);
    ~AfterglowEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    class ParameterRow;
    void timerCallback() override;
    AfterglowProcessor& processor;
    juce::Label title, subtitle, filterTitle, hint;
    juce::ToggleButton enabled { "Delay ON" }, sync { "Sync to host" }, eq { "EQ ON" };
    juce::ComboBox division, timing;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment, syncAttachment, eqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divisionAttachment, timingAttachment;
    std::vector<std::unique_ptr<ParameterRow>> rows;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AfterglowEditor)
};
