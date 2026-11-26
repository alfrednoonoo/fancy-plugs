// Phase 4 Wave 3 for Fancy Limit: multiband limiting (docs/limit.md, "Phase 4 Wave 3: multiband
// limiting", section 6; modelled in docs/research/limit-multiband-model.py). One band is held to
// v1.6.0 by LimitW1Tests' legacy test, which runs the current LimitDsp at its defaults.

#include "LimitDsp.h"
#include "CompCpuReference.h"

#include <juce_dsp/juce_dsp.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

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

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples), right ((size_t) numSamples) {}
    int size() const { return (int) left.size(); }
};

Stereo mono (const std::vector<double>& x)
{
    Stereo s ((int) x.size());

    for (size_t i = 0; i < x.size(); ++i)
        s.left[i] = s.right[i] = (float) x[i];

    return s;
}

Stereo run (LimitDsp& dsp, const LimitDsp::Targets& targets, Stereo signal, int blockSize = 333)
{
    dsp.setTargets (targets);
    dsp.prepare (rate);

    for (int start = 0; start < signal.size(); start += blockSize)
    {
        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, std::min (blockSize, signal.size() - start));
    }

    return signal;
}

Stereo run (const LimitDsp::Targets& targets, Stereo signal, int blockSize = 333)
{
    LimitDsp dsp;
    return run (dsp, targets, std::move (signal), blockSize);
}

