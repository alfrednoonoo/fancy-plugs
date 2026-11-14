// Phase 3 Q2.4: EQ Match. Design: "Phase 3 Q2.4" in docs/eq.md; the fit was tried first in
// docs/research/eq-match-model.py.

#include "EqDsp.h"
#include "EqMatch.h"
#include "EqProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <thread>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr int hop = 2048; // the learner's, at 48 kHz

using eq::BandSpec;
using eq::BandType;
using eq::match::Curve;

BandSpec makeBand (BandType type, double hz, double gainDb, double q = eq::butterworthQ)
{
    BandSpec spec;
    spec.type = type;
    spec.frequencyHz = hz;
    spec.gainDb = gainDb;
    spec.q = q;
    spec.order = 2;
    spec.on = true;
    return spec;
}

// Pink noise, from Paul Kellet's filter. The same seed gives the same noise.
std::vector<float> pinkNoise (int numSamples, unsigned seed, float level = 0.05f)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<float> uniform (-1.0f, 1.0f);

    std::vector<float> samples ((size_t) numSamples);
    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f, b3 = 0.0f, b4 = 0.0f, b5 = 0.0f, b6 = 0.0f;

    for (auto& sample : samples)
    {
        const auto white = uniform (random);

        b0 = 0.99886f * b0 + white * 0.0555179f;
        b1 = 0.99332f * b1 + white * 0.0750759f;
        b2 = 0.96900f * b2 + white * 0.1538520f;
        b3 = 0.86650f * b3 + white * 0.3104856f;
        b4 = 0.55000f * b4 + white * 0.5329522f;
        b5 = -0.7616f * b5 - white * 0.0168980f;

        sample = level * (b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f);
        b6 = white * 0.115926f;
    }

    return samples;
}

Curve learn (const std::vector<float>& samples)
{
    eq::match::Learner learner;
    learner.prepare (rate);
    learner.addSamples (samples.data(), (int) samples.size());
    return learner.getSpectrumDb();
}

// A mono signal through the EQ, in Minimum phase.
std::vector<float> throughEq (std::vector<float> samples, const std::vector<BandSpec>& bands)
{
    EqDsp::Targets targets;

    for (size_t b = 0; b < bands.size(); ++b)
        targets.bands[b] = bands[b];

    EqDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    auto right = samples;

    for (int start = 0; start < (int) samples.size(); start += 512)
    {
        float* channels[] { samples.data() + start, right.data() + start };
        dsp.process (channels, 2, std::min (512, (int) samples.size() - start));
    }

    return samples;
}

const std::vector<BandSpec>& knownEq()
{
    static const std::vector<BandSpec> bands {
        makeBand (BandType::lowShelf, 120.0, 4.0),
        makeBand (BandType::band, 300.0, -3.0, 1.4),
        makeBand (BandType::band, 1000.0, 5.0, 2.0),
        makeBand (BandType::band, 2500.0, -4.0, 1.0),
        makeBand (BandType::band, 6000.0, 3.0, 3.0),
        makeBand (BandType::highShelf, 9000.0, -5.0),
    };

    return bands;
}

void setParameter (EqProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getApvts().getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}
} // namespace

TEST_CASE ("Match: the learner reads a sine at its level, and nothing from silence", "[eq][match]")
{
    eq::match::Learner learner;
    learner.prepare (rate);

    std::vector<float> silence (48000, 0.0f);
    learner.addSamples (silence.data(), (int) silence.size());
    CHECK (learner.getSeconds() == 0.0);

    std::vector<float> tone (96000);

    for (size_t i = 0; i < tone.size(); ++i)
        tone[i] = 0.25f * (float) std::sin (2.0 * 3.14159265358979323846 * 1000.0 * (double) i / rate);

    learner.addSamples (tone.data(), (int) tone.size());

    // 1 kHz is point 48 * log2 (1000 / 20) = 270.9. The grid averages a 48th of an octave, so
    // the peak reads a little under the sine's -12 dB.
    const auto spectrum = learner.getSpectrumDb();
    const auto peak = *std::max_element (spectrum.begin(), spectrum.end());

    CHECK (learner.getSeconds() > 1.5);
    CHECK (peak < -12.0);
    CHECK (peak > -16.0);
    CHECK (std::distance (spectrum.begin(), std::max_element (spectrum.begin(), spectrum.end())) == 271);
}

