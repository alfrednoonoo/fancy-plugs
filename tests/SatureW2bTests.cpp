// Phase 4 Wave 2 for Fancy Sature, items 35 to 39: Offset, Diode and Fold, the cuts, Spread and
// Channels (docs/sature.md, "Phase 4 Wave 2: curve shaping, curve families, cuts, harmonic spread
// and Mid/Side", section 7). Figures: docs/research/sature-w2b-model.py (tests 2 and 3),
// docs/research/sature-families-prepush.py (test 3's aliasing) and
// docs/research/sature-w2b-build-check.py (tests 4 and 5, measured as here).

#include "LegacySatureW2Dsp.h"
#include "SatureDsp.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <complex>
#include <cstdio>
#include <numbers>
#include <random>
#include <vector>

namespace sat = fancy::dsp::saturation;

namespace
{
constexpr double pi = std::numbers::pi;

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples, 0.0f), right ((size_t) numSamples, 0.0f) {}
    int size() const { return (int) left.size(); }
};

Stereo tones (std::initializer_list<std::pair<double, double>> hzAndAmplitude, double seconds, double rate)
{
    Stereo s ((int) (seconds * rate));

    for (int i = 0; i < s.size(); ++i)
    {
        auto x = 0.0;

        for (const auto& [hz, amplitude] : hzAndAmplitude)
            x += amplitude * std::sin (2.0 * pi * hz * i / rate);

        s.left[(size_t) i] = s.right[(size_t) i] = (float) x;
    }

    return s;
}

Stereo pink (int numSamples, unsigned seed)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<double> uniform (-1.0, 1.0);
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    Stereo s (numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto white = uniform (random);
        b0 = 0.99886 * b0 + white * 0.0555179;
        b1 = 0.99332 * b1 + white * 0.0750759;
        b2 = 0.96900 * b2 + white * 0.1538520;
        b3 = 0.86650 * b3 + white * 0.3104856;
        b4 = 0.55000 * b4 + white * 0.5329522;
        b5 = -0.7616 * b5 - white * 0.0168980;
        const auto value = 0.03 * (b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362);
        b6 = white * 0.115926;
        s.left[(size_t) i] = (float) value;
        s.right[(size_t) i] = (float) (0.7 * value + 0.01 * uniform (random));
    }

    return s;
}

template <typename Dsp, typename Targets>
Stereo run (const Targets& targets, Stereo signal, double rate = 48000.0, int blockSize = 512)
{
    Dsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    for (int start = 0; start < signal.size(); start += blockSize)
    {
        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, std::min (blockSize, signal.size() - start));
    }

    return signal;
}

double toDb (double gain) { return 20.0 * std::log10 (std::max (gain, 1.0e-15)); }

// One frequency's level in dBFS, Hann windowed over the samples from `from` (SatureTests' levelAt).
double levelAt (const std::vector<float>& samples, int from, double hz, double rate = 48000.0)
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

// The bare curve's settings, as the models have them.
SatureDsp::Targets bare (int type, double driveDb)
{
    SatureDsp::Targets t;
    t.type = type;
    t.driveDb = driveDb;
    t.character = 0.0;
    t.autoGain = false;
    return t;
}

// Power spectrum (Hann, 2^15 points from `from`), for tests 3 and 4.
std::vector<double> powerSpectrum (const std::vector<float>& x, int from, int order = 15)
{
    const auto size = 1 << order;
    std::vector<float> data ((size_t) size * 2, 0.0f);

    for (int i = 0; i < size; ++i)
        data[(size_t) i] = (float) (x[(size_t) (from + i)] * (0.5 - 0.5 * std::cos (2.0 * pi * i / size)));

    juce::dsp::FFT fft (order);
    fft.performFrequencyOnlyForwardTransform (data.data());

    std::vector<double> power ((size_t) size / 2);

    for (int bin = 0; bin < size / 2; ++bin)
        power[(size_t) bin] = (double) data[(size_t) bin] * data[(size_t) bin];

    return power;
}

