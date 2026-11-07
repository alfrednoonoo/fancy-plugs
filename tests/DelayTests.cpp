#include "DelayDsp.h"
#include "DelayEditor.h"
#include "DelayProcessor.h"
#include "TapMap.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <juce_dsp/juce_dsp.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <random>
#include <vector>

using Catch::Approx;

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = std::numbers::pi;

DelayDsp::Targets oneTap (double ms = 100.0)
{
    DelayDsp::Targets targets;

    targets.taps[0].used = true;
    targets.taps[0].noteMode = false;
    targets.taps[0].lengthMs = ms;
    targets.dryGain = 0.0;

    return targets;
}

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples), right ((size_t) numSamples) {}

    int size() const { return (int) left.size(); }

    void process (DelayDsp& dsp, const DelayDsp::Targets& targets, int blockSize = 64, int start = 0)
    {
        for (int position = start; position < size(); position += blockSize)
        {
            const auto count = std::min (blockSize, size() - position);
            float* channels[] { left.data() + position, right.data() + position };

            dsp.setTargets (targets);
            dsp.process (channels, 2, count);
        }
    }
};

Stereo impulse (int numSamples)
{
    Stereo signal (numSamples);
    signal.left[0] = signal.right[0] = 1.0f;
    return signal;
}

void prepare (DelayDsp& dsp, const DelayDsp::Targets& targets, double sampleRate = rate)
{
    dsp.prepare (sampleRate);
    dsp.snapToTargets (targets);
}

int firstSound (const std::vector<float>& samples, int from = 0)
{
    for (int i = from; i < (int) samples.size(); ++i)
        if (std::abs (samples[(size_t) i]) > 1.0e-6f)
            return i;

    return -1;
}

// Energy at and above a frequency, in a window starting at the given sample.
double energyAbove (const std::vector<float>& samples, int start, double hz, double sampleRate = rate)
{
    constexpr int order = 12, size = 1 << order;

    juce::dsp::FFT fft (order);
    std::vector<float> data ((size_t) size * 2, 0.0f);

    for (int i = 0; i < size && start + i < (int) samples.size(); ++i)
        data[(size_t) i] = samples[(size_t) (start + i)];

    fft.performFrequencyOnlyForwardTransform (data.data());

    auto energy = 0.0;

    for (int bin = juce::roundToInt (hz * size / sampleRate); bin <= size / 2; ++bin)
        energy += (double) data[(size_t) bin] * (double) data[(size_t) bin];

    return energy;
}

double largestStep (const std::vector<float>& samples, int from, int to)
{
    auto largest = 0.0;

    for (int i = from + 1; i < to; ++i)
        largest = std::max (largest, std::abs ((double) samples[(size_t) i] - (double) samples[(size_t) i - 1]));

    return largest;
}

double peakOf (const std::vector<float>& samples, int from = 0)
{
    auto peak = 0.0;

    for (int i = from; i < (int) samples.size(); ++i)
        peak = std::max (peak, (double) std::abs (samples[(size_t) i]));

    return peak;
}

void setParameter (DelayProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getApvts().getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

std::vector<float> render (DelayProcessor& processor, int numSamples)
{
    juce::AudioBuffer<float> buffer (2, numSamples);
    juce::MidiBuffer midi;

    for (int i = 0; i < numSamples; ++i)
    {
        buffer.setSample (0, i, 0.25f * (float) std::sin (2.0 * pi * 220.0 * i / rate));
        buffer.setSample (1, i, 0.25f * (float) std::sin (2.0 * pi * 331.0 * i / rate));
    }

    processor.processBlock (buffer, midi);

    std::vector<float> samples (buffer.getReadPointer (0), buffer.getReadPointer (0) + numSamples);
    samples.insert (samples.end(), buffer.getReadPointer (1), buffer.getReadPointer (1) + numSamples);
    return samples;
}

class FixedPlayHead final : public juce::AudioPlayHead
{
public:
    explicit FixedPlayHead (double beatsPerMinute) : bpm (beatsPerMinute) {}

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (bpm);
        return info;
    }

    double bpm;
};
} // namespace

// Note lengths ------------------------------------------------------------------------------

