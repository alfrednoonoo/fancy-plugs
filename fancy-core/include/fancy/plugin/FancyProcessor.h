#pragma once

#include "PresetManager.h"
#include "Workflow.h"

#include <fancy/dsp/CompareStage.h>
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
    // 2: Phase 2.1 W adds the A/B slots. Version 1 states load with B as a copy of A.
    // 3: Phase 2.1 D adds EQ's dynamic bands. From here on, a parameter missing from a state
    //    takes its default, rather than keeping the value the instance had.
    static constexpr int stateVersion = 4;

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

    // Final, so Compare (W.4) wraps every plugin. Plugins implement prepareAudio and processAudio.
    void prepareToPlay (double sampleRate, int samplesPerBlock) final;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) final;
    using juce::AudioProcessor::processBlock;

    virtual void prepareAudio (double sampleRate, int samplesPerBlock) = 0;
    virtual void processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) = 0;

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

    // Changes whenever state outside the parameters changes, for the undo history. A plugin adds
    // changes that happen on their own, such as FIR finishing a noise profile.
    virtual int getEditCounter() const { return getEditCount(); }

    // Undo, redo and A/B (W.2, W.3).
    Workflow& getWorkflow() noexcept { return *workflow; }

    // Makes every parameter change inside it a user edit, for code that sets parameters
    // without a gesture: adding or deleting a band, loading a preset. One step, with this name
    // if it's the outermost scope.
    class UserEdit
    {
    public:
        UserEdit (FancyProcessor& processor, const juce::String& name = {}) : workflow (processor.getWorkflow()) { workflow.beginUserEdit (name); }
        ~UserEdit() { workflow.endUserEdit(); }

    private:
        Workflow& workflow;
        JUCE_DECLARE_NON_COPYABLE (UserEdit)
    };

    // Level-matched Compare (W.4). Not a parameter: the editor turns it on and off.
    dsp::CompareStage compare;

    // The plugin's listen or preview switch, which replaces the output with a diagnostic signal
    // (§0.3 of docs/BUILD_PROMPT_PHASE3_NEW_PLUGINS.md). It's temporary: not a parameter, so it's
    // never saved, automated, undone or switched by A/B, and it only counts while an editor is
    // open. Closing the last editor turns it off.
    void setListening (bool shouldListen) noexcept { listening.store (shouldListen, std::memory_order_relaxed); }
    bool isListeningSwitchOn() const noexcept { return listening.load (std::memory_order_relaxed); }

    // What the audio follows: the switch, while an editor is open.
    bool isListening() const noexcept
    {
        return listening.load (std::memory_order_relaxed) && openEditors.load (std::memory_order_relaxed) > 0;
    }

    // ShellEditor calls these from its constructor and destructor.
    void editorOpened() noexcept { openEditors.fetch_add (1, std::memory_order_relaxed); }

    void editorClosed() noexcept
    {
        if (openEditors.fetch_sub (1, std::memory_order_relaxed) <= 1)
            listening.store (false, std::memory_order_relaxed);
    }

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
    std::atomic<bool> listening { false };
    std::atomic<int> openEditors { 0 };
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;
    std::unique_ptr<Workflow> workflow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FancyProcessor)
};

} // namespace fancy
