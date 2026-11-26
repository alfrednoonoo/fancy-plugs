// Phase 4 Wave 3 for Fancy Verb: Freeze and Shimmer (docs/verb.md, "Phase 4 Wave 3: Freeze and
// shimmer", section 6). Every limit was run first through docs/research/verb-w3-prepush.py, on the
// same IRs: the noise here is the model's portable generator (xorshift32 and Box-Muller), not the
// standard library's, so both make the same samples.

#include "CompCpuReference.h"
#include "IrBuilder.h"
#include "LegacyVerbW2Dsp.h"
#include "VerbDsp.h"
#include "VerbProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = std::numbers::pi;
constexpr int second = 48000;
using Mode = verb::Convolver::Mode;

std::vector<double> portableNoise (int n, std::uint32_t seed)
{
    std::uint32_t x = 0x12345678u ^ (seed * 0x9E3779B1u);

    if (x == 0)
        x = 1;

    const auto next = [&x]
    {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        return ((double) x + 0.5) / 4294967296.0;
    };

    std::vector<double> out ((size_t) n);

    for (auto& value : out)
    {
        const auto a = next(), b = next();
        value = std::sqrt (-2.0 * std::log (a)) * std::cos (2.0 * pi * b);
    }

    return out;
}

std::vector<float> decayingNoise (double seconds, double rt, std::uint32_t seed, double scale = 0.05)
{
    const auto n = (int) (seconds * rate);
    const auto noise = portableNoise (n, seed);
    std::vector<float> ir ((size_t) n);

    for (int i = 0; i < n; ++i)
        ir[(size_t) i] = (float) (scale * noise[(size_t) i] * std::exp (-6.907755 * (double) i / rate / rt));

    return ir;
}

struct Irs
{
    std::vector<float> left, right;
};

// The model's hall: left and right correlated as halls are.
Irs hall()
{
    const auto a = decayingNoise (3.5, 2.5, 1), b = decayingNoise (3.5, 2.5, 2);
    Irs irs { a, a };

    for (size_t i = 0; i < a.size(); ++i)
        irs.right[i] = (float) (0.5 * (double) a[i] + std::sqrt (0.75) * (double) b[i]);

    return irs;
}

Irs mono (std::vector<float> ir)
{
    return { ir, ir };
}

std::vector<float> chord (int n)
{
    std::vector<float> x ((size_t) n, 0.0f);

    for (int i = 0; i < (int) (1.2 * rate) && i < n; ++i)
    {
        const auto t = (double) i / rate;
        auto sum = 0.0;

        for (const auto f0 : { 196.0, 246.9, 293.7 })
            for (int k = 1; k <= 5; ++k)
                sum += std::sin (2.0 * pi * f0 * k * t) / k;

        x[(size_t) i] = (float) (sum * std::min (1.0, t / 0.01) * std::exp (-t / 0.8) * 0.1);
    }

    return x;
}

std::vector<float> note (int n, double hz = 220.0, double seconds = 0.4, double amplitude = 0.5)
{
    std::vector<float> x ((size_t) n, 0.0f);

    for (int i = 0; i < (int) (seconds * rate) && i < n; ++i)
    {
        const auto t = (double) i / rate;
        x[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * hz * t) * std::min (1.0, t / 0.01) * std::min (1.0, (seconds - t) / 0.02));
    }

    return x;
}

// What VerbProcessor gives a convolver (VerbProcessor.cpp, loopInfoOf): the busier output's energy
// and the broadband RT60.
std::pair<double, double> loopInfo (const Irs& irs)
{
    auto left = 0.0, right = 0.0;
    std::vector<double> envelope (irs.left.size());

    for (size_t i = 0; i < irs.left.size(); ++i)
    {
        left += (double) irs.left[i] * irs.left[i];
        right += (double) irs.right[i] * irs.right[i];
        envelope[i] = (double) irs.left[i] * irs.left[i] + (double) irs.right[i] * irs.right[i];
    }

    return { std::max (left, right), verb::schroederRt60 (envelope, rate) };
}

struct Render
{
    std::vector<float> left, right;
    double rt60 = 0.0;
};

