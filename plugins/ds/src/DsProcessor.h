#pragma once

#include "DsDsp.h"

#include <fancy/plugin/FancyProcessor.h>

class DsProcessor final : public fancy::FancyProcessor
{
public:
    DsProcessor();
    ~DsProcessor() override;

    void prepareAudio (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    bool waitForBackgroundWork (int timeoutMs) override;

    juce::AudioProcessorEditor* createEditor() override;

    DsDsp::Targets readTargets() const;
    DsDsp& getDsp() noexcept { return dsp; }

    // Message thread.
    void setParameter (const juce::String& parameterId, float value);

private:
    // Designs the linear-phase split when Freq, Width or the shape move. An offline render
    // designs in-line instead, so it's repeatable.
    class DesignThread final : public juce::Thread
    {
    public:
        explicit DesignThread (DsProcessor& processorToUse) : Thread ("Fancy DS FIR design"), processor (processorToUse) {}
        void run() override;

    private:
        DsProcessor& processor;
    };

    float read (const char* parameterId) const;
    bool isOn (const char* parameterId) const;

    DsDsp dsp;
    DesignThread designThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DsProcessor)
};
