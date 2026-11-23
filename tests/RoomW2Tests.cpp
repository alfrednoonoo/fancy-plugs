// Phase 4 Wave 2 for Fancy Room (docs/room.md, "Phase 4 Wave 2", section 5): Low decay, Attack,
// Duck from Sidechain and Listen to ducking, and the legacy check against v1.7.0's engine.
// Tests 2 and 3's figures come from docs/research/room-w2-prepush.py, which measures the same way.

#include "LegacyRoomW1Dsp.h"
#include "RoomDsp.h"
#include "RoomProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
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

// The input on the left, 0.7 x it on the right; returns left then right, one after the other.
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

// The late tail alone, from an impulse, as docs/research/room-w2-prepush.py renders it.
RoomDsp::Targets lateTail (double decay)
{
    auto targets = wetOnly();
    targets.decay = decay;
    targets.mod = 0.0;
    targets.erGain = 0.0;
    targets.predelayMs = 0.0;
    targets.lowCutHz = 20.0;
    targets.highCutHz = 20000.0;
    return targets;
}

// Room-model's biquads (RBJ), run in double.
struct Biquad
{
    double b0, b1, b2, a1, a2, s1 = 0.0, s2 = 0.0;

    static Biquad bandPass (double hz, double q)
    {
        const auto w0 = 2.0 * std::numbers::pi * hz / rate;
        const auto alpha = std::sin (w0) / (2.0 * q);
        const auto a0 = 1.0 + alpha;
        return { alpha / a0, 0.0, -alpha / a0, -2.0 * std::cos (w0) / a0, (1.0 - alpha) / a0 };
    }

    double process (double x)
    {
        const auto y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }
};

// T30: the Schroeder curve's least-squares slope from -5 to -35 dB.
double t30 (const std::vector<double>& energy)
{
    std::vector<double> curve (energy.size());
    auto sum = 0.0;

    for (size_t i = energy.size(); i-- > 0;)
    {
        sum += energy[i];
        curve[i] = sum;
    }

    auto n = 0.0, sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;

    for (size_t i = 0; i < curve.size(); ++i)
    {
        const auto db = 10.0 * std::log10 (std::max (curve[i] / curve[0], 1.0e-30));

        if (db <= -5.0 && db >= -35.0)
        {
            const auto x = (double) i / rate;
            n += 1.0;
            sx += x;
            sy += db;
            sxx += x * x;
            sxy += x * db;
        }
    }

    const auto slope = (n * sxy - sx * sy) / (n * sxx - sx * sx);
    return -60.0 / slope;
}

// The octave's T30: two cascaded band-passes (Q 1.4142) on each channel, then L^2 + R^2.
double bandT30 (const std::vector<float>& stereo, double hz)
{
    const auto total = stereo.size() / 2;
    std::vector<double> energy (total);
    Biquad filters[2][2] { { Biquad::bandPass (hz, 1.4142), Biquad::bandPass (hz, 1.4142) },
                           { Biquad::bandPass (hz, 1.4142), Biquad::bandPass (hz, 1.4142) } };

    for (size_t i = 0; i < total; ++i)
    {
        const auto l = filters[0][1].process (filters[0][0].process ((double) stereo[i]));
        const auto r = filters[1][1].process (filters[1][0].process ((double) stereo[total + i]));
        energy[i] = l * l + r * r;
    }

    return t30 (energy);
}

LegacyRoomW1Dsp::Targets legacyOf (const RoomDsp::Targets& t)
{
    LegacyRoomW1Dsp::Targets old;
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
    return old;
}

// Runs the engine with a main input and a sidechain, in blocks; returns the main output (left).
std::vector<float> renderKeyed (RoomDsp& dsp, const std::vector<float>& main, const std::vector<float>* key, int block = 256)
{
    const auto total = (int) main.size();
    std::vector<float> left (main), right (main), keyLeft, keyRight;

    if (key != nullptr)
        keyLeft = keyRight = *key;

    for (int position = 0; position < total; position += block)
    {
        const auto count = std::min (block, total - position);
        float* channels[] { left.data() + position, right.data() + position };

        if (key != nullptr)
        {
            const float* sidechain[] { keyLeft.data() + position, keyRight.data() + position };
            dsp.process (channels, 2, count, sidechain, 2);
        }
        else
        {
            dsp.process (channels, 2, count);
        }
    }

    return left;
}

