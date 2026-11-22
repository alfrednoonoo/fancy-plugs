// Phase 4 Wave 2 for Fancy Comp: Optical mode (docs/comp.md, "Phase 4 Wave 2: Optical mode",
// section 4). Test 1 (Optical off renders as v1.6.0) is CompW1Tests' legacy test, which runs
// the current CompDsp with optical at its default.

#include "CompDsp.h"

#include <fancy/dsp/Compression.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <vector>

namespace
{
constexpr double rate = 48000.0;

fancy::dsp::OpticalFollower follower()
{
    fancy::dsp::OpticalFollower f;
    f.set (10.0, 500.0, false, 0.0, rate);
    return f;
}

double attackMs (double overDb)
{
    auto f = follower();

    for (int n = 0; n < (int) rate; ++n)
        if (f.follow (overDb) >= 0.632 * overDb)
            return n * 1000.0 / rate;

    return 1.0e9;
}

double recoveryMs (double burstSeconds)
{
    auto f = follower();

    for (int n = 0; n < (int) (burstSeconds * rate); ++n)
        f.follow (12.0);

    for (int n = 0; n < (int) (30.0 * rate); ++n)
        if (f.follow (0.0) <= 1.0)
            return n * 1000.0 / rate;

    return 1.0e9;
}

struct Stereo
{
    std::vector<float> left, right;
    explicit Stereo (int n) : left ((size_t) n), right ((size_t) n) {}
};

// A 1 kHz tone that steps between -30 and -6 dBFS every 0.4 s.
Stereo bursts (int numSamples)
{
    Stereo s (numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto loud = (i / (int) (0.4 * rate)) % 2 == 1;
        const auto amplitude = std::pow (10.0, (loud ? -6.0 : -30.0) / 20.0);
        s.left[(size_t) i] = s.right[(size_t) i] = (float) (amplitude * std::sin (2.0 * std::numbers::pi * 1000.0 * i / rate));
    }

    return s;
}

Stereo render (const CompDsp::Targets& targets, Stereo signal, int blockSize)
{
    CompDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    const auto total = (int) signal.left.size();

    for (int start = 0; start < total; start += blockSize)
    {
        float* main[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (main, 2, nullptr, 0, std::min (blockSize, total - start));
    }

    return signal;
}

CompDsp::Targets optical()
{
    CompDsp::Targets targets;
    targets.thresholdDb = -30.0;
    targets.ratio = 8.0;
    targets.attackMs = 10.0;
    targets.releaseMs = 300.0;
    targets.optical = true;
    return targets;
}
} // namespace

TEST_CASE ("Comp Optical: attack speeds up with the overshoot", "[comp][w2][optical]")
{
    CHECK (attackMs (12.0) == Catch::Approx (8.6).margin (1.0));
    CHECK (attackMs (3.0) == Catch::Approx (20.0).margin (2.0));
}

TEST_CASE ("Comp Optical: a long squash recovers more slowly", "[comp][w2][optical]")
{
    CHECK (recoveryMs (0.3) == Catch::Approx (1700.0).margin (200.0));
    CHECK (recoveryMs (6.0) == Catch::Approx (3300.0).margin (300.0));
}

TEST_CASE ("Comp Optical: Classic and Auto release don't apply; Range does", "[comp][w2][optical]")
{
    const auto input = bursts ((int) (2.0 * rate));
    const auto plain = render (optical(), input, 512);

    auto withClassic = optical();
    withClassic.classic = true;
    withClassic.autoRelease = true;
    const auto classic = render (withClassic, input, 512);

    REQUIRE (plain.left == classic.left);

    auto withRange = optical();
    withRange.rangeDb = 3.0;
    const auto ranged = render (withRange, input, 512);

    REQUIRE (plain.left != ranged.left);
}

TEST_CASE ("Comp Optical renders the same at every block size", "[comp][w2][optical]")
{
    const auto input = bursts ((int) (2.0 * rate));
    const auto a = render (optical(), input, 1);
    const auto b = render (optical(), input, 997);

    REQUIRE (a.left == b.left);
    REQUIRE (a.right == b.right);
}
