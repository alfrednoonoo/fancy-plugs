// Fancy MultiComp DSP as released in v1.7.0, a copy of plugins/multicomp/src/MultiCompDsp.h and
// Crossover.h at commit c83b24a, renamed, kept so a test can check that Wave 2 items 23 to 25
// (per-band lookahead, detector EQ, per-crossover slopes) at their migrated values change nothing.

#pragma once

// ---- Crossover.h at c83b24a, renamed ----

#include <fancy/dsp/Smoother.h>
#include <fancy/dsp/Svf.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace legacymcw2
{

constexpr int maxBands = 8;
constexpr int maxCrossovers = maxBands - 1;

constexpr double lowestHz = 20.0, highestHz = 20000.0;

// 2^(1/3), the closest two crossovers may be.
constexpr double minCrossoverRatio = 1.2599210498948732;

// Dynamic phase (Phase 4 Wave 2) is appended, so stored indices keep their meaning. Its detectors
// use the LR 24 split; its audio isn't split (fancy::dsp::ShelfCascade).
enum class CrossoverType { lr24, lr48, linear, dynamic };

// Sorts out whatever automation asked for: in order, in range and at least 1/3 octave apart.
inline void sanitiseCrossovers (double* hz, int count) noexcept
{
    if (count <= 0)
        return;

    std::sort (hz, hz + count);

    hz[0] = std::max (hz[0], lowestHz);

    for (int i = 1; i < count; ++i)
        hz[i] = std::max (hz[i], hz[i - 1] * minCrossoverRatio);

    hz[count - 1] = std::min (hz[count - 1], highestHz);

    for (int i = count - 2; i >= 0; --i)
        hz[i] = std::min (hz[i], hz[i + 1] / minCrossoverRatio);
}

// Linkwitz-Riley bands from a cascade of splits. With compensation on, each lower band goes
// through the all-pass of every crossover above it, so all the bands share one phase and sum
// flat. See docs/multicomp.md.
class MinPhaseCrossover
{
public:
    void prepare (double sampleRate, bool compensateBands) noexcept
    {
        rate = sampleRate;
        compensate = compensateBands;

        for (auto& smoother : logHz)
            smoother.setTime (20.0, rate);

        reset();
    }

    void reset() noexcept
    {
        for (auto& channel : channels)
        {
            for (auto& crossover : channel.lows)
                for (auto& section : crossover)
                    section.reset();

            for (auto& crossover : channel.highs)
                for (auto& section : crossover)
                    section.reset();

            for (auto& band : channel.allPasses)
                for (auto& crossover : band)
                    for (auto& section : crossover)
                        section.reset();
        }
    }

    void setType (CrossoverType type) noexcept
    {
        steep = type == CrossoverType::lr48;
    }

    void setBandCount (int bands) noexcept
    {
        numBands = bands;
    }

    void setFrequencies (const double* hz, int count) noexcept
    {
        for (int i = 0; i < count; ++i)
            logHz[(size_t) i].setTarget (std::log (hz[i]));
    }

    void snap() noexcept
    {
        for (int i = 0; i < maxCrossovers; ++i)
        {
            logHz[(size_t) i].snapToTarget();
            updateCoefficients (i);
        }
    }

    // Writes numBands bands, each channel of each band at (band * 2 + channel) * stride.
    void process (const float* left, const float* right, double* bands, int stride, int numSamples) noexcept
    {
        const float* const inputs[] { left, right };

        const auto numCrossovers = numBands - 1;
        const auto numSections = steep ? 2 : 1;

        for (int i = 0; i < numSamples; ++i)
        {
            for (int j = 0; j < numCrossovers; ++j)
            {
                if (! logHz[(size_t) j].isSettled())
                {
                    logHz[(size_t) j].next();
                    updateCoefficients (j);
                }
            }

            for (int ch = 0; ch < 2; ++ch)
            {
                auto& state = channels[(size_t) ch];
                auto rest = (double) inputs[ch][i];

                for (int j = 0; j < numCrossovers; ++j)
                {
                    const auto& sections = coefficients[(size_t) j];

                    auto low = rest, high = rest;

                    // Two of each Butterworth stage make the Linkwitz-Riley response.
                    for (int stage = 0; stage < numSections * 2; ++stage)
                    {
                        const auto& c = sections[(size_t) (stage % numSections)];

                        low = state.lows[(size_t) j][(size_t) stage].tick (c, low).low;
                        high = state.highs[(size_t) j][(size_t) stage].tick (c, high).high;
                    }

                    if (compensate)
                    {
                        for (int above = j + 1; above < numCrossovers; ++above)
                        {
                            const auto& upper = coefficients[(size_t) above];

                            for (int section = 0; section < numSections; ++section)
                            {
                                const auto& c = upper[(size_t) section];
                                const auto band = state.allPasses[(size_t) j][(size_t) above][(size_t) section].tick (c, low).band;

                                low -= 2.0 * c.k * band;
                            }
                        }
                    }

                    bands[(j * 2 + ch) * stride + i] = low;
                    rest = high;
                }

                bands[(numCrossovers * 2 + ch) * stride + i] = rest;
            }
        }
    }

private:
    using Svf = fancy::dsp::Svf;
    using Coefficients = fancy::dsp::SvfCoefficients;

    static constexpr int maxStages = 4;

    struct Channel
    {
        std::array<std::array<Svf, maxStages>, maxCrossovers> lows, highs;
        std::array<std::array<std::array<Svf, 2>, maxCrossovers>, maxBands> allPasses;
    };

    void updateCoefficients (int crossover) noexcept
    {
        // The two Butterworth Qs of a 4th-order stage, and the one of a 2nd-order stage.
        constexpr double butterworth2 = 0.70710678118654752;
        constexpr double butterworth4a = 1.3065629648763766, butterworth4b = 0.54119610014619698;

        const auto hz = std::min (std::exp (logHz[(size_t) crossover].getCurrent()), 0.45 * rate);
        auto& sections = coefficients[(size_t) crossover];

        if (steep)
        {
            sections[0].set (hz, butterworth4a, rate);
            sections[1].set (hz, butterworth4b, rate);
        }
        else
        {
            sections[0].set (hz, butterworth2, rate);
        }
    }

    double rate = 48000.0;
    int numBands = 4;
    bool steep = false, compensate = true;

    std::array<fancy::dsp::Smoother, maxCrossovers> logHz;
    std::array<std::array<Coefficients, 2>, maxCrossovers> coefficients;
    std::array<Channel, 2> channels;
};

} // namespace legacymcw2