double rms (const std::vector<float>& samples, int from, int to)
{
    auto sum = 0.0;

    for (int i = from; i < to; ++i)
        sum += (double) samples[(size_t) i] * samples[(size_t) i];

    return std::sqrt (sum / std::max (1, to - from));
}
} // namespace

// 1. Legacy -----------------------------------------------------------------------------------

TEST_CASE ("Room W2: at the new defaults the engine renders as v1.7.0's, bit for bit", "[room][w2]")
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
                targets.eqMidDb = 3.0;
                targets.gate = ducking > 0.0;
                targets.autoGain = true;

                const auto now = renderWith<RoomDsp> (targets, input, 60000, 333);
                const auto old = renderWith<LegacyRoomW1Dsp> (legacyOf (targets), input, 60000, 333);
                REQUIRE (now == old);
            }
        }
    }
}

TEST_CASE ("Room W2: the new parameters' defaults are the engine's", "[room][w2]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    RoomProcessor processor;
    auto& apvts = processor.getApvts();

    for (const auto* id : { "lowDecay", "attack", "duckFrom" })
        REQUIRE (apvts.getParameter (id) != nullptr);

    const RoomDsp::Targets defaults;
    CHECK ((double) apvts.getRawParameterValue ("lowDecay")->load() == defaults.lowDecay);
    CHECK ((double) apvts.getRawParameterValue ("attack")->load() == defaults.attackMs);
    CHECK (apvts.getRawParameterValue ("duckFrom")->load() == 0.0f);
    CHECK_FALSE (processor.readTargets().duckSidechain);

    // Appended after Wave 1's last, so every earlier index keeps its meaning.
    const auto& parameters = processor.getParameters();
    const auto indexOf = [&] (const char* id) { return apvts.getParameter (id)->getParameterIndex(); };
    CHECK (indexOf ("lowDecay") == indexOf ("autoGain") + 1);
    CHECK (indexOf ("attack") == indexOf ("autoGain") + 2);
    CHECK (indexOf ("duckFrom") == indexOf ("autoGain") + 3);
    CHECK (indexOf ("duckFrom") < parameters.size());
}

// 2. Low decay --------------------------------------------------------------------------------

TEST_CASE ("Room W2: Low decay sets how long the lows ring, and leaves 1 kHz at Decay", "[room][w2]")
{
    // The model gives 63 Hz 1.094 and 3.788 s, 1 kHz 1.921 and 2.072 s. The 125 Hz octave is only
    // part way into the 250 Hz shelf (1.13 and 3.36 s), so the lows are measured an octave down.
    const std::vector<float> impulse { 1.0f };
    constexpr int total = 7 * 48000;

    struct Case
    {
        double multiplier, lows, lowsMargin;
    };

    for (const auto& c : { Case { 0.5, 1.0, 0.12 }, Case { 2.0, 4.0, 0.4 } })
    {
        auto targets = lateTail (2.0);
        targets.lowDecay = c.multiplier;
        const auto out = renderWith<RoomDsp> (targets, impulse, total);

        const auto lows = bandT30 (out, 63.0), mids = bandT30 (out, 1000.0);
        INFO ("Low decay x" << c.multiplier << ": 63 Hz " << lows << " s, 1 kHz " << mids << " s");
        CHECK (lows == Catch::Approx (c.lows).margin (c.lowsMargin));
        CHECK (mids == Catch::Approx (2.0).margin (0.2));
    }

}

TEST_CASE ("Room W2: a frozen tail holds with Low decay away from x1", "[room][w2]")
{
    const auto input = noise (24000, 8, 0.3f);
    constexpr int total = 4 * 48000;

    for (const auto multiplier : { 0.25, 4.0 })
    {
        RoomDsp dsp;
        auto targets = wetOnly();
        targets.lowDecay = multiplier;
        dsp.setTargets (targets);
        dsp.prepare (rate);

        std::vector<float> left ((size_t) total, 0.0f), right ((size_t) total, 0.0f);
        std::copy (input.begin(), input.end(), left.begin());
        std::copy (input.begin(), input.end(), right.begin());

        for (int position = 0; position < total; position += 480)
        {
            if (position == 24000)
            {
                targets.freeze = true;
                dsp.setTargets (targets);
            }

            float* channels[] { left.data() + position, right.data() + position };
            dsp.process (channels, 2, 480);
        }

        const auto early = rms (left, 48000, 72000), late = rms (left, 168000, 192000);
        INFO ("Low decay x" << multiplier << ": " << 20.0 * std::log10 (late / early) << " dB over 2.5 s frozen");
        CHECK (std::isfinite (late));
        CHECK (std::abs (20.0 * std::log10 (late / early)) < 1.0);
    }
}

