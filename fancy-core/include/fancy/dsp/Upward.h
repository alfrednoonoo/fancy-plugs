#pragma once

#include <algorithm>

// Upward compression's static curve (Phase 2.1 U), shared by Fancy MultiComp and Fancy Comp
// (Phase 3 Q2.5). Plain C++.
namespace fancy::dsp::upward
{

constexpr double floorFadeDb = 8.0;

struct Settings
{
    double upThresholdDb = -40.0;
    double upRatio = 2.0;
    double maxLiftDb = 12.0;
    double floorDb = -70.0;
};

// The lift in dB for a detector level: zero at and above the Up threshold, which is clamped to
// at most the downward Threshold; limited to Max lift; faded out over 8 dB above the Floor, and
// zero below it.
inline double liftFor (double levelDb, double thresholdDb, const Settings& s) noexcept
{
    // An Up threshold above the Threshold acts as the Threshold.
    const auto upThreshold = std::min (s.upThresholdDb, thresholdDb);

    if (levelDb >= upThreshold || levelDb < s.floorDb)
        return 0.0;

    const auto lift = std::min (s.maxLiftDb, (upThreshold - levelDb) * (1.0 - 1.0 / std::max (1.0, s.upRatio)));
    const auto fade = std::clamp ((levelDb - s.floorDb) / floorFadeDb, 0.0, 1.0);
    return lift * fade;
}

// The ratio from the smoothed slope 1 - 1/ratio, as MultiComp smooths it.
inline double ratioFromSlope (double slope) noexcept
{
    return slope < 1.0 ? 1.0 / (1.0 - slope) : 1.0e9;
}

} // namespace fancy::dsp::upward
