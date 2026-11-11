// Fancy Sature (docs/sature.md): the §4.4 tests of docs/BUILD_PROMPT_PHASE3_NEW_PLUGINS.md, with
// Decision 7's aliasing limits, and the plugin's own checks.

#include "SatureDsp.h"
#include "SatureEditor.h"
#include "SatureProcessor.h"

#if FANCY_HAS_COMP
 #include "CompDsp.h"
#endif

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <fancy/dsp/LoudnessMeter.h>
#include <fancy/dsp/Saturation.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <numbers>
#include <random>
#include <vector>

using Catch::Approx;
namespace sat = fancy::dsp::saturation;

namespace
{
constexpr double pi = std::numbers::pi;
const char* const typeNames[] { "Tape", "Tube", "Transistor", "Clip" };
const char* const factorNames[] { "Off", "2x", "4x", "8x" };

double toDb (double gain) { return 20.0 * std::log10 (std::max (gain, 1.0e-15)); }

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples, 0.0f), right ((size_t) numSamples, 0.0f) {}
    int size() const { return (int) left.size(); }
};

Stereo sine (double hz, double amplitude, double seconds, double rate)
{
    Stereo s ((int) (seconds * rate));

    for (int i = 0; i < s.size(); ++i)
        s.left[(size_t) i] = s.right[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * hz * i / rate));

    return s;
}

// Pink noise at -18 dBFS RMS, as the auto-gain table is measured on (another seed).
Stereo pinkNoise (double seconds, double rate, unsigned seed)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<double> uniform (-1.0, 1.0);
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;

    const auto warmUp = (int) (0.5 * rate);
    Stereo s ((int) (seconds * rate));
    std::vector<double> values;

    for (int i = 0; i < s.size() + warmUp; ++i)
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

        if (i >= warmUp)
            values.push_back (value);
    }

    auto mean = 0.0, power = 0.0;
    for (auto v : values) mean += v;
    mean /= (double) values.size();
    for (auto& v : values) { v -= mean; power += v * v; }

    const auto scale = sat::tableNoiseRms / std::sqrt (power / (double) values.size());

    for (int i = 0; i < s.size(); ++i)
        s.left[(size_t) i] = s.right[(size_t) i] = (float) (values[(size_t) i] * scale);

    return s;
}

SatureDsp::Targets plain()
{
    SatureDsp::Targets targets;
    return targets;
}

// Runs the DSP over a copy, in blocks of the given size, or random sizes from 1 to 2048 if 0.
Stereo run (const SatureDsp::Targets& targets, const Stereo& input, double rate, int blockSize = 256, unsigned seed = 11)
{
    SatureDsp dsp;
    dsp.setTargets (targets);
    dsp.prepare (rate);

    auto output = input;
    std::mt19937 random (seed);
    std::uniform_int_distribution<int> sizes (1, 2048);

    for (int start = 0; start < output.size();)
    {
        const auto count = std::min (blockSize > 0 ? blockSize : sizes (random), output.size() - start);
        float* channels[] { output.left.data() + start, output.right.data() + start };
        dsp.setTargets (targets);
        dsp.process (channels, 2, count);
        start += count;
    }

    return output;
}

// Level of one frequency in dBFS, Hann windowed over the samples from `from`.
double levelAt (const std::vector<float>& samples, int from, double hz, double rate)
{
    const auto count = (int) samples.size() - from;
    std::complex<double> sum = 0.0;
    double windowSum = 0.0;

    for (int i = 0; i < count; ++i)
    {
        const auto window = 0.5 - 0.5 * std::cos (2.0 * pi * i / count);
        const auto phase = 2.0 * pi * hz * i / rate;
        sum += (double) samples[(size_t) (from + i)] * window * std::complex<double> (std::cos (phase), -std::sin (phase));
        windowSum += window;
    }

    return toDb (2.0 * std::abs (sum) / windowSum);
}

double loudness (const Stereo& s, int from = 0)
{
    fancy::dsp::LoudnessMeter meter;
    meter.prepare (48000.0);
    meter.process (s.left.data() + from, s.right.data() + from, s.size() - from);
    return meter.getIntegrated();
}