TEST_CASE ("Match: eight bands fit a known six-band EQ within 1 dB, the same way every time", "[eq][match]")
{
    constexpr int length = 48000 * 20;

    const auto current = learn (pinkNoise (length, 11));
    const auto reference = learn (throughEq (pinkNoise (length, 23), knownEq()));

    const eq::match::Settings settings {}; // 1/3 octave, 100 %, 8 bands, 30 Hz to 18 kHz
    const auto target = eq::match::makeTarget (reference, current, Curve {}, settings);
    const auto fitted = eq::match::fit (target, settings, rate);

    REQUIRE (! fitted.empty());
    REQUIRE ((int) fitted.size() <= settings.maxBands);

    for (const auto& band : fitted)
    {
        CHECK (std::abs (band.gainDb) <= eq::match::maxGainDb);
        CHECK (band.q >= eq::match::minQ);
        CHECK (band.q <= eq::match::maxQ);
        CHECK ((band.type == BandType::band || band.type == BandType::lowShelf || band.type == BandType::highShelf));
    }

    // Against the EQ the reference went through, less the level Match takes out.
    auto wanted = eq::match::responseOf (knownEq(), rate);
    const auto smoothed = eq::match::smooth (wanted, settings.smoothingOctaves);

    auto mean = 0.0;
    auto count = 0;

    for (int i = 0; i < eq::match::numPoints; ++i)
    {
        if (eq::match::hzAt (i) >= 200.0 && eq::match::hzAt (i) <= 5000.0)
        {
            mean += smoothed[(size_t) i];
            ++count;
        }
    }

    for (auto& value : wanted)
        value -= mean / count;

    const auto error = eq::match::rmsDifference (eq::match::responseOf (fitted, rate), wanted, 50.0, 16000.0);

    std::printf ("Match fit: %d bands, %.3f dB RMS from the known EQ between 50 Hz and 16 kHz (target 1 dB)\n", (int) fitted.size(), error);

    for (const auto& band : fitted)
        std::printf ("    %-10s %8.1f Hz %+6.2f dB  Q %.2f\n",
                     band.type == BandType::band ? "bell" : (band.type == BandType::lowShelf ? "low shelf" : "high shelf"),
                     band.frequencyHz, band.gainDb, band.q);

    CHECK (error < 1.0);

    // Deterministic: the same target gives the same bands.
    CHECK (eq::match::fit (target, settings, rate) == fitted);

    // Bands is the most it uses.
    auto three = settings;
    three.maxBands = 3;
    CHECK (eq::match::fit (target, three, rate).size() <= 3);
}

TEST_CASE ("Match: silent gaps don't change what's learned, and a level difference gives no bands", "[eq][match]")
{
    // Six pieces of 47 hops, played on end and with 24 hops of silence between them.
    constexpr int piece = 47 * hop, gap = 24 * hop, pieces = 6;

    const auto noise = pinkNoise (piece * pieces, 5);

    std::vector<float> gapped;

    for (int p = 0; p < pieces; ++p)
    {
        gapped.insert (gapped.end(), noise.begin() + p * piece, noise.begin() + (p + 1) * piece);
        gapped.insert (gapped.end(), (size_t) gap, 0.0f);
    }

    const auto whole = learn (noise);
    const auto withGaps = learn (gapped);

    CHECK (eq::match::rmsDifference (whole, withGaps, 50.0, 16000.0) < 0.2);

    // The same noise 6 dB up: nothing to match.
    auto louder = noise;

    for (auto& sample : louder)
        sample *= 2.0f;

    const eq::match::Settings settings {};
    const auto target = eq::match::makeTarget (learn (louder), whole, Curve {}, settings);

    CHECK (eq::match::rmsBetween (target, 20.0, 20000.0) < 0.01);
    CHECK (eq::match::fit (target, settings, rate).empty());
}

