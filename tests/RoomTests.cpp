// Fancy Room (docs/room.md): the §3.4 tests of docs/BUILD_PROMPT_PHASE3_NEW_PLUGINS.md, run as
// docs/research/room-model.py runs them (the same signals and the same measurements), and the
// plugin's own checks. Limits are the brief's; the model's figures are in docs/room.md.

#include "RoomDsp.h"
#include "RoomEditor.h"
#include "RoomProcessor.h"

#if FANCY_HAS_COMP
 #include "CompDsp.h"
#endif

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <functional>
#include <limits>
#include <memory>
#include <numbers>
#include <random>
#include <utility>
#include <vector>

using Catch::Approx;

namespace
{
constexpr double fs = 48000.0;
constexpr double pi = std::numbers::pi;

struct Stereo
{
    std::vector<double> left, right;
};

using Change = std::pair<int, RoomDsp::Targets>;

std::function<int()> fixedBlocks (int size)
{
    return [size] { return size; };
}

std::function<int()> randomBlocks (unsigned seed)
{
    auto random = std::make_shared<std::mt19937> (seed);
    return [random] { return (int) ((*random)() % 2048); }; // 0 included
}

// Renders total samples of the input (silence after it), with settings changed at sample
// positions. Blocks are cut at each change, as a host's are at automation points.
Stereo render (const RoomDsp::Targets& targets, const std::vector<float>& inLeft, const std::vector<float>& inRight, int total,
               const std::vector<Change>& changes = {}, std::function<int()> nextBlock = fixedBlocks (480), double rate = fs)
{
    RoomDsp dsp;
    dsp.setTargets (targets);
    dsp.prepare (rate);

    std::vector<float> left ((size_t) total, 0.0f), right ((size_t) total, 0.0f);
    std::copy_n (inLeft.begin(), std::min ((int) inLeft.size(), total), left.begin());
    std::copy_n (inRight.begin(), std::min ((int) inRight.size(), total), right.begin());

    size_t next = 0;

    for (int position = 0; position < total;)
    {
        while (next < changes.size() && changes[next].first <= position)
            dsp.setTargets (changes[next++].second);

        auto size = std::min (nextBlock(), total - position);

        if (next < changes.size())
            size = std::min (size, changes[next].first - position);

        float* channels[] { left.data() + position, right.data() + position };
        dsp.process (channels, 2, size);
        position += size;
    }

    return { std::vector<double> (left.begin(), left.end()), std::vector<double> (right.begin(), right.end()) };
}

std::vector<float> impulse()
{
    return { 1.0f };
}

std::vector<float> noise (int numSamples, double db, unsigned seed)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<double> uniform (-1.0, 1.0);
    const auto amplitude = std::pow (10.0, db / 20.0) * std::sqrt (3.0);
    std::vector<float> out ((size_t) numSamples);

    for (auto& v : out)
        v = (float) (amplitude * uniform (random));

    return out;
}

// The model's biquad(): RBJ low-pass, high-pass, or band-pass with a 0 dB peak.
struct Biquad
{
    double b0, b1, b2, a1, a2;

    static Biquad bandPass (double f0, double q)
    {
        const auto w = 2.0 * pi * f0 / fs;
        const auto c = std::cos (w), a = std::sin (w) / (2.0 * q), a0 = 1.0 + a;
        return { a / a0, 0.0, -a / a0, -2.0 * c / a0, (1.0 - a) / a0 };
    }

    std::vector<double> run (const std::vector<double>& x) const
    {
        std::vector<double> y (x.size());
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;

        for (size_t i = 0; i < x.size(); ++i)
        {
            const auto o = b0 * x[i] + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x[i]; y2 = y1; y1 = o;
            y[i] = o;
        }

        return y;
    }
};

void fft (std::vector<std::complex<double>>& a)
{
    const auto n = a.size();

    for (size_t i = 1, j = 0; i < n; ++i)
    {
        auto bit = n >> 1;

        for (; (j & bit) != 0; bit >>= 1)
            j ^= bit;

        j |= bit;

        if (i < j)
            std::swap (a[i], a[j]);
    }

    for (size_t size = 2; size <= n; size <<= 1)
    {
        const auto step = std::polar (1.0, -2.0 * pi / (double) size);

        for (size_t start = 0; start < n; start += size)
        {
            std::complex<double> w (1.0, 0.0);

            for (size_t k = 0; k < size / 2; ++k)
            {
                const auto t = a[start + k + size / 2] * w;
                a[start + k + size / 2] = a[start + k] - t;
                a[start + k] += t;
                w *= step;
            }
        }
    }
}