double bandDb (const std::vector<double>& power, double lowHz, double highHz, double excludeHz = -1.0, double rate = 48000.0)
{
    const auto binHz = rate / (2.0 * (double) power.size());
    auto sum = 0.0;

    for (size_t bin = 0; bin < power.size(); ++bin)
    {
        const auto hz = (double) bin * binHz;

        if (hz > lowHz && hz < highHz && (excludeHz < 0.0 || std::abs (hz - excludeHz) > 5.0))
            sum += power[bin];
    }

    return 10.0 * std::log10 (sum + 1.0e-30);
}

// The worst non-harmonic component in the audio band, as SatureTests' worstAliasDb, for any Type.
double worstAliasDb (int type, int factor, double toneHz)
{
    constexpr double rate = 44100.0;
    auto targets = bare (type, 24.0);
    targets.oversampling = factor;

    constexpr int order = 16, size = 1 << order;
    const auto settle = (int) rate;
    const auto output = run<SatureDsp> (targets, tones ({ { toneHz, 0.5 } }, (double) (settle + size) / rate + 0.01, rate), rate);

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

        if (hz < 20.0 || hz > 20000.0)
            continue;

        const auto nearest = std::round (hz / toneHz) * toneHz;

        if (nearest > 0.0 && std::abs (hz - nearest) < 6.0)
            continue;

        worst = std::max (worst, 2.0 * (double) data[(size_t) bin] / windowSum);
    }

    return toDb (worst);
}

// The level of a signal's part other than one tone: the tone fitted by least squares and taken off.
std::vector<double> withoutTone (const std::vector<float>& x, int from, double hz, double rate = 48000.0)
{
    double ss = 0, sc = 0, cc = 0, xs = 0, xc = 0;

    for (int i = from; i < (int) x.size(); ++i)
    {
        const auto s = std::sin (2.0 * pi * hz * i / rate), c = std::cos (2.0 * pi * hz * i / rate);
        ss += s * s; sc += s * c; cc += c * c;
        xs += x[(size_t) i] * s; xc += x[(size_t) i] * c;
    }

    const auto det = ss * cc - sc * sc;
    const auto a = (xs * cc - xc * sc) / det, b = (xc * ss - xs * sc) / det;
    std::vector<double> rest;

    for (int i = from; i < (int) x.size(); ++i)
        rest.push_back ((double) x[(size_t) i] - a * std::sin (2.0 * pi * hz * i / rate) - b * std::cos (2.0 * pi * hz * i / rate));

    return rest;
}
} // namespace

// 1. Compatibility ------------------------------------------------------------

TEST_CASE ("Sature with every new setting at its default renders exactly as v1.7.0", "[sature][w2b]")
{
    const auto type = GENERATE (0, 1, 2, 3);
    const auto oversampling = GENERATE (0, 1, 2, 3);
    const auto outer = GENERATE (false, true);
    const auto character = GENERATE (-0.3, 0.6);

    LegacySatureW2Dsp::Targets old;
    old.type = type;
    old.driveDb = 18.0;
    old.character = character;
    old.toneDb = 2.0;
    old.oversampling = oversampling;
    old.minimumPhase = oversampling != 3;

    if (outer)
    {
        old.lowOn = old.highOn = true;
        old.lowHz = 200.0;
        old.highHz = 6000.0;
        old.lowDriveDb = 12.0;
        old.highDriveDb = 6.0;
        old.wetGain = 0.7;
        old.dryGain = 0.5;
    }

    SatureDsp::Targets now;
    now.type = old.type;
    now.driveDb = old.driveDb;
    now.character = old.character;
    now.toneDb = old.toneDb;
    now.oversampling = old.oversampling;
    now.minimumPhase = old.minimumPhase;
    now.lowOn = old.lowOn;
    now.highOn = old.highOn;
    now.lowHz = old.lowHz;
    now.highHz = old.highHz;
    now.lowDriveDb = old.lowDriveDb;
    now.highDriveDb = old.highDriveDb;
    now.wetGain = old.wetGain;
    now.dryGain = old.dryGain;

    const auto input = pink (24000, 5);
    const auto a = run<LegacySatureW2Dsp> (old, input);
    const auto b = run<SatureDsp> (now, input);

    INFO ("type " << type << ", oversampling " << oversampling << ", outer " << outer << ", character " << character);
    REQUIRE (a.left == b.left);
    REQUIRE (a.right == b.right);
}

