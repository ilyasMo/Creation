#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ParameterText.h"

class AfterglowEditor::ParameterRow final : public juce::Component
{
public:
    ParameterRow(juce::AudioProcessorValueTreeState& state, const char* id, const char* text)
        : parameter(*state.getParameter(id))
    {
        name.setText(text, juce::dontSendNotification);
        name.setColour(juce::Label::textColourId, juce::Colour(0xffddd8c4));
        name.setFont(juce::Font(juce::FontOptions(13.0f)));
        addAndMakeVisible(name);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::trackColourId, juce::Colour(0xff9fb79a));
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffe4c78e));
        addAndMakeVisible(slider);
        value.setEditable(false, true, false);
        value.setJustificationType(juce::Justification::centredRight);
        value.setColour(juce::Label::textColourId, juce::Colour(0xffe5c792));
        value.setColour(juce::Label::backgroundColourId, juce::Colour(0xff1d2b24));
        value.setColour(juce::Label::outlineColourId, juce::Colour(0xff667562));
        value.setTooltip("Double-click to type a value. Enter saves; Escape cancels.");
        value.onTextChange = [this]
        {
            double parsed = 0;
            if (afterglow::parseParameterText(value.getText().toStdString(), parameter.getLabel().toStdString(), parsed))
            {
                const auto& range = parameter.getNormalisableRange();
                const float requested = range.snapToLegalValue(juce::jlimit(range.start, range.end, static_cast<float>(parsed)));
                parameter.beginChangeGesture();
                parameter.setValueNotifyingHost(parameter.convertTo0to1(requested));
                parameter.endChangeGesture();
                slider.setValue(requested, juce::dontSendNotification);
            }
            refreshValue();
        };
        value.onEditorHide = [this] { refreshValue(); };
        addAndMakeVisible(value);
        slider.onValueChange = [this] { refreshValue(); };
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, id, slider);
        refreshValue();
    }
    void resized() override
    {
        auto area = getLocalBounds().reduced(0, 4);
        name.setBounds(area.removeFromLeft(151));
        value.setBounds(area.removeFromRight(104));
        area.removeFromRight(12);
        slider.setBounds(area);
    }
private:
    void refreshValue()
    {
        if (value.isBeingEdited()) return;
        const auto unit = parameter.getLabel();
        const int decimals = unit == "Hz" || unit == "ms" ? 0 : 1;
        value.setText(juce::String(slider.getValue(), decimals) + " " + unit, juce::dontSendNotification);
    }
    juce::RangedAudioParameter& parameter;
    juce::Label name, value;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

