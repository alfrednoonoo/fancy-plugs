// Phase 4 Wave 1 for Fancy Exciter: listen to added harmonics, the THD read-out, Auto gain and
// the ceiling (docs/exciter.md, "Phase 4 Wave 1", section 5).

#include "ExciterDsp.h"
#include "ExciterProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = 3.14159265358979323846;

std::vector<float> run (const ExciterDsp::Targets& targets, std::vector<float> signal)
{
    ExciterDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    auto right = signal;

    for (size_t start = 0; start < signal.size(); start += 333)
    {
        float* channels[] { signal.data() + start, right.data() + start };
        dsp.process (channels, 2, (int) std::min<size_t> (333, signal.size() - start));
    }

    return signal;
}

std::vector<float> music (int numSamples)
{
    std::mt19937 random (8);
    std::uniform_real_distribution<double> noise (-1.0, 1.0);
    std::vector<float> signal ((size_t) numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal[(size_t) i] = (float) (0.3 * std::sin (2.0 * pi * 300.0 * i / rate) + 0.2 * std::sin (2.0 * pi * 3500.0 * i / rate) + 0.05 * noise (random));

    return signal;
}
} // namespace

TEST_CASE ("Listen to added harmonics plus the dry signal is the normal output", "[exciter][w1]")
{
    ExciterDsp::Targets targets;
    targets.harmonics = 0.6;
    targets.boostGain = std::pow (10.0, 9.0 / 20.0);
    targets.oversampling = ExciterDsp::x2;

    const auto input = music (24000);
    const auto normal = run (targets, input);

    auto listening = targets;
    listening.listen = true;
    const auto added = run (listening, input);

    // The dry path through the same latency: Harmonics at 0 adds nothing.
    auto dryTargets = targets;
    dryTargets.harmonics = 0.0;
    const auto dry = run (dryTargets, input);

    auto worst = 0.0, addedPeak = 0.0;

    for (size_t i = 4800; i < input.size(); ++i)
    {
        worst = std::max (worst, std::abs ((double) dry[i] + (double) added[i] - (double) normal[i]));
        addedPeak = std::max (addedPeak, std::abs ((double) added[i]));
    }

    CHECK (worst < std::pow (10.0, -100.0 / 20.0));
    CHECK (addedPeak > 0.001);
}

TEST_CASE ("The THD read-out rises with Harmonics and Auto gain cancels the added level", "[exciter][w1]")
{
    ExciterDsp::Targets targets;
    targets.frequencyHz = 3000.0;
    targets.boostGain = std::pow (10.0, 12.0 / 20.0);

    targets.harmonics = 0.0;
    const auto none = ExciterProcessor::analyse (targets);

    targets.harmonics = 1.0;
    const auto full = ExciterProcessor::analyse (targets);

    INFO ("THD " << none.thdPercent << " % at 0, " << full.thdPercent << " % at 100 %; gain " << full.gainDb << " dB");
    CHECK (none.thdPercent < 0.05f);
    CHECK (full.thdPercent > 1.0f);
    CHECK (full.gainDb > 0.0f);
    CHECK (std::abs (none.gainDb) < 0.05f);
}

TEST_CASE ("Exciter's new settings are off by default", "[exciter][w1]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    ExciterProcessor processor;
    CHECK (processor.getApvts().getParameter ("ceiling")->getCurrentValueAsText() == "Off");
    CHECK (processor.getApvts().getRawParameterValue ("autoGain")->load() == 0.0f);
}