// Wet only (Wet 1, Dry 0), offline. Freeze is on from freezeAt to freezeOff (samples, set at block
// starts, so multiples of the block size).
Render render (const Irs& irs, const std::vector<float>& input, VerbDsp::Targets targets, int blockSize = 512, Mode mode = Mode::zeroLatency,
               int freezeAt = -1, int freezeOff = -1)
{
    auto convolver = std::make_unique<verb::Convolver> (irs.left.data(), irs.right.data(), (int) irs.left.size(), mode, 8192);
    const auto [energy, rt60] = loopInfo (irs);
    convolver->setLoopInfo (energy, rt60);

    VerbDsp dsp;
    dsp.prepare (rate, verb::Convolver::latencyOf (mode));
    dsp.offer (std::move (convolver));

    targets.wet = 1.0;
    targets.dry = 0.0;
    dsp.snapToTargets (targets);

    Render out { input, input, rt60 };

    for (int start = 0; start < (int) input.size(); start += blockSize)
    {
        const auto count = std::min (blockSize, (int) input.size() - start);
        targets.freeze = freezeAt >= 0 && start >= freezeAt && (freezeOff < 0 || start < freezeOff);
        dsp.setTargets (targets);

        float* channels[] { out.left.data() + start, out.right.data() + start };
        dsp.process (channels, 2, count, true);
    }

    return out;
}

VerbDsp::Targets shimmer (double amount, double semitones)
{
    VerbDsp::Targets targets;
    targets.shimmer = amount;
    targets.shimmerRatio = std::exp2 (semitones / 12.0);
    return targets;
}

// 50 ms RMS frames, in dB.
std::vector<double> envelopeDb (const std::vector<float>& x, size_t from, size_t to, double ms = 50.0)
{
    const auto frame = (size_t) (ms * 0.001 * rate);
    std::vector<double> out;

    for (auto start = from; start + frame <= std::min (to, x.size()); start += frame)
    {
        auto sum = 0.0;

        for (auto i = start; i < start + frame; ++i)
            sum += (double) x[i] * x[i];

        out.push_back (10.0 * std::log10 (std::max (sum / (double) frame, 1.0e-30)));
    }

    return out;
}

double percentile (std::vector<double> values, double p)
{
    std::sort (values.begin(), values.end());
    const auto position = p / 100.0 * (double) (values.size() - 1);
    const auto low = (size_t) position;
    const auto high = std::min (low + 1, values.size() - 1);
    return values[low] + (values[high] - values[low]) * (position - (double) low);
}

// The power spectrum of x[from, to) under a symmetric Hann window, zero-padded to a power of two
// (the model pads the same way).
std::vector<double> powerSpectrum (const std::vector<float>& x, size_t from, size_t to, int& fftSize)
{
    const auto n = (int) (to - from);
    auto order = 0;

    while ((1 << order) < n)
        ++order;

    fftSize = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> data ((size_t) (2 * fftSize), 0.0f);

    for (int i = 0; i < n; ++i)
        data[(size_t) i] = x[from + (size_t) i] * (float) (0.5 - 0.5 * std::cos (2.0 * pi * (double) i / (double) (n - 1)));

    fft.performRealOnlyForwardTransform (data.data(), true);

    std::vector<double> power ((size_t) (fftSize / 2 + 1));

    for (size_t k = 0; k < power.size(); ++k)
        power[k] = (double) data[2 * k] * data[2 * k] + (double) data[2 * k + 1] * data[2 * k + 1];

    return power;
}

// Third octaves from 31 Hz to 16 kHz, each as a share of the total, in dB.
std::vector<double> thirdOctaves (const std::vector<float>& x, size_t from, size_t to)
{
    int fftSize = 0;
    const auto power = powerSpectrum (x, from, to, fftSize);
    std::vector<double> bands;

    for (int b = -15; b <= 12; ++b)
    {
        const auto centre = 1000.0 * std::exp2 ((double) b / 3.0);
        auto sum = 0.0;

        for (size_t k = 0; k < power.size(); ++k)
        {
            const auto hz = (double) k * rate / (double) fftSize;

            if (hz >= centre * std::exp2 (-1.0 / 6.0) && hz < centre * std::exp2 (1.0 / 6.0))
                sum += power[k];
        }

        bands.push_back (10.0 * std::log10 (std::max (sum, 1.0e-30)));
    }

    auto total = 0.0;

    for (const auto band : bands)
        total += std::pow (10.0, band / 10.0);

    for (auto& band : bands)
        band -= 10.0 * std::log10 (total);

    return bands;
}

