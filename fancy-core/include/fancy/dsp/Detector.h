#pragma once

#include "Svf.h"

#include <algorithm>
#include <cmath>

namespace fancy::dsp
{

// One channel of a compressor's or gate's detector: a high-pass and a low-pass that can each
// be blended out, then a one-pole mean square. See docs/comp.md.
struct DetectorFilter
{
    void reset() noexcept
    {
        highPass.reset();
        lowPass.reset();
        meanSquare = 0.0;
    }

    // The filtered detector sample, also what Preview filter plays.
    double filter (double in, const SvfCoefficients& highPassCoefficients, const SvfCoefficients& lowPassCoefficients,
                   double highPassBlend, double lowPassBlend) noexcept
    {
        const auto highPassed = highPass.tick (highPassCoefficients, in).high;
        const auto afterHighPass = in + (highPassed - in) * highPassBlend;
        const auto lowPassed = lowPass.tick (lowPassCoefficients, afterHighPass).low;

        return afterHighPass + (lowPassed - afterHighPass) * lowPassBlend;
    }

    // Level in dB. A coefficient of 1 is peak detection.
    double levelDb (double filtered, double coefficient) noexcept
    {
        constexpr double silenceFloor = 1.0e-30;

        meanSquare += coefficient * (filtered * filtered - meanSquare);
        return 10.0 * std::log10 (std::max (meanSquare, silenceFloor));
    }

    Svf highPass, lowPass;
    double meanSquare = 0.0;
};

} // namespace fancy::dsp
