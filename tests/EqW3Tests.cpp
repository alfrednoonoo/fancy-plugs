// Phase 4 Wave 3 for Fancy EQ: spectral dynamics (docs/eq.md, "Phase 4 Wave 3: spectral dynamics",
// section 6). The limits were run first through docs/research/eq-spectral-prepush.py, a port of
// eq::SpectralStage; test 8 (FIR on the shared framing) is in StftFramesTests.cpp.

#include "CompCpuReference.h"
#include "EqDsp.h"
#include "EqProcessor.h"
#include "LegacyEqW2Dsp.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <random>
#include <vector>

namespace
{
constexpr double pi = std::numbers::pi;

struct Stereo
{
    std::vector<float> left, right;
};

// Pink noise at an RMS level (Paul Kellet's filter on seeded white noise).
std::vector<float> pink (int n, double rmsDb, unsigned seed)
{
    std::mt19937 random (seed);
    std::normal_distribution<double> white (0.0, 1.0);
    std::vector<double> x ((size_t) n);
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;

    for (auto& value : x)
    {
        const auto w = white (random);
        b0 = 0.99886 * b0 + w * 0.0555179;
        b1 = 0.99332 * b1 + w * 0.0750759;
        b2 = 0.96900 * b2 + w * 0.1538520;
        b3 = 0.86650 * b3 + w * 0.3104856;
        b4 = 0.55000 * b4 + w * 0.5329522;
        b5 = -0.7616 * b5 - w * 0.0168980;
        value = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362;
        b6 = w * 0.115926;
    }

    auto sum = 0.0;

    for (const auto value : x)
        sum += value * value;

    const auto scale = std::pow (10.0, rmsDb / 20.0) / std::sqrt (sum / (double) n);
    std::vector<float> out ((size_t) n);

    for (size_t i = 0; i < out.size(); ++i)
        out[i] = (float) (x[i] * scale);

    return out;
}

std::vector<float> sine (int n, double hz, double rmsDb, double fromSeconds = 0.0, double rate = 48000.0)
{
    std::vector<float> x ((size_t) n, 0.0f);
    const auto amplitude = std::sqrt (2.0) * std::pow (10.0, rmsDb / 20.0);

    for (int i = (int) (fromSeconds * rate); i < n; ++i)
        x[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * hz * (double) i / rate));

    return x;
}

eq::BandSpec bell (double hz, double q = 1.0)
{
    eq::BandSpec spec;
    spec.type = eq::BandType::band;
    spec.frequencyHz = hz;
    spec.gainDb = 0.0;
    spec.q = q;
    spec.on = true;
    return spec;
}

eq::DynamicSpec dynamics (double thresholdDb, double rangeDb, bool spectral, double attackMs = 5.0, double releaseMs = 80.0)
{
    eq::DynamicSpec spec;
    spec.on = true;
    spec.thresholdDb = thresholdDb;
    spec.rangeDb = rangeDb;
    spec.attackMs = attackMs;
    spec.releaseMs = releaseMs;
    spec.spectral = spectral;
    return spec;
}

template <typename Dsp>
Stereo run (Dsp& dsp, const Stereo& input, int blockSize = 256, const Stereo* key = nullptr)
{
    Stereo out = input;

    for (int start = 0; start < (int) input.left.size(); start += blockSize)
    {
        const auto count = std::min (blockSize, (int) input.left.size() - start);
        float* channels[] { out.left.data() + start, out.right.data() + start };

        if (key != nullptr)
        {
            const float* sidechain[] { key->left.data() + start, key->right.data() + start };
            dsp.process (channels, 2, count, sidechain, 2);
        }
        else
        {
            dsp.process (channels, 2, count);
        }
    }

    return out;
}

Stereo both (const std::vector<float>& x)
{
    return { x, x };
}

// The power in [lo, hi) Hz of x[from, to), under a Hann window, zero-padded to a power of two.
double bandDb (const std::vector<float>& x, size_t from, size_t to, double lo, double hi, double rate = 48000.0)
{
    const auto n = (int) (to - from);
    auto order = 0;

    while ((1 << order) < n)
        ++order;

    const auto size = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> data ((size_t) (2 * size), 0.0f);

    for (int i = 0; i < n; ++i)
        data[(size_t) i] = x[from + (size_t) i] * (float) (0.5 - 0.5 * std::cos (2.0 * pi * (double) i / (double) (n - 1)));

    fft.performRealOnlyForwardTransform (data.data(), true);
    auto sum = 0.0;

    for (int k = 0; k <= size / 2; ++k)
    {
        const auto hz = (double) k * rate / (double) size;

        if (hz >= lo && hz < hi)
            sum += (double) data[(size_t) (2 * k)] * data[(size_t) (2 * k)] + (double) data[(size_t) (2 * k + 1)] * data[(size_t) (2 * k + 1)];
    }

    return 10.0 * std::log10 (std::max (sum, 1.0e-30));
}

