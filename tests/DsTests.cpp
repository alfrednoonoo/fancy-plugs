// Fancy DS (docs/ds.md): the §2.4 tests of docs/BUILD_PROMPT_PHASE3_NEW_PLUGINS.md.

#include "DsDsp.h"
#include "DsProcessor.h"

#if FANCY_HAS_COMP
 #include "CompDsp.h"
#endif

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <fancy/dsp/Svf.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <random>
#include <vector>

using Catch::Approx;

namespace
{
constexpr double pi = std::numbers::pi;
constexpr double rate = 48000.0;

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples, 0.0f), right ((size_t) numSamples, 0.0f) {}
    int size() const { return (int) left.size(); }
};

struct Segment
{
    int start, end;
};

// A synthetic voice: a 150 Hz sawtooth through three vowel formants (700, 1200 and 2500 Hz),
// with "s" bursts of noise band-passed to 5-9 kHz. Each 100 ms burst follows 400 ms of vowel.
// The bursts' RMS is `burstDb` against the vowel's.
struct Voice
{
    Stereo audio;
    std::vector<Segment> bursts, vowels;

    Voice (double levelDb, double burstDb = -6.0, double seconds = 4.0) : audio ((int) (seconds * rate))
    {
        const auto n = audio.size();
        std::vector<double> vowel ((size_t) n), noise ((size_t) n);

        fancy::dsp::SvfCoefficients formants[3], lowEdge, highEdge;
        formants[0].set (700.0, 5.0, rate);
        formants[1].set (1200.0, 6.0, rate);
        formants[2].set (2500.0, 8.0, rate);
        lowEdge.set (5000.0, 0.7071, rate);
        highEdge.set (9000.0, 0.7071, rate);

        fancy::dsp::Svf formantFilters[3], lowFilters[2], highFilters[2];
        std::mt19937 random (17);
        std::normal_distribution<double> white (0.0, 1.0);
        auto phase = 0.0;

        for (int i = 0; i < n; ++i)
        {
            phase = std::fmod (phase + 150.0 / rate, 1.0);
            const auto saw = 2.0 * phase - 1.0;

            auto v = 0.0;

            for (int f = 0; f < 3; ++f)
                v += formantFilters[f].tick (formants[f], saw).band * (f == 0 ? 1.0 : 0.5);

            vowel[(size_t) i] = v;

            // Fourth-order band-pass: two high-passes at 5 kHz, two low-passes at 9 kHz.
            auto s = white (random);
            s = lowFilters[1].tick (lowEdge, lowFilters[0].tick (lowEdge, s).high).high;
            s = highFilters[1].tick (highEdge, highFilters[0].tick (highEdge, s).low).low;
            noise[(size_t) i] = s;
        }

        const auto rms = [] (const std::vector<double>& x)
        {
            auto sum = 0.0;
            for (const auto value : x) sum += value * value;
            return std::sqrt (sum / (double) x.size());
        };

        const auto vowelScale = std::pow (10.0, levelDb / 20.0) / rms (vowel);
        const auto burstScale = vowelScale * rms (vowel) / rms (noise) * std::pow (10.0, burstDb / 20.0);
        const auto period = (int) (0.5 * rate), burstLength = (int) (0.1 * rate);

        for (int i = 0; i < n; ++i)
        {
            const auto inBurst = i % period >= period - burstLength;
            const auto x = inBurst ? noise[(size_t) i] * burstScale : vowel[(size_t) i] * vowelScale;
            audio.left[(size_t) i] = audio.right[(size_t) i] = (float) x;
        }

        // Skip the first period, while the detector settles, and 50 ms after each burst for the release.
        for (int start = period; start + period <= n; start += period)
        {
            bursts.push_back ({ start + period - burstLength, start + period });
            vowels.push_back ({ start + (int) (0.05 * rate), start + period - burstLength });
        }
    }
};

Stereo run (DsDsp::Targets targets, const Stereo& input, int blockSize = 256, float* largestReduction = nullptr)
{
    DsDsp dsp;
    dsp.setSynchronousDesign (true);
    dsp.setTargets (targets);
    dsp.prepare (rate);

    auto output = input;
    float largest = 0.0f;

    for (int start = 0; start < output.size(); start += blockSize)
    {
        const auto count = std::min (blockSize, output.size() - start);
        float* channels[] { output.left.data() + start, output.right.data() + start };
        const float* sidechain[] { nullptr, nullptr };
        dsp.setTargets (targets);
        dsp.process (channels, 2, sidechain, 0, count);
        largest = std::max (largest, dsp.takeReduction());
    }

    if (largestReduction != nullptr)
        *largestReduction = largest;

    return output;
}

