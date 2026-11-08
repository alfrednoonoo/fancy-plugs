#pragma once

#include <fancy/dsp/Detector.h>
#include <fancy/dsp/MeterTap.h>
#include <fancy/dsp/Smoother.h>
#include <fancy/dsp/Svf.h>

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

// See docs/gate.md.
class GateDsp
{
public:
    enum Detector { mainLR, mainL, mainR, sideLR, sideL, sideR };
    enum Light { lightClosed, lightOpen, lightHold };

    static constexpr double lookaheadMs = 50.0;
    static constexpr double rangeFloorDb = -80.0;    // at or below this, Range is -inf
    static constexpr double curveFloorDb = -120.0;   // where a -inf Range's curve runs to
    static constexpr double lowpassOffHz = 22000.0, highpassOffHz = 20.0;

    struct Targets
    {
        double thresholdDb = -40.0, hysteresisDb = -6.0;
        double preOpenMs = 5.0, attackMs = 0.5, holdMs = 20.0, releaseMs = 100.0;
        double rangeDb = -200.0, rmsMs = 0.0;
        double lowpassHz = lowpassOffHz, highpassHz = highpassOffHz;
        int detector = mainLR;
        bool lookahead = false, preview = false, duck = false, link = true;
        double wetGain = 1.0, dryGain = 0.0, noiseGain = 0.0, outputGain = 1.0;
        bool midi = false, velocityFromLevel = false;
        int note = 69, channel = 1;
        double velocityWindowMs = 2.0;
        bool bypass = false;
    };

    // One open, closed, attack, hold and release cycle. See "State machine" in docs/gate.md.
    struct Machine
    {
        enum State { closed, opening, open, hold, closing };
        enum Event { none, opened, retriggered, shut };

        Event step (double levelDb, double openDb, double closeDb, double attackStep, double releaseStep, int holdSamples) noexcept;
        void reset() noexcept { state = closed; position = 0.0; holdLeft = 0; armed = true; }

        State state = closed;
        double position = 0.0;  // 0 closed, 1 open
        int holdLeft = 0;
        bool armed = true;
    };

    // -60 to 0 dBFS onto 1 to 127.
    static int velocityFor (double peakDb) noexcept;

    // How late a note-on is, in ms, for these settings. Zero when it's on time.
    static double noteLatenessMs (const Targets& targets) noexcept;

    void prepare (double sampleRate);
    void reset();

    void setTargets (const Targets& targets) noexcept;
    void snapToTargets (const Targets& targets) noexcept;

    // midi may be null. Events land on the samples where they're heard.
    void process (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int numSamples, juce::MidiBuffer* midi) noexcept;

    int getLatencySamples() const noexcept { return activeLookahead ? lookaheadSamples : 0; }

    // For the editor, all lock-free.
    int getLight() const noexcept { return light.load (std::memory_order_relaxed); }
    float takeReduction() noexcept { return reduction.exchange (0.0f, std::memory_order_relaxed); }
    bool isSidechainMissing() const noexcept { return sidechainMissing.load (std::memory_order_relaxed); }

    struct HistoryColumn
    {
        float levelDb, gainDb;
    };

    // One column every columnSeconds. Returns how many were copied.
    static constexpr double historySeconds = 3.0;
    void setHistoryColumns (int columns) noexcept { historyColumns.store (std::max (1, columns), std::memory_order_relaxed); }
    int popHistory (HistoryColumn* destination, int maxColumns) noexcept;

private:
    static constexpr int maxQueuedEvents = 64;
    static constexpr int historyCapacity = 4096;

    struct QueuedEvent
    {
        std::int64_t time;
        bool on;
        int note, channel, velocity;
    };

    void clearRings() noexcept;
    void updateFilters() noexcept;
    double gainFor (double position) noexcept;
    void schedule (std::int64_t time, bool on, int velocity) noexcept;
    void flushEvents (juce::MidiBuffer* midi, std::int64_t blockStart, int numSamples) noexcept;
    void midiEvent (Machine::Event event, std::int64_t detectorTime) noexcept;
    void pushHistory (float levelDb, float gainDb) noexcept;

    double rate = 48000.0;
    Targets current;

    int lookaheadSamples = 2400;
    bool activeLookahead = false;
    int ringSize = 1, ringMask = 0;
    std::vector<float> audioRing[2], filteredRing[2], gainRing[2], openRing;
    int holdOffSamples = 0;

    fancy::dsp::DetectorFilter detectors[2];
    fancy::dsp::SvfCoefficients highpassCoefficients, lowpassCoefficients;
    fancy::dsp::Smoother logHighpass, logLowpass, highpassBlend, lowpassBlend, rms;
    double rmsCoefficient = 1.0;

    Machine machines[2];
    double cachedPosition[2] { -1.0, -1.0 }, cachedGain[2] { 0.0, 0.0 };

    // Range and the Gate/Duck switch move the curve smoothly. Range is smoothed as a linear gain,
    // 0 for -inf, so a jump from -inf to 0 dB settles like any gain change rather than creeping
    // up from -120 dB. curveRangeDb is its value in dB, floored at curveFloorDb.
    fancy::dsp::Smoother curveRange, duckBlend;
    double curveRangeDb = curveFloorDb;
    void updateCurveRangeDb() noexcept;

    fancy::dsp::Smoother wet, dry, noise, output, bypass, switchGain, preOpen;
    std::uint32_t noiseState[2] { 0x12345678u, 0x9abcdef1u };

    std::int64_t now = 0;

    // MIDI: the note in flight and what's waiting to go out.
    std::array<QueuedEvent, maxQueuedEvents> queue {};
    int queued = 0;
    bool noteSounding = false;
    std::int64_t lastOnTime = -1;
    bool velocityPending = false;
    std::int64_t velocityDue = 0, velocityHeard = 0;
    double velocityPeak = 0.0;
    int activeNote = 69, activeChannel = 1;

    // Sidechain warning.
    double silentSidechainSeconds = 0.0;

    // History.
    std::atomic<int> historyColumns { 456 };
    int columnFill = 0;
    float columnLevel = -200.0f, columnGain = 0.0f;
    std::vector<HistoryColumn> history = std::vector<HistoryColumn> ((size_t) historyCapacity);
    std::atomic<int> historyWrite { 0 }, historyRead { 0 };

    std::atomic<int> light { lightClosed };
    std::atomic<float> reduction { 0.0f };
    std::atomic<bool> sidechainMissing { false };
};
