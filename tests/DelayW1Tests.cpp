// Phase 4 Wave 1 for Fancy Delay: Freeze and the output ceiling (docs/delay.md, "Phase 4 Wave 1",
// section 4).

#include "DelayDsp.h"
#include "DelayProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = 3.14159265358979323846;

double rmsDb (const std::vector<float>& samples, int from, int to)
{
    double sum = 0.0;

    for (int i = from; i < to; ++i)
        sum += (double) samples[(size_t) i] * (double) samples[(size_t) i];

    return 10.0 * std::log10 (std::max (sum / (double) (to - from), 1.0e-30));
}
} // namespace

TEST_CASE ("Freeze holds the repeats and keeps new input out", "[delay][w1]")
{
    DelayDsp::Targets targets;
    targets.taps[0].used = true;
    targets.taps[0].noteMode = false;
    targets.taps[0].lengthMs = 250.0;
    targets.taps[0].feedback = 0.5;
    targets.dryGain = 0.0;

    DelayDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    // A 100 ms burst of tone, then silence until 0.6 s, then Freeze, then loud input from 2 s.
    const auto total = (int) (12.0 * rate);
    std::vector<float> left ((size_t) total), right ((size_t) total);

    for (int i = 0; i < total; ++i)
    {
        const auto t = (double) i / rate;
        const auto x = (t < 0.1 || t > 2.0) ? (float) (0.3 * std::sin (2.0 * pi * 440.0 * t)) : 0.0f;
        left[(size_t) i] = right[(size_t) i] = x;
    }

    for (int start = 0; start < total; start += 512)
    {
        if (start >= (int) (0.6 * rate))
        {
            targets.freeze = true;
            dsp.setTargets (targets);
        }

        float* channels[] { left.data() + start, right.data() + start };
        dsp.process (channels, 2, std::min (512, total - start));
    }

    // The loop's level just after freezing, against 10 s later.
    const auto atFreeze = rmsDb (left, (int) (0.75 * rate), (int) (1.75 * rate));
    const auto later = rmsDb (left, (int) (10.75 * rate), (int) (11.75 * rate));

    INFO ("at freeze " << atFreeze << " dB, 10 s later " << later << " dB");
    CHECK (atFreeze > -40.0);
    CHECK (later == Catch::Approx (atFreeze).margin (0.5));
}

TEST_CASE ("Delay's ceiling holds 110 % feedback under it, and is Off by default", "[delay][w1]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    DelayProcessor processor;
    auto set = [&] (const juce::String& id, float value)
    {
        auto* parameter = processor.getApvts().getParameter (id);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    CHECK (processor.getApvts().getParameter ("ceiling")->getCurrentValueAsText() == "Off");

    set ("ceiling", -1.0f);
    processor.prepareToPlay (rate, 512);

    for (auto* parameter : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            if (ranged->getParameterID() == "t1fb")
                ranged->setValueNotifyingHost (ranged->convertTo0to1 (ranged->getNormalisableRange().end));

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    auto peak = 0.0f;

    for (int block = 0; block < (int) (10.0 * rate / 512.0); ++block)
    {
        buffer.clear();

        if (block < 10)
            for (int i = 0; i < 512; ++i)
                buffer.setSample (0, i, (float) (0.9 * std::sin (2.0 * pi * 200.0 * (block * 512 + i) / rate))),
                buffer.setSample (1, i, buffer.getSample (0, i));

        processor.processBlock (buffer, midi);

        if (block > 20)
            peak = std::max ({ peak, buffer.getMagnitude (0, 0, 512), buffer.getMagnitude (1, 0, 512) });
    }

    CHECK (peak <= std::pow (10.0f, -1.0f / 20.0f) + 1.0e-4f);
    CHECK (processor.getLatencySamples() > 0);
}
