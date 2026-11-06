#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ParameterText.h"
#include "BinaryData.h"
#include <array>
#include <cmath>

namespace
{
const juce::Colour ink { 0xff292821 }, paper { 0xfffff4e1 }, orange { 0xffd87b32 };
using State = juce::AudioProcessorValueTreeState;

// Fixed irregularities: paths never change randomly during a repaint or drag.
juce::Path inkBox(juce::Rectangle<float> r, float lean = 0.0f)
{
    juce::Path p;
    const auto x=r.getX(), y=r.getY(), w=r.getWidth(), h=r.getHeight();
    p.startNewSubPath(x+3, y+2);
    p.cubicTo(x+w*.28f, y-1, x+w*.72f, y+2, x+w-2, y+lean);
    p.cubicTo(x+w+1, y+h*.3f, x+w-1, y+h*.7f, x+w-3, y+h-1);
    p.cubicTo(x+w*.64f, y+h+2, x+w*.3f, y+h-2, x+1, y+h);
    p.cubicTo(x-1, y+h*.68f, x+2, y+h*.32f, x+3, y+2);
    p.closeSubPath(); return p;
}
void line(juce::Graphics& g, float x1,float y1,float x2,float y2,float thickness,juce::Colour c)
{
    juce::Path p; p.startNewSubPath(x1,y1);
    p.cubicTo(x1+(x2-x1)*.32f+.7f,y1+(y2-y1)*.33f-.5f,
              x1+(x2-x1)*.67f-.8f,y1+(y2-y1)*.66f+.7f,x2,y2);
    g.setColour(c); g.strokePath(p,juce::PathStrokeType(thickness,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
}
void box(juce::Graphics& g,juce::Rectangle<float> r,bool fill=true,float thickness=3.5f)
{
    const auto p=inkBox(r, -1.2f);
    if(fill) { g.setColour(paper); g.fillPath(p); }
    g.setColour(ink); g.strokePath(p,juce::PathStrokeType(thickness,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
}

class InkLook final : public juce::LookAndFeel_V4
{
public:
    InkLook()
    {
        face=juce::Typeface::createSystemTypefaceFor(BinaryData::Caveat_ttf, BinaryData::Caveat_ttfSize);
        setColour(juce::TooltipWindow::backgroundColourId,paper);
        setColour(juce::TooltipWindow::textColourId,ink);
        setColour(juce::TooltipWindow::outlineColourId,ink.withAlpha(.4f));
        setColour(juce::PopupMenu::backgroundColourId,paper);
        setColour(juce::PopupMenu::textColourId,ink);
        setColour(juce::PopupMenu::highlightedBackgroundColourId,orange.withAlpha(.2f));
        setColour(juce::PopupMenu::highlightedTextColourId,ink);
        setColour(juce::TextEditor::backgroundColourId,paper);
        setColour(juce::TextEditor::textColourId,ink);
        setColour(juce::TextEditor::highlightColourId,orange.withAlpha(.25f));
        setColour(juce::TextEditor::outlineColourId,orange);
        setColour(juce::CaretComponent::caretColourId,ink);
    }
    juce::Font font(float size) const { return juce::Font(juce::FontOptions(face).withHeight(size)); }
    juce::Font getLabelFont(juce::Label& l) override { return font(l.getFont().getHeight()); }
    juce::Font getTextButtonFont(juce::TextButton&,int h) override { return font(static_cast<float>(h)*.65f); }
    juce::Font getPopupMenuFont() override { return font(26); }
    int getSliderThumbRadius(juce::Slider&) override { return 28; }
    void drawLabel(juce::Graphics& g,juce::Label& label) override
    {
        if(!label.isBeingEdited())
        {
            box(g,label.getLocalBounds().toFloat().reduced(3),true,2.8f);
            g.setColour(ink.withAlpha(label.isEnabled()?1.0f:.45f)); g.setFont(getLabelFont(label));
            g.drawFittedText(label.getText(),label.getLocalBounds().reduced(8,2),label.getJustificationType(),1);
        }
    }
    void drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,float pos,float,float,
                          juce::Slider::SliderStyle style,juce::Slider& slider) override
    {
        const float alpha=slider.isEnabled()?1.0f:.4f;
        juce::Graphics::ScopedSaveState save(g); g.setOpacity(alpha);
        const bool vertical=style==juce::Slider::LinearVertical;
        if(vertical)
        {
            const float cx=static_cast<float>(x)+static_cast<float>(width)*.5f;
            const float top=static_cast<float>(y)+28, bottom=static_cast<float>(y+height)-28;
            box(g,{cx-19,top,38,bottom-top},false,4.5f);
            line(g,cx+1,bottom-6,cx,pos,7,orange);
            box(g,{cx-65,pos-27,130,54},true,4.8f);
            line(g,cx-62,pos+23,cx+61,pos+25,1.3f,ink.withAlpha(.25f));
            if(slider.isMouseOverOrDragging()) line(g,cx-45,pos+15,cx+43,pos+14,2,orange.withAlpha(.65f));
        }
        else
        {
            const float cy=static_cast<float>(y)+static_cast<float>(height)*.5f;
            const float left=static_cast<float>(x)+28,right=static_cast<float>(x+width)-28;
            box(g,{left,cy-11,right-left,22},false,3.5f);
            line(g,left+5,cy,pos,cy,5,orange);
            box(g,{pos-16,cy-29,32,58},true,3.5f);
        }
    }
    void drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool over,bool) override
    {
        box(g,{4,10,32,32},false,3);
        if(b.getToggleState()) { line(g,10,25,18,33,4,orange); line(g,18,33,37,9,4,orange); }
        g.setColour(over?orange:ink);g.setFont(font(36));
        g.drawText(b.getButtonText(),45,0,b.getWidth()-45,b.getHeight(),juce::Justification::centredLeft);
    }
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool over,bool down) override
    {
        if(over||down) { g.setColour(orange.withAlpha(down?.18f:.08f));g.fillRect(b.getLocalBounds()); }
    }
    void drawButtonText(juce::Graphics& g,juce::TextButton& b,bool,bool) override
    {
        g.setColour(ink);g.setFont(font(32));g.drawText(b.getButtonText(),b.getLocalBounds(),juce::Justification::centred);
    }
private:
    juce::Typeface::Ptr face;
};

void setupValue(juce::Label& value,const char* id)
{
    value.setComponentID(juce::String(id)+"Value");
    value.setEditable(false,true,false);
    value.setFont(juce::Font(juce::FontOptions(40.0f)));
    value.setJustificationType(juce::Justification::centred);
    value.setColour(juce::Label::textColourId,ink);
    value.setTooltip("Double-click to type a value. Enter applies; Escape cancels.");
}

class InkSlider final : public juce::Slider
{
public:
    InkSlider() {setSliderSnapsToMousePosition(false);}
    bool reverse=false;
    double valueToProportionOfLength(double v) override
    {const double n=juce::Slider::valueToProportionOfLength(v);return reverse?1.0-n:n;}
    double proportionOfLengthToValue(double n) override
    {return juce::Slider::proportionOfLengthToValue(reverse?1.0-n:n);}
    void mouseDown(const juce::MouseEvent& e) override
    {last=adjusted=e.position;juce::Slider::mouseDown(e);}
    void mouseDrag(const juce::MouseEvent& e) override
    {
        adjusted+=(e.position-last)*(e.mods.isShiftDown()?.12f:1.0f);last=e.position;
        juce::Slider::mouseDrag(e.withNewPosition(adjusted));
    }
private:
    juce::Point<float> last,adjusted;
};

class InkFader final : public juce::Component
{
public:
    InkFader(State& s,const char* id,bool vertical=true) : state(s)
    {
        slider.setSliderStyle(vertical?juce::Slider::LinearVertical:juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        slider.setScrollWheelEnabled(false); // Avoid changing sound while scrolling a host window.
        slider.setVelocityBasedMode(false);
        addAndMakeVisible(slider); setupValue(value,id);addAndMakeVisible(value);
        value.onTextChange=[this]
        {
            if(parameter==nullptr)return;
            double parsed=0;
            if(afterglow::parseParameterText(value.getText().toStdString(),parameter->getLabel().toStdString(),parsed))
            {
                const auto& r=parameter->getNormalisableRange();
                const float v=r.snapToLegalValue(juce::jlimit(r.start,r.end,static_cast<float>(parsed)));
                parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->convertTo0to1(v));parameter->endChangeGesture();
                slider.setValue(v,juce::dontSendNotification);
            }
            refresh();
        };
        value.onEditorHide=[this]{refresh();}; slider.onValueChange=[this]{refresh();};
        bind(id);
    }
    void bind(const char* id)
    {
        attachment.reset();parameter=state.getParameter(id);
        slider.reverse=juce::String(id)=="delayDivision";
        slider.setComponentID(id);setComponentID(juce::String(id)+"Fader");
        attachment=std::make_unique<State::SliderAttachment>(state,id,slider);
        slider.setTooltip(parameter->getName(80)+". Drag to change; Shift for finer control. Double-click to reset.");
        refresh();
    }
    void resized() override
    {
        if(slider.isVertical()) {auto r=getLocalBounds();value.setBounds(r.removeFromBottom(60).reduced(2,3));r.removeFromBottom(10);slider.setBounds(r);}
        else {auto r=getLocalBounds();value.setBounds(r.removeFromRight(110).reduced(2,6));r.removeFromRight(12);slider.setBounds(r);}
        slider.setMouseDragSensitivity(juce::jmax(80,(slider.isVertical()?slider.getHeight():slider.getWidth())-56));
    }
    void refresh()
    {
        if(value.isBeingEdited()||parameter==nullptr)return;
        if(format) value.setText(format(static_cast<float>(slider.getValue())),juce::dontSendNotification);
        else
        {
            const auto unit=parameter->getLabel(); const double v=slider.getValue();
            juce::String text;
            if(unit=="Hz"&&v>=1000)text=juce::String(v/1000.0,2).trimCharactersAtEnd("0").trimCharactersAtEnd(".")+" kHz";
            else text=juce::String(v,unit=="ms"||unit=="Hz"||std::abs(v-std::round(v))<.001?0:1)+(unit=="%"?"%":" "+unit);
            value.setText(text,juce::dontSendNotification);
        }
    }
    InkSlider slider;
    juce::Label value;
    std::function<juce::String(float)> format;
private:
    State& state;
    juce::RangedAudioParameter* parameter=nullptr;
    std::unique_ptr<State::SliderAttachment> attachment;
};

// Two separately automated filters share one logarithmic frequency rail.
class InkEQ final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit InkEQ(State& state)
    {
        setComponentID("sharedEQ"); setTooltip("Drag HIGH CUT or LOW CUT. Double-click either number to type a frequency.");
        const char* ids[]{"eqLowCutHz","eqHighCutHz"};
        for(size_t i=0;i<2;++i)
        {
            params[i]=state.getParameter(ids[i]);setupValue(values[i],ids[i]);addAndMakeVisible(values[i]);
            values[i].setFont(juce::Font(juce::FontOptions(34.0f)));
            values[i].onTextChange=[this,i]
            {
                double parsed=0;
                if(afterglow::parseParameterText(values[i].getText().toStdString(),"Hz",parsed))
                    attachments[i]->setValueAsCompleteGesture(static_cast<float>(juce::jlimit(20.0,20000.0,parsed)));
                updateLabels();
            };
            values[i].onEditorHide=[this]{updateLabels();};
            attachments[i]=std::make_unique<juce::ParameterAttachment>(*params[i],[this,i](float hz){frequency[i]=hz;updateLabels();resized();repaint();},nullptr);
        }
        for(auto& a:attachments)a->sendInitialUpdate();
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    }
    void paint(juce::Graphics& g) override
    {
        box(g,{182,32,38,520},false,4.3f);
        line(g,201,thumbY(0),200,thumbY(1),7,orange.withAlpha(.9f));
        for(size_t i=0;i<2;++i)
        {
            const float y=displayY(i);
            if(std::abs(y-thumbY(i))>1)line(g,201,thumbY(i),201,y,2,ink);
            box(g,{107,y-26,187,52},true,4.3f);
            auto* look=dynamic_cast<InkLook*>(&getLookAndFeel());
            if(look)g.setFont(look->font(35));
            g.setColour(ink);g.drawText(i==0?"LOW CUT":"HIGH CUT",110,juce::roundToInt(y-24),180,48,juce::Justification::centred);
        }
    }
    void resized() override
    {
        for(size_t i=0;i<2;++i)values[i].setBounds(304,juce::roundToInt(displayY(i)-27),134,55);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const float d0=std::abs(e.position.y-displayY(0)),d1=std::abs(e.position.y-displayY(1));
        selected=d0<d1?0:1;startY=e.position.y;startFrequency=frequency[static_cast<size_t>(selected)];
        attachments[static_cast<size_t>(selected)]->beginGesture();
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if(selected<0)return;
        const float scale=e.mods.isShiftDown()?.15f:1.0f;
        const double norm=std::log(static_cast<double>(startFrequency)/20.0)/std::log(1000.0)
                         +static_cast<double>((startY-e.position.y)*scale/520.0f);
        attachments[static_cast<size_t>(selected)]->setValueAsPartOfGesture(static_cast<float>(20*std::pow(1000.0,juce::jlimit(0.0,1.0,norm))));
    }
    void mouseUp(const juce::MouseEvent&) override
    {if(selected>=0)attachments[static_cast<size_t>(selected)]->endGesture();selected=-1;}
private:
    float thumbY(size_t i) const {return 552.0f-520.0f*static_cast<float>(std::log(std::clamp(frequency[i],20.0f,20000.0f)/20.0f)/std::log(1000.0f));}
    float displayY(size_t i) const
    {
        const auto a=thumbY(0),b=thumbY(1);
        if(std::abs(a-b)<62){const float middle=juce::jlimit(56.0f,528.0f,(a+b)*.5f);return middle+(i==0?31.0f:-31.0f);}
        return thumbY(i);
    }
    void updateLabels()
    {
        for(size_t i=0;i<2;++i)if(!values[i].isBeingEdited())
        {
            const auto f=frequency[i];
            values[i].setText(f>=1000?juce::String(f/1000,2).trimCharactersAtEnd("0").trimCharactersAtEnd(".")+" kHz":juce::String(juce::roundToInt(f))+" Hz",juce::dontSendNotification);
        }
    }
    std::array<juce::RangedAudioParameter*,2> params{};
    std::array<float,2> frequency{{20,20000}};
    std::array<juce::Label,2> values;
    int selected=-1;float startY=0,startFrequency=20;
    std::array<std::unique_ptr<juce::ParameterAttachment>,2> attachments;
};

class HeadingSwitch final : public juce::Button
{
public:
    explicit HeadingSwitch(const char* label) : Button(label) {setClickingTogglesState(true);}
    void paintButton(juce::Graphics& g,bool over,bool) override
    {
        // Heading lettering comes from the original artwork; this is a quiet ink underline.
        if(getToggleState()||over)line(g,12,static_cast<float>(getHeight()-35),static_cast<float>(getWidth()-12),static_cast<float>(getHeight()-37),4,orange.withAlpha(getToggleState()?1.0f:.35f));
        if(auto* look=dynamic_cast<InkLook*>(&getLookAndFeel()))g.setFont(look->font(23));
        g.setColour(getToggleState()?orange:ink.withAlpha(.55f));
        g.drawText(getToggleState()?"ON":"OFF",getWidth()-45,getHeight()-24,42,24,juce::Justification::centred);
    }
};
}