// The worst component below Nyquist that isn't a harmonic of the tone, in dBFS: a 65536-point
// Blackman-Harris FFT after a second of settling, with 6 Hz either side of each harmonic left out.
double worstAliasDb (int factor, double toneHz, double rate = 44100.0)
{
    auto targets = plain();
    targets.type = sat::clip;
    targets.driveDb = 24.0;
    targets.character = 0.0;
    targets.autoGain = false; // the model's figures are for the bare curve
    targets.oversampling = factor;

    constexpr int order = 16, size = 1 << order;
    const auto settle = (int) rate;
    const auto output = run (targets, sine (toneHz, 0.5, (double) (settle + size) / rate + 0.01, rate), rate, 512);

    std::vector<float> data ((size_t) size * 2, 0.0f);
    auto windowSum = 0.0;

    for (int i = 0; i < size; ++i)
    {
        const auto phase = 2.0 * pi * i / (size - 1);
        const auto window = 0.35875 - 0.48829 * std::cos (phase) + 0.14128 * std::cos (2.0 * phase) - 0.01168 * std::cos (3.0 * phase);
        data[(size_t) i] = (float) (output.left[(size_t) (settle + i)] * window);
        windowSum += window;
    }

    juce::dsp::FFT fft (order);
    fft.performFrequencyOnlyForwardTransform (data.data());

    const auto binHz = rate / size;
    auto worst = 0.0;

    for (int bin = 2; bin < size / 2; ++bin)
    {
        const auto hz = bin * binHz;
        const auto nearest = std::round (hz / toneHz) * toneHz;

        if (nearest > 0.0 && std::abs (hz - nearest) < 6.0)
            continue;

        worst = std::max (worst, 2.0 * (double) data[(size_t) bin] / windowSum);
    }

    return toDb (worst);
}
} // namespace

// Curves ------------------------------------------------------------------------------------

TEST_CASE ("Sature: every curve's antiderivative matches it, and every curve has slope 1 at zero", "[sature]")
{
    for (int type = 0; type < sat::numTypes; ++type)
    {
        for (const auto character : { -1.0, -0.3, 0.0, 0.5, 1.0 })
        {
            INFO (typeNames[type] << ", character " << character);
            const auto curve = sat::makeCurve (type, character);

            for (auto u = -4.0; u <= 4.0; u += 0.0137)
            {
                const auto step = 1.0e-5;
                const auto slope = (curve.integral (u + step) - curve.integral (u - step)) / (2.0 * step);
                CHECK (slope == Approx (curve.shape (u)).margin (1.0e-6));
            }

            CHECK (curve.shape (0.0) == 0.0);
            CHECK ((curve.shape (1.0e-6) - curve.shape (-1.0e-6)) / 2.0e-6 == Approx (1.0).margin (1.0e-6));
            CHECK (std::abs (curve.shape (100.0)) <= sat::tubeNegativeLimit);
        }
    }
}

TEST_CASE ("Sature: Character's read-out and typing", "[sature]")
{
    CHECK (SatureProcessor::characterText (-30.0f) == "even 30 %");
    CHECK (SatureProcessor::characterText (0.0f) == "neutral");
    CHECK (SatureProcessor::characterText (55.0f) == "odd 55 %");
    CHECK (SatureProcessor::parseCharacter ("even 30 %") == -30.0f);
    CHECK (SatureProcessor::parseCharacter ("odd 50") == 50.0f);
    CHECK (SatureProcessor::parseCharacter ("neutral") == 0.0f);
    CHECK (SatureProcessor::parseCharacter ("-20") == -20.0f);
}

TEST_CASE ("Sature: Low and High have Off at their ends, and High keeps an octave above Low", "[sature]")
{
    SatureProcessor processor;
    auto& low = *processor.getApvts().getParameter ("low");
    auto& high = *processor.getApvts().getParameter ("high");

    CHECK (low.getCurrentValueAsText() == "Off");
    CHECK (high.getCurrentValueAsText() == "Off");
    CHECK_FALSE (processor.readTargets().lowOn);
    CHECK_FALSE (processor.readTargets().highOn);

    low.setValueNotifyingHost (low.convertTo0to1 (1500.0f));
    high.setValueNotifyingHost (high.convertTo0to1 (1000.0f));

    const auto targets = processor.readTargets();
    CHECK (targets.lowOn);
    CHECK (targets.highOn);
    CHECK (SatureDsp::effectiveHighHz (targets) == Approx (3000.0));

    CHECK (low.getValueForText ("off") == Approx (low.convertTo0to1 (SatureProcessor::lowOffHz)));
    CHECK (high.getValueForText ("2.5k") == Approx (high.convertTo0to1 (2500.0f)));
}