double rmsDb (const std::vector<float>& x, size_t from, size_t to)
{
    auto sum = 0.0;

    for (auto i = from; i < to; ++i)
        sum += (double) x[i] * x[i];

    return 10.0 * std::log10 (std::max (sum / (double) (to - from), 1.0e-30));
}

// The model's signal: pink noise at -24 dBFS and a 2.5 kHz tone at -14 dBFS, on 0.5 s, off 0.5 s.
std::vector<float> ringingMix (int n)
{
    auto x = pink (n, -24.0, 9);

    for (int i = 0; i < n; ++i)
    {
        const auto t = (double) i / 48000.0;
        const auto phase = std::fmod (t, 1.0);
        const auto ramp = std::clamp (std::min (phase, 0.5 - phase) / 0.001, 0.0, 1.0);
        x[(size_t) i] += (float) (std::pow (10.0, -14.0 / 20.0) * std::sqrt (2.0) * std::sin (2.0 * pi * 2500.0 * t) * (phase < 0.5 ? ramp : 0.0));
    }

    return x;
}
} // namespace

// 1. Legacy -----------------------------------------------------------------------------------

TEST_CASE ("EQ W3: with no spectral band, the EQ renders exactly as v1.7.1, with no latency", "[eq][w3]")
{
    const auto linear = GENERATE (false, true);

    EqDsp now;
    LegacyEqW2Dsp old;
    EqDsp::Targets a;
    LegacyEqW2Dsp::Targets b;

    const auto set = [&] (auto& t)
    {
        const double hz[] { 80.0, 200.0, 450.0, 900.0, 1800.0, 3500.0, 7000.0, 12000.0 };

        for (int band = 0; band < 8; ++band)
        {
            t.bands[(size_t) band] = bell (hz[band], 1.2);
            t.bands[(size_t) band].gainDb = band % 2 == 0 ? 3.0 : -2.0;
            t.dynamics[(size_t) band] = dynamics (-30.0, band % 2 == 0 ? -6.0 : 4.0, false);
        }

        t.bands[2].channel = eq::Channel::mid;
        t.bands[5].channel = eq::Channel::side;
        t.linearPhase = linear;
    };

    set (a);
    set (b);

    now.setSynchronousDesign (true);
    old.setSynchronousDesign (true);
    now.prepare (48000.0, 2);
    old.prepare (48000.0, 2);
    now.snapToTargets (a);
    old.snapToTargets (b);
    now.setTargets (a);
    old.setTargets (b);

    const auto input = Stereo { pink (48000, -18.0, 1), pink (48000, -20.0, 2) };
    const auto x = run (now, input), y = run (old, input);

    CAPTURE (linear);
    CHECK (x.left == y.left);
    CHECK (x.right == y.right);
    CHECK (now.getLatencySamples() == old.getLatencySamples());

    if (! linear)
        CHECK (now.getLatencySamples() == 0);
}

// 2. Selective --------------------------------------------------------------------------------