// 3. Attack -----------------------------------------------------------------------------------

TEST_CASE ("Room W2: Attack 150 ms swells the tail in and keeps its level", "[room][w2]")
{
    // The model: Attack 0 peaks at 70 ms (the network building up), 150 ms at 200 ms, and the
    // energy is 0.16 dB lower.
    const std::vector<float> impulse { 1.0f };
    constexpr int total = 3 * 48000, frame = 480;

    const auto measure = [&] (double attackMs, int& peakMs)
    {
        auto targets = lateTail (1.8);
        targets.attackMs = attackMs;
        const auto out = renderWith<RoomDsp> (targets, impulse, total);

        auto best = -1.0, sum = 0.0;

        for (int start = 0; start + frame <= total; start += frame)
        {
            auto e = 0.0;

            for (int i = start; i < start + frame; ++i)
                e += (double) out[(size_t) i] * out[(size_t) i] + (double) out[(size_t) (total + i)] * out[(size_t) (total + i)];

            if (e > best)
            {
                best = e;
                peakMs = start / 48;
            }

            sum += e;
        }

        return 10.0 * std::log10 (sum);
    };

    int peakAt0 = 0, peakAt150 = 0;
    const auto energy0 = measure (0.0, peakAt0);
    const auto energy150 = measure (150.0, peakAt150);

    INFO ("peaks at " << peakAt0 << " and " << peakAt150 << " ms, energy " << energy150 - energy0 << " dB");
    CHECK (peakAt150 - peakAt0 >= 120);
    CHECK (peakAt150 - peakAt0 <= 180);
    CHECK (std::abs (energy150 - energy0) < 1.0);
}

TEST_CASE ("Room W2: Attack leaves the early reflections alone, and moving it doesn't click", "[room][w2]")
{
    const std::vector<float> impulse { 1.0f };

    // Until the network's shortest line first comes round (Pre-delay 12 ms + 26.5 ms at Size
    // 55 %), the output is the early reflections alone, and Attack doesn't touch them.
    constexpr int total = 2400;
    auto targets = wetOnly();
    targets.mod = 0.0;
    const auto plain = renderWith<RoomDsp> (targets, impulse, total);
    targets.attackMs = 300.0;
    const auto swelled = renderWith<RoomDsp> (targets, impulse, total);
    const auto erOnlyEnd = (int) ((12.0 + 20.0) * 0.001 * rate);
    auto reflected = 0.0;

    for (int i = 0; i < erOnlyEnd; ++i)
    {
        REQUIRE (plain[(size_t) i] == swelled[(size_t) i]);
        REQUIRE (plain[(size_t) (total + i)] == swelled[(size_t) (total + i)]);
        reflected = std::max (reflected, (double) std::abs (plain[(size_t) i]));
    }

    CHECK (reflected > 0.01);
    CHECK (plain != swelled);

    // Sweeping Attack from 0 to 500 ms on held noise: no step bigger than before the move. (A
    // pure tone isn't used: 16 evenly spaced taps are a comb, which lifts a tone whose period
    // matches their spacing.)
    const auto tone = noise (96000, 11, 0.3f);

    const auto sweep = [&] (bool move)
    {
        RoomDsp dsp;
        auto moving = wetOnly();
        dsp.setTargets (moving);
        dsp.prepare (rate);

        std::vector<float> left (tone), right (tone);

        for (int position = 0; position < (int) tone.size(); position += 256)
        {
            if (move && position >= 48000 && position < 72000)
            {
                moving.attackMs = 500.0 * (position - 48000) / 24000.0;
                dsp.setTargets (moving);
            }

            float* channels[] { left.data() + position, right.data() + position };
            dsp.process (channels, 2, 256);
        }

        auto largest = 0.0;

        for (size_t i = 48001; i < 96000; ++i)
            largest = std::max (largest, (double) std::abs (left[i] - left[i - 1]));

        return largest;
    };

    const auto still = sweep (false), moved = sweep (true);
    INFO ("largest step with Attack at 0 " << still << ", while it moves " << moved);
    CHECK (moved < still * 1.5);
}