// §4.4 ---------------------------------------------------------------------------------------

TEST_CASE ("Sature: aliasing, 10 kHz at -6 dBFS, Drive 24, Clip, 44.1 kHz (Decision 7)", "[sature][aliasing]")
{
    // The model in docs/research/sature-alias-model.py: -55.9 dBFS at 4x, -69.3 at 8x.
    const auto at4 = worstAliasDb (SatureDsp::x4, 10000.0);
    const auto at8 = worstAliasDb (SatureDsp::x8, 10000.0);
    INFO ("4x " << at4 << " dBFS, 8x " << at8 << " dBFS");
    CHECK (at4 < -50.0);
    CHECK (at8 < -65.0);
}

TEST_CASE ("Sature: aliasing, 1 kHz at -6 dBFS, Drive 24, Clip, 44.1 kHz (Decision 7)", "[sature][aliasing]")
{
    // The model: -94.4 dBFS at 4x; -42.3 with Oversampling Off, which runs without ADAA.
    const auto at4 = worstAliasDb (SatureDsp::x4, 1000.0);
    const auto off = worstAliasDb (SatureDsp::off, 1000.0);
    INFO ("4x " << at4 << " dBFS, Off " << off << " dBFS");
    CHECK (at4 < -90.0);
    CHECK (off < -38.0);
}

TEST_CASE ("Sature: Auto gain keeps pink noise within 1 LU, every Type, Drive 0 to 36 dB", "[sature][autogain]")
{
    constexpr double rate = 48000.0;
    const auto noise = pinkNoise (4.0, rate, 77);
    const auto inputLufs = loudness (noise, (int) rate);

    for (int type = 0; type < sat::numTypes; ++type)
    {
        for (const auto drive : { 0.0, 12.0, 24.0, 36.0 })
        {
            auto targets = plain();
            targets.type = type;
            targets.driveDb = drive;

            const auto change = loudness (run (targets, noise, rate), (int) rate) - inputLufs;
            INFO (typeNames[type] << ", Drive " << drive << " dB: " << change << " LU");
            CHECK (std::abs (change) <= 1.0);
        }
    }
}

TEST_CASE ("Sature: with Low at 200 Hz, a 60 Hz sine gains no harmonics above -90 dBFS", "[sature][band]")
{
    constexpr double rate = 48000.0;
    const auto input = sine (60.0, 0.5, 3.0, rate);

    for (int type = 0; type < sat::numTypes; ++type)
    {
        for (const auto drive : { 6.0, 18.0, 36.0 })
        {
            auto targets = plain();
            targets.type = type;
            targets.driveDb = drive;
            targets.lowOn = true;
            targets.lowHz = 200.0;

            const auto output = run (targets, input, rate);
            auto worst = -400.0;

            for (int h = 2; h <= 20; ++h)
                worst = std::max (worst, levelAt (output.left, (int) rate, 60.0 * h, rate));

            INFO (typeNames[type] << ", Drive " << drive << " dB: worst harmonic " << worst << " dBFS");
            CHECK (worst < -90.0);
        }
    }
}

TEST_CASE ("Sature: Character at either end and Drive 36 leave no DC", "[sature][dc]")
{
    constexpr double rate = 48000.0;
    const auto input = sine (100.0, 0.5, 4.0, rate);

    for (int type = 0; type < sat::numTypes; ++type)
    {
        for (const auto character : { -1.0, 1.0 })
        {
            auto targets = plain();
            targets.type = type;
            targets.driveDb = 36.0;
            targets.character = character;

            const auto output = run (targets, input, rate);

            // The last second: 100 whole periods.
            auto sum = 0.0;
            for (int i = output.size() - (int) rate; i < output.size(); ++i)
                sum += (double) output.left[(size_t) i];

            const auto dc = toDb (std::abs (sum / rate));
            INFO (typeNames[type] << ", character " << character << ": DC " << dc << " dBFS");
            CHECK (dc < -100.0);
        }
    }
}