// Output against input energy over the segments, in dB, the output read `latency` later.
double changeDb (const Stereo& input, const Stereo& output, const std::vector<Segment>& segments, int latency = 0)
{
    auto in = 0.0, out = 0.0;

    for (const auto& segment : segments)
    {
        for (int i = segment.start; i < segment.end && i + latency < output.size(); ++i)
        {
            in += (double) input.left[(size_t) i] * input.left[(size_t) i];
            out += (double) output.left[(size_t) (i + latency)] * output.left[(size_t) (i + latency)];
        }
    }

    return 10.0 * std::log10 (out / in);
}
} // namespace

TEST_CASE ("DS: the default preset cuts the bursts by 6 dB or more and the vowels by under 0.5 dB", "[ds]")
{
    const Voice voice (-18.0);
    const auto output = run ({}, voice.audio);

    const auto bursts = changeDb (voice.audio, output, voice.bursts);
    const auto vowels = changeDb (voice.audio, output, voice.vowels);

    std::printf ("ds default: bursts %.2f dB, vowels %.2f dB\n", bursts, vowels);
    CHECK (bursts <= -6.0);
    CHECK (std::abs (vowels) < 0.5);
}

TEST_CASE ("DS: a 200 Hz + 3 kHz vowel 6 dB over Threshold triggers under 1 dB", "[ds]")
{
    // Absolute threshold, 6 dB under the signal's peak.
    Stereo input ((int) (2.0 * rate));

    for (int i = 0; i < input.size(); ++i)
        input.left[(size_t) i] = input.right[(size_t) i] = (float) (0.25 * std::sin (2.0 * pi * 200.0 * i / rate) + 0.25 * std::sin (2.0 * pi * 3000.0 * i / rate));

    DsDsp::Targets targets;
    targets.relative = false;
    targets.thresholdDb = 20.0 * std::log10 (0.5) - 6.0;

    float largest = 0.0f;
    run (targets, input, 256, &largest);

    INFO ("largest reduction " << largest << " dB");
    CHECK (largest < 1.0f);
}

TEST_CASE ("DS: Relative threshold cuts quiet and loud voices alike", "[ds]")
{
    std::vector<double> cuts;

    for (const auto level : { -30.0, -18.0, -6.0 })
    {
        const Voice voice (level);
        cuts.push_back (changeDb (voice.audio, run ({}, voice.audio), voice.bursts));
    }

    std::printf ("ds relative: bursts at -30, -18, -6 dBFS cut by %.2f, %.2f, %.2f dB\n", cuts[0], cuts[1], cuts[2]);
    CHECK (std::abs (cuts[0] - cuts[1]) <= 1.0);
    CHECK (std::abs (cuts[1] - cuts[2]) <= 1.0);
    CHECK (std::abs (cuts[0] - cuts[2]) <= 1.0);
}

TEST_CASE ("DS: with Range 0 dB the output is the input, in both split filters", "[ds]")
{
    const auto linear = GENERATE (false, true);
    const Voice voice (-12.0, 0.0, 2.0);

    DsDsp::Targets targets;
    targets.rangeDb = 0.0;
    targets.linearPhase = linear;

    DsDsp probe;
    probe.setTargets (targets);
    probe.prepare (rate);
    const auto latency = probe.getLatencySamples();

    const auto output = run (targets, voice.audio, 333);
    auto worst = 0.0;
    auto identical = true;

    for (int i = latency; i < output.size(); ++i)
    {
        const auto expected = voice.audio.left[(size_t) (i - latency)];
        identical = identical && output.left[(size_t) i] == expected;
        worst = std::max (worst, std::abs ((double) output.left[(size_t) i] - (double) expected));
    }

    INFO ((linear ? "linear phase" : "minimum phase") << ", latency " << latency << ", worst " << worst);

    if (linear)
        CHECK (20.0 * std::log10 (worst + 1.0e-30) < -120.0);
    else
        CHECK (identical);
}

TEST_CASE ("DS: the linear-phase split cuts the band as the minimum-phase one does", "[ds]")
{
    const Voice voice (-18.0);

    DsDsp::Targets targets;
    targets.linearPhase = true;

    DsDsp probe;
    probe.setTargets (targets);
    probe.prepare (rate);

    const auto output = run (targets, voice.audio);
    const auto bursts = changeDb (voice.audio, output, voice.bursts, probe.getLatencySamples());

    std::printf ("ds linear phase: bursts %.2f dB\n", bursts);
    CHECK (bursts <= -6.0);
}

