#pragma once

#include "DelayDsp.h"

#include <fancy/plugin/FancyProcessor.h>

#include <array>
#include <atomic>

class DelayProcessor final : public fancy::FancyProcessor
{
public:
    DelayProcessor();

    void prepareAudio (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    double getTailLengthSeconds() const override { return delay::maxDelayMs * 0.001; }

    juce::AudioProcessorEditor* createEditor() override;

    static juce::String id (int tap, const char* suffix) { return "t" + juce::String (tap + 1) + suffix; }

    DelayDsp::TapSpec readTap (int tap) const;
    DelayDsp::Targets readTargets() const;
    bool isTapUsed (int tap) const;

    double getBpm() const noexcept { return tempo.load (std::memory_order_relaxed); }
    bool hasHostTempo() const noexcept { return hostTempo.load (std::memory_order_relaxed); }

    // Exposed so tests can stand in for the host.
    void setTempoForTesting (double bpm) noexcept { tempo.store (bpm, std::memory_order_relaxed); }

    // Message thread.
    void setParameter (const juce::String& parameterId, float value);
    void resetTap (int tap);
    int addTap (int after); // returns -1 when all 16 are in use
    void removeTap (int tap);
    int countTaps() const;

private:
    struct TapValues
    {
        std::atomic<float>* used, *on, *solo, *mode, *ms, *note, *feel, *mult, *feedback;
        std::atomic<float>* lowpass, *highpass, *bits, *width, *volume, *pan, *ping, *inLoop, *saturate;
    };

    float read (const char* parameterId) const;
    void updateTempo();

    std::array<TapValues, delay::maxTaps> tapValues {};
    std::atomic<double> tempo { 120.0 };
    std::atomic<bool> hostTempo { false };
    DelayDsp dsp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayProcessor)
};