double correlation (const std::vector<float>& a, const std::vector<float>& b, size_t from, size_t to)
{
    auto meanA = 0.0, meanB = 0.0;

    for (auto i = from; i < to; ++i)
    {
        meanA += a[i];
        meanB += b[i];
    }

    meanA /= (double) (to - from);
    meanB /= (double) (to - from);
    auto ab = 0.0, aa = 0.0, bb = 0.0;

    for (auto i = from; i < to; ++i)
    {
        ab += ((double) a[i] - meanA) * ((double) b[i] - meanB);
        aa += ((double) a[i] - meanA) * ((double) a[i] - meanA);
        bb += ((double) b[i] - meanB) * ((double) b[i] - meanB);
    }

    return ab / std::sqrt (aa * bb + 1.0e-30);
}

bool finite (const Render& r)
{
    const auto ok = [] (float v) { return std::isfinite (v); };
    return std::all_of (r.left.begin(), r.left.end(), ok) && std::all_of (r.right.begin(), r.right.end(), ok);
}

void setParameter (fancy::FancyProcessor& processor, const char* id, float value)
{
    auto& parameter = *processor.getApvts().getParameter (id);
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
}
} // namespace

// 1. Legacy -----------------------------------------------------------------------------------

TEST_CASE ("Verb W3: Freeze off and Shimmer 0 render exactly as Verb v1.7.1", "[verb][w3]")
{
    const auto mode = GENERATE (Mode::zeroLatency, Mode::lowCpu);
    const auto blockSize = GENERATE (7, 512, 3000);
    const auto variant = GENERATE (0, 1);

    const auto irs = mono (decayingNoise (1.0, 0.8, 5));
    const auto right = decayingNoise (1.0, 0.8, 6);

    std::vector<float> input ((size_t) (2 * second), 0.0f);
    const auto noise = portableNoise ((int) input.size(), 9);

    for (size_t i = 0; i < input.size(); ++i)
        input[i] = (i / 12000) % 2 == 0 ? (float) (0.2 * noise[i]) : 0.0f;

    VerbDsp::Targets current;
    LegacyVerbW2Dsp::Targets old;

    const auto set = [&] (auto& t)
    {
        t.predelayMs = 12.0;
        t.center = 0.2;
        t.width = variant == 0 ? 1.0 : 1.6; // Extra width
        t.wet = 0.7;
        t.dry = 0.5;
        t.output = 0.9;
        t.ducking = variant == 0 ? 0.0 : 0.6;
        t.gate = variant == 1;
        t.gateThresholdDb = -24.0;
    };

    set (current);
    set (old);

    VerbDsp dsp;
    LegacyVerbW2Dsp legacy;
    dsp.prepare (rate, verb::Convolver::latencyOf (mode));
    legacy.prepare (rate, verb::Convolver::latencyOf (mode));
    dsp.offer (std::make_unique<verb::Convolver> (irs.left.data(), right.data(), (int) right.size(), mode, 8192));
    legacy.offer (std::make_unique<verb::Convolver> (irs.left.data(), right.data(), (int) right.size(), mode, 8192));
    dsp.snapToTargets (current);
    legacy.snapToTargets (old);

    auto a = input, b = input, c = input, d = input;

    for (int start = 0; start < (int) input.size(); start += blockSize)
    {
        const auto count = std::min (blockSize, (int) input.size() - start);
        dsp.setTargets (current);
        legacy.setTargets (old);
        float* now[] { a.data() + start, b.data() + start };
        float* before[] { c.data() + start, d.data() + start };
        dsp.process (now, 2, count, true);
        legacy.process (before, 2, count, true);
    }

    CAPTURE ((int) mode, blockSize, variant);
    CHECK (a == c);
    CHECK (b == d);
}

