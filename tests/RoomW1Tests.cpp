// Phase 4 Wave 1 for Fancy Room (docs/room.md, "Phase 4 Wave 1", section 7): Mod rate, the
// reverb EQ, the gate and Auto gain, and the legacy check against v1.6.0's engine.

#include "LegacyRoomDsp.h"
#include "RoomDsp.h"
#include "RoomProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;

std::vector<float> noise (int length, unsigned seed, float amplitude = 0.5f)
{
    std::mt19937 generator (seed);
    std::uniform_real_distribution<float> distribution (-amplitude, amplitude);
    std::vector<float> samples ((size_t) length);

    for (auto& sample : samples)
        sample = distribution (generator);

    return samples;
}

template <typename Dsp, typename Targets>
std::vector<float> renderWith (const Targets& targets, const std::vector<float>& input, int total, int block = 480)
{
    Dsp dsp;
    dsp.setTargets (targets);
    dsp.prepare (rate);

    std::vector<float> left ((size_t) total, 0.0f), right ((size_t) total, 0.0f);
    std::copy_n (input.begin(), std::min ((int) input.size(), total), left.begin());

    for (int i = 0; i < std::min ((int) input.size(), total); ++i)
        right[(size_t) i] = 0.7f * input[(size_t) i];

    for (int position = 0; position < total; position += block)
    {
        const auto count = std::min (block, total - position);
        float* channels[] { left.data() + position, right.data() + position };
        dsp.process (channels, 2, count);
    }

    left.insert (left.end(), right.begin(), right.end());
    return left;
}

RoomDsp::Targets wetOnly()
{
    RoomDsp::Targets targets;
    targets.wetGain = 1.0;
    targets.dryGain = 0.0;
    return targets;
}

double magnitudeDbAt (const std::vector<float>& samples, int from, int to, double hz)
{
    std::complex<double> sum;

    for (int i = from; i < to; ++i)
        sum += (double) samples[(size_t) i] * std::polar (1.0, -2.0 * std::numbers::pi * hz * i / rate);

    return 20.0 * std::log10 (std::abs (sum));
}

double rmsDb (const std::vector<float>& samples, int from, int to)
{
    auto sum = 0.0;

    for (int i = from; i < to; ++i)
        sum += (double) samples[(size_t) i] * samples[(size_t) i];

    return 10.0 * std::log10 (sum / std::max (1, to - from));
}

LegacyRoomDsp::Targets legacyOf (const RoomDsp::Targets& t)
{
    LegacyRoomDsp::Targets old;
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
    return old;
}
} // namespace

// 1. Legacy -----------------------------------------------------------------------------------

TEST_CASE ("Room W1: at the new defaults the engine renders as v1.6.0's, bit for bit", "[room][w1]")
{
    const auto input = noise (24000, 3);

    for (const auto quality : { (int) RoomDsp::eco, (int) RoomDsp::normal, (int) RoomDsp::high })
    {
        for (const auto freeze : { false, true })
        {
            for (const auto ducking : { 0.0, 0.6 })
            {
                INFO ("quality " << quality << ", freeze " << freeze << ", ducking " << ducking);

                auto targets = wetOnly();
                targets.dryGain = 0.5;
                targets.quality = quality;
                targets.freeze = freeze;
                targets.ducking = ducking;
                targets.mod = 0.4;

                const auto now = renderWith<RoomDsp> (targets, input, 60000, 333);
                const auto old = renderWith<LegacyRoomDsp> (legacyOf (targets), input, 60000, 333);
                REQUIRE (now == old);
            }
        }
    }
}

TEST_CASE ("Room W1: the new parameters' defaults are the engine's", "[room][w1]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    RoomProcessor processor;
    const auto& apvts = processor.getApvts();

    for (const auto* id : { "modRate", "eqLowFreq", "eqLowGain", "eqMidFreq", "eqMidGain", "eqMidQ", "eqHighFreq", "eqHighGain",
                            "gate", "gateThr", "gateHold", "gateDecay", "autoGain" })
        REQUIRE (apvts.getParameter (id) != nullptr);

    const RoomDsp::Targets defaults;
    CHECK ((double) apvts.getRawParameterValue ("modRate")->load() == defaults.modRate);
    CHECK ((double) apvts.getRawParameterValue ("eqLowFreq")->load() == defaults.eqLowHz);
    CHECK ((double) apvts.getRawParameterValue ("eqMidGain")->load() == defaults.eqMidDb);
    CHECK ((double) apvts.getRawParameterValue ("gateThr")->load() == defaults.gateThresholdDb);
    CHECK (apvts.getRawParameterValue ("gate")->load() == 0.0f);
    CHECK (apvts.getRawParameterValue ("autoGain")->load() == 0.0f);
}

// 2. Mod rate ---------------------------------------------------------------------------------