TEST_CASE ("note lengths are exact at 120 BPM", "[delay]")
{
    CHECK (delay::noteMs ({ 3, delay::dotted, 1 }, 120.0) == Approx (375.0));
    CHECK (delay::noteMs ({ 3, delay::triplet, 1 }, 120.0) == Approx (166.6667).margin (0.001));
    CHECK (delay::noteMs ({ 2, delay::straight, 1 }, 120.0) == Approx (500.0));
    CHECK (delay::noteMs ({ 0, delay::straight, 1 }, 120.0) == Approx (2000.0));
    CHECK (delay::noteMs ({ 6, delay::straight, 1 }, 120.0) == Approx (31.25));
    CHECK (delay::noteMs ({ 2, delay::straight, 3 }, 120.0) == Approx (1500.0));
}

TEST_CASE ("long note lengths stop at the longest delay", "[delay]")
{
    CHECK (delay::noteMs ({ 0, delay::dotted, 16 }, 20.0) == delay::maxDelayMs);
}

TEST_CASE ("note lengths arrive within a sample of their time", "[delay]")
{
    struct Case
    {
        int note, feel;
        double ms;
    };

    const auto which = GENERATE (Case { 3, delay::dotted, 375.0 }, Case { 3, delay::triplet, 500.0 / 3.0 }, Case { 2, delay::straight, 500.0 });

    auto targets = oneTap();
    targets.taps[0].noteMode = true;
    targets.taps[0].noteLength = { which.note, which.feel, 1 };
    targets.bpm = 120.0;

    DelayDsp dsp;
    prepare (dsp, targets);

    auto signal = impulse (30000);
    signal.process (dsp, targets);

    const auto expected = which.ms * 0.001 * rate;

    CHECK (std::abs ((double) firstSound (signal.left) - expected) <= 1.0);
    CHECK (signal.left[(size_t) firstSound (signal.left)] == 1.0f);
}

TEST_CASE ("the nearest note prefers simple lengths", "[delay]")
{
    const auto dotted = delay::nearestNote (376.0, 120.0);

    CHECK (delay::noteMs (dotted, 120.0) == Approx (375.0));
    CHECK (dotted.multiplier == 1);

    const auto quarter = delay::nearestNote (503.0, 120.0);

    CHECK (delay::noteMs (quarter, 120.0) == Approx (500.0));
    CHECK (quarter.feel == delay::straight);
    CHECK (quarter.multiplier == 1);
}

TEST_CASE ("the host tempo moves a tap in Note mode", "[delay]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    DelayProcessor processor;
    FixedPlayHead playHead (60.0);

    processor.setPlayHead (&playHead);
    processor.prepareToPlay (rate, 512);

    CHECK (processor.getBpm() == 60.0);
    CHECK (processor.hasHostTempo());

    auto targets = processor.readTargets();
    DelayDsp dsp;
    prepare (dsp, targets);

    CHECK (dsp.getDelaySamples (0) == 36000);

    targets.bpm = 120.0;
    dsp.setTargets (targets);

    CHECK (dsp.getDelaySamples (0) == 18000);

    processor.setPlayHead (nullptr);
}

// P2.3 --------------------------------------------------------------------------------------

TEST_CASE ("filters inside the loop darken every repeat", "[delay]")
{
    constexpr int delaySamples = 9600, repeats = 5;

    auto targets = oneTap (delaySamples / rate * 1000.0);
    targets.taps[0].feedback = 0.6;
    targets.taps[0].lowpassHz = 2000.0;

    const auto brightness = [&] (bool inLoop)
    {
        targets.taps[0].filtersInLoop = inLoop;

        DelayDsp dsp;
        prepare (dsp, targets);

        auto signal = impulse (delaySamples * (repeats + 1));
        signal.process (dsp, targets);

        std::vector<double> ratios;

        for (int repeat = 1; repeat <= repeats; ++repeat)
        {
            const auto start = delaySamples * repeat - 64;
            ratios.push_back (energyAbove (signal.left, start, 2000.0) / energyAbove (signal.left, start, 20.0));
        }

        return ratios;
    };

    const auto inside = brightness (true);

    for (size_t i = 1; i < inside.size(); ++i)
    {
        INFO ("repeat " << i);
        CHECK (inside[i] < inside[i - 1] * 0.9);
    }

    // The original's behaviour: filtered once, so every repeat has the same tone.
    const auto outside = brightness (false);

    for (size_t i = 1; i < outside.size(); ++i)
        CHECK (outside[i] == Approx (outside[0]).epsilon (0.02));
}

