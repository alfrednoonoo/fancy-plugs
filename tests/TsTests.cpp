// Fancy TS (docs/ts.md, "Tests"). The model's figures are from docs/research/ts-model.py. The host,
// preset, pop-up, listen and workflow checks run from the suite's shared tests.

#include "TsDsp.h"
#include "TsProcessor.h"

#if FANCY_HAS_COMP
 #include "CompProcessor.h"
#endif

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

namespace
{
constexpr double pi = std::numbers::pi;

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples, 0.0f), right ((size_t) numSamples, 0.0f) {}
    int size() const { return (int) left.size(); }
};

Stereo run (const TsDsp::Targets& targets, Stereo signal, double rate, int blockSize = 333, const Stereo* key = nullptr, int numChannels = 2)
{
    TsDsp dsp;
    dsp.setTargets (targets);
    dsp.prepare (rate);

    for (int start = 0; start < signal.size(); start += blockSize)
    {
        const auto count = std::min (blockSize, signal.size() - start);
        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        const float* sidechain[] { key != nullptr ? key->left.data() + start : nullptr, key != nullptr ? key->right.data() + start : nullptr };
        dsp.process (channels, numChannels, key != nullptr ? sidechain : nullptr, key != nullptr ? 2 : 0, count);
    }

    return signal;
}

// The model's kick: a 5 ms click into a 120 ms 60 Hz body, 0.1 s in, peaking at levelDb.
Stereo kick (double rate, double levelDb, bool click = true, double seconds = 0.6)
{
    std::mt19937 random (2);
    std::normal_distribution<double> noise;
    Stereo s ((int) (seconds * rate));
    const auto start = (int) (0.1 * rate);
    auto peak = 0.0;
    std::vector<double> x ((size_t) s.size(), 0.0);

    for (int i = start; i < s.size(); ++i)
    {
        const auto t = (double) (i - start) / rate;
        x[(size_t) i] = 0.5 * std::sin (2.0 * pi * 60.0 * t) * std::exp (-t / 0.12)
                      + (click && t < 0.005 ? 0.8 * noise (random) * std::exp (-t / 0.0015) : 0.0);
        peak = std::max (peak, std::abs (x[(size_t) i]));
    }

    const auto scale = std::pow (10.0, levelDb / 20.0) / peak;

    for (int i = 0; i < s.size(); ++i)
        s.left[(size_t) i] = s.right[(size_t) i] = (float) (x[(size_t) i] * scale);

    return s;
}

// The gain applied at sample i, in dB, where the input isn't near a zero crossing.
double gainAt (const Stereo& in, const Stereo& out, int i)
{
    return 20.0 * std::log10 (std::abs ((double) out.left[(size_t) i] / (double) in.left[(size_t) i]));
}

// The gain around a time (ms after the hit), from the loudest input sample in a 2 ms window.
double gainNear (const Stereo& in, const Stereo& out, double rate, double ms)
{
    const auto centre = (int) ((0.1 + ms * 0.001) * rate);
    auto best = centre;

    for (int i = centre - (int) (0.001 * rate); i <= centre + (int) (0.001 * rate); ++i)
        if (std::abs (in.left[(size_t) i]) > std::abs (in.left[(size_t) best]))
            best = i;

    return gainAt (in, out, best);
}
} // namespace

TEST_CASE ("TS at its defaults passes the audio through to the bit", "[ts]")
{
    const auto rate = GENERATE (44100.0, 48000.0, 96000.0);
    const auto blockSize = GENERATE (1, 64, 4096, 8192);
    const auto numChannels = GENERATE (1, 2);

    auto input = kick (rate, -6.0, true, 0.3);

    for (int i = 0; i < input.size(); ++i)
        input.right[(size_t) i] = 0.5f * input.left[(size_t) i];

    const auto output = run (TsDsp::Targets {}, input, rate, blockSize, nullptr, numChannels);

    INFO ("rate " << rate << ", block " << blockSize << ", channels " << numChannels);
    REQUIRE (output.left == input.left);

    if (numChannels == 2)
        REQUIRE (output.right == input.right);
}

