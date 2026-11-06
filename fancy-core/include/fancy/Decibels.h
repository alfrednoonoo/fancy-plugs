#pragma once

#include <algorithm>
#include <cmath>

namespace fancy
{

inline double dbToGain (double db) noexcept
{
    return std::pow (10.0, db / 20.0);
}

// Gains at or below the floor report the floor, which keeps silence finite.
inline double gainToDb (double gain, double floorDb = -150.0) noexcept
{
    return std::max (floorDb, 20.0 * std::log10 (std::max (gain, 1.0e-30)));
}

} // namespace fancy
