// Phase 3 Q2.3 for Fancy Delay: modulation, character, diffusion, ducking, swing and the fast
// tanh (docs/delay.md, "Phase 3 Q2.3", section 9).

#include "DelayDsp.h"
#include "DelayProcessor.h"
#include "LegacyDelayDsp.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
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

    template <typename Dsp>
    void process (Dsp& dsp, int blockSize = 256)
    {
        for (int start = 0; start < size(); start += blockSize)
        {
            float* channels[] { left.data() + start, right.data() + start };
            dsp.process (channels, 2, std::min (blockSize, size() - start));
        }
    }
};

Stereo tone (double hz, double amplitude, int numSamples, int start = 0, int end = -1)
{
    Stereo signal (numSamples);

    for (int i = start; i < (end < 0 ? numSamples : end); ++i)
        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * hz * i / rate));

    return signal;
}

double toneDb (const std::vector<float>& x, int from, int count, double hz)
{
    std::complex<double> sum = 0.0;
    auto windowSum = 0.0;

    for (int i = 0; i < count; ++i)
    {
        const auto window = 0.5 - 0.5 * std::cos (2.0 * pi * i / count);
        sum += (double) x[(size_t) (from + i)] * window * std::polar (1.0, -2.0 * pi * hz * i / rate);
        windowSum += window;
    }

    return 20.0 * std::log10 (std::max (2.0 * std::abs (sum) / windowSum, 1.0e-12));
}

double rmsDb (const std::vector<float>& x, int from, int count)
{
    auto sum = 0.0;

    for (int i = from; i < from + count; ++i)
        sum += (double) x[(size_t) i] * x[(size_t) i];

    return 10.0 * std::log10 (std::max (sum / count, 1.0e-30));
}

// One tap in ms mode, wet only.
DelayDsp::Targets oneTap (double ms, double feedback = 0.0)
{
    DelayDsp::Targets targets;
    targets.taps[0].used = true;
    targets.taps[0].noteMode = false;
    targets.taps[0].lengthMs = ms;
    targets.taps[0].feedback = feedback;
    targets.dryGain = 0.0;
    return targets;
}
} // namespace

// 1. Compatibility ------------------------------------------------------------------------

TEST_CASE ("the Q2.3 settings at their defaults render exactly as Delay did before", "[delay][q23]")
{
    const auto variant = GENERATE (0, 1, 2, 3, 4);

    LegacyDelayDsp::Targets old;
    old.taps[0].used = true;
    old.taps[0].feedback = 0.6;
    old.taps[0].lowpassHz = 6000.0;
    old.taps[1].used = true;
    old.taps[1].noteMode = false;
    old.taps[1].lengthMs = 230.0;
    old.taps[1].feedback = 0.4;
    old.taps[1].pan = -0.5;
    old.dryGain = 0.7;

    switch (variant)
    {
        case 1: old.taps[0].saturate = true; old.taps[0].feedback = 1.05; break;
        case 2: old.taps[0].pingPong = true; old.taps[1].bits = 8.0; break;
        case 3: old.smoothTime = false; old.taps[1].filtersInLoop = false; old.taps[1].highpassHz = 300.0; break;
        case 4: old.taps[0].feedback = 1.0; old.taps[1].width = 1.6; break;
        default: break;
    }

    DelayDsp::Targets now;

    for (size_t t = 0; t < now.taps.size(); ++t)
    {
        const auto& o = old.taps[t];
        auto& n = now.taps[t];
        n.used = o.used; n.on = o.on; n.solo = o.solo; n.noteMode = o.noteMode; n.pingPong = o.pingPong;
        n.filtersInLoop = o.filtersInLoop; n.saturate = o.saturate; n.lengthMs = o.lengthMs; n.noteLength = o.noteLength;
        n.feedback = o.feedback; n.lowpassHz = o.lowpassHz; n.highpassHz = o.highpassHz; n.bits = o.bits;
        n.width = o.width; n.volumeGain = o.volumeGain; n.pan = o.pan;
    }

    now.bpm = old.bpm; now.wetGain = old.wetGain; now.dryGain = old.dryGain; now.outputGain = old.outputGain;
    now.smoothTime = old.smoothTime; now.bypass = old.bypass;
    now.fastTanh = false;

    LegacyDelayDsp legacy;
    DelayDsp current;
    legacy.prepare (rate);
    current.prepare (rate);
    legacy.snapToTargets (old);
    current.snapToTargets (now);

    std::mt19937 random (17);
    std::uniform_real_distribution<float> noise (-0.8f, 0.8f);
    Stereo a (96000);

    for (int i = 0; i < 24000; ++i)
        a.left[(size_t) i] = noise (random), a.right[(size_t) i] = noise (random);

    auto b = a;

    for (int start = 0; start < a.size(); start += 300)
    {
        // A time change part way, through both paths.
        if (start == 48000)
        {
            old.taps[1].lengthMs = now.taps[1].lengthMs = 310.0;
            legacy.setTargets (old);
            current.setTargets (now);
        }

        const auto count = std::min (300, a.size() - start);
        float* first[] { a.left.data() + start, a.right.data() + start };
        float* second[] { b.left.data() + start, b.right.data() + start };
        legacy.process (first, 2, count);
        current.process (second, 2, count);
    }

    INFO ("variant " << variant);
    REQUIRE (a.left == b.left);
    REQUIRE (a.right == b.right);
}

