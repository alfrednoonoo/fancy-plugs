// Phase 4 Wave 2 for Fancy Gate, v1.7.1: the Transient detector (item 11) and Upward mode (item 12),
// docs/gate.md, sections 5 of "Transient detector mode" and "Upward mode". Every number here was run
// first through docs/research/gate-w2b-prepush.py, which models GateDsp as built.
//
// Legacy (Level, Gate and Duck render as v1.6.0) is GateW1Tests' first case, which runs the current
// GateDsp against LegacyGateDsp at these items' defaults; the first case below adds that the new
// settings change nothing until their mode is chosen.

#include "GateDsp.h"
#include "GateProcessor.h"

#if FANCY_HAS_COMP
 #include "CompProcessor.h"
#endif

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = 3.14159265358979323846;

int samples (double seconds)
{
    return (int) (seconds * rate);
}

// The same signal in both channels, through the gate; returns the left channel.
std::vector<float> run (const GateDsp::Targets& targets, const std::vector<float>& input, int blockSize = 333, juce::MidiBuffer* allMidi = nullptr)
{
    GateDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    std::vector<float> left (input), right (input);
    const auto size = (int) input.size();

    for (int start = 0; start < size; start += blockSize)
    {
        const auto count = std::min (blockSize, size - start);
        float* channels[] { left.data() + start, right.data() + start };
        juce::MidiBuffer midi;
        dsp.process (channels, 2, nullptr, 0, count, &midi);

        if (allMidi != nullptr)
            allMidi->addEvents (midi, 0, count, start);
    }

    return left;
}

GateDsp::Targets transient (double thresholdDb = -40.0)
{
    GateDsp::Targets targets;
    targets.transient = true;
    targets.riseDb = 8.0;
    targets.thresholdDb = thresholdDb;
    targets.attackMs = 0.5;
    targets.holdMs = 150.0;
    targets.releaseMs = 50.0;
    targets.rangeDb = -200.0;   // -inf: closed is silence
    return targets;
}

bool isOpen (float sample)
{
    return sample != 0.0f;
}

// The design note's song: a tom mic with bass bleed that's louder in the chorus (as the model's).
std::vector<float> song (std::vector<int>& hits, unsigned seed)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<double> uniform (-1.7, 1.7);
    const auto n = samples (6.0);
    std::vector<double> x ((size_t) n, 0.0);

    const double verse[] { -12.0, -18.0, -24.0 }, chorus[] { -6.0, -9.0 };

    for (int k = 0; k < 12; ++k)
    {
        const auto start = samples (0.2 + 0.5 * k);
        hits.push_back (start);
        const auto db = start < samples (3.0) ? verse[k % 3] : chorus[k % 2];
        const auto length = samples (0.4);

        std::vector<double> tom ((size_t) length);
        auto peak = 0.0;

        for (int m = 0; m < length; ++m)
        {
            const auto body = std::sin (2.0 * pi * 110.0 * m / rate) * std::exp (-m / (0.15 * rate));
            const auto click = m < 0.003 * rate ? uniform (random) * std::exp (-m / (0.001 * rate)) : 0.0;
            tom[(size_t) m] = body + 0.8 * click;
            peak = std::max (peak, std::abs (tom[(size_t) m]));
        }

        for (int m = 0; m < length && start + m < n; ++m)
            x[(size_t) (start + m)] += std::pow (10.0, db / 20.0) * tom[(size_t) m] / peak;
    }

    const double notes[] { 55.0, 65.4, 73.4, 82.4 };

    for (int k = 0; k < 15; ++k)
    {
        const auto start = samples (k * 0.4);
        const auto length = std::min (samples (0.4), n - start);
        const auto peak = std::pow (10.0, (start < samples (3.0) ? -40.0 : -20.0) / 20.0);

        std::vector<double> note ((size_t) length);
        auto largest = 0.0;

        for (int m = 0; m < length; ++m)
        {
            note[(size_t) m] = std::sin (2.0 * pi * notes[k % 4] * m / rate) * (1.0 - std::exp (-m / (0.03 * rate))) * std::exp (-m / (0.8 * rate));
            largest = std::max (largest, std::abs (note[(size_t) m]));
        }

        for (int m = 0; m < length; ++m)
            x[(size_t) (start + m)] += peak * note[(size_t) m] / largest;
    }

    std::vector<float> result ((size_t) n);

    for (int i = 0; i < n; ++i)
        result[(size_t) i] = (float) (x[(size_t) i] + 0.001 * uniform (random));

    return result;
}