TEST_CASE ("ping-pong alternates at the right level", "[delay]")
{
    constexpr int delaySamples = 4800;
    constexpr double feedback = 0.5;

    auto targets = oneTap (delaySamples / rate * 1000.0);
    targets.taps[0].feedback = feedback;
    targets.taps[0].pingPong = true;

    const auto startsLeft = GENERATE (true, false);
    targets.taps[0].pan = startsLeft ? -0.6 : 0.6;

    DelayDsp dsp;
    prepare (dsp, targets);

    // Below the feedback safety curve, which has its own test.
    Stereo signal (delaySamples * 7);
    signal.left[0] = signal.right[0] = 0.5f;
    signal.process (dsp, targets);

    auto expected = 0.5;

    for (int repeat = 1; repeat <= 6; ++repeat)
    {
        const auto onLeft = (repeat % 2 == 1) == startsLeft;
        const auto at = (size_t) (delaySamples * repeat);

        INFO ("repeat " << repeat);
        CHECK ((onLeft ? signal.left[at] : signal.right[at]) == Approx (expected).margin (1.0e-3));
        CHECK ((onLeft ? signal.right[at] : signal.left[at]) == Approx (0.0).margin (1.0e-3));

        expected *= feedback;
    }
}

TEST_CASE ("full feedback through a tempo ramp stays below 0 dBFS", "[delay]")
{
    constexpr int blockSize = 128;
    constexpr int numSamples = (int) (30.0 * rate);

    auto targets = oneTap();
    targets.taps[0].noteMode = true;
    targets.taps[0].noteLength = { 3, delay::dotted, 1 };
    targets.taps[0].feedback = 1.0;
    targets.taps[0].saturate = GENERATE (false, true);

    DelayDsp dsp;
    prepare (dsp, targets);

    std::mt19937 random (1);
    std::uniform_real_distribution<float> noise (-0.1f, 0.1f);

    auto phase = 0.0;
    auto peak = 0.0;
    auto finite = true;

    float left[blockSize], right[blockSize];
    float* channels[] { left, right };

    for (int position = 0; position < numSamples; position += blockSize)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            left[i] = right[i] = 0.5f * (float) std::sin (phase) + noise (random);
            phase += 2.0 * pi * 220.0 / rate;
        }

        targets.bpm = 90.0 + 90.0 * (double) position / (double) numSamples;

        dsp.setTargets (targets);
        dsp.process (channels, 2, blockSize);

        for (int i = 0; i < blockSize; ++i)
        {
            finite = finite && std::isfinite (left[i]) && std::isfinite (right[i]);
            peak = std::max ({ peak, (double) std::abs (left[i]), (double) std::abs (right[i]) });
        }
    }

    CHECK (finite);
    CHECK (peak < 1.0);
    CHECK (peak > 0.5);
}

TEST_CASE ("110 % feedback needs Saturate and is bounded with it", "[delay]")
{
    constexpr int delaySamples = 4800;

    auto targets = oneTap (delaySamples / rate * 1000.0);
    targets.taps[0].feedback = 1.1;

    SECTION ("Without Saturate it holds at 100 %")
    {
        DelayDsp dsp;
        prepare (dsp, targets);

        Stereo signal (delaySamples * 8);
        signal.left[0] = signal.right[0] = 0.5f;
        signal.process (dsp, targets);

        // The DC block in the loop takes a little from each pass of an impulse.
        CHECK (signal.left[(size_t) (delaySamples * 7)] == Approx (0.5f).margin (0.01));
    }

    SECTION ("With Saturate it grows to a limit below 0 dBFS")
    {
        targets.taps[0].saturate = true;

        DelayDsp dsp;
        prepare (dsp, targets);

        Stereo signal ((int) rate * 5);

        for (int i = 0; i < 4800; ++i)
            signal.left[(size_t) i] = signal.right[(size_t) i] = 0.5f * (float) std::sin (2.0 * pi * 300.0 * i / rate);

        signal.process (dsp, targets);

        CHECK (peakOf (signal.left) < 1.0);
    }
}

TEST_CASE ("a step in time adds no jump beyond the input's own", "[delay]")
{
    constexpr int stepAt = 48000;
    constexpr int numSamples = stepAt + 24000 + 48000;
    constexpr double hz = 440.0, level = 0.5;

    // The step's own extra slope during the crossfade is a few parts in a thousand of the input's.
    constexpr double margin = 1.05;

    auto before = oneTap (375.0);
    before.smoothTime = GENERATE (true, false);

    auto after = before;
    after.taps[0].lengthMs = 500.0;

    DelayDsp dsp;
    prepare (dsp, before);

    Stereo output (numSamples);

    for (int i = 0; i < numSamples; ++i)
        output.left[(size_t) i] = output.right[(size_t) i] = (float) (level * std::sin (2.0 * pi * hz * i / rate));

    const auto inputStep = largestStep (output.left, 0, numSamples);

    for (int position = 0; position < numSamples; position += 64)
    {
        const auto count = std::min (64, numSamples - position);
        float* pointers[] { output.left.data() + position, output.right.data() + position };

        dsp.setTargets (position >= stepAt ? after : before);
        dsp.process (pointers, 2, count);
    }

    const auto settledFrom = (int) (0.375 * rate) + 256;
    const auto largest = largestStep (output.left, settledFrom, numSamples);

    INFO ("input step " << inputStep << ", output step " << largest);
    CHECK (largest <= inputStep * margin);
}