TEST_CASE ("DS: with 2 ms of lookahead the first 1 ms of a burst is already cut by 3 dB", "[ds]")
{
    const Voice voice (-18.0);

    DsDsp::Targets targets;
    targets.lookahead = DsDsp::lookahead2;

    DsDsp probe;
    probe.setTargets (targets);
    probe.prepare (rate);
    const auto latency = probe.getLatencySamples();
    CHECK (latency == 96);

    std::vector<Segment> onsets;

    for (const auto& burst : voice.bursts)
        onsets.push_back ({ burst.start, burst.start + (int) (0.001 * rate) });

    const auto cut = changeDb (voice.audio, run (targets, voice.audio), onsets, latency);

    std::printf ("ds lookahead: first 1 ms of each burst %.2f dB\n", cut);
    CHECK (cut <= -3.0);
}

TEST_CASE ("DS: Wide turns the whole signal down; Listen plays what's removed", "[ds]")
{
    const Voice voice (-18.0);

    DsDsp::Targets targets;
    const auto split = run (targets, voice.audio);

    targets.listen = true;
    const auto removed = run (targets, voice.audio);

    // Split plus removed is the input.
    auto worst = 0.0;

    for (int i = 0; i < split.size(); ++i)
        worst = std::max (worst, std::abs ((double) split.left[(size_t) i] + (double) removed.left[(size_t) i] - (double) voice.audio.left[(size_t) i]));

    CHECK (worst < 1.0e-5);

    targets.listen = false;
    targets.wide = true;
    CHECK (changeDb (voice.audio, run (targets, voice.audio), voice.bursts) <= -6.0);
}

// CPU, by the Phase 2.1 §0.4 method --------------------------------------------------------

namespace
{
template <typename Function>
double medianOfFive (Function&& measureOnce)
{
    std::vector<double> runs;

    for (int i = 0; i < 5; ++i)
        runs.push_back (measureOnce());

    std::sort (runs.begin(), runs.end());
    return runs[2];
}

constexpr int measureBlock = 128, measureBlocks = 6000;

double dsCpu (const DsDsp::Targets& targets)
{
    DsDsp dsp;
    dsp.setSynchronousDesign (true);
    dsp.setTargets (targets);
    dsp.prepare (rate);

    float left[measureBlock], right[measureBlock];
    float* channels[] { left, right };
    const float* sidechain[] { nullptr, nullptr };
    std::mt19937 random (8);
    std::uniform_real_distribution<float> noise (-0.25f, 0.25f);
    std::chrono::duration<double> spent {};

    for (int block = 0; block < measureBlocks; ++block)
    {
        for (int i = 0; i < measureBlock; ++i)
        {
            left[i] = noise (random);
            right[i] = noise (random);
        }

        const auto start = std::chrono::steady_clock::now();
        dsp.process (channels, 2, sidechain, 0, measureBlock);
        spent += std::chrono::steady_clock::now() - start;
    }

    return 100.0 * spent.count() / ((double) (measureBlocks * measureBlock) / rate);
}
} // namespace

TEST_CASE ("ds CPU measurement", "[.measure]")
{
    DsDsp::Targets linear;
    linear.linearPhase = true;

    const auto plain = medianOfFive ([] { return dsCpu ({}); });
    const auto phase = medianOfFive ([&] { return dsCpu (linear); });

    std::printf ("ds cpu (median of 5): default (target < 1 %%) %.3f %%, linear phase %.3f %%\n", plain, phase);

   #if FANCY_HAS_COMP
    const auto comp = medianOfFive ([]
    {
        CompDsp dsp;
        dsp.prepare (rate);
        dsp.snapToTargets (CompDsp::Targets {});

        float left[measureBlock], right[measureBlock];
        float* channels[] { left, right };
        std::mt19937 random (8);
        std::uniform_real_distribution<float> noise (-0.25f, 0.25f);
        std::chrono::duration<double> spent {};

        for (int block = 0; block < measureBlocks; ++block)
        {
            for (int i = 0; i < measureBlock; ++i)
            {
                left[i] = noise (random);
                right[i] = noise (random);
            }

            const auto start = std::chrono::steady_clock::now();
            dsp.process (channels, 2, nullptr, 0, measureBlock);
            spent += std::chrono::steady_clock::now() - start;
        }

        return 100.0 * spent.count() / ((double) (measureBlocks * measureBlock) / rate);
    });

    std::printf ("ds cpu reference: Fancy Comp default %.3f %% in the same run; ds default / comp = %.2f, linear phase / comp = %.2f\n",
                 comp, plain / comp, phase / comp);
   #endif

    SUCCEED();
}
