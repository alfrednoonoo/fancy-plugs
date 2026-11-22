// Phase 4 Wave 2 for Fancy Limit: the clipper before the limiter (docs/limit.md, "Phase 4 Wave 2:
// clipper", section 4). Clip off is held to v1.6.0 by LimitW1Tests' legacy test, which runs the
// current LimitDsp at its defaults.

#include "LimitDsp.h"

#include <juce_dsp/juce_dsp.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = 3.14159265358979323846;

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples), right ((size_t) numSamples) {}
    int size() const { return (int) left.size(); }
};

Stereo run (const LimitDsp::Targets& targets, Stereo signal, int blockSize = 333)
{
    LimitDsp dsp;
    dsp.setTargets (targets);
    dsp.prepare (rate);

    for (int start = 0; start < signal.size(); start += blockSize)
    {
        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, std::min (blockSize, signal.size() - start));
    }

    return signal;
}

// The model's drum-like signal: 60 Hz kicks with a 3 ms click, noise snares, a quiet pad.
Stereo drums()
{
    std::mt19937 random (3);
    std::normal_distribution<double> noise;
    const auto n = (int) (4.0 * rate);
    std::vector<double> x ((size_t) n);

    for (int i = 0; i < n; ++i)
        x[(size_t) i] = 0.05 * std::sin (2.0 * pi * 220.0 * i / rate);

    const auto beatLength = (int) (0.25 * rate);

    for (int beat = 0; beat < 16; ++beat)
    {
        const auto start = beat * beatLength;

        for (int k = 0; k < beatLength && start + k < n; ++k)
        {
            auto& sample = x[(size_t) (start + k)];

            if (beat % 2 == 0)
            {
                if (k < (int) (0.003 * rate))
                    sample += 0.9 * std::exp (-k / (0.004 * rate)) * noise (random);

                sample += 0.7 * std::exp (-k / (0.08 * rate)) * std::sin (2.0 * pi * 60.0 * k / rate);
            }
            else
            {
                sample += 0.6 * std::exp (-k / (0.03 * rate)) * noise (random);
            }
        }
    }

    Stereo s (n);

    for (int i = 0; i < n; ++i)
        s.left[(size_t) i] = s.right[(size_t) i] = (float) x[(size_t) i];

    return s;
}

double meanSquareDb (const std::vector<float>& x, int from)
{
    auto sum = 0.0;

    for (auto i = (size_t) from; i < x.size(); ++i)
        sum += (double) x[i] * (double) x[i];

    return 10.0 * std::log10 (sum / (double) (x.size() - (size_t) from));
}

// The loudest alias of a 7 kHz tone's odd harmonics that land under 20 kHz, in dBFS.
double worstAliasDbfs (const std::vector<float>& x)
{
    constexpr int order = 15, size = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> data ((size_t) size * 2, 0.0f);
    auto windowSum = 0.0;
    const auto start = (size_t) (0.5 * rate);

    for (int i = 0; i < size; ++i)
    {
        const auto w = 0.5 - 0.5 * std::cos (2.0 * pi * i / size);
        data[(size_t) i] = (float) (w * x[start + (size_t) i]);
        windowSum += w;
    }

    fft.performFrequencyOnlyForwardTransform (data.data(), true);

    auto worst = -300.0;

    for (int harmonic = 3; harmonic <= 25; harmonic += 2)
    {
        auto hz = std::fmod (7000.0 * harmonic, rate);

        if (hz > rate / 2)
            hz = rate - hz;

        if (hz > 20000.0 || std::abs (hz - 7000.0) < 300.0)
            continue;

        const auto bin = (int) std::lround (hz * size / rate);

        for (int b = std::max (1, bin - 3); b <= bin + 3; ++b)
            worst = std::max (worst, 20.0 * std::log10 (std::max (2.0 * (double) data[(size_t) b] / windowSum, 1.0e-15)));
    }

    return worst;
}
} // namespace

TEST_CASE ("Limit's clip curve: linear under the knee, never over its ceiling", "[limit][w2]")
{
    // Clip 12: the clipper's ceiling is the limiter's (1.0 in its scale).
    for (auto x = -8.0; x <= 8.0; x += 0.001)
        REQUIRE (std::abs (LimitDsp::softClip (x, 1.0)) <= 1.0);

    CHECK (LimitDsp::softClip (0.4, 1.0) == 0.4);   // 8 dB under: untouched

    // Clip 6: a tone 10 dB over Ceiling comes out at most 6 dB over.
    const auto ceiling6 = std::pow (10.0, 6.0 / 20.0);
    const auto tenOver = std::pow (10.0, 10.0 / 20.0);
    CHECK (20.0 * std::log10 (LimitDsp::softClip (tenOver, ceiling6)) <= 6.0 + 1.0e-9);
}

TEST_CASE ("Limit with Clip off renders the same whatever the Clip amount", "[limit][w2]")
{
    LimitDsp::Targets a;
    a.thresholdDb = -8.0;

    auto b = a;
    b.clipAmountDb = 11.0;

    const auto input = drums();
    REQUIRE (run (a, input).left == run (b, input).left);
}

TEST_CASE ("Limit's clipper shaving 3 dB makes the same push louder", "[limit][w2]")
{
    const auto input = drums();
    constexpr double push = 8.0;

    auto peak = 0.0;

    for (const auto sample : input.left)
        peak = std::max (peak, (double) std::abs (sample));

    LimitDsp::Targets limitOnly;
    limitOnly.thresholdDb = -push;
    limitOnly.ceilingDb = 0.0;

    // The clipper's ceiling 3 dB under the pushed peak: 12 - amount = peak over Ceiling - 3.
    auto clipped = limitOnly;
    clipped.clip = true;
    clipped.clipAmountDb = std::clamp (15.0 - (20.0 * std::log10 (peak) + push), 0.0, 12.0);

    const auto a = run (limitOnly, input);
    const auto b = run (clipped, input);
    const auto gain = meanSquareDb (b.left, (int) (0.25 * rate)) - meanSquareDb (a.left, (int) (0.25 * rate));

    INFO ("clip amount " << clipped.clipAmountDb << " dB, louder by " << gain << " dB");
    CHECK (gain > 0.1);
    CHECK (gain < 1.5);

    // The output still never passes Ceiling.
    for (const auto sample : b.left)
        REQUIRE (std::abs (sample) <= 1.0f + 1.0e-6f);
}

TEST_CASE ("Limit's clipper runs oversampled, so it folds back far less", "[limit][w2]")
{
    LimitDsp::Targets targets;
    targets.ceilingDb = 0.0;
    targets.clip = true;   // True peak on (the default): its margin keeps the final clip out of it
    targets.clipAmountDb = 6.0;   // the clipper's ceiling 6 dB over Ceiling

    // A 7 kHz sine 6 dB over the clipper's ceiling.
    Stereo input ((int) (1.5 * rate));

    for (int i = 0; i < input.size(); ++i)
        input.left[(size_t) i] = input.right[(size_t) i] = (float) (4.0 * std::sin (2.0 * pi * 7000.0 * i / rate));

    auto off = targets;
    off.oversampling = LimitDsp::osOff;
    auto four = targets;
    four.oversampling = LimitDsp::os4;

    const auto aliasOff = worstAliasDbfs (run (off, input).left);
    const auto alias4x = worstAliasDbfs (run (four, input).left);

    INFO ("aliases: off " << aliasOff << " dBFS, 4x " << alias4x << " dBFS");
    CHECK (alias4x < aliasOff - 20.0);
    CHECK (alias4x < -60.0);
}
