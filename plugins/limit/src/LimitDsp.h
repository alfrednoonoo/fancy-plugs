#pragma once

#include <fancy/dsp/LoudnessMeter.h>
#include <fancy/dsp/MinPhaseCrossover.h>
#include <fancy/dsp/Smoother.h>
#include <fancy/dsp/TruePeakLimiter.h>

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

// See docs/limit.md.
class LimitDsp
{
public:
    enum Style { clean, punchy, dense };
    enum Oversampling { osOff, os2, os4, os8 };
    enum Dither { ditherOff, dither16, dither16Shaped, dither24, dither24Shaped };

    static constexpr double maxLookaheadMs = 5.0;

    // The true-peak target sits this far under Ceiling, for the interpolator's error.
    static constexpr double truePeakMarginDb = 0.05;

    // Phase 4 Wave 3 (docs/limit.md, "multiband limiting", decisions A): up to four bands, each
    // limited 3 dB under Ceiling with a fixed 5 ms of lookahead delay, before the final stage.
    static constexpr int maxBands = 4;
    static constexpr double bandHeadroomDb = 3.0, bandLookaheadMs = 5.0;

    struct Targets
    {
        double thresholdDb = 0.0, ceilingDb = -1.0, releaseMs = 80.0, lookaheadMs = 2.0, link = 1.0;
        int style = clean;
        bool truePeak = true, autoRelease = true, listen = false;
        int oversampling = os4;
        bool linearPhase = false;
        int dither = ditherOff;
        double outputGain = 1.0;
        bool bypass = false;

        // Phase 4 Wave 1 (docs/limit.md). Off leaves the output as it was, to the bit.
        bool gainLock = false;
        double gainLockRefDb = 0.0;
        bool dcFilter = false;
        bool linearRelease = false;

        // Phase 4 Wave 2 (docs/limit.md): a soft clipper before the limiter, its ceiling
        // 12 - clipAmountDb above Ceiling. Off, it isn't run.
        bool clip = false;
        double clipAmountDb = 3.0;

        // Phase 4 Wave 3: multiband limiting. One band skips the split and the band limiters,
        // so the output is as before, to the bit, with the same latency.
        int bands = 1;
        bool crossoverLr48 = false;
        std::array<double, maxBands - 1> crossoverHz { 150.0, 2500.0, 8000.0 };
        std::array<double, maxBands> bandReleaseMs { 120.0, 80.0, 50.0, 40.0 };
    };

    // The clip curve, in the limiter's scale: linear to 6 dB under the ceiling, then a knee that
    // meets the ceiling flat (docs/research/limit-clip-model.py).
    static double softClip (double x, double ceiling) noexcept;

    // Allocates. Blocks of any length are processed in pieces of 256 samples.
    void prepare (double sampleRate);
    void reset();

    void setTargets (const Targets& targets) noexcept { current = targets; }
    void snapToTargets (const Targets& targets) noexcept;

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    // The latency of the active oversampling setting: constant for any Lookahead.
    int getLatencySamples() const noexcept { return latencyFor (active); }
    int latencyFor (int oversamplingIndex, bool linearPhase, int bands = 1) const noexcept;

    // Phase 4 Wave 3: the band stage's delay, 0 at one band.
    int bandLatencyFor (int bands) const noexcept { return bands > 1 ? bandLimiters[0].getLatencySamples() : 0; }

    // Requested and active settings differ while a change of oversampling or band count fades through.
    bool isSwitching() const noexcept
    {
        return active.oversampling != current.oversampling || active.linearPhase != current.linearPhase || active.bands != current.bands;
    }

    // For the editor, all lock-free.
    float takeReduction() noexcept { return reduction.exchange (0.0f, std::memory_order_relaxed); }

    // Phase 4 Wave 3: a band's largest reduction in dB since the last call (0 while it isn't running).
    float takeBandReduction (int band) noexcept { return bandReductions[(size_t) band].exchange (0.0f, std::memory_order_relaxed); }

    // The crossovers as the stage uses them for this many bands: in order, 1/3 octave apart.
    static std::array<double, maxBands - 1> sanitisedCrossovers (const Targets& targets, int bands) noexcept;

    // Phase 4 Wave 1: output samples the final clip had to catch since the last reset.
    int getOvers() const noexcept { return overs.load (std::memory_order_relaxed); }
    void requestOversReset() noexcept { oversResetRequested.store (true, std::memory_order_relaxed); }

    struct HistoryColumn
    {
        float inputDb, outputDb, reductionDb;
        float clipDb = 0.0f;   // Phase 4 Wave 2: the most the clipper took, in dB
    };