// Welch: 4096-point Hann, 50 % overlap, L and R averaged.
std::vector<double> welch (const Stereo& s, int start, int stop, int nfft = 4096)
{
    std::vector<double> window ((size_t) nfft), accumulated ((size_t) nfft / 2, 0.0);

    for (int i = 0; i < nfft; ++i)
        window[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * pi * i / nfft);

    auto count = 0;

    for (const auto* x : { &s.left, &s.right })
    {
        for (auto position = start; position + nfft <= stop; position += nfft / 2)
        {
            std::vector<std::complex<double>> frame ((size_t) nfft);

            for (int i = 0; i < nfft; ++i)
                frame[(size_t) i] = (*x)[(size_t) (position + i)] * window[(size_t) i];

            fft (frame);

            for (int k = 0; k < nfft / 2; ++k)
                accumulated[(size_t) k] += std::norm (frame[(size_t) k]);

            ++count;
        }
    }

    for (auto& value : accumulated)
        value /= count;

    return accumulated;
}

// T30 from the energy-decay curve: the fit between -5 and -35 dB, as RT60.
double t30 (const std::vector<double>& energy)
{
    std::vector<double> curve (energy.size());
    auto total = 0.0;

    for (auto i = energy.size(); i-- > 0;)
    {
        total += energy[i];
        curve[i] = total;
    }

    const auto top = curve[0];
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    auto n = 0;

    for (size_t i = 0; i < curve.size(); ++i)
    {
        const auto db = 10.0 * std::log10 (std::max (curve[i] / top, 1.0e-30));

        if (db <= -5.0 && db >= -35.0)
        {
            const auto x = (double) i / fs;
            sx += x; sy += db; sxx += x * x; sxy += x * db;
            ++n;
        }
    }

    const auto slope = (n * sxy - sx * sy) / (n * sxx - sx * sx);
    return -60.0 / slope;
}

double tMid (const Stereo& s)
{
    auto sum = 0.0;

    for (const auto fc : { 500.0, 1000.0 })
    {
        const auto band = Biquad::bandPass (fc, 1.4142);
        const auto l = band.run (band.run (s.left));
        const auto r = band.run (band.run (s.right));
        std::vector<double> energy (l.size());

        for (size_t i = 0; i < l.size(); ++i)
            energy[i] = l[i] * l[i] + r[i] * r[i];

        sum += t30 (energy);
    }

    return 0.5 * sum;
}

bool allFinite (const Stereo& s)
{
    for (const auto* channel : { &s.left, &s.right })
        for (const auto v : *channel)
            if (! std::isfinite (v))
                return false;

    return true;
}

double windowDb (const Stereo& s, int from, int to)
{
    auto e = 0.0;

    for (int i = from; i < to; ++i)
        e += s.left[(size_t) i] * s.left[(size_t) i] + s.right[(size_t) i] * s.right[(size_t) i];

    return 10.0 * std::log10 (std::max (e / std::max (1, to - from), 1.0e-30));
}

void setValue (fancy::FancyProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.getApvts().getParameter (id);
    REQUIRE (parameter != nullptr);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}
} // namespace

// §3.4 ------------------------------------------------------------------------------------------

