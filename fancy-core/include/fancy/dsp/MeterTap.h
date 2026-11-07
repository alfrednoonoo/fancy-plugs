#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>

namespace fancy::dsp
{

// Peak levels passed from the audio thread to the UI. Lock-free on both sides.
class MeterTap
{
public:
    // A mono signal is reported on both channels.
    void push (const float* const* channels, int numChannels, int numSamples) noexcept
    {
        for (int ch = 0; ch < std::min (numChannels, 2); ++ch)
        {
            float peak = 0.0f;

            for (int i = 0; i < numSamples; ++i)
                peak = std::max (peak, std::abs (channels[ch][i]));

            raise (levels[ch], peak);

            if (numChannels == 1)
                raise (levels[1], peak);
        }
    }

    // For a signal that isn't in float buffers.
    void report (int channel, float peak) noexcept
    {
        raise (levels[channel], peak);
    }

    // Returns the peak since the last call and clears it.
    float take (int channel) noexcept
    {
        return levels[channel].exchange (0.0f, std::memory_order_relaxed);
    }

private:
    static void raise (std::atomic<float>& level, float peak) noexcept
    {
        auto current = level.load (std::memory_order_relaxed);

        while (peak > current
               && ! level.compare_exchange_weak (current, peak, std::memory_order_relaxed))
        {
        }
    }

    std::atomic<float> levels[2] {};
};

} // namespace fancy::dsp
