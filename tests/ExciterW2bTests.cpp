// Phase 4 Wave 2 for Fancy Exciter: Band 2, Punch, the High cut and Width (docs/exciter.md,
// "Phase 4 Wave 2", sections 7, 9 and 10). Tests 2 and 3 run at Oversampling Off, where
// docs/research/exciter-w2b-prepush.py models the path exactly; run it for the expected figures
// (test 2: a 43.0 dB rise; test 3: 11.66 dB).

#include "ExciterDsp.h"
#include "ExciterProcessor.h"
#include "LegacyExciterW1Dsp.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <fancy/Decibels.h>

#include <algorithm>
#include <cmath>
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
};

// Renders through a fresh ExciterDsp in blocks of 333 samples. With a tap, the top band's own
// added part (left) is written there.
Stereo render (const ExciterDsp::Targets& targets, Stereo signal, std::vector<float>* tap = nullptr)
{
    ExciterDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    if (tap != nullptr)
        tap->assign (signal.left.size(), 0.0f);

    const auto total = signal.left.size();

    for (size_t start = 0; start < total; start += 333)
    {
        const auto count = std::min<size_t> (333, total - start);
        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.setTopBandTap (tap != nullptr ? tap->data() + start : nullptr);
        dsp.process (channels, 2, (int) count);
    }

    return signal;
}

Stereo mono (const std::vector<double>& samples)
{
    Stereo signal;

    for (auto v : samples)
        signal.left.push_back ((float) v);

    signal.right = signal.left;
    return signal;
}

// What Exciter adds at Oversampling Off, where the latency is 0: the output minus the input.
std::vector<double> addedPart (const Stereo& output, const Stereo& input)
{
    std::vector<double> added (output.left.size());

    for (size_t i = 0; i < added.size(); ++i)
        added[i] = (double) output.left[i] - (double) input.left[i];

    return added;
}

// As the model's band_db: a Hann window (numpy's) over the whole span, the sum of |X|^2 over the
// DFT bins from lo up to (not including) hi, over the length, in dB.
double bandDb (const std::vector<double>& y, size_t from, size_t count, double lo, double hi)
{
    std::vector<double> windowed (count);

    for (size_t i = 0; i < count; ++i)
        windowed[i] = y[from + i] * (0.5 - 0.5 * std::cos (2.0 * pi * (double) i / (double) (count - 1)));

    std::vector<double> cosines (count), sines (count);

    for (size_t j = 0; j < count; ++j)
    {
        cosines[j] = std::cos (2.0 * pi * (double) j / (double) count);
        sines[j] = std::sin (2.0 * pi * (double) j / (double) count);
    }

    auto sum = 0.0;

    for (size_t k = 0; k <= count / 2; ++k)
    {
        const auto hz = (double) k * rate / (double) count;

        if (hz < lo || hz >= hi)
            continue;

        auto re = 0.0, im = 0.0;

        for (size_t i = 0; i < count; ++i)
        {
            const auto j = (k * i) % count;
            re += windowed[i] * cosines[j];
            im -= windowed[i] * sines[j];
        }

        sum += re * re + im * im;
    }

    return 10.0 * std::log10 (sum / (double) count + 1.0e-30);
}

double meanSquareDb (const std::vector<double>& y, size_t from, size_t to)
{
    auto sum = 0.0;

    for (auto i = from; i < to; ++i)
        sum += y[i] * y[i];

    return 10.0 * std::log10 (sum / (double) (to - from) + 1.0e-30);
}

Stereo music (int numSamples, unsigned seed)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<double> noise (-1.0, 1.0);
    Stereo signal;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto t = (double) i / rate;
        const auto common = 0.3 * std::sin (2.0 * pi * 300.0 * t) + 0.2 * std::sin (2.0 * pi * 3500.0 * t);
        signal.left.push_back ((float) (common + 0.15 * std::sin (2.0 * pi * 5100.0 * t) + 0.05 * noise (random)));
        signal.right.push_back ((float) (common + 0.15 * std::sin (2.0 * pi * 7300.0 * t) + 0.05 * noise (random)));
    }

    return signal;
}
} // namespace