TEST_CASE ("Sature: Wet 0 dB and Dry 0 dB at Drive 0 sum flat within 0.1 dB, 20 Hz to 20 kHz", "[sature][flat]")
{
    constexpr double rate = 48000.0;
    const auto factor = GENERATE (0, 1, 2, 3);
    const auto minimumPhase = GENERATE (true, false);
    INFO (factorNames[factor] << (minimumPhase ? ", minimum phase" : ", linear phase"));

    auto targets = plain();
    targets.driveDb = 0.0;
    targets.wetGain = 1.0;
    targets.dryGain = 1.0;
    targets.oversampling = factor;
    targets.minimumPhase = minimumPhase;

    auto lowest = 1.0e9, highest = -1.0e9;

    for (int step = 0; step <= 30; ++step)
    {
        const auto hz = 20.0 * std::pow (1000.0, step / 30.0);
        const auto input = sine (hz, 0.01, 0.6, rate);
        const auto output = run (targets, input, rate);
        const auto gain = levelAt (output.left, (int) (0.3 * rate), hz, rate) - levelAt (input.left, (int) (0.3 * rate), hz, rate);

        lowest = std::min (lowest, gain);
        highest = std::max (highest, gain);
    }

    INFO ("from " << lowest << " to " << highest << " dB");
    CHECK (highest - lowest <= 0.1);
}

TEST_CASE ("Sature: the reported latency is where the dry signal lands", "[sature][latency]")
{
    constexpr double rate = 48000.0;
    const auto factor = GENERATE (0, 1, 2, 3);
    const auto minimumPhase = GENERATE (true, false);
    INFO (factorNames[factor] << (minimumPhase ? ", minimum phase" : ", linear phase"));

    auto targets = plain();
    targets.wetGain = 0.0;
    targets.dryGain = 1.0;
    targets.oversampling = factor;
    targets.minimumPhase = minimumPhase;

    SatureDsp dsp;
    dsp.setTargets (targets);
    dsp.prepare (rate);

    Stereo impulse (4096);
    impulse.left[100] = impulse.right[100] = 0.5f;
    float* channels[] { impulse.left.data(), impulse.right.data() };
    dsp.process (channels, 2, impulse.size());

    const auto peak = (int) (std::max_element (impulse.left.begin(), impulse.left.end(),
                                               [] (float a, float b) { return std::abs (a) < std::abs (b); })
                             - impulse.left.begin()) - 100;
    const auto reported = dsp.getLatencySamples();
    INFO ("reported " << reported << ", peak at " << peak);

    if (minimumPhase && factor != 0)
        CHECK ((peak >= reported - 1 && peak <= reported + 3));
    else
        CHECK (std::abs (peak - reported) <= 1);
}

TEST_CASE ("Sature: Listen plays what's added, so a quiet tone at Drive 0 nearly vanishes", "[sature][listen]")
{
    constexpr double rate = 48000.0;
    const auto input = sine (1000.0, 0.01, 1.0, rate);

    auto targets = plain();
    targets.driveDb = 0.0;
    targets.character = 0.0;
    targets.listen = true;

    const auto quiet = levelAt (run (targets, input, rate).left, (int) (0.5 * rate), 1000.0, rate);
    INFO ("-40 dBFS in, " << quiet << " dBFS out");
    // Auto gain's small correction at Drive 0 is all that's left: at least 25 dB down.
    CHECK (quiet < -65.0);

    targets.driveDb = 30.0;
    const auto loud = run (targets, sine (1000.0, 0.5, 1.0, rate), rate);
    CHECK (levelAt (loud.left, (int) (0.5 * rate), 3000.0, rate) > -40.0);
}

TEST_CASE ("Sature: random block sizes give the same output, bit for bit", "[sature][blocks]")
{
    constexpr double rate = 48000.0;
    const auto factor = GENERATE (0, 2);

    auto targets = plain();
    targets.type = sat::tube;
    targets.driveDb = 20.0;
    targets.toneDb = 3.0;
    targets.lowOn = true;
    targets.lowHz = 150.0;
    targets.highOn = true;
    targets.highHz = 6000.0;
    targets.dryGain = 0.5;
    targets.oversampling = factor;

    const auto input = pinkNoise (1.0, rate, 3);
    const auto fixed = run (targets, input, rate, 512);
    const auto random = run (targets, input, rate, 0, 23);

    CHECK (fixed.left == random.left);
    CHECK (fixed.right == random.right);
}

