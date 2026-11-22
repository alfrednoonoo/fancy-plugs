#pragma once

#include <fancy/dsp/Smoother.h>
#include <fancy/dsp/Svf.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

// Fancy Chorus's signal path. See docs/chorus.md; modelled in docs/research/chorus-model.py.
//
//   input (Stereo, or summed to Mono) + feedback -> one delay line per channel
//   -> 1 to 6 voices, each reading both lines at Delay + Depth x 5 ms x lfo (Hermite)
//   -> each voice narrowed and panned by Width -> sum / sqrt(voices)
//   -> Low cut, High cut, Age (saturation, low-pass, hiss), Keep bass mono -> x Wet x Ducking
//   -> plus Dry -> Output, with Listen to wet and the host's bypass. Zero latency.
class ChorusDsp
{
public:
    enum Mode { classic, ensemble, dimension, doubler, flange };
    enum Shape { sine, triangle, smoothRandom };

    static constexpr int maxVoices = 6;
    static constexpr double depthRangeMs = 5.0;       // Depth 100 % = +-5 ms
    static constexpr double minDelayMs = 1.0, maxDelayMs = 30.0;
    static constexpr double flangeMinMs = 0.3, flangeMaxMs = 10.0;
    static constexpr double maxFeedback = 0.9;
    static constexpr double duckThresholdDb = -24.0, duckDepthDb = 24.0, duckAttackMs = 1.0, duckReleaseMs = 250.0;

    struct Targets
    {
        int mode = ensemble;
        double rateHz = 0.8;
        bool sync = false;
        double syncBeats = 4.0;      // one LFO cycle, in quarter notes
        double bpm = 120.0;
        bool playing = false;        // the host's transport, for Tempo sync's phase
        double ppq = 0.0;            // at the block's first sample
        double depth = 0.45;         // 0 to 1
        double delayMs = 7.0;        // 1 to 30 (Flange maps it to 0.3 to 10)
        double feedback = 0.0;       // -0.9 to 0.9
        int voices = 3;
        double spread = 0.6, width = 1.0;   // 0 to 1
        bool lowCutOn = true, highCutOn = true;
        double lowCutHz = 150.0, highCutHz = 9000.0;
        double age = 0.25;           // 0 to 1
        bool bassMono = true;
        int shape = sine;
        bool monoInput = false;
        double lfoPhaseDeg = 90.0;   // the right channel's LFO against the left's
        double ducking = 0.0;        // 0 to 1
        double wetGain = 0.70794578438413791, dryGain = 1.0, outputGain = 1.0;
        bool listen = false, bypass = false;
    };

    ChorusDsp();

    void prepare (double sampleRate);
    void reset();

    void setTargets (const Targets& targets) noexcept { current = targets; }
    void snapToTargets (const Targets& targets) noexcept;

    // Any block size, mono or stereo.
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    // Delay's knob in Flange mode is mapped from 1 to 30 ms onto 0.3 to 10 ms.
    static double flangeDelayMs (double delayMs) noexcept;

    // The centre delay and the modulation depth of a mode, in ms.
    static double centreDelayMs (int mode, double delayMs) noexcept;
    static double sweepMs (int mode, double delayMs, double depth) noexcept;

    // Where voice v of n sits, -1 (left) to +1 (right), at Width 1.
    static double voicePosition (int voice, int numVoices) noexcept;

    // Voice v's LFO offset in cycles: Spread spreads the voices over one cycle.
    static double offsetFor (double spread, int voice, int numVoices) noexcept { return spread * (double) voice / (double) std::max (1, numVoices); }

    // The LFO of a shape at a phase in cycles, -1 to 1.
    static double lfoValue (int shape, double phase, std::uint32_t seed) noexcept;

    // The display's columns: each voice's left-channel delay in ms. Lock-free.
    struct HistoryColumn
    {
        std::array<float, maxVoices> delayMs;
        int voices;
    };

