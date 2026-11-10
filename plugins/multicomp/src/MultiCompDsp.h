#pragma once

#include "Crossover.h"
#include <fancy/dsp/LinearCrossover.h>

#include <fancy/dsp/Compression.h>
#include <fancy/dsp/MeterTap.h>
#include <fancy/dsp/Smoother.h>
#include <fancy/dsp/TruePeakLimiter.h>

#include <array>
#include <atomic>
#include <vector>

// See docs/multicomp.md.
class MultiCompDsp
{
public:
    enum Detector { mainLR, mainL, mainR, sideLR, sideL, sideR };

    static constexpr double infiniteRatio = 100.0;
    static constexpr double maxExpansionDb = 24.0;

    struct BandSpec
    {
        double topHz = 20000.0;
        double thresholdDb = 0.0, ratio = 2.0, kneeDb = 6.0;
        double attackMs = 10.0, releaseMs = 120.0, rmsMs = 5.0, gainDb = 0.0;
        bool active = true, solo = false, autoMakeup = false, autoRelease = true, feedback = false, sidechain = false;

        // Phase 2.1 U: upward compression, off by default.
        bool upward = false;
        double upThresholdDb = -40.0, upRatio = 2.0, maxLiftDb = 12.0, floorDb = -70.0;
    };

    struct Targets
    {
        std::array<BandSpec, multicomp::maxBands> bands {};
        int numBands = 4;
        int detector = mainLR;
        multicomp::CrossoverType crossover = multicomp::CrossoverType::lr24;
        double wetGain = 1.0, dryGain = 0.0, outputGain = 1.0;
        bool limitOutput = false, attack90 = false, bypass = false;

        // Phase 2.1 U: scale every band's gain change, and its attack and release.
        double depth = 1.0, time = 1.0;
    };

    // The upward part of a band's static curve, in dB, before ballistics and Depth: the lift
    // for a level, with the Up threshold clamped to the Threshold, Max lift and the Floor fade.
    static double liftFor (double levelDb, double thresholdDb, const BandSpec& band) noexcept;

    // The band's whole static curve, in dB of gain change, as the transfer-curve display draws it.
    static double staticChangeDb (double levelDb, const BandSpec& band, double depth) noexcept;

    // The gain computer's slope. Above 1:1 it's positive, and below it's negative.
    static double slopeFor (double ratio) noexcept;

    // Estimated from the settings alone, so it can't pump. See docs/multicomp.md.
    static double autoMakeupDb (const BandSpec& band) noexcept;

    void prepare (double sampleRate);
    void reset();

    void setTargets (const Targets& targets) noexcept;

    // Jumps straight to the targets, for use before processing starts.
    void snapToTargets (const Targets& targets) noexcept;

    void process (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int numSamples) noexcept;

    int getLatencySamples() const noexcept { return activeLatency; }

    // Worker thread, or inline in an offline render. Designs the linear-phase filters when
    // the crossovers have moved.
    void updateLinearDesign (const Targets& targets) noexcept;

    // Whether the linear-phase crossover design is behind these settings.
    bool isDesignPending (const Targets& targets) const noexcept;

    // Runs the design inline, so an offline render is repeatable.
    void setSynchronousDesign (bool shouldBeSynchronous) noexcept { synchronousDesign = shouldBeSynchronous; }

    // Largest gain reduction since the last call, as a positive number of dB.
    float takeGainReduction (int band) noexcept { return bandStates[(size_t) band].gainReduction.exchange (0.0f, std::memory_order_relaxed); }

    fancy::dsp::MeterTap& getBandInput (int band) noexcept { return bandStates[(size_t) band].input; }

private:
    static constexpr int maxChunk = 128;

    struct Config
    {
        int bands = 4;
        multicomp::CrossoverType crossover = multicomp::CrossoverType::lr24;
        bool limiter = false;

        bool operator== (const Config&) const = default;
    };

    struct BandState
    {
        fancy::dsp::Smoother threshold, slope, knee, gainDb, rms, activeMix, soloGain;
        fancy::dsp::Smoother upThreshold, upSlope, maxLift, floor, depth;
        fancy::dsp::Ballistics ballistics;
        fancy::dsp::EnvelopeFollower follower, upFollower;

        double meanSquare[2] {};
        double previousOutput[2] {};
        double rmsCoefficient = 1.0;
        double lastAttack = -1.0, lastRelease = -1.0, lastTime = -1.0;
        bool lastAuto = false, lastAttack90 = false;

        std::atomic<float> gainReduction { 0.0f };
        fancy::dsp::MeterTap input;
    };

    double* bandBuffer (std::vector<double>& buffer, int band, int channel) noexcept { return buffer.data() + (band * 2 + channel) * maxChunk; }

    void processChunk (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int offset, int numSamples) noexcept;
    void compressBand (int band, int numSamples, bool keyAvailable) noexcept;
    void updateConfig (int numSamples) noexcept;
    void applyConfig (const Config& config) noexcept;
    void pushCrossovers() noexcept;
    bool needsKey() const noexcept;
    Config configFor (const Targets& targets) const noexcept;
    int latencyOf (const Config& config) const noexcept;
    void resetCore() noexcept;

    double sampleRate = 48000.0;

    multicomp::MinPhaseCrossover mainCrossover, keyCrossover;
    fancy::dsp::LinearCrossover linearCrossover;
    fancy::dsp::TruePeakLimiter limiter;

    std::array<BandState, multicomp::maxBands> bandStates;

    std::vector<double> bands, keyBands;
    std::vector<float> inputs[4];
    std::vector<float> bypassDelay[2];
    std::vector<double> wetMix[2], dryMix[2];

    Targets current;
    Config activeConfig, wantedConfig;
    int wantedStableSamples = 0, holdSamples = 0;
    int activeLatency = 0;
    int bypassLength = 1, bypassPosition = 0;

    std::array<double, multicomp::maxCrossovers> crossoverHz {};
    std::array<double, multicomp::maxCrossovers> designedHz {};
    int designedBands = 0;
    std::atomic<bool> designNeeded { true };
    bool synchronousDesign = false;

    fancy::dsp::Smoother wet, dry, output, bypass, limiterAmount, switchGain;
};
