// Phase 4 Wave 2 for Fancy DS, items 32 and 33: Reduce (Both, Mid, Side) and Hiss mode
// (docs/ds.md, "Phase 4 Wave 2: Mid/Side reduction and Hiss mode", section 5). Test 3's signal
// and measurement follow docs/research/ds-hiss-build-check.py (run it for the expected figures).
// DS-Mini's Hiss profile (test 5) is in tests/DsMiniW2bTests.cpp.

#include "DsProcessor.h"
#include "LegacyDeEsserW2.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <fancy/dsp/DeEsser.h>
#include <fancy/dsp/Svf.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = 3.14159265358979323846;

using Engine = fancy::dsp::DeEsser;
using Legacy = fancy::dsp::LegacyDeEsserW2;

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples), right ((size_t) numSamples) {}
    int size() const { return (int) left.size(); }
};

template <typename E, typename Targets>
Stereo run (const Targets& targets, Stereo signal, int blockSize = 333)
{
    E engine;
    engine.prepare (rate);
    engine.setSynchronousDesign (true);
    engine.snapToTargets (targets);

    for (int start = 0; start < signal.size(); start += blockSize)
    {
        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        const float* sidechain[] { nullptr, nullptr };
        engine.process (channels, 2, sidechain, 0, std::min (blockSize, signal.size() - start));
    }

    return signal;
}

// Different in each channel: a sibilant-band tone that comes and goes, and noise.
Stereo stereoTest (int numSamples, unsigned seed)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<float> noise (-0.05f, 0.05f);
    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto on = std::fmod ((double) i / rate, 0.25) < 0.1 ? 1.0 : 0.1;
        signal.left[(size_t) i] = (float) (0.3 * on * std::sin (2.0 * pi * 6800.0 * i / rate)) + noise (random);
        signal.right[(size_t) i] = (float) (0.2 * on * std::sin (2.0 * pi * 5100.0 * i / rate + 1.0)) + noise (random);
    }

    return signal;
}

double meanSquare (const std::vector<float>& x)
{
    double sum = 0.0;

    for (auto v : x)
        sum += (double) v * v;

    return sum / std::max<size_t> (1, x.size());
}

// What docs/research/ds-hiss-build-check.py measures: energy after a 4th-order Butterworth
// high-pass at 8 kHz, over the masked samples.
double above8kDb (const std::vector<float>& x, const std::vector<bool>& mask)
{
    fancy::dsp::SvfCoefficients first, second;
    first.set (8000.0, 0.5412, rate);
    second.set (8000.0, 1.3066, rate);
    fancy::dsp::Svf a, b;
    double sum = 0.0;
    int count = 0;

    for (size_t i = 0; i < x.size(); ++i)
    {
        const auto y = b.tick (second, a.tick (first, (double) x[i]).high).high;

        if (mask[i])
        {
            sum += y * y;
            ++count;
        }
    }

    return 10.0 * std::log10 (sum / std::max (1, count) + 1.0e-30);
}

// The model's vocal with tape hiss (docs/research/ds-hiss-prepush.py): 160 Hz harmonics falling
// at 1/h, sung 0.6 s in every second, with Gaussian hiss at -54 dBFS throughout.
struct HissTest
{
    Stereo audio { (int) (4.0 * rate) };
    std::vector<bool> pauses, loud;
};

HissTest hissTest()
{
    HissTest test;
    const auto n = test.audio.size();
    std::mt19937 random (33);
    std::uniform_real_distribution<double> phase (0.0, 6.0);
    std::normal_distribution<double> gaussian;

    std::vector<double> voice ((size_t) n, 0.0);

    for (int h = 1; h < 40; ++h)
    {
        const auto offset = phase (random);

        for (int i = 0; i < n; ++i)
            voice[(size_t) i] += std::sin (2.0 * pi * 160.0 * h * i / rate + offset) / h;
    }

    test.pauses.resize ((size_t) n);
    test.loud.resize ((size_t) n);
    const auto hiss = std::pow (10.0, -54.0 / 20.0);

    for (int i = 0; i < n; ++i)
    {
        const auto t = std::fmod ((double) i / rate, 1.0);
        const auto phrase = t < 0.6;
        const auto x = (phrase ? 0.2 * voice[(size_t) i] : 0.0) + hiss * gaussian (random);

        test.audio.left[(size_t) i] = test.audio.right[(size_t) i] = (float) x;
        test.pauses[(size_t) i] = ! phrase && t > 0.7;
        test.loud[(size_t) i] = phrase && t > 0.1 && t < 0.5;
    }

    return test;
}

Engine::Targets hissTargets()
{
    Engine::Targets t;
    t.shelf = t.hiss = true;
    t.freqHz = 4000.0;
    t.thresholdDb = -38.0;
    t.rangeDb = -12.0;
    t.relative = true;   // ignored: Hiss mode always measures in dBFS
    return t;
}
} // namespace

// 1. Compatibility ------------------------------------------------------------