TEST_CASE ("Room: no metallic ringing, Decay 2 s, Mod 25 %", "[room][metallic]")
{
    RoomDsp::Targets targets;
    targets.decay = 2.0;
    targets.mod = 0.25;

    const auto quality = GENERATE (0, 1, 2);
    targets.quality = quality;

    const auto out = render (targets, impulse(), impulse(), (int) (1.6 * fs));
    const auto psd = welch (out, (int) (0.3 * fs), (int) (1.5 * fs));
    constexpr int nfft = 4096;

    auto worst = -99.0, at = 0.0;

    for (int k = 1; k < (int) psd.size(); ++k)
    {
        const auto f = k * fs / nfft;

        if (f < 100.0 || f > 16000.0)
            continue;

        const auto lo = std::max (1, (int) (k * std::pow (2.0, -1.0 / 6.0)));
        const auto hi = std::min ((int) psd.size() - 1, (int) std::ceil (k * std::pow (2.0, 1.0 / 6.0)));
        auto mean = 0.0;

        for (int j = lo; j <= hi; ++j)
            mean += psd[(size_t) j];

        mean /= (hi - lo + 1);
        const auto ratio = 10.0 * std::log10 (psd[(size_t) k] / mean);

        if (ratio > worst)
        {
            worst = ratio;
            at = f;
        }
    }

    std::printf ("room metallic (quality %d): worst peak %+.2f dB over the 1/3-octave average at %.0f Hz (limit +6)\n", quality, worst, at);
    CHECK (allFinite (out));
    CHECK (worst <= 6.0);
}

TEST_CASE ("Room: RT60 (T30) within 10 % of Decay at Size 20, 55 and 90 %", "[room][rt60]")
{
    const auto size = GENERATE (0.2, 0.55, 0.9);
    const auto decay = GENERATE (0.5, 2.0, 8.0);

    RoomDsp::Targets targets;
    targets.size = size;
    targets.decay = decay;

    const auto out = render (targets, impulse(), impulse(), (int) ((1.3 * decay + 0.4) * fs));
    const auto measured = tMid (out);
    const auto error = 100.0 * (measured / decay - 1.0);

    std::printf ("room rt60: Size %3.0f %% Decay %4.1f s: T_mid %6.3f s (%+5.1f %%, limit 10)\n", size * 100.0, decay, measured, error);
    CHECK (std::abs (error) <= 10.0);
}

TEST_CASE ("Room: Freeze holds the level within 1 dB for 60 s, with no NaN and no growth", "[room][freeze]")
{
    const auto quality = GENERATE (0, 1, 2);

    RoomDsp::Targets targets;
    targets.quality = quality;
    auto frozen = targets;
    frozen.freeze = true;

    constexpr int seconds = 62;
    const auto out = render (targets, noise (2 * (int) fs, -12.0, 1), noise (2 * (int) fs, -12.0, 2), seconds * (int) fs,
                             { { (int) (1.5 * fs), frozen } });

    std::vector<double> levels;

    for (int s = 2; s < seconds; ++s)
        levels.push_back (windowDb (out, s * (int) fs, (s + 1) * (int) fs));

    auto deviation = 0.0;

    for (const auto level : levels)
        deviation = std::max (deviation, std::abs (level - levels.front()));

    std::printf ("room freeze (quality %d): 1 s levels over 60 s within %.3f dB of the first (limit 1), last - first %+.4f dB\n",
                 quality, deviation, levels.back() - levels.front());
    CHECK (allFinite (out));
    CHECK (deviation <= 1.0);
    CHECK (levels.back() - levels.front() <= 0.5);
}

TEST_CASE ("Room: at Width 1.00 the tail's mono sum is within 3 dB of its stereo energy", "[room][mono]")
{
    const auto out = render ({}, impulse(), impulse(), (int) (2.0 * fs));
    double mono = 0, stereo = 0;

    for (auto i = (size_t) (0.05 * fs); i < out.left.size(); ++i)
    {
        mono += (out.left[i] + out.right[i]) * (out.left[i] + out.right[i]);
        stereo += out.left[i] * out.left[i] + out.right[i] * out.right[i];
    }

    const auto db = 10.0 * std::log10 (mono / stereo);
    std::printf ("room mono: tail mono sum %+.2f dB against stereo energy (limit 3)\n", db);
    CHECK (std::abs (db) <= 3.0);
}

