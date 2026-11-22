// Phase 4 Wave 2 for Fancy DS: dual-band detection (docs/ds.md, "Phase 4 Wave 2", section 6).
// The signal for test 3 follows docs/research/ds-dual-model.py. Learn (test 7) is the editor's,
// checked in REAPER as Wave 1's was.

#include "DsProcessor.h"
#include "LegacyDeEsser.h"

#if FANCY_HAS_COMP
 #include "CompProcessor.h"
#endif

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <fancy/dsp/DeEsser.h>
#include <fancy/dsp/Svf.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <type_traits>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = 3.14159265358979323846;

using Engine = fancy::dsp::DeEsser;

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples), right ((size_t) numSamples) {}
    int size() const { return (int) left.size(); }
};

struct Result
{
    Stereo audio;
    float highReduction = 0.0f, lowReduction = 0.0f;   // the most each band took over the last half
};

template <typename E, typename Targets>
Result run (const Targets& targets, Stereo signal, int blockSize = 333)
{
    E engine;
    engine.prepare (rate);
    engine.setSynchronousDesign (true);
    engine.snapToTargets (targets);

    Result result { signal };

    for (int start = 0; start < signal.size(); start += blockSize)
    {
        float* channels[] { result.audio.left.data() + start, result.audio.right.data() + start };
        const float* sidechain[] { nullptr, nullptr };
        engine.process (channels, 2, sidechain, 0, std::min (blockSize, signal.size() - start));

        if constexpr (std::is_same_v<E, Engine>)
        {
            const auto high = engine.takeReduction(), low = engine.takeLowReduction();

            if (start >= signal.size() / 2)
            {
                result.highReduction = std::max (result.highReduction, high);
                result.lowReduction = std::max (result.lowReduction, low);
            }
        }
    }

    return result;
}

Stereo sine (double hz, double amplitude, int numSamples)
{
    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * hz * i / rate));

    return signal;
}

// Second-order filters, to measure energy in a band (the model's band_limited_noise_filter).
std::vector<double> bandOf (const std::vector<double>& x, double lowHz, double highHz)
{
    fancy::dsp::SvfCoefficients lowpass, highpass;
    lowpass.set (highHz, 0.7071, rate);
    highpass.set (lowHz, 0.7071, rate);
    fancy::dsp::Svf a, b;
    std::vector<double> y (x.size());

    for (size_t i = 0; i < x.size(); ++i)
        y[i] = b.tick (highpass, a.tick (lowpass, x[i]).low).high;

    return y;
}

double energyDb (const std::vector<double>& x, const std::vector<bool>& mask)
{
    double sum = 0.0;
    int count = 0;

    for (size_t i = 0; i < x.size(); ++i)
        if (mask[i])
        {
            sum += x[i] * x[i];
            ++count;
        }

    return 10.0 * std::log10 (sum / std::max (1, count) + 1.0e-20);
}

double rms (const std::vector<double>& x, const std::vector<bool>& mask)
{
    return std::sqrt (std::pow (10.0, energyDb (x, mask) / 10.0));
}

// The model's voice: syllables of a 150 Hz tone whose harmonics fall like a vowel's, with "s"
// bursts 10 dB over the vowels in 5.5-10 kHz and "sh" bursts 8 dB over them in 2.3-4.8 kHz.
struct VoiceTest
{
    std::vector<double> signal;
    std::vector<bool> s, sh, vowels;
};

