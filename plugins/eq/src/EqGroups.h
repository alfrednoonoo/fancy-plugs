#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

// Phase 4 Wave 2 item 46 (docs/eq.md, "Phase 4 Wave 2: band list and groups", section 2): how a
// group of bands follows one of them. Contains no JUCE types, so tests can drive it directly.
//
// A move is worked out from the values every member had when the edit began, not step by step,
// so a long drag doesn't drift. Freq and Q move by a ratio, Gain by an offset in dB; the move
// stops where the first member would leave its range, so the group keeps its shape.
namespace eq::groups
{

enum class Kind { ratio, offset };

// The largest part of `move` (a ratio, or an offset) that keeps every value within [low, high].
// A ratio above 1 is limited by the member nearest `high`, below 1 by the one nearest `low`.
inline double limitMove (Kind kind, double move, const std::vector<double>& starts, double low, double high) noexcept
{
    if (kind == Kind::ratio)
    {
        if (move <= 0.0 || ! std::isfinite (move))
            return 1.0;

        auto limited = move;

        for (const auto start : starts)
        {
            if (start <= 0.0)
                continue;

            if (start * limited > high)
                limited = high / start;

            if (start * limited < low)
                limited = low / start;
        }

        return limited;
    }

    auto limited = move;

    for (const auto start : starts)
    {
        if (start + limited > high)
            limited = high - start;

        if (start + limited < low)
            limited = low - start;
    }

    return limited;
}

// What one start value becomes under a move.
inline double apply (Kind kind, double start, double move) noexcept
{
    return kind == Kind::ratio ? start * move : start + move;
}

// The move from the edited band's start value to the value it was set to.
inline double moveFor (Kind kind, double start, double now) noexcept
{
    if (kind == Kind::ratio)
        return start > 0.0 ? now / start : 1.0;

    return now - start;
}

// A group's letter for a Group choice index: 0 is none.
inline char letter (int group) noexcept
{
    return group > 0 && group <= 4 ? (char) ('A' + group - 1) : 0;
}

} // namespace eq::groups