TEST_CASE ("Room: the reported tail holds everything down to -90 dB", "[room][tail]")
{
    const auto decay = GENERATE (0.5, 1.8, 8.0);

    RoomDsp::Targets targets;
    targets.decay = decay;

    const auto tail = RoomDsp::tailSeconds (targets.predelayMs, targets.size, decay);
    const auto total = (int) ((1.0 + tail + 0.3) * fs);
    const auto out = render (targets, noise ((int) fs, -6.0, 3), noise ((int) fs, -6.0, 4), total);

    const auto window = (int) (0.05 * fs);
    std::vector<double> levels;

    for (int s = 0; s + window <= total; s += window)
        levels.push_back (windowDb (out, s, s + window));

    const auto peak = *std::max_element (levels.begin(), levels.end());
    const auto end = (size_t) ((1.0 + tail) * fs) / (size_t) window;
    const auto after = *std::max_element (levels.begin() + (std::ptrdiff_t) end, levels.end()) - peak;

    std::printf ("room tail: Decay %4.1f s, reported tail %6.3f s; loudest 50 ms after it %+.1f dB re peak (limit -90)\n", decay, tail, after);
    CHECK (after <= -90.0);
}

// The plugin's own checks ---------------------------------------------------------------------

TEST_CASE ("Room: random block sizes give the same output, bit for bit, through changes", "[room][blocks]")
{
    RoomDsp::Targets targets;
    targets.ducking = 0.5;
    targets.dryGain = 0.5;

    auto moved = targets;
    moved.size = 0.8;
    moved.predelayMs = 70.0;
    moved.decay = 3.0;
    moved.lowCutHz = 400.0;

    auto frozen = moved;
    frozen.freeze = true;

    auto eco = targets;
    eco.quality = RoomDsp::eco;

    const std::vector<Change> changes { { 12345, moved }, { 30000, frozen }, { 50001, eco }, { 70000, targets } };
    const auto input = noise ((int) fs, -18.0, 9);

    const auto steady = render (targets, input, input, 2 * (int) fs, changes, fixedBlocks (512));
    const auto random = render (targets, input, input, 2 * (int) fs, changes, randomBlocks (23));

    CHECK (allFinite (steady));
    CHECK (steady.left == random.left);
    CHECK (steady.right == random.right);
}

TEST_CASE ("Room: a Quality change lets the old tail ring out, then stops the old network", "[room][quality]")
{
    RoomDsp::Targets targets;
    targets.decay = 8.0;

    RoomDsp dsp;
    dsp.setTargets (targets);
    dsp.prepare (fs);

    const auto input = noise ((int) fs, -12.0, 5);
    std::vector<float> left ((size_t) (5 * fs), 0.0f), right ((size_t) (5 * fs), 0.0f);
    std::copy (input.begin(), input.end(), left.begin());
    std::copy (input.begin(), input.end(), right.begin());

    const auto block = 480;
    const auto switchAt = (int) (1.5 * fs);
    auto runningAfterSwitch = 0, runningAt4s = 0;

    for (int position = 0; position < (int) left.size(); position += block)
    {
        if (position == switchAt)
        {
            targets.quality = RoomDsp::eco;
            dsp.setTargets (targets);
            runningAfterSwitch = dsp.getNumRunningNetworks();
        }

        float* channels[] { left.data() + position, right.data() + position };
        dsp.process (channels, 2, block);

        if (position == 4 * (int) fs)
            runningAt4s = dsp.getNumRunningNetworks();
    }

    const Stereo out { std::vector<double> (left.begin(), left.end()), std::vector<double> (right.begin(), right.end()) };
    const auto before = windowDb (out, switchAt - 2400, switchAt);
    const auto after = windowDb (out, switchAt, switchAt + 2400);

    std::printf ("room quality change: 50 ms before %.1f dB, 50 ms after %.1f dB\n", before, after);
    CHECK (dsp.getActiveQuality() == RoomDsp::eco);
    CHECK (runningAfterSwitch == 2);
    CHECK (runningAt4s == 1);           // the old one stops by 2 s after the change, with its fade
    CHECK (after > before - 1.0);       // the tail doesn't drop out
    CHECK (allFinite (out));
}

