// Phase 4 Wave 3 for Fancy Room (docs/room.md, "Phase 4 Wave 3: shimmer", section 6): Shimmer and
// its Interval, and the legacy check against v1.7.1's engine. Tests 2 to 5 measure as
// docs/research/room-w3-prepush.py does, which ports the loop as built onto room-model.py.

#include "LegacyRoomW2Dsp.h"
#include "RoomDsp.h"
#include "RoomProcessor.h"

#include <juce_dsp/juce_dsp.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr int second = 48000;
constexpr double pi = std::numbers::pi;

RoomDsp::Targets wetOnly()
{
    RoomDsp::Targets targets;
    targets.wetGain = 1.0;
    targets.dryGain = 0.0;
    return targets;
}

RoomDsp::Targets shimmering (double amount, double semitones)
{
    auto targets = wetOnly();
    targets.shimmer = amount;
    targets.shimmerRatio = std::exp2 (semitones / 12.0);
    return targets;
}

struct Stereo
{
    std::vector<float> left, right;
};

// The same input on both sides (or its own right side), in blocks of `block`; freezeAt switches
// Freeze on at that sample.
template <typename Dsp, typename Targets>
Stereo render (Targets targets, const std::vector<float>& inLeft, const std::vector<float>& inRight, int total, int block = 480,
               int freezeAt = -1)
{
    Dsp dsp;
    dsp.setTargets (targets);
    dsp.prepare (rate);

    Stereo out { std::vector<float> ((size_t) total, 0.0f), std::vector<float> ((size_t) total, 0.0f) };
    std::copy_n (inLeft.begin(), std::min ((int) inLeft.size(), total), out.left.begin());
    std::copy_n (inRight.begin(), std::min ((int) inRight.size(), total), out.right.begin());

    for (int position = 0; position < total; position += block)
    {
        const auto count = std::min (block, total - position);

        if (freezeAt >= 0 && position >= freezeAt && ! targets.freeze)
        {
            targets.freeze = true;
            dsp.setTargets (targets);
        }

        float* channels[] { out.left.data() + position, out.right.data() + position };
        dsp.process (channels, 2, count);
    }

    return out;
}

// As the model's note(): a sine of `seconds` with 5 ms linear fades.
std::vector<float> note (double hz, double seconds = 0.4, double amplitude = 0.5)
{
    const auto n = (int) (seconds * rate);
    const auto fade = (int) (0.005 * rate);
    std::vector<float> x ((size_t) n);

    for (int i = 0; i < n; ++i)
    {
        auto v = amplitude * std::sin (2.0 * pi * hz * (double) i / rate);

        if (i < fade)
            v *= (double) i / (double) (fade - 1);
        else if (i >= n - fade)
            v *= (double) (n - 1 - i) / (double) (fade - 1);

        x[(size_t) i] = (float) v;
    }

    return x;
}

// 50 ms mean of L^2 + R^2, in dB, from the start.
std::vector<double> envelopeDb (const Stereo& out)
{
    const auto window = (size_t) (0.05 * rate);
    std::vector<double> levels;

    for (size_t from = 0; from + window <= out.left.size(); from += window)
    {
        auto sum = 0.0;

        for (auto i = from; i < from + window; ++i)
            sum += (double) out.left[i] * out.left[i] + (double) out.right[i] * out.right[i];

        levels.push_back (10.0 * std::log10 (sum / (double) window + 1.0e-30));
    }

    return levels;
}

// One Hann-windowed FFT of x[from, from + 2^order), as power per bin.
std::vector<double> powerSpectrum (const std::vector<float>& x, size_t from, int order)
{
    const auto n = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> data ((size_t) (2 * n), 0.0f);

    for (int i = 0; i < n; ++i)
        data[(size_t) i] = x[from + (size_t) i] * (float) (0.5 - 0.5 * std::cos (2.0 * pi * (double) i / (double) n));

    fft.performRealOnlyForwardTransform (data.data(), true);

    std::vector<double> power ((size_t) (n / 2 + 1));

    for (size_t k = 0; k < power.size(); ++k)
        power[k] = (double) data[2 * k] * data[2 * k] + (double) data[2 * k + 1] * data[2 * k + 1];

    return power;
}

