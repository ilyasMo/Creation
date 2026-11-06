#include "PluginProcessor.h"
#include "ParameterText.h"
#include <algorithm>
#include <vector>

AfterglowProcessor::~AfterglowProcessor()
{
    for(auto* parameter:getParameters())
        if(auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(parameter))parameters.removeParameterListener(ranged->paramID,this);
}
void AfterglowProcessor::parameterChanged(const juce::String&,float)
{if(!applyingPreset.load())presetDirty.store(true);}

juce::Point<int> AfterglowProcessor::editorSize() const noexcept
{
    const int width=editorWidth.load();return {width,juce::roundToInt(width*944.0/1664.0)};
}
void AfterglowProcessor::rememberEditorWidth(int width,bool notifyHost) noexcept
{
    const int clamped=juce::jlimit(915,1664,width);
    if(editorWidth.exchange(clamped)!=clamped&&notifyHost)
        updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged(true));
}

juce::String AfterglowProcessor::currentPresetName() const
{const juce::ScopedLock lock(presetMetadataLock);return presetName;}
juce::String AfterglowProcessor::presetDisplayName() const
{return currentPresetName()+(presetDirty.load()?" *":"");}
void AfterglowProcessor::setPresetIdentity(const juce::String& name,bool dirty)
{const juce::ScopedLock lock(presetMetadataLock);presetName=name;presetDirty.store(dirty);}
bool AfterglowProcessor::parametersAreDefault() const
{
    for(auto* parameter:getParameters())
        if(std::abs(parameter->getValue()-parameter->getDefaultValue())>1e-6f)return false;
    return true;
}
juce::Array<juce::File> AfterglowProcessor::userPresets() const
{
    auto files=presetDirectory.findChildFiles(juce::File::findFiles,false,"*.afterglowpreset");
    std::sort(files.begin(),files.end(),[](const auto& a,const auto& b)
        {return a.getFileNameWithoutExtension().compareNatural(b.getFileNameWithoutExtension())<0;});
    return files;
}
void AfterglowProcessor::loadDefaultPreset()
{
    applyingPreset.store(true);
    for(auto* parameter:getParameters())
    {
        parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->getDefaultValue());parameter->endChangeGesture();
    }
    applyingPreset.store(false);setPresetIdentity("Default",false);updateHostDisplay();
}

juce::Result AfterglowProcessor::savePresetAs(const juce::String& requested)
{
    const auto name=requested.trim();
    if(name.isEmpty()||name.length()>80)return juce::Result::fail("Enter a preset name (up to 80 characters).");
    if(name.equalsIgnoreCase("Default"))return juce::Result::fail("Default is reserved. Choose a custom preset name.");
    if(name.containsAnyOf("\\/:*?\"<>|")||name.endsWithChar('.'))return juce::Result::fail("Use a name without file-path characters or a trailing dot.");
    for(auto c:name)if(c<32||c==127)return juce::Result::fail("Use a name without control characters.");
    const auto base=name.upToFirstOccurrenceOf(".",false,false).toUpperCase();
    const juce::StringArray reserved{"CON","PRN","AUX","NUL","COM1","COM2","COM3","COM4","COM5","COM6","COM7","COM8","COM9","LPT1","LPT2","LPT3","LPT4","LPT5","LPT6","LPT7","LPT8","LPT9"};
    if(reserved.contains(base))return juce::Result::fail("That filename is reserved. Choose a different name.");
    for(const auto& existing:userPresets())if(existing.getFileNameWithoutExtension().equalsIgnoreCase(name))
        return juce::Result::fail("A preset with that name already exists. Choose another name.");
    const auto directoryResult=presetDirectory.createDirectory();if(directoryResult.failed())return directoryResult;
    const auto file=presetDirectory.getChildFile(name+".afterglowpreset");
    juce::ValueTree snapshot("AfterglowState");
    const auto current=parameters.copyState();
    for(auto* parameter:getParameters())if(auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(parameter))
    {
        const auto child=current.getChildWithProperty("id",ranged->paramID);
        if(child.isValid())snapshot.appendChild(child.createCopy(),nullptr);
    }
    juce::XmlElement xml("AFTERGLOW_PRESET");xml.setAttribute("formatVersion",1);xml.setAttribute("name",name);
    xml.addChildElement(snapshot.createXml().release());
    juce::TemporaryFile temporary(file);
    if(!xml.writeTo(temporary.getFile())||!temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not write this preset. Check the preset folder is writable.");
    setPresetIdentity(name,false);updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged(true));return juce::Result::ok();
}

juce::Result AfterglowProcessor::applyPresetParameters(const juce::ValueTree& snapshot,const juce::String& name)
{
    if(!snapshot.isValid()||snapshot.getType()!=parameters.state.getType())return juce::Result::fail("This preset has no valid Afterglow settings.");
    std::vector<std::pair<juce::RangedAudioParameter*,float>> values;
    int found=0;
    for(auto* parameter:getParameters())if(auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(parameter))
    {
        float normalised=ranged->getDefaultValue();const auto child=snapshot.getChildWithProperty("id",ranged->paramID);
        if(child.isValid())
        {
            double value=0;const auto& range=ranged->getNormalisableRange();
            if(!afterglow::parseParameterText(child.getProperty("value").toString().toStdString(),"",value)
                ||value<range.start-1e-5||value>range.end+1e-5)return juce::Result::fail("This preset contains an invalid parameter value.");
            normalised=ranged->convertTo0to1(range.snapToLegalValue(static_cast<float>(value)));++found;
        }
        values.emplace_back(ranged,normalised);
    }
    if(found==0)return juce::Result::fail("This preset contains no recognized settings.");
    applyingPreset.store(true);
    for(const auto& value:values)
    {value.first->beginChangeGesture();value.first->setValueNotifyingHost(value.second);value.first->endChangeGesture();}
    applyingPreset.store(false);setPresetIdentity(name,false);updateHostDisplay();return juce::Result::ok();
}
juce::Result AfterglowProcessor::loadPreset(const juce::File& file)
{
    if(!file.existsAsFile()||file.getSize()>1024*1024)return juce::Result::fail("The preset is missing or too large.");
    const auto xml=juce::XmlDocument::parse(file);
    if(!xml||!xml->hasTagName("AFTERGLOW_PRESET")||xml->getIntAttribute("formatVersion")!=1)
        return juce::Result::fail("This is not a supported Afterglow preset.");
    const auto* settings=xml->getChildByName("AfterglowState");
    if(settings==nullptr)return juce::Result::fail("This preset has no audio settings.");
    auto name=xml->getStringAttribute("name",file.getFileNameWithoutExtension()).trim().substring(0,80);
    if(name.isEmpty())name=file.getFileNameWithoutExtension();
    return applyPresetParameters(juce::ValueTree::fromXml(*settings),name);
}
