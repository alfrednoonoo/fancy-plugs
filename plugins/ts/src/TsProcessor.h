#pragma once

#include "TsDsp.h"

#include <fancy/plugin/FancyProcessor.h>

class TsProcessor final : public fancy::FancyProcessor
{
public:
    TsProcessor();

    void prepareAudio (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;

    TsDsp::Targets readTargets() const;
    TsDsp& getDsp() noexcept { return dsp; }

    // Highpass at its bottom and Lowpass at its top are Off.
    static constexpr float highpassOffHz = 20.0f, highpassMaxHz = 2000.0f;
    static constexpr float lowpassMinHz = 200.0f, lowpassOffHz = 20000.0f;
    static constexpr float mixFloorDb = -60.0f;

private:
    float read (const char* parameterId) const;
    bool isOn (const char* parameterId) const;

    TsDsp dsp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsProcessor)
};