// Power within hz x 2^(+-width/2 octave), in dB, from one FFT of 2^15 samples at `from`.
double bandDb (const std::vector<float>& x, size_t from, double hz, double width = 0.25)
{
    constexpr int order = 15;
    const auto power = powerSpectrum (x, from, order);
    auto sum = 0.0;

    for (size_t k = 0; k < power.size(); ++k)
    {
        const auto f = (double) k * rate / (double) (1 << order);

        if (f >= hz * std::exp2 (-width / 2.0) && f < hz * std::exp2 (width / 2.0))
            sum += power[k];
    }

    return 10.0 * std::log10 (sum + 1.0e-30);
}

// Welch: Hann segments of 4096, half overlapped, averaged, summed per third octave of 1 kHz x
// 2^(k/3), k = -17 to 12.
std::vector<double> thirdOctaves (const std::vector<float>& x, size_t from, size_t to)
{
    constexpr int order = 12, n = 1 << order;
    std::vector<double> average ((size_t) (n / 2 + 1), 0.0);
    auto count = 0;

    for (auto start = from; start + (size_t) n <= to; start += (size_t) (n / 2))
    {
        const auto power = powerSpectrum (x, start, order);

        for (size_t k = 0; k < average.size(); ++k)
            average[k] += power[k];

        ++count;
    }

    std::vector<double> bands;

    for (int b = -17; b <= 12; ++b)
    {
        const auto centre = 1000.0 * std::exp2 ((double) b / 3.0);
        auto sum = 0.0;

        for (size_t k = 0; k < average.size(); ++k)
        {
            const auto hz = (double) k * rate / (double) n;

            if (hz >= centre * std::exp2 (-1.0 / 6.0) && hz < centre * std::exp2 (1.0 / 6.0))
                sum += average[k] / std::max (1, count);
        }

        bands.push_back (10.0 * std::log10 (sum + 1.0e-30));
    }

    return bands;
}

LegacyRoomW2Dsp::Targets legacyOf (const RoomDsp::Targets& t)
{
    LegacyRoomW2Dsp::Targets old;
    old.predelayMs = t.predelayMs;
    old.size = t.size;
    old.decay = t.decay;
    old.diffusion = t.diffusion;
    old.mod = t.mod;
    old.width = t.width;
    old.damping = t.damping;
    old.lowCutHz = t.lowCutHz;
    old.highCutHz = t.highCutHz;
    old.freeze = t.freeze;
    old.ducking = t.ducking;
    old.quality = t.quality;
    old.erGain = t.erGain;
    old.monoSum = t.monoSum;
    old.wetGain = t.wetGain;
    old.dryGain = t.dryGain;
    old.outputGain = t.outputGain;
    old.bypass = t.bypass;
    old.modRate = t.modRate;
    old.eqLowHz = t.eqLowHz;
    old.eqLowDb = t.eqLowDb;
    old.eqMidHz = t.eqMidHz;
    old.eqMidDb = t.eqMidDb;
    old.eqMidQ = t.eqMidQ;
    old.eqHighHz = t.eqHighHz;
    old.eqHighDb = t.eqHighDb;
    old.gate = t.gate;
    old.gateThresholdDb = t.gateThresholdDb;
    old.gateHoldMs = t.gateHoldMs;
    old.gateDecayMs = t.gateDecayMs;
    old.autoGain = t.autoGain;
    old.lowDecay = t.lowDecay;
    old.attackMs = t.attackMs;
    old.duckSidechain = t.duckSidechain;
    old.listenDucking = t.listenDucking;
    return old;
}

std::vector<float> uniformNoise (int length, unsigned seed, float amplitude = 0.5f)
{
    std::mt19937 generator (seed);
    std::uniform_real_distribution<float> distribution (-amplitude, amplitude);
    std::vector<float> samples ((size_t) length);

    for (auto& sample : samples)
        sample = distribution (generator);

    return samples;
}
} // namespace

// 1. Legacy -----------------------------------------------------------------------------------