LimitDsp::Targets withBands (int bands)
{
    LimitDsp::Targets targets;
    targets.bands = bands;
    targets.crossoverHz = bands == 4 ? std::array<double, 3> { 150.0, 1000.0, 6000.0 } : std::array<double, 3> { 150.0, 2500.0, 8000.0 };
    targets.bandReleaseMs = bands == 4 ? std::array<double, 4> { 120.0, 90.0, 60.0, 40.0 } : std::array<double, 4> { 120.0, 80.0, 50.0, 40.0 };
    return targets;
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

std::vector<double> highpass2 (const std::vector<double>& x, double hz)
{
    const auto w = 2.0 * pi * hz / rate, c = std::cos (w), alpha = std::sin (w) / (2.0 * 0.7071067811865476), a0 = 1.0 + alpha;
    const double b[] { (1.0 + c) / 2.0 / a0, -(1.0 + c) / a0, (1.0 + c) / 2.0 / a0 };
    const double a[] { 1.0, -2.0 * c / a0, (1.0 - alpha) / a0 };
    return biquad (x, b, a);
}

std::vector<double> bandpass (const std::vector<double>& x, double hz, double q)
{
    const auto w = 2.0 * pi * hz / rate, alpha = std::sin (w) / (2.0 * q), a0 = 1.0 + alpha;
    const double b[] { alpha / a0, 0.0, -alpha / a0 };
    const double a[] { 1.0, -2.0 * std::cos (w) / a0, (1.0 - alpha) / a0 };
    return biquad (x, b, a);
}

const std::vector<int>& kickStarts()
{
    static const std::vector<int> starts = []
    {
        std::vector<int> s;

        for (int k = 0; k < 8; ++k)
            s.push_back ((int) (k * 0.5 * rate));

        return s;
    }();

    return starts;
}

// The model's mix: 60 Hz kicks every 0.5 s, a steady 1 kHz tone, hats above 6 kHz, pushed 8 dB.
std::vector<double> modelMix (bool click)
{
    const auto n = (int) (4.0 * rate);
    std::mt19937 random (5);
    std::normal_distribution<double> noise;
    std::vector<double> body ((size_t) n, 0.0), clicks ((size_t) n, 0.0), hats ((size_t) n);

    for (auto& h : hats)
        h = noise (random);

    hats = highpass2 (highpass2 (highpass2 (highpass2 (hats, 6000.0), 6000.0), 6000.0), 6000.0);

    for (const auto start : kickStarts())
    {
        for (int k = 0; k < (int) (0.5 * rate) && start + k < n; ++k)
        {
            body[(size_t) (start + k)] += 0.9 * std::exp (-k / (0.09 * rate)) * std::sin (2.0 * pi * 60.0 * k / rate);

            if (k < (int) (0.003 * rate))
                clicks[(size_t) (start + k)] += 0.5 * std::exp (-k / (0.002 * rate)) * noise (random);
        }
    }

    const auto push = std::pow (10.0, 8.0 / 20.0);
    std::vector<double> x ((size_t) n);

    for (int i = 0; i < n; ++i)
        x[(size_t) i] = (body[(size_t) i] + (click ? clicks[(size_t) i] : 0.0) + 0.12 * std::sin (2.0 * pi * 1000.0 * i / rate)
                         + 0.075 * hats[(size_t) i]) * push;

    return x;
}

// The 1 kHz tone's mean dip in the 150 ms after each kick but the first, against the 25 ms before
// it, through a band-pass, in 5 ms RMS windows (scratchpad pre-push model: 1 band -5.3, 3 bands -1.4 dB).
double meanToneDip (const std::vector<float>& out, int latency)
{
    std::vector<double> x (out.begin(), out.end());
    const auto y = bandpass (x, 1000.0, 4.0);
    const auto window = (int) (0.005 * rate);
    auto total = 0.0;
    auto count = 0;

    const auto rms = [&] (int from, int to)
    {
        auto sum = 0.0;

        for (int i = from; i < to; ++i)
            sum += y[(size_t) i] * y[(size_t) i];

        return std::sqrt (sum / (double) (to - from));
    };

    for (size_t hit = 1; hit < kickStarts().size(); ++hit)
    {
        const auto start = kickStarts()[hit] + latency;
        const auto before = rms (start - (int) (0.03 * rate), start - (int) (0.005 * rate));
        auto sum = 0.0;
        auto windows = 0;

        for (int k = (int) (0.005 * rate); k < (int) (0.150 * rate); k += window)
        {
            sum += 20.0 * std::log10 (rms (start + k, start + k + window) / before);
            ++windows;
        }

        total += sum / windows;
        ++count;
    }

    return total / count;
}

double toneAmplitude (const std::vector<float>& x, double hz, int from, int length)
{
    double re = 0.0, im = 0.0;

    for (int i = 0; i < length; ++i)
    {
        const auto phase = 2.0 * pi * hz * i / rate;
        re += x[(size_t) (from + i)] * std::cos (phase);
        im -= x[(size_t) (from + i)] * std::sin (phase);
    }

    return 2.0 * std::sqrt (re * re + im * im) / length;
}

double peakOf (const Stereo& s)
{
    auto peak = 0.0;

    for (int i = 0; i < s.size(); ++i)
        peak = std::max ({ peak, (double) std::abs (s.left[(size_t) i]), (double) std::abs (s.right[(size_t) i]) });

    return peak;
}
} // namespace

// 1 -------------------------------------------------------------------------------------------

TEST_CASE ("Limit W3: one band renders the same whatever the band settings, with the old latency", "[limit][w3]")
{
    const auto oversampling = GENERATE (0, 2);
    const auto variant = GENERATE (0, 1, 2);

    auto base = LimitDsp::Targets {};
    base.oversampling = oversampling;
    base.thresholdDb = -8.0;
    base.truePeak = variant != 1;
    base.autoRelease = variant != 2;
    base.clip = variant == 2;
    base.dcFilter = variant == 1;
    base.gainLock = variant == 2;

    auto changed = base;
    changed.crossoverLr48 = true;
    changed.crossoverHz = { 90.0, 700.0, 12000.0 };
    changed.bandReleaseMs = { 500.0, 3.0, 20.0, 900.0 };

    const auto signal = mono (modelMix (true));
    LimitDsp a, b;
    const auto outA = run (a, base, signal);
    const auto outB = run (b, changed, signal);

    CAPTURE (oversampling, variant);
    CHECK (outA.left == outB.left);
    CHECK (outA.right == outB.right);
    CHECK (a.getLatencySamples() == a.latencyFor (oversampling, false));
    CHECK (a.bandLatencyFor (1) == 0);
}