TEST_CASE ("Dry at -inf leaves the delayed signal at full level", "[delay]")
{
    auto targets = oneTap (100.0);
    targets.dryGain = 0.0;
    targets.wetGain = 1.0;

    DelayDsp dsp;
    prepare (dsp, targets);

    auto signal = impulse (10000);
    signal.process (dsp, targets);

    CHECK (signal.left[0] == 0.0f);
    CHECK (signal.left[4800] == 1.0f);
    CHECK (signal.right[4800] == 1.0f);
}

TEST_CASE ("Wet and Dry are independent", "[delay]")
{
    auto targets = oneTap (100.0);
    targets.dryGain = 0.5;
    targets.wetGain = 0.25;

    DelayDsp dsp;
    prepare (dsp, targets);

    auto signal = impulse (10000);
    signal.process (dsp, targets);

    CHECK (signal.left[0] == 0.5f);
    CHECK (signal.left[4800] == 0.25f);
}

// Tap controls ------------------------------------------------------------------------------

TEST_CASE ("a plain delay is an exact copy", "[delay]")
{
    auto targets = oneTap (10.0);

    DelayDsp dsp;
    prepare (dsp, targets);

    std::mt19937 random (2);
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);

    Stereo signal (4800);

    for (auto& sample : signal.left)
        sample = noise (random);

    signal.right = signal.left;

    const auto input = signal.left;
    signal.process (dsp, targets);

    for (int i = 480; i < 4800; ++i)
        REQUIRE (signal.left[(size_t) i] == input[(size_t) (i - 480)]);
}

TEST_CASE ("a held delay doesn't dull its repeats through interpolation", "[delay]")
{
    // 0.1 ms is 4.8 samples, which rounds to a whole sample.
    auto targets = oneTap (100.1);
    targets.taps[0].feedback = 1.0;

    DelayDsp dsp;
    prepare (dsp, targets);

    CHECK (dsp.getDelaySamples (0) == 4805);

    Stereo signal (4805 * 9);
    signal.left[0] = signal.right[0] = 0.5f;
    signal.process (dsp, targets);

    // Interpolation would put a large part of the impulse into its neighbour. All that's
    // there is the DC block's small tail, from eight passes.
    CHECK (signal.left[4805 * 8] > 0.49f);
    CHECK (std::abs (signal.left[4805 * 8 + 1]) < 0.01f);
}

TEST_CASE ("the feedback safety curve only touches peaks above its knee", "[delay]")
{
    const auto level = [] (float input, double feedback)
    {
        auto targets = oneTap (10.0);
        targets.taps[0].feedback = feedback;

        DelayDsp dsp;
        prepare (dsp, targets);

        Stereo signal (1000);
        signal.left[0] = signal.right[0] = input;
        signal.process (dsp, targets);

        return signal.left[480];
    };

    CHECK (level (0.89f, 0.5) == 0.89f);
    CHECK (level (1.0f, 0.5) < 1.0f);
    CHECK (level (1.0f, 0.5) > 0.95f);
    CHECK (level (2.0f, 0.5) < 1.0f);

    // With Feedback at 0 % it isn't in the path at all.
    CHECK (level (1.0f, 0.0) == 1.0f);
    CHECK (level (2.0f, 0.0) == 2.0f);
}

TEST_CASE ("pan is a balance law and centre is unity", "[delay]")
{
    auto targets = oneTap (10.0);
    targets.taps[0].pan = GENERATE (-1.0, -0.5, 0.0, 0.5, 1.0);

    DelayDsp dsp;
    prepare (dsp, targets);

    auto signal = impulse (1000);
    signal.process (dsp, targets);

    const auto pan = targets.taps[0].pan;

    CHECK (signal.left[480] == Approx (std::min (1.0, 1.0 - pan)));
    CHECK (signal.right[480] == Approx (std::min (1.0, 1.0 + pan)));
}

