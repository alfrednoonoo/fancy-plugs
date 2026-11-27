// Phase 4 Wave 3 for Fancy Gate: multiband gating (docs/gate.md, "Phase 4 Wave 3: multiband
// gating", section 6; modelled in docs/research/gate-multiband-model.py). Every number here was run
// first through docs/research/gate-w3-prepush.py, which models GateDsp as built.

#include "GateDsp.h"
#include "LegacyGateW2bDsp.h"
#include "CompCpuReference.h"

#include <juce_dsp/juce_dsp.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = 3.14159265358979323846;

int samples (double seconds)
{
    return (int) (seconds * rate);
}

struct Render
{
    std::vector<float> left, right;
    std::vector<juce::MidiMessage> events;
    std::vector<int> times;
};

// Renders through any GateDsp-like class (the current one and the v1.7.1 copy share process()).
template <typename Dsp, typename Targets>
Render render (Dsp& dsp, const Targets& targets, const std::vector<float>& left, const std::vector<float>& right,
               const std::vector<float>* sidechain, int blockSize, bool randomBlocks = false)
{
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    Render out { left, right, {}, {} };
    const auto size = (int) left.size();
    std::vector<float> side = sidechain != nullptr ? *sidechain : std::vector<float> ((size_t) size, 0.0f);
    std::mt19937 random (11);
    std::uniform_int_distribution<int> sizes (1, 2000);

    for (int start = 0; start < size;)
    {
        const auto count = std::min (randomBlocks ? sizes (random) : blockSize, size - start);
        float* channels[] { out.left.data() + start, out.right.data() + start };
        const float* keys[] { side.data() + start, side.data() + start };
        juce::MidiBuffer midi;
        dsp.setTargets (targets);
        dsp.process (channels, 2, keys, sidechain != nullptr ? 2 : 0, count, &midi);

        for (const auto metadata : midi)
        {
            out.events.push_back (metadata.getMessage());
            out.times.push_back (start + metadata.samplePosition);
        }

        start += count;
    }

    return out;
}

LegacyGateW2bDsp::Targets toLegacy (const GateDsp::Targets& t)
{
    LegacyGateW2bDsp::Targets l;
    l.thresholdDb = t.thresholdDb;
    l.hysteresisDb = t.hysteresisDb;
    l.preOpenMs = t.preOpenMs;
    l.attackMs = t.attackMs;
    l.holdMs = t.holdMs;
    l.releaseMs = t.releaseMs;
    l.rangeDb = t.rangeDb;
    l.rmsMs = t.rmsMs;
    l.lowpassHz = t.lowpassHz;
    l.highpassHz = t.highpassHz;
    l.detector = t.detector;
    l.lookahead = t.lookahead;
    l.preview = t.preview;
    l.duck = t.duck;
    l.link = t.link;
    l.wetGain = t.wetGain;
    l.dryGain = t.dryGain;
    l.noiseGain = t.noiseGain;
    l.outputGain = t.outputGain;
    l.midi = t.midi;
    l.velocityFromLevel = t.velocityFromLevel;
    l.note = t.note;
    l.channel = t.channel;
    l.velocityWindowMs = t.velocityWindowMs;
    l.bypass = t.bypass;
    l.midiIn = t.midiIn;
    l.ratio = t.ratio;
    l.deBleed = t.deBleed;
    l.syncStepBeats = t.syncStepBeats;
    l.syncShape = t.syncShape;
    l.transient = t.transient;
    l.riseDb = t.riseDb;
    l.upward = t.upward;
    l.boostDb = t.boostDb;
    l.upRatio = t.upRatio;
    return l;
}

// An RBJ biquad, in double.
std::vector<double> biquad (const std::vector<double>& x, const double (&b)[3], const double (&a)[3])
{
    std::vector<double> y (x.size());
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    for (size_t i = 0; i < x.size(); ++i)
    {
        const auto out = b[0] * x[i] + b[1] * x1 + b[2] * x2 - a[1] * y1 - a[2] * y2;
        x2 = x1;
        x1 = x[i];
        y2 = y1;
        y1 = out;
        y[i] = out;
    }

    return y;
}