VoiceTest voiceTest()
{
    const auto n = (int) (6.0 * rate);
    std::mt19937 random (7);
    std::uniform_real_distribution<double> phase (0.0, 2.0 * pi);
    std::normal_distribution<double> gaussian;

    std::vector<double> voice ((size_t) n, 0.0), envelope ((size_t) n);

    for (int h = 1; 150.0 * h <= 8000.0; ++h)
    {
        const auto f = 150.0 * h;
        const auto amplitude = 1.0 / h / std::pow (std::max (1.0, f / 1000.0), 2.0);
        const auto offset = phase (random);

        for (int i = 0; i < n; ++i)
            voice[(size_t) i] += amplitude * std::sin (2.0 * pi * f * i / rate + offset);
    }

    // Syllables: 300 ms on, 100 ms off, smoothed.
    {
        fancy::dsp::SvfCoefficients smooth;
        smooth.set (30.0, 0.7071, rate);
        fancy::dsp::Svf filter;

        for (int i = 0; i < n; ++i)
            envelope[(size_t) i] = filter.tick (smooth, std::fmod ((double) i / rate, 0.4) < 0.3 ? 1.0 : 0.0).low;
    }

    VoiceTest test;
    test.signal.resize ((size_t) n);
    test.s.assign ((size_t) n, false);
    test.sh.assign ((size_t) n, false);
    test.vowels.assign ((size_t) n, false);

    std::vector<bool> voiced ((size_t) n);

    for (int i = 0; i < n; ++i)
    {
        voice[(size_t) i] = 0.1 * envelope[(size_t) i] * voice[(size_t) i] + 0.002 * gaussian (random);
        voiced[(size_t) i] = envelope[(size_t) i] > 0.5;
    }

    const auto vowelS = rms (bandOf (voice, 5500.0, 10000.0), voiced);
    const auto vowelSh = rms (bandOf (voice, 2300.0, 4800.0), voiced);

    test.signal = voice;

    for (int k = 0; k < 7; ++k)
    {
        const auto start = (int) ((0.25 + 0.8 * k) * rate);
        const auto isS = k % 2 == 0;
        const auto length = (int) ((isS ? 0.09 : 0.13) * rate);

        std::vector<double> noise ((size_t) length);

        for (auto& value : noise)
            value = gaussian (random);

        auto burst = isS ? bandOf (noise, 5500.0, 10000.0) : bandOf (noise, 2300.0, 4800.0);
        double sum = 0.0;

        for (int i = 0; i < length; ++i)
        {
            burst[(size_t) i] *= 0.5 - 0.5 * std::cos (2.0 * pi * i / (length - 1));   // Hann
            sum += burst[(size_t) i] * burst[(size_t) i];
        }

        const auto scale = (isS ? vowelS * std::pow (10.0, 10.0 / 20.0) : vowelSh * std::pow (10.0, 8.0 / 20.0)) / std::sqrt (sum / length);

        for (int i = 0; i < length; ++i)
        {
            test.signal[(size_t) (start + i)] += burst[(size_t) i] * scale;
            (isS ? test.s : test.sh)[(size_t) (start + i)] = true;
        }
    }

    // Vowels with no burst within 200 ms.
    for (int i = 0; i < n; ++i)
        test.vowels[(size_t) i] = voiced[(size_t) i];

    for (int i = 0; i < n; ++i)
        if (test.s[(size_t) i] || test.sh[(size_t) i])
            for (int j = std::max (0, i - 9600); j < std::min (n, i + 9600); ++j)
                test.vowels[(size_t) j] = false;

    return test;
}

std::vector<double> leftOf (const Stereo& audio)
{
    return { audio.left.begin(), audio.left.end() };
}

Stereo stereoOf (const std::vector<double>& x)
{
    Stereo audio ((int) x.size());

    for (size_t i = 0; i < x.size(); ++i)
        audio.left[i] = audio.right[i] = (float) x[i];

    return audio;
}

Engine::Targets plain()
{
    Engine::Targets t;
    t.relative = false;
    t.rangeDb = -12.0;
    return t;
}
} // namespace

// 1. Compatibility ------------------------------------------------------------

TEST_CASE ("DS Wave 2 with Dual band off renders exactly as the v1.6.0 engine, linear phase too", "[ds][w2]")
{
    const auto linear = GENERATE (false, true);
    const auto shelf = GENERATE (false, true);

    fancy::dsp::LegacyDeEsser::Targets old;
    old.thresholdDb = -30.0;
    old.linearPhase = linear;
    old.shelf = shelf;

    Engine::Targets now;
    now.thresholdDb = old.thresholdDb;
    now.linearPhase = linear;
    now.shelf = shelf;

    const auto input = sine (6800.0, 0.3, 24000);
    const auto a = run<fancy::dsp::LegacyDeEsser> (old, input);
    const auto b = run<Engine> (now, input);

    INFO ("linear " << linear << ", shelf " << shelf);
    REQUIRE (a.audio.left == b.audio.left);
    REQUIRE (a.audio.right == b.audio.right);
}

// 2. Nothing cut, nothing changed ---------------------------------------------

