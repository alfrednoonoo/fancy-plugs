#pragma once

#include "PresetManager.h"

#include <fancy/dsp/MeterTap.h>

#include <juce_audio_processors/juce_audio_processors.h>

namespace fancy
{

// Shared AudioProcessor behaviour: parameters, versioned state, presets and meters.
// Every plugin's layout must contain a bool parameter with the ID "bypass", and a float
// parameter with the ID "output" for the header knob.
class FancyProcessor : public juce::AudioProcessor
{
public:
    static constexpr int stateVersion = 1;

    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

    FancyProcessor (const juce::String& pluginName,
                    const juce::Identifier& stateType,
                    const BusesProperties& buses,
                    Layout layout,
                    std::vector<FactoryPreset> factoryPresets);

    const juce::String getName() const override { return name; }
    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    juce::AudioProcessorParameter* getBypassParameter() const override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Validates the tree first, so a damaged preset or session can't reach the parameters.
    bool applyState (const juce::ValueTree& tree);

    juce::AudioProcessorValueTreeState& getApvts() noexcept { return apvts; }
    PresetManager& getPresets() noexcept { return presets; }

    dsp::MeterTap inputMeter;
    dsp::MeterTap outputMeter;

private:
    juce::String name;
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FancyProcessor)
};

} // namespace fancy
