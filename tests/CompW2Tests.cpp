// Phase 4 Wave 2 for Fancy Comp: the tempo-synced trigger (docs/comp.md, "Phase 4 Wave 2",
// section 5). The other Detector inputs are held to v1.6.0 by CompW1Tests' legacy test.

#include "CompDsp.h"
#include "CompProcessor.h"

#include <fancy/dsp/TempoTrigger.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <random>
#include <vector>

namespace
{
using fancy::dsp::TempoTrigger;

// The samples where hits land, from a host at bpm, with each block's start position given.
std::vector<long long> hitsOf (double rate, double bpm, double stepBeats, double startPpq, long long total,
                               const std::vector<int>& blocks, bool playing = true)
{
    TempoTrigger trigger;
    trigger.prepare (rate);
    trigger.setStep (stepBeats);
    trigger.setShape (0.25);

    std::vector<long long> hits;
    std::vector<float> out (8192);
    long long position = 0;
    size_t next = 0;

    while (position < total)
    {
        const auto block = (int) std::min<long long> (blocks[next++ % blocks.size()], total - position);
        trigger.setPosition ({ true, playing, startPpq + (double) position * bpm / (60.0 * rate), bpm });
        trigger.process (out.data(), block);

        for (int i = 0; i < block; ++i)
            if (out[(size_t) i] == 1.0f)
                hits.push_back (position + i);

        position += block;
    }

    return hits;
}

std::vector<long long> expectedHits (double rate, double bpm, double stepBeats, double startPpq, long long total)
{
    std::vector<long long> hits;
    const auto samplesPerStep = stepBeats * 60.0 * rate / bpm;

    for (auto k = std::ceil (startPpq / stepBeats); ; k += 1.0)
    {
        const auto at = (long long) std::ceil ((k * stepBeats - startPpq) / stepBeats * samplesPerStep - 1.0e-6);

        if (at >= total)
            break;

        hits.push_back (at);
    }

    return hits;
}
} // namespace

TEST_CASE ("Comp W2: Host sync hits land on the grid at every block size, with no double hits", "[comp][w2]")
{
    std::mt19937 random (5);
    std::vector<int> randomBlocks;

    for (int i = 0; i < 400; ++i)
        randomBlocks.push_back (1 + (int) (random() % 2048));

    for (const auto rate : { 44100.0, 48000.0, 96000.0 })
    {
        const auto total = (long long) (rate * 4.0);
        const auto expected = expectedHits (rate, 120.0, 1.0, 0.37, total);
        REQUIRE (expected.size() == 8);

        for (const auto& blocks : { std::vector<int> { 1 }, std::vector<int> { 64 }, std::vector<int> { 441 }, std::vector<int> { 4096 }, randomBlocks })
        {
            const auto hits = hitsOf (rate, 120.0, 1.0, 0.37, total, blocks);
            INFO ("rate " << rate << ", first block " << blocks.front());
            REQUIRE (hits.size() == expected.size());

            for (size_t h = 0; h < hits.size(); ++h)
                CHECK (std::abs (hits[h] - expected[h]) <= 1);
        }
    }
}

TEST_CASE ("Comp W2: Rate 1/8 dotted at 120 BPM hits every 375 ms", "[comp][w2]")
{
    const auto index = CompProcessor::syncRateNames().indexOf ("1/8 dotted");
    REQUIRE (index >= 0);
    CHECK (CompProcessor::syncStepBeats (index) == Catch::Approx (0.75));
    CHECK (CompProcessor::syncRateNames()[CompProcessor::defaultSyncRate] == "1/4");
    CHECK (CompProcessor::syncStepBeats (CompProcessor::defaultSyncRate) == 1.0);
    CHECK (CompProcessor::syncRateNames().size() == 15);

    const auto hits = hitsOf (48000.0, 120.0, 0.75, 0.0, 96000, { 512 });
    REQUIRE (hits.size() >= 3);

    for (size_t h = 1; h < hits.size(); ++h)
        CHECK (hits[h] - hits[h - 1] == 18000);
}