// A Linkwitz-Riley 24 dB/oct low or high pass, as the model's lr24.
std::vector<double> lr24 (const std::vector<double>& x, double hz, bool low)
{
    const auto w = 2.0 * pi * hz / rate, c = std::cos (w), alpha = std::sin (w) / (2.0 * 0.7071067811865476), a0 = 1.0 + alpha;
    const double lb[] { (1.0 - c) / 2.0 / a0, (1.0 - c) / a0, (1.0 - c) / 2.0 / a0 };
    const double hb[] { (1.0 + c) / 2.0 / a0, -(1.0 + c) / a0, (1.0 + c) / 2.0 / a0 };
    const double a[] { 1.0, -2.0 * c / a0, (1.0 - alpha) / a0 };
    return low ? biquad (biquad (x, lb, a), lb, a) : biquad (biquad (x, hb, a), hb, a);
}

std::vector<float> toFloat (const std::vector<double>& x)
{
    return std::vector<float> (x.begin(), x.end());
}

std::vector<double> toDouble (const std::vector<float>& x)
{
    return std::vector<double> (x.begin(), x.end());
}

// The magnitude response in dB of an impulse response, at bin k of an n-point FFT.
std::vector<double> magnitudeDb (const std::vector<float>& response, int order)
{
    juce::dsp::FFT fft (order);
    const auto n = 1 << order;
    std::vector<float> buffer ((size_t) n * 2, 0.0f);
    std::copy_n (response.begin(), std::min ((int) response.size(), n), buffer.begin());
    fft.performFrequencyOnlyForwardTransform (buffer.data());

    std::vector<double> db ((size_t) n / 2);

    for (int k = 0; k < n / 2; ++k)
        db[(size_t) k] = 20.0 * std::log10 (std::max ((double) buffer[(size_t) k], 1.0e-30));

    return db;
}

// The largest deviation from expectedDb between 20 Hz and 20 kHz.
double worstDeviation (const std::vector<double>& db, int order, double expectedDb)
{
    const auto n = 1 << order;
    auto worst = 0.0;

    for (int k = 1; k < n / 2; ++k)
    {
        const auto hz = k * rate / n;

        if (hz >= 20.0 && hz <= 20000.0)
            worst = std::max (worst, std::abs (db[(size_t) k] - expectedDb));
    }

    return worst;
}

GateDsp::Targets bandTargets (int bands)
{
    GateDsp::Targets targets;
    targets.bands = bands;
    targets.crossoverHz = { 250.0, 2500.0 };
    targets.bandOffsetDb = { 0.0, 6.0, 6.0 };
    targets.bandDepth = { 1.0, 1.0, 1.0 };
    targets.thresholdDb = -12.0;
    targets.hysteresisDb = -6.0;
    targets.attackMs = 0.5;
    targets.holdMs = 40.0;
    targets.releaseMs = 120.0;
    targets.rangeDb = -40.0;
    targets.rmsMs = 0.0;
    return targets;
}

// The model's tom close mic (docs/research/gate-multiband-model.py), with this file's own random numbers.
struct TomMic
{
    std::vector<double> tom, mic;
    std::vector<int> hits;
};

TomMic tomMic()
{
    const auto n = samples (4.5);
    std::mt19937 random (7);
    std::normal_distribution<double> normal;
    TomMic t { std::vector<double> ((size_t) n, 0.0), {}, {} };

    for (int k = 0; k < 6; ++k)
        t.hits.push_back (samples (0.25 + 0.75 * k));

    for (const auto h : t.hits)
    {
        for (int k = 0; k < samples (0.8) && h + k < n; ++k)
        {
            const auto phase = 2.0 * pi * 110.0 * k / rate * (1.0 + 0.15 * std::exp (-k / (0.02 * rate)));
            t.tom[(size_t) (h + k)] += 0.7 * std::exp (-k / (0.18 * rate)) * std::sin (phase);

            if (k < 0.004 * rate)
                t.tom[(size_t) (h + k)] += 0.25 * std::exp (-k / (0.004 * rate)) * normal (random);
        }
    }

    std::vector<double> envelope ((size_t) n, 0.02), noise ((size_t) n);

    for (const auto c : { samples (0.6), samples (2.9) })
        for (int k = 0; c + k < n; ++k)
            envelope[(size_t) (c + k)] += 0.06 * std::exp (-k / (0.6 * rate));

    for (auto& v : noise)
        v = normal (random);

    auto cymbal = lr24 (noise, 3000.0, false);

    std::vector<double> snare ((size_t) n, 0.0);

    for (const auto h : t.hits)
    {
        const auto s = h + samples (0.375);

        for (int k = 0; k < samples (0.15) && s + k < n; ++k)
            snare[(size_t) (s + k)] += normal (random) * 0.05 * std::exp (-k / (0.03 * rate));
    }

    snare = lr24 (lr24 (snare, 200.0, false), 2000.0, true);

    t.mic.resize ((size_t) n);

    for (int i = 0; i < n; ++i)
        t.mic[(size_t) i] = t.tom[(size_t) i] + cymbal[(size_t) i] * envelope[(size_t) i] + snare[(size_t) i];

    return t;
}

