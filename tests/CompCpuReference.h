#pragma once

// Fancy Comp at its defaults, the reference §0.4's CPU ratios divide by, timed inside the test
// that uses it: the runners' speed varies up to 1.7 times between runs, and Comp's own figure
// moves within one job too, so a ratio is only as good as the reference measured beside it.

#if FANCY_HAS_COMP
 #include "CompDsp.h"
#endif

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace fancytest
{
// Median of five, 48 kHz, 128-sample stereo blocks of noise, as eq q24's reference measures it.
// 0 when Comp isn't in the build.
inline double compDefaultCpu()
{
   #if FANCY_HAS_COMP
    constexpr double rate = 48000.0;
    constexpr int blockSize = 128, blocks = 6000, runs = 5;
    std::vector<double> values;

    for (int run = 0; run < runs; ++run)
    {
        CompDsp comp;
        comp.prepare (rate);
        comp.snapToTargets (CompDsp::Targets {});

        float left[blockSize], right[blockSize];
        float* channels[] { left, right };
        std::mt19937 random (8);
        std::uniform_real_distribution<float> noise (-0.25f, 0.25f);
        std::chrono::duration<double> spent {};

        for (int block = 0; block < blocks; ++block)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                left[i] = noise (random);
                right[i] = noise (random);
            }

            const auto start = std::chrono::steady_clock::now();
            comp.process (channels, 2, nullptr, 0, blockSize);
            spent += std::chrono::steady_clock::now() - start;
        }

        values.push_back (100.0 * spent.count() / ((double) (blocks * blockSize) / rate));
    }

    std::sort (values.begin(), values.end());
    return values[runs / 2];
   #else
    return 0.0;
   #endif
}

// " = 1.23 x Comp", or nothing when Comp isn't in the build.
inline std::string timesComp (double percent, double comp)
{
    if (comp <= 0.0)
        return {};

    char text[32];
    std::snprintf (text, sizeof (text), " = %.2f x Comp", percent / comp);
    return text;
}
} // namespace fancytest