TEST_CASE ("Room W3: at Shimmer 0 the engine renders as v1.7.1's, bit for bit", "[room][w3]")
{
    const auto input = uniformNoise (24000, 3);
    std::vector<float> right (input);

    for (auto& sample : right)
        sample *= 0.7f;

    for (const auto quality : { (int) RoomDsp::eco, (int) RoomDsp::normal, (int) RoomDsp::high })
    {
        for (const auto freeze : { false, true })
        {
            for (const auto shaped : { false, true })
            {
                INFO ("quality " << quality << ", freeze " << freeze << ", ducking, gate, Low decay and Attack " << shaped);

                auto targets = wetOnly();
                targets.dryGain = 0.5;
                targets.quality = quality;
                targets.freeze = freeze;
                targets.mod = 0.4;
                targets.eqMidDb = 3.0;
                targets.autoGain = true;
                targets.ducking = shaped ? 0.6 : 0.0;
                targets.gate = shaped;
                targets.lowDecay = shaped ? 2.0 : 1.0;
                targets.attackMs = shaped ? 150.0 : 0.0;
                targets.shimmerRatio = std::exp2 (7.0 / 12.0); // Interval does nothing at 0 %

                const auto now = render<RoomDsp> (targets, input, right, 60000, 333);
                const auto old = render<LegacyRoomW2Dsp> (legacyOf (targets), input, right, 60000, 333);
                REQUIRE (now.left == old.left);
                REQUIRE (now.right == old.right);
            }
        }
    }
}

TEST_CASE ("Room W3: Shimmer and Interval are appended, and their defaults are the engine's", "[room][w3]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    RoomProcessor processor;
    auto& apvts = processor.getApvts();

    for (const auto* id : { "shimmer", "shimmerPitch" })
        REQUIRE (apvts.getParameter (id) != nullptr);

    CHECK (apvts.getRawParameterValue ("shimmer")->load() == 0.0f);
    CHECK (apvts.getRawParameterValue ("shimmerPitch")->load() == 0.0f);
    CHECK (processor.readTargets().shimmer == 0.0);
    CHECK (processor.readTargets().shimmerRatio == 2.0);

    const auto indexOf = [&] (const char* id) { return apvts.getParameter (id)->getParameterIndex(); };
    CHECK (indexOf ("shimmer") == indexOf ("duckFrom") + 1);
    CHECK (indexOf ("shimmerPitch") == indexOf ("duckFrom") + 2);
    CHECK (indexOf ("shimmerPitch") == processor.getParameters().size() - 1);

    // The six intervals, in the menu's order, as Verb's.
    auto* interval = apvts.getParameter ("shimmerPitch");
    const auto choose = [&] (int index)
    {
        interval->setValueNotifyingHost (interval->convertTo0to1 ((float) index));
        return processor.readTargets().shimmerRatio;
    };

    CHECK (std::abs (choose (1) - std::exp2 (7.0 / 12.0)) < 1.0e-12);
    CHECK (std::abs (choose (3) - std::exp2 (19.0 / 12.0)) < 1.0e-12);
    CHECK (std::abs (choose (4) - 4.0) < 1.0e-12);
    CHECK (std::abs (choose (5) - 0.5) < 1.0e-12);

    // The host's tail triples while Shimmer is up.
    const auto plain = processor.getTailLengthSeconds();
    auto* amount = apvts.getParameter ("shimmer");
    amount->setValueNotifyingHost (amount->convertTo0to1 (50.0f));
    CHECK (std::abs (processor.getTailLengthSeconds() - 3.0 * plain) < 1.0e-9);
}

// The loop gain's energy (docs/research/room-w3-prepush.py, `energy`): never under the network's
// own impulse response, so Shimmer 100 % passes at most 0.81 of the energy round the loop.
TEST_CASE ("Room W3: the loop energy is at least what the late network passes", "[room][w3]")
{
    std::vector<float> impulse (1, 1.0f);

    for (const auto quality : { (int) RoomDsp::eco, (int) RoomDsp::normal })
    {
        for (const auto [size, decay] : { std::pair { 0.55, 1.8 }, std::pair { 0.0, 3.0 }, std::pair { 1.0, 6.0 } })
        {
            auto targets = wetOnly();
            targets.quality = quality;
            targets.size = size;
            targets.decay = decay;
            targets.damping = 0.0;
            targets.mod = 0.0;
            targets.erGain = 0.0;
            targets.predelayMs = 0.0;
            targets.lowCutHz = 10.0;
            targets.highCutHz = 20000.0;

            const auto out = render<RoomDsp> (targets, impulse, impulse, (int) ((1.5 * decay + 0.5) * rate));
            auto left = 0.0, right = 0.0;

            for (size_t i = 0; i < out.left.size(); ++i)
            {
                left += (double) out.left[i] * out.left[i];
                right += (double) out.right[i] * out.right[i];
            }

            const auto passed = 0.81 * std::max (left, right) / RoomDsp::loopEnergy (size, decay, quality == RoomDsp::eco ? 8 : 16);
            CAPTURE (quality, size, decay, left, right, passed);
            CHECK (passed < 0.85);
        }
    }
}

