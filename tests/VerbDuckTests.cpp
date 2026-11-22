// Phase 4 Wave 2 for Fancy Verb: Ducking and the level gate (docs/verb.md, "Phase 4 Wave 2:
// ducking and level gate", section 4). Both are skipped at their defaults, so the other Verb
// tests hold the dry and wet paths as they were.

#include "VerbDsp.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>
#include <numbers>
#include <random>
#include <vector>

namespace
{
using Mode = verb::Convolver::Mode;

struct Render
{
    std::vector<float> left, right;
};

std::vector<float> decayingNoise (double rate, double seconds, double rtSeconds, unsigned seed)
{
    std::mt19937 random (seed);
    std::normal_distribution<float> noise;
    std::vector<float> ir ((size_t) (seconds * rate));

    for (size_t i = 0; i < ir.size(); ++i)
        ir[i] = 0.05f * noise (random) * (float) std::exp (-6.907755 * (double) i / rate / rtSeconds);

    return ir;
}

// Wet only, offline, with the main input and an optional sidechain.
Render render (const VerbDsp::Targets& settings, double rate, Mode mode, const std::vector<float>& main,
               const std::vector<float>* key, int blockSize = 512)
{
    const auto irLeft = decayingNoise (rate, 3.0, 3.0, 1), irRight = decayingNoise (rate, 3.0, 3.0, 2);

    VerbDsp dsp;
    dsp.prepare (rate, verb::Convolver::latencyOf (mode));
    dsp.offer (std::make_unique<verb::Convolver> (irLeft.data(), irRight.data(), (int) irLeft.size(), mode, 8192));

    auto targets = settings;
    targets.wet = 1.0;
    targets.dry = 0.0;
    dsp.snapToTargets (targets);

    Render out { main, main };
    std::vector<float> keyLeft, keyRight;

    if (key != nullptr)
        keyLeft = keyRight = *key;

    for (int start = 0; start < (int) main.size(); start += blockSize)
    {
        const auto count = std::min (blockSize, (int) main.size() - start);
        float* channels[] { out.left.data() + start, out.right.data() + start };
        const float* sidechain[] { key != nullptr ? keyLeft.data() + start : nullptr, key != nullptr ? keyRight.data() + start : nullptr };
        dsp.process (channels, 2, key != nullptr ? sidechain : nullptr, key != nullptr ? 2 : 0, count, true);
    }

    return out;
}

double rmsDb (const std::vector<float>& x, double rate, double fromSeconds, double toSeconds)
{
    const auto from = (size_t) (fromSeconds * rate), to = std::min (x.size(), (size_t) (toSeconds * rate));
    auto sum = 0.0;

    for (auto i = from; i < to; ++i)
        sum += (double) x[i] * (double) x[i];

    return 10.0 * std::log10 (std::max (sum / (double) (to - from), 1.0e-30));
}

std::vector<float> tone (double rate, double seconds, double fromSeconds, double toSeconds, double dbfs)
{
    std::vector<float> x ((size_t) (seconds * rate), 0.0f);
    const auto amplitude = std::pow (10.0, dbfs / 20.0);

    for (auto i = (size_t) (fromSeconds * rate); i < (size_t) (toSeconds * rate) && i < x.size(); ++i)
        x[i] = (float) (amplitude * std::sin (2.0 * std::numbers::pi * 1000.0 * (double) i / rate));

    return x;
}
} // namespace