double energy (const std::vector<double>& x, int from, int to)
{
    auto sum = 0.0;

    for (int i = std::max (0, from); i < std::min ((int) x.size(), to); ++i)
        sum += x[(size_t) i] * x[(size_t) i];

    return sum;
}

struct TomFigures
{
    double tailKept, cymbalWithToms, bleedInGaps;
};

TomFigures measureTom (const TomMic& t, const std::vector<double>& out)
{
    const auto toms = lr24 (t.tom, 300.0, true), kept = lr24 (out, 300.0, true);
    const auto highIn = lr24 (t.mic, 2000.0, false), highOut = lr24 (out, 2000.0, false);
    const auto n = (int) out.size();

    auto tail = 0.0;
    std::vector<bool> inside ((size_t) n, false), outside ((size_t) n, true);

    for (const auto h : t.hits)
    {
        const auto a = h + samples (0.03), b = h + samples (0.4);
        tail += energy (kept, a, b) / energy (toms, a, b);

        for (int i = a; i < std::min (n, b); ++i)
            inside[(size_t) i] = true;

        for (int i = std::max (0, h - samples (0.01)); i < std::min (n, b); ++i)
            outside[(size_t) i] = false;
    }

    double inIn = 0, inOut = 0, gapIn = 0, gapOut = 0;

    for (int i = 0; i < n; ++i)
    {
        if (inside[(size_t) i])
        {
            inIn += highIn[(size_t) i] * highIn[(size_t) i];
            inOut += highOut[(size_t) i] * highOut[(size_t) i];
        }

        if (outside[(size_t) i])
        {
            gapIn += highIn[(size_t) i] * highIn[(size_t) i];
            gapOut += highOut[(size_t) i] * highOut[(size_t) i];
        }
    }

    return { 10.0 * std::log10 (tail / (double) t.hits.size()), 10.0 * std::log10 (inOut / inIn), 10.0 * std::log10 (gapOut / gapIn) };
}

std::vector<float> sine (double hz, double amplitude, int n)
{
    std::vector<float> x ((size_t) n);

    for (int i = 0; i < n; ++i)
        x[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * hz * i / rate));

    return x;
}

// Drums and noise for the legacy and block-size tests: decaying hits over a noise floor.
std::vector<float> drums (unsigned seed, int n)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);
    std::vector<float> x ((size_t) n);

    for (int i = 0; i < n; ++i)
    {
        const auto t = i % samples (0.3);
        x[(size_t) i] = 0.004f * noise (random)
                      + (float) (0.6 * std::exp (-t / (0.05 * rate)) * std::sin (2.0 * pi * (90.0 + 30.0 * (i / samples (0.3) % 3)) * t / rate))
                      + (float) (0.08 * std::exp (-t / (0.01 * rate))) * noise (random);
    }

    return x;
}
} // namespace

// 1 ------------------------------------------------------------------------------------------