TEST_CASE ("EQ W3: a spectral band cuts the resonance and leaves the noise round it", "[eq][w3]")
{
    const auto n = 3 * 48000;
    const auto input = both (ringingMix (n));

    const auto render = [&] (bool spectral)
    {
        EqDsp dsp;
        EqDsp::Targets targets;
        targets.bands[0] = bell (2500.0, 1.0);
        targets.dynamics[0] = dynamics (-30.0, -9.0, spectral);
        dsp.prepare (48000.0, 2);
        dsp.snapToTargets (targets);
        dsp.setTargets (targets);
        const auto out = run (dsp, input);
        return std::pair { out, dsp.getLatencySamples() };
    };

    const auto [spectralOut, latency] = render (true);
    const auto [todayOut, noLatency] = render (false);
    REQUIRE (latency == 1024);
    REQUIRE (noLatency == 0);

    const auto from = (size_t) (1.15 * 48000), to = (size_t) (1.5 * 48000);

    const auto change = [&] (const Stereo& out, int delay, double lo, double hi)
    {
        return bandDb (out.left, from + (size_t) delay, to + (size_t) delay, lo, hi) - bandDb (input.left, from, to, lo, hi);
    };

    // The model: tone -8.98 dB, noise -0.00 and +0.00 dB; today's band -4.1 dB at 1.5 kHz.
    const auto tone = change (spectralOut, latency, 2425.0, 2575.0);
    const auto at1500 = change (spectralOut, latency, 1400.0, 1600.0);
    const auto at4000 = change (spectralOut, latency, 3800.0, 4200.0);
    const auto today = change (todayOut, 0, 1400.0, 1600.0);

    CAPTURE (tone, at1500, at4000, today);
    CHECK (std::abs (tone + 9.0) < 0.5);
    CHECK (std::abs (at1500) < 0.5);
    CHECK (std::abs (at4000) < 0.5);
    CHECK (today < -3.0);
}

// 3. Calibration ------------------------------------------------------------------------------

TEST_CASE ("EQ W3: a sine 12 dB over Threshold gets the full Range, as today's band does", "[eq][w3]")
{
    EqDsp dsp;
    EqDsp::Targets targets;
    targets.bands[0] = bell (1000.0, 1.0);
    targets.dynamics[0] = dynamics (-30.0, -6.0, true);
    dsp.prepare (48000.0, 2);
    dsp.snapToTargets (targets);
    dsp.setTargets (targets);

    const auto input = both (sine (2 * 48000, 1000.0, -18.0));
    const auto out = run (dsp, input);
    const auto latency = (size_t) dsp.getLatencySamples();
    const auto change = rmsDb (out.left, 48000 + latency, 2 * 48000) - rmsDb (input.left, 48000, 2 * 48000 - latency);

    // The model: -5.97 dB.
    CAPTURE (change);
    CHECK (std::abs (change + 6.0) < 0.5);
}

// 4. Reconstruction ---------------------------------------------------------------------------

TEST_CASE ("EQ W3: a spectral band at Range 0 gives back the input, delayed by the latency", "[eq][w3]")
{
    EqDsp dsp;
    EqDsp::Targets targets;
    targets.bands[0] = bell (2500.0, 1.0);
    targets.dynamics[0] = dynamics (-30.0, 0.0, true);
    dsp.prepare (48000.0, 2);
    dsp.snapToTargets (targets);
    dsp.setTargets (targets);

    const auto input = Stereo { pink (48000, -20.0, 3), pink (48000, -26.0, 4) };
    const auto out = run (dsp, input);
    const auto latency = (size_t) dsp.getLatencySamples();
    REQUIRE (latency == 1024);

    auto worst = 0.0;

    for (auto i = latency + 2048; i < input.left.size(); ++i)
    {
        worst = std::max (worst, std::abs ((double) out.left[i] - (double) input.left[i - latency]));
        worst = std::max (worst, std::abs ((double) out.right[i] - (double) input.right[i - latency]));
    }

    // The model: 1.7e-16 in double precision; the plugin works in float.
    CAPTURE (worst);
    CHECK (worst < 1.0e-6);
}

// 5. Latency ----------------------------------------------------------------------------------

TEST_CASE ("EQ W3: the reported latency is the measured delay at 44.1, 48 and 96 kHz", "[eq][w3]")
{
    const auto rate = GENERATE (44100.0, 48000.0, 96000.0);

    EqDsp dsp;
    EqDsp::Targets targets;
    targets.bands[0] = bell (2500.0, 1.0);
    targets.dynamics[0] = dynamics (-30.0, 0.0, true);
    dsp.prepare (rate, 2);
    dsp.snapToTargets (targets);
    dsp.setTargets (targets);

    const auto length = (int) rate;
    Stereo impulse { std::vector<float> ((size_t) length, 0.0f), std::vector<float> ((size_t) length, 0.0f) };
    impulse.left[(size_t) (length / 4)] = impulse.right[(size_t) (length / 4)] = 1.0f;

    const auto out = run (dsp, impulse);
    const auto peak = std::distance (out.left.begin(), std::max_element (out.left.begin(), out.left.end(),
                                                                            [] (float p, float q) { return std::abs (p) < std::abs (q); }));

    CAPTURE (rate, dsp.getLatencySamples());
    CHECK (dsp.getLatencySamples() == eq::SpectralStage::frameLengthFor (rate));
    CHECK ((int) peak - length / 4 == dsp.getLatencySamples());
}