TEST_CASE ("Verb W3: Freeze, Shimmer and Interval are appended, and older states load them off", "[verb][w3]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;
    const auto& parameters = processor.getParameters();
    const auto idOf = [&] (int index) { return dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[index])->paramID; };

    const auto count = parameters.size();
    REQUIRE (count >= 4);
    CHECK (idOf (count - 4) == "extraWidth");
    CHECK (idOf (count - 3) == "freeze");
    CHECK (idOf (count - 2) == "shimmer");
    CHECK (idOf (count - 1) == "shimmerPitch");

    setParameter (processor, "freeze", 1.0f);
    setParameter (processor, "shimmer", 60.0f);
    setParameter (processor, "shimmerPitch", 3.0f);

    juce::MemoryBlock state;
    processor.getStateInformation (state);
    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    REQUIRE (xml != nullptr);

    for (auto* child = xml->getFirstChildElement(); child != nullptr;)
    {
        auto* next = child->getNextElement();
        const auto id = child->getStringAttribute ("id");

        if (child->hasTagName ("PARAM") && (id == "freeze" || id == "shimmer" || id == "shimmerPitch"))
            xml->removeChildElement (child, true);

        child = next;
    }

    juce::MemoryBlock legacy;
    juce::AudioProcessor::copyXmlToBinary (*xml, legacy);

    VerbProcessor restored;
    restored.setStateInformation (legacy.getData(), (int) legacy.getSize());
    CHECK (restored.getApvts().getRawParameterValue ("freeze")->load() == 0.0f);
    CHECK (restored.getApvts().getRawParameterValue ("shimmer")->load() == 0.0f);
    CHECK (restored.getApvts().getRawParameterValue ("shimmerPitch")->load() == 0.0f);
}

// 2. Freeze -----------------------------------------------------------------------------------

TEST_CASE ("Verb W3: Freeze holds the captured spectrum and image without repeating", "[verb][w3]")
{
    const auto irs = hall();
    const auto input = chord (12 * second);
    const auto at = (int) (1.5 * rate) / 1024 * 1024;
    const auto out = render (irs, input, {}, 512, Mode::zeroLatency, at);
    REQUIRE (finite (out));

    const auto captureFrom = (size_t) (at - (int) (0.25 * rate));
    const auto frozenFrom = (size_t) (at + second / 2), frozenTo = frozenFrom + (size_t) (10 * second);

    // Third octaves within 20 dB of the strongest (the model: 2.00 dB; 1.6 to 3.7 dB over four IR
    // draws, because a 250 ms reference is itself a rough estimate; docs/verb.md, As built).
    const auto frozen = thirdOctaves (out.left, frozenFrom, frozenTo);
    const auto captured = thirdOctaves (out.left, captureFrom, (size_t) at);
    const auto strongest = *std::max_element (captured.begin(), captured.end());
    auto worst = 0.0;

    for (size_t b = 0; b < captured.size(); ++b)
        if (captured[b] > strongest - 20.0)
            worst = std::max (worst, std::abs (frozen[b] - captured[b]));

    // The 50 ms level wanders as a steady noisy tail does (the model: 3.33 dB).
    const auto envelope = envelopeDb (out.left, frozenFrom, frozenTo);
    const auto wander = percentile (envelope, 95.0) - percentile (envelope, 5.0);

    // No repetition of the envelope at lags of 0.2 to 5 s (the model: 0.14).
    std::vector<double> linear;

    for (const auto value : envelope)
        linear.push_back (std::pow (10.0, value / 20.0));

    auto mean = 0.0;

    for (const auto value : linear)
        mean += value / (double) linear.size();

    for (auto& value : linear)
        value -= mean;

    const auto lagSum = [&] (size_t lag)
    {
        auto sum = 0.0;

        for (size_t i = 0; i + lag < linear.size(); ++i)
            sum += linear[i] * linear[i + lag];

        return sum;
    };

    const auto zero = lagSum (0);
    auto repeats = -1.0;

    for (size_t lag = 4; lag <= 100; ++lag)
        repeats = std::max (repeats, lagSum (lag) / zero);

    // The image: left and right as correlated as they were (the model: +0.19 against +0.11).
    const auto before = correlation (out.left, out.right, captureFrom, (size_t) at);
    const auto after = correlation (out.left, out.right, frozenFrom, frozenTo);

    CAPTURE (worst, wander, repeats, before, after);
    CHECK (worst < 4.0);
    CHECK (wander < 4.0);
    CHECK (repeats < 0.3);
    CHECK (std::abs (after - before) < 0.15);
}

