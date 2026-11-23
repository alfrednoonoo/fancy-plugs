// Phase 4 Wave 2 for Fancy Delay: LFO shape and stereo phase (docs/delay.md, "Phase 4 Wave 2",
// section 4). The numbers were run first through docs/research/delay-lfo-prepush.py.

#include "DelayDsp.h"
#include "DelayProcessor.h"
#include "LegacyDelayW1Dsp.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cmath>
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

    template <typename Dsp>
    void process (Dsp& dsp, int blockSize = 256)
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

// Each cycle's pitch in cents against hz, from interpolated upward zero crossings after `from`.
std::vector<double> cycleCents (const std::vector<float>& x, int from, double hz)
{
    std::vector<double> crossings, cents;

    for (int i = std::max (1, from); i < (int) x.size(); ++i)
        if (x[(size_t) i - 1] < 0.0f && x[(size_t) i] >= 0.0f)
            crossings.push_back ((double) (i - 1) + (double) x[(size_t) i - 1] / ((double) x[(size_t) i - 1] - x[(size_t) i]));

    for (size_t c = 1; c < crossings.size(); ++c)
        cents.push_back (1200.0 * std::log2 (rate / (crossings[c] - crossings[c - 1]) / hz));

    return cents;
}
} // namespace

// 1. Legacy -------------------------------------------------------------------------------------