TEST_CASE ("Match: the target takes out what the bands that stay already do, and fades outside the range", "[eq][match]")
{
    Curve reference {}, current {}, existing {};

    for (int i = 0; i < eq::match::numPoints; ++i)
    {
        // 4 dB more around 1 kHz, and 20 dB more everywhere, which is level and not tone.
        const auto octaves = std::log2 (eq::match::hzAt (i) / 1000.0);

        reference[(size_t) i] = -40.0 + 4.0 * std::exp (-octaves * octaves);
        current[(size_t) i] = -60.0;
        existing[(size_t) i] = 1.5;
    }

    eq::match::Settings settings;
    settings.lowHz = 100.0;
    settings.highHz = 8000.0;

    const auto plain = eq::match::makeTarget (reference, current, Curve {}, settings);
    const auto withBands = eq::match::makeTarget (reference, current, existing, settings);

    // Point 271 is 1 kHz; the 200 Hz to 5 kHz mean has come out.
    CHECK (plain[271] > 1.5);
    CHECK (plain[271] < 4.0);
    CHECK (withBands[271] == Catch::Approx (plain[271] - 1.5).margin (1.0e-9));

    // 20 Hz and 20 kHz are more than a third of an octave outside 100 Hz to 8 kHz.
    CHECK (plain[0] == Catch::Approx (0.0).margin (1.0e-9));
    CHECK (withBands[eq::match::numPoints - 1] == Catch::Approx (0.0).margin (1.0e-9));

    // Amount scales it, and it never asks for more than 12 dB.
    settings.amount = 0.5;
    CHECK (eq::match::makeTarget (reference, current, Curve {}, settings)[271] == Catch::Approx (0.5 * plain[271]).margin (1.0e-9));

    for (auto& value : reference)
        value *= -3.0;

    settings.amount = 1.0;

    for (const auto value : eq::match::makeTarget (reference, current, Curve {}, settings))
        CHECK (std::abs (value) <= eq::match::maxGainDb + 1.0e-9);
}

TEST_CASE ("Match: bands are written as one undo step, and the next Match replaces them", "[eq][match]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    EqProcessor processor;
    auto& workflow = processor.getWorkflow();
    workflow.rebase();

    std::vector<BandSpec> before;

    for (int band = 0; band < 4; ++band)
        before.push_back (processor.readBand (band));

    CHECK (processor.getNumFreeForMatch() == eq::maxBands - 4);

    const std::vector<BandSpec> first { makeBand (BandType::band, 300.0, -3.0, 1.4), makeBand (BandType::band, 2000.0, 4.0, 2.0),
                                        makeBand (BandType::highShelf, 8000.0, 3.0) };

    CHECK (processor.applyMatch (first) == 3);
    workflow.commit();

    CHECK (workflow.getNumUndoSteps() == 1);

    // The four bands that were there are untouched, and the next three hold the fit.
    for (int band = 0; band < 4; ++band)
        CHECK (processor.readBand (band) == before[(size_t) band]);

    for (int band = 4; band < 7; ++band)
    {
        const auto spec = processor.readBand (band);
        const auto& wanted = first[(size_t) (band - 4)];

        CHECK (processor.isBandUsed (band));
        CHECK (processor.isMatchBand (band));
        CHECK (spec.on);
        CHECK (spec.type == wanted.type);
        CHECK (spec.frequencyHz == Catch::Approx (wanted.frequencyHz).margin (0.1));
        CHECK (spec.gainDb == Catch::Approx (wanted.gainDb).margin (0.05));
        CHECK (spec.channel == eq::Channel::stereo);
        CHECK_FALSE (processor.readDynamics (band).on);
    }

    CHECK_FALSE (processor.isBandUsed (7));
    CHECK (processor.getNumFreeForMatch() == eq::maxBands - 4);

    // What the last Match wrote isn't in the way of the next target.
    CHECK (eq::match::rmsBetween (processor.getMatchExistingDb(), 20.0, 20000.0) < 1.0e-6);

    // A second Match replaces the first's bands.
    const std::vector<BandSpec> second { makeBand (BandType::band, 500.0, 2.0, 1.0), makeBand (BandType::band, 4000.0, -2.0, 1.0) };

    CHECK (processor.applyMatch (second) == 2);
    workflow.commit();

    CHECK (processor.readBand (4).frequencyHz == Catch::Approx (500.0).margin (0.1));
    CHECK (processor.readBand (5).frequencyHz == Catch::Approx (4000.0).margin (0.1));
    CHECK_FALSE (processor.isBandUsed (6));
    CHECK (workflow.getNumUndoSteps() == 2);

    // A Match band that's been edited is the user's: it stays, and the next Match goes around it.
    setParameter (processor, "b5gain", 6.0f);
    workflow.commit();

    CHECK_FALSE (processor.isMatchBand (4));
    CHECK (processor.isMatchBand (5));
    CHECK (processor.getNumFreeForMatch() == eq::maxBands - 5);
    CHECK (eq::match::rmsBetween (processor.getMatchExistingDb(), 20.0, 20000.0) > 0.1);

    CHECK (processor.applyMatch (first) == 3);
    workflow.commit();

    CHECK (processor.readBand (4).gainDb == Catch::Approx (6.0));
    CHECK (processor.readBand (4).frequencyHz == Catch::Approx (500.0).margin (0.1));
    CHECK (processor.readBand (5).frequencyHz == Catch::Approx (300.0).margin (0.1));
    CHECK (processor.readBand (6).frequencyHz == Catch::Approx (2000.0).margin (0.1));
    CHECK (processor.readBand (7).type == BandType::highShelf);

    // Undo takes the whole Match back in one step.
    const auto steps = workflow.getNumUndoSteps();
    REQUIRE (workflow.undo());

    CHECK (workflow.getNumUndoSteps() == steps - 1);
    CHECK (processor.readBand (5).frequencyHz == Catch::Approx (4000.0).margin (0.1));
    CHECK_FALSE (processor.isBandUsed (7));

    // With every band in use there's nowhere to write.
    for (int band = 0; band < eq::maxBands; ++band)
        setParameter (processor, EqProcessor::id (band, "used"), 1.0f);

    setParameter (processor, "b6gain", 1.0f);
    setParameter (processor, "b7gain", 1.0f);
    setParameter (processor, "b8gain", 1.0f);

    CHECK (processor.applyMatch (first) == 0);
}