// 2. Offset -------------------------------------------------------------------

TEST_CASE ("Sature's Offset 0.25 on Tape gives H2 at -21 dB and leaves no DC", "[sature][w2b]")
{
    auto t = bare (sat::tape, 12.0);
    t.offset = 0.25;

    const auto out = run<SatureDsp> (t, tones ({ { 1000.0, 0.5 } }, 2.0, 48000.0));
    const auto from = 48000;
    const auto h2 = levelAt (out.left, from, 2000.0) - levelAt (out.left, from, 1000.0);
    const auto h3 = levelAt (out.left, from, 3000.0) - levelAt (out.left, from, 1000.0);

    // The last second: 1000 whole periods.
    auto sum = 0.0;
    for (int i = from; i < out.size(); ++i)
        sum += (double) out.left[(size_t) i];

    const auto dc = toDb (std::abs (sum / (out.size() - from)));
    std::printf ("sature offset 0.25: H2 %.1f dB (model -21), H3 %.1f dB (model -15), DC %.1f dBFS\n", h2, h3, dc);
    CHECK (std::abs (h2 + 21.0) <= 2.0);
    CHECK (dc < -80.0);
}

// 3. Diode and Fold -----------------------------------------------------------

TEST_CASE ("Sature's Diode and Fold give the model's harmonics", "[sature][w2b]")
{
    struct Expected { int type; double h2, h3, h4, h5; };
    const auto expected = GENERATE (Expected { sat::diode, -12.8, -15.3, -18.6, -26.5 }, Expected { sat::fold, -400.0, 1.3, -400.0, -14.9 });

    const auto out = run<SatureDsp> (bare (expected.type, 12.0), tones ({ { 1000.0, 0.5 } }, 2.0, 48000.0));
    const auto from = 48000;
    const auto h1 = levelAt (out.left, from, 1000.0);
    const double want[] { expected.h2, expected.h3, expected.h4, expected.h5 };

    for (int k = 2; k <= 5; ++k)
    {
        const auto level = levelAt (out.left, from, 1000.0 * k) - h1;
        std::printf ("sature %s: H%d %.1f dB\n", expected.type == sat::diode ? "diode" : "fold", k, level);
        INFO ((expected.type == sat::diode ? "Diode" : "Fold") << " H" << k << " " << level << " dB, model " << want[k - 2]);

        // Fold has no even harmonics: they only have to stay far down.
        if (want[k - 2] < -100.0)
            CHECK (level < -60.0);
        else
            CHECK (std::abs (level - want[k - 2]) <= 2.0);
    }
}

TEST_CASE ("Sature's Diode and Fold alias no more than Clip's limits", "[sature][w2b][aliasing]")
{
    const auto type = GENERATE (sat::diode, sat::fold);

    const auto offAt1k = worstAliasDb (type, SatureDsp::off, 1000.0);
    const auto x4At1k = worstAliasDb (type, SatureDsp::x4, 1000.0);
    const auto x4At10k = worstAliasDb (type, SatureDsp::x4, 10000.0);

    std::printf ("sature %s aliasing: 1 kHz Off %.1f, 4x %.1f; 10 kHz 4x %.1f dBFS\n", type == sat::diode ? "diode" : "fold", offAt1k, x4At1k, x4At10k);
    CHECK (offAt1k < -38.0);
    CHECK (x4At1k < -90.0);
    CHECK (x4At10k < -50.0);
}

// 4. The cuts -----------------------------------------------------------------