TEST_CASE ("TS: Attack and Sustain shape a kick as the model does", "[ts]")
{
    constexpr double rate = 48000.0;
    const auto input = kick (rate, -6.0);

    TsDsp::Targets attackOnly;
    attackOnly.attackDb = 6.0;
    const auto boosted = run (attackOnly, input, rate);

    auto largest = -100.0;

    for (int i = (int) (0.1 * rate); i < (int) (0.11 * rate); ++i)
        if (std::abs (input.left[(size_t) i]) > 1.0e-3f)
            largest = std::max (largest, gainAt (input, boosted, i));

    CHECK (largest == Catch::Approx (6.0).margin (0.3));
    CHECK (std::abs (gainNear (input, boosted, rate, 100.0)) < 0.3);

    TsDsp::Targets sustainOnly;
    sustainOnly.sustainDb = -6.0;
    const auto tamed = run (sustainOnly, input, rate);

    CHECK (std::abs (gainNear (input, tamed, rate, 2.0)) < 0.3);
    CHECK (gainNear (input, tamed, rate, 100.0) == Catch::Approx (-4.9).margin (0.5));
}

TEST_CASE ("TS shapes a quiet hit as it does a loud one", "[ts]")
{
    constexpr double rate = 48000.0;
    TsDsp::Targets targets;
    targets.attackDb = 6.0;
    targets.sustainDb = -6.0;

    const auto loud = kick (rate, -6.0), quiet = kick (rate, -30.0);
    const auto loudOut = run (targets, loud, rate), quietOut = run (targets, quiet, rate);
    const auto floor = (float) std::pow (10.0, -80.0 / 20.0);

    for (int i = 0; i < loud.size(); ++i)
        if (std::abs (quiet.left[(size_t) i]) > floor)
            REQUIRE (std::abs (gainAt (loud, loudOut, i) - gainAt (quiet, quietOut, i)) < 1.5);
}

TEST_CASE ("TS leaves a steady tone alone", "[ts]")
{
    constexpr double rate = 48000.0;
    Stereo tone ((int) rate);

    for (int i = 0; i < tone.size(); ++i)
        tone.left[(size_t) i] = tone.right[(size_t) i] = (float) (0.5 * std::sin (2.0 * pi * 440.0 * i / rate));

    TsDsp::Targets targets;
    targets.attackDb = 12.0;
    targets.sustainDb = 12.0;
    const auto out = run (targets, tone, rate);

    for (int i = (int) (0.5 * rate); i < tone.size(); ++i)
        if (std::abs (tone.left[(size_t) i]) > 0.05f)
            REQUIRE (std::abs (gainAt (tone, out, i)) < 0.05);
}

TEST_CASE ("TS: Lookahead's latency is reported and the dry path lines up", "[ts]")
{
    const auto rate = GENERATE (44100.0, 48000.0, 96000.0);
    TsDsp::Targets targets;
    targets.lookahead = 3;   // 5 ms
    targets.wetGain = 0.0;
    targets.dryGain = 1.0;

    TsDsp dsp;
    dsp.setTargets (targets);
    dsp.prepare (rate);
    const auto latency = dsp.getLatencySamples();
    CHECK (latency == (int) std::lround (0.005 * rate));

    const auto input = kick (rate, -6.0, true, 0.3);
    const auto out = run (targets, input, rate);

    for (int i = latency; i < input.size(); ++i)
        REQUIRE (out.left[(size_t) i] == input.left[(size_t) (i - latency)]);
}

TEST_CASE ("TS: Clip output never passes 0 dBFS", "[ts]")
{
    constexpr double rate = 48000.0;
    const auto oversampling = GENERATE (0, 1, 2);
    TsDsp::Targets targets;
    targets.attackDb = 12.0;
    targets.clip = true;
    targets.oversampling = oversampling;

    const auto out = run (targets, kick (rate, -1.0), rate);

    for (const auto sample : out.left)
        REQUIRE (std::abs (sample) <= 1.0f);
}

