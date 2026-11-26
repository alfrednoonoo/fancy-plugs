#pragma once

#include "FirShared.h"

#include <fancy/dsp/StftFrames.h>

#include <juce_dsp/juce_dsp.h>

#include <memory>
#include <vector>

namespace fir
{

// What the engine needs from the parameters, once per block.
struct FrameSettings
{
    int mode = eq;
    double gateFloorGain = 0.01, gateAttackMs = 1.0, gateReleaseMs = 80.0;
    double compSlope = 0.5, compAttackMs = 5.0, compReleaseMs = 80.0;
    double compRatio = 2.0; // Phase 4 Wave 2: the knob, which the ratio curve is relative to
    double reductionGain = 0.125, offsetPower = 1.0, smoothing = 0.35, subReleaseMs = 80.0;
    bool reduceArtifacts = true, listen = false, sidechain = false;

    // Phase 2.1 A: Learned, Adaptive or Both, and the minimum search's window in seconds.
    int source = learnedSource;
    double adaptSeconds = 2.0;

    // Phase 3 Q2.7: Resonance mode. See docs/fir.md, "Phase 3 Q2.7".
    double resDepthDb = 6.0, resSharpness = 0.5, resAttackMs = 5.0, resReleaseMs = 80.0;
    double resLowHz = 100.0, resHighHz = 20000.0;

    // Phase 4 Wave 1 (docs/fir.md): Resonance tilt, -1 to 1. 0 is today's flat Depth.
    double resTilt = 0.0;
};

// One short-time Fourier transform engine. See docs/fir.md.
//
// The constructor allocates, so it's built on a worker thread, or inline offline. process()
// never allocates.
class SpectralEngine
{
public:
    struct Config
    {
        int fftSize = 4096;
        bool lowLatency = false;
        bool convolve = false;

        // Phase 4 Wave 2 (item 28): mid and side instead of left and right. A change builds a
        // new engine, as a size change does, so the switch crossfades. Ignored by Convolve L/R
        // and on a mono input.
        bool midSide = false;

        bool operator== (const Config&) const = default;
    };

    static int latencyOf (const Config& config) noexcept;

    SpectralEngine (const Config& config, double sampleRate, Shared& shared);

    const Config& getConfig() const noexcept { return engineConfig; }
    int getLatency() const noexcept { return latency; }

    void reset() noexcept;

    // Replaces the output with the processed input, delayed by the latency. keyLeft and
    // keyRight may be null. Only the engine that's heard should pass learns as true.
    void process (const float* inLeft, const float* inRight, const float* keyLeft, const float* keyRight,
                  float* outLeft, float* outRight, int numChannels, int numSamples,
                  const FrameSettings& settings, bool learns) noexcept;

private:
    using FFT = juce::dsp::FFT;

    static constexpr int numStages = 6;

    void runStage (int stage) noexcept;
    void analyse (int channel) noexcept;
    void computeMasks() noexcept;
    void synthesise (int channel) noexcept;
    void convolveChannels() noexcept;

    void refreshCurve (int curve) noexcept;
    void refreshProfile() noexcept;
    void learnFrame() noexcept;

    // Phase 4 Wave 2 (item 28): true while this frame is mid and side. A domain is 0 (both
    // channels, or the mid) or 1 (the side).
    bool midSideFrame() const noexcept { return engineConfig.midSide && ! engineConfig.convolve && frameChannels > 1; }
    int domainOf (int channel) const noexcept { return midSideFrame() ? channel : 0; }
    int curveFor (int curve, int channel) const noexcept { return domainOf (channel) == 1 ? sideCurve (curve) : curve; }
    int shownDomain() const noexcept { return midSideFrame() && shared.viewSide.load (std::memory_order_relaxed) ? 1 : 0; }

    // Phase 2.1 A: Martin's minimum statistics, once per frame. See docs/phase2_1-A.md. In
    // Mid/Side each domain has its own tracker, on its own channel.
    void trackNoise (int domain, bool publishes) noexcept;
    void startTracking (int domain) noexcept;
    void publishTracking (int domain) noexcept;

    // Phase 3 Q2.7: one linked gain for both channels, into resonanceGain. In Mid/Side, one gain
    // per domain, each from its own channel.
    void computeResonance (int domain, double* reductionColumns) noexcept;
    void prepareResonanceWindows (double sharpness) noexcept;
    void prepareResonanceRange (double lowHz, double highHz) noexcept;
    void prepareResonanceTilt (double tilt) noexcept;

public:
    // Phase 4 Wave 1: Depth's factor at a frequency for a tilt (-1 to 1): log-linear about 1 kHz,
    // 2 at 16 kHz and 0.5 at 62.5 Hz for +1.
    static double tiltFactor (double hz, double tilt) noexcept;

private:

    double perHop (double milliseconds) const noexcept;
    void measureLevels (const std::vector<float>& spectrum) noexcept;
    float binPower (const std::vector<float>& spectrum, int bin) const noexcept;

    Config engineConfig;
    Shared& shared;

    double rate;
    int frameLength, fftLength, bins, hop, synthesisLength, extra, latency;

    // Phase 4 Wave 3: the framing, in fancy-core (docs/eq.md, decision 5 A).
    fancy::dsp::StftFrames framing;
    double levelScale = 1.0, powerScale = 1.0;

    // The frame in progress.
    int frameChannels = 2;
    bool frameKey = false, frameLearns = false;
    FrameSettings frameSettings;
    std::vector<float> spectra[4];

    // Per bin, per channel.
    std::vector<float> gains[2], envelopes[2], priorSnr[2], smoothedKey[2], mask[2];
    std::vector<float> binHz, curveDb[numCurves], profilePower[2], scratch, detectorLevels;
    int seenCurveVersion[numCurves] { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    // Phase 4 Wave 2: per domain, the depth curve at 100 % and the ratio curve at 0 everywhere,
    // which take the old paths, to the bit.
    bool depthFlat[2] { true, true }, ratioFlat[2] { true, true };
    int seenProfileVersion = -1;
    bool profileReady[2] { false, false };

    std::vector<double> learnSum[2];
    int learnFrames = 0;

    // The adaptive estimator's state, per bin. ring holds the last trackSubwindows sub-window
    // minima, bin after bin.
    static constexpr int trackSubwindows = 8;

    struct Tracker
    {
        std::vector<float> power, smoothed, noise, moment1, moment2, qInverse,
            actMin, subMin, ringMin, ring, ratio, sort, presence;
        std::vector<double> prefix;
        std::vector<int> hold;
        double alphaC = 1.0;
        int subFrames = 0, subLength = 1, ringIndex = 0, publishCountdown = 0;
        bool started = false;
    };

    Tracker trackers[2];

    std::vector<int> binColumn;

    // Resonance mode's state, per bin. The detector is shared by both channels (linked), or in
    // Mid/Side is one per domain.
    std::vector<float> resDetector[2], resLevel, resClipped, resEnvelope, resReduction[2], resWeight, resonanceGain[2];
    std::vector<double> resPrefix;
    std::vector<int> resLow, resHigh;
    double resWindowSharpness = -1.0, resRangeLow = -1.0, resRangeHigh = -1.0;
    std::vector<float> resTiltFactor; // Phase 4 Wave 1, per bin
    double resTiltShown = 0.0;
    bool resStarted[2] { false, false };
};

} // namespace fir
