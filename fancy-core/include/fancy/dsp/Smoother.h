#pragma once

#include <cmath>

namespace fancy::dsp
{

// One-pole smoother. It lands exactly on the target once close, so a settled
// parameter produces bit-identical output and lets callers skip recalculation.
class Smoother
{
public:
    void setTime (double milliseconds, double sampleRate) noexcept
    {
        const auto samples = milliseconds * 0.001 * sampleRate;
        coefficient = samples > 0.0 ? 1.0 - std::exp (-1.0 / samples) : 1.0;
    }

    void snapTo (double value) noexcept
    {
        current = target = value;
    }

    void setTarget (double value) noexcept
    {
        target = value;
    }

    double next() noexcept
    {
        if (current != target)
        {
            current += (target - current) * coefficient;

            if (std::abs (target - current) < 1.0e-9)
                current = target;
        }

        return current;
    }

    bool isSettled() const noexcept { return current == target; }
    double getCurrent() const noexcept { return current; }

private:
    double coefficient = 1.0;
    double current = 0.0;
    double target = 0.0;
};

} // namespace fancy::dsp
