#pragma once

#include "ChorusDsp.h"

#include <fancy/plugin/FancyProcessor.h>

class ChorusProcessor final : public fancy::FancyProcessor
{
public:
    ChorusProcessor();

    void prepareAudio (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;

    ChorusDsp::Targets readTargets() const;
    ChorusDsp& getDsp() noexcept { return dsp; }

    float read (const char* parameterId) const;

    // Tempo sync's note lengths: 4/1 to 1/32, each plain, dotted and triplet.
    static constexpr int numNotes = 24, defaultNote = 6;   // 1/1
    static juce::String noteName (int noteIndex);
    static double noteBeats (int noteIndex);               // quarter notes per LFO cycle

    double getBpm() const noexcept { return tempo.load (std::memory_order_relaxed); }

    // Low cut at its bottom and High cut at its top are Off.
    static constexpr float lowCutOffHz = 20.0f, lowCutMaxHz = 1000.0f;
    static constexpr float highCutMinHz = 1000.0f, highCutOffHz = 20000.0f;
    static constexpr float mixFloorDb = -60.0f;

private:
    bool isOn (const char* parameterId) const;
    void updateTransport();

    ChorusDsp dsp;
    std::atomic<double> tempo { 120.0 };
    std::atomic<bool> playing { false };
    std::atomic<double> ppq { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChorusProcessor)
};