TEST_CASE ("Comp W2: stopped, the trigger runs on at the last tempo, and play re-aligns it", "[comp][w2]")
{
    TempoTrigger trigger;
    trigger.prepare (48000.0);
    trigger.setStep (1.0);

    std::vector<float> out (24000 * 3);

    // Stopped from the start: it runs on its own from where it was (0) at the host's tempo,
    // whatever position the stopped host reports.
    trigger.setPosition ({ true, false, 2.5, 120.0 });
    trigger.process (out.data(), (int) out.size());

    std::vector<int> hits;

    for (int i = 0; i < (int) out.size(); ++i)
        if (out[(size_t) i] == 1.0f)
            hits.push_back (i);

    REQUIRE (hits.size() == 3);
    CHECK (hits[0] == 0);
    CHECK (hits[1] - hits[0] == 24000);

    // Play from beat 10: the first sample is on the grid and hits at once.
    trigger.setPosition ({ true, true, 10.0, 120.0 });
    trigger.process (out.data(), 100);
    CHECK (out[0] == 1.0f);
}

TEST_CASE ("Comp W2: Host sync ducks as a real sidechain kick of the same shape does", "[comp][w2]")
{
    constexpr double rate = 48000.0;
    constexpr int total = 96000;

    CompDsp::Targets targets;
    targets.thresholdDb = -20.0;
    targets.ratio = 8.0;
    targets.attackMs = 1.0;
    targets.releaseMs = 120.0;
    targets.syncStepBeats = 1.0;
    targets.syncShape = 0.25;

    std::vector<float> tone (total);

    for (int i = 0; i < total; ++i)
        tone[(size_t) i] = (float) (0.25 * std::sin (2.0 * std::numbers::pi * 1000.0 * i / rate));

    // The kick the trigger makes, as a sidechain signal.
    std::vector<float> kick (total);
    {
        TempoTrigger trigger;
        trigger.prepare (rate);
        trigger.setStep (1.0);
        trigger.setShape (0.25);
        trigger.setPosition ({ true, true, 0.0, 120.0 });
        trigger.process (kick.data(), total);
    }

    const auto render = [&] (int detector)
    {
        auto settings = targets;
        settings.detector = detector;

        CompDsp dsp;
        dsp.prepare (rate);
        dsp.snapToTargets (settings);

        std::vector<float> left (tone), right (tone), keyLeft (kick), keyRight (kick);

        for (int start = 0; start < total; start += 256)
        {
            CompDsp::Transport transport;
            transport.playing = true;
            transport.bpm = 120.0;
            transport.ppq = (double) start * 120.0 / (60.0 * rate);
            dsp.setTransport (transport);

            float* main[] { left.data() + start, right.data() + start };
            const float* key[] { keyLeft.data() + start, keyRight.data() + start };
            dsp.process (main, 2, key, 2, std::min (256, total - start));
        }

        return left;
    };

    const auto synced = render (CompDsp::hostSync);
    const auto sidechained = render (CompDsp::sideLR);

    // Gain per 5 ms window, against the tone.
    auto worst = 0.0;
    auto deepest = 0.0;

    for (int start = 4800; start + 240 <= total; start += 240)
    {
        auto a = 0.0, b = 0.0, x = 0.0;

        for (int i = start; i < start + 240; ++i)
        {
            a += (double) synced[(size_t) i] * synced[(size_t) i];
            b += (double) sidechained[(size_t) i] * sidechained[(size_t) i];
            x += (double) tone[(size_t) i] * tone[(size_t) i];
        }

        const auto gainA = 10.0 * std::log10 (a / x), gainB = 10.0 * std::log10 (b / x);
        worst = std::max (worst, std::abs (gainA - gainB));
        deepest = std::min (deepest, gainA);
    }

    INFO ("deepest duck " << deepest << " dB, worst difference from the sidechain kick " << worst << " dB");
    CHECK (deepest < -10.0);
    CHECK (worst < 0.5);
}