TEST_CASE ("width 0 is mono and width 2 doubles the sides", "[delay]")
{
    auto targets = oneTap (10.0);
    targets.taps[0].width = GENERATE (0.0, 1.0, 2.0);

    DelayDsp dsp;
    prepare (dsp, targets);

    Stereo signal (1000);
    signal.left[0] = 1.0f;
    signal.process (dsp, targets);

    const auto width = targets.taps[0].width;

    CHECK (signal.left[480] == Approx (0.5 + 0.5 * width));
    CHECK (signal.right[480] == Approx (0.5 - 0.5 * width).margin (1.0e-6));
}

TEST_CASE ("resolution rounds to the grid and 24 bit is off", "[delay]")
{
    auto targets = oneTap (1.0);
    targets.taps[0].bits = 4.0;

    DelayDsp dsp;
    prepare (dsp, targets);

    Stereo signal (200);
    signal.left[0] = signal.right[0] = 0.3f;
    signal.process (dsp, targets);

    CHECK (signal.left[48] == Approx (0.25f));

    targets.taps[0].bits = 24.0;
    prepare (dsp, targets);

    Stereo clean (200);
    clean.left[0] = clean.right[0] = 0.3f;
    clean.process (dsp, targets);

    CHECK (clean.left[48] == 0.3f);
}

TEST_CASE ("volume scales the tap", "[delay]")
{
    auto targets = oneTap (1.0);
    targets.taps[0].volumeGain = 0.5;

    DelayDsp dsp;
    prepare (dsp, targets);

    auto signal = impulse (200);
    signal.process (dsp, targets);

    CHECK (signal.left[48] == 0.5f);
}

TEST_CASE ("solo mutes the other taps but keeps them running", "[delay]")
{
    auto targets = oneTap (1.0);
    targets.taps[1] = targets.taps[0];
    targets.taps[1].lengthMs = 2.0;
    targets.taps[1].solo = true;

    DelayDsp dsp;
    prepare (dsp, targets);

    auto signal = impulse (400);
    signal.process (dsp, targets);

    CHECK (signal.left[48] == 0.0f);
    CHECK (signal.left[96] == 1.0f);
}

TEST_CASE ("a tap that's off passes nothing, and doesn't replay when it comes back", "[delay]")
{
    auto targets = oneTap (10.0);
    targets.taps[0].on = false;

    DelayDsp dsp;
    prepare (dsp, targets);

    auto signal = impulse (4800);
    signal.process (dsp, targets);

    CHECK (peakOf (signal.left) == 0.0);

    targets.taps[0].on = true;

    Stereo silence (4800);
    silence.process (dsp, targets);

    CHECK (peakOf (silence.left) == 0.0);
}

TEST_CASE ("a removed tap drains and goes to sleep", "[delay]")
{
    auto targets = oneTap (10.0);
    targets.taps[0].feedback = 0.5;

    DelayDsp dsp;
    prepare (dsp, targets);

    CHECK_FALSE (dsp.isTapAsleep (0));
    CHECK (dsp.isTapAsleep (1));

    targets.taps[0].used = false;

    Stereo silence ((int) rate * 13);
    silence.process (dsp, targets, 512);

    CHECK (dsp.isTapAsleep (0));
    CHECK (peakOf (silence.left) == 0.0);
}

TEST_CASE ("moving filters don't click", "[delay]")
{
    constexpr int numSamples = 48000;
    constexpr double hz = 220.0;

    auto targets = oneTap (10.0);
    targets.taps[0].lowpassHz = 500.0;

    DelayDsp dsp;
    prepare (dsp, targets);

    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) (0.5 * std::sin (2.0 * pi * hz * i / rate));

    const auto inputStep = largestStep (signal.left, 0, numSamples);

    for (int position = 0; position < numSamples; position += 64)
    {
        const auto count = std::min (64, numSamples - position);
        float* pointers[] { signal.left.data() + position, signal.right.data() + position };

        const auto sweep = (double) position / (double) numSamples;
        targets.taps[0].lowpassHz = 500.0 * std::pow (30.0, sweep);
        targets.taps[0].highpassHz = 20.0 * std::pow (5.0, sweep);

        dsp.setTargets (targets);
        dsp.process (pointers, 2, count);
    }

    CHECK (largestStep (signal.left, 1024, numSamples) < inputStep * 1.2);
}

// Host behaviour ----------------------------------------------------------------------------