// Bass notes back to back, 400 ms each, swelling in over 30 ms, at -6 dBFS peak.
std::vector<float> bassNotes (double hz, int count)
{
    const auto length = samples (0.4);
    std::vector<float> x ((size_t) (length * count));
    std::vector<double> note ((size_t) length);
    auto largest = 0.0;

    for (int m = 0; m < length; ++m)
    {
        note[(size_t) m] = std::sin (2.0 * pi * hz * m / rate) * (1.0 - std::exp (-m / (0.03 * rate))) * std::exp (-m / (0.8 * rate));
        largest = std::max (largest, std::abs (note[(size_t) m]));
    }

    for (int k = 0; k < count; ++k)
        for (int m = 0; m < length; ++m)
            x[(size_t) (k * length + m)] = (float) (0.5 * note[(size_t) m] / largest);

    return x;
}

std::vector<float> sine (double hz, double amplitude, int numSamples, int from = 0, int to = -1)
{
    std::vector<float> x ((size_t) numSamples, 0.0f);

    for (int i = from; i < (to < 0 ? numSamples : to); ++i)
        x[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * hz * i / rate));

    return x;
}

double gainDb (const std::vector<float>& output, const std::vector<float>& input, int from, int to)
{
    double out = 0.0, in = 0.0;

    for (int i = from; i < to; ++i)
    {
        out += (double) output[(size_t) i] * output[(size_t) i];
        in += (double) input[(size_t) i] * input[(size_t) i];
    }

    return 10.0 * std::log10 (std::max (out, 1.0e-30) / std::max (in, 1.0e-30));
}

int noteOns (const juce::MidiBuffer& midi)
{
    auto count = 0;

    for (const auto metadata : midi)
        count += metadata.getMessage().isNoteOn() ? 1 : 0;

    return count;
}
} // namespace

// 1. Compatibility -------------------------------------------------------------

TEST_CASE ("Gate's Rise, Boost and Up ratio change nothing in Level, Gate and Duck", "[gate][w2b]")
{
    const auto variant = GENERATE (0, 1, 2, 3);

    GateDsp::Targets plain;
    plain.thresholdDb = -30.0;
    plain.rangeDb = -30.0;
    plain.holdMs = 40.0;
    plain.rmsMs = 10.0;

    switch (variant)
    {
        case 1: plain.duck = true; break;
        case 2: plain.lookahead = true; plain.preOpenMs = 3.0; break;
        case 3: plain.ratio = 2.0; plain.link = false; break;
        default: break;
    }

    auto changed = plain;
    changed.riseDb = 15.0;
    changed.boostDb = 9.0;
    changed.upRatio = 3.0;

    std::vector<int> hits;
    const auto input = song (hits, 3);

    INFO ("variant " << variant);
    REQUIRE (run (plain, input) == run (changed, input));
}

TEST_CASE ("Gate's Mode and Detector mode choices map to the DSP", "[gate][w2b]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    GateProcessor processor;
    const auto set = [&processor] (const char* id, float value) { processor.setParameter (id, value); };

    auto targets = processor.readTargets();
    CHECK_FALSE (targets.duck);
    CHECK_FALSE (targets.upward);
    CHECK_FALSE (targets.transient);
    CHECK (targets.riseDb == Catch::Approx (8.0));
    CHECK (targets.boostDb == Catch::Approx (4.0));
    CHECK (targets.upRatio == Catch::Approx (1.5));
    CHECK (processor.getApvts().getParameter ("ceiling")->getCurrentValueAsText() == "Off");

    set ("mode", 1.0f);
    targets = processor.readTargets();
    CHECK (targets.duck);
    CHECK_FALSE (targets.upward);

    set ("mode", 2.0f);
    set ("detMode", 1.0f);
    targets = processor.readTargets();
    CHECK_FALSE (targets.duck);
    CHECK (targets.upward);
    CHECK (targets.transient);
}

// 2. Transient: the model's song ------------------------------------------------

