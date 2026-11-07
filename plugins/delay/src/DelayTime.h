#pragma once

#include <algorithm>
#include <cmath>

namespace delay
{

constexpr int maxTaps = 16;
constexpr double maxDelayMs = 10000.0;

constexpr int noteCount = 7;   // 1/1 to 1/64
constexpr int feelCount = 3;
constexpr int maxMultiplier = 16;

enum Feel { straight, dotted, triplet };

struct NoteLength
{
    int note = 3, feel = dotted, multiplier = 1;
};

constexpr double feelFactor (int feel) noexcept
{
    return feel == dotted ? 1.5 : (feel == triplet ? 2.0 / 3.0 : 1.0);
}

inline double beatMs (double bpm) noexcept
{
    return 60000.0 / bpm;
}

// note 0 is a whole note, and each step halves it. Limited to the longest delay.
inline double noteMs (const NoteLength& length, double bpm) noexcept
{
    const auto beats = 4.0 / (double) (1 << length.note);

    return std::min (maxDelayMs, beatMs (bpm) * beats * feelFactor (length.feel) * (double) length.multiplier);
}

// The simplest note length close to a time, for dragging a tap in Note mode. Closest wins
// first, then anything within 2 % of that with a smaller multiplier or a straight feel.
inline NoteLength nearestNote (double ms, double bpm) noexcept
{
    auto closest = std::abs (noteMs ({}, bpm) - ms);

    const auto forEachLength = [] (auto&& action)
    {
        for (int multiplier = 1; multiplier <= maxMultiplier; ++multiplier)
            for (int feel = 0; feel < feelCount; ++feel)
                for (int note = 0; note < noteCount; ++note)
                    action (NoteLength { note, feel, multiplier });
    };

    forEachLength ([&] (const NoteLength& length) { closest = std::min (closest, std::abs (noteMs (length, bpm) - ms)); });

    const auto tolerance = closest + 0.02 * ms;

    NoteLength best;
    auto found = false;

    forEachLength ([&] (const NoteLength& length)
    {
        if (! found && std::abs (noteMs (length, bpm) - ms) <= tolerance)
        {
            best = length;
            found = true;
        }
    });

    return best;
}

} // namespace delay
