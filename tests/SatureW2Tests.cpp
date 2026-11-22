// Phase 4 Wave 2 for Fancy Sature: outer-band drive (docs/sature.md, "Phase 4 Wave 2", section 5).
// Test 6 (flat with every drive at 0) is SatureTests' flatness test, which still runs.

#include "LegacySatureDsp.h"
#include "SatureDsp.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>
#include <numbers>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = std::numbers::pi;

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples, 0.0f), right ((size_t) numSamples, 0.0f) {}
    int size() const { return (int) left.size(); }
};

Stereo sine (double hz, double amplitude, int numSamples)
{
    Stereo s (numSamples);

    for (int i = 0; i < numSamples; ++i)
        s.left[(size_t) i] = s.right[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * hz * i / rate));

    return s;
}

// Pink noise (Kellet's filter), scaled to -18 dBFS RMS.
Stereo pink (int numSamples, unsigned seed)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<double> uniform (-1.0, 1.0);
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    std::vector<double> values ((size_t) numSamples);
    auto sum = 0.0;

    for (int i = 0; i < numSamples + 24000; ++i)
    {
        const auto white = uniform (random);
        b0 = 0.99886 * b0 + white * 0.0555179;
        b1 = 0.99332 * b1 + white * 0.0750759;
        b2 = 0.96900 * b2 + white * 0.1538520;
        b3 = 0.86650 * b3 + white * 0.3104856;
        b4 = 0.55000 * b4 + white * 0.5329522;
        b5 = -0.7616 * b5 - white * 0.0168980;
        const auto value = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362;
        b6 = white * 0.115926;

        if (i >= 24000)
        {
            values[(size_t) (i - 24000)] = value;
            sum += value * value;
        }
    }

    const auto scale = std::pow (10.0, -18.0 / 20.0) / std::sqrt (sum / numSamples);
    Stereo s (numSamples);

    for (int i = 0; i < numSamples; ++i)
        s.left[(size_t) i] = s.right[(size_t) i] = (float) (values[(size_t) i] * scale);

    return s;
}

template <typename Dsp, typename Targets>
Stereo run (const Targets& targets, Stereo signal, int blockSize = 512)
{
    Dsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    for (int start = 0; start < signal.size(); start += blockSize)
    {
        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, std::min (blockSize, signal.size() - start));
    }

    return signal;
}

double rmsDb (const std::vector<float>& x, int from)
{
    auto sum = 0.0;

    for (int i = from; i < (int) x.size(); ++i)
        sum += (double) x[(size_t) i] * x[(size_t) i];

    return 10.0 * std::log10 (std::max (sum / (double) ((int) x.size() - from), 1.0e-30));
}

double differenceDb (const std::vector<float>& a, const std::vector<float>& b, int from)
{
    std::vector<float> d (a.size());

    for (size_t i = 0; i < a.size(); ++i)
        d[i] = a[i] - b[i];

    return rmsDb (d, from) - rmsDb (a, from);
}

SatureDsp::Targets bands (double lowHz, double highHz)
{
    SatureDsp::Targets t;
    t.lowOn = lowHz > 0.0;
    t.lowHz = lowHz > 0.0 ? lowHz : 20.0;
    t.highOn = highHz > 0.0;
    t.highHz = highHz > 0.0 ? highHz : 20000.0;
    return t;
}
} // namespace

TEST_CASE ("Sature with both outer drives at 0 renders exactly as v1.6.0", "[sature][w2]")
{
    const auto type = GENERATE (0, 1, 2, 3);
    const auto factor = GENERATE (0, 1, 2, 3);
    const auto edges = GENERATE (0, 1);

    LegacySatureDsp::Targets old;
    old.type = type;
    old.oversampling = factor;
    old.character = 0.4;
    old.driveDb = 14.0;

    if (edges == 1)
    {
        old.lowOn = true;
        old.lowHz = 200.0;
        old.highOn = true;
        old.highHz = 6000.0;
    }

    SatureDsp::Targets now;
    now.type = old.type;
    now.oversampling = old.oversampling;
    now.character = old.character;
    now.driveDb = old.driveDb;
    now.lowOn = old.lowOn;
    now.lowHz = old.lowHz;
    now.highOn = old.highOn;
    now.highHz = old.highHz;

    const auto input = pink (24000, 7);
    const auto a = run<LegacySatureDsp> (old, input);
    const auto b = run<SatureDsp> (now, input);

    INFO ("type " << type << ", oversampling " << factor << ", edges " << edges);
    REQUIRE (a.left == b.left);
    REQUIRE (a.right == b.right);
}

TEST_CASE ("Sature's low band drives only below Low", "[sature][w2]")
{
    auto clean = bands (200.0, 0.0);
    auto driven = clean;
    driven.lowDriveDb = 24.0;

    // A 60 Hz sine gains harmonics...
    {
        const auto input = sine (60.0, 0.25, 48000);
        const auto a = run<SatureDsp> (clean, input);
        const auto b = run<SatureDsp> (driven, input);
        CHECK (differenceDb (a.left, b.left, 12000) > -40.0);
    }

    // ...and a 1 kHz sine alone is left as it was.
    {
        const auto input = sine (1000.0, 0.25, 48000);
        const auto a = run<SatureDsp> (clean, input);
        const auto b = run<SatureDsp> (driven, input);
        CHECK (differenceDb (a.left, b.left, 12000) < -80.0);
    }
}

TEST_CASE ("Sature's outer Mix at 0 is the same as Drive 0", "[sature][w2]")
{
    auto off = bands (200.0, 6000.0);
    auto mixedOut = off;
    mixedOut.lowDriveDb = mixedOut.highDriveDb = 24.0;
    mixedOut.lowMix = mixedOut.highMix = 0.0;

    const auto input = pink (24000, 11);
    const auto a = run<SatureDsp> (off, input);
    const auto b = run<SatureDsp> (mixedOut, input);

    REQUIRE (a.left == b.left);
    REQUIRE (a.right == b.right);
}

TEST_CASE ("Sature's auto gain keeps a driven low band at its level", "[sature][w2]")
{
    auto clean = bands (200.0, 0.0);
    auto driven = clean;
    driven.lowDriveDb = 24.0;

    const auto input = pink (96000, 5);
    const auto a = run<SatureDsp> (clean, input);
    const auto b = run<SatureDsp> (driven, input);

    CHECK (rmsDb (b.left, 24000) == Catch::Approx (rmsDb (a.left, 24000)).margin (1.0));
}

TEST_CASE ("Sature's Low drive does nothing while Low is Off", "[sature][w2]")
{
    auto clean = bands (0.0, 6000.0);
    auto driven = clean;
    driven.lowDriveDb = 24.0;

    const auto input = pink (24000, 13);
    const auto a = run<SatureDsp> (clean, input);
    const auto b = run<SatureDsp> (driven, input);

    REQUIRE (a.left == b.left);
}
