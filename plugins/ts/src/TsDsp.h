#pragma once

#include <fancy/dsp/Detector.h>
#include <fancy/dsp/Smoother.h>
#include <fancy/dsp/Svf.h>

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

// Fancy TS's signal path. See docs/ts.md; modelled in docs/research/ts-model.py.
//
//   detector (Main, Sidechain, Mid or Side) -> Highpass, Lowpass -> peak envelope (dB)
//   -> fast and slow followers (transient amount a) and the peak memory (sustain amount s)
//   gain dB = Attack x a + Sustain x s x window - Pump x 12 dB x p x (1 - a)
//   audio -> Lookahead delay -> gain (Stereo, Mid only or Side only) -> [Clip output, oversampled]
//   -> Wet, plus the dry path delayed to match -> Output, with the host's bypass.
class TsDsp
{
public:
    enum Contour { sharp, medium, smooth };
    enum Detector { mainLR, sidechainLR, mid, side };
    enum Channels { stereo, midOnly, sideOnly };
    enum Oversampling { osOff, os2, os4 };

    static constexpr int numLookaheads = 4;
    static constexpr std::array<double, numLookaheads> lookaheadMs { 0.0, 1.0, 2.0, 5.0 };
    static constexpr double floorDb = -90.0;   // below this the amounts fade to nothing

    struct Targets
    {
        double attackDb = 0.0, speedMs = 8.0, sustainDb = 0.0, lengthMs = 180.0;
        double pump = 0.0;          // 0 to 1
        double sensitivity = 0.6;   // 0 to 1
        double highpassHz = 20.0, lowpassHz = 20000.0;
        bool highpassOn = false, lowpassOn = false;
        int contour = medium;
        int detector = mainLR;
        int channels = stereo;
        bool link = true, clip = false;
        int lookahead = 0;          // index into lookaheadMs
        int oversampling = os2;
        double deadbandDb = 1.0;
        double wetGain = 1.0, dryGain = 0.0, outputGain = 1.0;
        bool listen = false, bypass = false;
    };

    TsDsp();

    void prepare (double sampleRate);
    void reset();

    void setTargets (const Targets& targets) noexcept { current = targets; }
    void snapToTargets (const Targets& targets) noexcept;

    // sidechain may be null. Any block size.
    void process (float* const* channels, int numChannels, const float* const* sidechain, int numSidechain, int numSamples) noexcept;

    // The latency of the setting in use: Lookahead, plus the oversampler's while Clip output is
    // on (Decision 2 A). It changes during process() when those do.
    int getLatencySamples() const noexcept { return latencyFor (active); }
    int latencyFor (int lookahead, bool clip, int oversampling) const noexcept;

    // The clip curve (Fancy Limit's): linear to 6 dB under 1.0, then a knee that meets it flat.
    static double softClip (double x) noexcept;

    // The display's columns: input and output peaks and the gain applied, in dB, and whether a
    // hit started in the column. Lock-free.
    struct HistoryColumn
    {
        float inputDb, outputDb, gainDb;
        bool hit;
    };

    static constexpr double historySeconds = 3.0;
    void setHistoryColumns (int columns) noexcept { historyColumns.store (std::max (1, columns), std::memory_order_relaxed); }
    int popHistory (HistoryColumn* destination, int maxColumns) noexcept;

    bool isSidechainMissing() const noexcept { return sidechainMissing.load (std::memory_order_relaxed); }

private:
    static constexpr int maxChunk = 256;
    static constexpr int historyCapacity = 4096;
    using Oversampler = juce::dsp::Oversampling<float>;

    struct Active
    {
        int lookahead = 0;
        bool clip = false;
        int oversampling = os2;
    };

    int latencyFor (const Active& a) const noexcept { return latencyFor (a.lookahead, a.clip, a.oversampling); }

    // One detector's followers (two when the channels aren't linked).
    struct Follower
    {
        double envelope = 0.0;
        double fast = -200.0, slow = -200.0, peak = -200.0;
        double pump = 0.0;
        long long since = 1LL << 40;
        bool inHit = false;
    };

    void processChunk (float* const* channels, int numChannels, const float* const* sidechain, int numSidechain, int offset, int numSamples) noexcept;
    double gainDbFor (Follower& follower, double level, double attackDb, double sustainDb, double pump, bool& hitStarted) noexcept;
    void updateCoefficients() noexcept;
    void clearDelays() noexcept;
    void pushHistory (float inputDb, float outputDb, float gainDb, bool hit) noexcept;

    double rate = 48000.0;
    Targets current;
    Active active;
    int holdOffSamples = 0;

    fancy::dsp::DetectorFilter detectors[2];
    fancy::dsp::SvfCoefficients highpassCoefficients, lowpassCoefficients;
    Follower followers[2];

    // Per-sample coefficients, from the current Speed and Length.
    double envelopeRelease = 0.0, fastAttack = 0.0, fastRelease = 0.0, slowAttack = 0.0, slowRelease = 0.0;
    double peakFall = 0.0, lengthSamples = 1.0;
    double lastSpeedMs = -1.0, lastLengthMs = -1.0;

    fancy::dsp::Smoother attack, sustain, pump, highpassBlend, lowpassBlend, wet, dry, output, bypass, listen, switchGain;

    // The Lookahead delay on the shaped path, and the dry path's delay to the full latency.
    std::array<std::vector<float>, 2> audioRing, dryRing;
    int ringSize = 1, ringMask = 0;
    long long now = 0;

    std::array<std::unique_ptr<Oversampler>, 2> oversamplers;   // 2x, 4x
    std::array<int, 2> oversamplerLatency {};
    std::array<std::array<float, maxChunk>, 2> shaped {}, delayedDry {};

    // History.
    std::atomic<int> historyColumns { 480 };
    int columnFill = 0;
    float columnInput = -200.0f, columnOutput = -200.0f, columnGain = 0.0f;
    bool columnHit = false;
    std::vector<HistoryColumn> history = std::vector<HistoryColumn> ((size_t) historyCapacity);
    std::atomic<int> historyWrite { 0 }, historyRead { 0 };

    double silentSidechainSeconds = 0.0;
    std::atomic<bool> sidechainMissing { false };
};
