#pragma once

#include "SatureDsp.h"

#include <fancy/plugin/FancyProcessor.h>

class SatureProcessor final : public fancy::FancyProcessor
{
public:
    SatureProcessor();

    void prepareAudio (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;

    SatureDsp::Targets readTargets() const;
    SatureDsp& getDsp() noexcept { return dsp; }

    // Low below 20 Hz and High above 20 kHz are Off.
    static constexpr float lowOffHz = 16.0f, lowMinHz = 20.0f, lowMaxHz = 2000.0f;
    static constexpr float highMinHz = 1000.0f, highMaxHz = 20000.0f, highOffHz = 25000.0f;
    static constexpr float mixFloorDb = -60.0f;

    static juce::String characterText (float value);
    static float parseCharacter (const juce::String& text);

private:
    float read (const char* parameterId) const;
    bool isOn (const char* parameterId) const;

    SatureDsp dsp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SatureProcessor)
};
