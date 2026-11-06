#include "PluginProcessor.h"

AfterglowProcessor::AfterglowProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "AfterglowState",
          juce::AudioProcessorValueTreeState::ParameterLayout {
              std::make_unique<juce::AudioParameterFloat>(
                  juce::ParameterID { "outputGain", 1 }, "Output gain",
                  juce::NormalisableRange<float> { -24.0f, 12.0f, 0.1f }, 0.0f,
                  juce::AudioParameterFloatAttributes().withLabel("dB")) })
{
    outputGain = parameters.getRawParameterValue("outputGain");
}

bool AfterglowProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto output = layout.getMainOutputChannelSet();
    return (output == juce::AudioChannelSet::mono()
            || output == juce::AudioChannelSet::stereo())
           && output == layout.getMainInputChannelSet();
}

void AfterglowProcessor::prepareToPlay(double sampleRate, int)
{
    gainStage.prepare(sampleRate, juce::Decibels::decibelsToGain(outputGain->load()));
    setLatencySamples(0);
}

void AfterglowProcessor::applyGain(juce::AudioBuffer<float>& buffer, float gain)
{
    juce::ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());
    gainStage.process(buffer.getArrayOfWritePointers(),
                      juce::jmin(buffer.getNumChannels(), getTotalNumInputChannels()),
                      buffer.getNumSamples(), gain);
}

void AfterglowProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    applyGain(buffer, juce::Decibels::decibelsToGain(outputGain->load()));
}

void AfterglowProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    applyGain(buffer, 1.0f);
}

juce::AudioProcessorEditor* AfterglowProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void AfterglowProcessor::getStateInformation(juce::MemoryBlock& data)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, data);
}

void AfterglowProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType().toString()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AfterglowProcessor();
}