// ---- MultiCompDsp.h at c83b24a, renamed ----

#include <fancy/dsp/LinearCrossover.h>

#include <fancy/dsp/Compression.h>
#include <fancy/dsp/MeterTap.h>
#include <fancy/dsp/ShelfCascade.h>
#include <fancy/dsp/Smoother.h>
#include <fancy/dsp/TruePeakLimiter.h>

#include <array>
#include <atomic>
#include <vector>

// See docs/multicomp.md.
class LegacyMultiCompW2Dsp
{
public:
    enum Detector { mainLR, mainL, mainR, sideLR, sideL, sideR };
    enum Channel { stereo, left, right, mid, side };

    static constexpr double maxLookaheadMs = 10.0;

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

        // Phase 3 Q2.6: the channel the band compresses (Channel); the other one passes.
        int channel = stereo;

        // Phase 4 Wave 1 (docs/multicomp.md): out of the sum entirely. Off is the band as it was.
        bool mute = false;
    };

    struct Targets
    {
        std::array<BandSpec, legacymcw2::maxBands> bands {};
        int numBands = 4;
        int detector = mainLR;
        legacymcw2::CrossoverType crossover = legacymcw2::CrossoverType::lr24;
        double wetGain = 1.0, dryGain = 0.0, outputGain = 1.0;
        bool limitOutput = false, attack90 = false, bypass = false;

        // Phase 2.1 U: scale every band's gain change, and its attack and release.
        double depth = 1.0, time = 1.0;

        // Phase 3 Q2.6.
        double lookaheadMs = 0.0;
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
        legacymcw2::CrossoverType crossover = legacymcw2::CrossoverType::lr24;
        bool limiter = false;
        int lookahead = 0; // samples

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

    legacymcw2::MinPhaseCrossover mainCrossover, keyCrossover;
    fancy::dsp::LinearCrossover linearCrossover;
    fancy::dsp::TruePeakLimiter limiter;

    std::array<BandState, legacymcw2::maxBands> bandStates;

    std::vector<double> bands, keyBands;
    std::vector<float> inputs[4];
    std::vector<float> bypassDelay[2];
    std::vector<double> wetMix[2], dryMix[2];

    Targets current;
    Config activeConfig, wantedConfig;
    int wantedStableSamples = 0, holdSamples = 0;
    int activeLatency = 0;
    int bypassLength = 1, bypassPosition = 0;

    std::array<double, legacymcw2::maxCrossovers> crossoverHz {};
    std::array<double, legacymcw2::maxCrossovers> designedHz {};
    int designedBands = 0;
    std::atomic<bool> designNeeded { true };
    bool synchronousDesign = false;

    fancy::dsp::Smoother wet, dry, output, bypass, limiterAmount, switchGain;

    // Phase 4 Wave 2: Dynamic phase. Each band's gain per sample, on L/R (Stereo, Left and Right
    // bands) and on M/S (Mid and Side bands), for the shelf cascades; and the blend to the LR 24
    // bands while a band is soloed or muted (Decision 2 A).
    bool isDynamic() const noexcept { return activeConfig.crossover == legacymcw2::CrossoverType::dynamic; }
    void processDynamic (int numSamples, int lookahead) noexcept;

    std::vector<double> gainsLR, gainsMS; // (band * 2 + channel) * maxChunk
    std::array<fancy::dsp::ShelfCascade<legacymcw2::maxBands>, 2> cascadeLR, cascadeMS;
    std::vector<double> dynamicOut[2];
    fancy::dsp::Smoother bandPath; // 0 dynamic, 1 the LR 24 bands
    std::array<double, legacymcw2::maxCrossovers> shelfHz {};
    int shelfCount = -1;

    // Phase 3 Q2.6: the bands' audio waits Lookahead behind their detectors.
    std::array<std::array<std::vector<double>, 2>, legacymcw2::maxBands> lookaheadRings;
    int lookaheadLength = 1, lookaheadPosition = 0;
};