TEST_CASE ("gate w3: one band renders exactly as v1.7.1, whatever the band settings", "[gate][w3]")
{
    const auto setting = GENERATE (range (0, 14));

    GateDsp::Targets targets;
    targets.thresholdDb = -30.0;
    targets.attackMs = 1.0;
    targets.holdMs = 30.0;
    targets.releaseMs = 80.0;
    targets.rangeDb = -30.0;
    targets.noiseGain = 0.01;

    switch (setting)
    {
        case 0: break;
        case 1: targets.duck = true; break;
        case 2: targets.upward = true; break;
        case 3: targets.lookahead = true; targets.preOpenMs = 3.0; break;
        case 4: targets.link = false; break;
        case 5: targets.ratio = 3.0; break;
        case 6: targets.transient = true; break;
        case 7: targets.deBleed = 0.7; targets.highpassHz = 120.0; targets.lowpassHz = 3000.0; break;
        case 8: targets.detector = GateDsp::sideLR; break;
        case 9: targets.detector = GateDsp::mid; targets.rmsMs = 20.0; break;
        case 10: targets.midi = true; targets.velocityFromLevel = true; break;
        case 11: targets.upward = true; targets.transient = true; targets.link = false; break;
        case 12: targets.rangeDb = -200.0; targets.dryGain = 0.3; break;
        case 13: targets.duck = true; targets.lookahead = true; targets.ratio = 2.0; targets.midi = true; break;
        default: break;
    }

    const auto legacyTargets = toLegacy (targets);

    // Band settings far from their defaults, with Bands 1: none of them may count.
    targets.bands = 1;
    targets.crossoverLr48 = true;
    targets.crossoverHz = { 900.0, 6000.0 };
    targets.bandOffsetDb = { 12.0, -20.0, 7.0 };
    targets.bandDepth = { 0.3, 0.0, 0.8 };

    const auto n = samples (1.5);
    const auto left = drums (3, n), right = drums (4, n), side = drums (5, n);

    GateDsp current;
    LegacyGateW2bDsp legacy;
    const auto a = render (current, targets, left, right, &side, 333);
    const auto b = render (legacy, legacyTargets, left, right, &side, 333);

    INFO ("setting " << setting);
    CHECK (a.left == b.left);
    CHECK (a.right == b.right);
    REQUIRE (a.events.size() == b.events.size());

    for (size_t i = 0; i < a.events.size(); ++i)
    {
        CHECK (a.times[i] == b.times[i]);
        CHECK (a.events[i].getRawDataSize() == b.events[i].getRawDataSize());
        CHECK (std::equal (a.events[i].getRawData(), a.events[i].getRawData() + a.events[i].getRawDataSize(), b.events[i].getRawData()));
    }
}

// 2 ------------------------------------------------------------------------------------------

TEST_CASE ("gate w3: with every band open, 2 and 3 bands sum flat at LR 24 and LR 48", "[gate][w3]")
{
    const auto bands = GENERATE (2, 3);
    const auto lr48 = GENERATE (false, true);
    constexpr int order = 15;

    auto targets = bandTargets (bands);
    targets.crossoverLr48 = lr48;
    targets.rangeDb = 0.0;   // every band passes whole, open or shut

    std::vector<float> impulse ((size_t) 1 << order, 0.0f);
    impulse[0] = 1.0f;

    GateDsp dsp;
    const auto out = render (dsp, targets, impulse, impulse, nullptr, 512);
    const auto worst = worstDeviation (magnitudeDb (out.left, order), order, 0.0);

    INFO ("bands " << bands << (lr48 ? ", LR 48" : ", LR 24") << ": " << worst << " dB");
    CHECK (worst < 0.05);
}

// 3 ------------------------------------------------------------------------------------------

TEST_CASE ("gate w3: the model's tom mic, two bands cut the cymbals with the toms and in the gaps", "[gate][w3]")
{
    const auto t = tomMic();
    const auto input = toFloat (t.mic);

    GateDsp twoBands;
    const auto banded = measureTom (t, toDouble (render (twoBands, bandTargets (2), input, input, nullptr, 256).left));

    auto wide = bandTargets (1);
    wide.thresholdDb = -6.0;
    GateDsp wideband;
    const auto plain = measureTom (t, toDouble (render (wideband, wide, input, input, nullptr, 256).left));

    std::printf ("gate w3 tom mic: 2 bands: tail kept %+.2f dB, cymbal with toms %+.1f dB, bleed in gaps %+.1f dB; "
                 "wideband -6 dB: cymbal with toms %+.1f dB (model as built: -0.19, -13.4, -40.0; -1.5)\n",
                 banded.tailKept, banded.cymbalWithToms, banded.bleedInGaps, plain.cymbalWithToms);

    CHECK (std::abs (banded.tailKept) < 1.0);
    CHECK (banded.cymbalWithToms <= -8.0);
    CHECK (banded.bleedInGaps <= -30.0);
    CHECK (plain.cymbalWithToms >= -3.0);
}

// 4 ------------------------------------------------------------------------------------------

