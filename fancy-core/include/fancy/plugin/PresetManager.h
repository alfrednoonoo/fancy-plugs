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
};

// Message-thread only: it touches files and resets parameters.
class PresetManager
{
public:
    PresetManager (FancyProcessor& processor, std::vector<FactoryPreset> factoryPresets);

    juce::StringArray getNames() const;
    juce::String getCurrentName() const;

    void load (int index);
    bool saveUser (const juce::String& name);

    juce::File getUserDirectory() const;

private:
    juce::Array<juce::File> getUserFiles() const;
    void loadFactory (const FactoryPreset& preset);
    void setCurrentName (const juce::String& name);

    FancyProcessor& owner;
    std::vector<FactoryPreset> factory;
};

} // namespace fancy
