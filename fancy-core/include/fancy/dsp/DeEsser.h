#pragma once

#include <fancy/dsp/Compression.h>
#include <fancy/dsp/LinearCrossover.h>
#include <fancy/dsp/MeterTap.h>
#include <fancy/dsp/Smoother.h>
#include <fancy/dsp/Svf.h>

#include <array>
#include <atomic>
#include <vector>

namespace fancy::dsp
{

// Fancy DS's engine, shared with Fancy DS-Mini (docs/ds.md, docs/ds-mini.md). Moved from
// plugins/ds unchanged, so Fancy DS is bit-identical; DS still calls it DsDsp.
class DeEsser
{
public:
    enum Detector { mainLR, mainMid, mainSide, mainL, mainR, sideLR, sideL, sideR };
    enum Lookahead { lookaheadOff, lookahead1, lookahead2, lookahead5 };

    static constexpr double minHz = 2000.0, maxHz = 16000.0;
    static constexpr double lowMinHz = 1000.0, lowMaxHz = 8000.0;   // Phase 4 Wave 2: the Low band
    static constexpr double ratioSlope = 0.875;    // 8:1
    static constexpr double kneeDb = 6.0;
    static constexpr double fullBandFloorDb = -60.0;

    struct Targets
    {
        double thresholdDb = -28.0, freqHz = 6200.0, widthOctaves = 1.0;
        bool shelf = false;
        double rangeDb = -12.0, attackMs = 0.5, releaseMs = 60.0;
        int detector = mainLR;
        bool wide = false, relative = true, listen = false, link = true;
        int lookahead = lookaheadOff;
        bool linearPhase = false;
        double outputGain = 1.0;
        bool bypass = false;

        // Fancy DS's detection slope (docs/ds.md, Phase 4 Wave 1): 1, 2 or 4 detector filters in
        // a row, 12, 24 or 48 dB/oct. 1 is the detector as it was; DS-Mini never changes it.
        int slopeStages = 1;

        // Fancy DS's dual-band detection (docs/ds.md, Phase 4 Wave 2): a second, Low band with its
        // own detector, threshold and range, cut after the High band. Off, it isn't run, and the
        // engine is as it was to the bit. DS-Mini never turns it on.
        bool dual = false;
        double lowFreqHz = 3500.0, lowWidthOctaves = 1.0, lowThresholdDb = -20.0, lowRangeDb = -9.0;
    };

    static double lookaheadMs (int choice) noexcept;

    // The band-pass's Q for a width in octaves.
    static double qForWidth (double octaves) noexcept;

    // Allocates.
    void prepare (double sampleRate);
    void reset();

    void setTargets (const Targets& targets) noexcept { current = targets; }
    void snapToTargets (const Targets& targets) noexcept;

    // Takes over another engine's running state: detector levels, filters, the gain and the
    // delay lines, so this one carries on where it is. Then snapToTargets sets new settings.
    // For Fancy DS-Mini's profile changes; both must be prepared at the same rate. No allocation.
    void continueFrom (const DeEsser& other) noexcept;

    void process (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int numSamples) noexcept;

    int getLatencySamples() const noexcept { return latencyFor (activeLookahead, activeLinear); }
    int latencyFor (int lookahead, bool linear) const noexcept;

    // The linear-phase split, designed on a worker thread, or in-line in an offline render.
    void updateLinearDesign (const Targets& targets) noexcept;
    bool isDesignPending (const Targets& targets) const noexcept;
    void setSynchronousDesign (bool shouldBeSynchronous) noexcept { synchronousDesign = shouldBeSynchronous; }

    // For the editor, all lock-free.
    float takeReduction() noexcept { return reduction.exchange (0.0f, std::memory_order_relaxed); }
    float takeLowReduction() noexcept { return lowReduction.exchange (0.0f, std::memory_order_relaxed); }

    // The detector level the Threshold meter shows: absolute, or relative to the full band.
    fancy::dsp::MeterTap levelMeter;
    fancy::dsp::MeterTap lowLevelMeter;   // the Low band's, while it runs

    struct HistoryColumn
    {
        float levelDb, reductionDb;
        float lowLevelDb = -200.0f, lowReductionDb = 0.0f;   // Phase 4 Wave 2
    };