TEST_CASE ("TS: the sidechain finds the hits, and Highpass keeps a low thump out", "[ts]")
{
    constexpr double rate = 48000.0;

    // A steady tone on the main input, the kick on the sidechain.
    Stereo tone ((int) (0.6 * rate));

    for (int i = 0; i < tone.size(); ++i)
        tone.left[(size_t) i] = tone.right[(size_t) i] = (float) (0.3 * std::sin (2.0 * pi * 1000.0 * i / rate));

    TsDsp::Targets fromKey;
    fromKey.detector = TsDsp::sidechainLR;
    fromKey.attackDb = 12.0;
    const auto key = kick (rate, -6.0);
    const auto keyed = run (fromKey, tone, rate, 333, &key);

    auto largest = -100.0;

    for (int i = (int) (0.1 * rate); i < (int) (0.12 * rate); ++i)
        if (std::abs (tone.left[(size_t) i]) > 0.05f)
            largest = std::max (largest, gainAt (tone, keyed, i));

    CHECK (largest > 6.0);

    // A low thump (no click) over steady bright noise. Without Highpass it's a 20 dB jump, a hit;
    // with Highpass at 500 Hz the thump sits under the noise, so nothing counts.
    std::mt19937 random (7);
    std::uniform_real_distribution<float> noise (-0.1f, 0.1f);
    auto mix = kick (rate, 0.0, false);

    for (int i = 0; i < mix.size(); ++i)
    {
        const auto n = noise (random);
        mix.left[(size_t) i] += n;
        mix.right[(size_t) i] += n;
    }

    TsDsp::Targets open;
    open.attackDb = 12.0;
    open.sensitivity = 1.0;
    open.deadbandDb = 3.0;
    auto filtered = open;
    filtered.highpassOn = true;
    filtered.highpassHz = 500.0;

    const auto peakGain = [&] (const Stereo& out)
    {
        auto best = -100.0;

        for (int i = (int) (0.1 * rate); i < (int) (0.12 * rate); ++i)
            if (std::abs (mix.left[(size_t) i]) > 1.0e-3f)
                best = std::max (best, gainAt (mix, out, i));

        return best;
    };

    CHECK (peakGain (run (open, mix, rate)) > 9.0);
    CHECK (peakGain (run (filtered, mix, rate)) < 3.0);
}

TEST_CASE ("TS: Listen to change plays only what changed", "[ts]")
{
    constexpr double rate = 48000.0;
    TsDsp::Targets targets;
    targets.listen = true;   // Attack and Sustain at 0: nothing changes
    const auto out = run (targets, kick (rate, -6.0, true, 0.3), rate);

    for (const auto sample : out.left)
        REQUIRE (sample == 0.0f);
}

// CPU, by §0.4 of the Phase 2.1 brief: five runs in one build, the median, and the ratio to Fancy
// Comp at its defaults, measured the same way as the workflow test (processBlock, 48 kHz, 128
// samples, stereo noise, 10 s). Decision 4 A: under 1 x at defaults, under 1.5 x with Clip at 4x.
TEST_CASE ("TS CPU measurement", "[.measure]")
{
    constexpr double measureRate = 48000.0, seconds = 10.0;
    constexpr int measureBlock = 128;

    const auto cpu = [] (fancy::FancyProcessor& processor)
    {
        std::vector<double> runs;

        for (int pass = 0; pass < 5; ++pass)
        {
            processor.setRateAndBufferSizeDetails (measureRate, measureBlock);
            processor.prepareToPlay (measureRate, measureBlock);
            processor.waitForBackgroundWork (20000);

            juce::AudioBuffer<float> block (juce::jmax (2, processor.getTotalNumInputChannels()), measureBlock);
            juce::MidiBuffer midi;
            std::mt19937 random (8);
            std::uniform_real_distribution<float> noise (-0.25f, 0.25f);
            std::chrono::duration<double> spent {};

            for (int done = 0; done < (int) (seconds * measureRate); done += measureBlock)
            {
                block.clear();

                for (int ch = 0; ch < processor.getMainBusNumInputChannels(); ++ch)
                    for (int i = 0; i < measureBlock; ++i)
                        block.setSample (ch, i, noise (random));

                midi.clear();
                const auto start = std::chrono::steady_clock::now();
                processor.processBlock (block, midi);
                spent += std::chrono::steady_clock::now() - start;
            }

            processor.releaseResources();
            runs.push_back (100.0 * spent.count() / seconds);
        }

        std::sort (runs.begin(), runs.end());
        return runs[2];
    };

    const auto set = [] (TsProcessor& processor, const char* id, float value)
    {
        auto* parameter = processor.getApvts().getParameter (id);
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    double reference = 0.0;

   #if FANCY_HAS_COMP
    {
        CompProcessor comp;
        reference = cpu (comp);
    }
   #endif

    TsProcessor defaults;
    const auto plain = cpu (defaults);

    TsProcessor clipped;
    set (clipped, "clip", 1.0f);
    set (clipped, "oversampling", (float) TsDsp::os4);
    const auto clip4x = cpu (clipped);

    const auto ratio = [reference] (double value) { return reference > 0.0 ? value / reference : 0.0; };

    std::printf ("ts cpu (median of 5, 48 kHz, 128 samples, stereo): Fancy Comp default %.3f %%; TS default %.3f %% (%.2f x Comp, target < 1 x); "
                 "Clip output at 4x %.3f %% (%.2f x Comp, target < 1.5 x)\n",
                 reference, plain, ratio (plain), clip4x, ratio (clip4x));
}