TEST_CASE ("Verb W3: a tone played while frozen doesn't reach the reverb", "[verb][w3]")
{
    const auto irs = hall();
    auto input = chord (6 * second);
    const auto at = (int) (1.5 * rate) / 1024 * 1024;
    const auto plain = render (irs, input, {}, 512, Mode::zeroLatency, at);

    for (int i = at + second / 2; i < 6 * second; ++i)
        input[(size_t) i] += (float) (0.3 * std::sin (2.0 * pi * 3100.0 * (double) i / rate));

    const auto withTone = render (irs, input, {}, 512, Mode::zeroLatency, at);

    // The input is muted 20 ms after Freeze, so the tone adds nothing at all.
    auto difference = 0.0, level = 0.0;

    for (size_t i = (size_t) (at + second / 2); i < plain.left.size(); ++i)
    {
        difference += std::pow ((double) withTone.left[i] - plain.left[i], 2.0);
        level += (double) plain.left[i] * plain.left[i];
    }

    CHECK (difference <= 1.0e-6 * level);
}

// 3. Release ----------------------------------------------------------------------------------

TEST_CASE ("Verb W3: after Freeze, the held sound dies away at the IR's RT60", "[verb][w3]")
{
    const auto irs = hall();
    const auto at = (int) (1.5 * rate) / 1024 * 1024, off = (int) (4.0 * rate) / 1024 * 1024;
    const auto out = render (irs, chord (9 * second), {}, 512, Mode::zeroLatency, at, off);

    const auto envelope = envelopeDb (out.left, (size_t) off, out.left.size());
    std::vector<double> t, y;

    for (size_t i = 0; i < envelope.size(); ++i)
    {
        const auto seconds = 0.05 * (double) i;

        if (seconds >= 0.05 && seconds <= 0.5 * out.rt60)
        {
            t.push_back (seconds);
            y.push_back (envelope[i]);
        }
    }

    REQUIRE (t.size() > 10);
    auto mt = 0.0, my = 0.0;

    for (size_t i = 0; i < t.size(); ++i)
    {
        mt += t[i] / (double) t.size();
        my += y[i] / (double) y.size();
    }

    auto sxy = 0.0, sxx = 0.0;

    for (size_t i = 0; i < t.size(); ++i)
    {
        sxy += (t[i] - mt) * (y[i] - my);
        sxx += (t[i] - mt) * (t[i] - mt);
    }

    const auto measured = -60.0 / (sxy / sxx);

    // The model: 2.509 s against 2.494 s.
    CAPTURE (measured, out.rt60);
    CHECK (std::abs (measured / out.rt60 - 1.0) < 0.1);
}

// 4. Shimmer ----------------------------------------------------------------------------------

TEST_CASE ("Verb W3: Shimmer +12 at 100 % stacks octaves and still dies away", "[verb][w3]")
{
    const auto irs = mono (decayingNoise (3.5, 2.5, 1));
    const auto out = render (irs, note (10 * second), shimmer (1.0, 12.0));
    REQUIRE (finite (out));

    int fftSize = 0;
    const auto power = powerSpectrum (out.left, (size_t) (2 * second), (size_t) (2.5 * rate), fftSize);

    const auto levelAt = [&] (double hz)
    {
        auto sum = 0.0;

        for (size_t k = 0; k < power.size(); ++k)
            if (std::abs ((double) k * rate / fftSize - hz) < hz * 0.03)
                sum += power[k];

        return 10.0 * std::log10 (std::max (sum, 1.0e-30));
    };

    auto strongest = -1000.0;

    for (const auto hz : { 220.0, 440.0, 880.0, 1760.0, 3520.0, 7040.0, 14080.0 })
        strongest = std::max (strongest, levelAt (hz));

    // The model, at 2 s: 880 -5.7, 1760 -1.1, 3520 0 dB.
    for (const auto hz : { 880.0, 1760.0, 3520.0 })
    {
        CAPTURE (hz, levelAt (hz) - strongest);
        CHECK (levelAt (hz) > strongest - 30.0);
    }

    const auto envelope = envelopeDb (out.left, 0, out.left.size());
    CHECK (envelope.back() < *std::max_element (envelope.begin(), envelope.end()) - 60.0);
}

