#pragma once

#include <fancy/dsp/Analyzer.h>
#include "EqDsp.h"
#include "EqMatch.h"

#include <fancy/plugin/FancyProcessor.h>

#include <atomic>
#include <memory>

class EqProcessor final : public fancy::FancyProcessor,
                          private juce::AudioProcessorValueTreeState::Listener
{
public:
    EqProcessor();
    ~EqProcessor() override;

    void prepareAudio (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    bool waitForBackgroundWork (int timeoutMs) override;
    void processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    // Match's learned spectra and settings go with the project, not with presets or undo.
    void addProjectData (juce::ValueTree& state) override;
    void stateApplied() override;

    juce::AudioProcessorEditor* createEditor() override;

    static juce::String id (int band, const char* suffix) { return "b" + juce::String (band + 1) + suffix; }

    // Reads the band's parameters. It's used by the audio and worker threads and by the graph.
    eq::BandSpec readBand (int band);
    bool isBandUsed (int band);

    EqDsp::Targets readTargets();
    eq::DynamicSpec readDynamics (int band);

    // A band's current dynamic gain change in dB, and whether it's dynamic now (Phase 2.1 D).
    float getDynamicChange (int band) const noexcept { return dsp.getDynamicChange (band); }
    bool isDynamic (int band);

    // Test only: see EqDsp::setStepLength.
    void setStepLength (int samples) noexcept { dsp.setStepLength (samples); }

    // Message thread.
    void setParameter (const juce::String& parameterId, float value);
    void resetBand (int band);
    int addBand (float hz, float gainDb); // returns -1 when all 24 are in use
    void removeBand (int band);

    // EQ Match (Phase 3 Q2.4). Message thread. Match adds nothing to the audio path: its result
    // is ordinary bands.
    eq::match::Engine match;

    // A band the last Match wrote and nobody has changed since. The next Match replaces it.
    bool isMatchBand (int band);

    // The bands a Match may use: the unused ones and the last Match's.
    int getNumFreeForMatch();

    // What the bands that stay do to a signal in both channels, in dB on Match's grid.
    eq::match::Curve getMatchExistingDb();

    // What the new bands should add, from the learned spectra and Match's settings.
    eq::match::Curve getMatchTarget();

    // The settings a fit runs with: Bands is held to the free ones.
    eq::match::Settings getMatchFitSettings();

    // Writes fitted bands into free bands, as one undo step, replacing the last Match's bands.
    // Returns how many were written.
    int applyMatch (const std::vector<eq::BandSpec>& fitted);

    double getMatchRate() const { return getSampleRate() > 0.0 ? getSampleRate() : 48000.0; }

    // The analyzer's feed. The editor turns it on only while it's open.
    fancy::dsp::SampleFifo analyzerFifo;
    std::atomic<bool> analyzerActive { false };

private:
    class DesignThread final : public juce::Thread
    {
    public:
        explicit DesignThread (EqProcessor& processorToUse)
            : Thread ("Fancy EQ FIR design"), processor (processorToUse) {}

        void run() override;

    private:
        EqProcessor& processor;
    };

    using SharedFlag = std::shared_ptr<std::atomic<bool>>;

    explicit EqProcessor (SharedFlag logScaleFlag);

    void parameterChanged (const juce::String& parameterID, float newValue) override;

    float read (const char* parameterId);
    bool isOn (const char* parameterId);

    struct BandValues
    {
        std::atomic<float>* used, *on, *type, *frequency, *gain, *q, *slope, *channel;
    };

    struct DynamicValues
    {
        std::atomic<float>* on, *threshold, *range, *attack, *release, *mode, *sidechain;
    };

    SharedFlag logScale;
    std::array<BandValues, eq::maxBands> bandValues {};
    std::array<DynamicValues, eq::maxBands> dynamicValues {};
    EqDsp dsp;
    DesignThread designThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqProcessor)
};