    static constexpr double historySeconds = 4.0;
    void setHistoryColumns (int columns) noexcept { historyColumns.store (std::max (1, columns), std::memory_order_relaxed); }
    int popHistory (HistoryColumn* destination, int maxColumns) noexcept;

    fancy::dsp::LoudnessMeter loudness;
    fancy::dsp::TruePeakMeter truePeakMeter;

    // The settings the shared limiter gets for these targets, at this oversampled rate.
    static fancy::dsp::TruePeakLimiter::Settings limiterSettings (const Targets& targets, double rate, int factor, double baseRate) noexcept;
    static fancy::dsp::TruePeakLimiter::Settings guardSettings (const Targets& targets, double baseRate) noexcept;
    static fancy::dsp::TruePeakLimiter::Settings bandSettings (const Targets& targets, int band, double baseRate) noexcept;

private:
    using Oversampler = juce::dsp::Oversampling<float>;

    static constexpr int maxChunk = 256;
    static constexpr int historyCapacity = 4096;

    struct Active
    {
        int oversampling = os4;
        bool linearPhase = false;
        int bands = 1; // Phase 4 Wave 3
    };

    int latencyFor (const Active& a) const noexcept { return latencyFor (a.oversampling, a.linearPhase, a.bands); }
    Oversampler* oversamplerFor (const Active& a) const noexcept;
    fancy::dsp::TruePeakLimiter& limiterFor (const Active& a) noexcept { return limiters[(size_t) a.oversampling]; }

    void processChunk (float* const* channels, int numChannels, int numSamples) noexcept;
    void configureBands (const Targets& targets, bool snap) noexcept;
    void processBands (float* a, float* b, int numSamples, bool listen) noexcept;
    double ditherSample (int channel, double x) noexcept;
    void pushHistory (float inputDb, float outputDb, float reductionDb, float clipDb) noexcept;

    double rate = 48000.0;
    Targets current;
    Active active;

    // One limiter per oversampling factor, each prepared at its own rate.
    std::array<fancy::dsp::TruePeakLimiter, 4> limiters;

    // The true-peak guard: the same shared limiter at the base rate, after the downsampling
    // filters, with a longer interpolator. It catches the peaks those filters add.
    fancy::dsp::TruePeakLimiter guard;
    std::array<std::unique_ptr<Oversampler>, 6> oversamplers;   // 2x, 4x, 8x, each minimum and linear phase
    std::array<int, 6> oversamplerLatencies {};

    // Dry path for bypass, delayed by the latency.
    std::array<std::vector<float>, 2> dryLines;
    int dryPosition = 0, dryLength = 1;

    std::array<std::vector<float>, 2> scratch;

    fancy::dsp::Smoother inputGain, ceilingGain, outputGain, bypassMix, switchGain;
    fancy::dsp::Smoother clipCeiling; // Phase 4 Wave 2, in the limiter's scale
    int holdOffSamples = 0;

    // Dither: a seeded generator, so renders repeat exactly.
    std::uint32_t noiseState = 0x2545f491u;
    std::array<std::array<double, 2>, 2> ditherError {};

    // History.
    std::atomic<int> historyColumns { 340 };
    int columnFill = 0;
    float columnInput = -200.0f, columnOutput = -200.0f, columnReduction = 0.0f, columnClip = 0.0f;
    std::vector<HistoryColumn> history = std::vector<HistoryColumn> ((size_t) historyCapacity);
    std::atomic<int> historyWrite { 0 }, historyRead { 0 };

    std::atomic<float> reduction { 0.0f };

    // Phase 4 Wave 1.
    double outputTargetFor (const Targets& targets) const noexcept;
    std::atomic<int> overs { 0 };
    std::atomic<bool> oversResetRequested { false };
    double dcCoefficient = 0.0;
    std::array<double, 2> dcInput {}, dcOutput {};
    bool dcWasOn = false;

    // Phase 4 Wave 3: the crossover (MultiComp's, moved to fancy-core), a limiter per band, the
    // bands' signals for one chunk, and their reductions for the editor.
    fancy::dsp::MinPhaseCrossover crossover;
    std::array<fancy::dsp::TruePeakLimiter, maxBands> bandLimiters;
    std::vector<double> bandSignals;
    std::array<std::atomic<float>, maxBands> bandReductions {};

    // Listen to removed with bands: what the bands took away, delayed to meet what the final
    // stage takes away.
    std::array<std::vector<float>, 2> removedLines;
    int removedPosition = 0, removedMask = 0;
};
