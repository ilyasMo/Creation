#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>

juce::Component* find(juce::Component& root,const juce::String& id)
{
    if(root.getComponentID()==id)return &root;
    for(auto* child:root.getChildren())if(auto* match=find(*child,id))return match;
    return nullptr;
}
void require(bool condition,const char* text) {if(!condition)throw std::runtime_error(text);}
void save(juce::Component& editor,const juce::File& file)
{
    file.deleteFile();auto out=file.createOutputStream();
    require(out!=nullptr,"Cannot create screenshot");
    juce::PNGImageFormat png;
    require(png.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()),*out),"Cannot encode screenshot");
}
void typeAndCommit(juce::Label& label,const juce::String& text)
{
    label.showEditor();auto* entry=label.getCurrentTextEditor();require(entry!=nullptr,"Numeric field did not open");
    entry->setText(text,false);
    // Label's normal modal-input commit invokes its Return handler, including the editor-hide callbacks.
    static_cast<juce::Component&>(label).inputAttemptWhenModal();
    require(!label.isBeingEdited(),"Numeric editor did not close after commit");
}
void drag(juce::Component& control,juce::Point<float> start,juce::Point<float> finish,bool fine=false)
{
    const auto source=juce::Desktop::getInstance().getMainMouseSource();
    const auto now=juce::Time::getCurrentTime();
    const juce::ModifierKeys modifiers(juce::ModifierKeys::leftButtonModifier|(fine?juce::ModifierKeys::shiftModifier:0));
    const juce::MouseEvent down(source,start,modifiers,1,0,0,0,0,&control,&control,now,start,now,1,false);
    control.mouseDown(down);
    const juce::MouseEvent moved(source,finish,modifiers,1,0,0,0,0,&control,&control,now,start,now,1,true);
    control.mouseDrag(moved);control.mouseUp(moved);
}
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        const auto dir=argc>1?juce::File(argv[1]):juce::File::getCurrentWorkingDirectory();dir.createDirectory();
        AfterglowProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        editor->setVisible(true);editor->setSize(1664,944);save(*editor,dir.getChildFile("Afterglow-Initial.png"));
        auto& state=processor.getParameterState();
        auto* mix=dynamic_cast<juce::Slider*>(find(*editor,"masterMix"));require(mix!=nullptr,"Missing native mix fader");
        for(const char* id:{"masterMix","delayTimeMs","delayWow","delayFlutter","delayFeedback"})
        {
            auto* fader=dynamic_cast<juce::Slider*>(find(*editor,id));require(fader!=nullptr,"Missing fader");
            const auto layout=fader->getLookAndFeel().getSliderLayout(*fader);
            const auto r=layout.sliderBounds;
            if(fader->isVertical())require(r.getY()-28>=0&&r.getBottom()+28<=fader->getHeight(),"Fader cap escapes rail/component at an endpoint");
            else require(r.getX()-17>=0&&r.getRight()+17<=fader->getWidth(),"Horizontal cap escapes rail/component");
        }
        auto* noteMenu=dynamic_cast<juce::ComboBox*>(find(*editor,"delayDivisionMenu"));
        auto* styleMenu=dynamic_cast<juce::ComboBox*>(find(*editor,"delayTimingMenu"));
        require(noteMenu&&styleMenu&&!noteMenu->isEnabled()&&!styleMenu->isEnabled(),"Note menus must be disabled before Sync");
        mix->setValue(0,juce::sendNotificationSync);save(*editor,dir.getChildFile("Afterglow-Mix-Min.png"));
        mix->setValue(100,juce::sendNotificationSync);save(*editor,dir.getChildFile("Afterglow-Mix-Max.png"));
        mix->setValue(46.5,juce::sendNotificationSync);
        require(std::abs(state.getRawParameterValue("masterMix")->load()-46.5f)<.11f,"Mix fader did not notify processor");
        mix->setValue(50,juce::sendNotificationSync);
        drag(*mix,{83,250},{83,200});const auto normal=mix->getValue()-50;
        mix->setValue(50,juce::sendNotificationSync);
        drag(*mix,{83,250},{83,200},true);const auto fine=mix->getValue()-50;
        require(normal>1&&fine>0&&fine<normal*.2,"Fader drag/fine drag failed");
        auto* value=dynamic_cast<juce::Label*>(find(*editor,"masterMixValue"));require(value!=nullptr,"Missing mix readout");
        value->showEditor();auto* entry=value->getCurrentTextEditor();require(entry!=nullptr,"No numeric TextEditor");
        require(entry->findColour(juce::TextEditor::textColourId).getBrightness()<.3f,"Typing colour is too light");
        require(entry->findColour(juce::TextEditor::highlightedTextColourId).getBrightness()<.3f,"Selected typing colour is too light");
        entry->setText("60",false);save(*editor,dir.getChildFile("Afterglow-Typing.png"));
        static_cast<juce::Component*>(value)->inputAttemptWhenModal();
        require(std::abs(state.getRawParameterValue("masterMix")->load()-60)<.11f,"Return-handler commit reverted typed value");
        typeAndCommit(*value,"64,5%");
        require(std::abs(state.getRawParameterValue("masterMix")->load()-64.5f)<.11f,"Decimal comma entry failed");
        value->showEditor();value->getCurrentTextEditor()->setText("20",false);value->hideEditor(true);
        require(std::abs(state.getRawParameterValue("masterMix")->load()-64.5f)<.11f,"Cancelled edit changed parameter");
        typeAndCommit(*value,"70%");
        require(std::abs(state.getRawParameterValue("masterMix")->load()-70)<.11f,"Typed mix value failed");
        auto* cut=dynamic_cast<juce::Label*>(find(*editor,"eqHighCutHzValue"));require(cut!=nullptr,"Missing EQ readout");
        typeAndCommit(*cut,"8.5 kHz");
        require(std::abs(state.getRawParameterValue("eqHighCutHz")->load()-8500)<2,"Typed cutoff failed");
        typeAndCommit(*cut,"nonsense");
        require(std::abs(state.getRawParameterValue("eqHighCutHz")->load()-8500)<2,"Invalid text changed frequency");
        auto set=[&](const char* id,float v){auto* p=state.getParameter(id);p->setValueNotifyingHost(p->convertTo0to1(v));};
        set("eqLowCutHz",120);set("delayEnabled",1);set("eqEnabled",1);set("delayWow",18);set("delayFlutter",8);
        auto* tape=dynamic_cast<juce::Button*>(find(*editor,"tapeEnabled"));require(tape!=nullptr,"Missing Tape heading bypass");
        tape->setToggleState(true,juce::sendNotificationSync);
        require(state.getRawParameterValue("tapeEnabled")->load()>.5f,"Tape heading did not enable modulation");
        tape->setToggleState(false,juce::sendNotificationSync);
        require(state.getRawParameterValue("tapeEnabled")->load()<.5f&&state.getRawParameterValue("delayWow")->load()==18,"Tape bypass lost its amount");
        tape->setToggleState(true,juce::sendNotificationSync);
        auto* sync=dynamic_cast<juce::ToggleButton*>(find(*editor,"delaySync"));require(sync!=nullptr,"Missing sync");
        sync->setToggleState(true,juce::sendNotificationSync);sync->onClick();
        require(find(*editor,"delayDivision")!=nullptr,"Time fader did not switch to note divisions");
        require(noteMenu->isEnabled()&&styleMenu->isEnabled(),"Sync did not enable note/style selectors");
        noteMenu->setSelectedId(4,juce::sendNotificationSync);styleMenu->setSelectedId(2,juce::sendNotificationSync);
        require(state.getRawParameterValue("delayDivision")->load()==3&&state.getRawParameterValue("delayTiming")->load()==1,"Note menus did not notify processor");
        sync->onClick();
        juce::MemoryBlock saved;processor.getStateInformation(saved);
        AfterglowProcessor restored;restored.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
        require(restored.getParameterState().getRawParameterValue("tapeEnabled")->load()>.5f,"Tape bypass did not restore");
        auto oldState=state.copyState();oldState.removeChild(oldState.getChildWithProperty("id","tapeEnabled"),nullptr);
        juce::MemoryBlock legacy;juce::AudioProcessor::copyXmlToBinary(*oldState.createXml(),legacy);
        AfterglowProcessor migrated;migrated.setStateInformation(legacy.getData(),static_cast<int>(legacy.getSize()));
        require(migrated.getParameterState().getRawParameterValue("tapeEnabled")->load()>.5f,"Old nonzero tape settings lost modulation");
        save(*editor,dir.getChildFile("Afterglow-Sketch.png"));
        // Values remain separately reachable if the filter handles occupy the same frequency.
        set("eqLowCutHz",8500);save(*editor,dir.getChildFile("Afterglow-EQ-Overlap.png"));
        auto* low=find(*editor,"eqLowCutHzValue");auto* high=find(*editor,"eqHighCutHzValue");
        require(low!=nullptr&&high!=nullptr&&!low->getBounds().intersects(high->getBounds()),"EQ readouts overlap");
        auto* settings=dynamic_cast<juce::TextButton*>(find(*editor,"settings"));require(settings!=nullptr,"Missing settings");settings->onClick();
        require(find(*editor,"outputGain")->isVisible() && find(*editor,"outputGain")->getParentComponent()->isVisible(),"Output gain is inaccessible");
        save(*editor,dir.getChildFile("Afterglow-Settings.png"));
        settings->onClick();set("eqLowCutHz",120);
        editor->setSize(915,519);save(*editor,dir.getChildFile("Afterglow-Small.png"));
        std::cout<<"Native editor bindings, Return/cancel entry, contrast, fader limits, Tape bypass, note menus, EQ overlap and resize checks passed.\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
