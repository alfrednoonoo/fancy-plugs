#pragma once

#include "Smoother.h"
#include "Svf.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace fancy::dsp
{

// Sorts out whatever automation asked for: in order, in range and at least minRatio apart
// (Fancy MultiComp's 1/3 octave by default).
inline void sanitiseCrossovers (double* hz, int count, double lowestHz = 20.0, double highestHz = 20000.0,
                                double minRatio = 1.2599210498948732) noexcept
{
    if (count <= 0)
        return;

    std::sort (hz, hz + count);

    hz[0] = std::max (hz[0], lowestHz);

    for (int i = 1; i < count; ++i)
        hz[i] = std::max (hz[i], hz[i - 1] * minRatio);

    hz[count - 1] = std::min (hz[count - 1], highestHz);

    for (int i = count - 2; i >= 0; --i)
        hz[i] = std::min (hz[i], hz[i + 1] / minRatio);
}

// Linkwitz-Riley bands from a cascade of splits. With compensation on, each lower band goes
// through the all-pass of every crossover above it, so all the bands share one phase and sum
// flat. See docs/multicomp.md. Moved here from Fancy MultiComp as it was, for Fancy Limit's bands
// (docs/limit.md, Phase 4 Wave 3), so MultiComp renders as before to the bit.
class MinPhaseCrossover
{
public:
    static constexpr int maxBands = 8;
    static constexpr int maxCrossovers = maxBands - 1;

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

    // Every crossover LR 48 (true) or LR 24 (false).
    void setSteep (bool lr48) noexcept
    {
        steep.fill (lr48);
    }

    // Phase 4 Wave 2 (item 25): each crossover's own slope, bit k for crossover k (set: LR 48).
    // Each lower band goes through the all-pass of each crossover above it at that crossover's
    // own order, so mixed slopes still sum flat.
    void setSlopes (unsigned steepMask) noexcept
    {
        for (int i = 0; i < maxCrossovers; ++i)
            steep[(size_t) i] = ((steepMask >> i) & 1u) != 0;
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
                    const auto numSections = steep[(size_t) j] ? 2 : 1;

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
                            const auto upperSections = steep[(size_t) above] ? 2 : 1;

                            for (int section = 0; section < upperSections; ++section)
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

        if (steep[(size_t) crossover])
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
    std::array<bool, maxCrossovers> steep {};
    bool compensate = true;

    std::array<fancy::dsp::Smoother, maxCrossovers> logHz;
    std::array<std::array<Coefficients, 2>, maxCrossovers> coefficients;
    std::array<Channel, 2> channels;
};

} // namespace fancy::dsp