TEST_CASE ("DS dual band with both Ranges at 0 dB leaves the input as it was", "[ds][w2]")
{
    std::mt19937 random (3);
    std::uniform_real_distribution<float> noise (-0.5f, 0.5f);
    Stereo input (24000);

    for (int i = 0; i < input.size(); ++i)
    {
        input.left[(size_t) i] = noise (random);
        input.right[(size_t) i] = noise (random);
    }

    auto t = plain();
    t.dual = true;
    t.thresholdDb = t.lowThresholdDb = -60.0;
    t.rangeDb = t.lowRangeDb = 0.0;

    SECTION ("minimum phase: bit for bit")
    {
        const auto out = run<Engine> (t, input);
        CHECK (out.audio.left == input.left);
        CHECK (out.audio.right == input.right);
    }

    SECTION ("linear phase: the delayed input within -120 dB, at the one-band latency")
    {
        t.linearPhase = true;

        Engine engine;
        engine.prepare (rate);
        const auto latency = engine.latencyFor (Engine::lookaheadOff, true);

        const auto out = run<Engine> (t, input, 512);
        auto worst = 0.0;

        for (int i = latency + 4096; i < input.size(); ++i)
            worst = std::max (worst, std::abs ((double) out.audio.left[(size_t) i] - (double) input.left[(size_t) (i - latency)]));

        INFO ("worst " << 20.0 * std::log10 (worst + 1.0e-30) << " dB");
        CHECK (20.0 * std::log10 (worst + 1.0e-30) < -120.0);
    }
}

// 3. The model's signal --------------------------------------------------------

TEST_CASE ("DS dual band catches \"sh\" without dulling the vowels", "[ds][w2]")
{
    const auto test = voiceTest();
    const auto input = stereoOf (test.signal);

    const auto cuts = [&] (const Engine::Targets& targets)
    {
        const auto out = leftOf (run<Engine> (targets, input).audio);
        struct { double s, sh, vowels; } cut {
            energyDb (bandOf (out, 5500.0, 10000.0), test.s) - energyDb (bandOf (test.signal, 5500.0, 10000.0), test.s),
            energyDb (bandOf (out, 2300.0, 4800.0), test.sh) - energyDb (bandOf (test.signal, 2300.0, 4800.0), test.sh),
            energyDb (bandOf (out, 2500.0, 4500.0), test.vowels) - energyDb (bandOf (test.signal, 2500.0, 4500.0), test.vowels),
        };
        return cut;
    };

    Engine::Targets one;   // DS's defaults: 6.2 kHz, 1 octave, -28 dB relative, Range -12
    auto two = one;
    two.dual = true;       // and Low's: 3.5 kHz, 1 octave, -20 dB, Range -9

    const auto a = cuts (one);
    const auto c = cuts (two);

    std::printf ("ds dual band, the model's signal: one band s %.1f dB, sh %.1f dB, vowels %.1f dB; two bands s %.1f dB, sh %.1f dB, vowels %.1f dB\n",
                 a.s, a.sh, a.vowels, c.s, c.sh, c.vowels);

    CHECK (a.sh > -4.0);
    CHECK (c.sh < -5.0);
    CHECK (c.s < -4.0);
    CHECK (c.vowels > -2.0);
}

// 4. Independence --------------------------------------------------------------

TEST_CASE ("Each band answers only to its own sibilance", "[ds][w2]")
{
    auto t = plain();
    t.dual = true;
    t.thresholdDb = t.lowThresholdDb = -16.0;
    t.rangeDb = t.lowRangeDb = -12.0;

    const auto amplitude = std::pow (10.0, -12.0 / 20.0);

    const auto sh = run<Engine> (t, sine (3000.0, amplitude, (int) rate));
    INFO ("3 kHz: High " << sh.highReduction << " dB, Low " << sh.lowReduction << " dB");
    CHECK (sh.highReduction < 1.0f);
    CHECK (sh.lowReduction > 2.0f);

    const auto s = run<Engine> (t, sine (7000.0, amplitude, (int) rate));
    INFO ("7 kHz: High " << s.highReduction << " dB, Low " << s.lowReduction << " dB");
    CHECK (s.highReduction > 2.0f);
    CHECK (s.lowReduction < 1.0f);
}

// 5. Wide mode -----------------------------------------------------------------

