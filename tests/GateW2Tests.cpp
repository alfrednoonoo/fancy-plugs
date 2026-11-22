// Phase 4 Wave 2 for Fancy Gate: Host sync (docs/gate.md, "Phase 4 Wave 2: tempo-synced trigger",
// section 5). Test 1 (every other Detector input renders as v1.6.0) is GateW1Tests' first case,
// which runs the current GateDsp against LegacyGateDsp.

#include "GateDsp.h"
#include "GateProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>
#include <vector>

namespace
{
GateDsp::Targets synced (double thresholdDb)
{
    GateDsp::Targets targets;
    targets.detector = GateDsp::hostSync;
    targets.thresholdDb = thresholdDb;
    targets.attackMs = 0.1;
    targets.holdMs = 0.0;
    targets.releaseMs = 0.1;
    targets.rmsMs = 0.0;
    targets.rangeDb = -200.0;   // -inf: shut is silence
    targets.syncStepBeats = 0.5;   // 1/8
    targets.syncShape = 0.5;
    return targets;
}

// A steady 0.5 through the gate, with the host playing from 0 at 120 BPM.
std::vector<float> render (const GateDsp::Targets& targets, double rate, int numSamples, int blockSize)
{
    GateDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    std::vector<float> left ((size_t) numSamples, 0.5f), right ((size_t) numSamples, 0.5f);

    for (int start = 0; start < numSamples; start += blockSize)
    {
        const auto count = std::min (blockSize, numSamples - start);
        dsp.setTransport ({ true, true, (double) start * 120.0 / (60.0 * rate), 120.0 });

        float* channels[] { left.data() + start, right.data() + start };
        dsp.process (channels, 2, nullptr, 0, count, nullptr);
    }

    return left;
}

// The samples where the output goes from silence to sound.
std::vector<int> openings (const std::vector<float>& output)
{
    std::vector<int> result;
    auto wasOpen = false;

    for (int i = 0; i < (int) output.size(); ++i)
    {
        const auto isOpen = output[(size_t) i] != 0.0f;

        if (isOpen && ! wasOpen)
            result.push_back (i);

        wasOpen = isOpen;
    }

    return result;
}
} // namespace

TEST_CASE ("Gate Host sync opens on the host's grid to the sample at any block size", "[gate][w2]")
{
    const auto rate = GENERATE (44100.0, 48000.0, 96000.0);
    const auto blockSize = GENERATE (1, 64, 333, 4096);

    const auto output = render (synced (-30.0), rate, (int) (rate * 1.2), blockSize);
    const auto starts = openings (output);
    const auto step = 0.25 * rate;   // an eighth at 120 BPM

    INFO ("rate " << rate << ", block " << blockSize);
    REQUIRE (starts.size() == 5);

    for (size_t k = 0; k < starts.size(); ++k)
        CHECK (std::abs ((double) starts[k] - (double) k * step) <= 1.0);
}

TEST_CASE ("Gate Host sync's pulse holds the gate open for Shape of the step at any Threshold", "[gate][w2]")
{
    constexpr double rate = 48000.0;
    const auto threshold = GENERATE (-50.0, -30.0, -12.0);

    const auto output = render (synced (threshold), rate, (int) rate, 480);
    const auto starts = openings (output);
    REQUIRE (starts.size() >= 3);

    // The second opening, counted until the output is silent again.
    auto end = starts[1];

    while (end < (int) output.size() && output[(size_t) end] != 0.0f)
        ++end;

    const auto openMs = (double) (end - starts[1]) * 1000.0 / rate;

    INFO ("threshold " << threshold);
    CHECK (openMs == Catch::Approx (125.0).margin (1.0));
}

TEST_CASE ("Gate Host sync in Duck mode ducks by Range during each pulse", "[gate][w2]")
{
    constexpr double rate = 48000.0;
    auto targets = synced (-30.0);
    targets.duck = true;
    targets.rangeDb = -12.0;

    const auto output = render (targets, rate, (int) rate, 512);

    // Mid-pulse (60 ms into a step) and mid-gap (190 ms into it), for the second and third steps.
    for (const auto stepStart : { 12000, 24000 })
    {
        CHECK (20.0 * std::log10 (output[(size_t) (stepStart + 2880)] / 0.5f) == Catch::Approx (-12.0).margin (0.1));
        CHECK (20.0 * std::log10 (output[(size_t) (stepStart + 9120)] / 0.5f) == Catch::Approx (0.0).margin (0.05));
    }
}

TEST_CASE ("Gate's MIDI trigger wins over Host sync", "[gate][w2]")
{
    auto targets = synced (-30.0);
    targets.midiIn = true;   // no notes arrive

    const auto output = render (targets, 48000.0, 48000, 480);

    for (const auto sample : output)
        REQUIRE (sample == 0.0f);
}

TEST_CASE ("Gate's sync rates match Comp's list, 1/8 by default", "[gate][w2]")
{
    CHECK (GateProcessor::syncRateNames().size() == 15);
    CHECK (GateProcessor::syncRateNames()[GateProcessor::defaultSyncRate] == "1/8");
    CHECK (GateProcessor::syncStepBeats (GateProcessor::defaultSyncRate) == Catch::Approx (0.5));
    CHECK (GateProcessor::syncStepBeats (10) == Catch::Approx (0.75));        // 1/8 dotted
    CHECK (GateProcessor::syncStepBeats (11) == Catch::Approx (1.0 / 3.0));   // 1/8 triplet
}
