// Phase 4 Wave 2 for Fancy MultiComp: the dynamic-phase crossover (docs/multicomp.md,
// "Phase 4 Wave 2", section 4). LR 24, LR 48 and Linear are held to their old output by
// MultiCompQ26Tests' legacy test, which still runs every crossover type against the old DSP.

#include "MultiCompDsp.h"
#include "MultiCompProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = std::numbers::pi;

MultiCompDsp::Targets threeBands (multicomp::CrossoverType crossover)
{
    MultiCompDsp::Targets targets;
    targets.numBands = 3;
    targets.crossover = crossover;
    targets.bands[0].topHz = 200.0;
    targets.bands[1].topHz = 2000.0;

    for (auto& band : targets.bands)
    {
        band.thresholdDb = 0.0;
        band.ratio = 2.0;
        band.autoMakeup = false;
        band.attackMs = 1.0;
        band.releaseMs = 50.0;
    }

    return targets;
}

struct Stereo
{
    std::vector<float> left, right;
};

Stereo render (const MultiCompDsp::Targets& targets, Stereo signal, int block = 256)
{
    MultiCompDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    for (int start = 0; start < (int) signal.left.size(); start += block)
    {
        float* main[] { signal.left.data() + start, signal.right.data() + start };
        const float* key[2] {};
        dsp.process (main, 2, key, 0, std::min (block, (int) signal.left.size() - start));
    }

    return signal;
}

Stereo quietNoise (int numSamples, unsigned seed, double level = 0.03)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<double> noise (-level, level);
    Stereo signal { std::vector<float> ((size_t) numSamples), std::vector<float> ((size_t) numSamples) };

    for (int i = 0; i < numSamples; ++i)
    {
        signal.left[(size_t) i] = (float) noise (random);
        signal.right[(size_t) i] = (float) noise (random);
    }

    return signal;
}

double magnitudeDbAt (const std::vector<float>& samples, double hz)
{
    std::complex<double> sum;

    for (size_t i = 0; i < samples.size(); ++i)
        sum += (double) samples[i] * std::polar (1.0, -2.0 * pi * hz * (double) i / rate);

    return 20.0 * std::log10 (std::abs (sum));
}
} // namespace

TEST_CASE ("MultiComp W2: Dynamic phase leaves idle audio untouched, bit for bit, with no latency", "[multicomp][w2]")
{
    const auto input = quietNoise (48000, 1);
    const auto targets = threeBands (multicomp::CrossoverType::dynamic);

    MultiCompDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);
    CHECK (dsp.getLatencySamples() == 0);

    const auto out = render (targets, input);
    CHECK (out.left == input.left);
    CHECK (out.right == input.right);
}

TEST_CASE ("MultiComp W2: equal band gains are a plain gain", "[multicomp][w2]")
{
    const auto input = quietNoise (24000, 2);
    auto targets = threeBands (multicomp::CrossoverType::dynamic);

    for (auto& band : targets.bands)
        band.gainDb = -6.0;

    const auto out = render (targets, input);
    const auto gain = std::pow (10.0, -6.0 / 20.0);
    auto worst = 0.0;

    for (size_t i = 0; i < input.left.size(); ++i)
        worst = std::max (worst, std::abs ((double) out.left[i] - gain * input.left[i]));

    CHECK (worst < 1.0e-6);
}

