#pragma once

#include "GateDsp.h"

#include <fancy/dsp/OutputCeiling.h>
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
    bool acceptsMidi() const override { return true; }   // Phase 4 Wave 1: the MIDI trigger

    juce::AudioProcessorEditor* createEditor() override;

    GateDsp::Targets readTargets() const;
    GateDsp& getDsp() noexcept { return dsp; }

    // Message thread.
    void setParameter (const juce::String& parameterId, float value);

    static juce::String noteName (int note);

    // Phase 4 Wave 2: Host sync's rates (1/1 to 1/16, plain, dotted, triplet), 1/8 by default.
    static const juce::StringArray& syncRateNames();
    static double syncStepBeats (int index) noexcept;
    static constexpr int defaultSyncRate = 9;

private:
    float read (const char* parameterId) const;
    bool isOn (const char* parameterId) const;
    fancy::dsp::TempoTrigger::Position readPosition();

    GateDsp dsp;

    // Phase 4 Wave 2, v1.7.1 (Upward, Decision 3 A): the shared output ceiling, out of the path
    // while it's Off.
    fancy::dsp::OutputCeiling ceiling;
    int totalLatency() const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GateProcessor)
};
