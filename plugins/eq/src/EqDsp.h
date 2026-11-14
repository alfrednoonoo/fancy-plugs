#pragma once

#include "EqFilters.h"
#include "LinearPhase.h"

#include <fancy/dsp/Smoother.h>

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

// The audio path. No allocation or locks in process(), except that an offline render designs
// the linear-phase FIR on the calling thread, where waiting is allowed.
class EqDsp
{
public:
    static constexpr int maxChannels = 2;
    static constexpr int stepSamples = 16;

    struct Targets
    {
        std::array<eq::BandSpec, eq::maxBands> bands;
        std::array<eq::DynamicSpec, eq::maxBands> dynamics;
        double gain = 1.0;
        double outputGain = 1.0;
        int firLength = 2;
        int oversampling = 0;
        bool linearPhase = false;
        bool bypass = false;
    };

    // The channel count is the bus's, when it's known: Linear phase designs for it (a mono bus
    // treats every band as Stereo). process() corrects it if the host sends something else.
    void prepare (double sampleRate, int numChannels = maxChannels);
    void reset();

    void setTargets (const Targets& targets) noexcept { current = targets; }

    // Jumps straight to these settings, for prepareToPlay.
    void snapToTargets (const Targets& targets);

    // The sidechain feeds the dynamic bands that ask for it; it may be null.
    void process (float* const* channels, int numChannels, int numSamples,
                  const float* const* sidechain = nullptr, int numSidechain = 0) noexcept;

    // Each band's current dynamic gain change in dB, for the graph (Phase 2.1 D).
    float getDynamicChange (int band) const noexcept { return liveChange[(size_t) band].load (std::memory_order_relaxed); }

    // Whether a band is dynamic now: DYN on, a type that allows it, and Minimum phase.
    bool isDynamic (int band) const noexcept;

    // Test only: redesign moving bands every this many samples instead of 16.
    void setStepLength (int samples) noexcept { stepLength = std::clamp (samples, 1, (int) stepSamples); }

    int getLatencySamples() const noexcept { return activeLatency.load (std::memory_order_relaxed); }

    // With this on, process() designs the FIR itself, so a render is repeatable.
    void setSynchronousDesign (bool shouldBeSynchronous) noexcept { synchronousDesign.store (shouldBeSynchronous); }

    // Worker thread: designs an FIR if these targets need a new one.
    void updateLinearDesign (const Targets& targets) noexcept;

private:
    static constexpr int numOversamplers = 2;

    using Oversampler = juce::dsp::Oversampling<float>;

    struct Config
    {
        int oversampling = 0;
        int firLength = 2;
        bool linearPhase = false;

        bool operator== (const Config&) const = default;
    };

    struct Coefficients
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    };

    struct Band
    {
        eq::BandSpec active;
        fancy::dsp::Smoother logFrequency, gainDb, q;
        double mix = 0.0, mixTarget = 0.0, mixStep = 0.0;
        int numSections = 0;
        bool moving = false;
        double appliedChange = 0.0;

        std::array<Coefficients, eq::maxSections> coefficients, increments;
        std::array<std::array<std::array<double, 2>, eq::maxSections>, maxChannels> state {};
    };

    // One per band: what the band hears, its level, and the gain change it asks for.
    struct Detector
    {
        eq::BandSpec shape;                 // the filter's own spec, to know when to redesign
        eq::BandFilter filter;
        std::array<std::array<std::array<double, 2>, eq::maxSections>, maxChannels> state {};
        std::array<double, maxChannels> meanSquare {};
        double change = 0.0;
        eq::Channel channel = eq::Channel::stereo;
        bool designed = false;
    };

    static eq::BandSpec effectiveSpec (const eq::BandSpec& spec, bool dynamic) noexcept;
    void runDetectors (const float* const* main, int numMain, const float* const* sidechain, int numSidechain, int numSamples) noexcept;

    Config configFor (const Targets& targets) const noexcept;
    int latencyOf (const Config& config) const noexcept;
    void applyConfig (const Config& config);
    void updateConfig (int numSamples) noexcept;

    void snapBand (Band& band, const eq::BandSpec& spec) noexcept;
    void setCoefficients (Band& band, const eq::BandSpec& spec, bool ramp) noexcept;
    void updateBands (int baseIndex) noexcept;
    void filterChunk (float* const* channels, int numChannels, int offset, int numSamples) noexcept;
    void filterBlock (float* const* channels, int numChannels, int numSamples) noexcept;
    void runBand (Band& band, int numChannels, int numSamples) noexcept;
    void processChunk (float* const* channels, int numChannels, int offset, int numSamples,
                       const float* const* sidechain, int numSidechain) noexcept;

    double sampleRate = 48000.0;
    double processRate = 48000.0;

    Targets current;
    Config activeConfig, wantedConfig;
    std::atomic<int> activeLatency { 0 };
    int wantedStableSamples = 0;
    int holdSamples = 0;

    std::array<Band, eq::maxBands> bands;
    int stepPosition = 0;
    int stepLength = stepSamples;

    std::array<Detector, eq::maxBands> detectors;
    std::array<std::array<double, 512>, eq::maxBands> chunkChange {};
    int chunkLength = 0;
    int detectorClock = 0;
    double levelCoefficient = 0.0;
    std::array<std::atomic<float>, eq::maxBands> liveChange {};
    double work[maxChannels][stepSamples] {};
    double unfiltered[maxChannels][stepSamples] {};

    std::array<std::unique_ptr<Oversampler>, numOversamplers> oversamplers;
    std::array<int, numOversamplers> oversamplerLatencies {};
    juce::AudioBuffer<float> oversamplingBuffer;

    eq::LinearPhase linear;
    std::atomic<bool> synchronousDesign { false };
public:
    // Whether the linear-phase design is behind these settings.
    bool isDesignPending (const Targets& targets) noexcept;

private:
    std::atomic<bool> designRequested { false };
    std::atomic<int> busChannels { maxChannels };
    juce::SpinLock designLock;
    std::array<eq::BandSpec, eq::maxBands> designedBands;
    int designedLength = -1;

    fancy::dsp::Smoother gain, output, bypass, switchGain;

    std::array<std::vector<float>, maxChannels> dryDelay;
    int dryLength = 1;
    int dryPosition = 0;
    juce::AudioBuffer<float> dryBuffer;
};