TEST_CASE ("Sature: the harmonics card's analysis reads a known curve", "[sature]")
{
    SatureDsp dsp;
    dsp.prepare (48000.0);

    auto targets = plain();
    targets.type = sat::tape;
    targets.driveDb = 14.0;
    targets.character = -0.3;

    const auto harmonics = SatureEditor::analyse (dsp, targets);
    CHECK (harmonics.levelsDb[0] == Approx (0.0f));
    CHECK (harmonics.levelsDb[1] > -60.0f); // even harmonics from the bias
    CHECK (harmonics.levelsDb[2] > -40.0f); // odd ones from the curve
    CHECK (harmonics.thdPercent > 1.0f);

    targets.character = 0.0;
    targets.driveDb = 0.0;
    const auto clean = SatureEditor::analyse (dsp, targets);
    CHECK (clean.thdPercent < 2.0f);

    targets.lowOn = true;
    targets.lowHz = 1500.0;
    CHECK (SatureEditor::analyse (dsp, targets).outsideBand);
}

// Measurements -------------------------------------------------------------------------------

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

double satureCpu (const SatureDsp::Targets& targets)
{
    constexpr double rate = 48000.0;

    SatureDsp dsp;
    dsp.setTargets (targets);
    dsp.prepare (rate);

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
        dsp.process (channels, 2, measureBlock);
        spent += std::chrono::steady_clock::now() - start;
    }

    return 100.0 * spent.count() / ((double) (measureBlocks * measureBlock) / rate);
}
} // namespace

TEST_CASE ("sature measurements", "[.measure]")
{
    std::printf ("sature aliasing (Clip, Drive 24, -6 dBFS, 44.1 kHz, bare curve), worst alias in dBFS:\n");

    for (int factor = 0; factor < 4; ++factor)
        std::printf ("sature   %-3s: 10 kHz %7.1f, 1 kHz %7.1f\n", factorNames[factor], worstAliasDb (factor, 10000.0), worstAliasDb (factor, 1000.0));

    SatureDsp dsp;
    dsp.prepare (48000.0);

    for (int factor = 0; factor < 4; ++factor)
        std::printf ("sature latency %-3s: minimum phase %d, linear phase %d samples at 48 kHz\n", factorNames[factor],
                     dsp.latencyFor (factor, true), dsp.latencyFor (factor, false));

    const auto noise = pinkNoise (4.0, 48000.0, 77);
    const auto inputLufs = loudness (noise, 48000);

    for (int type = 0; type < sat::numTypes; ++type)
    {
        std::printf ("sature auto gain %-10s:", typeNames[type]);

        for (const auto drive : { 0.0, 12.0, 24.0, 36.0 })
        {
            auto targets = plain();
            targets.type = type;
            targets.driveDb = drive;
            std::printf (" Drive %2.0f %+5.2f LU", drive, loudness (run (targets, noise, 48000.0), 48000) - inputLufs);
        }

        std::printf ("\n");
    }

    auto defaults = plain();
    auto linear = plain();
    linear.minimumPhase = false;
    auto off = plain();
    off.oversampling = SatureDsp::off;
    auto eight = plain();
    eight.oversampling = SatureDsp::x8;
    auto banded = plain();
    banded.lowOn = banded.highOn = true;
    banded.lowHz = 150.0;
    banded.highHz = 6000.0;

    const auto cpu = medianOfFive ([&] { return satureCpu (defaults); });
    const auto cpuLinear = medianOfFive ([&] { return satureCpu (linear); });
    const auto cpuOff = medianOfFive ([&] { return satureCpu (off); });
    const auto cpuEight = medianOfFive ([&] { return satureCpu (eight); });
    const auto cpuBanded = medianOfFive ([&] { return satureCpu (banded); });

    std::printf ("sature cpu (median of 5): defaults, 4x minimum phase (target < 2 %%) %.3f %%, 4x linear %.3f %%, off %.3f %%, 8x %.3f %%, 4x with Low and High %.3f %%\n",
                 cpu, cpuLinear, cpuOff, cpuEight, cpuBanded);

   #if FANCY_HAS_COMP
    const auto comp = medianOfFive ([]
    {
        CompDsp compDsp;
        compDsp.prepare (48000.0);
        compDsp.snapToTargets (CompDsp::Targets {});

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
            compDsp.process (channels, 2, nullptr, 0, measureBlock);
            spent += std::chrono::steady_clock::now() - start;
        }

        return 100.0 * spent.count() / ((double) (measureBlocks * measureBlock) / 48000.0);
    });

    std::printf ("sature cpu reference: Fancy Comp default %.3f %% in the same run; sature 4x / comp = %.2f, linear / comp = %.2f, off / comp = %.2f, 8x / comp = %.2f\n",
                 comp, cpu / comp, cpuLinear / comp, cpuOff / comp, cpuEight / comp);
   #endif

    SUCCEED();
}
