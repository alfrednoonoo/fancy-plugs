#include <fancy/plugin/FancyProcessor.h>

#include <cmath>

namespace fancy
{

namespace
{
const juce::Identifier presetNameProperty ("presetName");
const juce::Identifier presetUserProperty ("presetUser");
const juce::Identifier presetModifiedProperty ("presetModified");
const juce::String defaultName ("Default");
const juce::String presetExtension (".fancypreset");

// Smaller than any change a control can make, larger than float rounding through the host.
constexpr float changeTolerance = 1.0e-5f;
} // namespace

PresetManager::PresetManager (FancyProcessor& processor, std::vector<FactoryPreset> factoryPresets)
    : owner (processor), factory (std::move (factoryPresets))
{
    takeDefaultSnapshot();
}

juce::File PresetManager::getUserDirectory() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Fancy Plugs")
        .getChildFile (owner.getName())
        .getChildFile ("Presets");
}

void PresetManager::openUserDirectory() const
{
    const auto directory = getUserDirectory();

    if (directory.createDirectory())
        directory.startAsProcess();
}

juce::Array<juce::File> PresetManager::getUserFiles() const
{
    auto files = getUserDirectory().findChildFiles (juce::File::findFiles, false, "*" + presetExtension);
    files.sort();
    return files;
}

juce::File PresetManager::getUserFile (const juce::String& name) const
{
    return getUserDirectory().getChildFile (name + presetExtension);
}

juce::StringArray PresetManager::getFactoryNames() const
{
    juce::StringArray names;

    for (const auto& preset : factory)
        names.add (preset.name);

    return names;
}

juce::StringArray PresetManager::getUserNames() const
{
    juce::StringArray names;

    for (const auto& file : getUserFiles())
        names.add (file.getFileNameWithoutExtension());

    return names;
}

juce::StringArray PresetManager::getNames() const
{
    auto names = getFactoryNames();
    names.addArray (getUserNames());
    return names;
}

juce::String PresetManager::getCurrentName() const
{
    const auto name = owner.getApvts().state.getProperty (presetNameProperty).toString();
    return name.isEmpty() ? defaultName : name;
}

bool PresetManager::isUserPresetLoaded() const
{
    const auto& state = owner.getApvts().state;
    const auto name = state.getProperty (presetNameProperty).toString();

    if (name.isEmpty() || ! getUserFile (name).existsAsFile())
        return false;

    // Projects saved before v1.2 don't say which kind it was.
    if (state.hasProperty (presetUserProperty))
        return (bool) state.getProperty (presetUserProperty);

    return ! getFactoryNames().contains (name);
}

bool PresetManager::nameExists (const juce::String& name) const
{
    const auto trimmed = name.trim();
    return getFactoryNames().contains (trimmed, true) || getUserNames().contains (trimmed, true);
}

void PresetManager::setCurrent (const juce::String& name, bool isUser)
{
    auto& state = owner.getApvts().state;
    state.setProperty (presetNameProperty, name, nullptr);
    state.setProperty (presetUserProperty, isUser, nullptr);
    state.removeProperty (presetModifiedProperty, nullptr);
}

void PresetManager::takeSnapshot()
{
    reference.clear();

    for (auto* parameter : owner.getParameters())
        reference.push_back (parameter == owner.getBypassParameter() ? std::nanf ("") : parameter->getValue());

    referenceEdits = owner.getEditCount();
    modifiedWhenSaved = false;
}

void PresetManager::takeDefaultSnapshot()
{
    reference.clear();

    for (auto* parameter : owner.getParameters())
        reference.push_back (parameter == owner.getBypassParameter() ? std::nanf ("") : parameter->getDefaultValue());

    referenceEdits = owner.getEditCount();
    modifiedWhenSaved = false;
}

bool PresetManager::isModified() const
{
    if (modifiedWhenSaved || owner.getEditCount() != referenceEdits)
        return true;

    const auto& parameters = owner.getParameters();

    for (int i = 0; i < parameters.size() && i < (int) reference.size(); ++i)
        if (! std::isnan (reference[(size_t) i]) && std::abs (parameters[i]->getValue() - reference[(size_t) i]) > changeTolerance)
            return true;

    return false;
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

    const FancyProcessor::UserEdit edit (owner, "Load preset");

    if (index < (int) factory.size())
    {
        loadFactory (factory[(size_t) index]);
        owner.factoryPresetLoaded (factory[(size_t) index].data);
        setCurrent (factory[(size_t) index].name, false);
        takeSnapshot();
        return;
    }

    const auto files = getUserFiles();
    const auto userIndex = index - (int) factory.size();

    if (userIndex >= files.size())
        return;

    const auto file = files[userIndex];

    if (const auto xml = juce::XmlDocument::parse (file))
    {
        if (owner.applyState (juce::ValueTree::fromXml (*xml)))
        {
            setCurrent (file.getFileNameWithoutExtension(), true);
            takeSnapshot();
        }
    }
}

void PresetManager::reload()
{
    const auto name = getCurrentName();

    if (isUserPresetLoaded())
    {
        const auto userIndex = getUserNames().indexOf (name);

        if (userIndex >= 0)
            load ((int) factory.size() + userIndex);

        return;
    }

    load (juce::jmax (0, getFactoryNames().indexOf (name)));
}

bool PresetManager::saveUser (const juce::String& name)
{
    const auto legalName = juce::File::createLegalFileName (name).trim();

    if (legalName.isEmpty())
        return false;

    const auto directory = getUserDirectory();

    if (! directory.createDirectory())
        return false;

    setCurrent (legalName, true);

    auto state = owner.getApvts().copyState();
    state.setProperty ("stateVersion", FancyProcessor::stateVersion, nullptr);
    owner.addToState (state);

    const auto xml = state.createXml();

    if (xml == nullptr || ! xml->writeTo (getUserFile (legalName)))
        return false;

    takeSnapshot();
    return true;
}

bool PresetManager::renameUser (const juce::String& newName)
{
    if (! isUserPresetLoaded())
        return false;

    const auto legalName = juce::File::createLegalFileName (newName).trim();
    const auto current = getCurrentName();

    if (legalName.isEmpty() || (legalName != current && nameExists (legalName)))
        return false;

    if (legalName == current)
        return true;

    if (! getUserFile (current).moveFileTo (getUserFile (legalName)))
        return false;

    owner.getApvts().state.setProperty (presetNameProperty, legalName, nullptr);
    return true;
}

bool PresetManager::deleteUser()
{
    if (! isUserPresetLoaded())
        return false;

    if (! getUserFile (getCurrentName()).deleteFile())
        return false;

    // The settings stay as they are, now measured against the defaults.
    setCurrent ({}, false);
    takeDefaultSnapshot();
    return true;
}

void PresetManager::addToState (juce::ValueTree& state) const
{
    state.setProperty (presetModifiedProperty, isModified(), nullptr);
}

void PresetManager::stateRestored()
{
    auto& state = owner.getApvts().state;
    const auto wasModified = (bool) state.getProperty (presetModifiedProperty, false);
    state.removeProperty (presetModifiedProperty, nullptr);

    if (state.getProperty (presetNameProperty).toString().isEmpty())
        takeDefaultSnapshot();
    else
        takeSnapshot();

    modifiedWhenSaved = wasModified;
}

} // namespace fancy
