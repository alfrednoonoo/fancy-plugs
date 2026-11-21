// Phase 4 Wave 1 for Fancy Gate: MIDI trigger, Ratio, Mid/Side sources and De-bleed
// (docs/gate.md, "Phase 4 Wave 1", section 8). Learn is the editor's, checked in REAPER.

#include "GateDsp.h"
#include "GateProcessor.h"
#include "LegacyGateDsp.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

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

template <typename Dsp, typename Targets>
Stereo run (const Targets& targets, Stereo signal, int blockSize = 333)
{
    Dsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    for (int start = 0; start < signal.size(); start += blockSize)
    {
        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, nullptr, 0, std::min (blockSize, signal.size() - start), nullptr);
    }

    return signal;
}

// Hits over a noise floor, different in each channel.
Stereo hits (int numSamples, unsigned seed)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<double> noise (-1.0, 1.0);
    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto phase = std::fmod ((double) i / rate, 0.25) / 0.25;
        const auto envelope = phase < 0.4 ? std::exp (-phase * 12.0) : 0.0;
        const auto tone = std::sin (2.0 * pi * 180.0 * i / rate);

        signal.left[(size_t) i] = (float) (0.8 * envelope * tone + 0.002 * noise (random));
        signal.right[(size_t) i] = (float) (0.6 * envelope * tone + 0.003 * noise (random));
    }

    return signal;
}

Stereo sine (double hz, double rmsDb, int numSamples, bool sideOnly = false)
{
    Stereo signal (numSamples);
    const auto amplitude = std::sqrt (2.0) * std::pow (10.0, rmsDb / 20.0);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto x = (float) (amplitude * std::sin (2.0 * pi * hz * i / rate));
        signal.left[(size_t) i] = x;
        signal.right[(size_t) i] = sideOnly ? -x : x;
    }

    return signal;
}

double rmsDb (const std::vector<float>& samples, int from, int to)
{
    double sum = 0.0;

    for (int i = from; i < to; ++i)
        sum += (double) samples[(size_t) i] * (double) samples[(size_t) i];

    return 10.0 * std::log10 (std::max (sum / (double) (to - from), 1.0e-30));
}

GateDsp::Targets gate (double thresholdDb)
{
    GateDsp::Targets targets;
    targets.thresholdDb = thresholdDb;
    targets.attackMs = 0.5;
    targets.holdMs = 20.0;
    targets.releaseMs = 50.0;
    targets.rmsMs = 10.0;
    return targets;
}
} // namespace

// 1. Compatibility ------------------------------------------------------------

TEST_CASE ("Gate Wave 1 at its defaults renders exactly as Gate did in v1.6.0", "[gate][w1]")
{
    const auto variant = GENERATE (0, 1, 2, 3);

    LegacyGateDsp::Targets old;
    old.thresholdDb = -30.0;
    old.rangeDb = -30.0;
    old.holdMs = 40.0;
    old.releaseMs = 80.0;
    old.highpassHz = 90.0;

    switch (variant)
    {
        case 1: old.duck = true; break;
        case 2: old.lookahead = true; old.preOpenMs = 3.0; break;
        case 3: old.link = false; old.detector = LegacyGateDsp::mainR; old.noiseGain = 0.05; break;
        default: break;
    }

    GateDsp::Targets now;
    now.thresholdDb = old.thresholdDb;
    now.hysteresisDb = old.hysteresisDb;
    now.preOpenMs = old.preOpenMs;
    now.attackMs = old.attackMs;
    now.holdMs = old.holdMs;
    now.releaseMs = old.releaseMs;
    now.rangeDb = old.rangeDb;
    now.rmsMs = old.rmsMs;
    now.lowpassHz = old.lowpassHz;
    now.highpassHz = old.highpassHz;
    now.detector = old.detector;
    now.lookahead = old.lookahead;
    now.preview = old.preview;
    now.duck = old.duck;
    now.link = old.link;
    now.wetGain = old.wetGain;
    now.dryGain = old.dryGain;
    now.noiseGain = old.noiseGain;
    now.outputGain = old.outputGain;
    now.bypass = old.bypass;

    const auto input = hits (24000, 3);
    const auto a = run<LegacyGateDsp> (old, input);
    const auto b = run<GateDsp> (now, input);

    INFO ("variant " << variant);
    REQUIRE (a.left == b.left);
    REQUIRE (a.right == b.right);
}