TEST_CASE ("Exciter W2 1: at the new settings' defaults, Exciter renders exactly as v1.7.0", "[exciter][w2]")
{
    // Harmonics 2 at 0, Punch 0, High cut Off and Width 100 % against tests/LegacyExciterW1Dsp,
    // through every oversampling setting, gliding settings, Listen, mono and random block sizes.
    const auto sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    const auto numChannels = GENERATE (1, 2);
    INFO ("rate " << sampleRate << ", channels " << numChannels);

    ExciterDsp current;
    LegacyExciterW1Dsp legacy;

    const auto targetsAt = [] (int step)
    {
        ExciterDsp::Targets targets;
        targets.frequencyHz = step % 2 == 0 ? 3000.0 : 6500.0;
        targets.boostGain = fancy::dbToGain (step % 3 == 0 ? 6.0 : 18.0);
        targets.harmonics = 0.3 + 0.1 * (step % 5);
        targets.mixBackGain = 0.8;
        targets.character = step % 2 == 0 ? 0.2 : 0.9;
        targets.oversampling = step % 4;
        targets.bypass = step == 9;
        targets.listen = step == 5;
        return targets;
    };

    const auto legacyTargets = [] (const ExciterDsp::Targets& t)
    {
        LegacyExciterW1Dsp::Targets l;
        l.frequencyHz = t.frequencyHz;
        l.boostGain = t.boostGain;
        l.harmonics = t.harmonics;
        l.mixBackGain = t.mixBackGain;
        l.character = t.character;
        l.outputGain = t.outputGain;
        l.oversampling = t.oversampling;
        l.bypass = t.bypass;
        l.listen = t.listen;
        return l;
    };

    current.prepare (sampleRate);
    legacy.prepare (sampleRate);
    current.snapToTargets (targetsAt (2));
    legacy.snapToTargets (legacyTargets (targetsAt (2)));

    std::mt19937 random (7);
    std::uniform_int_distribution<int> sizes (1, 700);
    std::uniform_real_distribution<float> noise (-0.7f, 0.7f);

    long long compared = 0, differing = 0;

    for (int step = 0; step < 12; ++step)
    {
        const auto targets = targetsAt (step);
        const auto length = (int) (0.15 * sampleRate);

        for (int done = 0; done < length;)
        {
            const auto count = std::min (sizes (random), length - done);
            std::vector<float> a ((size_t) count * 2), b;

            for (auto& v : a)
                v = noise (random);

            b = a;

            float* currentChannels[] { a.data(), a.data() + count };
            float* legacyChannels[] { b.data(), b.data() + count };

            current.setTargets (targets);
            legacy.setTargets (legacyTargets (targets));
            current.process (currentChannels, numChannels, count);
            legacy.process (legacyChannels, numChannels, count);

            CHECK (current.getLatencySamples() == legacy.getLatencySamples());

            for (size_t i = 0; i < a.size(); ++i)
            {
                ++compared;
                differing += a[i] != b[i] ? 1 : 0;
            }

            done += count;
        }
    }

    INFO (differing << " of " << compared << " samples differ");
    CHECK (differing == 0);
}

TEST_CASE ("Exciter W2 2: Band 2 at 400 Hz fills 400 Hz to 1 kHz and leaves the top band's part alone", "[exciter][w2]")
{
    // The model's 150 Hz voice-like stack; Freq 3 kHz, Clip Boost 12 dB, Harmonics 100 %, Mix
    // Back -6 dB, Oversampling Off. The model: -30.9 dB with Band 2 off, 12.1 dB on.
    const int n = 24000;
    std::vector<double> voice ((size_t) n);

    for (int i = 0; i < n; ++i)
        for (int h = 1; h < 20; ++h)
            voice[(size_t) i] += 0.3 * std::sin (2.0 * pi * 150.0 * h * i / rate) / h;

    const auto input = mono (voice);

    ExciterDsp::Targets targets;
    targets.frequencyHz = 3000.0;
    targets.boostGain = fancy::dbToGain (12.0);
    targets.harmonics = 1.0;
    targets.mixBackGain = fancy::dbToGain (-6.0);
    targets.oversampling = ExciterDsp::off;

    auto withBand2 = targets;
    withBand2.frequency2Hz = 400.0;
    withBand2.harmonics2 = 1.0;

    std::vector<float> tapOff, tapOn;
    const auto off = addedPart (render (targets, input, &tapOff), input);
    const auto on = addedPart (render (withBand2, input, &tapOn), input);

    const auto from = (size_t) n / 2, count = (size_t) n / 2;
    const auto offDb = bandDb (off, from, count, 400.0, 1000.0);
    const auto onDb = bandDb (on, from, count, 400.0, 1000.0);

    INFO ("400 Hz to 1 kHz: off " << offDb << " dB, on " << onDb << " dB");
    CHECK (onDb - offDb > 30.0);

    // Reported, not tested (docs/exciter.md, section 9): what Band 2 adds above Freq.
    std::printf ("Exciter W2 Band 2 at 400 Hz: 400 Hz-1 kHz %.1f -> %.1f dB; above 3 kHz %.1f -> %.1f dB\n",
                 offDb, onDb, bandDb (off, from, count, 3000.0, 20000.0), bandDb (on, from, count, 3000.0, 20000.0));

    CHECK (tapOff == tapOn);

    // The same at 4x, where Band 2 has its own oversampler.
    targets.oversampling = withBand2.oversampling = ExciterDsp::x4;
    std::vector<float> tapOff4, tapOn4;
    render (targets, input, &tapOff4);
    render (withBand2, input, &tapOn4);
    CHECK (tapOff4 == tapOn4);
}

