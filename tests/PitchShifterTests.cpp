// fancy::dsp::PitchShifter, the shared shifter of Fancy Delay's pitched taps and Verb's and Room's
// shimmer (docs/delay.md, "Phase 4 Wave 3", section 3). The numbers were run first through
// test_shifter() in docs/research/delay-w3-prepush.py.

#include <fancy/dsp/PitchShifter.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

namespace
{
constexpr double pi = std::numbers::pi;

std::vector<double> shift (double rate, double hz, double semitones, double seconds, fancy::dsp::PitchShifter::Mode mode = fancy::dsp::PitchShifter::Mode::aligned)
{
    fancy::dsp::PitchShifter shifter;
    shifter.prepare (rate);
    shifter.setMode (mode);

    const auto ratio = std::exp2 (semitones / 12.0);
    std::vector<double> out ((size_t) (seconds * rate));

    for (size_t i = 0; i < out.size(); ++i)
    {
        const double in[2] { 0.5 * std::sin (2.0 * pi * hz * (double) i / rate), 0.5 * std::sin (2.0 * pi * hz * (double) i / rate) };
        double result[2];
        shifter.process (in, result, 2, ratio);
        out[i] = result[0];
    }

    return out;
}

// The strongest frequency near `guess`, from a Hann-windowed DFT searched coarse to fine.
double peakHz (const std::vector<double>& x, size_t from, double rate, double guess)
{
    const auto n = x.size() - from;

    const auto magnitude = [&] (double hz)
    {
        double re = 0.0, im = 0.0;
        const auto step = 2.0 * pi * hz / rate;

        for (size_t i = 0; i < n; ++i)
        {
            const auto w = 0.5 - 0.5 * std::cos (2.0 * pi * (double) i / (double) (n - 1));
            re += w * x[from + i] * std::cos (step * (double) i);
            im += w * x[from + i] * std::sin (step * (double) i);
        }

        return re * re + im * im;
    };

    auto best = guess;

    for (const auto [span, step] : { std::pair { 30.0, 1.0 }, std::pair { 1.0, 0.05 }, std::pair { 0.05, 0.005 } })
    {
        const auto centre = best;
        auto bestValue = -1.0;

        for (auto hz = centre - span; hz <= centre + span; hz += step)
            if (const auto value = magnitude (hz); value > bestValue)
                bestValue = value, best = hz;
    }

    return best;
}

// The level against the 0.5 input, in dB, and how far it wavers (2 ms RMS, max over min).
std::pair<double, double> levelAndWaver (const std::vector<double>& x, size_t from, double rate)
{
    const auto frame = (size_t) (0.002 * rate);
    auto sum = 0.0, window = 0.0, lowest = 1.0e30, highest = 0.0;

    for (size_t i = from; i < x.size(); ++i)
    {
        sum += x[i] * x[i];
        window += x[i] * x[i];

        if (i >= from + frame)
            window -= x[i - frame] * x[i - frame];

        if (i >= from + frame - 1)
        {
            const auto rms = std::sqrt (std::max (window, 0.0) / (double) frame);
            lowest = std::min (lowest, rms);
            highest = std::max (highest, rms);
        }
    }

    const auto level = 10.0 * std::log10 (sum / (double) (x.size() - from) / 0.125);
    return { level, 20.0 * std::log10 (highest / std::max (lowest, 1.0e-12)) };
}
} // namespace

TEST_CASE ("PitchShifter: +12 st turns 440 Hz into 880 Hz at the same level, steadily", "[pitchshifter]")
{
    constexpr double rate = 48000.0;
    const auto out = shift (rate, 440.0, 12.0, 1.0);
    const auto from = (size_t) (0.3 * rate);
    const auto [level, waver] = levelAndWaver (out, from, rate);

    // The model: 879.91 Hz, +0.00 dB, 0.79 dB (most of it the 2 ms frame against 880 Hz's period).
    CHECK (std::abs (peakHz (out, from, rate, 880.0) - 880.0) < 0.5);
    CHECK (std::abs (level) < 0.3);
    CHECK (waver < 2.0);
}

TEST_CASE ("PitchShifter: -12 st turns 1 kHz into 500 Hz", "[pitchshifter]")
{
    constexpr double rate = 48000.0;
    const auto out = shift (rate, 1000.0, -12.0, 1.0);
    const auto from = (size_t) (0.3 * rate);
    const auto [level, waver] = levelAndWaver (out, from, rate);

    CHECK (std::abs (peakHz (out, from, rate, 500.0) - 500.0) < 0.5);
    CHECK (std::abs (level) < 0.3);
    CHECK (waver < 2.0);
}

TEST_CASE ("PitchShifter: +7 st is right at every rate from 44.1 to 384 kHz", "[pitchshifter]")
{
    const auto rate = GENERATE (44100.0, 96000.0, 384000.0);
    const auto out = shift (rate, 1000.0, 7.0, rate > 200000.0 ? 0.6 : 1.0);
    const auto from = (size_t) (0.3 * rate);
    const auto [level, waver] = levelAndWaver (out, from, rate);

    INFO ("rate " << rate);
    CHECK (std::abs (peakHz (out, from, rate, 1498.31) - 1498.31) < 1.0);
    CHECK (std::abs (level) < 0.3);
    CHECK (waver < 2.0);
}

TEST_CASE ("PitchShifter: plain mode keeps the level", "[pitchshifter]")
{
    constexpr double rate = 48000.0;
    const auto out = shift (rate, 440.0, 12.0, 1.0, fancy::dsp::PitchShifter::Mode::plain);
    const auto [level, waver] = levelAndWaver (out, (size_t) (0.3 * rate), rate);

    // The model: -0.82 dB; plain grains warble (2.6 dB here), which is what Aligned is for.
    CHECK (std::abs (level) < 2.0);
    CHECK (std::all_of (out.begin(), out.end(), [] (double v) { return std::isfinite (v); }));
}

TEST_CASE ("PitchShifter: a glide across the whole range on loud noise stays finite and bounded", "[pitchshifter]")
{
    constexpr double rate = 48000.0;
    fancy::dsp::PitchShifter shifter;
    shifter.prepare (rate);

    std::mt19937 random (9);
    std::uniform_real_distribution<double> noise (-1.0, 1.0);
    auto peak = 0.0;
    auto finite = true;

    for (int i = 0; i < (int) (3.0 * rate); ++i)
    {
        // -12 to +12 and back, through 0 st, where a grain is longest.
        const auto semitones = 12.0 * std::sin (2.0 * pi * 0.5 * (double) i / rate);
        const double in[2] { noise (random), noise (random) };
        double out[2];
        shifter.process (in, out, 2, std::exp2 (semitones / 12.0));

        finite = finite && std::isfinite (out[0]) && std::isfinite (out[1]);
        peak = std::max ({ peak, std::abs (out[0]), std::abs (out[1]) });
    }

    CHECK (finite);
    CHECK (peak < 4.0);
}
