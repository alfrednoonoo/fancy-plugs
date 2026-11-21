#pragma once

#include "MultiCompDsp.h"

#include <fancy/dsp/Analyzer.h>
#include <fancy/dsp/Svf.h>
#include <fancy/plugin/FancyProcessor.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

class MultiCompProcessor final : public fancy::FancyProcessor,
                                 private juce::AudioProcessorValueTreeState::Listener,
                                 private juce::AudioProcessorParameter::Listener
{
public:
    MultiCompProcessor();
    ~MultiCompProcessor() override;

    void prepareAudio (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    bool waitForBackgroundWork (int timeoutMs) override;
    void processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;

    static juce::String id (int band, const char* suffix) { return "b" + juce::String (band + 1) + suffix; }

    MultiCompDsp::BandSpec readBand (int band) const;
    MultiCompDsp::Targets readTargets() const;
    int getNumBands() const;

    // Band edges, after the DSP's own sorting and spacing.
    double bandBottomHz (int band) const;
    double bandTopHz (int band) const;

    float takeGainReduction (int band) noexcept { return dsp.takeGainReduction (band); }
    fancy::dsp::MeterTap& getBandInput (int band) noexcept { return dsp.getBandInput (band); }

    // Message thread.
    void setParameter (const juce::String& parameterId, float value);
    void resetBand (int band);
    int splitBand (int band, double hz); // returns the new band, or -1 if there's no room
    void removeBand (int band);
    void setCrossover (int crossover, double hz);

    // Phase 4 Wave 1: Learn crossovers. startLearning() listens to 5 s of input with signal;
    // the editor polls getLearnProgress() (-1 idle, 0 to 1, 1 done) and then calls
    // applyLearning(), which places the crossovers as one undo step.
    void startLearning();
    float getLearnProgress() const noexcept;
    bool applyLearning();

    // Where the crossovers go for these third-octave energies (25 Hz up): equal shares. Tests use it.
    static std::vector<double> crossoversFor (const std::array<double, 30>& energy, int numBands);

    // The spectrum's feed. The editor turns it on only while it's open.
    fancy::dsp::SampleFifo analyzerFifo;
    std::atomic<bool> analyzerActive { false };

private:
    class DesignThread final : public juce::Thread
    {
    public:
        explicit DesignThread (MultiCompProcessor& processorToUse)
            : Thread ("Fancy MultiComp FIR design"), processor (processorToUse) {}

        void run() override;

    private:
        MultiCompProcessor& processor;
    };

    using SharedFlag = std::shared_ptr<std::atomic<bool>>;

    explicit MultiCompProcessor (SharedFlag logScaleFlag);

    // The log-scale switch for top frequencies.
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    // Link bands.
    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override;

    float read (const char* parameterId) const;
    bool isOn (const char* parameterId) const;
    void copyBand (int from, int to);
    void writeValue (const juce::String& parameterId, float value);

    struct BandValues
    {
        std::atomic<float>* on, *solo, *top, *threshold, *ratio, *knee, *attack, *release, *rms, *gain, *makeup, *autoRelease, *feedback, *sidechain;
    };

    SharedFlag logScale;
    struct UpwardValues
    {
        std::atomic<float>* on, *threshold, *ratio, *maxLift, *floor;
    };

    std::array<BandValues, multicomp::maxBands> bandValues {};
    std::array<UpwardValues, multicomp::maxBands> upwardValues {};
    std::array<std::atomic<float>*, multicomp::maxBands> channelValues {}; // Phase 3 Q2.6
    std::array<std::atomic<float>*, multicomp::maxBands> muteValues {};    // Phase 4 Wave 1

    // Phase 4 Wave 1: Learn crossovers.
    void learnFrom (const float* const* main, int numChannels, int numSamples) noexcept;
    enum LearnState { idle, learning, learned };
    std::atomic<int> learnState { idle };
    std::array<fancy::dsp::SvfCoefficients, 30> learnCoefficients {};
    std::array<fancy::dsp::Svf, 30> learnFilters {};
    std::array<double, 30> learnEnergy {};
    std::int64_t learnHeard = 0;
    bool learnPrepared = false;
    MultiCompDsp dsp;
    DesignThread designThread;

    // Message thread only.
    int gestureParameter = -1;
    float gestureValue = 0.0f;
    bool propagating = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiCompProcessor)
};
