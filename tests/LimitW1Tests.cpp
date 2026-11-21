// Phase 4 Wave 1 for Fancy Limit: Gain lock, DC filter, release curve and overs
// (docs/limit.md, "Phase 4 Wave 1", section 6). Auto to target is the editor's, checked in REAPER.

#include "LegacyLimitDsp.h"
#include "LimitDsp.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <fancy/dsp/TruePeakLimiter.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>
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

template <typename Dsp, typename Targets>
Stereo run (const Targets& targets, Stereo signal, int blockSize = 333)
{
    Dsp dsp;
    dsp.setTargets (targets);
    dsp.prepare (rate);

    for (int start = 0; start < signal.size(); start += blockSize)
    {
        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, std::min (blockSize, signal.size() - start));
    }

    return signal;
}

Stereo programme (int numSamples, unsigned seed)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<double> noise (-1.0, 1.0);
    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto envelope = 0.1 + 0.8 * std::pow (0.5 + 0.5 * std::sin (2.0 * pi * 4.0 * i / rate), 6.0);
        const auto tone = std::sin (2.0 * pi * 110.0 * i / rate) + 0.3 * std::sin (2.0 * pi * 3100.0 * i / rate);

        signal.left[(size_t) i] = (float) (envelope * (0.7 * tone + 0.2 * noise (random)));
        signal.right[(size_t) i] = (float) (envelope * (0.6 * tone + 0.3 * noise (random)));
    }

    return signal;
}

Stereo sine (double hz, double amplitude, int numSamples, double offset = 0.0)
{
    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) (offset + amplitude * std::sin (2.0 * pi * hz * i / rate));

    return signal;
}

double rmsDb (const std::vector<float>& samples, int from)
{
    double sum = 0.0;

    for (size_t i = (size_t) from; i < samples.size(); ++i)
        sum += (double) samples[i] * (double) samples[i];

    return 10.0 * std::log10 (std::max (sum / (double) (samples.size() - (size_t) from), 1.0e-30));
}
} // namespace

// 1. Compatibility ------------------------------------------------------------

TEST_CASE ("Limit Wave 1 at its defaults renders exactly as Limit did in v1.6.0", "[limit][w1]")
{
    const auto style = GENERATE (0, 1, 2);
    const auto oversampling = GENERATE (0, 1, 2);

    LegacyLimitDsp::Targets old;
    old.thresholdDb = -9.0;
    old.ceilingDb = -1.0;
    old.style = style;
    old.oversampling = oversampling;
    old.autoRelease = style != 1;

    LimitDsp::Targets now;
    now.thresholdDb = old.thresholdDb;
    now.ceilingDb = old.ceilingDb;
    now.releaseMs = old.releaseMs;
    now.lookaheadMs = old.lookaheadMs;
    now.link = old.link;
    now.style = old.style;
    now.truePeak = old.truePeak;
    now.autoRelease = old.autoRelease;
    now.listen = old.listen;
    now.oversampling = old.oversampling;
    now.linearPhase = old.linearPhase;
    now.dither = old.dither;
    now.outputGain = old.outputGain;
    now.bypass = old.bypass;

    const auto input = programme (24000, 9);
    const auto a = run<LegacyLimitDsp> (old, input);
    const auto b = run<LimitDsp> (now, input);

    INFO ("style " << style << ", oversampling " << oversampling);
    REQUIRE (a.left == b.left);
    REQUIRE (a.right == b.right);
}

// 2. Gain lock ----------------------------------------------------------------

TEST_CASE ("Gain lock turns the output down by as much as Threshold pushes it up", "[limit][w1]")
{
    LimitDsp::Targets targets;
    targets.thresholdDb = -6.0;

    auto locked = targets;
    locked.gainLock = true;
    locked.gainLockRefDb = 0.0;

    // A quiet tone, so the limiter itself does nothing.
    const auto input = sine (1000.0, std::pow (10.0, -20.0 / 20.0), (int) rate);
    const auto open = run<LimitDsp> (targets, input);
    const auto held = run<LimitDsp> (locked, input);

    CHECK (rmsDb (held.left, (int) rate / 2) - rmsDb (open.left, (int) rate / 2) == Catch::Approx (-6.0).margin (0.01));

    // With the reference at -6, nothing changes at -6.
    locked.gainLockRefDb = -6.0;
    const auto same = run<LimitDsp> (locked, input);
    CHECK (rmsDb (same.left, (int) rate / 2) == Catch::Approx (rmsDb (open.left, (int) rate / 2)).margin (0.01));
}

