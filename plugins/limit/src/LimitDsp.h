#pragma once

#include <fancy/dsp/LoudnessMeter.h>
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
    };

    // Allocates. Blocks of any length are processed in pieces of 256 samples.
    void prepare (double sampleRate);
    void reset();

    void setTargets (const Targets& targets) noexcept { current = targets; }
    void snapToTargets (const Targets& targets) noexcept;

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    // The latency of the active oversampling setting: constant for any Lookahead.
    int getLatencySamples() const noexcept { return latencyFor (active); }
    int latencyFor (int oversamplingIndex, bool linearPhase) const noexcept;

    // Requested and active settings differ while a change of oversampling fades through.
    bool isSwitching() const noexcept { return active.oversampling != current.oversampling || active.linearPhase != current.linearPhase; }

    // For the editor, all lock-free.
    float takeReduction() noexcept { return reduction.exchange (0.0f, std::memory_order_relaxed); }

    struct HistoryColumn
    {
        float inputDb, outputDb, reductionDb;
    };

    static constexpr double historySeconds = 4.0;
    void setHistoryColumns (int columns) noexcept { historyColumns.store (std::max (1, columns), std::memory_order_relaxed); }
    int popHistory (HistoryColumn* destination, int maxColumns) noexcept;

    fancy::dsp::LoudnessMeter loudness;
    fancy::dsp::TruePeakMeter truePeakMeter;

    // The settings the shared limiter gets for these targets, at this oversampled rate.
    static fancy::dsp::TruePeakLimiter::Settings limiterSettings (const Targets& targets, double rate, int factor, double baseRate) noexcept;

private:
    using Oversampler = juce::dsp::Oversampling<float>;

    static constexpr int maxChunk = 256;
    static constexpr int historyCapacity = 4096;

    struct Active
    {
        int oversampling = os4;
        bool linearPhase = false;
    };

    int latencyFor (const Active& a) const noexcept { return latencyFor (a.oversampling, a.linearPhase); }
    Oversampler* oversamplerFor (const Active& a) const noexcept;
    fancy::dsp::TruePeakLimiter& limiterFor (const Active& a) noexcept { return limiters[(size_t) a.oversampling]; }

    void processChunk (float* const* channels, int numChannels, int numSamples) noexcept;
    double ditherSample (int channel, double x) noexcept;
    void pushHistory (float inputDb, float outputDb, float reductionDb) noexcept;

    double rate = 48000.0;
    Targets current;
    Active active;

    // One limiter per oversampling factor, each prepared at its own rate.
    std::array<fancy::dsp::TruePeakLimiter, 4> limiters;
    std::array<std::unique_ptr<Oversampler>, 6> oversamplers;   // 2x, 4x, 8x, each minimum and linear phase
    std::array<int, 6> oversamplerLatencies {};

    // Dry path for bypass, delayed by the latency.
    std::array<std::vector<float>, 2> dryLines;
    int dryPosition = 0, dryLength = 1;

    std::array<std::vector<float>, 2> scratch;

    fancy::dsp::Smoother inputGain, ceilingGain, outputGain, bypassMix, switchGain;
    int holdOffSamples = 0;

    // Dither: a seeded generator, so renders repeat exactly.
    std::uint32_t noiseState = 0x2545f491u;
    std::array<std::array<double, 2>, 2> ditherError {};

    // History.
    std::atomic<int> historyColumns { 340 };
    int columnFill = 0;
    float columnInput = -200.0f, columnOutput = -200.0f, columnReduction = 0.0f;
    std::vector<HistoryColumn> history = std::vector<HistoryColumn> ((size_t) historyCapacity);
    std::atomic<int> historyWrite { 0 }, historyRead { 0 };

    std::atomic<float> reduction { 0.0f };
};