TEST_CASE ("Room: Width never changes the mono sum, and Mono-sum feeds both sides the same", "[room][width]")
{
    const auto input = noise ((int) (0.5 * fs), -12.0, 6);
    std::vector<float> silent (input.size(), 0.0f);

    RoomDsp::Targets targets;
    Stereo sums[3];
    auto index = 0;

    for (const auto w : { 0.0, 1.0, 2.0 })
    {
        targets.width = w;
        sums[index++] = render (targets, input, silent, (int) fs);
    }

    auto worst = 0.0;

    for (int k = 1; k < 3; ++k)
        for (size_t i = 0; i < sums[0].left.size(); ++i)
            worst = std::max (worst, std::abs ((sums[k].left[i] + sums[k].right[i]) - (sums[0].left[i] + sums[0].right[i])));

    CHECK (worst < 1.0e-6);

    // Width 0 is mono.
    auto side = 0.0;

    for (size_t i = 0; i < sums[0].left.size(); ++i)
        side = std::max (side, std::abs (sums[0].left[i] - sums[0].right[i]));

    CHECK (side < 1.0e-6);

    // Mono-sum: left only in, or the same signal split over both, sound the same.
    targets.width = 1.0;
    targets.monoSum = true;
    const auto oneSide = render (targets, input, silent, (int) fs);
    std::vector<float> half (input.size());

    for (size_t i = 0; i < input.size(); ++i)
        half[i] = 0.5f * input[i];

    const auto bothSides = render (targets, half, half, (int) fs);
    auto difference = 0.0;

    for (size_t i = 0; i < oneSide.left.size(); ++i)
        difference = std::max (difference, std::abs (oneSide.left[i] - bothSides.left[i]));

    CHECK (difference < 1.0e-6);
}

TEST_CASE ("Room: Ducking turns the reverb down while the dry signal plays", "[room][ducking]")
{
    RoomDsp::Targets targets;
    targets.decay = 3.0;

    auto ducked = targets;
    ducked.ducking = 1.0;

    const auto input = noise ((int) fs, -10.0, 7);
    const auto plain = render (targets, input, input, 3 * (int) fs);
    const auto duck = render (ducked, input, input, 3 * (int) fs);

    // While the input plays above -20 dBFS: 24 dB down. Once it stops: back up, at 300 ms.
    const auto during = windowDb (duck, (int) (0.6 * fs), (int) (0.9 * fs)) - windowDb (plain, (int) (0.6 * fs), (int) (0.9 * fs));
    const auto later = windowDb (duck, (int) (2.5 * fs), (int) (2.8 * fs)) - windowDb (plain, (int) (2.5 * fs), (int) (2.8 * fs));

    std::printf ("room ducking 100 %%: %.1f dB while playing, %.1f dB 1.5 s after\n", during, later);
    CHECK (during == Approx (-24.0).margin (1.5));
    CHECK (later > -1.0);
}

