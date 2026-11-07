#pragma once

#include <fancy/dsp/Analyzer.h>
#include "EqDsp.h"

#include <fancy/plugin/FancyProcessor.h>

#include <atomic>
#include <memory>

class EqProcessor final : public fancy::FancyProcessor,
                          private juce::AudioProcessorValueTreeState::Listener
{
public:
    EqProcessor();
    ~EqProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;

    static juce::String id (int band, const char* suffix) { return "b" + juce::String (band + 1) + suffix; }

    // Reads the band's parameters. It's used by the audio and worker threads and by the graph.
    eq::BandSpec readBand (int band);
    bool isBandUsed (int band);

    EqDsp::Targets readTargets();

    // Message thread.
    void setParameter (const juce::String& parameterId, float value);
    void resetBand (int band);
    int addBand (float hz, float gainDb); // returns -1 when all 16 are in use
    void removeBand (int band);

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
        std::atomic<float>* used, *on, *type, *frequency, *gain, *q, *slope;
    };

    SharedFlag logScale;
    std::array<BandValues, eq::maxBands> bandValues {};
    EqDsp dsp;
    DesignThread designThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqProcessor)
};