// 5. Determinism ------------------------------------------------------------------------------

TEST_CASE ("Verb W3: Shimmer renders bit-identically at any block size", "[verb][w3]")
{
    const auto mode = GENERATE (Mode::zeroLatency, Mode::lowCpu);
    const auto irs = mono (decayingNoise (1.5, 1.2, 3));
    const auto input = note (3 * second);
    const auto reference = render (irs, input, shimmer (0.75, 12.0), 64, mode);
    REQUIRE (finite (reference));

    for (const auto blockSize : { 1, 1000, 8192 })
    {
        const auto out = render (irs, input, shimmer (0.75, 12.0), blockSize, mode);
        CAPTURE ((int) mode, blockSize);
        CHECK (out.left == reference.left);
        CHECK (out.right == reference.right);
    }
}

// 6. Folding ----------------------------------------------------------------------------------

TEST_CASE ("Verb W3: Two octaves up on a 6 kHz tone folds nothing down", "[verb][w3]")
{
    const auto irs = mono (decayingNoise (3.5, 2.5, 1));
    std::vector<float> input ((size_t) (3 * second));

    for (size_t i = 0; i < input.size(); ++i)
    {
        const auto t = (double) i / rate;
        input[i] = (float) (0.25 * std::sin (2.0 * pi * 6000.0 * t) * std::min (1.0, t / 0.01));
    }

    const auto out = render (irs, input, shimmer (1.0, 24.0));
    REQUIRE (finite (out));

    int fftSize = 0;
    const auto power = powerSpectrum (out.left, (size_t) (2 * second), (size_t) (3 * second), fftSize);
    auto tone = 0.0, below = 0.0;

    for (size_t k = 0; k < power.size(); ++k)
    {
        const auto hz = (double) k * rate / fftSize;

        if (std::abs (hz - 6000.0) < 50.0)
            tone = std::max (tone, power[k]);
        else if (hz > 20.0 && hz < 5000.0)
            below = std::max (below, power[k]);
    }

    // The model: -152.5 dB.
    CAPTURE (10.0 * std::log10 (below / tone));
    CHECK (10.0 * std::log10 (below / tone) < -80.0);
}

// 7. Guard ------------------------------------------------------------------------------------

TEST_CASE ("Verb W3: the loop guard keeps Metal Ping's shimmer near the note's own level", "[verb][w3]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto loudest = [] (float amount, double hz)
    {
        VerbProcessor processor;
        const auto uid = processor.addModule (verb::ModuleType::file, 0);
        REQUIRE (processor.loadLibrary (uid, "Special/Metal Ping"));
        setParameter (processor, "wet", 0.0f);
        setParameter (processor, "dry", -60.0f);
        setParameter (processor, "shimmer", amount);
        setParameter (processor, "shimmerPitch", 1.0f); // Fifth up
        processor.setNonRealtime (true);
        processor.prepareToPlay (rate, 512);
        REQUIRE (processor.waitForBuild (20000));

        const auto input = note (6 * second, hz);
        juce::AudioBuffer<float> buffer (2, (int) input.size());
        buffer.copyFrom (0, 0, input.data(), (int) input.size());
        buffer.copyFrom (1, 0, input.data(), (int) input.size());
        juce::MidiBuffer midi;

        for (int start = 0; start < buffer.getNumSamples(); start += 512)
        {
            juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 2, start, std::min (512, buffer.getNumSamples() - start));
            processor.processBlock (view, midi);
        }

        std::vector<float> left (buffer.getReadPointer (0), buffer.getReadPointer (0) + buffer.getNumSamples());
        const auto envelope = envelopeDb (left, 0, left.size());
        return *std::max_element (envelope.begin(), envelope.end());
    };

    // The model (a stand-in for Metal Ping): +0.0 dB on every note.
    for (const auto hz : { 196.0, 220.0, 246.9, 261.6, 329.6, 392.0 })
    {
        const auto rise = loudest (100.0f, hz) - loudest (0.0f, hz);
        CAPTURE (hz, rise);
        CHECK (rise < 10.0);
    }
}

