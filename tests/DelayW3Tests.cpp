// Phase 4 Wave 3 for Fancy Delay: reverse and pitch-shifted taps (docs/delay.md, "Phase 4 Wave 3",
// section 6). The numbers were run first through docs/research/delay-w3-prepush.py, measured the
// same way.

#include "CompCpuReference.h"
#include "DelayDsp.h"
#include "DelayProcessor.h"
#include "LegacyDelayW2Dsp.h"

#include <juce_dsp/juce_dsp.h>

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

    void process (DelayDsp& dsp, int blockSize = 256)
    {
        for (int start = 0; start < size(); start += blockSize)
        {
            float* channels[] { left.data() + start, right.data() + start };
            dsp.process (channels, 2, std::min (blockSize, size() - start));
        }
    }
};

Stereo tone (double hz, double amplitude, int numSamples)
{
    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * hz * i / rate));

    return signal;
}

// One tap in ms mode, wet only.
DelayDsp::Targets oneTap (double ms)
{
    DelayDsp::Targets targets;
    targets.taps[0].used = true;
    targets.taps[0].noteMode = false;
    targets.taps[0].lengthMs = ms;
    targets.dryGain = 0.0;
    return targets;
}

Stereo render (const DelayDsp::Targets& targets, Stereo signal)
{
    DelayDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);
    signal.process (dsp);
    return signal;
}

double energy (const std::vector<float>& x, int from, int to)
{
    auto sum = 0.0;

    for (int i = std::max (0, from); i < std::min (to, (int) x.size()); ++i)
        sum += (double) x[(size_t) i] * x[(size_t) i];

    return sum;
}

// The strongest frequency near `guess`, from a Hann-windowed DFT searched coarse to fine.
double peakHz (const std::vector<float>& x, int from, int to, double guess, double span = 60.0)
{
    const auto n = to - from;

    const auto magnitude = [&] (double hz)
    {
        double re = 0.0, im = 0.0;
        const auto step = 2.0 * pi * hz / rate;

        for (int i = 0; i < n; ++i)
        {
            const auto w = 0.5 - 0.5 * std::cos (2.0 * pi * (double) i / (double) (n - 1));
            re += w * x[(size_t) (from + i)] * std::cos (step * (double) i);
            im += w * x[(size_t) (from + i)] * std::sin (step * (double) i);
        }

        return re * re + im * im;
    };

    auto best = guess;

    for (const auto [half, step] : { std::pair { span, span / 30.0 }, std::pair { span / 15.0, span / 300.0 }, std::pair { span / 150.0, span / 3000.0 } })
    {
        const auto centre = best;
        auto bestValue = -1.0;

        for (auto hz = centre - half; hz <= centre + half; hz += step)
            if (const auto value = magnitude (hz); value > bestValue)
                bestValue = value, best = hz;
    }

    return best;
}

// x with everything under `hz` removed, through one long FFT (zero-padded).
std::vector<double> highPassed (const std::vector<float>& x, double hz)
{
    constexpr int order = 18, size = 1 << order;
    REQUIRE ((int) x.size() <= size);

    juce::dsp::FFT fft (order);
    std::vector<std::complex<float>> time ((size_t) size), spectrum ((size_t) size);

    for (size_t i = 0; i < x.size(); ++i)
        time[i] = x[i];

    fft.perform (time.data(), spectrum.data(), false);

    const auto cut = (int) std::ceil (hz * size / rate);

    for (int bin = 0; bin < size; ++bin)
        if (const auto folded = std::min (bin, size - bin); folded < cut)
            spectrum[(size_t) bin] = 0.0f;

    fft.perform (spectrum.data(), time.data(), true);

    std::vector<double> out (x.size());

    for (size_t i = 0; i < x.size(); ++i)
        out[i] = (double) time[i].real();

    return out;
}

