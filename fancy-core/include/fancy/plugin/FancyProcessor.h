#pragma once

#include "PresetManager.h"

#include <fancy/dsp/MeterTap.h>
#include <fancy/dsp/SidechainWatch.h>

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

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
    // VST3 hosts see one program; Steinberg's validator requires it to have a name.
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    juce::AudioProcessorParameter* getBypassParameter() const override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Validates the tree first, so a damaged preset or session can't reach the parameters.
    bool applyState (const juce::ValueTree& tree);

    // Runs after a state or preset replaces the parameters. A plugin with state beyond its
    // parameters checks it here.
    virtual void stateApplied() {}

    // Adds state that isn't kept in the value tree, such as data the audio thread writes, to
    // a copy that's about to be saved.
    virtual void addToState (juce::ValueTree& state) { juce::ignoreUnused (state); }

    // Waits until any background thread has caught up with the current settings: a design, a
    // build or an impulse. For tests that render "real time" as a fast machine would hear it.
    virtual bool waitForBackgroundWork (int timeoutMs) { juce::ignoreUnused (timeoutMs); return true; }

    // Runs after a factory preset sets its parameters, with the preset's data string.
    virtual void factoryPresetLoaded (const juce::String& data) { juce::ignoreUnused (data); }

    // A plugin calls this when the user changes state outside the parameters, such as Verb's
    // chain or FIR's curves, so the preset dropdown can mark the preset as changed.
    void noteEdited() noexcept { edits.fetch_add (1, std::memory_order_relaxed); }
    int getEditCount() const noexcept { return edits.load (std::memory_order_relaxed); }

    juce::AudioProcessorValueTreeState& getApvts() noexcept { return apvts; }
    PresetManager& getPresets() noexcept { return presets; }
    const PresetManager& getPresets() const noexcept { return presets; }

    dsp::MeterTap inputMeter;
    dsp::MeterTap outputMeter;

    // Plugins with a sidechain update it each block; the header shows its warning.
    dsp::SidechainWatch sidechainWatch;

private:
    juce::String name;
    std::atomic<int> edits { 0 };
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FancyProcessor)
};

} // namespace fancy
