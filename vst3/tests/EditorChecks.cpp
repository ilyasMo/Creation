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
        mix->setValue(46.5,juce::sendNotificationSync);
        require(std::abs(state.getRawParameterValue("masterMix")->load()-46.5f)<.11f,"Mix fader did not notify processor");
        mix->setValue(50,juce::sendNotificationSync);
        drag(*mix,{83,250},{83,200});const auto normal=mix->getValue()-50;
        mix->setValue(50,juce::sendNotificationSync);
        drag(*mix,{83,250},{83,200},true);const auto fine=mix->getValue()-50;
        require(normal>1&&fine>0&&fine<normal*.2,"Fader drag/fine drag failed");
        auto* value=dynamic_cast<juce::Label*>(find(*editor,"masterMixValue"));require(value!=nullptr,"Missing mix readout");
        value->setText("70%",juce::sendNotificationSync);
        require(std::abs(state.getRawParameterValue("masterMix")->load()-70)<.11f,"Typed mix value failed");
        auto* cut=dynamic_cast<juce::Label*>(find(*editor,"eqHighCutHzValue"));require(cut!=nullptr,"Missing EQ readout");
        cut->setText("8.5 kHz",juce::sendNotificationSync);
        require(std::abs(state.getRawParameterValue("eqHighCutHz")->load()-8500)<2,"Typed cutoff failed");
        cut->setText("nonsense",juce::sendNotificationSync);
        require(std::abs(state.getRawParameterValue("eqHighCutHz")->load()-8500)<2,"Invalid text changed frequency");
        auto set=[&](const char* id,float v){auto* p=state.getParameter(id);p->setValueNotifyingHost(p->convertTo0to1(v));};
        set("eqLowCutHz",120);set("delayEnabled",1);set("eqEnabled",1);set("delayWow",18);set("delayFlutter",8);
        auto* sync=dynamic_cast<juce::ToggleButton*>(find(*editor,"delaySync"));require(sync!=nullptr,"Missing sync");
        sync->setToggleState(true,juce::sendNotificationSync);sync->onClick();
        require(find(*editor,"delayDivision")!=nullptr,"Time fader did not switch to note divisions");
        auto* note=dynamic_cast<juce::Slider*>(find(*editor,"delayDivision"));note->setValue(3,juce::sendNotificationSync);
        set("delayTiming",1);
        save(*editor,dir.getChildFile("Afterglow-Sketch.png"));
        // Values remain separately reachable if the filter handles occupy the same frequency.
        set("eqLowCutHz",8500);save(*editor,dir.getChildFile("Afterglow-EQ-Overlap.png"));
        auto* low=find(*editor,"eqLowCutHzValue");auto* high=find(*editor,"eqHighCutHzValue");
        require(low!=nullptr&&high!=nullptr&&!low->getBounds().intersects(high->getBounds()),"EQ readouts overlap");
        auto* settings=dynamic_cast<juce::TextButton*>(find(*editor,"settings"));require(settings!=nullptr,"Missing settings");settings->onClick();
        require(find(*editor,"outputGain")->isShowing(),"Output gain is inaccessible");
        save(*editor,dir.getChildFile("Afterglow-Settings.png"));
        editor->setSize(915,519);save(*editor,dir.getChildFile("Afterglow-Small.png"));
        std::cout<<"Native editor binding, typed values, sync switching, EQ overlap and resize checks passed.\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