TEST_CASE ("gate w3: a band opens on its own sound, and another band's stays shut", "[gate][w3]")
{
    const auto n = samples (2.0);
    std::vector<float> input ((size_t) n);

    for (int i = 0; i < n; ++i)
    {
        const auto t = i / rate;
        const auto burst = std::fmod (t, 1.0) < 0.5 ? 0.5 * std::sin (2.0 * pi * 100.0 * t) : 0.0;
        input[(size_t) i] = (float) (burst + 0.0316 * std::sin (2.0 * pi * 5000.0 * t));
    }

    auto targets = bandTargets (2);
    targets.crossoverHz = { 1000.0, 2500.0 };
    targets.thresholdDb = -20.0;

    GateDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    constexpr int blockSize = 64;
    std::vector<float> left (input), right (input);
    auto lowOpenInBursts = true, highEverOpen = false;
    auto leastHighCut = 200.0f;

    for (int start = 0; start + blockSize <= n; start += blockSize)
    {
        float* channels[] { left.data() + start, right.data() + start };
        dsp.process (channels, 2, nullptr, 0, blockSize, nullptr);

        const auto t = std::fmod ((start + blockSize) / rate, 1.0);

        if (t > 0.1 && t < 0.4 && ! dsp.isBandOpen (0))
            lowOpenInBursts = false;

        highEverOpen = highEverOpen || dsp.isBandOpen (1);
        leastHighCut = std::min (leastHighCut, dsp.takeBandReduction (1));
    }

    INFO ("band 2's least cut " << leastHighCut << " dB");
    CHECK (lowOpenInBursts);
    CHECK_FALSE (highEverOpen);
    CHECK (leastHighCut > 39.9f);   // at Range, -40 dB, throughout
}

// 5 ------------------------------------------------------------------------------------------

TEST_CASE ("gate w3: a band's depth sets how far it closes", "[gate][w3]")
{
    const auto rangeDb = GENERATE (-40.0, -200.0);
    const auto expected = rangeDb < -100.0 ? 60.0f : 20.0f;   // -inf counts as -120 dB

    auto targets = bandTargets (2);
    targets.rangeDb = rangeDb;
    targets.bandDepth = { 1.0, 0.5, 1.0 };

    const auto input = sine (5000.0, 0.001, samples (0.5));
    GateDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    std::vector<float> left (input), right (input);
    float cut = 0.0f;

    for (int start = 0; start + 256 <= (int) input.size(); start += 256)
    {
        float* channels[] { left.data() + start, right.data() + start };
        dsp.process (channels, 2, nullptr, 0, 256, nullptr);
        cut = dsp.takeBandReduction (1);
    }

    INFO ("Range " << rangeDb << ": band 2 cuts " << cut << " dB");
    CHECK (std::abs (cut - expected) <= 0.5f);
}

// 6 ------------------------------------------------------------------------------------------

TEST_CASE ("gate w3: a band's threshold offset opens it that much later", "[gate][w3]")
{
    const auto opensAt = [] (double offsetDb)
    {
        auto targets = bandTargets (2);
        targets.thresholdDb = -30.0;
        targets.bandOffsetDb = { 0.0, offsetDb, 0.0 };

        const auto n = samples (0.8);
        std::vector<float> rise ((size_t) n);

        for (int i = 0; i < n; ++i)
            rise[(size_t) i] = (float) (std::pow (10.0, (-60.0 + 100.0 * i / rate) / 20.0) * std::sin (2.0 * pi * 2000.0 * i / rate));

        GateDsp dsp;
        dsp.prepare (rate);
        dsp.snapToTargets (targets);

        std::vector<float> left (rise), right (rise);

        for (int i = 0; i < n; ++i)
        {
            float* channels[] { left.data() + i, right.data() + i };
            dsp.process (channels, 2, nullptr, 0, 1, nullptr);

            if (dsp.isBandOpen (1))
                return -60.0 + 100.0 * i / rate;
        }

        return 0.0;
    };

    const auto atZero = opensAt (0.0), atSix = opensAt (6.0);

    INFO ("opens at " << atZero << " dB with offset 0 and " << atSix << " dB with +6 (model: -29.94 and -23.94)");
    CHECK (std::abs ((atSix - atZero) - 6.0) <= 0.5);
    CHECK (std::abs (atZero + 30.0) <= 0.5);
}

// 7 ------------------------------------------------------------------------------------------