    static constexpr double historySeconds = 2.0;
    void setHistoryColumns (int columns) noexcept { historyColumns.store (columns > 1 ? columns : 1, std::memory_order_relaxed); }
    int popHistory (HistoryColumn* destination, int maxColumns) noexcept;

private:
    static constexpr int historyCapacity = 4096;
    static constexpr int coefficientInterval = 16;

    void updateFilters (double lowCutHz, double highCutHz, double ageAmount) noexcept;
    double readLine (int channel, double delaySamples) const noexcept;
    double voiceLfo (int mode, int shape, double phase, int voice, int channel) const noexcept;
    double lfoAt (int voice, int channel, double basePhase, double rightOffset) const noexcept;
    void pushHistory (const HistoryColumn& column) noexcept;

    double rate = 48000.0;
    Targets current;

    // The mode, shape and sync in use: a change fades the wet out, switches and fades back in.
    int activeMode = ensemble, activeShape = sine;
    bool activeSync = false;

    // The LFO's phase in cycles, shared by every voice.
    double phase = 0.0;
    bool aligned = false;
    long long now = 0;

    std::array<std::vector<float>, 2> lines;
    int lineSize = 1, lineMask = 0;
    int writeIndex = 0;

    fancy::dsp::Smoother rateSmoother, depth, delay, feedback, width, age, lfoPhase, ducking,
        lowCutHz, highCutHz, lowCutBlend, highCutBlend, bassMonoBlend, wet, dry, output, bypass, listen, switchGain;
    // Each voice's gain, place in stereo and LFO offset (Spread x v / voices, in cycles), all glide
    // when Voices or Spread change, so no voice's delay jumps.
    std::array<fancy::dsp::Smoother, maxVoices> voiceGains, voicePositions, voiceOffsets;
    fancy::dsp::Smoother normalise;

    std::array<fancy::dsp::Svf, 2> lowCuts, highCuts, ageFilters;
    fancy::dsp::Svf bassMonoFilter;
    fancy::dsp::SvfCoefficients lowCutCoefficients, highCutCoefficients, ageCoefficients, bassMonoCoefficients;

    // The feedback path's DC blocker, one per channel.
    std::array<double, 2> dcIn {}, dcOut {};
    double dcCoefficient = 0.9995;

    // Ducking: the input's peak, and the reduction in dB.
    double duckPeak = 0.0, duckDb = 0.0;
    double duckPeakFall = 0.0, duckAttack = 1.0, duckRelease = 1.0;

    // Age's hiss.
    std::uint32_t noiseState = 0x1234567u;

    // Values worked out only when what they depend on moves: each voice's pan law, and Age's.
    struct Pan
    {
        double position = 0.0, left = 1.0, right = 1.0;
    };

    struct AgeCache
    {
        double amount = -1.0, drive = 1.0, inverseDrive = 1.0, blend = 0.0, hiss = 0.0;
    };

    std::array<Pan, maxVoices> pans {};
    AgeCache ageCache;

    struct MotionCache
    {
        double delay = -1.0, depth = -1.0;
        int mode = -1;
        double centre = 0.0, sweep = 0.0;
    };

    MotionCache motionCache;

    // The LFOs in straight lines between points lfoSegment samples apart.
    static constexpr int lfoSegment = 8;
    std::array<std::array<double, 2>, maxVoices> lfoNow {}, lfoStep {}, lfoEnd {};
    int lfoSegmentLeft = 0;
    bool lfoSegmentValid = false;
    std::array<double, 3> filterInputs { -1.0, -1.0, -1.0 };   // Low cut, High cut, Age at the last update

    // History.
    std::atomic<int> historyColumns { 480 };
    int columnFill = 0;
    HistoryColumn column {};
    std::vector<HistoryColumn> history = std::vector<HistoryColumn> ((size_t) historyCapacity);
    std::atomic<int> historyWrite { 0 }, historyRead { 0 };
};