TEST_CASE ("Gate Transient catches every hit and stays shut on the bass", "[gate][w2b]")
{
    const auto seed = GENERATE (1u, 2u);

    std::vector<int> hits;
    const auto input = song (hits, seed);

    auto targets = transient();
    targets.lowpassHz = 1000.0;

    auto level = targets;
    level.transient = false;
    level.thresholdDb = -30.0;

    const auto score = [&] (const std::vector<float>& output, int& caught)
    {
        std::vector<bool> near (output.size(), false);
        caught = 0;

        for (const auto hit : hits)
        {
            auto found = false;

            for (int i = hit; i < hit + samples (0.005); ++i)
                found = found || isOpen (output[(size_t) i]);

            caught += found ? 1 : 0;

            for (int i = std::max (0, hit - samples (0.005)); i < std::min ((int) output.size(), hit + samples (0.25)); ++i)
                near[(size_t) i] = true;
        }

        auto strayMs = 0.0;

        for (size_t i = 0; i < output.size(); ++i)
            strayMs += isOpen (output[i]) && ! near[i] ? 1000.0 / rate : 0.0;

        return strayMs;
    };

    int caught = 0, levelCaught = 0;
    const auto strayMs = score (run (targets, input), caught);
    const auto levelStrayMs = score (run (level, input), levelCaught);

    INFO ("seed " << seed << ": Transient caught " << caught << ", open " << strayMs << " ms on the bass; Level open " << levelStrayMs << " ms");
    CHECK (caught == 12);
    CHECK (strayMs < 50.0);
    CHECK (levelStrayMs > 1000.0);
}

// 3. A held sound can't keep it open --------------------------------------------

TEST_CASE ("Gate Transient opens once on a held tone and closes after Hold and Release", "[gate][w2b]")
{
    const auto n = samples (1.3);
    const auto input = sine (1000.0, 0.5, n, samples (0.1), samples (1.1));

    auto targets = transient();
    targets.holdMs = 50.0;
    targets.releaseMs = 50.0;
    targets.midi = true;

    juce::MidiBuffer midi;
    const auto output = run (targets, input, 256, &midi);

    auto level = targets;
    level.transient = false;
    const auto levelOutput = run (level, input);

    auto transientOpenLate = false;

    for (int i = samples (0.3); i < samples (1.05); ++i)
        transientOpenLate = transientOpenLate || isOpen (output[(size_t) i]);

    CHECK (noteOns (midi) == 1);
    CHECK_FALSE (transientOpenLate);

    // Level stays open on it throughout.
    CHECK (gainDb (levelOutput, input, samples (0.3), samples (1.05)) == Catch::Approx (0.0).margin (0.01));
}

// 4. Slow swells don't count, clicks do -----------------------------------------

TEST_CASE ("Gate Transient ignores bass notes swelling in and opens on a click", "[gate][w2b]")
{
    const auto hz = GENERATE (55.0, 82.4, 110.0);

    auto targets = transient();
    targets.holdMs = 20.0;
    targets.releaseMs = 20.0;

    // After the first note, which rises from silence, none opens it.
    const auto output = run (targets, bassNotes (hz, 6));
    auto opened = false;

    for (int i = samples (0.4) + samples (0.045); i < (int) output.size(); ++i)
        opened = opened || isOpen (output[(size_t) i]);

    INFO (hz << " Hz");
    CHECK_FALSE (opened);

    // A 1 ms click 10 dB over Threshold (-40 dB) opens it within a millisecond.
    std::vector<float> click ((size_t) samples (0.3), 0.0f);
    const auto at = samples (0.1);

    for (int i = 0; i < 48; ++i)
        click[(size_t) (at + i)] = (float) (std::pow (10.0, -30.0 / 20.0) * (i % 2 == 0 ? 1.0 : -1.0));

    const auto clicked = run (targets, click);
    auto first = -1;

    for (int i = 0; i < (int) clicked.size() && first < 0; ++i)
        if (isOpen (clicked[(size_t) i]))
            first = i;

    CHECK (first >= at);
    CHECK (first < at + samples (0.001));
}

// 5. Lookahead, Duck and MIDI out work as in Level ------------------------------