// Each third-octave band's energy in dB, 250 Hz to 16 kHz, from a Hann-windowed FFT (zero-padded).
std::vector<double> thirdOctaves (const std::vector<float>& x, int from, int to)
{
    constexpr int order = 17, size = 1 << order;
    const auto n = to - from;
    REQUIRE (n <= size);

    juce::dsp::FFT fft (order);
    std::vector<float> data ((size_t) size * 2, 0.0f);

    for (int i = 0; i < n; ++i)
        data[(size_t) i] = x[(size_t) (from + i)] * (float) (0.5 - 0.5 * std::cos (2.0 * pi * i / (n - 1)));

    fft.performFrequencyOnlyForwardTransform (data.data());

    std::vector<double> bands;

    for (auto centre = 250.0; centre < 16000.0; centre *= std::exp2 (1.0 / 3.0))
    {
        const auto lo = centre / std::exp2 (1.0 / 6.0), hi = centre * std::exp2 (1.0 / 6.0);
        auto sum = 0.0;

        for (int bin = (int) std::ceil (lo * size / rate); bin < (int) std::ceil (hi * size / rate); ++bin)
            sum += (double) data[(size_t) bin] * data[(size_t) bin];

        bands.push_back (10.0 * std::log10 (sum + 1.0e-30));
    }

    return bands;
}

// A 2 ms RMS envelope.
std::vector<double> envelope (const std::vector<float>& x)
{
    const auto frame = (int) (0.002 * rate);
    std::vector<double> out (x.size(), 0.0);
    auto window = 0.0;

    for (int i = 0; i < (int) x.size(); ++i)
    {
        window += (double) x[(size_t) i] * x[(size_t) i];

        if (i >= frame)
            window -= (double) x[(size_t) (i - frame)] * x[(size_t) (i - frame)];

        out[(size_t) i] = std::sqrt (std::max (window, 0.0) / frame);
    }

    return out;
}

double worstSecondDifference (const Stereo& signal, int from)
{
    auto worst = 0.0;

    for (const auto* channel : { &signal.left, &signal.right })
        for (size_t i = (size_t) from; i < channel->size(); ++i)
            worst = std::max (worst, std::abs ((double) (*channel)[i] - 2.0 * (*channel)[i - 1] + (*channel)[i - 2]));

    return worst;
}
} // namespace

// 1. Legacy -------------------------------------------------------------------------------------

