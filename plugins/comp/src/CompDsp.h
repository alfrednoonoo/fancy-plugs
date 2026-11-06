#pragma once

#include "TruePeakLimiter.h"

#include <fancy/dsp/Smoother.h>
#include <fancy/dsp/Svf.h>

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

// See docs/comp.md.
class CompDsp
{
public:
    enum Detector { mainLR, mainL, mainR, sideLR, sideL, sideR, mid, side };
    enum Oversampling { off, x2, x4 };

    static constexpr double infiniteRatio = 100.0;

    struct Targets
    {
        double thresholdDb = 0.0;
        double ratio = 4.0;
        double kneeDb = 0.5;
        double attackMs = 3.0;
        double releaseMs = 200.0;
        double preCompMs = 0.0;
        double rmsMs = 5.0;
        double lowpassHz = 24000.0;
        double highpassHz = 20.0;
        double wetGain = 1.0;
        double dryGain = 0.0;
        double outputGain = 1.0;
        double linkAmount = 1.0;
        int detector = mainLR;
        int oversampling = off;
        bool weirdKnee = false;
        bool classic = false;
        bool autoRelease = false;
        bool linearRelease = false;
        bool attack90 = false;
        bool minimumPhase = false;
        bool linkChannels = true;
        bool feedback = false;
        bool autoMakeup = false;
        bool previewFilter = false;
        bool limitOutput = false;
        bool bypass = false;
    };

    CompDsp();
    ~CompDsp();

    void prepare (double sampleRate);
    void reset();

    void setTargets (const Targets& targets);

    // Jumps straight to the targets, for use before processing starts.
    void snapToTargets (const Targets& targets);

    void process (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int numSamples);

    int getLatencySamples() const noexcept { return activeLatency; }

    // Largest gain reduction since the last call, as a positive number of dB.
    float takeGainReduction (int channel) noexcept { return gainReduction[(size_t) channel].exchange (0.0f, std::memory_order_relaxed); }

    // The static curve, exposed for tests. Returns gain reduction as a positive number of dB.
    static double computeReduction (double levelDb, double thresholdDb, double slope, double kneeDb, bool weird) noexcept;

private:
    static constexpr int maxChunk = 64;
    static constexpr int maxFactor = 4;
    static constexpr int numOversamplers = 4;
    static constexpr double maxPreCompMs = 250.0;

    struct Config
    {
        int oversampling = off;
        int preCompSamples = 0;
        bool minimumPhase = false;
        bool limiter = false;

        bool operator== (const Config&) const = default;
    };

    struct ChannelState
    {
        fancy::dsp::Svf highPass, lowPass;
        std::vector<float> dryDelay, preCompDelay;
        double meanSquare = 0.0;
        double envelope = 0.0;
        double slowEnvelope = 0.0;
        double previousOutput = 0.0;
    };

    using Oversampler = juce::dsp::Oversampling<float>;

    void processChunk (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int offset, int numSamples);
    void compress (float* const* signal, const float* const* key, int numSamples);
    void updateConfig (int numSamples);
    void applyConfig (const Config& config);
    void updateBallistics() noexcept;
    double follow (ChannelState& state, double target) noexcept;
    Config configFor (const Targets& targets) const noexcept;
    int latencyOf (const Config& config) const noexcept;
    int factorOf (const Config& config) const noexcept;
    Oversampler* oversamplerFor (const Config& config, bool sidechainPath) const noexcept;
    void resetCore();

    double sampleRate = 48000.0, internalRate = 48000.0;

    std::array<std::unique_ptr<Oversampler>, numOversamplers> mainOversamplers, keyOversamplers;
    std::array<int, numOversamplers> oversamplerLatencies {};

    juce::AudioBuffer<float> mainBuffer, keyBuffer;
    std::array<ChannelState, 2> channels;
    TruePeakLimiter limiter;

    fancy::dsp::SvfCoefficients highPassCoefficients, lowPassCoefficients;

    Targets current;
    Config activeConfig, wantedConfig;
    int wantedStableSamples = 0;
    int holdSamples = 0;
    int activeLatency = 0;
    int preCompLength = 1, dryLength = 1;
    int preCompPosition = 0, dryPosition = 0;

    double attackCoefficient = 1.0, releaseCoefficient = 1.0, fastReleaseCoefficient = 1.0, linearReleaseStep = 1.0, rmsCoefficient = 1.0;

    fancy::dsp::Smoother threshold, slope, knee, makeup, rms, previewAmount, lowPassAmount, highPassAmount;
    fancy::dsp::Smoother logLowPass, logHighPass;
    fancy::dsp::Smoother wet, dry, output, bypass, limiterAmount, switchGain;

    std::array<std::atomic<float>, 2> gainReduction {};
};