TEST_CASE ("every sample rate and block size lands in the same place", "[delay]")
{
    const auto sampleRate = GENERATE (44100.0, 48000.0, 96000.0, 192000.0, 384000.0);
    const auto blockSize = GENERATE (1, 64, 512, 8192);

    auto targets = oneTap (100.0);

    DelayDsp dsp;
    prepare (dsp, targets, sampleRate);

    const auto expected = (int) std::lround (0.1 * sampleRate);

    auto signal = impulse (expected + 100);
    signal.process (dsp, targets, blockSize);

    CHECK (firstSound (signal.left) == expected);
    CHECK (signal.left[(size_t) expected] == 1.0f);
}

TEST_CASE ("variable block sizes change nothing", "[delay]")
{
    auto targets = oneTap (10.0);
    targets.taps[0].feedback = 0.5;
    targets.taps[0].lowpassHz = 3000.0;

    const auto renderIn = [&] (const std::vector<int>& blocks)
    {
        DelayDsp dsp;
        prepare (dsp, targets);

        auto signal = impulse (20000);
        auto position = 0;

        for (size_t i = 0; position < signal.size(); ++i)
        {
            const auto count = std::min (blocks[i % blocks.size()], signal.size() - position);
            float* pointers[] { signal.left.data() + position, signal.right.data() + position };

            dsp.setTargets (targets);
            dsp.process (pointers, 2, count);
            position += count;
        }

        return signal.left;
    };

    CHECK (renderIn ({ 512 }) == renderIn ({ 1, 7, 64, 1000, 3, 8192, 300 }));
}

TEST_CASE ("mono works and ignores the stereo controls", "[delay]")
{
    auto targets = oneTap (10.0);
    targets.taps[0].pan = 1.0;
    targets.taps[0].width = 0.0;
    targets.taps[0].pingPong = true;

    DelayDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets, true);

    std::vector<float> samples (1000);
    samples[0] = 1.0f;

    float* channels[] { samples.data() };

    dsp.setTargets (targets, true);
    dsp.process (channels, 1, 1000);

    CHECK (samples[480] == 1.0f);
}

TEST_CASE ("bypass fades to the input and back without a step", "[delay]")
{
    constexpr int numSamples = 48000;

    auto targets = oneTap (10.0);
    targets.dryGain = 1.0;

    DelayDsp dsp;
    prepare (dsp, targets);

    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) (0.5 * std::sin (2.0 * pi * 440.0 * i / rate));

    const auto input = signal.left;

    for (int position = 0; position < numSamples; position += 64)
    {
        const auto count = std::min (64, numSamples - position);
        float* pointers[] { signal.left.data() + position, signal.right.data() + position };

        targets.bypass = position >= 12000;

        dsp.setTargets (targets);
        dsp.process (pointers, 2, count);
    }

    for (int i = 24000; i < numSamples; ++i)
        REQUIRE (signal.left[(size_t) i] == Approx (input[(size_t) i]).margin (1.0e-5));
}

TEST_CASE ("random settings stay finite", "[delay]")
{
    std::mt19937 random (3);
    std::uniform_real_distribution<double> unit (0.0, 1.0);

    const auto chance = [&] { return unit (random) < 0.5; };

    DelayDsp dsp;
    dsp.prepare (rate);

    DelayDsp::Targets targets;
    dsp.snapToTargets (targets);

    float left[1024], right[1024];
    float* channels[] { left, right };

    for (int round = 0; round < 300; ++round)
    {
        for (auto& tap : targets.taps)
        {
            tap.used = chance();
            tap.on = chance();
            tap.solo = unit (random) < 0.1;
            tap.noteMode = chance();
            tap.pingPong = chance();
            tap.filtersInLoop = chance();
            tap.saturate = chance();
            tap.lengthMs = unit (random) * 10000.0;
            tap.noteLength = { (int) (unit (random) * 7.0), (int) (unit (random) * 3.0), 1 + (int) (unit (random) * 16.0) };
            tap.feedback = unit (random) * 1.1;
            tap.lowpassHz = 200.0 * std::pow (110.0, unit (random));
            tap.highpassHz = 20.0 * std::pow (250.0, unit (random));
            tap.bits = 4.0 + std::floor (unit (random) * 21.0);
            tap.width = unit (random) * 2.0;
            tap.volumeGain = unit (random) * 4.0;
            tap.pan = unit (random) * 2.0 - 1.0;
        }

        targets.bpm = 20.0 + unit (random) * 979.0;
        targets.smoothTime = chance();

        for (int i = 0; i < 1024; ++i)
            left[i] = right[i] = (float) (unit (random) * 2.0 - 1.0);

        dsp.setTargets (targets);
        dsp.process (channels, 2, 1024);

        for (int i = 0; i < 1024; ++i)
            REQUIRE ((std::isfinite (left[i]) && std::isfinite (right[i])));
    }
}