TEST_CASE ("Sature's Low cut Pre stops a bass's intermodulation; Post leaves the added part above it", "[sature][w2b]")
{
    // The model's input: 80 Hz at -6 dBFS and 2 kHz at -18; Tape at Drive 12; only the added part.
    const auto input = tones ({ { 80.0, std::pow (10.0, -6.0 / 20.0) }, { 2000.0, std::pow (10.0, -18.0 / 20.0) } }, 1.5, 48000.0);
    auto t = bare (sat::tape, 12.0);
    t.oversampling = SatureDsp::off;
    t.listen = true;

    const auto none = powerSpectrum (run<SatureDsp> (t, input).left, 24000);

    t.lowCutOn = true;
    t.lowCutHz = 200.0;
    t.lowCutPost = false;
    const auto pre = powerSpectrum (run<SatureDsp> (t, input).left, 24000);

    t.lowCutPost = true;
    const auto post = powerSpectrum (run<SatureDsp> (t, input).left, 24000);

    const auto imNone = bandDb (none, 1800.0, 2200.0, 2000.0);
    const auto imPre = bandDb (pre, 1800.0, 2200.0, 2000.0);
    const auto aboveNone = bandDb (none, 1000.0, 24000.0);
    const auto abovePost = bandDb (post, 1000.0, 24000.0);

    std::printf ("sature low cut: intermodulation around 2 kHz %.1f dB without, %.1f dB Pre (drop %.1f, model 17.7); "
                 "added part above 1 kHz, Post against none, %.2f dB\n", imNone, imPre, imNone - imPre, abovePost - aboveNone);
    CHECK (imNone - imPre >= 12.0);
    CHECK (std::abs (abovePost - aboveNone) < 0.5);
}

// 5. Spread -------------------------------------------------------------------

TEST_CASE ("Sature's Spread at 50 % decorrelates the harmonics and leaves the fundamental", "[sature][w2b]")
{
    const auto input = tones ({ { 220.0, 0.4 } }, 1.5, 48000.0);
    auto t = bare (sat::tape, 12.0);
    t.oversampling = SatureDsp::off;

    const auto centred = run<SatureDsp> (t, input);
    t.spread = 0.5;
    const auto spread = run<SatureDsp> (t, input);

    const auto from = 24000;
    const auto left = withoutTone (spread.left, from, 220.0);
    const auto right = withoutTone (spread.right, from, 220.0);

    double lr = 0, ll = 0, rr = 0;

    for (size_t i = 0; i < left.size(); ++i)
    {
        lr += left[i] * right[i];
        ll += left[i] * left[i];
        rr += right[i] * right[i];
    }

    const auto correlation = lr / std::sqrt (ll * rr);
    const auto base = levelAt (centred.left, from, 220.0);
    const auto leftChange = levelAt (spread.left, from, 220.0) - base;
    const auto rightChange = levelAt (spread.right, from, 220.0) - base;

    std::printf ("sature spread 50 %%: harmonics' correlation %+.3f (model +0.006), fundamental L %+.3f dB, R %+.3f dB\n",
                 correlation, leftChange, rightChange);
    CHECK (std::abs (correlation) <= 0.05);
    CHECK (std::abs (leftChange) < 0.1);
    CHECK (std::abs (rightChange) < 0.1);
}

// 6. Channels -----------------------------------------------------------------

TEST_CASE ("Sature's Mid only leaves a side-only input as it was, and Side only a mid-only one", "[sature][w2b]")
{
    const auto side = GENERATE (false, true);

    auto t = bare (sat::tape, 24.0);
    t.character = -0.3;   // with the default bias, which must cancel exactly too
    t.oversampling = SatureDsp::off;
    t.channels = side ? SatureDsp::sideOnly : SatureDsp::midOnly;

    auto input = pink (24000, 9);

    for (int i = 0; i < input.size(); ++i)
        input.right[(size_t) i] = side ? input.left[(size_t) i] : -input.left[(size_t) i];

    const auto out = run<SatureDsp> (t, input);

    INFO ((side ? "Side only, mid-only input" : "Mid only, side-only input"));
    CHECK (out.left == input.left);
    CHECK (out.right == input.right);

    // And the other way round, it saturates.
    for (int i = 0; i < input.size(); ++i)
        input.right[(size_t) i] = side ? -input.left[(size_t) i] : input.left[(size_t) i];

    const auto driven = run<SatureDsp> (t, input);
    auto difference = 0.0;

    for (int i = 0; i < input.size(); ++i)
        difference = std::max (difference, (double) std::abs (driven.left[(size_t) i] - input.left[(size_t) i]));

    CHECK (difference > 0.01);
}
