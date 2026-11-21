#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

// Phase 4 Wave 2 (docs/multicomp.md, "dynamic-phase crossover"): band gains applied without
// splitting the audio. The lowest band's gain, then at each crossover a second-order high shelf
// (Q 0.707) from the band below's gain to the band above's. Equal neighbours make a shelf exactly
// 1, so it adds no phase; phase appears only where neighbouring gains differ. Modelled in
// docs/research/multicomp-dynphase-model.py (S6, order 2).
//
// Each shelf is a TPT state-variable filter in Andrew Simper's (Cytomic) form, which stays clean
// while its gain moves every sample. At a gain step of exactly 1 its output is its input, to the
// bit, and its state keeps running, so it can start shaping at any moment without a click.
namespace fancy::dsp
{

template <int maxBands>
class ShelfCascade
{
public:
    static constexpr int maxShelves = maxBands - 1;

    void reset() noexcept
    {
        for (auto& shelf : shelves)
            shelf.ic1 = shelf.ic2 = 0.0;
    }

    // tan (pi f / fs) for each crossover. Call when the crossovers move.
    void setCrossovers (const double* hz, int count, double sampleRate) noexcept
    {
        numShelves = std::clamp (count, 0, maxShelves);

        for (int i = 0; i < numShelves; ++i)
            shelves[(size_t) i].warp = std::tan (std::numbers::pi * std::min (hz[i], 0.49 * sampleRate) / sampleRate);
    }

    // One sample through the cascade, with each band's linear gain (gains[0 .. numShelves]).
    double process (double x, const double* gains) noexcept
    {
        constexpr double k = 1.4142135623730951; // 1 / Q, Q = 0.707

        auto y = gains[0] == 1.0 ? x : x * gains[0];

        for (int i = 0; i < numShelves; ++i)
        {
            auto& shelf = shelves[(size_t) i];
            const auto below = gains[i], above = gains[i + 1];
            const auto ratio = below == above ? 1.0 : (below > 0.0 ? above / below : 1.0);

            // A = sqrt (linear step), as RBJ's and Simper's shelves define it.
            const auto a = ratio == 1.0 ? 1.0 : std::sqrt (std::max (ratio, 1.0e-6));
            const auto g = ratio == 1.0 ? shelf.warp : shelf.warp * std::sqrt (a);
            const auto a1 = 1.0 / (1.0 + g * (g + k));
            const auto a2 = g * a1;
            const auto a3 = g * a2;

            const auto v3 = y - shelf.ic2;
            const auto v1 = a1 * shelf.ic1 + a2 * v3;
            const auto v2 = shelf.ic2 + a2 * shelf.ic1 + a3 * v3;
            shelf.ic1 = 2.0 * v1 - shelf.ic1;
            shelf.ic2 = 2.0 * v2 - shelf.ic2;

            if (ratio != 1.0)
                y = a * a * y + k * (1.0 - a) * a * v1 + (1.0 - a * a) * v2;
        }

        return y;
    }

private:
    struct Shelf
    {
        double warp = 0.1, ic1 = 0.0, ic2 = 0.0;
    };

    std::array<Shelf, (size_t) std::max (1, maxShelves)> shelves {};
    int numShelves = 0;
};

} // namespace fancy::dsp
