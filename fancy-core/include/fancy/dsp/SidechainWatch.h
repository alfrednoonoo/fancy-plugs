#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>

namespace fancy::dsp
{

// Notices a sidechain source that has nothing on it: the sidechain is chosen, it stays below
// -90 dBFS for 2 s, and the main input plays above -60 dBFS. The same rule as Fancy Gate's.
// An inactive sidechain bus counts as silent. Audio thread: update(); any thread: isMissing().
class SidechainWatch
{
public:
    static constexpr double silentDb = -90.0, mainActiveDb = -60.0, warningSeconds = 2.0;

    void update (const float* const* main, int numMain, const float* const* sidechain, int numSidechain,
                 int numSamples, double sampleRate, bool sidechainChosen) noexcept
    {
        if (numSamples <= 0 || sampleRate <= 0.0)
            return;

        const auto sidechainDb = peakDb (sidechain, numSidechain, numSamples);

        if (sidechainChosen && sidechainDb < silentDb && peakDb (main, numMain, numSamples) > mainActiveDb)
            silentSeconds += (double) numSamples / sampleRate;
        else if (! sidechainChosen || sidechainDb >= silentDb)
            silentSeconds = 0.0;

        missing.store (silentSeconds >= warningSeconds, std::memory_order_relaxed);
    }

    bool isMissing() const noexcept { return missing.load (std::memory_order_relaxed); }

    void reset() noexcept
    {
        silentSeconds = 0.0;
        missing.store (false, std::memory_order_relaxed);
    }

private:
    static double peakDb (const float* const* channels, int numChannels, int numSamples) noexcept
    {
        auto peak = 0.0f;

        for (int channel = 0; channel < numChannels; ++channel)
            if (channels != nullptr && channels[channel] != nullptr)
                for (int i = 0; i < numSamples; ++i)
                    peak = std::max (peak, std::abs (channels[channel][i]));

        return peak > 0.0f ? 20.0 * std::log10 ((double) peak) : -400.0;
    }

    double silentSeconds = 0.0;
    std::atomic<bool> missing { false };
};

} // namespace fancy::dsp