TEST_CASE ("Sine at 90 degrees renders exactly as Delay v1.7.0", "[delay][w2]")
{
    const auto variant = GENERATE (0, 1, 2, 3, 4);

    LegacyDelayW1Dsp::Targets old;
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
        case 1: old.taps[0].modRandom = 0.0; old.taps[1].modDepthCents = 30.0; old.taps[1].modRateHz = 4.0; break;
        case 2: old.taps[0].pingPong = true; old.taps[0].character = LegacyDelayW1Dsp::tape; old.taps[1].modDepthCents = 5.0; break;
        case 3: old.smoothTime = false; old.taps[1].diffusion = 0.5; old.taps[1].modDepthCents = 20.0; old.taps[1].modRandom = 1.0; break;
        case 4: old.taps[0].modDepthCents = 0.0; old.taps[1].character = LegacyDelayW1Dsp::bbd; old.duckAmountDb = 12.0; break;
        default: break;
    }

    DelayDsp::Targets now;

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
    }

    now.bpm = old.bpm; now.wetGain = old.wetGain; now.dryGain = old.dryGain; now.outputGain = old.outputGain;
    now.smoothTime = old.smoothTime; now.bypass = old.bypass;
    now.duckAmountDb = old.duckAmountDb; now.duckThresholdDb = old.duckThresholdDb; now.duckReleaseMs = old.duckReleaseMs;
    now.swing = old.swing; now.fastTanh = old.fastTanh; now.freeze = old.freeze;

    LegacyDelayW1Dsp legacy;
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
        // A depth change part way, through both paths.
        if (start == 48000)
        {
            old.taps[0].modDepthCents = now.taps[0].modDepthCents = 25.0;
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

TEST_CASE ("Delay's Mod shape and Mod stereo default to Sine and 90 degrees", "[delay][w2]")
{
    DelayProcessor processor;

    for (int tap = 0; tap < delay::maxTaps; ++tap)
    {
        const auto spec = processor.readTap (tap);
        CHECK (spec.modShape == DelayDsp::sine);
        CHECK (spec.modStereoDegrees == 90.0);
    }

    auto& stereo = *processor.getApvts().getParameter ("t3modStereo");
    stereo.setValueNotifyingHost (stereo.getValueForText ("45"));
    CHECK (processor.readTap (2).modStereoDegrees == 45.0);
}

// 2. Triangle -----------------------------------------------------------------------------------

TEST_CASE ("Triangle holds the detune at +-Depth and flips without a jump", "[delay][w2]")
{
    auto targets = oneTap (150.0);
    targets.taps[0].modDepthCents = 12.0;
    targets.taps[0].modRateHz = 0.8;
    targets.taps[0].modRandom = 0.0;
    targets.taps[0].modShape = DelayDsp::triangle;

    DelayDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    auto signal = tone (1000.0, 0.5, (int) (4.0 * rate));
    signal.process (dsp);

    const auto cents = cycleCents (signal.left, (int) (0.5 * rate), 1000.0);
    REQUIRE (cents.size() > 3000);

    auto held = 0;
    auto largestStep = 0.0;

    for (size_t c = 0; c < cents.size(); ++c)
    {
        if (std::abs (cents[c]) >= 0.9 * 12.0)
            ++held;

        if (c > 0)
            largestStep = std::max (largestStep, std::abs (cents[c] - cents[c - 1]));
    }

    const auto share = (double) held / (double) cents.size();
    const auto [lowest, highest] = std::minmax_element (cents.begin(), cents.end());

    // The model: 87 % held, 0.30 cents largest change, -12.1 to +12.0 cents.
    INFO ("held " << 100.0 * share << " %, largest change " << largestStep << " cents, range " << *lowest << " to " << *highest);
    CHECK (share >= 0.8);
    CHECK (largestStep < 1.0);
    CHECK (*highest < 12.0 * 1.1);
    CHECK (*lowest > -12.0 * 1.1);
}

// 3. Stereo -------------------------------------------------------------------------------------

TEST_CASE ("Stereo 0 moves both sides together; 180 moves them in opposition", "[delay][w2]")
{
    SECTION ("0 degrees: identical left and right for a mono input")
    {
        const auto shape = GENERATE (DelayDsp::sine, DelayDsp::triangle);

        auto targets = oneTap (150.0);
        targets.taps[0].modDepthCents = 12.0;
        targets.taps[0].modRateHz = 0.8;
        targets.taps[0].modShape = shape;
        targets.taps[0].modStereoDegrees = 0.0;

        DelayDsp dsp;
        dsp.prepare (rate);
        dsp.snapToTargets (targets);

        auto signal = tone (1000.0, 0.5, (int) (2.0 * rate));
        signal.process (dsp);

        INFO ("shape " << shape);
        CHECK (signal.left == signal.right);
    }

    SECTION ("180 degrees: the pitches move in opposition")
    {
        auto targets = oneTap (150.0);
        targets.taps[0].modDepthCents = 12.0;
        targets.taps[0].modRateHz = 0.8;
        targets.taps[0].modRandom = 0.0;
        targets.taps[0].modStereoDegrees = 180.0;

        DelayDsp dsp;
        dsp.prepare (rate);
        dsp.snapToTargets (targets);

        auto signal = tone (1000.0, 0.5, (int) (4.0 * rate));
        signal.process (dsp);

        const auto left = cycleCents (signal.left, (int) (0.5 * rate), 1000.0);
        const auto right = cycleCents (signal.right, (int) (0.5 * rate), 1000.0);
        const auto count = std::min (left.size(), right.size());
        REQUIRE (count > 3000);

        auto meanLeft = 0.0, meanRight = 0.0;

        for (size_t c = 0; c < count; ++c)
            meanLeft += left[c], meanRight += right[c];

        meanLeft /= (double) count;
        meanRight /= (double) count;

        auto both = 0.0, leftSquares = 0.0, rightSquares = 0.0;

        for (size_t c = 0; c < count; ++c)
        {
            both += (left[c] - meanLeft) * (right[c] - meanRight);
            leftSquares += (left[c] - meanLeft) * (left[c] - meanLeft);
            rightSquares += (right[c] - meanRight) * (right[c] - meanRight);
        }

        const auto correlation = both / std::sqrt (leftSquares * rightSquares);

        // The model: -1.000.
        INFO ("correlation " << correlation);
        CHECK (correlation < -0.9);
    }
}

// 4. Switching ----------------------------------------------------------------------------------

TEST_CASE ("changing Shape and Stereo while playing is click-free", "[delay][w2]")
{
    auto targets = oneTap (20.0);
    targets.dryGain = 1.0;
    targets.taps[0].modDepthCents = 12.0;
    targets.taps[0].modRateHz = 0.8;

    DelayDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    auto signal = tone (200.0, 0.25, 48000);
    const auto at = [] (double seconds) { return (int) std::lround (seconds * rate / 64.0) * 64; };

    for (int start = 0; start < signal.size(); start += 64)
    {
        if (start == at (0.2)) { targets.taps[0].modShape = DelayDsp::triangle; dsp.setTargets (targets); }
        if (start == at (0.3)) { targets.taps[0].modStereoDegrees = 0.0; dsp.setTargets (targets); }
        if (start == at (0.5)) { targets.taps[0].modShape = DelayDsp::sine; dsp.setTargets (targets); }
        if (start == at (0.6)) { targets.taps[0].modStereoDegrees = 180.0; dsp.setTargets (targets); }

        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, 64);
    }

    auto worst = 0.0;

    for (const auto* channel : { &signal.left, &signal.right })
    {
        const auto& x = *channel;

        for (size_t i = 2400; i < x.size(); ++i)
            worst = std::max (worst, std::abs ((double) x[i] - 2.0 * x[i - 1] + x[i - 2]));
    }

    // The model: 3.3e-4 on the left.
    INFO ("worst second difference " << worst);
    CHECK (worst < 2.0e-3);
}