TEST_CASE ("Room: the processor's tail, Wet only, Sync pre-delay and read-outs", "[room]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    RoomProcessor processor;

    // Decision 3.
    CHECK (processor.getTailLengthSeconds() == Approx (RoomDsp::tailSeconds (12.0, 0.55, 1.8)));
    setValue (processor, "decay", 8.0f);
    CHECK (processor.getTailLengthSeconds() == Approx (RoomDsp::tailSeconds (12.0, 0.55, 8.0)));
    setValue (processor, "freeze", 1.0f);
    CHECK (std::isinf (processor.getTailLengthSeconds()));
    setValue (processor, "freeze", 0.0f);

    // Decision 1: Wet only treats Dry as -inf and keeps its value.
    setValue (processor, "dry", -3.0f);
    setValue (processor, "wetonly", 1.0f);
    CHECK (processor.readTargets().dryGain == 0.0);
    CHECK (processor.read ("dry") == Approx (-3.0f));
    setValue (processor, "wetonly", 0.0f);
    CHECK (processor.readTargets().dryGain == Approx (std::pow (10.0, -3.0 / 20.0)).epsilon (1.0e-4));

    // Decision 5: notes at the tempo, capped at 250 ms, the ms value kept.
    processor.setTempoForTesting (120.0);
    setValue (processor, "predelay", 40.0f);
    setValue (processor, "sync", 1.0f);
    CHECK (processor.getPredelayMs() == Approx (62.5)); // 1/32 at 120 BPM
    CHECK_FALSE (processor.isPredelayCapped());
    setValue (processor, "prenote", 10.0f);             // 1/8 dotted, 375 ms
    CHECK (RoomProcessor::noteName (10) == "1/8 dotted");
    CHECK (processor.getPredelayMs() == Approx (250.0));
    CHECK (processor.isPredelayCapped());
    setValue (processor, "sync", 0.0f);
    CHECK (processor.getPredelayMs() == Approx (40.0));

    // Read-outs as the mockup's.
    auto& apvts = processor.getApvts();
    const auto text = [&] (const char* id, float value)
    {
        auto* parameter = apvts.getParameter (id);
        return parameter->getText (parameter->convertTo0to1 (value), 32);
    };

    CHECK (text ("predelay", 12.0f) == "12 ms");
    CHECK (text ("decay", 1.8f) == "1.80 s");
    CHECK (text ("decay", 12.5f) == "12.5 s");
    CHECK (text ("width", 1.0f) == "1.00");
    CHECK (text ("lowcut", 150.0f) == "150 Hz");
    CHECK (text ("highcut", 9000.0f) == "9.0 kHz");

    // Width's 1.00 sits in the middle of the knob, its default mark.
    CHECK (apvts.getParameter ("width")->convertTo0to1 (1.0f) == Approx (0.5f));
    CHECK (apvts.getParameter ("width")->convertFrom0to1 (0.0f) == Approx (-1.0f));
    CHECK (apvts.getParameter ("width")->convertFrom0to1 (1.0f) == Approx (2.0f));
    CHECK (apvts.getParameter ("decay")->getValueForText ("500 ms") == Approx (apvts.getParameter ("decay")->convertTo0to1 (0.5f)));
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

template <typename Process>
double measure (Process&& process)
{
    float left[measureBlock], right[measureBlock];
    float* channels[] { left, right };
    std::mt19937 random (8);
    std::uniform_real_distribution<float> noiseValue (-0.25f, 0.25f);
    std::chrono::duration<double> spent {};

    for (int block = 0; block < measureBlocks; ++block)
    {
        for (int i = 0; i < measureBlock; ++i)
        {
            left[i] = noiseValue (random);
            right[i] = noiseValue (random);
        }

        const auto start = std::chrono::steady_clock::now();
        process (channels);
        spent += std::chrono::steady_clock::now() - start;
    }

    return 100.0 * spent.count() / ((double) (measureBlocks * measureBlock) / fs);
}

double roomCpu (const RoomDsp::Targets& targets)
{
    RoomDsp dsp;
    dsp.setTargets (targets);
    dsp.prepare (fs);
    return measure ([&] (float* const* channels) { dsp.process (channels, 2, measureBlock); });
}
} // namespace

TEST_CASE ("room measurements", "[.measure]")
{
    RoomDsp::Targets normal;
    normal.dryGain = 1.0;
    normal.wetGain = 0.25;
    auto eco = normal;
    eco.quality = RoomDsp::eco;
    auto high = normal;
    high.quality = RoomDsp::high;
    auto busy = normal;
    busy.freeze = true;
    busy.ducking = 0.5;

    const auto cpuNormal = medianOfFive ([&] { return roomCpu (normal); });
    const auto cpuEco = medianOfFive ([&] { return roomCpu (eco); });
    const auto cpuHigh = medianOfFive ([&] { return roomCpu (high); });
    const auto cpuBusy = medianOfFive ([&] { return roomCpu (busy); });

    std::printf ("room cpu (median of 5): Normal (target < 3 %%) %.3f %%, Eco %.3f %%, High %.3f %%, Normal with Freeze and Ducking %.3f %%\n",
                 cpuNormal, cpuEco, cpuHigh, cpuBusy);

   #if FANCY_HAS_COMP
    const auto comp = medianOfFive ([]
    {
        CompDsp compDsp;
        compDsp.prepare (fs);
        compDsp.snapToTargets (CompDsp::Targets {});
        return measure ([&] (float* const* channels) { compDsp.process (channels, 2, nullptr, 0, measureBlock); });
    });

    std::printf ("room cpu reference: Fancy Comp default %.3f %% in the same run; Normal / comp = %.2f, Eco / comp = %.2f, High / comp = %.2f\n",
                 comp, cpuNormal / comp, cpuEco / comp, cpuHigh / comp);
   #endif

    SUCCEED();
}