TEST_CASE ("Reverse off and Pitch 0 render exactly as Delay v1.7.1", "[delay][w3]")
{
    const auto variant = GENERATE (0, 1, 2, 3, 4, 5);

    LegacyDelayW2Dsp::Targets old;
    old.taps[0].used = true;
    old.taps[0].feedback = 0.6;
    old.taps[0].lowpassHz = 6000.0;
    old.taps[0].modDepthCents = 12.0;
    old.taps[0].modRateHz = 0.8;
    old.taps[1].used = true;
    old.taps[1].noteMode = false;
    old.taps[1].lengthMs = 230.0;
    old.taps[1].feedback = 0.4;
    old.taps[1].pan = -0.5;
    old.dryGain = 0.7;

    switch (variant)
    {
        case 1: old.taps[0].modShape = LegacyDelayW2Dsp::triangle; old.taps[0].modStereoDegrees = 45.0; old.taps[1].modDepthCents = 30.0; break;
        case 2: old.taps[0].pingPong = true; old.taps[0].character = LegacyDelayW2Dsp::tape; old.taps[1].modDepthCents = 5.0; break;
        case 3: old.smoothTime = false; old.taps[1].diffusion = 0.5; old.taps[1].modDepthCents = 20.0; old.taps[1].modRandom = 1.0; break;
        case 4: old.taps[0].modDepthCents = 0.0; old.taps[1].character = LegacyDelayW2Dsp::bbd; old.duckAmountDb = 12.0; break;
        case 5: old.taps[0].saturate = true; old.taps[0].feedback = 1.05; old.taps[1].bits = 10.0; old.fastTanh = true; break;
        default: break;
    }

    DelayDsp::Targets now;

    const auto copy = [&]
    {
        for (size_t t = 0; t < now.taps.size(); ++t)
        {
            const auto& o = old.taps[t];
            auto& n = now.taps[t];
            n.used = o.used; n.on = o.on; n.solo = o.solo; n.noteMode = o.noteMode; n.pingPong = o.pingPong;
            n.filtersInLoop = o.filtersInLoop; n.saturate = o.saturate; n.lengthMs = o.lengthMs; n.noteLength = o.noteLength;
            n.feedback = o.feedback; n.lowpassHz = o.lowpassHz; n.highpassHz = o.highpassHz; n.bits = o.bits;
            n.width = o.width; n.volumeGain = o.volumeGain; n.pan = o.pan;
            n.character = o.character; n.characterAmount = o.characterAmount; n.modRateHz = o.modRateHz;
            n.modDepthCents = o.modDepthCents; n.modRandom = o.modRandom; n.diffusion = o.diffusion;
            n.modShape = o.modShape; n.modStereoDegrees = o.modStereoDegrees;
        }

        now.bpm = old.bpm; now.wetGain = old.wetGain; now.dryGain = old.dryGain; now.outputGain = old.outputGain;
        now.smoothTime = old.smoothTime; now.bypass = old.bypass;
        now.duckAmountDb = old.duckAmountDb; now.duckThresholdDb = old.duckThresholdDb; now.duckReleaseMs = old.duckReleaseMs;
        now.swing = old.swing; now.fastTanh = old.fastTanh; now.freeze = old.freeze;

        // The host's transport moves only reverse windows, so it changes nothing here.
        now.hostPlaying = true;
        now.hostPpq = 3.7;
    };

    copy();

    LegacyDelayW2Dsp legacy;
    DelayDsp current;
    legacy.prepare (rate);
    current.prepare (rate);
    legacy.snapToTargets (old);
    current.snapToTargets (now);

    std::mt19937 random (23);
    std::uniform_real_distribution<float> noise (-0.8f, 0.8f);
    Stereo a (96000);

    for (int i = 0; i < 24000; ++i)
        a.left[(size_t) i] = noise (random), a.right[(size_t) i] = noise (random);

    auto b = a;

    for (int start = 0; start < a.size(); start += 300)
    {
        // A depth change, then Freeze on and off, through both paths.
        if (start == 36000 || start == 54000 || start == 75000)
        {
            if (start == 36000) old.taps[0].modDepthCents = 25.0;
            if (start == 54000) old.freeze = true;
            if (start == 75000) old.freeze = false;

            copy();
            now.hostPpq = 3.7 + start / 24000.0;
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

// 2. Reverse ------------------------------------------------------------------------------------

TEST_CASE ("a reverse tap keeps the level and its window edges don't click", "[delay][w3]")
{
    constexpr int window = 24000;   // 500 ms
    const auto input = tone (437.25, 0.5, 4 * (int) rate);

    auto targets = oneTap (500.0);
    const auto forward = render (targets, input);
    targets.taps[0].reverse = true;
    const auto reversed = render (targets, input);

    const auto level = 10.0 * std::log10 (energy (reversed.left, 2 * window, reversed.size()) / energy (forward.left, 2 * window, forward.size()));

    // The model: -0.055 dB.
    INFO ("level " << level << " dB");
    CHECK (std::abs (level) < 0.1);

    const auto high = highPassed (reversed.left, 6000.0);
    const auto half = (int) (0.01 * rate);
    auto worst = 0.0;

    for (int edge = 2 * window; edge + half < reversed.size(); edge += window)
        for (int i = edge - half; i < edge + half; ++i)
            worst = std::max (worst, std::abs (high[(size_t) i]));

    // The model: -92.3 dB under the tone.
    const auto click = 20.0 * std::log10 (std::max (worst, 1.0e-15) / 0.5);
    INFO ("click " << click << " dB");
    CHECK (click < -80.0);
}

TEST_CASE ("a reverse tap plays an impulse at twice the window's end less its time", "[delay][w3]")
{
    constexpr int window = 24000, at = 29760;   // 0.62 s, in the window from 0.5 s

    auto targets = oneTap (500.0);
    targets.taps[0].reverse = true;

    Stereo input (5 * window);
    input.left[at] = input.right[at] = 1.0f;
    const auto out = render (targets, input);

    auto loudest = 0;

    for (int i = 0; i < out.size(); ++i)
        if (std::abs (out.left[(size_t) i]) > std::abs (out.left[(size_t) loudest]))
            loudest = i;

    CHECK (std::abs (loudest - (2 * 2 * window - at)) <= 1);
    CHECK (std::abs (out.left[(size_t) loudest]) > 0.99f);
}

TEST_CASE ("a Note-mode reverse tap's windows start on its note grid while the host plays", "[delay][w3]")
{
    constexpr int blockSize = 512, quarter = 24000;   // a 1/4 at 120 BPM
    constexpr double startPpq = 0.3;

    DelayDsp::Targets targets;
    targets.taps[0].used = true;
    targets.taps[0].noteMode = true;
    targets.taps[0].noteLength = { 2, delay::straight, 1 };
    targets.taps[0].reverse = true;
    targets.bpm = 120.0;
    targets.dryGain = 0.0;
    targets.hostPlaying = true;
    targets.hostPpq = startPpq;

    DelayDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    Stereo signal (4 * (int) rate);
    std::fill (signal.left.begin(), signal.left.end(), 0.5f);
    std::fill (signal.right.begin(), signal.right.end(), 0.5f);

    for (int start = 0; start < signal.size(); start += blockSize)
    {
        targets.hostPpq = startPpq + start / (double) quarter;
        dsp.setTargets (targets);

        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, std::min (blockSize, signal.size() - start));
    }

    // Each window's first sample has no gain at all, so a steady input leaves exact zeros there:
    // on the beats, where the host's position is a whole number.
    std::vector<int> zeros;

    for (int i = (int) rate; i < signal.size(); ++i)
        if (signal.left[(size_t) i] == 0.0f)
            zeros.push_back (i);

    REQUIRE (zeros.size() >= 3);

    for (const auto i : zeros)
    {
        const auto beats = startPpq + i / (double) quarter;
        INFO ("zero at " << i << ", beat " << beats);
        CHECK (std::abs (beats - std::round (beats)) * quarter <= 1.0);
    }
}

// 3. Pitch --------------------------------------------------------------------------------------

TEST_CASE ("a +7 st tap turns 1 kHz into 1498.3 Hz, steadily, on the tap time", "[delay][w3]")
{
    auto targets = oneTap (300.0);
    targets.taps[0].pitchSemitones = 7.0;

    const auto out = render (targets, tone (1000.0, 0.5, (int) (1.6 * rate)));
    const auto from = (int) (0.5 * rate), to = (int) (1.5 * rate);

    // The model: 1498.31 Hz, wavering 0.01 dB.
    const auto hz = peakHz (out.left, from, to, 1498.31, 30.0);
    INFO ("pitch " << hz << " Hz");
    CHECK (std::abs (hz - 1498.31) < 1.0);

    const auto env = envelope (out.left);
    const auto [lowest, highest] = std::minmax_element (env.begin() + from, env.begin() + to);
    const auto waver = 20.0 * std::log10 (*highest / *lowest);
    INFO ("wavers " << waver << " dB");
    CHECK (waver < 2.0);

    // A 200 ms burst with 10 ms raised-cosine edges: its envelope lands on the tap time.
    Stereo burst ((int) rate);
    const auto length = (int) (0.2 * rate), edge = (int) (0.01 * rate), at = (int) (0.1 * rate);

    for (int k = 0; k < length; ++k)
    {
        const auto ramp = std::min (1.0, std::min (k, length - 1 - k) / (double) edge);
        burst.left[(size_t) (at + k)] = burst.right[(size_t) (at + k)] = (float) (0.5 * std::sin (2.0 * pi * 1000.0 * k / rate) * (0.5 - 0.5 * std::cos (pi * ramp)));
    }

    const auto shifted = render (targets, burst);
    const auto in = envelope (burst.left), outEnvelope = envelope (shifted.left);
    auto bestLag = 0;
    auto bestValue = -1.0;

    for (int lag = (int) (0.25 * rate); lag < (int) (0.35 * rate); ++lag)
    {
        auto sum = 0.0;

        for (int i = 0; i + lag < burst.size(); ++i)
            sum += in[(size_t) i] * outEnvelope[(size_t) (i + lag)];

        if (sum > bestValue)
            bestValue = sum, bestLag = lag;
    }

    // The model: 300.44 ms.
    const auto ms = bestLag / rate * 1000.0;
    INFO ("lands at " << ms << " ms");
    CHECK (std::abs (ms - 300.0) <= 2.0);
}

TEST_CASE ("+12 st in the loop climbs in octaves and Feedback keeps its meaning", "[delay][w3]")
{
    constexpr double tap = 0.5, note = 0.3;

    auto targets = oneTap (tap * 1000.0);
    targets.taps[0].pitchSemitones = 12.0;
    targets.taps[0].feedback = 0.6;

    Stereo input ((int) ((5.0 * tap + 0.2) * rate));
    const auto length = (int) (note * rate), edge = (int) (0.02 * rate);

    for (int k = 0; k < length; ++k)
    {
        const auto ramp = std::min (1.0, std::min (k, length - 1 - k) / (double) edge);
        input.left[(size_t) k] = input.right[(size_t) k] = (float) (0.5 * std::sin (2.0 * pi * 220.0 * k / rate) * (0.5 - 0.5 * std::cos (pi * ramp)));
    }

    const auto out = render (targets, input);
    auto previous = 0.0;

    for (int repeat = 1; repeat <= 4; ++repeat)
    {
        const auto from = (int) ((tap * repeat - 0.03) * rate), to = (int) ((tap * repeat + note + 0.07) * rate);
        const auto want = 220.0 * std::exp2 ((double) repeat);
        const auto hz = peakHz (out.left, from, to, want, 0.05 * want);
        const auto level = 10.0 * std::log10 (energy (out.left, from, to));

        // The model: 440.0, 879.9, 1759.9 and 3519.8 Hz; 4.53, 4.56 and 4.54 dB a repeat.
        INFO ("repeat " << repeat << ": " << hz << " Hz, " << previous - level << " dB under the last");
        CHECK (std::abs (hz / want - 1.0) < 0.01);

        if (repeat > 1)
            CHECK (std::abs (previous - level - 4.4) <= 0.5);

        previous = level;
    }
}

TEST_CASE ("+12 st at 95 % feedback folds nothing back down", "[delay][w3]")
{
    auto targets = oneTap (300.0);
    targets.taps[0].pitchSemitones = 12.0;
    targets.taps[0].feedback = 0.95;

    const auto out = render (targets, tone (5000.0, 0.1, 20 * (int) rate));

    CHECK (std::all_of (out.left.begin(), out.left.end(), [] (float v) { return std::isfinite (v); }));

    constexpr int order = 16, size = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> data ((size_t) size * 2, 0.0f);
    auto windowSum = 0.0;

    for (int i = 0; i < size; ++i)
    {
        const auto w = 0.42 - 0.5 * std::cos (2.0 * pi * i / (size - 1)) + 0.08 * std::cos (4.0 * pi * i / (size - 1));
        data[(size_t) i] = out.left[(size_t) (out.size() - size + i)] * (float) w;
        windowSum += w;
    }

    fft.performFrequencyOnlyForwardTransform (data.data());

    auto largest = 0.0;

    for (int bin = (int) std::ceil (20.0 * size / rate); bin < (int) (4000.0 * size / rate); ++bin)
        largest = std::max (largest, (double) data[(size_t) bin] * 2.0 / windowSum);

    // The model: -159.5 dBFS.
    const auto db = 20.0 * std::log10 (std::max (largest, 1.0e-15));
    INFO ("largest part below 4 kHz " << db << " dBFS");
    CHECK (db < -80.0);
}

TEST_CASE ("Freeze holds a pitched loop instead of letting it climb", "[delay][w3]")
{
    auto targets = oneTap (300.0);
    targets.taps[0].pitchSemitones = 12.0;
    targets.taps[0].feedback = 0.6;

    // A second of pink noise, frozen at 1.2 s.
    std::mt19937 random (5);
    std::normal_distribution<double> gaussian;
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    Stereo signal ((int) (12.5 * rate));
    std::vector<double> noise ((size_t) rate);
    auto power = 0.0;

    for (auto& value : noise)
    {
        // Paul Kellet's pink filter.
        const auto white = gaussian (random);
        b0 = 0.99886 * b0 + white * 0.0555179; b1 = 0.99332 * b1 + white * 0.0750759; b2 = 0.96900 * b2 + white * 0.1538520;
        b3 = 0.86650 * b3 + white * 0.3104856; b4 = 0.55000 * b4 + white * 0.5329522; b5 = -0.7616 * b5 - white * 0.0168980;
        value = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362;
        b6 = white * 0.115926;
        power += value * value;
    }

    const auto scale = 0.1 / std::sqrt (power / (double) noise.size());

    for (size_t i = 0; i < noise.size(); ++i)
        signal.left[i] = signal.right[i] = (float) (noise[i] * scale);

    DelayDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    const auto freezeAt = (int) (1.2 * rate);

    for (int start = 0; start < signal.size(); start += 256)
    {
        if (start == freezeAt / 256 * 256)
        {
            targets.freeze = true;
            dsp.setTargets (targets);
        }

        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, std::min (256, signal.size() - start));
    }

    const auto early = thirdOctaves (signal.left, freezeAt + (int) (0.4 * rate), freezeAt + (int) (2.4 * rate));
    const auto late = thirdOctaves (signal.left, freezeAt + (int) (9.0 * rate), freezeAt + (int) (11.0 * rate));
    const auto loudest = *std::max_element (early.begin(), early.end());
    auto worst = 0.0;

    for (size_t band = 0; band < early.size(); ++band)
        if (early[band] > loudest - 60.0)
            worst = std::max (worst, std::abs (early[band] - late[band]));

    // The model, with this pink filter: 0.25 to 0.55 dB over three seeds, from 250 Hz up (below
    // it a third octave holds too few of the frozen loop's spectral lines to be steady).
    INFO ("largest third-octave change " << worst << " dB");
    CHECK (worst < 1.0);
}

// 4. Switching ----------------------------------------------------------------------------------

TEST_CASE ("switching Reverse and moving Pitch while playing is click-free", "[delay][w3]")
{
    auto targets = oneTap (20.0);
    targets.dryGain = 1.0;

    DelayDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    auto signal = tone (200.0, 0.25, (int) rate);
    const auto at = [] (double seconds) { return (int) std::lround (seconds * rate / 64.0) * 64; };

    for (int start = 0; start < signal.size(); start += 64)
    {
        if (start == at (0.15)) { targets.taps[0].reverse = true; dsp.setTargets (targets); }
        if (start == at (0.3)) { targets.taps[0].pitchSemitones = 7.0; dsp.setTargets (targets); }
        if (start == at (0.45)) { targets.taps[0].reverse = false; dsp.setTargets (targets); }
        if (start == at (0.65)) { targets.taps[0].pitchSemitones = -5.0; dsp.setTargets (targets); }
        if (start == at (0.8)) { targets.taps[0].pitchSemitones = 0.0; dsp.setTargets (targets); }

        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, 64);
    }

    // As the Wave 2 switches: the tone alone has 1.7e-4 here.
    const auto worst = worstSecondDifference (signal, 2400);
    INFO ("worst second difference " << worst);
    CHECK (worst < 2.0e-3);
}

