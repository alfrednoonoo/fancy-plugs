#pragma once

#include "ExciterDsp.h"

#include <fancy/plugin/FancyProcessor.h>

#include <atomic>

class ExciterProcessor final : public fancy::FancyProcessor
{
public:
    ExciterProcessor();

    void prepareAudio (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;

private:
    ExciterDsp::Targets readTargets() const;

    ExciterDsp dsp;

    std::atomic<float>& freq;
    std::atomic<float>& clipBoost;
    std::atomic<float>& harmonics;
    std::atomic<float>& mixBack;
    std::atomic<float>& oversampling;
    std::atomic<float>& character;
    std::atomic<float>& output;
    std::atomic<float>& bypass;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExciterProcessor)
};