class AfterglowEditor::SketchPanel final : public juce::Component,private juce::Timer
{
public:
    explicit SketchPanel(AfterglowProcessor& p)
        : state(p.getParameterState()),mix(state,"masterMix"),time(state,"delayTimeMs"),
          feedback(state,"delayFeedback",false),wow(state,"delayWow"),flutter(state,"delayFlutter"),
          filters(state),output(state,"outputGain",false),pingPong(state,"delayPingPong",false)
    {
        setLookAndFeel(&look);
        background=juce::ImageCache::getFromMemory(BinaryData::Faceplate_png,BinaryData::Faceplate_pngSize);
        juce::Component* controls[]{&mix,&time,&feedback,&wow,&flutter,&filters,&sync,&delay,&eq,&settings};
        for(auto* c : controls)addAndMakeVisible(c);
        sync.setComponentID("delaySync");delay.setComponentID("delayEnabled");eq.setComponentID("eqEnabled");
        sync.setTooltip("Match delay time to Ableton's tempo. Drag Time to choose a note; click its value for dotted/triplet options.");
        delay.setTooltip("Click the DELAY heading to switch Delay on or off.");
        eq.setTooltip("Click the EQ heading to switch the whole processed signal EQ on or off.");
        delayAttachment=std::make_unique<State::ButtonAttachment>(state,"delayEnabled",delay);
        eqAttachment=std::make_unique<State::ButtonAttachment>(state,"eqEnabled",eq);
        syncAttachment=std::make_unique<State::ButtonAttachment>(state,"delaySync",sync);
        settings.setComponentID("settings");settings.setTooltip("Output gain and stereo ping-pong settings.");
        settings.onClick=[this]{drawerOpen=!drawerOpen;drawer.setVisible(drawerOpen);output.setVisible(drawerOpen);pingPong.setVisible(drawerOpen);resized();repaint();};
        addChildComponent(output);addChildComponent(pingPong);
        addMouseListener(this,true);
        sync.onClick=[this]{updateSync();};
        updateSync();startTimerHz(20);
        setSize(1664,944);
    }
    ~SketchPanel() override {stopTimer();removeMouseListener(this);setLookAndFeel(nullptr);}
    void paint(juce::Graphics& g) override
    {
        g.drawImage(background,getLocalBounds().toFloat());
        // Settings mark is functional, matching the ink drawing rather than a stock gear.
        g.setColour(ink);g.setFont(look.font(72));g.drawText("*",1568,20,70,70,juce::Justification::centred);
    }
    void paintOverChildren(juce::Graphics& g) override
    {
        if(drawerOpen)
        {
            // Labels remain above the two controls; the paper drawer is painted by a child below them.
            g.setColour(ink);g.setFont(look.font(27));
            g.drawText("OUTPUT",1190,106,160,34,juce::Justification::centredLeft);
            g.drawText("PING-PONG",1190,206,180,34,juce::Justification::centredLeft);
        }
    }
    void resized() override
    {
        mix.setBounds(114,218,166,633);
        time.setBounds(418,318,166,414);
        feedback.setBounds(353,779,409,73);
        wow.setBounds(813,288,166,555);flutter.setBounds(978,288,166,555);
        filters.setBounds(1188,220,450,575);
        sync.setBounds(614,290,146,54);
        delay.setBounds(464,125,230,100);eq.setBounds(1310,125,150,100);
        settings.setBounds(1568,20,70,70);
        drawer.setBounds(1162,92,478,251);output.setBounds(1190,139,420,66);pingPong.setBounds(1190,239,420,66);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if(synced&&e.eventComponent==&time.value)
        {
            // A synced note is a discrete musical choice, not a numeric millisecond field.
            time.value.hideEditor(true);showNotes();
        }
    }
private:
    class PaperDrawer final : public juce::Component
    {void paint(juce::Graphics& g) override {box(g,getLocalBounds().toFloat().reduced(3),true,3);}} drawer;
    void timerCallback() override {updateSync();}
    void updateSync()
    {
        const bool now=state.getRawParameterValue("delaySync")->load()>=.5f;
        if(!initialised||now!=synced)
        {
            synced=now;initialised=true;
            time.format=now?std::function<juce::String(float)>([this](float v)
            {
                const juce::StringArray notes{"1/1","1/2","1/4","1/8","1/16","1/32","1/64"};
                const int style=juce::roundToInt(state.getRawParameterValue("delayTiming")->load());
                return notes[juce::jlimit(0,6,juce::roundToInt(v))]+(style==1?" D":style==2?" T":"");
            }):std::function<juce::String(float)>();
            time.bind(now?"delayDivision":"delayTimeMs");
            time.value.setEditable(false,!now,false);
            time.value.setTooltip(now?"Click to choose a note and Straight, Dotted or Triplet timing.":"Double-click to type milliseconds. Enter applies; Escape cancels.");
        }
        time.refresh();
        if(drawer.getParentComponent()==nullptr)
        {
            addChildComponent(drawer);drawer.toFront(false);output.toFront(false);pingPong.toFront(false);
        }
        drawer.setVisible(drawerOpen);
    }
    void showNotes()
    {
        juce::PopupMenu menu,notes,styles;
        const char* names[]{"1/1","1/2","1/4","1/8","1/16","1/32","1/64"};
        const auto note=juce::roundToInt(state.getRawParameterValue("delayDivision")->load());
        const auto style=juce::roundToInt(state.getRawParameterValue("delayTiming")->load());
        for(int i=0;i<7;++i)notes.addItem(i+1,names[i],true,i==note);
        styles.addItem(20,"Straight",true,style==0);styles.addItem(21,"Dotted",true,style==1);styles.addItem(22,"Triplet",true,style==2);
        menu.addSubMenu("Note",notes);menu.addSubMenu("Timing",styles);
        juce::Component::SafePointer<SketchPanel> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&time.value),[safe](int result)
        {
            if(safe==nullptr||result==0)return;
            auto* param=safe->state.getParameter(result<20?"delayDivision":"delayTiming");
            const float value=static_cast<float>(result<20?result-1:result-20);
            param->beginChangeGesture();param->setValueNotifyingHost(param->convertTo0to1(value));param->endChangeGesture();
            safe->time.refresh();
        });
    }
    State& state;
    InkLook look;
    juce::Image background;
    InkFader mix,time,feedback,wow,flutter;
    InkEQ filters;
    juce::ToggleButton sync{"SYNC"};
    HeadingSwitch delay{"DELAY"},eq{"EQ"};
    juce::TextButton settings;
    InkFader output,pingPong;
    bool synced=false,initialised=false,drawerOpen=false;
    std::unique_ptr<State::ButtonAttachment> delayAttachment,eqAttachment,syncAttachment;
};

AfterglowEditor::AfterglowEditor(AfterglowProcessor& p) : AudioProcessorEditor(p),panel(std::make_unique<SketchPanel>(p))
{
    addAndMakeVisible(*panel);
    setResizable(true,true);setResizeLimits(915,519,1664,944);
    if(auto* c=getConstrainer())c->setFixedAspectRatio(1664.0/944.0);
    setSize(1165,661);
}
AfterglowEditor::~AfterglowEditor()=default;
void AfterglowEditor::resized()
{
    const float scale=juce::jmin(static_cast<float>(getWidth())/1664.0f,static_cast<float>(getHeight())/944.0f);
    panel->setTransform(juce::AffineTransform::scale(scale));panel->setTopLeftPosition(0,0);
}
