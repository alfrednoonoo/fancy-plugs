#include <fancy/plugin/FancyProcessor.h>

namespace fancy
{

namespace
{
const juce::Identifier presetNameProperty ("presetName");
const juce::String noPreset ("No preset");
const juce::String presetExtension (".fancypreset");
} // namespace

PresetManager::PresetManager (FancyProcessor& processor, std::vector<FactoryPreset> factoryPresets)
    : owner (processor), factory (std::move (factoryPresets))
{
}

juce::File PresetManager::getUserDirectory() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Fancy Plugs")
        .getChildFile (owner.getName())
        .getChildFile ("Presets");
}

juce::Array<juce::File> PresetManager::getUserFiles() const
{
    auto files = getUserDirectory().findChildFiles (juce::File::findFiles, false, "*" + presetExtension);
    files.sort();
    return files;
}

juce::StringArray PresetManager::getNames() const
{
    juce::StringArray names;

    for (const auto& preset : factory)
        names.add (preset.name);

    for (const auto& file : getUserFiles())
        names.add (file.getFileNameWithoutExtension());

    return names;
}

juce::String PresetManager::getCurrentName() const
{
    const auto name = owner.getApvts().state.getProperty (presetNameProperty).toString();
    return name.isEmpty() ? noPreset : name;
}

void PresetManager::setCurrentName (const juce::String& name)
{
    owner.getApvts().state.setProperty (presetNameProperty, name, nullptr);
}

void PresetManager::loadFactory (const FactoryPreset& preset)
{
    for (auto* parameter : owner.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);

        if (ranged == nullptr || parameter == owner.getBypassParameter())
            continue;

        auto normalised = ranged->getDefaultValue();

        for (const auto& [id, value] : preset.values)
            if (id == ranged->getParameterID())
                normalised = ranged->convertTo0to1 (value);

        ranged->setValueNotifyingHost (normalised);
    }
}

void PresetManager::load (int index)
{
    if (index < 0)
        return;

    if (index < (int) factory.size())
    {
        loadFactory (factory[(size_t) index]);
        owner.factoryPresetLoaded (factory[(size_t) index].data);
        setCurrentName (factory[(size_t) index].name);
        return;
    }

    const auto files = getUserFiles();
    const auto userIndex = index - (int) factory.size();

    if (userIndex >= files.size())
        return;

    const auto file = files[userIndex];

    if (const auto xml = juce::XmlDocument::parse (file))
        if (owner.applyState (juce::ValueTree::fromXml (*xml)))
            setCurrentName (file.getFileNameWithoutExtension());
}

bool PresetManager::saveUser (const juce::String& name)
{
    const auto legalName = juce::File::createLegalFileName (name).trim();

    if (legalName.isEmpty())
        return false;

    const auto directory = getUserDirectory();

    if (! directory.createDirectory())
        return false;

    setCurrentName (legalName);

    auto state = owner.getApvts().copyState();
    state.setProperty ("stateVersion", FancyProcessor::stateVersion, nullptr);
    owner.addToState (state);

    const auto xml = state.createXml();
    return xml != nullptr && xml->writeTo (directory.getChildFile (legalName + presetExtension));
}

} // namespace fancy