TEST_CASE ("Gate Transient sends one note per hit, ducks in Duck mode and opens Pre-open ahead with Lookahead", "[gate][w2b]")
{
    // Four decaying hits, 250 ms apart.
    const auto n = samples (1.0);
    std::vector<float> hits ((size_t) n, 0.0f);

    for (int k = 0; k < 4; ++k)
    {
        const auto start = samples (0.05 + 0.25 * k);

        for (int m = 0; m < samples (0.2); ++m)
            hits[(size_t) (start + m)] = (float) (0.5 * std::sin (2.0 * pi * 180.0 * m / rate) * std::exp (-m / (0.03 * rate)));
    }

    auto targets = transient();
    targets.holdMs = 20.0;
    targets.releaseMs = 20.0;
    targets.midi = true;

    juce::MidiBuffer midi;
    run (targets, hits, 128, &midi);
    CHECK (noteOns (midi) == 4);

    // Duck: down by Range just after the onset of a held tone, untouched once it has closed.
    const auto tone = sine (1000.0, 0.5, samples (0.8), samples (0.1));
    auto duck = transient();
    duck.duck = true;
    duck.rangeDb = -12.0;
    duck.holdMs = 50.0;
    duck.releaseMs = 50.0;

    const auto ducked = run (duck, tone);
    CHECK (gainDb (ducked, tone, samples (0.11), samples (0.14)) == Catch::Approx (-12.0).margin (0.1));
    CHECK (gainDb (ducked, tone, samples (0.4), samples (0.8)) == Catch::Approx (0.0).margin (0.01));

    // Lookahead: the gate is fully open when the delayed click arrives.
    auto ahead = transient();
    ahead.lookahead = true;
    ahead.preOpenMs = 5.0;

    std::vector<float> click ((size_t) samples (0.3), 0.0f);
    const auto at = samples (0.1);

    for (int i = 0; i < 48; ++i)
        click[(size_t) (at + i)] = (float) (0.1 * (i % 2 == 0 ? 1.0 : -1.0));

    const auto output = run (ahead, click);
    const auto delay = samples (0.05);

    for (int i = 0; i < 48; ++i)
        REQUIRE (output[(size_t) (at + delay + i)] == Catch::Approx (click[(size_t) (at + i)]).margin (1.0e-6));
}

// Upward 2 to 4 ---------------------------------------------------------------------

TEST_CASE ("Gate Upward leaves everything under Threshold untouched", "[gate][w2b]")
{
    GateDsp::Targets targets;
    targets.upward = true;
    targets.thresholdDb = -20.0;
    targets.rmsMs = 50.0;
    targets.attackMs = 10.0;
    targets.releaseMs = 200.0;

    // A tone at -30 dBFS RMS, 10 dB under Threshold.
    const auto input = sine (1000.0, std::sqrt (2.0) * std::pow (10.0, -30.0 / 20.0), samples (1.0));
    REQUIRE (run (targets, input) == input);
}

TEST_CASE ("Gate Upward raises a tone over Threshold by (level - Threshold) x (Up ratio - 1), up to Boost", "[gate][w2b]")
{
    const auto boost = GENERATE (12.0, 4.0);

    GateDsp::Targets targets;
    targets.upward = true;
    targets.thresholdDb = -30.0;
    targets.rmsMs = 50.0;
    targets.attackMs = 10.0;
    targets.releaseMs = 200.0;
    targets.upRatio = 1.5;
    targets.boostDb = boost;

    // RMS -20 dBFS, 10 dB over Threshold: 5 dB up, or Boost if that's less.
    const auto input = sine (1000.0, std::sqrt (2.0) * std::pow (10.0, -20.0 / 20.0), samples (1.0));
    const auto output = run (targets, input);

    INFO ("Boost " << boost);
    CHECK (gainDb (output, input, samples (0.6), samples (1.0)) == Catch::Approx (std::min (5.0, boost)).margin (0.1));
}

TEST_CASE ("Gate Upward lifts a loud chorus and leaves the verse alone", "[gate][w2b]")
{
    // A bed of noise at -24 dBFS RMS, then -16: Threshold -20, Up ratio 1.5, Boost 6 lifts the
    // chorus by (-16 - -20) x 0.5 = 2 dB.
    std::mt19937 random (9);
    std::uniform_real_distribution<double> uniform (-1.7, 1.7);
    const auto n = samples (8.0), half = n / 2;
    std::vector<float> input ((size_t) n);

    for (int i = 0; i < n; ++i)
        input[(size_t) i] = (float) (uniform (random) * std::pow (10.0, (i < half ? -24.0 : -16.0) / 20.0));

    GateDsp::Targets targets;
    targets.upward = true;
    targets.thresholdDb = -20.0;
    targets.rmsMs = 50.0;
    targets.attackMs = 10.0;
    targets.releaseMs = 200.0;
    targets.holdMs = 0.0;
    targets.upRatio = 1.5;
    targets.boostDb = 6.0;

    const auto output = run (targets, input);

    CHECK (std::abs (gainDb (output, input, samples (1.0), half)) < 0.05);
    CHECK (gainDb (output, input, half + samples (1.0), n) == Catch::Approx (2.0).margin (0.25));
}

// Upward 5: the ceiling ---------------------------------------------------------------