TEST_CASE ("EQ W3: switching the first spectral band on and off changes the latency without a step", "[eq][w3]")
{
    EqDsp dsp;
    EqDsp::Targets targets;
    targets.bands[0] = bell (2500.0, 1.0);
    targets.dynamics[0] = dynamics (-30.0, -6.0, false);
    dsp.prepare (48000.0, 2);
    dsp.snapToTargets (targets);

    const auto input = both (sine (3 * 48000, 440.0, -12.0));
    Stereo out = input;
    std::vector<int> latencies;

    for (int start = 0; start < (int) input.left.size(); start += 256)
    {
        targets.dynamics[0].spectral = start >= 48000 && start < 2 * 48000;
        dsp.setTargets (targets);
        float* channels[] { out.left.data() + start, out.right.data() + start };
        dsp.process (channels, 2, std::min (256, (int) input.left.size() - start));
        latencies.push_back (dsp.getLatencySamples());
    }

    CHECK (std::find (latencies.begin(), latencies.end(), 1024) != latencies.end());
    CHECK (latencies.back() == 0);

    auto inputStep = 0.0, outputStep = 0.0;

    for (size_t i = 1; i < input.left.size(); ++i)
    {
        inputStep = std::max (inputStep, std::abs ((double) input.left[i] - input.left[i - 1]));
        outputStep = std::max (outputStep, std::abs ((double) out.left[i] - out.left[i - 1]));
    }

    // The switch fades out over 2 ms, waits in silence for the new latency, and fades in: at
    // most the sine's own step plus the fade's (the amplitude x 1/96 a sample), 1.18 times it.
    CAPTURE (inputStep, outputStep);
    CHECK (outputStep < 1.25 * inputStep);
}

// 6. Timing -----------------------------------------------------------------------------------

TEST_CASE ("EQ W3: a spectral band reaches 90 % of its cut within 30 ms at Attack 5 ms", "[eq][w3]")
{
    EqDsp dsp;
    EqDsp::Targets targets;
    targets.bands[0] = bell (2500.0, 1.0);
    targets.dynamics[0] = dynamics (-30.0, -9.0, true);
    dsp.prepare (48000.0, 2);
    dsp.snapToTargets (targets);
    dsp.setTargets (targets);

    auto x = pink (2 * 48000, -40.0, 5);
    const auto tone = sine (2 * 48000, 2500.0, -14.0, 0.5);

    for (size_t i = 0; i < x.size(); ++i)
        x[i] += tone[i];

    Stereo out = both (x);
    std::vector<std::pair<int, float>> trace;

    for (int start = 0; start < (int) x.size(); start += 64)
    {
        float* channels[] { out.left.data() + start, out.right.data() + start };
        dsp.process (channels, 2, std::min (64, (int) x.size() - start));
        trace.emplace_back (start + 64, dsp.getDynamicChange (0));
    }

    const auto final = trace.back().second;
    auto reached = -1;

    for (const auto& [samples, change] : trace)
    {
        if (samples >= 24000 && change <= 0.9f * final)
        {
            reached = samples - 24000;
            break;
        }
    }

    // The model: 17.3 ms, the cut -9.0 dB at the tone's bin.
    CAPTURE (final, reached);
    CHECK (final < -8.0f);
    REQUIRE (reached >= 0);
    CHECK (reached < (int) (0.030 * 48000));
}

// 7. Channels and sidechain -------------------------------------------------------------------

TEST_CASE ("EQ W3: a Mid spectral band cuts a centred tone and not the same tone in the side", "[eq][w3]")
{
    const auto render = [] (bool side)
    {
        EqDsp dsp;
        EqDsp::Targets targets;
        targets.bands[0] = bell (2500.0, 1.0);
        targets.bands[0].channel = eq::Channel::mid;
        targets.dynamics[0] = dynamics (-30.0, -9.0, true);
        dsp.prepare (48000.0, 2);
        dsp.snapToTargets (targets);
        dsp.setTargets (targets);

        auto left = sine (48000, 2500.0, -14.0), right = left;

        if (side)
            for (auto& value : right)
                value = -value;

        const Stereo input { left, right };
        const auto out = run (dsp, input);
        return rmsDb (out.left, 36000, 48000) - rmsDb (input.left, 36000 - 1024, 48000 - 1024);
    };

    const auto centred = render (false), inSide = render (true);
    CAPTURE (centred, inSide);
    CHECK (centred < -8.0);
    CHECK (std::abs (inSide) < 0.5);
}

