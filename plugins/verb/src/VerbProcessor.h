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

    void prepareAudio (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    bool waitForBackgroundWork (int timeoutMs) override { return waitForBuild (timeoutMs); }
    void processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

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

    // Phase 3 Q2.8: pairs the File module's file with a second one for the right input, which
    // makes it true stereo, or removes the pair.
    bool loadPair (int uid, const juce::File& file);
    void clearPair (int uid);

    // Plays a library IR ("Category/Name") in a File module (Phase 3 Q2.8).
    bool loadLibrary (int uid, const juce::String& key);
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

    // Phase 4 Wave 1 (docs/verb.md) ----------------------------------------------------------

    // Equal loudness, kept on the chain so it travels with projects and presets.
    bool getEqualLoudness();
    void setEqualLoudness (bool on);

    // Writes the last built IR, after trim, as a 24-bit WAV at the session rate; true stereo as
    // four channels in a four-channel file's order. False when there's nothing built yet or the
    // file can't be written.
    bool exportIr (const juce::File& file);

    // A dropped file: replaces a File module's file when uid names one, or else becomes a new
    // File module at the end of the chain. False, with nothing changed, for a file that isn't audio.
    bool dropFile (const juce::File& file, int uid = 0);

    // Library favourites, as "Category/Name". In the user's settings file, never in projects.
    juce::StringArray getFavourites();
    bool isFavourite (const juce::String& key);
    void setFavourite (const juce::String& key, bool favourite);

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
    void updateModulation();
    void requestBuild();
    bool buildPending();
    void buildNow();
    juce::File settingsFile() const;
    void saveSettings();

    VerbDsp dsp;
    verb::IrResolver resolver;

    std::mutex buildMutex; // never taken on the audio thread
    verb::ChainSpec pendingChain;
    verb::IrPreview preview;
    std::vector<int> missing;
    juce::File irFolder;
    juce::StringArray favourites;

    // Phase 4 Wave 1: the finished IR in file order, for Export IR, and the rate it was built at.
    std::shared_ptr<const juce::AudioBuffer<float>> exportable;
    double exportableRate = 48000.0;

    std::atomic<double> buildRate { 48000.0 };
    std::atomic<double> irSeconds { 0.0 };
    std::atomic<int> requestedBuild { 1 }, completedBuild { 0 };
    std::atomic<int> chainRevision { 0 };
    std::atomic<int> builtMode { -1 }, builtMaxBlock { -1 };

    // Modulate's live settings (Phase 3 Q2.8), read by the audio thread.
    std::atomic<float> modulationRate { 0.3f }, modulationDepth { 0.5f }, modulationSpread { 1.0f };

    BuildThread buildThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VerbProcessor)
};