TEST_CASE ("MultiComp W2: a band's gain takes LR 24's shape within 0.6 dB", "[multicomp][w2]")
{
    // An impulse, far under every threshold, so only the bands' make-up gains act.
    Stereo impulse { std::vector<float> (32768, 0.0f), std::vector<float> (32768, 0.0f) };
    impulse.left[100] = impulse.right[100] = 0.25f;

    for (const auto db : { -6.0, 6.0 })
    {
        auto dynamic = threeBands (multicomp::CrossoverType::dynamic);
        auto lr = threeBands (multicomp::CrossoverType::lr24);
        dynamic.bands[1].gainDb = lr.bands[1].gainDb = db;

        const auto a = render (dynamic, impulse), b = render (lr, impulse);
        auto worst = 0.0, worstHz = 0.0;

        for (auto hz = 30.0; hz < 18000.0; hz *= 1.1)
        {
            const auto error = std::abs (magnitudeDbAt (a.left, hz) - magnitudeDbAt (b.left, hz));

            if (error > worst)
            {
                worst = error;
                worstHz = hz;
            }
        }

        INFO ("band 2 at " << db << " dB: worst " << worst << " dB at " << worstHz << " Hz; centre " << magnitudeDbAt (a.left, 632.0) - 20.0 * std::log10 (0.25));
        CHECK (worst < 0.6);
        CHECK (magnitudeDbAt (a.left, 632.0) - 20.0 * std::log10 (0.25) == Catch::Approx (db).margin (0.3));
    }
}

TEST_CASE ("MultiComp W2: fast compression through the shelves is as smooth as through LR 24", "[multicomp][w2]")
{
    // A 1 kHz tone that jumps 20 dB every 50 ms, into band 2 with a 1 ms attack.
    Stereo tone { std::vector<float> (96000), std::vector<float> (96000) };

    for (int i = 0; i < 96000; ++i)
        tone.left[(size_t) i] = tone.right[(size_t) i] = (float) ((i / 2400) % 2 == 0 ? 0.05 : 0.5) * (float) std::sin (2.0 * pi * 1000.0 * i / rate);

    auto dynamic = threeBands (multicomp::CrossoverType::dynamic);
    auto lr = threeBands (multicomp::CrossoverType::lr24);
    dynamic.bands[1].thresholdDb = lr.bands[1].thresholdDb = -20.0;
    dynamic.bands[1].ratio = lr.bands[1].ratio = 8.0;

    const auto a = render (dynamic, tone), b = render (lr, tone);

    const auto largestStep = [] (const std::vector<float>& x)
    {
        auto step = 0.0;

        for (size_t i = 4800; i < x.size(); ++i)
            step = std::max (step, (double) std::abs (x[i] - x[i - 1]));

        return step;
    };

    INFO ("largest step: dynamic " << largestStep (a.left) << ", LR 24 " << largestStep (b.left));
    CHECK (largestStep (a.left) < largestStep (b.left) * 1.25);
}

TEST_CASE ("MultiComp W2: Solo and Mute use the LR 24 bands", "[multicomp][w2]")
{
    const auto input = quietNoise (24000, 3, 0.3);

    for (const auto solo : { true, false })
    {
        auto dynamic = threeBands (multicomp::CrossoverType::dynamic);
        auto lr = threeBands (multicomp::CrossoverType::lr24);
        (solo ? dynamic.bands[1].solo : dynamic.bands[1].mute) = true;
        (solo ? lr.bands[1].solo : lr.bands[1].mute) = true;

        const auto a = render (dynamic, input), b = render (lr, input);
        CHECK (a.left == b.left);
        CHECK (a.right == b.right);
    }
}

TEST_CASE ("MultiComp W2: a Mid band leaves the side alone", "[multicomp][w2]")
{
    auto input = quietNoise (24000, 4, 0.3);

    for (size_t i = 0; i < input.left.size(); ++i)
        input.right[i] = -input.left[i]; // side only

    auto targets = threeBands (multicomp::CrossoverType::dynamic);
    targets.bands[1].channel = MultiCompDsp::mid;
    targets.bands[1].gainDb = -12.0;

    const auto out = render (targets, input);
    CHECK (out.left == input.left);
    CHECK (out.right == input.right);
}

TEST_CASE ("MultiComp W2: Dynamic phase is the fourth Crossover choice, and LR 24 stays the default", "[multicomp][w2]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    MultiCompProcessor processor;
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.getApvts().getParameter ("crossover"));
    REQUIRE (choice != nullptr);
    CHECK (choice->choices.size() == 4);
    CHECK (choice->choices[3] == "Dynamic phase");
    CHECK (choice->getIndex() == 0);
}