// 2 -------------------------------------------------------------------------------------------

TEST_CASE ("Limit W3: with nothing limited, the bands sum flat", "[limit][w3]")
{
    const auto bands = GENERATE (2, 3, 4);
    const auto lr48 = GENERATE (false, true);

    auto targets = withBands (bands);
    targets.crossoverLr48 = lr48;
    targets.oversampling = LimitDsp::osOff;

    constexpr int order = 16, size = 1 << order;
    Stereo impulse (size);
    impulse.left[0] = impulse.right[0] = 0.01f;

    const auto out = run (targets, impulse, 512);

    juce::dsp::FFT fft (order);
    std::vector<float> data ((size_t) size * 2, 0.0f);
    std::copy (out.left.begin(), out.left.end(), data.begin());
    fft.performFrequencyOnlyForwardTransform (data.data(), true);

    auto worst = 0.0;

    for (int bin = (int) std::ceil (20.0 * size / rate); bin <= (int) (20000.0 * size / rate); ++bin)
        worst = std::max (worst, std::abs (20.0 * std::log10 ((double) data[(size_t) bin] / 0.01)));

    CAPTURE (bands, lr48, worst);
    CHECK (worst < 0.05);
}

// 3 -------------------------------------------------------------------------------------------

TEST_CASE ("Limit W3: three bands at least halve the pumping a kick causes", "[limit][w3]")
{
    auto one = withBands (1);
    one.ceilingDb = 0.0;
    one.autoRelease = false; // as the model
    auto three = withBands (3);
    three.ceilingDb = 0.0;
    three.autoRelease = false;

    const auto signal = mono (modelMix (false));
    LimitDsp a, b;
    const auto outOne = run (a, one, signal);
    const auto outThree = run (b, three, signal);

    const auto dipOne = meanToneDip (outOne.left, a.getLatencySamples());
    const auto dipThree = meanToneDip (outThree.left, b.getLatencySamples());

    CAPTURE (dipOne, dipThree);
    CHECK (dipOne < -2.0);
    CHECK (dipThree > 0.5 * dipOne);
}

// 4 -------------------------------------------------------------------------------------------

TEST_CASE ("Limit W3: a loud bass band leaves a quiet treble band alone", "[limit][w3]")
{
    auto targets = withBands (3);
    const auto ceiling = std::pow (10.0, targets.ceilingDb / 20.0);
    const auto n = (int) (2.0 * rate);
    std::vector<double> x ((size_t) n);

    for (int i = 0; i < n; ++i)
        x[(size_t) i] = ceiling * (std::pow (10.0, 9.0 / 20.0) * std::sin (2.0 * pi * 60.0 * i / rate)
                                   + std::pow (10.0, -20.0 / 20.0) * std::sin (2.0 * pi * 5000.0 * i / rate));

    LimitDsp dsp;
    dsp.setTargets (targets);
    dsp.prepare (rate);

    auto signal = mono (x);
    auto bassReduction = 0.0f;

    for (int start = 0; start < n; start += 256)
    {
        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, std::min (256, n - start));

        if (start > n / 2)
            bassReduction = std::max (bassReduction, dsp.takeBandReduction (0));
        else
            dsp.takeBandReduction (0);
    }

    const auto from = n / 2, length = (int) rate / 2;
    const auto inTreble = ceiling * std::pow (10.0, -20.0 / 20.0);
    const auto outTreble = toneAmplitude (signal.left, 5000.0, from, length);
    const auto moved = 20.0 * std::log10 (outTreble / inTreble);

    CAPTURE (bassReduction, moved);
    CHECK (bassReduction >= 6.0f);
    CHECK (std::abs (moved) < 0.5);
}

// 5 -------------------------------------------------------------------------------------------