AfterglowEditor::AfterglowEditor(AfterglowProcessor& p) : AudioProcessorEditor(p), processor(p)
{
    auto& state = p.getParameterState();
    title.setText("Afterglow / Delay", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(26.0f)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xffe4c78e));
    subtitle.setText("CREATION AUDIO   /   WOW + FLUTTER", juce::dontSendNotification);
    subtitle.setFont(juce::Font(juce::FontOptions(10.0f)));
    subtitle.setColour(juce::Label::textColourId, juce::Colour(0xff9fb095));
    filterTitle.setText("PROCESSED SIGNAL EQ", juce::dontSendNotification);
    filterTitle.setFont(juce::Font(juce::FontOptions(11.0f)));
    filterTitle.setColour(juce::Label::textColourId, juce::Colour(0xffaabb9b));
    hint.setText("Double-click any number to type.  Master Mix blends the whole processed chain.", juce::dontSendNotification);
    hint.setFont(juce::Font(juce::FontOptions(10.0f)));
    hint.setColour(juce::Label::textColourId, juce::Colour(0xff9ba88e));
    for (auto* label : { &title, &subtitle, &filterTitle, &hint }) addAndMakeVisible(label);
    for (auto* button : { &enabled, &sync, &eq })
    {
        button->setColour(juce::ToggleButton::textColourId, juce::Colour(0xffe3ddc7));
        button->setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffdfc48d));
        addAndMakeVisible(button);
    }
    division.addItemList(juce::StringArray { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64" }, 1);
    timing.addItemList(juce::StringArray { "Straight", "Dotted", "Triplet" }, 1);
    division.setTooltip("Synced note division"); timing.setTooltip("Synced note style");
    for (auto* combo : { &division, &timing })
    {
        combo->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff203229));
        combo->setColour(juce::ComboBox::textColourId, juce::Colour(0xffddd8c4));
        addAndMakeVisible(combo);
    }
    using State = juce::AudioProcessorValueTreeState;
    enabledAttachment = std::make_unique<State::ButtonAttachment>(state, "delayEnabled", enabled);
    syncAttachment = std::make_unique<State::ButtonAttachment>(state, "delaySync", sync);
    eqAttachment = std::make_unique<State::ButtonAttachment>(state, "eqEnabled", eq);
    divisionAttachment = std::make_unique<State::ComboBoxAttachment>(state, "delayDivision", division);
    timingAttachment = std::make_unique<State::ComboBoxAttachment>(state, "delayTiming", timing);
    const std::array<std::pair<const char*,const char*>, 9> parameters {{
        {"delayTimeMs", "Free time"}, {"delayFeedback", "Feedback"}, {"delayWow", "Wow / slow drift"},
        {"delayFlutter", "Flutter / fast movement"}, {"delayPingPong", "Ping-pong"},
        {"eqLowCutHz", "Low cut / high-pass"}, {"eqHighCutHz", "High cut / low-pass"},
        {"masterMix", "Master Mix"}, {"outputGain", "Output gain"}
    }};
    for (const auto& item : parameters)
    {
        auto row = std::make_unique<ParameterRow>(state, item.first, item.second);
        addAndMakeVisible(*row); rows.push_back(std::move(row));
    }
    setSize(640, 620);
    timerCallback(); startTimerHz(15);
}
AfterglowEditor::~AfterglowEditor() = default;
void AfterglowEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff29392f));
    g.setColour(juce::Colour(0xff71816a)); g.drawLine(24, 82, getWidth()-24.0f, 82);
    g.setColour(juce::Colour(0xff203027)); g.fillRoundedRectangle(18.0f, 350.0f, getWidth()-36.0f, 128.0f, 5.0f);
}
void AfterglowEditor::resized()
{
    auto area = getLocalBounds().reduced(24, 16);
    title.setBounds(area.removeFromTop(35)); subtitle.setBounds(area.removeFromTop(24));
    area.removeFromTop(12);
    auto buttons = area.removeFromTop(34);
    enabled.setBounds(buttons.removeFromLeft(150)); sync.setBounds(buttons.removeFromLeft(150));
    rows[0]->setBounds(area.removeFromTop(40));
    auto notes = area.removeFromTop(38); notes.removeFromLeft(151);
    division.setBounds(notes.removeFromLeft(180).reduced(0,4)); notes.removeFromLeft(12);
    timing.setBounds(notes.reduced(0,4));
    for (int i=1;i<=4;++i) rows[static_cast<size_t>(i)]->setBounds(area.removeFromTop(40));
    auto filters = area.removeFromTop(30);
    filterTitle.setBounds(filters.removeFromLeft(300)); eq.setBounds(filters.removeFromRight(100));
    rows[5]->setBounds(area.removeFromTop(40)); rows[6]->setBounds(area.removeFromTop(40));
    area.removeFromTop(12);
    rows[7]->setBounds(area.removeFromTop(40)); rows[8]->setBounds(area.removeFromTop(40));
    hint.setBounds(area.removeFromTop(26));
}
void AfterglowEditor::timerCallback()
{
    const bool synced = sync.getToggleState();
    rows[0]->setEnabled(!synced); division.setEnabled(synced); timing.setEnabled(synced);
}
