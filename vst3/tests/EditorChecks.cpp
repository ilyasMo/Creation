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
struct HostStateObserver final : juce::AudioProcessorListener
{
    juce::AudioProcessor& processor;bool notified=false;
    explicit HostStateObserver(juce::AudioProcessor& p):processor(p){processor.addListener(this);}
    ~HostStateObserver() override{processor.removeListener(this);}
    void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {}
    void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails& details) override
    {if(details.nonParameterStateChanged)notified=true;}
};
struct PresetFixture
{
    juce::File directory=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("afterglow-preset-checks", "", false);
    PresetFixture(){require(directory.createDirectory().wasOk(),"Cannot create preset fixture");}
    ~PresetFixture(){directory.deleteRecursively();}
};
void checkPresetWorkflow(const juce::File& pictures)
{
    PresetFixture fixture;
    AfterglowProcessor processor(fixture.directory);
    HostStateObserver host(processor);
    auto& state=processor.getParameterState();
    auto set=[&](const char* id,float v){auto* p=state.getParameter(id);p->setValueNotifyingHost(p->convertTo0to1(v));};
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());editor->setVisible(true);
    require(editor->getWidth()==1165&&editor->getHeight()==661,"Fresh editor size changed");
    editor->setSize(1250,709);require(host.notified,"Resize did not notify host to save non-parameter state");editor.reset();
    editor.reset(processor.createEditor());editor->setVisible(true);
    require(editor->getWidth()==1250&&editor->getHeight()==709,"Editor size resets after close/reopen");
    AfterglowProcessor other(fixture.directory);std::unique_ptr<juce::AudioProcessorEditor> otherEditor(other.createEditor());
    otherEditor->setSize(915,519);
    require(processor.editorSize().x==1250,"Another plugin instance changed this instance's size");
    juce::MemoryBlock dimensions;processor.getStateInformation(dimensions);
    AfterglowProcessor restoredSize(fixture.directory);restoredSize.setStateInformation(dimensions.getData(),static_cast<int>(dimensions.getSize()));
    std::unique_ptr<juce::AudioProcessorEditor> restoredEditor(restoredSize.createEditor());
    require(restoredEditor->getWidth()==1250&&restoredEditor->getHeight()==709,"Project recall did not restore editor size");
    auto* menu=dynamic_cast<juce::ComboBox*>(find(*editor,"presetMenu"));
    auto* saveAs=dynamic_cast<juce::TextButton*>(find(*editor,"savePresetAs"));
    require(menu&&saveAs&&menu->getNumItems()==1&&menu->getText()=="Default","Missing fresh Default preset menu");
    set("inputGain",3);set("outputGain",-5);set("tapeEnabled",1);set("delayWow",23);set("eqEnabled",1);set("eqLowCutHz",140);set("masterMix",72);
    saveAs->onClick();auto* prompt=find(*editor,"savePresetPrompt");
    auto* name=dynamic_cast<juce::TextEditor*>(find(*editor,"presetNameEntry"));
    auto* confirm=dynamic_cast<juce::TextButton*>(find(*editor,"savePresetConfirm"));
    require(prompt&&prompt->isVisible()&&name&&confirm,"Save As dialog does not open");
    name->setText("Warm piano",false);save(*editor,pictures.getChildFile("Afterglow-Save-As.png"));confirm->onClick();
    require(!prompt->isVisible()&&processor.currentPresetName()=="Warm piano","Save As did not complete");
    const auto presets=processor.userPresets();require(presets.size()==1&&menu->getNumItems()==2,"Saved preset is missing from menu/disk");
    const auto originalFile=presets[0].loadFileAsString();
    require(processor.savePresetAs("Warm piano").failed()&&presets[0].loadFileAsString()==originalFile,"Save As overwrote an existing preset");
    require(processor.savePresetAs("Default").failed(),"Factory Default can be overwritten");
    require(processor.savePresetAs("../bad").failed(),"Preset name can escape its directory");
    menu->setSelectedId(1,juce::sendNotificationSync);
    require(processor.currentPresetName()=="Default","Default menu selection failed");
    for(auto* parameter:processor.getParameters())require(std::abs(parameter->getValue()-parameter->getDefaultValue())<1e-6f,"Default does not restore startup parameters");
    require(state.getRawParameterValue("masterMix")->load()==100&&state.getRawParameterValue("inputGain")->load()==0,"Default blend/gain is wrong");
    require(editor->getWidth()==1250,"Loading Default changed editor size");
    menu->setSelectedId(2,juce::sendNotificationSync);
    require(processor.currentPresetName()=="Warm piano"&&state.getRawParameterValue("inputGain")->load()==3&&state.getRawParameterValue("outputGain")->load()==-5,"Custom preset did not reload gain settings");
    require(state.getRawParameterValue("tapeEnabled")->load()>.5f&&state.getRawParameterValue("eqLowCutHz")->load()==140,"Custom preset did not reload effect settings");
    require(editor->getWidth()==1250,"Loading custom preset resized the editor");
    save(*editor,pictures.getChildFile("Afterglow-Presets.png"));
    AfterglowProcessor fresh(fixture.directory);require(fresh.loadPreset(presets[0]).wasOk(),"Preset cannot load in a fresh processor");
    require(fresh.getParameterState().getRawParameterValue("masterMix")->load()==72&&fresh.editorSize().x==1165,"Preset audio/geometry separation failed");
    set("delayWow",27);require(processor.presetDisplayName().endsWith(" *"),"Modified preset is not marked dirty");
    juce::MemoryBlock project;processor.getStateInformation(project);
    AfterglowProcessor portable(fixture.directory.getChildFile("empty-bank"));portable.setStateInformation(project.getData(),static_cast<int>(project.getSize()));
    require(portable.currentPresetName()=="Warm piano"&&portable.getParameterState().getRawParameterValue("delayWow")->load()==27&&portable.editorSize().x==1250,"Project needs the custom preset file to restore settings");
    const auto malformed=fixture.directory.getChildFile("Broken.afterglowpreset");malformed.replaceWithText("not a preset");
    require(processor.loadPreset(malformed).failed()&&state.getRawParameterValue("outputGain")->load()==-5,"Broken preset changed audio settings");
    malformed.replaceWithText("<AFTERGLOW_PRESET formatVersion=\"1\" name=\"Broken\"><AfterglowState><PARAM id=\"outputGain\" value=\"nan\"/></AfterglowState></AFTERGLOW_PRESET>");
    require(processor.loadPreset(malformed).failed()&&state.getRawParameterValue("outputGain")->load()==-5,"Invalid numeric preset changed audio settings");
    auto* settings=dynamic_cast<juce::TextButton*>(find(*editor,"settings"));settings->onClick();
    require(find(*editor,"delayPingPong")==nullptr,"Ping-pong is still in the settings menu");
    require(find(*editor,"inputGain")->getParentComponent()->isVisible()&&find(*editor,"outputGain")->getParentComponent()->isVisible(),"Input/Output controls are inaccessible");
    save(*editor,pictures.getChildFile("Afterglow-Input-Output.png"));
    settings->onClick();processor.loadDefaultPreset();
    auto* cancel=dynamic_cast<juce::TextButton*>(find(*editor,"savePresetCancel"));saveAs->onClick();cancel->onClick();
    require(!prompt->isVisible(),"Save As cancel does not close the dialog");
    // Real processor check: +6 dB input and -6 dB output must cancel with all effects off.
    set("inputGain",6);set("outputGain",-6);processor.prepareToPlay(48000,4096);
    juce::AudioBuffer<float> audio(2,4096);std::vector<float> source(4096);
    for(int i=0;i<4096;++i){source[static_cast<size_t>(i)]=.1f*std::sin(i*.06f);audio.setSample(0,i,source[static_cast<size_t>(i)]);audio.setSample(1,i,-source[static_cast<size_t>(i)]);}
    juce::MidiBuffer midi;processor.processBlock(audio,midi);
    for(int i=288;i<4096;++i)require(std::abs(audio.getSample(0,i)-source[static_cast<size_t>(i-288)])<2e-6f,"Input/Output gain processing does not match controls");
    set("outputGain",0);processor.prepareToPlay(48000,4096);
    for(int i=0;i<4096;++i){audio.setSample(0,i,source[static_cast<size_t>(i)]);audio.setSample(1,i,source[static_cast<size_t>(i)]);}
    processor.processBlock(audio,midi);const float gain=juce::Decibels::decibelsToGain(6.0f);
    for(int i=288;i<4096;++i)require(std::abs(audio.getSample(0,i)-gain*source[static_cast<size_t>(i-288)])<2e-6f,"Input gain has no audible processing effect");
    std::cout<<"Preset Save As/load/default, input/output audio, project state and editor reopen/resize checks passed.\n";
}

int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        const auto dir=argc>1?juce::File(argv[1]):juce::File::getCurrentWorkingDirectory();dir.createDirectory();
        checkPresetWorkflow(dir);
        AfterglowProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        editor->setVisible(true);editor->setSize(1664,944);save(*editor,dir.getChildFile("Afterglow-Initial.png"));
        processor.prepareToPlay(48000,512);
        require(processor.getLatencySamples()==288,"Host latency does not match Tape dry reference");
        juce::AudioBuffer<float> bypassAudio(2,512);bypassAudio.clear();bypassAudio.setSample(0,0,1);bypassAudio.setSample(1,0,1);
        juce::MidiBuffer midi;processor.processBlockBypassed(bypassAudio,midi);
        require(bypassAudio.getSample(0,288)==1&&bypassAudio.getSample(0,0)==0,"Host bypass is not latency aligned");
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
