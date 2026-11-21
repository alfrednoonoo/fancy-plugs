#pragma once

#include <fancy/dsp/TruePeakLimiter.h>

#include <algorithm>
#include <cmath>

namespace fancy::dsp
{

// The output ceiling Phase 4 Wave 1 adds to Fancy DS, Delay, Exciter and Kicker: the shared
// true-peak limiter after everything else, holding the output's true peak under the ceiling.
// While it's off it's out of the path entirely, so the plugin is as it was, latency included.
// Switching it changes the latency, so it's a setting, not something to automate.
class OutputCeiling
{
public:
    // A ceiling at or below this is Off: the parameters show the bottom of their travel as "Off".
    static constexpr double offDb = -12.5;

    // Allocates.
    void prepare (double sampleRate)
    {
        limiter.prepare (sampleRate);
        wasOn = false;
    }

    static bool isOn (double ceilingDb) noexcept { return ceilingDb > offDb + 0.01; }

    int getLatencySamples (double ceilingDb) const noexcept { return isOn (ceilingDb) ? limiter.getLatencySamples() : 0; }

    // Bypassed, the signal goes through untouched but still delayed, so the latency holds.
    void process (float* const* channels, int numChannels, int numSamples, double ceilingDb, bool bypassed) noexcept
    {
        const auto on = isOn (ceilingDb);

        if (on != wasOn)
        {
            limiter.reset();
            wasOn = on;
        }

        if (! on || numChannels <= 0)
            return;

        const auto ceiling = std::pow (10.0, ceilingDb / 20.0);
        const auto amount = bypassed ? 0.0 : 1.0;

        for (int i = 0; i < numSamples; ++i)
        {
            auto l = (double) channels[0][i] / ceiling;
            auto r = numChannels > 1 ? (double) channels[1][i] / ceiling : l;
            limiter.process (l, r, amount);
            channels[0][i] = (float) (l * ceiling);

            if (numChannels > 1)
                channels[1][i] = (float) (r * ceiling);
        }
    }

private:
    TruePeakLimiter limiter;
    bool wasOn = false;
};

} // namespace fancy::dsp