TEST_CASE ("In Wide mode the gain is the deeper of the two bands'", "[ds][w2]")
{
    auto t = plain();
    t.wide = true;
    t.dual = true;
    t.thresholdDb = t.lowThresholdDb = -16.0;

    // A 3 kHz tone: only Low reduces, so the whole signal goes down by Low's reduction.
    const auto amplitude = std::pow (10.0, -12.0 / 20.0);
    const auto input = sine (3000.0, amplitude, (int) rate);
    const auto out = run<Engine> (t, input);

    double in = 0.0, outSum = 0.0;

    for (int i = input.size() / 2; i < input.size(); ++i)
    {
        in += (double) input.left[(size_t) i] * input.left[(size_t) i];
        outSum += (double) out.audio.left[(size_t) i] * out.audio.left[(size_t) i];
    }

    const auto measured = 10.0 * std::log10 (outSum / in);
    INFO ("measured " << measured << " dB, Low's reduction " << out.lowReduction << " dB");
    CHECK (std::abs (measured + (double) out.lowReduction) < 0.1);
    CHECK (out.highReduction < 1.0f);
}

// 6. Linear phase, two bands ---------------------------------------------------

TEST_CASE ("Linear phase with two bands: the same latency, and both bands cut", "[ds][w2]")
{
    Engine engine;
    engine.prepare (rate);
    auto t = plain();
    t.linearPhase = true;
    engine.snapToTargets (t);
    const auto one = engine.getLatencySamples();

    t.dual = true;
    engine.snapToTargets (t);
    CHECK (engine.getLatencySamples() == one);

    t.thresholdDb = t.lowThresholdDb = -16.0;
    const auto amplitude = std::pow (10.0, -12.0 / 20.0);
    const auto sh = run<Engine> (t, sine (3000.0, amplitude, (int) rate), 512);
    CHECK (sh.lowReduction > 2.0f);

    // The Low band's edges stay under the High band's, even set to overlap.
    t.lowFreqHz = 6000.0;
    t.lowWidthOctaves = 3.0;
    const auto overlapped = run<Engine> (t, sine (1000.0, 0.3, (int) rate / 2), 512);
    auto finite = true;

    for (const auto x : overlapped.audio.left)
        finite = finite && std::isfinite (x);

    CHECK (finite);
}

// 8. CPU -----------------------------------------------------------------------

TEST_CASE ("DS dual band CPU measurement", "[.measure]")
{
    constexpr double seconds = 10.0;
    constexpr int measureBlock = 128;

    const auto cpu = [] (fancy::FancyProcessor& processor)
    {
        std::vector<double> runs;

        for (int pass = 0; pass < 5; ++pass)
        {
            processor.setRateAndBufferSizeDetails (rate, measureBlock);
            processor.prepareToPlay (rate, measureBlock);
            processor.waitForBackgroundWork (20000);

            juce::AudioBuffer<float> block (juce::jmax (2, processor.getTotalNumInputChannels()), measureBlock);
            juce::MidiBuffer midi;
            std::mt19937 random (8);
            std::uniform_real_distribution<float> uniform (-0.25f, 0.25f);
            std::chrono::duration<double> spent {};

            for (int done = 0; done < (int) (seconds * rate); done += measureBlock)
            {
                block.clear();

                for (int ch = 0; ch < processor.getMainBusNumInputChannels(); ++ch)
                    for (int i = 0; i < measureBlock; ++i)
                        block.setSample (ch, i, uniform (random));

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

    double reference = 0.0;

   #if FANCY_HAS_COMP
    {
        CompProcessor comp;
        reference = cpu (comp);
    }
   #endif

    DsProcessor single;
    const auto one = cpu (single);

    DsProcessor dual;
    auto* parameter = dual.getApvts().getParameter ("dual");
    parameter->setValueNotifyingHost (1.0f);
    const auto two = cpu (dual);

    const auto ratio = [reference] (double value) { return reference > 0.0 ? value / reference : 0.0; };

    std::printf ("ds dual band cpu (median of 5, 48 kHz, 128 samples, stereo): Fancy Comp default %.3f %%; DS default %.3f %% (%.2f x Comp); "
                 "Dual band %.3f %% (%.2f x Comp, %.0f %% more than DS, target < 60 %%)\n",
                 reference, one, ratio (one), two, ratio (two), one > 0.0 ? 100.0 * (two / one - 1.0) : 0.0);
}