TEST_CASE ("Limit W3: each band recovers at its own Release", "[limit][w3]")
{
    const auto recovery = [] (double releaseMs)
    {
        auto targets = withBands (3);
        targets.autoRelease = false;
        targets.bandReleaseMs[0] = releaseMs;

        const auto n = (int) (3.0 * rate), burst = (int) (0.5 * rate);
        std::vector<double> x ((size_t) n, 0.0);

        for (int i = 0; i < burst; ++i)
            x[(size_t) i] = 2.5 * std::sin (2.0 * pi * 60.0 * i / rate);

        LimitDsp dsp;
        dsp.setTargets (targets);
        dsp.prepare (rate);
        auto signal = mono (x);

        constexpr int block = 48; // 1 ms
        std::vector<float> trace;

        for (int start = 0; start < n; start += block)
        {
            float* channels[] { signal.left.data() + start, signal.right.data() + start };
            dsp.process (channels, 2, block);
            trace.push_back (dsp.takeBandReduction (0));
        }

        // From the burst's end, the time until band 1 has given back 6 dB of its reduction then.
        const auto end = (size_t) (burst / block);
        const auto atEnd = trace[end];

        for (auto i = end; i < trace.size(); ++i)
            if (trace[i] <= atEnd - 6.0f)
                return (double) (i - end);

        return 1.0e9;
    };

    const auto fast = recovery (120.0), slow = recovery (500.0);
    CAPTURE (fast, slow);
    CHECK (slow >= 2.0 * fast);
    CHECK (slow <= 6.0 * fast);
}

// 6 -------------------------------------------------------------------------------------------

TEST_CASE ("Limit W3: the output never passes Ceiling at any band count or oversampling", "[limit][w3]")
{
    const auto bands = GENERATE (2, 3, 4);
    const auto oversampling = GENERATE (0, 1, 2, 3);
    const auto click = GENERATE (false, true);

    auto targets = withBands (bands);
    targets.oversampling = oversampling;
    targets.thresholdDb = -6.0;

    const auto out = run (targets, mono (modelMix (click)));
    const auto ceiling = std::pow (10.0, targets.ceilingDb / 20.0);

    CAPTURE (bands, oversampling, click, peakOf (out), ceiling);
    CHECK (peakOf (out) <= ceiling * 1.000001);
}

// 7 -------------------------------------------------------------------------------------------

TEST_CASE ("Limit W3: bands add 5 ms of latency, and bypass and Listen stay aligned", "[limit][w3]")
{
    const auto oversampling = GENERATE (0, 2);

    LimitDsp one, three;
    auto targetsOne = withBands (1);
    targetsOne.oversampling = oversampling;
    auto targetsThree = withBands (3);
    targetsThree.oversampling = oversampling;

    one.setTargets (targetsOne);
    one.prepare (rate);
    three.setTargets (targetsThree);
    three.prepare (rate);

    CAPTURE (oversampling);
    CHECK (three.getLatencySamples() == one.getLatencySamples() + 240);

    // Bypass: the input, delayed by the latency, exactly.
    {
        auto bypassed = targetsThree;
        bypassed.bypass = true;
        const auto input = mono (modelMix (true));
        LimitDsp dsp;
        const auto out = run (dsp, bypassed, input);
        const auto latency = dsp.getLatencySamples();
        auto worst = 0.0;

        for (int i = latency; i < input.size(); ++i)
            worst = std::max (worst, (double) std::abs (out.left[(size_t) i] - input.left[(size_t) (i - latency)]));

        CHECK (worst == 0.0);
    }

    // Listen, oversampling off: what's taken away and what's left add back up to the input.
    if (oversampling == 0)
    {
        auto plain = targetsThree;
        plain.truePeak = false;
        plain.thresholdDb = 0.0;
        plain.ceilingDb = 0.0;
        auto listening = plain;
        listening.listen = true;

        const auto input = mono (modelMix (true));
        LimitDsp a, b;
        const auto kept = run (a, plain, input);
        const auto removed = run (b, listening, input);
        const auto latency = a.getLatencySamples();
        auto worst = 0.0, largest = 0.0;

        for (int i = latency; i < input.size(); ++i)
        {
            const auto sum = (double) kept.left[(size_t) i] + (double) removed.left[(size_t) i];
            worst = std::max (worst, std::abs (sum - (double) input.left[(size_t) (i - latency)]));
            largest = std::max (largest, (double) std::abs (removed.left[(size_t) i]));
        }

        CAPTURE (worst, largest);
        CHECK (largest > 0.1);
        CHECK (worst < 1.0e-3);
    }
}

