#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <utility>
#include <vector>

namespace fancy
{

class FancyProcessor;

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;
    juce::String data {}; // for state beyond the parameters; see FancyProcessor::factoryPresetLoaded
};

// Message-thread only: it touches files and resets parameters.
class PresetManager
{
public:
    PresetManager (FancyProcessor& processor, std::vector<FactoryPreset> factoryPresets);

    // Factory presets first, then user presets in name order. load() takes an index into this.
    juce::StringArray getNames() const;
    juce::StringArray getFactoryNames() const;
    juce::StringArray getUserNames() const;

    // The loaded preset's name, or "Default" when none has been loaded.
    juce::String getCurrentName() const;

    // Whether anything has changed since the preset loaded, or since the defaults.
    bool isModified() const;

    // Whether the loaded preset is a user preset, which can be renamed and deleted.
    bool isUserPresetLoaded() const;

    void load (int index);

    // Loads the current preset again, discarding changes.
    void reload();

    bool saveUser (const juce::String& name);
    bool renameUser (const juce::String& newName);
    bool deleteUser();

    // Whether a factory or user preset already has this name.
    bool nameExists (const juce::String& name) const;

    juce::File getUserDirectory() const;
    void openUserDirectory() const;

    // Called by FancyProcessor around saving and restoring a project.
    void addToState (juce::ValueTree& state) const;
    void stateRestored();

private:
    juce::Array<juce::File> getUserFiles() const;
    juce::File getUserFile (const juce::String& name) const;
    void loadFactory (const FactoryPreset& preset);
    void setCurrent (const juce::String& name, bool isUser);
    void takeSnapshot();
    void takeDefaultSnapshot();

    FancyProcessor& owner;
    std::vector<FactoryPreset> factory;

    std::vector<float> reference;
    int referenceEdits = 0;
    bool modifiedWhenSaved = false;
};

} // namespace fancy