TEST_CASE ("Verb's Gate lines up with the wet signal in Low CPU mode", "[verb][w2][duck]")
{
    const auto rate = GENERATE (44100.0, 48000.0, 96000.0);

    VerbDsp::Targets targets;
    targets.gate = true;
    targets.gateThresholdDb = -30.0;
    targets.gateHoldMs = 80.0;
    targets.gateDecayMs = 40.0;

    // A 50 ms noise hit at -6 dBFS, 0.2 s in.
    std::mt19937 random (9);
    std::uniform_real_distribution<float> noise (-0.5f, 0.5f);
    std::vector<float> input ((size_t) (1.5 * rate), 0.0f);

    for (auto i = (size_t) (0.2 * rate); i < (size_t) (0.25 * rate); ++i)
        input[i] = noise (random);

    const auto out = render (targets, rate, Mode::lowCpu, input, nullptr);
    const auto arrival = 0.2 + (double) verb::Convolver::lowCpuLatency / rate;
    const auto level = rmsDb (out.left, rate, arrival + 0.01, arrival + 0.04);

    INFO ("rate " << rate);
    CHECK (rmsDb (out.left, rate, arrival + 0.125, arrival + 0.13) > level - 60.0);   // still open in Hold
    CHECK (rmsDb (out.left, rate, arrival + 0.18, arrival + 0.185) < level - 60.0);   // shut 10 ms after 170 ms
}

TEST_CASE ("Verb's Ducking at 100 % turns the wet down 24 dB and lets it come back", "[verb][w2][duck]")
{
    constexpr double rate = 48000.0;
    const auto input = tone (rate, 3.0, 0.2, 1.2, -10.0);

    VerbDsp::Targets plain, ducked;
    ducked.ducking = 1.0;

    const auto a = render (plain, rate, Mode::zeroLatency, input, nullptr);
    const auto b = render (ducked, rate, Mode::zeroLatency, input, nullptr);

    CHECK (rmsDb (b.left, rate, 0.9, 1.1) - rmsDb (a.left, rate, 0.9, 1.1) == Catch::Approx (-24.0).margin (0.5));
    CHECK (std::abs (rmsDb (b.left, rate, 2.7, 2.75) - rmsDb (a.left, rate, 2.7, 2.75)) < 1.0);
}

TEST_CASE ("Verb's Ducking listens to the sidechain when chosen, and to the main input without one", "[verb][w2][duck]")
{
    constexpr double rate = 48000.0;
    const auto input = tone (rate, 2.0, 0.1, 1.9, -20.0);
    const auto loudKey = tone (rate, 2.0, 0.5, 1.5, -6.0);
    const std::vector<float> silentKey (input.size(), 0.0f);

    VerbDsp::Targets fromKey;
    fromKey.ducking = 1.0;
    fromKey.duckSidechain = true;

    const auto ducked = render (fromKey, rate, Mode::zeroLatency, input, &loudKey);
    const auto quiet = render (fromKey, rate, Mode::zeroLatency, input, &silentKey);
    const auto none = render (VerbDsp::Targets {}, rate, Mode::zeroLatency, input, nullptr);

    // A loud sidechain ducks the wet; a silent one leaves it exactly as Ducking 0 does.
    CHECK (rmsDb (ducked.left, rate, 1.0, 1.4) - rmsDb (none.left, rate, 1.0, 1.4) == Catch::Approx (-24.0).margin (0.5));
    REQUIRE (quiet.left == none.left);

    // With no sidechain bus, Sidechain acts as Main.
    VerbDsp::Targets fromMain = fromKey;
    fromMain.duckSidechain = false;
    const auto noBus = render (fromKey, rate, Mode::zeroLatency, input, nullptr);
    const auto main = render (fromMain, rate, Mode::zeroLatency, input, nullptr);
    REQUIRE (noBus.left == main.left);
}

TEST_CASE ("Verb at Ducking 0 and Gate off ignores the sidechain", "[verb][w2][duck]")
{
    constexpr double rate = 48000.0;
    const auto input = tone (rate, 1.0, 0.1, 0.6, -12.0);
    const auto key = tone (rate, 1.0, 0.0, 1.0, -3.0);

    const auto withKey = render (VerbDsp::Targets {}, rate, Mode::lowCpu, input, &key);
    const auto without = render (VerbDsp::Targets {}, rate, Mode::lowCpu, input, nullptr);

    REQUIRE (withKey.left == without.left);
    REQUIRE (withKey.right == without.right);
}