TEST_CASE ("a Delay state saved before Q2.3 keeps the exact tanh; a new instance uses the fast one", "[delay][q23]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    DelayProcessor fresh;
    CHECK (fresh.getApvts().getRawParameterValue ("satCurve")->load() == 1.0f);

    juce::MemoryBlock state;
    fresh.getStateInformation (state);

    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    REQUIRE (xml != nullptr);

    if (auto* child = xml->getChildByAttribute ("id", "satCurve"))
        xml->removeChildElement (child, true);

    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary (*xml, oldState);

    DelayProcessor loaded;
    loaded.setStateInformation (oldState.getData(), (int) oldState.getSize());
    CHECK (loaded.getApvts().getRawParameterValue ("satCurve")->load() == 0.0f);
}

// 7. Fast tanh ----------------------------------------------------------------------------

TEST_CASE ("the fast tanh is within 5e-4 and never above 1", "[delay][q23]")
{
    auto worst = 0.0;

    for (auto x = -20.0; x <= 20.0; x += 0.0005)
    {
        const auto y = DelayDsp::fastTanh (x);
        REQUIRE (std::abs (y) <= 1.0);
        worst = std::max (worst, std::abs (y - std::tanh (x)));
    }

    INFO ("worst error " << worst);
    CHECK (worst < 5.0e-4);
}

// 2. Modulation ---------------------------------------------------------------------------

TEST_CASE ("modulation depth is the peak pitch change", "[delay][q23]")
{
    const auto modRate = GENERATE (0.5, 2.0, 5.0);

    auto targets = oneTap (150.0);
    targets.taps[0].modDepthCents = 20.0;
    targets.taps[0].modRateHz = modRate;
    targets.taps[0].modRandom = 0.0;

    DelayDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    const auto length = (int) (4.0 * rate);
    auto signal = tone (1000.0, 0.5, length);
    signal.process (dsp);

    // The frequency of each cycle, from interpolated upward zero crossings.
    std::vector<double> crossings;
    const auto& x = signal.left;

    for (int i = (int) (0.5 * rate); i < length; ++i)
        if (x[(size_t) i - 1] < 0.0f && x[(size_t) i] >= 0.0f)
            crossings.push_back ((double) (i - 1) + (double) x[(size_t) i - 1] / ((double) x[(size_t) i - 1] - x[(size_t) i]));

    auto highest = 0.0, lowest = 1.0e9;

    for (size_t c = 1; c < crossings.size(); ++c)
    {
        const auto hz = rate / (crossings[c] - crossings[c - 1]);
        highest = std::max (highest, hz);
        lowest = std::min (lowest, hz);
    }

    const auto upCents = 1200.0 * std::log2 (highest / 1000.0);
    const auto downCents = -1200.0 * std::log2 (lowest / 1000.0);

    INFO ("rate " << modRate << " Hz: +" << upCents << " / -" << downCents << " cents");
    CHECK (upCents == Catch::Approx (20.0).epsilon (0.1));
    CHECK (downCents == Catch::Approx (20.0).epsilon (0.1));
}

// 3. Character ----------------------------------------------------------------------------