    static constexpr double historySeconds = 3.0;
    void setHistoryColumns (int columns) noexcept { historyColumns.store (std::max (1, columns), std::memory_order_relaxed); }
    int popHistory (HistoryColumn* destination, int maxColumns) noexcept;

private:
    static constexpr int maxChunk = 256;
    static constexpr int historyCapacity = 4096;

    struct Edges
    {
        double low = 0.0, high = 0.0;
        int bands = 3;
        double lowBandLow = 0.0, lowBandHigh = 0.0;   // with two bands, the Low band's edges, under low

        bool operator== (const Edges&) const = default;
    };

    static Edges edgesFor (const Targets& targets) noexcept;
    static Edges dualEdgesFor (const Targets& targets) noexcept;

    void processChunk (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int numSamples) noexcept;
    void updateFilters() noexcept;
    void updateLowFilters() noexcept;
    void resetLow() noexcept;
    void pushHistory (const HistoryColumn& column) noexcept;

    double rate = 48000.0;
    Targets current;

    // Settings that change the latency, as they're running now.
    int activeLookahead = lookaheadOff;
    bool activeLinear = false;
    bool activeShelf = false;
    bool activeDual = false;
    int holdOffSamples = 0;

    // Filters: one detector per channel, and the minimum-phase split per audio channel.
    fancy::dsp::SvfCoefficients coefficients;
    double warpedFreq = 0.4, bandQ = 1.414;
    std::array<fancy::dsp::Svf, 2> detectors, splits;
    std::array<std::array<fancy::dsp::Svf, 3>, 2> extraDetectors; // Phase 4 Wave 1: the slope's further stages
    fancy::dsp::Smoother logFreq, width;

    // Phase 4 Wave 2: the Low band. lowRunning stays true after Dual band goes off until Low's
    // reduction has fully released, so turning it off doesn't click.
    fancy::dsp::SvfCoefficients lowCoefficients;
    double lowWarpedFreq = 0.4, lowBandQ = 1.414;
    std::array<fancy::dsp::Svf, 2> lowDetectors, lowSplits;
    std::array<std::array<fancy::dsp::Svf, 3>, 2> lowExtraDetectors;
    fancy::dsp::Smoother lowLogFreq, lowWidth;
    std::array<double, 2> lowPeakDb { -200.0, -200.0 };
    std::array<int, 2> lowHoldLeft { 0, 0 };
    std::array<fancy::dsp::EnvelopeFollower, 2> lowFollowers;
    bool lowRunning = false;
    int filterCountdown = 0;

    // Levels.
    std::array<double, 2> peakDb { -200.0, -200.0 }, meanSquare { 0.0, 0.0 };
    std::array<int, 2> holdLeft { 0, 0 };
    double holdSamples = 48.0, fallPerSample = 0.0004, rmsCoefficient = 0.0004;

    fancy::dsp::Ballistics ballistics;
    std::array<fancy::dsp::EnvelopeFollower, 2> followers;

    // Delays: the lookahead, the linear split's raw path, the gain and the dry path for bypass.
    std::array<std::vector<float>, 2> lookaheadLines, rawLines, dryLines;
    std::array<std::vector<double>, 2> gainLines, lowGainLines;
    int lookaheadPosition = 0, rawPosition = 0, dryPosition = 0, gainPosition = 0;
    int lookaheadSize = 1, rawSize = 1, drySize = 1, gainSize = 1;

    fancy::dsp::LinearCrossover crossover;
    std::vector<double> bandScratch;
    std::array<std::vector<float>, 2> crossoverInput;
    Edges designedEdges { -1.0, -1.0, 0 };
    std::atomic<bool> designNeeded { true };
    bool synchronousDesign = false;

    fancy::dsp::Smoother outputGain, bypassMix, switchGain;

    // History.
    std::atomic<int> historyColumns { 510 };
    int columnFill = 0;
    float columnLevel = -200.0f, columnReduction = 0.0f, columnLowLevel = -200.0f, columnLowReduction = 0.0f;
    std::vector<HistoryColumn> history = std::vector<HistoryColumn> ((size_t) historyCapacity);
    std::atomic<int> historyWrite { 0 }, historyRead { 0 };

    std::atomic<float> reduction { 0.0f }, lowReduction { 0.0f };
};

} // namespace fancy::dsp
