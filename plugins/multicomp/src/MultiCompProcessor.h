#pragma once

#include "MultiCompDsp.h"

#include <fancy/dsp/Analyzer.h>
#include <fancy/plugin/FancyProcessor.h>

#include <array>
#include <atomic>
#include <memory>

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
    std::array<BandValues, multicomp::maxBands> bandValues {};
    MultiCompDsp dsp;
    DesignThread designThread;

    // Message thread only.
    int gestureParameter = -1;
    float gestureValue = 0.0f;
    bool propagating = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiCompProcessor)
};