// 4. Duck from Sidechain ----------------------------------------------------------------------

TEST_CASE ("Room W2: Duck from Sidechain follows the sidechain, not the main input", "[room][w2]")
{
    constexpr int total = 48000;
    const auto main = noise (total, 9, 0.5f);
    std::vector<float> key ((size_t) total), silent ((size_t) total, 0.0f);

    for (int i = 0; i < total; ++i)
        key[(size_t) i] = (float) (std::pow (10.0, -10.0 / 20.0) * std::sin (2.0 * std::numbers::pi * 300.0 * i / rate));

    auto targets = wetOnly();
    targets.ducking = 1.0;
    targets.duckSidechain = true;

    // A sidechain at -10 dBFS: the full 24 dB, whatever the main input does.
    {
        RoomDsp dsp;
        dsp.setTargets (targets);
        dsp.prepare (rate);
        renderKeyed (dsp, main, &key);
        CHECK (dsp.getDuckingDb() == Catch::Approx (24.0).margin (1.0));
    }

    // A silent sidechain leaves the reverb alone, though the main input is loud.
    {
        RoomDsp dsp;
        dsp.setTargets (targets);
        dsp.prepare (rate);
        const auto ducked = renderKeyed (dsp, main, &silent);
        CHECK (dsp.getDuckingDb() == 0.0);

        auto calm = targets;
        calm.ducking = 0.0;
        RoomDsp undisturbed;
        undisturbed.setTargets (calm);
        undisturbed.prepare (rate);
        CHECK (renderKeyed (undisturbed, main, &silent) == ducked);
    }

    // The wet output is 24 dB down while keyed, against the same with a silent key.
    {
        RoomDsp keyed, unkeyed;
        keyed.setTargets (targets);
        keyed.prepare (rate);
        unkeyed.setTargets (targets);
        unkeyed.prepare (rate);

        const auto down = renderKeyed (keyed, main, &key);
        const auto up = renderKeyed (unkeyed, main, &silent);
        const auto reduction = 20.0 * std::log10 (rms (up, 24000, total) / rms (down, 24000, total));
        CHECK (reduction == Catch::Approx (24.0).margin (1.0));
    }

    // Duck from Input, or Sidechain with no sidechain channels: Ducking reads the main input.
    {
        auto input = targets;
        input.duckSidechain = false;
        RoomDsp fromInput, noBus;
        fromInput.setTargets (input);
        fromInput.prepare (rate);
        noBus.setTargets (targets);
        noBus.prepare (rate);

        const auto a = renderKeyed (fromInput, main, &key);
        const auto b = renderKeyed (noBus, main, nullptr);
        CHECK (a == b);
        CHECK (fromInput.getDuckingDb() > 20.0);
    }
}

// 5. Listen to ducking ------------------------------------------------------------------------

TEST_CASE ("Room W2: Listen to ducking plays what Ducking takes from the wet signal", "[room][w2]")
{
    constexpr int total = 48000;
    const auto main = noise (total, 10, 0.5f);

    auto targets = wetOnly();
    targets.ducking = 0.5;
    targets.dryGain = 1.0;

    const auto run = [&] (double ducking, bool listen)
    {
        auto t = targets;
        t.ducking = ducking;
        t.listenDucking = listen;
        RoomDsp dsp;
        dsp.setTargets (t);
        dsp.prepare (rate);
        return renderKeyed (dsp, main, nullptr);
    };

    // What was taken (with the dry signal out) + the ducked output = the undisturbed output.
    const auto undisturbed = run (0.0, false);
    const auto ducked = run (0.5, false);
    const auto removed = run (0.5, true);

    auto worst = 0.0;

    for (int i = 0; i < total; ++i)
        worst = std::max (worst, std::abs ((double) ducked[(size_t) i] + removed[(size_t) i] - undisturbed[(size_t) i]));

    CHECK (worst < 1.0e-4);
    CHECK (rms (removed, 24000, total) > 0.0);

    // At Ducking 0 there's nothing taken away.
    const auto nothing = run (0.0, true);
    CHECK (rms (nothing, 0, total) == 0.0);
}
