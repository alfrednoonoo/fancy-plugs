// Phase 4 Wave 1 for Fancy EQ: band solo, auto gain, dynamic reach, analyzer Pre + Post
// (docs/eq.md, "Phase 4 Wave 1", section 6).

#include "EqDsp.h"
#include "EqProcessor.h"
#include "LegacyEqDsp.h"

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

template <typename Dsp>
void run (Dsp& dsp, Stereo& signal, int blockSize = 333)
{
    for (int start = 0; start < signal.size(); start += blockSize)
    {
        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, std::min (blockSize, signal.size() - start));
    }
}

// A signal with a changing level and different channels, for comparing renders.
Stereo programme (int numSamples, unsigned seed)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<double> noise (-1.0, 1.0);
    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto envelope = 0.05 + 0.6 * std::pow (0.5 + 0.5 * std::sin (2.0 * pi * 3.0 * i / rate), 4.0);
        const auto tone = std::sin (2.0 * pi * 180.0 * i / rate) + 0.4 * std::sin (2.0 * pi * 2700.0 * i / rate);

        signal.left[(size_t) i] = (float) (envelope * (0.7 * tone + 0.2 * noise (random)));
        signal.right[(size_t) i] = (float) (envelope * (0.5 * tone + 0.3 * noise (random)));
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

// Paul Kellet's economy pink filter on seeded white noise.
Stereo pink (int numSamples, unsigned seed = 3)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<double> white (-1.0, 1.0);
    double b0 = 0.0, b1 = 0.0, b2 = 0.0;
    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto w = white (random);
        b0 = 0.99765 * b0 + w * 0.0990460;
        b1 = 0.96300 * b1 + w * 0.2965164;
        b2 = 0.57000 * b2 + w * 1.0526913;
        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) (0.1 * (b0 + b1 + b2 + w * 0.1848));
    }

    return signal;
}

double rmsDb (const std::vector<float>& samples, int from)
{
    double sum = 0.0;

    for (size_t i = (size_t) from; i < samples.size(); ++i)
        sum += (double) samples[i] * (double) samples[i];

    return 10.0 * std::log10 (std::max (sum / (double) (samples.size() - (size_t) from), 1.0e-30));
}

eq::BandSpec band (eq::BandType type, double hz, double gainDb = 0.0, double q = 1.0, eq::Channel channel = eq::Channel::stereo)
{
    eq::BandSpec spec;
    spec.type = type;
    spec.frequencyHz = hz;
    spec.gainDb = gainDb;
    spec.q = q;
    spec.on = true;
    spec.channel = channel;
    return spec;
}

template <typename Dsp, typename Targets>
void prepare (Dsp& dsp, const Targets& targets)
{
    dsp.prepare (rate);
    dsp.setSynchronousDesign (true);
    dsp.snapToTargets (targets);
}
} // namespace

// 1. Compatibility ------------------------------------------------------------

TEST_CASE ("EQ Wave 1 at its defaults renders exactly as EQ did in v1.6.0", "[eq][w1]")
{
    const auto variant = GENERATE (0, 1, 2, 3);

    LegacyEqDsp::Targets old;
    old.bands[0] = band (eq::BandType::highPass, 60.0, 0.0, 0.7);
    old.bands[1] = band (eq::BandType::band, 300.0, -4.0, 1.5);
    old.bands[2] = band (eq::BandType::band, 2500.0, 3.0, 2.0, eq::Channel::mid);
    old.bands[3] = band (eq::BandType::highShelf, 8000.0, 2.0, 0.7, eq::Channel::side);
    old.bands[4] = band (eq::BandType::lowShelf, 120.0, 2.5, 0.7, eq::Channel::left);
    old.gain = 0.9;
    old.outputGain = 1.1;

    // Dynamic bands on the bell and shelf, one Below.
    old.dynamics[1].on = true;
    old.dynamics[1].thresholdDb = -30.0;
    old.dynamics[1].rangeDb = -6.0;
    old.dynamics[3].on = true;
    old.dynamics[3].thresholdDb = -36.0;
    old.dynamics[3].rangeDb = 4.0;
    old.dynamics[3].below = true;

    switch (variant)
    {
        case 1: old.oversampling = 1; break;
        case 2: old.oversampling = 2; break;
        case 3: old.linearPhase = true; old.firLength = 1; break;
        default: break;
    }

    EqDsp::Targets now;
    now.bands = old.bands;
    now.dynamics = old.dynamics;
    now.gain = old.gain;
    now.outputGain = old.outputGain;
    now.firLength = old.firLength;
    now.oversampling = old.oversampling;
    now.linearPhase = old.linearPhase;
    now.bypass = old.bypass;

    LegacyEqDsp legacy;
    EqDsp current;
    prepare (legacy, old);
    prepare (current, now);

    REQUIRE (current.getLatencySamples() == legacy.getLatencySamples());

    auto a = programme (24000, 5);
    auto b = a;
    run (legacy, a);
    run (current, b);

    INFO ("variant " << variant);
    REQUIRE (a.left == b.left);
    REQUIRE (a.right == b.right);
}