TEST_CASE ("Tape saturates and darkens each repeat; BBD band-limits quietly", "[delay][q23]")
{
    SECTION ("Tape adds a 3rd harmonic")
    {
        auto targets = oneTap (50.0);
        targets.taps[0].character = DelayDsp::tape;
        targets.taps[0].characterAmount = 1.0;

        DelayDsp dsp;
        dsp.prepare (rate);
        dsp.snapToTargets (targets);

        auto signal = tone (1000.0, 0.5, 24000);
        signal.process (dsp);

        const auto fundamental = toneDb (signal.left, 4800, 19200, 1000.0);
        const auto third = toneDb (signal.left, 4800, 19200, 3000.0);
        INFO ("fundamental " << fundamental << " dB, 3rd " << third << " dB");
        CHECK (third > -40.0);
    }

    SECTION ("each Tape repeat has less top end than the one before")
    {
        auto targets = oneTap (100.0, 0.6);
        targets.taps[0].character = DelayDsp::tape;
        targets.taps[0].characterAmount = 0.8;

        DelayDsp dsp;
        dsp.prepare (rate);
        dsp.snapToTargets (targets);

        std::mt19937 random (3);
        std::uniform_real_distribution<float> noise (-0.5f, 0.5f);
        Stereo signal (24000);

        for (int i = 0; i < 480; ++i)
            signal.left[(size_t) i] = signal.right[(size_t) i] = noise (random);

        signal.process (dsp);

        // Top-end share of each repeat: the first difference's energy against the signal's.
        const auto brightness = [&signal] (int repeat)
        {
            const auto start = repeat * 4800;
            auto diff = 0.0, total = 0.0;

            for (int i = start + 1; i < start + 960; ++i)
            {
                const auto d = (double) signal.left[(size_t) i] - signal.left[(size_t) i - 1];
                diff += d * d;
                total += (double) signal.left[(size_t) i] * signal.left[(size_t) i];
            }

            return diff / std::max (total, 1.0e-30);
        };

        CHECK (brightness (2) < brightness (1));
        CHECK (brightness (3) < brightness (2));
    }

    SECTION ("BBD at 500 ms cuts the top and keeps its noise down")
    {
        auto targets = oneTap (500.0);
        targets.taps[0].character = DelayDsp::bbd;
        targets.taps[0].characterAmount = 1.0;

        DelayDsp dsp;
        dsp.prepare (rate);
        dsp.snapToTargets (targets);

        auto high = tone (10000.0, 0.5, 72000);
        high.process (dsp);
        const auto passed = toneDb (high.left, 36000, 36000, 10000.0) - 20.0 * std::log10 (0.5);

        DelayDsp quiet;
        quiet.prepare (rate);
        quiet.snapToTargets (targets);

        Stereo silence (72000);
        silence.process (quiet);

        INFO ("10 kHz through BBD: " << passed << " dB; noise " << rmsDb (silence.left, 36000, 36000) << " dBFS");
        CHECK (passed < -30.0);
        CHECK (rmsDb (silence.left, 36000, 36000) < -75.0);
    }
}

TEST_CASE ("switching Character under signal is click-free", "[delay][q23]")
{
    auto targets = oneTap (20.0);
    targets.dryGain = 1.0;

    DelayDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    auto signal = tone (200.0, 0.25, 48000);

    for (int start = 0; start < signal.size(); start += 64)
    {
        if (start == 9600)  { targets.taps[0].character = DelayDsp::tape; dsp.setTargets (targets); }
        if (start == 24000) { targets.taps[0].character = DelayDsp::bbd; dsp.setTargets (targets); }
        if (start == 38400) { targets.taps[0].character = DelayDsp::clean; dsp.setTargets (targets); }

        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, 64);
    }

    auto worst = 0.0;

    for (size_t i = 2400; i < signal.left.size(); ++i)
        worst = std::max (worst, std::abs ((double) signal.left[i] - 2.0 * signal.left[i - 1] + signal.left[i - 2]));

    INFO ("worst second difference " << worst);
    CHECK (worst < 2.0e-3);
}

// 4. Diffusion ----------------------------------------------------------------------------

TEST_CASE ("diffusion smears repeats into a wash", "[delay][q23]")
{
    auto targets = oneTap (200.0, 0.7);
    targets.taps[0].diffusion = 0.7;

    DelayDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    Stereo signal (48000);
    signal.left[0] = signal.right[0] = 1.0f;
    signal.process (dsp);

    // The second repeat, after one trip through the diffusers.
    const auto start = (int) (0.4 * rate) - 100;
    auto peak = 0.0;

    for (int i = start; i < start + 2400; ++i)
        peak = std::max (peak, (double) std::abs (signal.left[(size_t) i]));

    auto peaks = 0;

    for (int i = start + 1; i < start + 2400 - 1; ++i)
    {
        const auto v = std::abs ((double) signal.left[(size_t) i]);

        if (v > 0.1 * peak && v >= std::abs ((double) signal.left[(size_t) i - 1]) && v > std::abs ((double) signal.left[(size_t) i + 1]))
            ++peaks;
    }

    INFO ("peaks in 50 ms: " << peaks);
    CHECK (peaks > 20);
}

// 5. Ducking ------------------------------------------------------------------------------

TEST_CASE ("ducking turns the wet signal down while the input is loud", "[delay][q23]")
{
    const auto wetDb = [] (double amountDb)
    {
        auto targets = oneTap (50.0);
        targets.duckAmountDb = amountDb;
        targets.duckThresholdDb = -24.0;

        DelayDsp dsp;
        dsp.prepare (rate);
        dsp.snapToTargets (targets);

        auto signal = tone (500.0, 0.5, 48000);
        signal.process (dsp);
        return rmsDb (signal.left, 24000, 24000);
    };

    const auto plain = wetDb (0.0), ducked = wetDb (12.0);
    INFO ("wet " << plain << " dB, ducked " << ducked << " dB");
    CHECK (plain - ducked == Catch::Approx (12.0).margin (1.0));
}

