#pragma once

#include <fancy/dsp/Biquad.h>
#include <fancy/dsp/Smoother.h>
#include <fancy/dsp/Svf.h>
#include <fancy/dsp/SwitchedOversampler.h>

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <memory>
#include <vector>

// See docs/exciter.md.
class ExciterDsp
{
public:
    enum Oversampling { off, x2, x4, x8 };

    struct Targets
    {
        double frequencyHz = 2000.0;
        double boostGain = 1.0;
        double harmonics = 0.0;
        double mixBackGain = 0.5;
        double character = 0.5;
        double outputGain = 1.0;
        int oversampling = x4;
        bool bypass = false;
    };

    ExciterDsp();
    ~ExciterDsp();

    void prepare (double sampleRate);
    void reset();

    void setTargets (const Targets& targets);

    // Jumps straight to the targets, for use before processing starts.
    void snapToTargets (const Targets& targets);

    void process (float* const* channels, int numChannels, int numSamples);

    // Latency of the setting currently in use. It changes during process() when the
    // oversampling setting does.
    int getLatencySamples() const noexcept;

    // Exposed for tests.
    static double shape (double x) noexcept;
    static double shapeAntiderivative (double x) noexcept;

private:
    static constexpr int maxChunk = 64;

    struct ChannelState
    {
        fancy::dsp::Svf preFilter, postFilter;
        fancy::dsp::Biquad lowPass;
        double previousInput = 0.0;
        double previousAntiderivative = 0.0;
        std::vector<float> dryDelay;
    };

    void processChunk (float* const* channels, int numChannels, int offset, int numSamples);
    void shapeBlock (juce::dsp::AudioBlock<float> block, int baseSamples);
    void resetCore();
    void updateFilter (double logHz) noexcept;

    double sampleRate = 48000.0;

    // Moved to fancy-core for Fancy Sature, unchanged (docs/sature.md, Decision 3).
    fancy::dsp::SwitchedOversampler oversampling;
    juce::AudioBuffer<float> wetBuffer;

    std::array<ChannelState, 2> channelStates;
    fancy::dsp::SvfCoefficients filter;

    // Each sample's filter and bias in the chunk being processed, so a gliding Freq or
    // Character gives the same output whatever the block sizes (Phase 2.1 H.1).
    std::array<fancy::dsp::SvfCoefficients, maxChunk> chunkFilters;
    std::array<double, maxChunk> chunkBias {};

    int requestedOversampling = x4;
    int delayWritePosition = 0;
    int delayLength = 1;

    fancy::dsp::Smoother logFrequency, boost, harmonics, mixBack, character, output, bypass;
};