// 4. Reach --------------------------------------------------------------------

TEST_CASE ("Reach sets how far past Threshold the full Range is reached", "[eq][w1]")
{
    // A tone 6 dB (RMS) over Threshold at the band's centre: half of Range at Reach 12, all of it at 3.
    const auto changeWith = [] (double reachDb)
    {
        EqDsp::Targets targets;
        targets.bands[0] = band (eq::BandType::band, 1000.0, 0.0, 1.0);
        targets.dynamics[0].on = true;
        targets.dynamics[0].thresholdDb = -30.0;
        targets.dynamics[0].rangeDb = -6.0;
        targets.dynamics[0].attackMs = 5.0;
        targets.dynamics[0].reachDb = reachDb;

        EqDsp dsp;
        prepare (dsp, targets);

        const auto amplitude = std::sqrt (2.0) * std::pow (10.0, -24.0 / 20.0);
        auto signal = sine (1000.0, amplitude, (int) rate);
        const auto inputDb = rmsDb (signal.left, (int) rate / 2);
        run (dsp, signal, 256);
        return rmsDb (signal.left, (int) rate / 2) - inputDb;
    };

    CHECK (changeWith (12.0) == Catch::Approx (-3.0).margin (0.3));
    CHECK (changeWith (3.0) == Catch::Approx (-6.0).margin (0.3));
}

// 3. Auto gain ----------------------------------------------------------------

TEST_CASE ("Auto gain keeps pink noise at the input's level", "[eq][w1]")
{
    const auto gainDb = GENERATE (6.0, -9.0);

    EqDsp::Targets targets;
    targets.bands[0] = band (eq::BandType::highShelf, 2000.0, gainDb, 0.7);
    targets.autoGain = true;

    EqDsp dsp;
    prepare (dsp, targets);

    auto signal = pink ((int) (2.0 * rate));
    const auto inputDb = rmsDb (signal.left, (int) rate);
    run (dsp, signal);

    INFO ("shelf " << gainDb << " dB, trim " << dsp.getAutoGainDb() << " dB");
    CHECK (rmsDb (signal.left, (int) rate) == Catch::Approx (inputDb).margin (0.75));
    CHECK ((gainDb > 0.0 ? dsp.getAutoGainDb() < -1.0f : dsp.getAutoGainDb() > 1.0f));
}

// 2. Solo ---------------------------------------------------------------------

TEST_CASE ("A band solo plays the band's region alone", "[eq][w1]")
{
    EqDsp::Targets targets;
    targets.bands[0] = band (eq::BandType::band, 1000.0, 6.0, 4.0);
    targets.bands[1] = band (eq::BandType::lowShelf, 200.0, -12.0, 0.7);
    targets.soloBand = 0;

    const auto levelOf = [&] (double hz)
    {
        EqDsp dsp;
        prepare (dsp, targets);

        auto signal = sine (hz, 0.5, (int) rate / 2);
        const auto inputDb = rmsDb (signal.left, (int) rate / 4);
        run (dsp, signal, 256);
        return rmsDb (signal.left, (int) rate / 4) - inputDb;
    };

    // The input through the band-pass, not the EQ: the boost and the other band don't apply.
    CHECK (levelOf (1000.0) == Catch::Approx (0.0).margin (0.5));
    CHECK (levelOf (4000.0) < -20.0);
    CHECK (levelOf (250.0) < -20.0);
}

TEST_CASE ("The solo is listen target band + 1, and only while an editor is open", "[eq][w1]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    EqProcessor processor;
    CHECK (processor.readTargets().soloBand == -1);

    processor.editorOpened();
    processor.setListenTarget (3);
    processor.setListening (true);
    CHECK (processor.readTargets().soloBand == 2);

    processor.editorClosed();
    CHECK (processor.readTargets().soloBand == -1);
}

// 5. Analyzer -----------------------------------------------------------------

TEST_CASE ("The Analyzer choice keeps its indices and adds Pre + Post", "[eq][w1]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    EqProcessor processor;
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.getApvts().getParameter ("analyzer"));
    REQUIRE (choice != nullptr);
    CHECK (choice->choices == juce::StringArray { "Off", "Pre", "Post", "Pre + Post" });
    CHECK (choice->getIndex() == 2);
}
