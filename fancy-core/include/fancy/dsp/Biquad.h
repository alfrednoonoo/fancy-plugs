#pragma once

#include <cmath>
#include <numbers>

namespace fancy::dsp
{

// Transposed direct form II, double precision.
class Biquad
{
public:
    void setButterworthHighPass (double frequency, double sampleRate) noexcept
    {
        const auto w = 2.0 * std::numbers::pi * frequency / sampleRate;
        const auto cosW = std::cos (w);
        const auto alpha = std::sin (w) * std::numbers::sqrt2 * 0.5; // sin(w) / (2Q), Q = 1/sqrt(2)
        const auto a0 = 1.0 + alpha;

        b0 = (1.0 + cosW) * 0.5 / a0;
        b1 = -(1.0 + cosW) / a0;
        b2 = b0;
        a1 = -2.0 * cosW / a0;
        a2 = (1.0 - alpha) / a0;
    }

    void reset() noexcept
    {
        z1 = z2 = 0.0;
    }

    double process (double in) noexcept
    {
        const auto out = b0 * in + z1;
        z1 = b1 * in - a1 * out + z2;
        z2 = b2 * in - a2 * out;
        return out;
    }

    bool isFinite() const noexcept
    {
        return std::isfinite (z1) && std::isfinite (z2);
    }

private:
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;
};

} // namespace fancy::dsp
