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

        // Phase 4 Wave 1 (docs/exciter.md): plays only what Exciter adds. Temporary, never saved.
        bool listen = false;

        // Phase 4 Wave 2 (docs/exciter.md, items 40 to 43). The defaults are v1.7.0's sound, bit
        // for bit: Band 2 at Harmonics 2 0 isn't run, nor Punch at 0, the High cut when Off, or
        // Width at 100 %.
        double frequency2Hz = 400.0; // Band 2 runs from here up to Freq
        double harmonics2 = 0.0;
        double punch = 0.0;          // 0 to 1
        bool highCutOn = false;
        double highCutHz = 20000.0;
        double width = 1.0;          // 0 to 1.5, the added part's side
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

    // For tests: while set, process() writes the top band's own added part (left channel, before
    // Band 2, the High cut and Width are added) here, at the same index as the samples it gets.
    void setTopBandTap (float* destination) noexcept { topBandTap = destination; }

private:
    static constexpr int maxChunk = 64;

    struct Adaa
    {
        double previousInput = 0.0;
        double previousAntiderivative = 0.0;
    };

    struct ChannelState
    {
        fancy::dsp::Svf preFilter, postFilter;
        fancy::dsp::Biquad lowPass;
        Adaa shaper;
        std::vector<float> dryDelay;

        // Phase 4 Wave 2: Band 2's own filters and shaper state, and the High cut.
        fancy::dsp::Svf preFilter2, postFilter2;
        fancy::dsp::Biquad lowPass2;
        Adaa shaper2;
        fancy::dsp::Svf highCut;
    };

    void processChunk (float* const* channels, int numChannels, int offset, int numSamples);
    void fillBias (int baseSamples);
    void shapeBlock (juce::dsp::AudioBlock<float> block, int baseSamples, Adaa ChannelState::* shaper);
    void resetCore();
    void resetBand2();
    void resetPunch();
    void updateFilter (double logHz) noexcept;
    void updateFilter2() noexcept;
    double nextWeight (double level) noexcept;

    double sampleRate = 48000.0;

    // Moved to fancy-core for Fancy Sature, unchanged (docs/sature.md, Decision 3). Copy 0 is the
    // top band's, copy 1 Band 2's.
    fancy::dsp::SwitchedOversampler oversampling;
    juce::AudioBuffer<float> wetBuffer, wetBuffer2;

    std::array<ChannelState, 2> channelStates;
    fancy::dsp::SvfCoefficients filter, filter2, highCutFilter;

    // Each sample's filter and bias in the chunk being processed, so a gliding Freq or
    // Character gives the same output whatever the block sizes (Phase 2.1 H.1).
    std::array<fancy::dsp::SvfCoefficients, maxChunk> chunkFilters, chunkFilters2;
    std::array<double, maxChunk> chunkBias {};
    std::array<double, maxChunk> chunkWeight {};

    int requestedOversampling = x4;
    int delayWritePosition = 0;
    int delayLength = 1;

    fancy::dsp::Smoother logFrequency, boost, harmonics, mixBack, character, output, bypass;
    fancy::dsp::Smoother listenMix; // Phase 4 Wave 1

    // Phase 4 Wave 2.
    fancy::dsp::Smoother logFrequency2, harmonics2, punch, logHighCut, highCutMix, width;
    bool band2Running = false, punchRunning = false, highCutRunning = false, highCutWanted = false;
    double detectorLevel = 0.0, detectorFast = 0.0, detectorSlow = 0.0;
    double levelCoefficient = 1.0, fastAttack = 1.0, slowAttack = 1.0, detectorRelease = 1.0;

    float* topBandTap = nullptr;
};
