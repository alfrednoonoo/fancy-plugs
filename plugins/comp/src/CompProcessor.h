#pragma once

#include "CompDsp.h"

#include <fancy/plugin/FancyProcessor.h>

#include <atomic>

class CompProcessor final : public fancy::FancyProcessor
{
public:
    CompProcessor();

    // Phase 4 Wave 2: Host sync's Rate choices (1/1 to 1/16, each plain, dotted and triplet),
    // and a choice's step in beats (quarter notes).
    static const juce::StringArray& syncRateNames();
    static double syncStepBeats (int index) noexcept;
    static constexpr int defaultSyncRate = 6; // 1/4

    // Phase 4 Wave 2: the detector bands' Type choices (CompDsp::BandType's order) and each
    // band's parameter IDs, 0 to 5.
    struct BandIds
    {
        const char* type;
        const char* frequency;
        const char* gain;
        const char* q;
    };

    static const juce::StringArray& bandTypeNames();
    static const BandIds& bandIds (int band) noexcept;

    // Whether Host sync is running on the host's tempo, for the Options page's caption.
    bool syncHasHostTempo() const noexcept { return dsp.syncHasHostTempo(); }

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