TEST_CASE ("Reverse and Pitch are per tap, appended, and load as off from older states", "[delay][w3]")
{
    DelayProcessor processor;
    auto& apvts = processor.getApvts();

    for (int tap = 0; tap < delay::maxTaps; ++tap)
    {
        REQUIRE (apvts.getParameter (DelayProcessor::id (tap, "reverse")) != nullptr);
        REQUIRE (apvts.getParameter (DelayProcessor::id (tap, "pitch")) != nullptr);
    }

    // The last two parameters are tap 16's.
    const auto& all = processor.getParameters();
    const auto idOf = [&] (int fromEnd) { return dynamic_cast<juce::RangedAudioParameter*> (all[all.size() - fromEnd])->getParameterID(); };
    CHECK (idOf (2) == "t16reverse");
    CHECK (idOf (1) == "t16pitch");

    auto& pitch = *apvts.getParameter ("t1pitch");
    CHECK (pitch.getText (pitch.convertTo0to1 (7.0f), 0) == "+7.00 st");
    CHECK (pitch.getText (pitch.convertTo0to1 (-0.1f), 0) == "-0.10 st");
    CHECK (pitch.getText (pitch.convertTo0to1 (0.0f), 0) == "0.00 st");
    CHECK (pitch.convertFrom0to1 (pitch.getValueForText ("-12")) == -12.0f);
    CHECK (std::abs (pitch.convertFrom0to1 (pitch.getValueForText ("7")) - 7.0f) < 1.0e-4f);

    const auto spec = processor.readTap (2);
    CHECK_FALSE (spec.reverse);
    CHECK (spec.pitchSemitones == 0.0);
}

