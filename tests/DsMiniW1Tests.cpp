// Phase 4 Wave 1 for Fancy DS-Mini: Auto Amount (docs/ds-mini.md, "Phase 4 Wave 1", section 2).

#include "DsMiniProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <random>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = 3.14159265358979323846;

// A voice with "s" bursts sibilanceDb louder or quieter, through a processor; returns the
// working Amount at the end.
float workingAfter (double sibilanceDb, bool autoOn, float knob = 5.0f)
{
    DsMiniProcessor processor;
    auto set = [&] (const char* id, float value)
    {
        auto* parameter = processor.getApvts().getParameter (id);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    set ("amount", knob);
    set ("autoAmount", autoOn ? 1.0f : 0.0f);
    processor.prepareToPlay (rate, 512);

    std::mt19937 random (6);
    std::uniform_real_distribution<double> noise (-1.0, 1.0);
    const auto hiss = std::pow (10.0, sibilanceDb / 20.0);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    double highPassed = 0.0, previous = 0.0;

    for (int block = 0; block < (int) (4.0 * rate / 512.0); ++block)
    {
        for (int i = 0; i < 512; ++i)
        {
            const auto t = (double) (block * 512 + i) / rate;
            const auto burst = std::fmod (t, 0.3) < 0.08 ? 1.0 : 0.0;
            const auto w = noise (random);
            highPassed = 0.5 * (highPassed + w - previous);   // crude high-pass: the hiss is bright
            previous = w;

            const auto x = (float) (0.3 * std::sin (2.0 * pi * 220.0 * t) + 0.15 * hiss * burst * highPassed);
            buffer.setSample (0, i, x);
            buffer.setSample (1, i, x);
        }

        processor.processBlock (buffer, midi);
    }

    return processor.getWorkingAmount();
}
} // namespace

TEST_CASE ("Auto Amount rides with the sibilance, within 3 of the knob", "[dsmini][w1]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto soft = workingAfter (-6.0, true);
    const auto harsh = workingAfter (6.0, true);

    INFO ("soft " << soft << ", harsh " << harsh);
    CHECK (harsh > soft + 1.0f);
    CHECK (soft >= 2.0f);
    CHECK (harsh <= 8.0f);

    // Off, the working Amount is the knob's.
    CHECK (workingAfter (6.0, false, 4.0f) == 4.0f);
}