TEST_CASE ("Exciter W2 3: Punch 100 % lifts a pluck's attack against its sustain by 10 to 14 dB", "[exciter][w2]")
{
    // A 220 Hz pluck whose harmonics all decay over 0.25 s; Freq 2 kHz, Clip Boost 12 dB,
    // Harmonics 100 %, Oversampling Off. The added part's first 20 ms against 100 to 300 ms.
    const int n = 16800;
    std::vector<double> pluck ((size_t) n);

    for (int i = 0; i < n; ++i)
    {
        const auto t = (double) i / rate;

        for (int h = 1; h < 12; ++h)
            pluck[(size_t) i] += 0.5 * std::sin (2.0 * pi * 220.0 * h * t) * std::exp (-t / 0.25) / h;
    }

    const auto input = mono (pluck);

    ExciterDsp::Targets targets;
    targets.frequencyHz = 2000.0;
    targets.boostGain = fancy::dbToGain (12.0);
    targets.harmonics = 1.0;
    targets.mixBackGain = fancy::dbToGain (-6.0);
    targets.oversampling = ExciterDsp::off;

    const auto difference = [&] (double punch)
    {
        auto settings = targets;
        settings.punch = punch;
        const auto added = addedPart (render (settings, input), input);
        return meanSquareDb (added, 0, 960) - meanSquareDb (added, 4800, 14400);
    };

    const auto flat = difference (0.0);
    const auto punched = difference (1.0);

    INFO ("attack over sustain: " << flat << " dB at Punch 0, " << punched << " dB at 100 %");
    CHECK (punched - flat >= 10.0);
    CHECK (punched - flat <= 14.0);
}

TEST_CASE ("Exciter W2 4: Width 0 % makes the added part mono, and Width never changes its mid", "[exciter][w2]")
{
    const auto input = music (24000, 11);

    ExciterDsp::Targets targets;
    targets.harmonics = 0.6;
    targets.boostGain = fancy::dbToGain (9.0);
    targets.oversampling = ExciterDsp::x4;

    // The dry path through the same latency: Harmonics 0 adds nothing.
    auto dryTargets = targets;
    dryTargets.harmonics = 0.0;
    const auto dry = render (dryTargets, input);

    const auto sideAndMid = [&] (double width)
    {
        auto settings = targets;
        settings.width = width;
        const auto out = render (settings, input);
        std::vector<double> side, mid;

        for (size_t i = 4800; i < out.left.size(); ++i)
        {
            const auto left = (double) out.left[i] - (double) dry.left[i];
            const auto right = (double) out.right[i] - (double) dry.right[i];
            side.push_back (0.5 * (left - right));
            mid.push_back (0.5 * (left + right));
        }

        return std::pair { side, mid };
    };

    const auto [side100, mid100] = sideAndMid (1.0);
    const auto [side0, mid0] = sideAndMid (0.0);
    const auto [side150, mid150] = sideAndMid (1.5);

    auto peakSide100 = 0.0, peakSide0 = 0.0, worstSide150 = 0.0, worstMid = 0.0;

    for (size_t i = 0; i < side100.size(); ++i)
    {
        peakSide100 = std::max (peakSide100, std::abs (side100[i]));
        peakSide0 = std::max (peakSide0, std::abs (side0[i]));
        worstSide150 = std::max (worstSide150, std::abs (side150[i] - 1.5 * side100[i]));
        worstMid = std::max ({ worstMid, std::abs (mid0[i] - mid100[i]), std::abs (mid150[i] - mid100[i]) });
    }

    INFO ("side peak at 100 %: " << peakSide100 << ", at 0 %: " << peakSide0);
    CHECK (peakSide100 > 0.01);
    CHECK (peakSide0 < 1.0e-5);   // float rounding of the dry plus the added part, no more
    CHECK (worstSide150 < 1.0e-5);
    CHECK (worstMid < 1.0e-5);
}

TEST_CASE ("Exciter W2: moving every new setting while playing stays finite and bounded", "[exciter][w2]")
{
    // Harmonics 2, Freq 2, Punch, High cut (on, off and gliding) and Width move every block.
    const auto input = music (48000, 3);

    ExciterDsp dsp;
    dsp.prepare (rate);

    ExciterDsp::Targets targets;
    targets.harmonics = 0.7;
    targets.boostGain = fancy::dbToGain (12.0);
    dsp.snapToTargets (targets);

    auto signal = input;
    auto worst = 0.0f;
    auto finite = true;

    for (int block = 0; block * 256 < (int) signal.left.size(); ++block)
    {
        targets.harmonics2 = (block / 8) % 2 == 0 ? 0.0 : 0.9;
        targets.frequency2Hz = 150.0 + 40.0 * (block % 20);
        targets.punch = (block % 12) / 11.0;
        targets.highCutOn = (block / 5) % 3 != 0;
        targets.highCutHz = 4000.0 + 800.0 * (block % 20);
        targets.width = 0.1 * (block % 16);
        targets.oversampling = (block / 40) % 4;
        dsp.setTargets (targets);

        const auto start = (size_t) block * 256;
        const auto count = std::min<size_t> (256, signal.left.size() - start);
        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, (int) count);

        for (size_t i = start; i < start + count; ++i)
        {
            finite = finite && std::isfinite (signal.left[i]) && std::isfinite (signal.right[i]);
            worst = std::max ({ worst, std::abs (signal.left[i]), std::abs (signal.right[i]) });
        }
    }

    CHECK (finite);
    CHECK (worst < 4.0f);
}