TEST_CASE ("EQ W3: with Sidechain, the sidechain's tone triggers the cut and the main input's doesn't", "[eq][w3]")
{
    const auto render = [] (bool keyTone)
    {
        EqDsp dsp;
        EqDsp::Targets targets;
        targets.bands[0] = bell (2500.0, 1.0);
        targets.dynamics[0] = dynamics (-30.0, -9.0, true);
        targets.dynamics[0].sidechain = true;
        dsp.prepare (48000.0, 2);
        dsp.snapToTargets (targets);
        dsp.setTargets (targets);

        const auto input = both (sine (48000, 2500.0, -14.0));
        const auto key = keyTone ? both (sine (48000, 2500.0, -14.0)) : both (std::vector<float> (48000, 0.0f));
        const auto out = run (dsp, input, 256, &key);
        return rmsDb (out.left, 36000, 48000) - rmsDb (input.left, 36000 - 1024, 48000 - 1024);
    };

    const auto silentKey = render (false), tonedKey = render (true);
    CAPTURE (silentKey, tonedKey);
    CHECK (std::abs (silentKey) < 0.5);
    CHECK (tonedKey < -8.0);
}

TEST_CASE ("EQ W3: Spectral is appended per band and older states load it off", "[eq][w3]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    EqProcessor processor;
    const auto& parameters = processor.getParameters();
    const auto count = parameters.size();
    REQUIRE (count > eq::maxBands);

    for (int band = 0; band < eq::maxBands; ++band)
    {
        const auto* parameter = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[count - eq::maxBands + band]);
        REQUIRE (parameter != nullptr);
        CHECK (parameter->paramID == EqProcessor::id (band, "spectral"));
        CHECK (parameter->getValue() == 0.0f);
    }

    CHECK_FALSE (processor.readDynamics (0).spectral);
}

// 9. CPU --------------------------------------------------------------------------------------

TEST_CASE ("eq w3 cpu measurement", "[.measure]")
{
    constexpr int blockSize = 128, blocks = 6000, runs = 5;

    const auto measure = [] (int numSpectral)
    {
        EqDsp dsp;
        EqDsp::Targets targets;
        const double hz[] { 300.0, 1200.0, 2500.0, 6000.0 };

        for (int band = 0; band < 4; ++band)
        {
            targets.bands[(size_t) band] = bell (hz[band], 1.0);
            targets.dynamics[(size_t) band] = dynamics (-40.0, -6.0, band < numSpectral);
        }

        dsp.prepare (48000.0, 2);
        dsp.snapToTargets (targets);
        dsp.setTargets (targets);

        const auto noise = pink (blockSize * 64, -18.0, 6);
        float left[blockSize], right[blockSize];
        float* channels[] { left, right };
        std::chrono::duration<double> spent {};

        for (int block = 0; block < blocks; ++block)
        {
            std::copy_n (noise.begin() + (block % 64) * blockSize, blockSize, left);
            std::copy_n (noise.begin() + (block % 64) * blockSize, blockSize, right);
            const auto start = std::chrono::steady_clock::now();
            dsp.process (channels, 2, blockSize);
            spent += std::chrono::steady_clock::now() - start;
        }

        return 100.0 * spent.count() / ((double) (blocks * blockSize) / 48000.0);
    };

    const auto median = [&] (int numSpectral)
    {
        std::vector<double> values;

        for (int run = 0; run < runs; ++run)
            values.push_back (measure (numSpectral));

        std::sort (values.begin(), values.end());
        return values[runs / 2];
    };

    const auto comp = fancytest::compDefaultCpu();
    const auto dynamicOnly = median (0), spectral = median (4);
    const auto x = [comp] (double percent) { return fancytest::timesComp (percent, comp); };
    std::printf ("eq w3 cpu (median of %d, 48 kHz, 128 samples, stereo): Fancy Comp default %.3f %%; four dynamic bands %.3f %%%s; "
                 "four spectral bands %.3f %%%s (target < 1)\n",
                 runs, comp, dynamicOnly, x (dynamicOnly).c_str(), spectral, x (spectral).c_str());
    SUCCEED();
}
