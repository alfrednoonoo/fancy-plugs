#pragma once

#include "RoomDsp.h"

#include <fancy/plugin/FancyProcessor.h>

#include <atomic>

class RoomProcessor final : public fancy::FancyProcessor
{
public:
    RoomProcessor();

    void prepareAudio (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    // Decision 3: Pre-delay + the longest reflection + 1.5 x Decay, and infinite while frozen.
    double getTailLengthSeconds() const override;

    juce::AudioProcessorEditor* createEditor() override;

    RoomDsp::Targets readTargets() const;
    RoomDsp& getDsp() noexcept { return dsp; }

    // Sync pre-delay (decision 5): Fancy Delay's notes from 1/64 to 1/8, straight, dotted and
    // triplet, capped at 250 ms. The note's index is note * 3 + feel, from 1/64 up.
    static constexpr int numNotes = 12;
    static constexpr int defaultNote = 3; // 1/32
    static double noteMs (int noteIndex, double bpm) noexcept;
    static juce::String noteName (int noteIndex);

    // The Pre-delay in use, in ms, and whether Sync's note is longer than the cap.
    double getPredelayMs() const noexcept;
    bool isPredelayCapped() const noexcept;

    double getBpm() const noexcept { return tempo.load (std::memory_order_relaxed); }
    void setTempoForTesting (double bpm) noexcept { tempo.store (bpm, std::memory_order_relaxed); }

    float read (const char* parameterId) const;
    bool isOn (const char* parameterId) const;

    static constexpr float mixFloorDb = -60.0f;

private:
    void updateTempo();
    double unclampedNoteMs() const noexcept;

    RoomDsp dsp;
    std::atomic<double> tempo { 120.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomProcessor)
};