// Processor ---------------------------------------------------------------------------------

TEST_CASE ("delay automation with an unchanged value is bit-identical", "[delay]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    DelayProcessor plain, automated;

    for (auto* processor : { &plain, &automated })
    {
        setParameter (*processor, "t1fb", 40.0f);
        setParameter (*processor, "t1lp", 3456.0f);
        setParameter (*processor, "t1pan", -30.0f);
        processor->prepareToPlay (rate, 48000);
    }

    for (auto* parameter : automated.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
        const auto before = automated.getApvts().getRawParameterValue (ranged->getParameterID())->load();

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->getValue());
        parameter->endChangeGesture();

        INFO (ranged->getParameterID());
        CHECK (automated.getApvts().getRawParameterValue (ranged->getParameterID())->load() == before);
    }

    REQUIRE (render (plain, 48000) == render (automated, 48000));
}

TEST_CASE ("delay state survives a save and load", "[delay]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    DelayProcessor source, restored;

    setParameter (source, "t1fb", 55.0f);
    setParameter (source, "t3used", 1.0f);
    setParameter (source, "t3note", 4.0f);
    setParameter (source, "wet", -9.0f);

    juce::MemoryBlock state;
    source.getStateInformation (state);
    restored.setStateInformation (state.getData(), (int) state.getSize());

    CHECK (restored.readTap (0).feedback == Approx (0.55));
    CHECK (restored.isTapUsed (2));
    CHECK (restored.readTap (2).noteLength.note == 4);
    CHECK (restored.countTaps() == 2);

    source.prepareToPlay (rate, 4800);
    restored.prepareToPlay (rate, 4800);

    CHECK (render (source, 4800) == render (restored, 4800));
}

TEST_CASE ("damaged delay state is ignored", "[delay]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    DelayProcessor processor;
    const char junk[] { 1, 2, 3, 4, 5, 6, 7, 8 };

    processor.setStateInformation (junk, (int) sizeof (junk));

    CHECK (processor.countTaps() == 1);
}

TEST_CASE ("every factory preset loads and plays", "[delay]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    DelayProcessor processor;
    auto& presets = processor.getPresets();

    const juce::StringArray expected { "Default", "Slapback", "1/8 dotted", "1/4 ping-pong", "Triplet echo", "Lo-fi tape", "Haas widener" };

    for (const auto& name : expected)
        CHECK (presets.getNames().contains (name));

    for (int i = 0; i < expected.size(); ++i)
    {
        presets.load (presets.getNames().indexOf (expected[i]));
        processor.prepareToPlay (rate, 48000);

        for (const auto sample : render (processor, 48000))
            REQUIRE (std::isfinite (sample));
    }

    presets.load (presets.getNames().indexOf ("Haas widener"));

    CHECK (processor.countTaps() == 2);
    CHECK (processor.readTap (0).lengthMs == Approx (8.0));
    CHECK (processor.readTap (1).lengthMs == Approx (13.0));
    CHECK (processor.readTap (0).pan == Approx (-1.0));
    CHECK (processor.readTap (1).pan == Approx (1.0));

    presets.load (presets.getNames().indexOf ("Lo-fi tape"));

    CHECK (processor.readTap (0).bits == 10.0);
    CHECK (processor.readTap (0).lowpassHz == Approx (3000.0));
    CHECK (processor.readTap (0).saturate);
}

TEST_CASE ("taps can be added up to 16 and at least one remains", "[delay]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    DelayProcessor processor;

    CHECK (processor.countTaps() == 1);

    for (int i = 1; i < delay::maxTaps; ++i)
        CHECK (processor.addTap (i - 1) == i);

    CHECK (processor.addTap (0) == -1);
    CHECK (processor.countTaps() == delay::maxTaps);

    for (int i = delay::maxTaps - 1; i > 0; --i)
        processor.removeTap (i);

    processor.removeTap (0);

    CHECK (processor.countTaps() == 1);
    CHECK (processor.isTapUsed (0));
}

TEST_CASE ("an added tap lands after the one it follows", "[delay]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    DelayProcessor processor;

    const auto added = processor.addTap (0);
    const auto first = DelayDsp::tapMs (processor.readTap (0), 120.0);
    const auto second = DelayDsp::tapMs (processor.readTap (added), 120.0);

    CHECK (second == Approx (first * 2.0));
}

