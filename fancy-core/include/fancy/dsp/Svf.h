#pragma once

#include <cmath>
#include <numbers>

namespace fancy::dsp
{

// Topology-preserving state-variable filter. It stays stable under fast modulation.
struct SvfCoefficients
{
    void set (double frequency, double q, double sampleRate) noexcept
    {
        const auto g = std::tan (std::numbers::pi * frequency / sampleRate);

        k = 1.0 / q;
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    double k = 2.0, a1 = 1.0, a2 = 0.0, a3 = 0.0;
};

struct Svf
{
    struct Outputs
    {
        double low, band, high;
    };

    void reset() noexcept
    {
        ic1 = ic2 = 0.0;
    }

    Outputs tick (const SvfCoefficients& c, double in) noexcept
    {
        const auto v3 = in - ic2;
        const auto v1 = c.a1 * ic1 + c.a2 * v3;
        const auto v2 = ic2 + c.a2 * ic1 + c.a3 * v3;

        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;

        return { v2, v1, in - c.k * v1 - v2 };
    }

    bool isFinite() const noexcept
    {
        return std::isfinite (ic1) && std::isfinite (ic2);
    }

    double ic1 = 0.0, ic2 = 0.0;
};

} // namespace fancy::dsp