TEST_CASE ("DS with Reduce Both renders exactly as the v1.7.0 engine in Band and Shelf", "[ds][w2b]")
{
    const auto linear = GENERATE (false, true);
    const auto shelf = GENERATE (false, true);
    const auto dual = GENERATE (false, true);
    const auto wide = GENERATE (false, true);
    const auto link = GENERATE (false, true);

    Legacy::Targets old;
    old.thresholdDb = -30.0;
    old.relative = false;
    old.linearPhase = linear;
    old.shelf = shelf;
    old.dual = dual;
    old.wide = wide;
    old.link = link;
    old.lowFreqHz = 3000.0;
    old.lowThresholdDb = -40.0;

    Engine::Targets now;
    now.thresholdDb = old.thresholdDb;
    now.relative = old.relative;
    now.linearPhase = linear;
    now.shelf = shelf;
    now.dual = dual;
    now.wide = wide;
    now.link = link;
    now.lowFreqHz = old.lowFreqHz;
    now.lowThresholdDb = old.lowThresholdDb;

    const auto input = stereoTest (24000, 5);
    const auto a = run<Legacy> (old, input);
    const auto b = run<Engine> (now, input);

    INFO ("linear " << linear << ", shelf " << shelf << ", dual " << dual << ", wide " << wide << ", link " << link);
    REQUIRE (a.left == b.left);
    REQUIRE (a.right == b.right);
}

// 2. Reduce Mid and Side --------------------------------------------------------

TEST_CASE ("DS Reduce Mid leaves a side-only input untouched and cuts a mid-only one; Side the other way round", "[ds][w2b]")
{
    const auto side = GENERATE (false, true);
    const auto wide = GENERATE (false, true);

    // Cutting hard: absolute, Threshold at the bottom.
    Engine::Targets t;
    t.relative = false;
    t.thresholdDb = -60.0;
    t.rangeDb = -12.0;
    t.wide = wide;
    t.reduce = side ? Engine::reduceSide : Engine::reduceMid;

    const auto test = stereoTest (24000, 9);
    Stereo midOnly (test.size()), sideOnly (test.size());

    for (int i = 0; i < test.size(); ++i)
    {
        midOnly.left[(size_t) i] = midOnly.right[(size_t) i] = test.left[(size_t) i];
        sideOnly.left[(size_t) i] = test.left[(size_t) i];
        sideOnly.right[(size_t) i] = -test.left[(size_t) i];
    }

    const auto& untouched = side ? midOnly : sideOnly;
    const auto& cut = side ? sideOnly : midOnly;

    INFO ((side ? "Side" : "Mid") << (wide ? ", Wide" : ", Split"));

    const auto kept = run<Engine> (t, untouched);
    CHECK (kept.left == untouched.left);
    CHECK (kept.right == untouched.right);

    const auto reduced = run<Engine> (t, cut);
    const auto changeDb = 10.0 * std::log10 (meanSquare (reduced.left) / meanSquare (cut.left));
    std::printf ("ds reduce %s (%s): the cut channel changes by %.2f dB\n", side ? "Side" : "Mid", wide ? "Wide" : "Split", changeDb);
    CHECK (changeDb < -1.0);
}

// 3 and 4. Hiss mode -------------------------------------------------------------

TEST_CASE ("DS Hiss mode takes the hiss out of the gaps and leaves the loud parts' highs", "[ds][w2b]")
{
    const auto test = hissTest();
    const auto out = run<Engine> (hissTargets(), test.audio);

    const auto pauses = above8kDb (out.left, test.pauses) - above8kDb (test.audio.left, test.pauses);
    const auto loud = above8kDb (out.left, test.loud) - above8kDb (test.audio.left, test.loud);

    std::printf ("ds hiss mode: pauses %.2f dB (limit -4.5), loud parts %.2f dB (limit 0.5)\n", pauses, loud);
    CHECK (pauses <= -4.5);
    CHECK (std::abs (loud) < 0.5);
}

TEST_CASE ("DS Hiss mode with Range 0 dB leaves the input as it was", "[ds][w2b]")
{
    const auto test = hissTest();
    auto t = hissTargets();
    t.rangeDb = 0.0;

    const auto out = run<Engine> (t, test.audio);
    CHECK (out.left == test.audio.left);
    CHECK (out.right == test.audio.right);
}

TEST_CASE ("DS reads Detection's third choice as Hiss mode, and Reduce", "[ds][w2b]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    DsProcessor ds;

    CHECK (! ds.readTargets().hiss);
    CHECK (ds.readTargets().reduce == Engine::reduceBoth);

    ds.setParameter ("shape", 1.0f);
    CHECK (ds.readTargets().shelf);
    CHECK (! ds.readTargets().hiss);

    ds.setParameter ("shape", 2.0f);
    CHECK (ds.readTargets().shelf);
    CHECK (ds.readTargets().hiss);

    ds.setParameter ("reduceChan", 2.0f);
    CHECK (ds.readTargets().reduce == Engine::reduceSide);
}