// 4. DC filter ----------------------------------------------------------------

TEST_CASE ("The DC filter removes an offset and leaves the lows", "[limit][w1]")
{
    LimitDsp::Targets targets;
    targets.dcFilter = true;

    const auto offset = run<LimitDsp> (targets, sine (1000.0, 0.0, (int) (1.5 * rate), 0.2));
    auto worst = 0.0;

    for (int i = (int) rate; i < offset.size(); ++i)
        worst = std::max (worst, std::abs ((double) offset.left[(size_t) i]));

    CHECK (worst < std::pow (10.0, -80.0 / 20.0));

    // 40 Hz loses under 0.1 dB (a 5 Hz first-order high-pass: 0.07 dB).
    targets.dcFilter = false;
    const auto low = sine (40.0, 0.1, (int) rate);
    const auto plain = run<LimitDsp> (targets, low);
    targets.dcFilter = true;
    const auto filtered = run<LimitDsp> (targets, low);

    CHECK (rmsDb (filtered.left, (int) rate / 2) - rmsDb (plain.left, (int) rate / 2) > -0.1);
}

// 5. Release curve ------------------------------------------------------------

TEST_CASE ("Linear release recovers at a constant rate in dB", "[limit][w1]")
{
    // The release in dB over two equal steps: equal for Linear, shrinking for Exponential.
    const auto steps = [] (bool linear)
    {
        fancy::dsp::TruePeakLimiter limiter;
        fancy::dsp::TruePeakLimiter::Settings settings;
        settings.releaseMs = 50.0;
        settings.linearRelease = linear;
        settings.truePeak = false;
        limiter.prepare (rate, settings);

        std::vector<double> gains;

        for (int i = 0; i < (int) rate / 4; ++i)
        {
            double l = i == 100 ? 4.0 : 0.0, r = l;
            limiter.process (l, r, 1.0);
            gains.push_back (20.0 * std::log10 (limiter.getLastGain (0)));
        }

        // From 10 ms after the hold at the deepest point ends, past the smoothing's own ramp.
        const auto lowest = *std::min_element (gains.begin(), gains.end());
        auto deepest = 0;

        for (int i = 0; i < (int) gains.size(); ++i)
            if (gains[(size_t) i] <= lowest + 1.0e-6)
                deepest = i;

        deepest += (int) (0.01 * rate);
        const auto step = (int) (0.025 * rate);
        return std::pair { gains[(size_t) (deepest + step)] - gains[(size_t) deepest],
                           gains[(size_t) (deepest + 2 * step)] - gains[(size_t) (deepest + step)] };
    };

    const auto [linearFirst, linearSecond] = steps (true);
    const auto [exponentialFirst, exponentialSecond] = steps (false);

    INFO ("linear " << linearFirst << ", " << linearSecond << " dB; exponential " << exponentialFirst << ", " << exponentialSecond << " dB");
    CHECK (linearFirst == Catch::Approx (8.686 / 2.0).margin (0.4));
    CHECK (linearSecond == Catch::Approx (linearFirst).margin (0.05 * linearFirst));
    CHECK (exponentialSecond < 0.6 * exponentialFirst);
}

// 6. Overs --------------------------------------------------------------------

TEST_CASE ("No output sample needs the final clip in normal use, and the count resets", "[limit][w1]")
{
    const auto oversampling = GENERATE (0, 2);

    LimitDsp::Targets targets;
    targets.thresholdDb = -12.0;
    targets.oversampling = oversampling;

    LimitDsp dsp;
    dsp.setTargets (targets);
    dsp.prepare (rate);

    auto signal = programme (48000, 4);

    for (int start = 0; start < signal.size(); start += 512)
    {
        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, std::min (512, signal.size() - start));
    }

    CHECK (dsp.getOvers() == 0);

    dsp.requestOversReset();
    float* channels[] { signal.left.data(), signal.right.data() };
    dsp.process (channels, 2, 64);
    CHECK (dsp.getOvers() == 0);
}