// 2. Rising octaves that die away ----------------------------------------------------------

TEST_CASE ("Room W3: Shimmer +12 at 100 % rises to 3.5 and 7 kHz and still dies away", "[room][w3]")
{
    const auto x = note (220.0);
    const auto out = render<RoomDsp> (shimmering (1.0, 12.0), x, x, 9 * second);

    for (size_t i = 0; i < out.left.size(); ++i)
        REQUIRE ((std::isfinite (out.left[i]) && std::isfinite (out.right[i])));

    // 2 s after the note (the model: 880 -3.7, 1760 -5.2, 3520 0, 7040 -2.1 dB).
    const auto at = (size_t) (2.4 * rate);
    std::vector<double> bands;

    for (const auto hz : { 220.0, 440.0, 880.0, 1760.0, 3520.0, 7040.0, 14080.0 })
        bands.push_back (bandDb (out.left, at, hz));

    const auto strongest = *std::max_element (bands.begin(), bands.end());
    CAPTURE (bands[0] - strongest, bands[1] - strongest, bands[2] - strongest, bands[3] - strongest, bands[4] - strongest,
             bands[5] - strongest, bands[6] - strongest);
    CHECK (bands[4] > strongest - 12.0);
    CHECK (bands[5] > strongest - 12.0);

    // Falls 60 dB within 6 s of the note's end (the model: 2.35 s).
    const auto levels = envelopeDb (out);
    const auto peak = *std::max_element (levels.begin(), levels.end());
    auto quietFrom = levels.size();

    while (quietFrom > 0 && levels[quietFrom - 1] < peak - 60.0)
        --quietFrom;

    const auto fallSeconds = (double) quietFrom * 0.05 - 0.4;
    CAPTURE (fallSeconds);
    CHECK (quietFrom < levels.size());
    CHECK (fallSeconds < 6.0);
}

// 3. The guard ------------------------------------------------------------------------------

TEST_CASE ("Room W3: the guard keeps a held note's bloom within 10 dB", "[room][w3]")
{
    for (const auto hz : { 196.0, 220.0, 246.9, 261.6, 329.6, 392.0 })
    {
        const auto x = note (hz);
        const auto plain = envelopeDb (render<RoomDsp> (wetOnly(), x, x, 5 * second));
        const auto shimmered = envelopeDb (render<RoomDsp> (shimmering (1.0, 7.0), x, x, 5 * second));

        // The loudest 50 ms from the note's end on (the model: +0.1 to +0.9 dB).
        const auto from = (size_t) 8;
        const auto rise = *std::max_element (shimmered.begin() + (std::ptrdiff_t) from, shimmered.end())
                        - *std::max_element (plain.begin() + (std::ptrdiff_t) from, plain.end());
        CAPTURE (hz, rise);
        CHECK (rise < 10.0);
    }
}

// 4. Freeze holds the shimmer ---------------------------------------------------------------

TEST_CASE ("Room W3: frozen with Shimmer 100 %, the level and spectrum hold", "[room][w3]")
{
    std::mt19937 generator (3);
    std::normal_distribution<float> gauss (0.0f, 0.25f);
    std::vector<float> left ((size_t) (2 * second)), right ((size_t) (2 * second));

    for (size_t i = 0; i < left.size(); ++i)
    {
        left[i] = gauss (generator);
        right[i] = gauss (generator);
    }

    const auto out = render<RoomDsp> (shimmering (1.0, 12.0), left, right, 13 * second, 480, (int) (2.5 * rate));

    // 1 s levels within 1 dB, as the plain Freeze test (the model: 0.11 dB).
    std::vector<double> levels;

    for (int s = 3; s < 13; ++s)
    {
        auto sum = 0.0;

        for (auto i = (size_t) (s * second); i < (size_t) ((s + 1) * second); ++i)
            sum += (double) out.left[i] * out.left[i] + (double) out.right[i] * out.right[i];

        levels.push_back (10.0 * std::log10 (sum / second + 1.0e-30));
    }

    const auto span = *std::max_element (levels.begin(), levels.end()) - *std::min_element (levels.begin(), levels.end());
    CAPTURE (span);
    CHECK (span < 1.0);

    // Third octaves of 125 Hz to 8 kHz within 30 dB of the strongest don't climb from seconds 3-5
    // to 11-13 (the model: -0.41 to +0.27 dB; Shimmer 0 moves -0.25 to +0.31).
    const auto first = thirdOctaves (out.left, (size_t) (3 * second), (size_t) (5 * second));
    const auto last = thirdOctaves (out.left, (size_t) (11 * second), (size_t) (13 * second));
    const auto strongest = *std::max_element (first.begin(), first.end());
    auto worst = 0.0;

    for (int b = -17; b <= 12; ++b)
    {
        const auto index = (size_t) (b + 17);
        const auto centre = 1000.0 * std::exp2 ((double) b / 3.0);

        if (centre >= 125.0 && centre <= 8000.0 && first[index] > strongest - 30.0)
            worst = std::max (worst, std::abs (last[index] - first[index]));
    }

    CAPTURE (worst);
    CHECK (worst < 1.0);
}