// 8. Freeze with Shimmer ----------------------------------------------------------------------

TEST_CASE ("Verb W3: freezing mutes Shimmer's loop and the held level stays put", "[verb][w3]")
{
    const auto irs = hall();
    const auto at = (int) (1.5 * rate) / 1024 * 1024;
    const auto out = render (irs, chord (13 * second), shimmer (0.75, 12.0), 512, Mode::zeroLatency, at);
    REQUIRE (finite (out));

    std::vector<double> levels;

    for (int k = 1; k <= 10; ++k)
    {
        auto sum = 0.0;

        for (int i = at + k * second; i < at + (k + 1) * second; ++i)
            sum += (double) out.left[(size_t) i] * out.left[(size_t) i];

        levels.push_back (10.0 * std::log10 (sum / second));
    }

    // The model: 0.58 dB.
    const auto span = *std::max_element (levels.begin(), levels.end()) - *std::min_element (levels.begin(), levels.end());
    CAPTURE (span);
    CHECK (span < 2.0);
}

// 9. CPU --------------------------------------------------------------------------------------

TEST_CASE ("verb w3 cpu measurement", "[.measure]")
{
    constexpr int blockSize = 128, blocks = 6000, runs = 5;
    const auto irs = mono (decayingNoise (3.0, 2.5, 7));

    const auto measure = [&] (double amount, bool frozen)
    {
        VerbDsp dsp;
        dsp.prepare (rate, 0);
        auto convolver = std::make_unique<verb::Convolver> (irs.left.data(), irs.right.data(), (int) irs.left.size(), Mode::zeroLatency, 8192);
        const auto [energy, rt60] = loopInfo (irs);
        convolver->setLoopInfo (energy, rt60);
        dsp.offer (std::move (convolver));

        auto targets = shimmer (amount, 12.0);
        dsp.snapToTargets (targets);

        float left[blockSize], right[blockSize];
        float* channels[] { left, right };
        const auto noise = portableNoise (blockSize * 64, 4);
        std::chrono::duration<double> spent {};

        for (int block = 0; block < blocks; ++block)
        {
            for (int i = 0; i < blockSize; ++i)
                left[i] = right[i] = (float) (0.2 * noise[(size_t) ((block % 64) * blockSize + i)]);

            // Frozen from the second second on, as a pad would be.
            targets.freeze = frozen && block * blockSize >= second;
            const auto start = std::chrono::steady_clock::now();
            dsp.setTargets (targets);
            dsp.process (channels, 2, blockSize, false);

            if (block * blockSize >= second)
                spent += std::chrono::steady_clock::now() - start;
        }

        return 100.0 * spent.count() / ((double) (blocks * blockSize - second) / rate);
    };

    const auto median = [&] (double amount, bool frozen)
    {
        std::vector<double> values;

        for (int run = 0; run < runs; ++run)
            values.push_back (measure (amount, frozen));

        std::sort (values.begin(), values.end());
        return values[runs / 2];
    };

    const auto comp = fancytest::compDefaultCpu();
    const auto plain = median (0.0, false), shimmered = median (0.5, false), frozen = median (0.0, true);
    const auto x = [comp] (double percent) { return fancytest::timesComp (percent, comp); };

    std::printf ("verb w3 cpu (median of %d, 48 kHz, 128 samples, stereo, 3 s IR, zero latency): Fancy Comp default %.3f %%; plain %.3f %%%s; "
                 "Shimmer 50 %% +12 st %.3f %%%s (+%.3f %%%s, target < 0.5); frozen %.3f %%%s (+%.3f %%%s, target < 0.3 Windows, < 0.4 Mac)\n",
                 runs, comp, plain, x (plain).c_str(), shimmered, x (shimmered).c_str(), shimmered - plain, x (shimmered - plain).c_str(),
                 frozen, x (frozen).c_str(), frozen - plain, x (frozen - plain).c_str());
    SUCCEED();
}
