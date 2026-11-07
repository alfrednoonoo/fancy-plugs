#pragma once

#include "FirShared.h"

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
    double reductionGain = 0.125, offsetPower = 1.0, smoothing = 0.35, subReleaseMs = 80.0;
    bool reduceArtifacts = true, listen = false, sidechain = false;
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
    void publishReduction() noexcept;

    double perHop (double milliseconds) const noexcept;
    float binPower (const std::vector<float>& spectrum, int bin) const noexcept;

    Config engineConfig;
    Shared& shared;

    double rate;
    int frameLength, fftLength, bins, hop, synthesisLength, extra, latency;
    int stageInterval = 0;

    std::unique_ptr<FFT> fft;
    std::vector<float> analysisWindow, synthesisWindow;
    double levelScale = 1.0, powerScale = 1.0;

    int inputSize, inputMask, outputSize, outputMask;
    std::vector<float> inputRings[4], outputRings[2];
    long long time = 0;

    // The frame in progress.
    long long frameEnd = -1;
    int nextStage = numStages;
    int frameChannels = 2;
    bool frameKey = false, frameLearns = false;
    FrameSettings frameSettings;
    std::vector<float> spectra[4];

    // Per bin, per channel.
    std::vector<float> gains[2], envelopes[2], priorSnr[2], smoothedKey[2], mask[2];
    std::vector<float> binHz, curveDb[numCurves], profilePower, scratch;
    int seenCurveVersion[numCurves] { -1, -1, -1 };
    int seenProfileVersion = -1;
    bool profileReady = false;

    std::vector<double> learnSum;
    int learnFrames = 0;

    std::vector<int> binColumn;
};

} // namespace fir
