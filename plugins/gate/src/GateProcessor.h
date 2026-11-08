#pragma once

#include "GateDsp.h"

#include <fancy/plugin/FancyProcessor.h>

class GateProcessor final : public fancy::FancyProcessor
{
public:
    GateProcessor();

    void prepareAudio (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    bool producesMidi() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override;

    GateDsp::Targets readTargets() const;
    GateDsp& getDsp() noexcept { return dsp; }

    // Message thread.
    void setParameter (const juce::String& parameterId, float value);

    static juce::String noteName (int note);

private:
    float read (const char* parameterId) const;
    bool isOn (const char* parameterId) const;

    GateDsp dsp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GateProcessor)
};
