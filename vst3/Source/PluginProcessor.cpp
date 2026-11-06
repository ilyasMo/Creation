#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    number("delayWow", "Wow", 0, 100, 0.1f, 0, "%");
    number("delayFlutter", "Flutter", 0, 100, 0.1f, 0, "%");
    layout.add(std::make_unique<juce::AudioParameterBool>(ID { "eqEnabled", 1 }, "EQ ON", false));
    number("eqLowCutHz", "Low cut / high-pass", 20, 20000, 1, 20, "Hz", 0.3f);
    number("eqHighCutHz", "High cut / low-pass", 20, 20000, 1, 20000, "Hz", 0.3f);
    number("delayPingPong", "Ping-pong", 0, 100, 0.1f, 0, "%");
    number("masterMix", "Master Mix", 0, 100, 0.1f, 100, "%");
    layout.add(std::make_unique<juce::AudioParameterBool>(ID { "tapeEnabled", 1 }, "Tape ON", false));
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
    p.tapeEnabled = parameters.getRawParameterValue("tapeEnabled");
    p.time = parameters.getRawParameterValue("delayTimeMs");
    p.feedback = parameters.getRawParameterValue("delayFeedback");
    p.eqEnabled = parameters.getRawParameterValue("eqEnabled");
    p.lowCut = parameters.getRawParameterValue("eqLowCutHz");
    p.highCut = parameters.getRawParameterValue("eqHighCutHz");
    p.wow = parameters.getRawParameterValue("delayWow");
    p.flutter = parameters.getRawParameterValue("delayFlutter");
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
    s.tapeEnabled = p.tapeEnabled->load() >= 0.5f;
    s.sync = p.sync->load() >= 0.5f; s.eqEnabled = p.eqEnabled->load() >= 0.5f;
    s.timeMs = p.time->load(); s.feedback = p.feedback->load() / 100.0f;
    s.lowCutHz = p.lowCut->load(); s.highCutHz = p.highCut->load();
    s.wow = p.wow->load() / 100.0f; s.flutter = p.flutter->load() / 100.0f;
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
    if (!s.enabled) return 0.25;
    const double feedback = std::clamp(static_cast<double>(s.feedback), 0.0, 0.95);
    const double repeats = feedback > 0.001 ? std::ceil(std::log(0.001) / std::log(feedback)) : 0.0;
    return afterglow::delaySeconds(s) * (repeats + 1.0) + 0.25;
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
    setLatencySamples(delayEngine.latencySamples());
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
    settings.enabled = false; settings.eqEnabled = false; settings.mix = 0;
    delayEngine.process(buffer.getArrayOfWritePointers(), getTotalNumInputChannels(),
                        buffer.getNumSamples(), settings);
    applyGain(buffer, 1.0f);
}

juce::AudioProcessorEditor* AfterglowProcessor::createEditor()
{
    return new AfterglowEditor(*this);
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
            // New EQ defaults are neutral/off. Removed feedback Tone/Wear are not restored.
            for (const char* id : { "delayToneHz", "delayDrive" })
            {
                const auto legacy = restored.getChildWithProperty("id", id);
                if (legacy.isValid()) restored.removeChild(legacy, nullptr);
            }
            // Earlier versions applied nonzero tape movement without a separate bypass.
            // Preserve that sound when restoring their sessions; new instances still start OFF.
            if (!restored.getChildWithProperty("id", "tapeEnabled").isValid())
            {
                const auto wow = restored.getChildWithProperty("id", "delayWow");
                const auto flutter = restored.getChildWithProperty("id", "delayFlutter");
                const bool usedTape = (wow.isValid() && static_cast<float>(wow.getProperty("value")) > 0)
                                  || (flutter.isValid() && static_cast<float>(flutter.getProperty("value")) > 0);
                juce::ValueTree tape("PARAM");tape.setProperty("id", "tapeEnabled", nullptr);
                tape.setProperty("value", usedTape ? 1.0f : 0.0f, nullptr);restored.appendChild(tape, nullptr);
            }
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