TEST_CASE ("delay values can be typed", "[delay]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    DelayProcessor processor;

    const auto typed = [&] (const char* id, const char* text)
    {
        const juce::RangedAudioParameter& parameter = *processor.getApvts().getParameter (id);
        return parameter.convertFrom0to1 (parameter.getValueForText (text));
    };

    CHECK (typed ("t1lp", "2.5k") == Approx (2500.0f).margin (1.0f));
    CHECK (typed ("t1lp", "2500") == Approx (2500.0f).margin (1.0f));
    CHECK (typed ("t1hp", "180 Hz") == Approx (180.0f).margin (1.0f));
    CHECK (typed ("t1pan", "60 L") == -60.0f);
    CHECK (typed ("t1pan", "L60") == -60.0f);
    CHECK (typed ("t1pan", "60 R") == 60.0f);
    CHECK (typed ("t1pan", "-60") == -60.0f);
    CHECK (typed ("t1ms", "750") == Approx (750.0f));
    CHECK (typed ("t1fb", "42 %") == 42.0f);
    CHECK (typed ("t1bits", "10 bit") == 10.0f);
    CHECK (typed ("t1width", "1,5") == Approx (1.5f));
    CHECK (typed ("t1vol", "-6") == Approx (-6.0f));
    CHECK (typed ("t1note", "1/16") == 4.0f);
    CHECK (typed ("t1note", "8") == 3.0f);
    CHECK (typed ("wet", "-inf") == -60.0f);
}

TEST_CASE ("the time axis covers four beats and extends for long taps", "[delay]")
{
    CHECK (TapMap::beatsForLongest (375.0, 120.0) == 4);
    CHECK (TapMap::beatsForLongest (2000.0, 120.0) == 4);
    CHECK (TapMap::beatsForLongest (2001.0, 120.0) == 5);
    CHECK (TapMap::beatsForLongest (10000.0, 120.0) == 20);
}

TEST_CASE ("the delay editor opens", "[delay]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    DelayProcessor processor;
    processor.prepareToPlay (rate, 512);

    const std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());

    REQUIRE (editor != nullptr);
    CHECK (editor->getWidth() == 760);

    processor.editorBeingDeleted (editor.get());
}

// Measurements ------------------------------------------------------------------------------

TEST_CASE ("delay CPU measurement", "[.measure]")
{
    constexpr int blockSize = 128, blocks = 8000;

    const auto measure = [] (int numTaps, bool everything)
    {
        DelayDsp::Targets targets;

        for (int tap = 0; tap < numTaps; ++tap)
        {
            auto& spec = targets.taps[(size_t) tap];

            spec.used = true;
            spec.noteMode = false;
            spec.lengthMs = 100.0 + 50.0 * tap;

            if (everything)
            {
                spec.feedback = 0.5;
                spec.lowpassHz = 5000.0;
                spec.highpassHz = 100.0;
                spec.bits = 12.0;
                spec.width = 1.5;
                spec.pan = 0.3;
                spec.pingPong = true;
                spec.saturate = true;
            }
        }

        DelayDsp dsp;
        prepare (dsp, targets);

        float left[blockSize], right[blockSize];
        float* channels[] { left, right };

        std::mt19937 random (4);
        std::uniform_real_distribution<float> noise (-0.25f, 0.25f);

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

    for (const auto numTaps : { 1, 4, 16 })
        std::printf ("delay cpu: %d taps plain %.2f %%, everything on %.2f %%\n", numTaps, measure (numTaps, false), measure (numTaps, true));

    SUCCEED();
}

TEST_CASE ("delay response measurement", "[.measure]")
{
    constexpr int delaySamples = 9600, repeats = 5;

    auto targets = oneTap (delaySamples / rate * 1000.0);
    targets.taps[0].feedback = 0.6;
    targets.taps[0].lowpassHz = 2000.0;

    DelayDsp dsp;
    prepare (dsp, targets);

    auto signal = impulse (delaySamples * (repeats + 1));
    signal.process (dsp, targets);

    for (int repeat = 1; repeat <= repeats; ++repeat)
    {
        const auto start = delaySamples * repeat - 64;

        std::printf ("delay repeat %d: above 2 kHz is %.1f %% of its energy\n", repeat,
                     100.0 * energyAbove (signal.left, start, 2000.0) / energyAbove (signal.left, start, 20.0));
    }

    SUCCEED();
}