TEST_CASE ("Room W1: Mod rate changes the modulation, smoothly, and nothing without Mod", "[room][w1]")
{
    const auto input = noise (12000, 4);

    auto targets = wetOnly();
    targets.mod = 0.0;
    const auto still = renderWith<RoomDsp> (targets, input, 48000);
    targets.modRate = 2.0;
    CHECK (renderWith<RoomDsp> (targets, input, 48000) == still);

    targets.mod = 1.0;
    targets.modRate = 1.0;
    const auto slow = renderWith<RoomDsp> (targets, input, 48000);
    targets.modRate = 4.0;
    const auto fast = renderWith<RoomDsp> (targets, input, 48000);
    CHECK (slow != fast);

    // Turning Mod rate while a tone rings: the output stays as smooth as without the move.
    std::vector<float> tone (96000);

    for (size_t i = 0; i < tone.size(); ++i)
        tone[i] = (float) (0.3 * std::sin (2.0 * std::numbers::pi * 440.0 * (double) i / rate));

    RoomDsp dsp;
    targets.modRate = 0.25;
    dsp.setTargets (targets);
    dsp.prepare (rate);

    auto steadiest = 0.0, moving = 0.0;
    std::vector<float> left (tone), right (tone);

    for (int position = 0; position < (int) tone.size(); position += 256)
    {
        if (position == 48000)
        {
            targets.modRate = 4.0;
            dsp.setTargets (targets);
        }

        float* channels[] { left.data() + position, right.data() + position };
        dsp.process (channels, 2, 256);
    }

    for (size_t i = 24001; i < 48000; ++i)
        steadiest = std::max (steadiest, (double) std::abs (left[i] - left[i - 1]));

    for (size_t i = 48001; i < 50400; ++i)
        moving = std::max (moving, (double) std::abs (left[i] - left[i - 1]));

    INFO ("largest step before " << steadiest << ", during the move " << moving);
    CHECK (moving < steadiest * 1.5);
}

// 3. Reverb EQ --------------------------------------------------------------------------------

TEST_CASE ("Room W1: the reverb EQ's bell raises the wet response by its gain", "[room][w1]")
{
    const auto input = noise (24000, 5);
    constexpr int total = 240000;

    auto targets = wetOnly();
    targets.mod = 0.0;
    const auto flat = renderWith<RoomDsp> (targets, input, total);

    targets.eqMidHz = 2000.0;
    targets.eqMidDb = 6.0;
    const auto boosted = renderWith<RoomDsp> (targets, input, total);

    const auto rise = magnitudeDbAt (boosted, 0, total, 2000.0) - magnitudeDbAt (flat, 0, total, 2000.0);
    CHECK (rise == Catch::Approx (6.0).margin (0.5));

    // A band back at 0 dB drops out, and the output is the flat one again.
    targets.eqMidDb = 0.0;
    CHECK (renderWith<RoomDsp> (targets, input, total) == flat);
}

// 4. Gate -------------------------------------------------------------------------------------

TEST_CASE ("Room W1: the gate stops the tail within hold + decay after the input does", "[room][w1]")
{
    const auto hit = noise (2400, 6, 0.8f); // 50 ms
    constexpr int total = 48000;

    auto targets = wetOnly();
    targets.decay = 3.0;
    targets.gate = true;
    targets.gateThresholdDb = -30.0;
    targets.gateHoldMs = 80.0;
    targets.gateDecayMs = 40.0;

    const auto gated = renderWith<RoomDsp> (targets, hit, total);

    targets.gate = false;
    const auto open = renderWith<RoomDsp> (targets, hit, total);

    const auto peak = [] (const std::vector<float>& samples, int from, int to)
    {
        auto largest = 0.0;

        for (int i = from; i < to; ++i)
            largest = std::max (largest, (double) std::abs (samples[(size_t) i]));

        return largest;
    };

    const auto shut = 2400 + (int) ((80.0 + 40.0 + 10.0) * 0.001 * rate);
    const auto whole = peak (gated, 0, 2400);

    INFO ("peak " << whole << ", after " << peak (gated, shut, total) << ", ungated after " << peak (open, shut, total));
    CHECK (peak (gated, shut, total) < whole * 0.001); // 60 dB down
    CHECK (peak (open, shut, total) > whole * 0.01);   // without it, the tail rings on
}

// 5. Auto gain --------------------------------------------------------------------------------

TEST_CASE ("Room W1: Auto gain holds the wet level as Decay goes from 1 to 6 s", "[room][w1]")
{
    const auto burst = noise (9600, 7); // 200 ms
    constexpr int total = 8 * 48000;

    const auto level = [&] (double decay, bool autoGain)
    {
        auto targets = wetOnly();
        targets.decay = decay;
        targets.autoGain = autoGain;
        targets.erGain = 0.0; // the tail alone, which is what Auto gain models
        const auto out = renderWith<RoomDsp> (targets, burst, total);
        return rmsDb (out, 0, (int) out.size());
    };

    const auto offApart = level (6.0, false) - level (1.0, false);
    const auto onApart = level (6.0, true) - level (1.0, true);

    INFO ("off " << offApart << " dB apart, on " << onApart << " dB apart");
    CHECK (offApart > 4.0);
    CHECK (std::abs (onApart) < 1.5);
    CHECK (RoomDsp::autoGainDb (0.55, 1.8) == Catch::Approx (0.0).margin (1.0e-12));
}
