#pragma once

#include "IrBuilder.h"
#include "IrFiles.h"
#include "VerbDsp.h"

#include <fancy/plugin/FancyProcessor.h>

#include <atomic>
#include <memory>
#include <mutex>

class VerbProcessor final : public fancy::FancyProcessor,
                            private juce::AudioProcessorValueTreeState::Listener
{
public:
    VerbProcessor();
    ~VerbProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    double getTailLengthSeconds() const override;

    juce::AudioProcessorEditor* createEditor() override;

    void stateApplied() override;
    void factoryPresetLoaded (const juce::String& data) override;

    // Chain editing, on the message thread. Modules are addressed by uid, because a card
    // can outlive its position when the chain is reordered.
    juce::ValueTree getChain();
    int addModule (verb::ModuleType type, int position); // returns the uid, or 0 when full
    void removeModule (int uid);
    void moveModule (int uid, int position);
    void setModuleOn (int uid, bool on);
    void setModuleValue (int uid, const juce::Identifier& id, const juce::var& value);
    bool loadFile (int uid, const juce::File& file);
    void setTrim (double startSeconds, double endSeconds);

    // Bumped when modules are added, removed or reordered, so the rack knows to rebuild.
    int getChainRevision() const noexcept { return chainRevision.load(); }

    // The last finished build, for the display.
    int getBuildRevision() const noexcept { return completedBuild.load(); }
    verb::IrPreview getPreview();
    bool isMissing (int uid);
    int getLateBlocks() const noexcept { return dsp.getLateBlocks(); }

    juce::File getIrFolder();
    void setIrFolder (const juce::File& folder);

    // Waits for the worker, for offline renders and tests.
    bool waitForBuild (int timeoutMs);

private:
    class BuildThread final : public juce::Thread
    {
    public:
        explicit BuildThread (VerbProcessor& processorToUse)
            : Thread ("Fancy Verb IR build"), processor (processorToUse) {}

        void run() override;

    private:
        VerbProcessor& processor;
    };

    void parameterChanged (const juce::String& parameterID, float newValue) override;

    float read (const char* parameterId);
    verb::Convolver::Mode readMode();
    int readMaxBlock();

    VerbDsp::Targets readTargets();

    juce::ValueTree findModule (int uid);
    int nextUid();
    void chainEdited (bool structural);
    void requestBuild();
    bool buildPending();
    void buildNow();
    juce::File settingsFile() const;

    VerbDsp dsp;
    verb::IrResolver resolver;

    std::mutex buildMutex; // never taken on the audio thread
    verb::ChainSpec pendingChain;
    verb::IrPreview preview;
    std::vector<int> missing;
    juce::File irFolder;

    std::atomic<double> buildRate { 48000.0 };
    std::atomic<double> irSeconds { 0.0 };
    std::atomic<int> requestedBuild { 1 }, completedBuild { 0 };
    std::atomic<int> chainRevision { 0 };
    std::atomic<int> builtMode { -1 }, builtMaxBlock { -1 };

    BuildThread buildThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VerbProcessor)
};