// 5. No folding -----------------------------------------------------------------------------

TEST_CASE ("Room W3: Two octaves up on 6 kHz folds nothing below 5 kHz", "[room][w3]")
{
    std::vector<float> tone ((size_t) (second / 2));

    for (size_t i = 0; i < tone.size(); ++i)
        tone[i] = (float) (0.5 * std::sin (2.0 * pi * 6000.0 * (double) i / rate));

    const auto out = render<RoomDsp> (shimmering (1.0, 24.0), tone, tone, 4 * second);

    // 2 s after the tone, against the same tone's own peak bin (the model: -136 dB).
    constexpr int order = 15;
    const auto power = powerSpectrum (out.left, (size_t) (2.5 * rate), order);

    std::vector<float> reference ((size_t) (1 << order));

    for (size_t i = 0; i < reference.size(); ++i)
        reference[i] = (float) (0.5 * std::sin (2.0 * pi * 6000.0 * (double) i / rate));

    const auto tonePower = powerSpectrum (reference, 0, order);
    const auto peak = *std::max_element (tonePower.begin(), tonePower.end());
    auto below = 0.0;

    for (size_t k = 0; k < power.size(); ++k)
        if ((double) k * rate / (double) (1 << order) < 5000.0)
            below = std::max (below, power[k]);

    const auto db = 10.0 * std::log10 (below / peak + 1.0e-30);
    CAPTURE (db);
    CHECK (db < -80.0);
}

// 6. Block sizes ----------------------------------------------------------------------------

TEST_CASE ("Room W3: with Shimmer 75 %, random block sizes give the same output, bit for bit", "[room][w3]")
{
    const auto input = uniformNoise (3 * second, 11, 0.3f);
    const auto total = 5 * second;

    // Shimmer moves from 75 to 40 % at 2 s, so the smoothing and the knots are crossed too; both
    // renders change it at that sample.
    const auto run = [&] (auto&& nextBlock)
    {
        auto targets = shimmering (0.75, 12.0);
        targets.dryGain = 0.5;

        RoomDsp dsp;
        dsp.setTargets (targets);
        dsp.prepare (rate);

        Stereo out { std::vector<float> ((size_t) total, 0.0f), std::vector<float> ((size_t) total, 0.0f) };
        std::copy (input.begin(), input.end(), out.left.begin());
        std::copy (input.begin(), input.end(), out.right.begin());

        for (int position = 0; position < total;)
        {
            if (position == 2 * second)
            {
                targets.shimmer = 0.4;
                dsp.setTargets (targets);
            }

            auto count = std::min (nextBlock(), total - position);

            if (position < 2 * second)
                count = std::min (count, 2 * second - position);

            float* channels[] { out.left.data() + position, out.right.data() + position };
            dsp.process (channels, 2, count);
            position += count;
        }

        return out;
    };

    std::mt19937 generator (19);
    std::uniform_int_distribution<int> sizes (1, 1500);
    const auto random = run ([&] { return sizes (generator); });
    const auto single = run ([] { return 1; });
    const auto large = run ([] { return 8192; });

    REQUIRE (random.left == single.left);
    REQUIRE (random.right == single.right);
    REQUIRE (large.left == single.left);
    REQUIRE (large.right == single.right);
}