// 2. MIDI trigger --------------------------------------------------------------

TEST_CASE ("The MIDI trigger opens on note-on and closes after note-off", "[gate][w1]")
{
    auto targets = gate (0.0);   // the level alone never opens it
    targets.midiIn = true;

    GateDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    auto signal = sine (500.0, -20.0, (int) rate);
    constexpr int blockSize = 480;

    for (int start = 0; start < signal.size(); start += blockSize)
    {
        std::vector<GateDsp::IncomingNote> notes;

        if (start <= 9600 && 9600 < start + blockSize)
            notes.push_back ({ 9600 - start, true });

        if (start <= 28800 && 28800 < start + blockSize)
            notes.push_back ({ 28800 - start, false });

        dsp.setIncomingNotes (notes.data(), (int) notes.size());

        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, nullptr, 0, blockSize, nullptr);
    }

    CHECK (rmsDb (signal.left, 0, 9000) < -100.0);                       // shut before the note
    CHECK (rmsDb (signal.left, 12000, 28000) == Catch::Approx (-20.0).margin (0.5));   // open while held
    CHECK (rmsDb (signal.left, 28800 + 4800, 48000) < -100.0);           // shut after hold + release
}

// 3. Ratio ------------------------------------------------------------------

TEST_CASE ("Ratio below inf makes Gate an expander", "[gate][w1]")
{
    auto targets = gate (-30.0);
    targets.ratio = 2.0;

    // 10 dB under Threshold at 2:1: 10 dB further down.
    const auto quiet = run<GateDsp> (targets, sine (500.0, -40.0, (int) rate));
    CHECK (rmsDb (quiet.left, 24000, 48000) == Catch::Approx (-50.0).margin (0.5));

    // Over Threshold: untouched.
    const auto loud = run<GateDsp> (targets, sine (500.0, -20.0, (int) rate));
    CHECK (rmsDb (loud.left, 24000, 48000) == Catch::Approx (-20.0).margin (0.1));

    // Never deeper than Range.
    targets.rangeDb = -6.0;
    const auto limited = run<GateDsp> (targets, sine (500.0, -60.0, (int) rate));
    CHECK (rmsDb (limited.left, 24000, 48000) == Catch::Approx (-66.0).margin (0.5));
}

// 5. Mid and Side ------------------------------------------------------------

TEST_CASE ("Mid and Side detector sources hear the sum and the difference", "[gate][w1]")
{
    auto targets = gate (-40.0);
    const auto input = sine (500.0, -20.0, (int) rate, true);   // side only: L = -R

    targets.detector = GateDsp::mid;
    CHECK (rmsDb (run<GateDsp> (targets, input).left, 24000, 48000) < -100.0);

    targets.detector = GateDsp::side;
    CHECK (rmsDb (run<GateDsp> (targets, input).left, 24000, 48000) == Catch::Approx (-20.0).margin (0.5));
}

TEST_CASE ("The Detector input choice keeps its indices and adds Mid and Side", "[gate][w1]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    GateProcessor processor;
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.getApvts().getParameter ("detector"));
    REQUIRE (choice != nullptr);
    CHECK (choice->choices.size() == 8);
    CHECK (choice->choices[6] == "Mid");
    CHECK (choice->choices[7] == "Side");
    CHECK (processor.acceptsMidi());
}

// 6. De-bleed ----------------------------------------------------------------

TEST_CASE ("De-bleed keeps loud sound outside the band from opening the gate", "[gate][w1]")
{
    auto targets = gate (-40.0);
    targets.highpassHz = 200.0;

    // A 60 Hz thump: through the high-pass it is still over Threshold, but far louder outside.
    const auto thump = sine (60.0, -10.0, (int) rate);
    CHECK (rmsDb (run<GateDsp> (targets, thump).left, 24000, 48000) > -12.0);   // opens without De-bleed

    targets.deBleed = 1.0;
    CHECK (rmsDb (run<GateDsp> (targets, thump).left, 24000, 48000) < -100.0);

    // In the band, it still opens.
    const auto snare = sine (1000.0, -20.0, (int) rate);
    CHECK (rmsDb (run<GateDsp> (targets, snare).left, 24000, 48000) == Catch::Approx (-20.0).margin (0.5));
}