TEST_CASE ("Match: spectra, settings and written bands are saved with the project, not with undo", "[eq][match]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    Curve reference {}, current {};

    for (int i = 0; i < eq::match::numPoints; ++i)
    {
        reference[(size_t) i] = -40.0 - 0.01 * i;
        current[(size_t) i] = -50.0 + 0.02 * i;
    }

    juce::MemoryBlock state;

    {
        EqProcessor processor;
        processor.match.restoreSpectra (reference, current, 12.5);
        processor.match.settings.smoothingOctaves = 0.5;
        processor.match.settings.amount = 0.6;
        processor.match.settings.maxBands = 5;
        processor.match.settings.lowHz = 80.0;
        processor.match.settings.highHz = 12000.0;
        processor.match.source = eq::match::Engine::Source::captured;

        CHECK (processor.applyMatch ({ makeBand (BandType::band, 700.0, 3.0, 1.0) }) == 1);
        CHECK (processor.match.isReady());

        // An undo step or an A/B slot copies the settings, and Match's data isn't among them.
        CHECK_FALSE (processor.getWorkflow().capture().extra.getChildWithName (eq::match::Engine::treeType).isValid());

        processor.getStateInformation (state);
    }

    EqProcessor loaded;
    CHECK_FALSE (loaded.match.isReady());

    loaded.setStateInformation (state.getData(), (int) state.getSize());

    CHECK (loaded.match.getReferenceSeconds() == Catch::Approx (12.5));
    CHECK (loaded.match.getCurrentSeconds() == Catch::Approx (12.5));
    CHECK (eq::match::rmsDifference (loaded.match.getReferenceDb(), reference, 20.0, 20000.0) < 1.0e-3);
    CHECK (eq::match::rmsDifference (loaded.match.getCurrentDb(), current, 20.0, 20000.0) < 1.0e-3);
    CHECK (loaded.match.settings.smoothingOctaves == Catch::Approx (0.5));
    CHECK (loaded.match.settings.amount == Catch::Approx (0.6));
    CHECK (loaded.match.settings.maxBands == 5);
    CHECK (loaded.match.settings.lowHz == Catch::Approx (80.0));
    CHECK (loaded.match.settings.highHz == Catch::Approx (12000.0));
    CHECK (loaded.match.source == eq::match::Engine::Source::captured);

    REQUIRE (loaded.match.getWritten().size() == 1);
    CHECK (loaded.isMatchBand (4));

    // The tree doesn't stay in the parameters' state.
    CHECK_FALSE (loaded.getApvts().state.getChildWithName (eq::match::Engine::treeType).isValid());

    // A damaged spectrum is dropped; the rest still loads.
    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    REQUIRE (xml != nullptr);

    auto* match = xml->getChildByName ("MATCH");
    REQUIRE (match != nullptr);
    match->setAttribute ("ref", "not a spectrum");
    match->setAttribute ("bands", "nan");
    match->setAttribute ("written", "99:2:1:1:1;x;4:2:700:3:1");

    juce::MemoryBlock damaged;
    juce::AudioProcessor::copyXmlToBinary (*xml, damaged);

    EqProcessor other;
    other.setStateInformation (damaged.getData(), (int) damaged.getSize());

    CHECK (other.match.getReferenceSeconds() == 0.0);
    CHECK (other.match.getCurrentSeconds() == Catch::Approx (12.5));
    CHECK (other.match.settings.maxBands >= 1);
    CHECK (other.match.settings.maxBands <= eq::maxBands);
    CHECK (other.match.getWritten().size() == 1);
}

