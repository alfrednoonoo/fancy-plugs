#pragma once

#include "LimitDsp.h"

#include <fancy/plugin/FancyProcessor.h>

class LimitProcessor final : public fancy::FancyProcessor
{
public:
    LimitProcessor();

    void prepareAudio (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;

    LimitDsp::Targets readTargets() const;
    LimitDsp& getDsp() noexcept { return dsp; }

    enum Target { streaming, podcast, broadcast, custom };

    // The loudness the Target dropdown stands for, in LUFS.
    float getTargetLufs() const;

    // Clears the integrated loudness, range and true peak. Any thread.
    void resetLoudness() noexcept;

    // Message thread.
    void setParameter (const juce::String& parameterId, float value);

private:
    float read (const char* parameterId) const;
    bool isOn (const char* parameterId) const;

    LimitDsp dsp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LimitProcessor)
};
