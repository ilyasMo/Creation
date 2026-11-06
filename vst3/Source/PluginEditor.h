#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>

class AfterglowProcessor;

class AfterglowEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AfterglowEditor(AfterglowProcessor&);
    ~AfterglowEditor() override;
    void resized() override;
private:
    void timerCallback() override;
    AfterglowProcessor& processor;
    bool sizeReady=false;
    class SketchPanel;
    std::unique_ptr<SketchPanel> panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AfterglowEditor)
};