TEST_CASE ("Match: learning from the main input and the sidechain, and a fit on the worker", "[eq][match]")
{
    using Engine = eq::match::Engine;

    Engine engine;
    engine.setSampleRate (rate);
    engine.setPanelOpen (true);

    const auto main = pinkNoise (48000 * 8, 31);
    const auto side = throughEq (pinkNoise (48000 * 8, 32), { makeBand (BandType::band, 1000.0, 6.0, 1.0) });

    // Nothing is taken while it isn't learning.
    {
        const float* channels[] { main.data() };
        engine.pushMain (channels, 1, 4096);
    }

    std::this_thread::sleep_for (std::chrono::milliseconds (60));
    CHECK (engine.getCurrentSeconds() == 0.0);

    engine.setLearning (Engine::Learning::both);
    CHECK (engine.isLearningFromSidechain());

    // Paced, as an audio callback would be, though much faster than real time.
    for (int start = 0; start + 2048 <= (int) main.size(); start += 2048)
    {
        const float* mainChannels[] { main.data() + start, main.data() + start };
        const float* sideChannels[] { side.data() + start };

        engine.pushMain (mainChannels, 2, 2048);
        engine.pushSidechain (sideChannels, 1, 2048);

        std::this_thread::sleep_for (std::chrono::milliseconds (2));
    }

    const auto waitFor = [] (const std::function<bool()>& done)
    {
        for (int i = 0; i < 2000 && ! done(); ++i)
            std::this_thread::sleep_for (std::chrono::milliseconds (10));

        return done();
    };

    REQUIRE (waitFor ([&] { return engine.isReady(); }));

    engine.setLearning (Engine::Learning::none);
    CHECK_FALSE (engine.isLearningFromSidechain());

    // The sidechain's 6 dB at 1 kHz is in the target.
    const eq::match::Settings settings {};
    const auto target = eq::match::makeTarget (engine.getReferenceDb(), engine.getCurrentDb(), Curve {}, settings);

    CHECK (target[271] > 2.5);

    std::vector<BandSpec> fitted;
    CHECK_FALSE (engine.takeFit (fitted));

    engine.requestFit (target, settings, rate);
    REQUIRE (waitFor ([&] { return engine.takeFit (fitted); }));

    CHECK (fitted == eq::match::fit (target, settings, rate));
    CHECK_FALSE (engine.takeFit (fitted));
    CHECK_FALSE (engine.isFitting());

    // Closing the card stops the worker; Clear forgets both spectra.
    engine.setPanelOpen (false);
    CHECK_FALSE (engine.isPanelOpen());

    engine.clear();
    CHECK (engine.getReferenceSeconds() == 0.0);
    CHECK_FALSE (engine.isReady());
}

TEST_CASE ("Match: learning changes nothing in the audio", "[eq][match]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto render = [] (bool learning)
    {
        EqProcessor processor;
        setParameter (processor, "b2gain", 4.0f);
        processor.prepareToPlay (rate, 512);

        if (learning)
        {
            processor.match.setPanelOpen (true);
            processor.match.setLearning (eq::match::Engine::Learning::both);
        }

        const auto noise = pinkNoise (512 * 40, 3, 0.2f);
        std::vector<float> output;

        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;

        for (int block = 0; block < 40; ++block)
        {
            for (int channel = 0; channel < 2; ++channel)
                buffer.copyFrom (channel, 0, noise.data() + block * 512, 512);

            processor.processBlock (buffer, midi);
            output.insert (output.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 512);
        }

        return output;
    };

    CHECK (render (true) == render (false));
}