// 6. Swing --------------------------------------------------------------------------------

TEST_CASE ("swing moves note taps on an odd eighth, and only them", "[delay][q23]")
{
    DelayDsp::TapSpec eighth;
    eighth.noteMode = true;
    eighth.noteLength = { 3, delay::straight, 1 };

    auto quarter = eighth;
    quarter.noteLength = { 2, delay::straight, 1 };

    auto dottedEighth = eighth;
    dottedEighth.noteLength = { 3, delay::dotted, 1 };

    auto threeEighths = eighth;
    threeEighths.noteLength = { 3, delay::straight, 3 };

    CHECK (DelayDsp::tapMs (eighth, 120.0, 0.6) == Catch::Approx (300.0));
    CHECK (DelayDsp::tapMs (quarter, 120.0, 0.6) == Catch::Approx (500.0));
    CHECK (DelayDsp::tapMs (dottedEighth, 120.0, 0.6) == Catch::Approx (375.0));
    CHECK (DelayDsp::tapMs (threeEighths, 120.0, 0.6) == Catch::Approx (800.0));
    CHECK (DelayDsp::tapMs (eighth, 120.0, 0.5) == Catch::Approx (250.0));

    DelayDsp::Targets targets;
    targets.taps[0] = eighth;
    targets.taps[0].used = true;
    targets.swing = 0.6;

    DelayDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);
    CHECK (dsp.getDelaySamples (0) == 14400);
}

// 8. CPU (section 0.4) ----------------------------------------------------------------------

TEST_CASE ("delay Q2.3 CPU measurement", "[.measure]")
{
    constexpr int blockSize = 128;
    constexpr int numBlocks = 7500; // 20 s
    constexpr int runs = 5;

    const auto timeOf = [] (const DelayDsp::Targets& targets)
    {
        std::vector<double> results;

        for (int run = 0; run < runs; ++run)
        {
            DelayDsp dsp;
            dsp.prepare (rate);
            dsp.snapToTargets (targets);

            std::mt19937 random (9);
            std::uniform_real_distribution<float> noise (-0.5f, 0.5f);
            std::vector<float> left ((size_t) blockSize * 64), right ((size_t) blockSize * 64);

            for (size_t i = 0; i < left.size(); ++i)
                left[i] = noise (random), right[i] = noise (random);

            const auto start = std::chrono::steady_clock::now();

            for (int i = 0; i < numBlocks; ++i)
            {
                const auto offset = (size_t) ((i % 64) * blockSize);
                std::vector<float> l (left.begin() + (std::ptrdiff_t) offset, left.begin() + (std::ptrdiff_t) offset + blockSize);
                std::vector<float> r (right.begin() + (std::ptrdiff_t) offset, right.begin() + (std::ptrdiff_t) offset + blockSize);
                float* channels[] { l.data(), r.data() };
                dsp.process (channels, 2, blockSize);
            }

            const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
            results.push_back (100.0 * elapsed.count() / ((double) (numBlocks * blockSize) / rate));
        }

        std::sort (results.begin(), results.end());
        return results[runs / 2];
    };

    DelayDsp::Targets plain;

    for (int t = 0; t < 4; ++t)
    {
        plain.taps[(size_t) t].used = true;
        plain.taps[(size_t) t].noteMode = false;
        plain.taps[(size_t) t].lengthMs = 120.0 + 90.0 * t;
        plain.taps[(size_t) t].feedback = 0.5;
    }

    plain.fastTanh = true;

    auto everything = plain;
    everything.duckAmountDb = 12.0;

    for (int t = 0; t < 4; ++t)
    {
        auto& tap = everything.taps[(size_t) t];
        tap.saturate = true;
        tap.lowpassHz = 6000.0;
        tap.highpassHz = 120.0;
        tap.bits = 12.0;
        tap.pingPong = t % 2 == 0;
        tap.character = t % 2 == 0 ? DelayDsp::tape : DelayDsp::bbd;
        tap.modDepthCents = 15.0;
        tap.diffusion = 0.5;
    }

    const auto plainPercent = timeOf (plain);
    const auto everythingPercent = timeOf (everything);

    std::printf ("delay q23 cpu, 4 taps, 48 kHz, 128 samples, median of %d: plain %.3f %%, everything on (saturate, filters, bits, ping-pong, Tape/BBD, modulation, diffusion, ducking) %.3f %% (target < 2 %%)\n",
                 runs, plainPercent, everythingPercent);
}
