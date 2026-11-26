#pragma once

#include <fancy/dsp/MinPhaseCrossover.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace multicomp
{

constexpr int maxBands = 8;
constexpr int maxCrossovers = maxBands - 1;

constexpr double lowestHz = 20.0, highestHz = 20000.0;

// 2^(1/3), the closest two crossovers may be.
constexpr double minCrossoverRatio = 1.2599210498948732;

// Dynamic phase (Phase 4 Wave 2) is appended, so stored indices keep their meaning. Its detectors
// use the LR 24 split; its audio isn't split (fancy::dsp::ShelfCascade).
enum class CrossoverType { lr24, lr48, linear, dynamic };

// Sorts out whatever automation asked for: in order, in range and at least 1/3 octave apart.
inline void sanitiseCrossovers (double* hz, int count) noexcept
{
    fancy::dsp::sanitiseCrossovers (hz, count, lowestHz, highestHz, minCrossoverRatio);
}

// fancy::dsp::MinPhaseCrossover (moved to fancy-core in v1.7.2 for Fancy Limit's bands), with
// MultiComp's crossover type.
class MinPhaseCrossover : public fancy::dsp::MinPhaseCrossover
{
public:
    void setType (CrossoverType type) noexcept
    {
        setSteep (type == CrossoverType::lr48);
    }
};

static_assert (fancy::dsp::MinPhaseCrossover::maxBands == maxBands);

} // namespace multicomp
