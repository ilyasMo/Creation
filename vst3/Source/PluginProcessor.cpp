#include "PluginProcessor.h"

namespace
{
juce::AudioProcessorValueTreeState::ParameterLayout createParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    using ID = juce::ParameterID;
    using Float = juce::AudioParameterFloat;
    auto number = [&](const char* id, const char* name, float min, float max,
                      float step, float initial, const char* unit, float skew = 1.0f)
    {
        layout.add(std::make_unique<Float>(ID { id, 1 }, name,
            juce::NormalisableRange<float> { min, max, step, skew }, initial,
            juce::AudioParameterFloatAttributes().withLabel(unit)));
    };
    // Keep the original ID stable for previously saved Live projects.
    number("outputGain", "Output gain", -24, 12, 0.1f, 0, "dB");
    layout.add(std::make_unique<juce::AudioParameterBool>(ID { "delayEnabled", 1 }, "Delay ON", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(ID { "delaySync", 1 }, "Sync to host", false));
    number("delayTimeMs", "Free delay time", 20, 2000, 1, 320, "ms", 0.5f);
    layout.add(std::make_unique<juce::AudioParameterChoice>(ID { "delayDivision", 1 }, "Synced note",
        juce::StringArray { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64" }, 3));
    layout.add(std::make_unique<juce::AudioParameterChoice>(ID { "delayTiming", 1 }, "Note style",
        juce::StringArray { "Straight", "Dotted", "Triplet" }, 0));
    number("delayFeedback", "Feedback", 0, 95, 0.1f, 35, "%");
    number("delayToneHz", "Feedback tone", 500, 14000, 1, 4000, "Hz", 0.4f);
    number("delayDrive", "Feedback wear", 0, 100, 0.1f, 25, "%");
    number("delayPingPong", "Ping-pong", 0, 100, 0.1f, 0, "%");
    number("masterMix", "Master Mix", 0, 100, 0.1f, 100, "%");
    return layout;
}
}

AfterglowProcessor::AfterglowProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "AfterglowState", createParameters())
{
    outputGain = parameters.getRawParameterValue("outputGain");
    auto& p = delayParameters;
    p.enabled = parameters.getRawParameterValue("delayEnabled");
    p.sync = parameters.getRawParameterValue("delaySync");
    p.time = parameters.getRawParameterValue("delayTimeMs");
    p.feedback = parameters.getRawParameterValue("delayFeedback");
    p.tone = parameters.getRawParameterValue("delayToneHz");
    p.drive = parameters.getRawParameterValue("delayDrive");
    p.pingPong = parameters.getRawParameterValue("delayPingPong");
    p.mix = parameters.getRawParameterValue("masterMix");
    p.division = parameters.getRawParameterValue("delayDivision");
    p.timing = parameters.getRawParameterValue("delayTiming");
}

afterglow::DelaySettings AfterglowProcessor::readDelaySettings() const noexcept
{
    const auto& p = delayParameters;
    afterglow::DelaySettings s;
    s.enabled = p.enabled->load() >= 0.5f;
    s.sync = p.sync->load() >= 0.5f;
    s.timeMs = p.time->load(); s.feedback = p.feedback->load() / 100.0f;
    s.toneHz = p.tone->load(); s.drive = p.drive->load() / 100.0f;
    s.pingPong = p.pingPong->load() / 100.0f; s.mix = p.mix->load() / 100.0f;
    s.division = juce::roundToInt(p.division->load());
    s.timing = juce::roundToInt(p.timing->load()); s.bpm = hostTempo.load();
    return s;
}

void AfterglowProcessor::updateTempo()
{
    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
            if (auto tempo = position->getBpm())
                if (std::isfinite(*tempo) && *tempo > 0.0)
                    hostTempo.store(*tempo);
}

double AfterglowProcessor::getTailLengthSeconds() const
{
    const auto s = readDelaySettings();
    if (!s.enabled) return 0.04;
    const double feedback = std::clamp(static_cast<double>(s.feedback), 0.0, 0.95);
    const double repeats = feedback > 0.001 ? std::ceil(std::log(0.001) / std::log(feedback)) : 0.0;
    return afterglow::delaySeconds(s) * (repeats + 1.0) + 0.04;
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
    updateTempo();
    delayEngine.prepare(sampleRate, readDelaySettings());
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
    juce::ScopedNoDenormals noDenormals;
    updateTempo();
    delayEngine.process(buffer.getArrayOfWritePointers(), getTotalNumInputChannels(),
                        buffer.getNumSamples(), readDelaySettings());
    applyGain(buffer, juce::Decibels::decibelsToGain(outputGain->load()));
}

void AfterglowProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    updateTempo();
    auto settings = readDelaySettings();
    settings.enabled = false; settings.mix = 0;
    delayEngine.process(buffer.getArrayOfWritePointers(), getTotalNumInputChannels(),
                        buffer.getNumSamples(), settings);
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
        {
            auto restored = juce::ValueTree::fromXml(*xml);
            // Earlier gain-only sessions have no delay parameters. Add their defaults.
            for (auto* parameter : getParameters())
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
                    if (!restored.getChildWithProperty("id", ranged->paramID).isValid())
                    {
                        juce::ValueTree child("PARAM");
                        child.setProperty("id", ranged->paramID, nullptr);
                        child.setProperty("value", ranged->convertFrom0to1(ranged->getDefaultValue()), nullptr);
                        restored.appendChild(child, nullptr);
                    }
            parameters.replaceState(restored);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AfterglowProcessor();
}
