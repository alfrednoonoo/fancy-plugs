// Phase 4 Wave 2 for Fancy Verb: the Decay module (docs/verb.md, "Phase 4 Wave 2: decay module",
// section 5). Test 1 (chains without it build as before) is VerbW1Tests' legacy test.

#include "IrBuilder.h"

#include <juce_dsp/juce_dsp.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = std::numbers::pi;

// Noise decaying with an RT60 of rtSeconds, over a -100 dB noise floor (a 24-bit file's).
std::vector<double> noiseIr (double seconds, double rtSeconds, unsigned seed)
{
    std::mt19937 random (seed);
    std::normal_distribution<double> noise;
    std::vector<double> ir ((size_t) (seconds * rate));

    for (size_t i = 0; i < ir.size(); ++i)
        ir[i] = 0.3 * noise (random) * std::exp (-6.907755 * (double) i / rate / rtSeconds) + 3.0e-6 * noise (random);

    return ir;
}

// A third octave around centre, as a smooth bell in log frequency (a brick wall that narrow
// rings for seconds by itself), then its RT60.
double bandRt (const std::vector<double>& ir, double centreHz)
{
    constexpr int order = 19, size = 1 << order;
    REQUIRE ((int) ir.size() * 2 <= size);

    juce::dsp::FFT fft (order);
    std::vector<float> data ((size_t) size * 2, 0.0f);

    for (size_t i = 0; i < ir.size(); ++i)
        data[i] = (float) ir[i];

    fft.performRealOnlyForwardTransform (data.data(), true);

    for (int k = 0; k <= size / 2; ++k)
    {
        const auto hz = (double) k * rate / size;
        const auto octaves = hz > 0.0 ? std::log2 (hz / centreHz) : -100.0;
        const auto weight = (float) std::exp (-0.5 * (octaves / (1.0 / 6.0)) * (octaves / (1.0 / 6.0)));
        data[(size_t) (2 * k)] *= weight;
        data[(size_t) (2 * k + 1)] *= weight;
    }

    for (int k = size / 2 + 1; k < size; ++k)
    {
        data[(size_t) (2 * k)] = data[(size_t) (2 * (size - k))];
        data[(size_t) (2 * k + 1)] = -data[(size_t) (2 * (size - k) + 1)];
    }

    fft.performRealOnlyInverseTransform (data.data());

    std::vector<double> energy (ir.size());

    for (size_t i = 0; i < ir.size(); ++i)
        energy[i] = (double) data[i] * (double) data[i];

    return verb::schroederRt60 (energy, rate);
}

verb::DecayReadout reshape (std::vector<double>& left, std::vector<double>& right, std::array<double, 3> multipliers)
{
    std::vector<std::vector<double>*> channels { &left, &right };
    return verb::reshapeDecay (channels, multipliers, 250.0, 4000.0, rate);
}

double levelDb (const std::vector<double>& x, size_t from, size_t to)
{
    auto sum = 0.0;

    for (size_t i = from; i < to; ++i)
        sum += x[i] * x[i];

    return 10.0 * std::log10 (std::max (sum / (double) (to - from), 1.0e-30));
}
} // namespace

TEST_CASE ("Verb Decay at 1 x everywhere leaves the IR unchanged", "[verb][w2][decay]")
{
    auto left = noiseIr (3.0, 2.0, 1), right = noiseIr (3.0, 2.0, 2);
    const auto originalLeft = left, originalRight = right;

    const auto readout = reshape (left, right, { 1.0, 1.0, 1.0 });

    for (size_t i = 0; i < left.size(); ++i)
    {
        REQUIRE (std::abs (left[i] - originalLeft[i]) <= 1.0e-6);
        REQUIRE (std::abs (right[i] - originalRight[i]) <= 1.0e-6);
    }

    for (int b = 0; b < 3; ++b)
    {
        CHECK (readout.measured[(size_t) b]);
        CHECK (readout.beforeSeconds[(size_t) b] == Catch::Approx (2.0).margin (0.2));
    }
}

TEST_CASE ("Verb Decay shortens the low band and lengthens the high band", "[verb][w2][decay]")
{
    auto left = noiseIr (4.0, 2.0, 3), right = noiseIr (4.0, 2.0, 4);
    const auto original = left;
    const auto midBefore = bandRt (original, 1000.0);

    const auto readout = reshape (left, right, { 0.5, 1.0, 2.0 });

    CHECK (bandRt (left, 100.0) == Catch::Approx (1.0).margin (0.1));
    CHECK (bandRt (left, 10000.0) == Catch::Approx (4.0).margin (0.3));
    CHECK (bandRt (left, 1000.0) == Catch::Approx (midBefore).epsilon (0.05));

    CHECK (readout.afterSeconds[0] == Catch::Approx (readout.beforeSeconds[0] * 0.5).epsilon (0.1));
    CHECK (readout.afterSeconds[2] > readout.beforeSeconds[2] * 1.6);

    // The first 50 ms hardly moves.
    CHECK (std::abs (levelDb (left, 0, 2400) - levelDb (original, 0, 2400)) < 0.5);
}

TEST_CASE ("Verb Decay doesn't lift the IR's noise floor", "[verb][w2][decay]")
{
    auto left = noiseIr (4.0, 2.0, 5), right = noiseIr (4.0, 2.0, 6);
    const auto original = left;

    const auto readout = reshape (left, right, { 1.0, 1.0, 4.0 });
    const auto end = left.size(), from = end - (size_t) (0.1 * rate);

    CHECK (levelDb (left, from, end) <= levelDb (original, from, end) + 0.5);
    CHECK (readout.limited[2]);
}

TEST_CASE ("Verb Decay leaves an IR with no decay as it is, and says so", "[verb][w2][decay]")
{
    std::vector<double> left ((size_t) rate, 0.0), right ((size_t) rate, 0.0);
    left[0] = right[0] = 1.0;
    const auto original = left;

    const auto readout = reshape (left, right, { 0.5, 1.0, 2.0 });

    REQUIRE (left == original);

    for (int b = 0; b < 3; ++b)
        CHECK_FALSE (readout.measured[(size_t) b]);
}