TEST_CASE ("gate w3: Dry is the crossover's all-pass, so Wet and Dry stay in phase", "[gate][w3]")
{
    constexpr int order = 15;
    std::vector<float> impulse ((size_t) 1 << order, 0.0f);
    impulse[0] = 1.0f;

    // Wet off, Dry at 0 dB: the bands summed ungated.
    auto dryOnly = bandTargets (3);
    dryOnly.wetGain = 0.0;
    dryOnly.dryGain = 1.0;

    GateDsp a;
    const auto dryWorst = worstDeviation (magnitudeDb (render (a, dryOnly, impulse, impulse, nullptr, 512).left, order), order, 0.0);

    // Both at 0 dB with every band passing: no comb, 6 dB up.
    auto both = dryOnly;
    both.wetGain = 1.0;
    both.rangeDb = 0.0;

    GateDsp b;
    const auto bothWorst = worstDeviation (magnitudeDb (render (b, both, impulse, impulse, nullptr, 512).left, order), order,
                                           20.0 * std::log10 (2.0));

    INFO ("dry only " << dryWorst << " dB, wet and dry " << bothWorst << " dB");
    CHECK (dryWorst < 0.05);
    CHECK (bothWorst < 0.05);
}

// 8 ------------------------------------------------------------------------------------------

TEST_CASE ("gate w3: the light, the Gain meter and MIDI out follow the most open band", "[gate][w3]")
{
    const auto n = samples (1.6);
    std::vector<float> input ((size_t) n, 0.0f);

    // Band 1 from 0.2 to 0.6 s, band 2 inside it from 0.3 to 0.5 s, then band 2 alone from 1.0 to 1.2 s.
    for (int i = 0; i < n; ++i)
    {
        const auto t = i / rate;
        auto x = 0.0;

        if (t >= 0.2 && t < 0.6)
            x += 0.5 * std::sin (2.0 * pi * 100.0 * (t - 0.2));

        if ((t >= 0.3 && t < 0.5) || (t >= 1.0 && t < 1.2))
            x += 0.5 * std::sin (2.0 * pi * 5000.0 * t);

        input[(size_t) i] = (float) x;
    }

    auto targets = bandTargets (2);
    targets.crossoverHz = { 1000.0, 2500.0 };
    targets.thresholdDb = -20.0;
    targets.bandOffsetDb = { 0.0, 0.0, 0.0 };
    targets.holdMs = 20.0;
    targets.releaseMs = 50.0;
    targets.midi = true;

    GateDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    constexpr int blockSize = 64;
    std::vector<float> left (input), right (input);
    auto ons = 0, offs = 0;
    auto lightOpenAlone = true;
    auto gainCutAlone = 0.0f;

    for (int start = 0; start + blockSize <= n; start += blockSize)
    {
        float* channels[] { left.data() + start, right.data() + start };
        juce::MidiBuffer midi;
        dsp.process (channels, 2, nullptr, 0, blockSize, &midi);

        for (const auto metadata : midi)
        {
            ons += metadata.getMessage().isNoteOn() ? 1 : 0;
            offs += metadata.getMessage().isNoteOff() ? 1 : 0;
        }

        const auto t = (start + blockSize) / rate;
        const auto cut = dsp.takeReduction();

        if (t > 1.05 && t < 1.15)
        {
            lightOpenAlone = lightOpenAlone && dsp.getLight() == GateDsp::lightOpen;
            gainCutAlone = std::max (gainCutAlone, cut);
        }
    }

    INFO ("note-ons " << ons << ", note-offs " << offs << ", Gain meter with band 2 alone " << gainCutAlone << " dB");
    CHECK (ons == 2);
    CHECK (offs == 2);
    CHECK (lightOpenAlone);
    CHECK (gainCutAlone < 1.0f);
}

// 9 ------------------------------------------------------------------------------------------