TEST_CASE ("Gate's Ceiling holds Upward's boost under it and reports its latency", "[gate][w2b]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    constexpr int block = 256;

    const auto render = [&] (GateProcessor& processor, float ceilingDb, int& latency)
    {
        processor.setParameter ("mode", 2.0f);
        processor.setParameter ("boost", 12.0f);
        processor.setParameter ("upRatio", 4.0f);
        processor.setParameter ("threshold", -40.0f);
        processor.setParameter ("ceiling", ceilingDb);

        processor.setRateAndBufferSizeDetails (rate, block);
        processor.prepareToPlay (rate, block);

        juce::AudioBuffer<float> buffer (4, block);
        juce::MidiBuffer midi;
        std::vector<float> output;

        for (int start = 0; start < samples (1.0); start += block)
        {
            buffer.clear();

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                    buffer.setSample (ch, i, (float) (0.98 * std::sin (2.0 * pi * 997.0 * (start + i) / rate)));

            midi.clear();
            processor.processBlock (buffer, midi);

            for (int i = 0; i < block; ++i)
                output.push_back (buffer.getSample (0, i));
        }

        latency = processor.getLatencySamples();
        processor.releaseResources();
        return output;
    };

    GateProcessor off, on;
    int offLatency = 0, onLatency = 0;

    const auto loud = render (off, -12.5f, offLatency);
    const auto held = render (on, -1.0f, onLatency);

    auto loudPeak = 0.0f, heldPeak = 0.0f;

    for (int i = samples (0.5); i < samples (1.0); ++i)
    {
        loudPeak = std::max (loudPeak, std::abs (loud[(size_t) i]));
        heldPeak = std::max (heldPeak, std::abs (held[(size_t) i]));
    }

    CHECK (offLatency == 0);
    CHECK (onLatency > 0);
    CHECK (20.0 * std::log10 (loudPeak) > 6.0);    // the boost passes 0 dBFS with Ceiling Off
    CHECK (20.0 * std::log10 (heldPeak) < -0.9);   // and is held under -1 dBTP's sample peak with it on
}

// Transient's CPU, against Level's ----------------------------------------------------

TEST_CASE ("Gate Transient CPU measurement", "[.measure]")
{
    constexpr double seconds = 10.0;
    constexpr int measureBlock = 128;

    const auto cpu = [] (fancy::FancyProcessor& processor)
    {
        std::vector<double> runs;

        for (int pass = 0; pass < 5; ++pass)
        {
            processor.setRateAndBufferSizeDetails (rate, measureBlock);
            processor.prepareToPlay (rate, measureBlock);
            processor.waitForBackgroundWork (20000);

            juce::AudioBuffer<float> block (juce::jmax (2, processor.getTotalNumInputChannels()), measureBlock);
            juce::MidiBuffer midi;
            std::mt19937 random (8);
            std::uniform_real_distribution<float> uniform (-0.25f, 0.25f);
            std::chrono::duration<double> spent {};

            for (int done = 0; done < (int) (seconds * rate); done += measureBlock)
            {
                block.clear();

                for (int ch = 0; ch < processor.getMainBusNumInputChannels(); ++ch)
                    for (int i = 0; i < measureBlock; ++i)
                        block.setSample (ch, i, uniform (random));

                midi.clear();
                const auto start = std::chrono::steady_clock::now();
                processor.processBlock (block, midi);
                spent += std::chrono::steady_clock::now() - start;
            }

            processor.releaseResources();
            runs.push_back (100.0 * spent.count() / seconds);
        }

        std::sort (runs.begin(), runs.end());
        return runs[2];
    };

    double reference = 0.0;

   #if FANCY_HAS_COMP
    {
        CompProcessor comp;
        reference = cpu (comp);
    }
   #endif

    GateProcessor level;
    const auto one = cpu (level);

    GateProcessor transientGate;
    transientGate.setParameter ("detMode", 1.0f);
    const auto two = cpu (transientGate);

    GateProcessor upwardGate;
    upwardGate.setParameter ("mode", 2.0f);
    const auto three = cpu (upwardGate);

    const auto ratio = [reference] (double value) { return reference > 0.0 ? value / reference : 0.0; };

    std::printf ("gate v1.7.1 cpu (median of 5, 48 kHz, 128 samples, stereo): Fancy Comp default %.3f %%; Gate default %.3f %% (%.2f x Comp); "
                 "Transient %.3f %% (%.2f x Comp, %.0f %% more than Level, target < 10 %%); Upward %.3f %% (%.2f x Comp)\n",
                 reference, one, ratio (one), two, ratio (two), one > 0.0 ? 100.0 * (two / one - 1.0) : 0.0, three, ratio (three));
}