// 8 -------------------------------------------------------------------------------------------

TEST_CASE ("Limit W3: sweeping the crossovers, band releases and band count stays clean", "[limit][w3]")
{
    const auto n = (int) (6.0 * rate);
    std::vector<double> x ((size_t) n);

    for (int i = 0; i < n; ++i)
        x[(size_t) i] = 0.5 * std::sin (2.0 * pi * 100.0 * i / rate);

    auto signal = mono (x);
    LimitDsp dsp;
    auto targets = withBands (4);
    targets.thresholdDb = -6.0;
    dsp.setTargets (targets);
    dsp.prepare (rate);

    constexpr int block = 64;
    auto finite = true;
    auto worstStep = 0.0;
    auto previous = 0.0f;

    for (int start = 0; start < n; start += block)
    {
        const auto t = (double) start / (double) n;

        // Crossovers and releases sweep through their ranges and back; the count steps through 4, 3, 2, 1.
        targets.crossoverHz = { 20.0 * std::pow (1000.0, t), 20.0 * std::pow (1000.0, 1.0 - t), 20.0 * std::pow (1000.0, 0.5 + 0.5 * std::sin (6.0 * t)) };
        targets.bandReleaseMs = { 1.0 * std::pow (1000.0, t), 1.0 * std::pow (1000.0, 1.0 - t), 50.0, 1000.0 * (1.0 - t) + 1.0 };
        targets.bands = 4 - std::min (3, (int) (t * 4.0));
        dsp.setTargets (targets);

        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, block);

        for (int i = start; i < start + block; ++i)
        {
            const auto value = signal.left[(size_t) i];
            finite = finite && std::isfinite (value);
            worstStep = std::max (worstStep, (double) std::abs (value - previous));
            previous = value;
        }
    }

    CAPTURE (worstStep);
    CHECK (finite);
    CHECK (worstStep < 0.1);
}

// 9 -------------------------------------------------------------------------------------------

TEST_CASE ("Limit W3: three bands render bit-identically at any block size", "[limit][w3]")
{
    const auto targets = withBands (3);
    const auto input = mono (modelMix (true));
    const auto reference = run (targets, input, 512);

    LimitDsp dsp;
    dsp.setTargets (targets);
    dsp.prepare (rate);

    auto signal = input;
    std::mt19937 random (11);
    std::uniform_int_distribution<int> sizes (1, 1500);

    for (int start = 0; start < signal.size();)
    {
        const auto size = std::min (sizes (random), signal.size() - start);
        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, size);
        start += size;
    }

    CHECK (signal.left == reference.left);
    CHECK (signal.right == reference.right);
}

// 11 ------------------------------------------------------------------------------------------

TEST_CASE ("limit w3 cpu measurement", "[.measure]")
{
    constexpr int blockSize = 128, blocks = 6000, runs = 5;

    const auto measure = [] (int bands)
    {
        LimitDsp dsp;
        dsp.setTargets (withBands (bands));
        dsp.prepare (48000.0);

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
            dsp.process (channels, 2, blockSize);
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
    const double cost[] { median (1), median (2), median (3), median (4) };

    std::printf ("limit w3 cpu (median of %d, 48 kHz, 128 samples, stereo, defaults): Fancy Comp default %.3f %%; 1 band %.3f %%%s; "
                 "2 bands +%.3f %%%s; 3 bands +%.3f %%%s; 4 bands +%.3f %%%s (target: four bands add under 2.5 x Comp)\n",
                 runs, comp, cost[0], x (cost[0]).c_str(), cost[1] - cost[0], x (cost[1] - cost[0]).c_str(),
                 cost[2] - cost[0], x (cost[2] - cost[0]).c_str(), cost[3] - cost[0], x (cost[3] - cost[0]).c_str());
    SUCCEED();
}
