#pragma once

#include "KickerDsp.h"

#include <fancy/plugin/FancyProcessor.h>

#include <atomic>

class KickerProcessor final : public fancy::FancyProcessor
{
public:
    KickerProcessor();

    void prepareAudio (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;

private:
    KickerDsp::Targets readTargets() const;

    KickerDsp dsp;

    std::atomic<float>& freq;
    std::atomic<float>& wet;
    std::atomic<float>& dry;
    std::atomic<float>& q;
    std::atomic<float>& output;
    std::atomic<float>& monoBelow;
    std::atomic<float>& softClip;
    std::atomic<float>& bypass;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickerProcessor)
};