// 5. CPU ----------------------------------------------------------------------------------------

TEST_CASE ("delay w3 cpu measurement", "[.measure]")
{
    constexpr int blockSize = 128, blocks = 6000, runs = 5;

    const auto measure = [] (double semitones, bool reverse)
    {
        DelayDsp::Targets targets;

        for (int tap = 0; tap < 4; ++tap)
        {
            auto& spec = targets.taps[(size_t) tap];
            spec.used = true;
            spec.noteMode = false;
            spec.lengthMs = 120.0 + 90.0 * tap;
            spec.feedback = 0.5;
            spec.pitchSemitones = semitones;
            spec.reverse = reverse;
        }

        DelayDsp dsp;
        dsp.prepare (rate);
        dsp.snapToTargets (targets);

        float left[blockSize], right[blockSize];
        float* channels[] { left, right };
        std::mt19937 random (4);
        std::uniform_real_distribution<float> noise (-0.5f, 0.5f);
        std::chrono::duration<double> spent {};

        for (int block = 0; block < blocks; ++block)
        {
            for (int i = 0; i < blockSize; ++i)
                left[i] = right[i] = noise (random);

            const auto start = std::chrono::steady_clock::now();
            dsp.setTargets (targets);
            dsp.process (channels, 2, blockSize);
            spent += std::chrono::steady_clock::now() - start;
        }

        return 100.0 * spent.count() / ((double) (blocks * blockSize) / rate);
    };

    const auto median = [&] (double semitones, bool reverse)
    {
        std::vector<double> values;

        for (int run = 0; run < runs; ++run)
            values.push_back (measure (semitones, reverse));

        std::sort (values.begin(), values.end());
        return values[runs / 2];
    };

    const auto comp = fancytest::compDefaultCpu();
    const auto plain = median (0.0, false), pitched = median (12.0, false), reversed = median (0.0, true), both = median (7.0, true);
    const auto x = [comp] (double percent) { return fancytest::timesComp (percent, comp); };

    std::printf ("delay w3 cpu (median of %d, 48 kHz, 128 samples, stereo, 4 taps at 50 %% feedback): Fancy Comp default %.3f %%; "
                 "unpitched %.3f %%%s; +12 st %.3f %%%s (+%.3f %%%s, target about +0.6 Mac, < +2 Windows); "
                 "reversed %.3f %%%s (+%.3f %%%s); reversed at +7 st %.3f %%%s\n",
                 runs, comp, plain, x (plain).c_str(), pitched, x (pitched).c_str(), pitched - plain, x (pitched - plain).c_str(),
                 reversed, x (reversed).c_str(), reversed - plain, x (reversed - plain).c_str(), both, x (both).c_str());
    SUCCEED();
}
