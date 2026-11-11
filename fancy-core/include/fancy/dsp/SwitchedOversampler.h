#pragma once

#include <fancy/dsp/Smoother.h>

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <memory>

namespace fancy::dsp
{

// Fancy Exciter's oversampling, moved here for Fancy Sature (docs/sature.md, Decision 3): JUCE
// half-band stages with Exciter's settings, linear phase (FIR equiripple) or, optionally,
// minimum phase (polyphase IIR), and Exciter's click-free switch: fade out over 2 ms, switch,
// stay silent while the caller's delays refill, fade back in.
//
// Each setting has `copies` independent stereo oversamplers, for plugins that run more than
// one signal through matched filters.
class SwitchedOversampler
{
public:
    enum Factor { off, x2, x4, x8 };
    static constexpr int numFactors = 3;
    static constexpr int maxCopies = 2;

    using Oversampler = juce::dsp::Oversampling<float>;

    // Allocates. Builds the linear-phase oversamplers, and the minimum-phase ones too if asked.
    void prepare (double sampleRate, int maxChunk, int copies, bool withMinimumPhase);

    // Clears the active setting's oversamplers.
    void reset();

    // Jumps to a setting with the fade fully up, for use before processing starts.
    void select (int factor, bool minimumPhase);

    // Call at the start of each chunk with the setting wanted. Returns true when it has just
    // switched, so the caller clears its own state (delay lines, filters) and refills.
    bool beginChunk (int requestedFactor, bool requestedMinimumPhase, int numSamples);

    // The switch's fade, per sample.
    double nextFade() noexcept { return fade.next(); }

    int getFactor() const noexcept { return activeFactor; }
    int getOversamplingRatio() const noexcept { return 1 << activeFactor; }
    bool isMinimumPhase() const noexcept { return activeMinimumPhase; }

    // Whole samples at the base rate, for the active setting.
    int getLatency() const noexcept { return latencyFor (activeFactor, activeMinimumPhase); }
    int latencyFor (int factor, bool minimumPhase) const noexcept;
    int getMaxLatency() const noexcept { return maxLatency; }

    // The active setting's oversampler. Not for Off.
    Oversampler& get (int copy) noexcept;

private:
    static int indexFor (int factor, bool minimumPhase) noexcept { return (minimumPhase ? numFactors : 0) + factor - 1; }

    std::array<std::array<std::unique_ptr<Oversampler>, 2 * numFactors>, maxCopies> oversamplers;
    std::array<int, 2 * numFactors> latencies {};

    double sampleRate = 48000.0;
    int numCopies = 1;
    int maxLatency = 0;
    int activeFactor = x4;
    bool activeMinimumPhase = false;
    int holdSamples = 0;

    Smoother fade;
};

} // namespace fancy::dsp
