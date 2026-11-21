// Phase 4 Wave 1 for Fancy DS: filter slope and output ceiling (docs/ds.md, "Phase 4 Wave 1",
// section 5). Learn is the editor's, checked in REAPER.

#include "DsProcessor.h"
#include "LegacyDeEsser.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <fancy/dsp/DeEsser.h>

#include <cmath>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = 3.14159265358979323846;

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples), right ((size_t) numSamples) {}
    int size() const { return (int) left.size(); }
};

template <typename Engine, typename Targets>
Stereo run (const Targets& targets, Stereo signal, int blockSize = 333)
{
    Engine engine;
    engine.prepare (rate);
    engine.setSynchronousDesign (true);
    engine.snapToTargets (targets);

    for (int start = 0; start < signal.size(); start += blockSize)
    {
        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        const float* sidechain[] { nullptr, nullptr };
        engine.process (channels, 2, sidechain, 0, std::min (blockSize, signal.size() - start));
    }

    return signal;
}

// A voice-like signal with "s" bursts.
Stereo voice (int numSamples, unsigned seed)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<double> noise (-1.0, 1.0);
    Stereo signal (numSamples);
    double hiss[2] {};

    for (int i = 0; i < numSamples; ++i)
    {
        const auto burst = std::fmod ((double) i / rate, 0.3) < 0.08 ? 1.0 : 0.05;
        const auto body = 0.3 * std::sin (2.0 * pi * 220.0 * i / rate);

        for (int ch = 0; ch < 2; ++ch)
            hiss[ch] = noise (random) - 0.6 * hiss[ch];

        signal.left[(size_t) i] = (float) (body + 0.25 * burst * hiss[0]);
        signal.right[(size_t) i] = (float) (0.9 * body + 0.2 * burst * hiss[1]);
    }

    return signal;
}

Stereo sine (double hz, double amplitude, int numSamples)
{
    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * hz * i / rate));

    return signal;
}

double rmsDb (const std::vector<float>& samples, int from)
{
    double sum = 0.0;

    for (size_t i = (size_t) from; i < samples.size(); ++i)
        sum += (double) samples[i] * (double) samples[i];

    return 10.0 * std::log10 (std::max (sum / (double) (samples.size() - (size_t) from), 1.0e-30));
}
} // namespace

// 1. Compatibility ------------------------------------------------------------

TEST_CASE ("DS Wave 1 at its defaults renders exactly as the v1.6.0 engine", "[ds][w1]")
{
    const auto variant = GENERATE (0, 1, 2, 3);

    fancy::dsp::LegacyDeEsser::Targets old;
    old.thresholdDb = -30.0;

    switch (variant)
    {
        case 1: old.shelf = true; old.freqHz = 7000.0; break;
        case 2: old.wide = true; old.relative = false; break;
        case 3: old.lookahead = fancy::dsp::LegacyDeEsser::lookahead2; old.link = false; break;
        default: break;
    }

    fancy::dsp::DeEsser::Targets now;
    now.thresholdDb = old.thresholdDb;
    now.freqHz = old.freqHz;
    now.widthOctaves = old.widthOctaves;
    now.shelf = old.shelf;
    now.rangeDb = old.rangeDb;
    now.attackMs = old.attackMs;
    now.releaseMs = old.releaseMs;
    now.detector = old.detector;
    now.wide = old.wide;
    now.relative = old.relative;
    now.listen = old.listen;
    now.link = old.link;
    now.lookahead = old.lookahead;
    now.linearPhase = old.linearPhase;
    now.outputGain = old.outputGain;
    now.bypass = old.bypass;

    const auto input = voice (24000, 2);
    const auto a = run<fancy::dsp::LegacyDeEsser> (old, input);
    const auto b = run<fancy::dsp::DeEsser> (now, input);

    INFO ("variant " << variant);
    REQUIRE (a.left == b.left);
    REQUIRE (a.right == b.right);
}

// 3. Slope --------------------------------------------------------------------

TEST_CASE ("A steeper slope ignores a tone two octaves from the band", "[ds][w1]")
{
    // Wide mode, so whatever the detector hears turns the whole signal down.
    fancy::dsp::DeEsser::Targets targets;
    targets.freqHz = 3000.0;
    targets.thresholdDb = -30.0;
    targets.relative = false;
    targets.wide = true;
    targets.rangeDb = -12.0;

    const auto input = sine (12000.0, std::pow (10.0, -10.0 / 20.0), (int) rate);
    const auto inputDb = rmsDb (input.left, (int) rate / 2);

    targets.slopeStages = 1;
    const auto gentle = rmsDb (run<fancy::dsp::DeEsser> (targets, input).left, (int) rate / 2) - inputDb;

    targets.slopeStages = 4;
    const auto steep = rmsDb (run<fancy::dsp::DeEsser> (targets, input).left, (int) rate / 2) - inputDb;

    INFO ("12 dB/oct: " << gentle << " dB, 48 dB/oct: " << steep << " dB");
    CHECK (gentle < -2.0);
    CHECK (steep == Catch::Approx (0.0).margin (0.1));
}

// 4. Ceiling ------------------------------------------------------------------

TEST_CASE ("The ceiling holds the output under it and is out of the path while Off", "[ds][w1]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    DsProcessor processor;
    auto set = [&] (const char* id, float value)
    {
        auto* parameter = processor.getApvts().getParameter (id);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    CHECK (processor.getApvts().getParameter ("ceiling")->getCurrentValueAsText() == "Off");

    processor.prepareToPlay (rate, 512);
    const auto latencyOff = processor.getLatencySamples();

    set ("output", 6.0f);
    set ("ceiling", -1.0f);

    juce::AudioBuffer<float> buffer (4, 512);
    juce::MidiBuffer midi;
    auto peak = 0.0f;

    for (int block = 0; block < 100; ++block)
    {
        buffer.clear();

        for (int i = 0; i < 512; ++i)
        {
            const auto x = ((block * 512 + i) / 40) % 2 == 0 ? 0.99f : -0.99f;   // a full-scale square
            buffer.setSample (0, i, x);
            buffer.setSample (1, i, x);
        }

        processor.processBlock (buffer, midi);

        if (block > 10)
            peak = std::max ({ peak, buffer.getMagnitude (0, 0, 512), buffer.getMagnitude (1, 0, 512) });
    }

    CHECK (peak <= std::pow (10.0f, -1.0f / 20.0f) + 1.0e-4f);
    CHECK (processor.getLatencySamples() > latencyOff);
}