TEST_CASE ("gate w3: crossovers, offsets, depths and the band count moved while playing stay smooth and finite", "[gate][w3]")
{
    const auto n = samples (3.0);
    const auto input = sine (100.0, 0.5, n);
    std::vector<float> left (input), right (input);

    auto targets = bandTargets (3);
    targets.thresholdDb = -20.0;

    GateDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    constexpr int blockSize = 64;

    for (int start = 0; start + blockSize <= n; start += blockSize)
    {
        const auto t = (double) start / (double) n;

        if (t < 0.6)
        {
            // Every band setting sweeps through its range.
            const auto u = t / 0.6;
            targets.crossoverHz = { 20.0 * std::pow (1000.0, u), 20.0 * std::pow (1000.0, 1.0 - u) };
            targets.bandOffsetDb = { -24.0 + 48.0 * u, 24.0 - 48.0 * u, std::sin (12.0 * u) * 24.0 };
            targets.bandDepth = { u, 1.0 - u, 0.5 + 0.5 * std::sin (9.0 * u) };
        }
        else
        {
            // Then the band count changes while playing.
            targets.bands = t < 0.73 ? 1 : (t < 0.86 ? 3 : 2);
        }

        dsp.setTargets (targets);
        float* channels[] { left.data() + start, right.data() + start };
        dsp.process (channels, 2, nullptr, 0, blockSize, nullptr);
    }

    auto finite = true;
    auto largestStep = 0.0f;

    for (size_t i = 1; i < left.size(); ++i)
    {
        finite = finite && std::isfinite (left[i]) && std::isfinite (right[i]);
        largestStep = std::max ({ largestStep, std::abs (left[i] - left[i - 1]), std::abs (right[i] - right[i - 1]) });
    }

    INFO ("largest step " << largestStep);
    CHECK (finite);
    CHECK (largestStep <= 0.1f);
}

// 10 -----------------------------------------------------------------------------------------

TEST_CASE ("gate w3: three bands render the same, bit for bit, with random block sizes", "[gate][w3]")
{
    const auto lookahead = GENERATE (false, true);

    auto targets = bandTargets (3);
    targets.thresholdDb = -25.0;
    targets.lookahead = lookahead;
    targets.preOpenMs = 2.0;
    targets.noiseGain = 0.01;
    targets.dryGain = 0.1;

    const auto n = samples (2.0);
    const auto left = drums (21, n), right = drums (22, n);

    GateDsp a, b;
    const auto fixed = render (a, targets, left, right, nullptr, 333);
    const auto random = render (b, targets, left, right, nullptr, 0, true);

    CHECK (fixed.left == random.left);
    CHECK (fixed.right == random.right);
}

// 11 -----------------------------------------------------------------------------------------

TEST_CASE ("gate w3 cpu measurement", "[.measure]")
{
    constexpr int blockSize = 128, blocks = 6000, runs = 5;

    const auto measure = [] (int bands)
    {
        GateDsp::Targets targets;
        targets.bands = bands;

        GateDsp dsp;
        dsp.prepare (48000.0);
        dsp.snapToTargets (targets);

        std::mt19937 random (8);
        std::uniform_real_distribution<float> noise (-0.25f, 0.25f);
        std::vector<float> source ((size_t) blockSize * 64);

        for (auto& sample : source)
            sample = noise (random);

        float left[blockSize], right[blockSize];
        float* channels[] { left, right };
        std::chrono::duration<double> spent {};

        for (int block = 0; block < blocks; ++block)
        {
            std::copy_n (source.begin() + (block % 64) * blockSize, blockSize, left);
            std::copy_n (source.begin() + (block % 64) * blockSize, blockSize, right);
            const auto start = std::chrono::steady_clock::now();
            dsp.process (channels, 2, nullptr, 0, blockSize, nullptr);
            spent += std::chrono::steady_clock::now() - start;
        }

        return 100.0 * spent.count() / ((double) (blocks * blockSize) / 48000.0);
    };

    const auto median = [&] (int bands)
    {
        std::vector<double> values;

        for (int run = 0; run < runs; ++run)
            values.push_back (measure (bands));

        std::sort (values.begin(), values.end());
        return values[runs / 2];
    };

    const auto comp = fancytest::compDefaultCpu();
    const auto x = [comp] (double percent) { return fancytest::timesComp (percent, comp); };
    const double cost[] { median (1), median (2), median (3) };

    std::printf ("gate w3 cpu (median of %d, 48 kHz, 128 samples, stereo, defaults): Fancy Comp default %.3f %%; 1 band %.3f %%%s; "
                 "2 bands +%.3f %%%s; 3 bands +%.3f %%%s (target: three bands add under 2 x Comp)\n",
                 runs, comp, cost[0], x (cost[0]).c_str(), cost[1] - cost[0], x (cost[1] - cost[0]).c_str(),
                 cost[2] - cost[0], x (cost[2] - cost[0]).c_str());
    SUCCEED();
}
