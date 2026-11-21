#pragma once

#include "CompDsp.h"

#include <fancy/plugin/FancyProcessor.h>

#include <atomic>

class CompProcessor final : public fancy::FancyProcessor
{
public:
    CompProcessor();

    void prepareAudio (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;

    float takeGainReduction (int channel) noexcept { return dsp.takeGainReduction (channel); }

    // Phase 3 Q2.5, for the editor.
    float getAutoThresholdOffset() const noexcept { return dsp.getAutoThresholdOffset(); }
    float getDisplayLevel() const noexcept { return dsp.getDisplayLevel(); }
    float getDisplayLift() const noexcept { return dsp.getDisplayLift(); }

    // Phase 4 Wave 1: the level history's new columns.
    int popHistory (fancy::dsp::LevelColumn* destination, int maxColumns) noexcept { return dsp.popHistory (destination, maxColumns); }

private:
    CompDsp::Targets readTargets();
    CompDsp::Transport readTransport();
    float read (const char* id);
    bool isOn (const char* id);

    CompDsp dsp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompProcessor)
};
