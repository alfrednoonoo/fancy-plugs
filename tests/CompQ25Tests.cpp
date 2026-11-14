// Phase 3 Q2.5 for Fancy Comp: character, upward compression, auto threshold, sidechain EQ and
// 8x (docs/comp.md, "Phase 3 Q2.5", section 8).

#include "CompDsp.h"
#include "CompProcessor.h"
#include "LegacyCompDsp.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <fancy/Decibels.h>
#include <fancy/dsp/Upward.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <numbers>
#include <random>
#include <utility>
#include <vector>

namespace
{
constexpr double pi = std::numbers::pi;
constexpr double rate = 48000.0;

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples, float value = 0.0f) : left ((size_t) numSamples, value), right ((size_t) numSamples, value) {}

    int size() const { return (int) left.size(); }

    template <typename Dsp>
    void process (Dsp& dsp, Stereo* key = nullptr, int blockSize = 256)
    {
        for (int start = 0; start < size(); start += blockSize)
        {
            float* main[] { left.data() + start, right.data() + start };
            const float* keyChannels[2] {};

            if (key != nullptr)
            {
                keyChannels[0] = key->left.data() + start;
                keyChannels[1] = key->right.data() + start;
            }

            dsp.process (main, 2, keyChannels, key != nullptr ? 2 : 0, std::min (blockSize, size() - start));
        }
    }
};

Stereo sine (double hz, double amplitude, double sampleRate, int numSamples)
{
    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * hz * i / sampleRate));

    return signal;
}

// Paul Kellet's economy pink filter on seeded white noise, scaled to an RMS level.
Stereo pink (double rmsDb, int numSamples, unsigned seed = 1)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<double> white (-1.0, 1.0);

    std::vector<double> raw ((size_t) numSamples);
    double b0 = 0.0, b1 = 0.0, b2 = 0.0, sum = 0.0;

    for (auto& sample : raw)
    {
        const auto w = white (random);
        b0 = 0.99765 * b0 + w * 0.0990460;
        b1 = 0.96300 * b1 + w * 0.2965164;
        b2 = 0.57000 * b2 + w * 1.0526913;
        sample = b0 + b1 + b2 + w * 0.1848;
        sum += sample * sample;
    }

    const auto scale = fancy::dbToGain (rmsDb) / std::sqrt (sum / (double) numSamples);
    Stereo signal (numSamples);

    for (size_t i = 0; i < raw.size(); ++i)
        signal.left[i] = signal.right[i] = (float) (raw[i] * scale);

    return signal;
}

double levelAt (const std::vector<float>& samples, int from, double hz, double sampleRate)
{
    const auto count = (int) samples.size() - from;
    std::complex<double> sum = 0.0;
    double windowSum = 0.0;

    for (int i = 0; i < count; ++i)
    {
        const auto window = 0.5 - 0.5 * std::cos (2.0 * pi * i / count);
        const auto phase = 2.0 * pi * hz * i / sampleRate;

        sum += (double) samples[(size_t) (from + i)] * window * std::complex<double> (std::cos (phase), -std::sin (phase));
        windowSum += window;
    }

    return fancy::gainToDb (2.0 * std::abs (sum) / windowSum, -200.0);
}

double rmsDb (const std::vector<float>& samples, int from, int to)
{
    double sum = 0.0;

    for (int i = from; i < to; ++i)
        sum += (double) samples[(size_t) i] * (double) samples[(size_t) i];

    return 10.0 * std::log10 (std::max (sum / (double) (to - from), 1.0e-30));
}

// No downward compression: everything Q2.5 adds is heard on its own.
CompDsp::Targets transparent()
{
    CompDsp::Targets targets;
    targets.ratio = 1.0;
    targets.thresholdDb = 0.0;
    return targets;
}

template <typename Dsp, typename Targets>
void prepare (Dsp& dsp, const Targets& targets, double sampleRate = rate)
{
    dsp.prepare (sampleRate);
    dsp.snapToTargets (targets);
}

void setParameter (CompProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getApvts().getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

std::vector<float> render (CompProcessor& processor, int numSamples)
{
    juce::AudioBuffer<float> buffer (4, numSamples);
    buffer.clear();
    juce::MidiBuffer midi;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto sample = (float) (0.5 * std::sin (2.0 * pi * 300.0 * i / rate) + 0.2 * std::sin (2.0 * pi * 4100.0 * i / rate));
        buffer.setSample (0, i, sample);
        buffer.setSample (1, i, sample * 0.7f);
    }

    processor.processBlock (buffer, midi);

    std::vector<float> out (buffer.getReadPointer (0), buffer.getReadPointer (0) + numSamples);
    out.insert (out.end(), buffer.getReadPointer (1), buffer.getReadPointer (1) + numSamples);
    return out;
}

// A signal with transients, a changing level and different channels, for comparing renders.
Stereo programme (int numSamples, unsigned seed)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<double> noise (-1.0, 1.0);
    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto envelope = 0.05 + 0.9 * std::pow (0.5 + 0.5 * std::sin (2.0 * pi * 3.0 * i / rate), 4.0);
        const auto tone = std::sin (2.0 * pi * 220.0 * i / rate) + 0.3 * std::sin (2.0 * pi * 3300.0 * i / rate);

        signal.left[(size_t) i] = (float) (envelope * (0.7 * tone + 0.2 * noise (random)));
        signal.right[(size_t) i] = (float) (envelope * (0.5 * tone + 0.3 * noise (random)));
    }

    return signal;
}
} // namespace

// 1. Compatibility ------------------------------------------------------------

TEST_CASE ("Q2.5 at its defaults renders exactly as Comp did before it", "[comp][q25]")
{
    const auto oversampling = GENERATE (0, 1, 2);
    const auto variant = GENERATE (range (0, 6));

    LegacyCompDsp::Targets old;
    old.thresholdDb = -24.0;
    old.ratio = 4.0;
    old.kneeDb = 3.0;
    old.attackMs = 5.0;
    old.releaseMs = 120.0;
    old.oversampling = oversampling;
    old.wetGain = 0.8;
    old.dryGain = 0.3;

    switch (variant)
    {
        case 1: old.highpassHz = 150.0; old.lowpassHz = 6000.0; break;
        case 2: old.classic = true; old.autoRelease = true; old.weirdKnee = true; break;
        case 3: old.feedback = true; old.linkChannels = false; old.preCompMs = 3.0; break;
        case 4: old.detector = LegacyCompDsp::sideLR; old.limitOutput = true; old.minimumPhase = true; break;
        case 5: old.autoMakeup = true; old.linearRelease = true; old.attack90 = true; old.detector = LegacyCompDsp::mid; old.rmsMs = 30.0; break;
        default: break;
    }

    CompDsp::Targets now;
    now.thresholdDb = old.thresholdDb;
    now.ratio = old.ratio;
    now.kneeDb = old.kneeDb;
    now.attackMs = old.attackMs;
    now.releaseMs = old.releaseMs;
    now.preCompMs = old.preCompMs;
    now.rmsMs = old.rmsMs;
    now.lowpassHz = old.lowpassHz;
    now.highpassHz = old.highpassHz;
    now.wetGain = old.wetGain;
    now.dryGain = old.dryGain;
    now.outputGain = old.outputGain;
    now.linkAmount = old.linkAmount;
    now.detector = old.detector;
    now.oversampling = old.oversampling;
    now.weirdKnee = old.weirdKnee;
    now.classic = old.classic;
    now.autoRelease = old.autoRelease;
    now.linearRelease = old.linearRelease;
    now.attack90 = old.attack90;
    now.minimumPhase = old.minimumPhase;
    now.linkChannels = old.linkChannels;
    now.feedback = old.feedback;
    now.autoMakeup = old.autoMakeup;
    now.previewFilter = old.previewFilter;
    now.limitOutput = old.limitOutput;
    now.bypass = old.bypass;

    LegacyCompDsp legacy;
    CompDsp current;
    prepare (legacy, old);
    prepare (current, now);

    REQUIRE (current.getLatencySamples() == legacy.getLatencySamples());

    auto a = programme (24000, 7);
    auto b = a;
    auto keyA = programme (24000, 8);
    auto keyB = keyA;

    // Odd block sizes, and a sidechain for the variant that listens to it.
    a.process (legacy, &keyA, 333);
    b.process (current, &keyB, 333);

    INFO ("oversampling " << oversampling << ", variant " << variant);
    REQUIRE (a.left == b.left);
    REQUIRE (a.right == b.right);
}

TEST_CASE ("a state saved before Q2.5 loads with every new parameter at its default, and renders the same", "[comp][q25]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::StringArray newIds { "charType", "charPos", "charDrive", "up", "upThr", "upRatio", "upLift", "upFloor", "autoThr",
                                     "scLowFreq", "scLowGain", "scMidFreq", "scMidGain", "scMidQ", "scHighFreq", "scHighGain" };

    const auto oversampling = GENERATE (0.0f, 1.0f, 2.0f);

    CompProcessor saved;
    setParameter (saved, "threshold", -22.0f);
    setParameter (saved, "highpass", 120.0f);
    setParameter (saved, "lowpass", 9000.0f);
    setParameter (saved, "oversampling", oversampling);

    juce::MemoryBlock state;
    saved.getStateInformation (state);

    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    REQUIRE (xml != nullptr);

    auto removed = 0;

    for (auto* child = xml->getFirstChildElement(); child != nullptr;)
    {
        auto* next = child->getNextElement();

        if (child->hasTagName ("PARAM") && newIds.contains (child->getStringAttribute ("id")))
        {
            xml->removeChildElement (child, true);
            ++removed;
        }

        child = next;
    }

    CHECK (removed == newIds.size());

    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary (*xml, oldState);

    // Into an instance whose new parameters have been moved.
    CompProcessor loaded;
    setParameter (loaded, "charType", 2.0f);
    setParameter (loaded, "up", 1.0f);
    setParameter (loaded, "autoThr", 1.0f);
    setParameter (loaded, "scMidGain", 9.0f);

    loaded.setStateInformation (oldState.getData(), (int) oldState.getSize());

    auto& apvts = loaded.getApvts();
    CHECK (apvts.getRawParameterValue ("charType")->load() == 0.0f);
    CHECK (apvts.getRawParameterValue ("up")->load() == 0.0f);
    CHECK (apvts.getRawParameterValue ("autoThr")->load() == 0.0f);
    CHECK (apvts.getRawParameterValue ("scMidGain")->load() == 0.0f);
    CHECK (apvts.getRawParameterValue ("oversampling")->load() == oversampling);

    saved.prepareToPlay (rate, 4096);
    loaded.prepareToPlay (rate, 4096);

    CHECK (render (saved, 4096) == render (loaded, 4096));
}

TEST_CASE ("the Oversampling choice keeps its old indices and adds 8x at the end", "[comp][q25]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    CompProcessor processor;
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.getApvts().getParameter ("oversampling"));
    REQUIRE (choice != nullptr);
    CHECK (choice->choices == juce::StringArray { "Off", "2x", "4x", "8x" });
}

// 3 to 6. Character -----------------------------------------------------------

TEST_CASE ("character leaves quiet material untouched", "[comp][q25]")
{
    const auto type = GENERATE ((int) CompDsp::warm, (int) CompDsp::bright);
    const auto oversampling = GENERATE ((int) CompDsp::off, (int) CompDsp::x4);
    const auto post = GENERATE (false, true);

    for (const auto hz : { 20.0, 100.0, 1000.0, 10000.0, 20000.0 })
    {
        const auto measure = [&] (int character)
        {
            auto targets = transparent();
            targets.oversampling = oversampling;
            targets.character = character;
            targets.characterPost = post;
            targets.driveDb = 24.0;

            CompDsp dsp;
            prepare (dsp, targets);

            const auto length = std::max (9600, (int) (8.0 * rate / hz) + 4800);
            auto signal = sine (hz, 0.001, rate, length);
            signal.process (dsp);
            return levelAt (signal.left, length / 2, hz, rate);
        };

        INFO ("type " << type << ", oversampling " << oversampling << ", post " << post << ", " << hz << " Hz");
        CHECK (measure (type) == Catch::Approx (measure (CompDsp::characterOff)).margin (0.05));
    }
}

TEST_CASE ("Warm makes even harmonics and Bright odd ones", "[comp][q25]")
{
    const auto harmonics = [] (int type)
    {
        auto targets = transparent();
        targets.oversampling = CompDsp::x4;
        targets.character = type;
        targets.driveDb = 12.0;

        CompDsp dsp;
        prepare (dsp, targets);

        auto signal = sine (1000.0, 1.0, rate, 19200);
        signal.process (dsp);
        return std::pair { levelAt (signal.left, 4800, 2000.0, rate), levelAt (signal.left, 4800, 3000.0, rate) };
    };

    const auto [warmSecond, warmThird] = harmonics (CompDsp::warm);
    const auto [brightSecond, brightThird] = harmonics (CompDsp::bright);

    INFO ("Warm 2nd " << warmSecond << ", 3rd " << warmThird << "; Bright 2nd " << brightSecond << ", 3rd " << brightThird);
    CHECK (warmSecond > -40.0);
    CHECK (brightThird > -40.0);
    CHECK (brightSecond < -80.0);
}

namespace
{
double characterAliasDb (int type, int oversampling)
{
    constexpr double sampleRate = 44100.0;
    constexpr double hz = 10000.0;

    auto targets = transparent();
    targets.oversampling = oversampling;
    targets.character = type;
    targets.driveDb = 24.0;

    CompDsp dsp;
    prepare (dsp, targets, sampleRate);

    auto signal = sine (hz, 1.0, sampleRate, 88200);
    signal.process (dsp);

    auto worst = -200.0;

    for (int harmonic = 2; harmonic <= 80; ++harmonic)
    {
        const auto wrapped = std::fmod ((double) harmonic * hz, sampleRate);
        const auto alias = std::min (wrapped, sampleRate - wrapped);

        if (alias < 20.0 || std::abs (alias - hz) < 1.0 || std::abs (alias - 2.0 * hz) < 1.0)
            continue;

        worst = std::max (worst, levelAt (signal.left, 44100, alias, sampleRate));
    }

    return worst;
}
} // namespace

// The design note asked for -90 at 8x and -70 at 4x. The model (docs/research/comp-q25-model.py)
// shows an ideal oversampler reaching -88.5 (Bright, 8x) and -73.3 (Bright, 4x) for this
// signal, so the targets keep a margin below what's possible: see docs/comp.md, section 10.
TEST_CASE ("character aliasing at Drive 24 dB", "[comp][q25]")
{
    const auto type = GENERATE ((int) CompDsp::warm, (int) CompDsp::bright);

    const auto at4x = characterAliasDb (type, CompDsp::x4);
    const auto at8x = characterAliasDb (type, CompDsp::x8);

    INFO ("type " << type << ": 4x " << at4x << " dBFS, 8x " << at8x << " dBFS");
    CHECK (at4x < -66.0);
    CHECK (at8x < -82.0);
    CHECK (at8x < at4x - 8.0);
}

TEST_CASE ("changing Character's Type and Position is click-free", "[comp][q25]")
{
    auto targets = transparent();
    targets.driveDb = 12.0;

    CompDsp dsp;
    prepare (dsp, targets);

    auto signal = sine (200.0, 0.25, rate, 48000);
    const auto blockSize = 64;
    const int switchAt[] { 4800, 14400, 24000, 33600, 40800 };
    const int types[] { CompDsp::warm, CompDsp::warm, CompDsp::bright, CompDsp::bright, CompDsp::characterOff };
    const bool post[] { true, false, false, true, true };
    auto next = 0;

    for (int start = 0; start < signal.size(); start += blockSize)
    {
        if (next < 5 && start >= switchAt[next])
        {
            targets.character = types[next];
            targets.characterPost = post[next];
            dsp.setTargets (targets);
            ++next;
        }

        float* main[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (main, 2, nullptr, 0, blockSize);
    }

    // A click is a jump in the second difference. The steady signal, saturated or not, stays
    // far below this; a hard switch of the added part would reach about 1e-2.
    auto worst = 0.0;

    for (size_t i = 2; i < signal.left.size(); ++i)
        worst = std::max (worst, std::abs ((double) signal.left[i] - 2.0 * signal.left[i - 1] + signal.left[i - 2]));

    INFO ("worst second difference " << worst);
    CHECK (worst < 1.0e-3);
}

TEST_CASE ("Pre character is heard by the detector, Post isn't", "[comp][q25]")
{
    const auto reduction = [] (int type, bool post)
    {
        CompDsp::Targets targets;
        targets.thresholdDb = -12.0;
        targets.ratio = 100.0;
        targets.kneeDb = 0.0;
        targets.rmsMs = 0.0;
        targets.attackMs = 0.0;
        targets.character = type;
        targets.characterPost = post;
        targets.driveDb = 24.0;

        CompDsp dsp;
        prepare (dsp, targets);

        auto signal = sine (100.0, 1.0, rate, 9600);
        signal.process (dsp);
        return (double) dsp.takeGainReduction (0);
    };

    // Drive rounds the peaks, so the compressor reacts less to them (Decision 1 A).
    CHECK (reduction (CompDsp::bright, false) < reduction (CompDsp::characterOff, false) - 1.0);
    CHECK (reduction (CompDsp::bright, true) == Catch::Approx (reduction (CompDsp::characterOff, true)).margin (1.0e-4));
}

// 7 and 8. Upward ------------------------------------------------------------

TEST_CASE ("upward follows the static curve", "[comp][q25]")
{
    struct Case
    {
        double thresholdDb, ratio, upThresholdDb, levelDb;
    };

    const auto c = GENERATE (Case { 0.0, 4.0, -40.0, -30.0 }, Case { 0.0, 4.0, -40.0, -46.0 }, Case { 0.0, 4.0, -40.0, -55.0 },
                             Case { 0.0, 4.0, -40.0, -64.0 }, Case { 0.0, 4.0, -40.0, -67.0 }, Case { 0.0, 4.0, -40.0, -75.0 },
                             Case { -20.0, 1.0, -10.0, -30.0 }); // Up threshold above Threshold acts as Threshold

    CompDsp::Targets targets;
    targets.thresholdDb = c.thresholdDb;
    targets.ratio = c.ratio;
    targets.rmsMs = 50.0;
    targets.attackMs = 10.0;
    targets.releaseMs = 100.0;
    targets.upward = true;
    targets.upThresholdDb = c.upThresholdDb;

    CompDsp dsp;
    prepare (dsp, targets);

    // A sine's level in the detector is its RMS.
    const auto amplitude = fancy::dbToGain (c.levelDb) * std::sqrt (2.0);
    auto signal = sine (1000.0, amplitude, rate, 48000);
    signal.process (dsp);

    const auto expected = fancy::dsp::upward::liftFor (c.levelDb, c.thresholdDb, { c.upThresholdDb, 2.0, 12.0, -70.0 });
    const auto measured = levelAt (signal.left, 24000, 1000.0, rate) - fancy::gainToDb (amplitude);

    INFO ("level " << c.levelDb << " dB: expected lift " << expected << ", measured " << measured);
    CHECK (measured == Catch::Approx (expected).margin (0.3));
}

TEST_CASE ("upward doesn't raise silence or noise below the floor", "[comp][q25]")
{
    auto targets = transparent();
    targets.upward = true;
    targets.upRatio = 10.0;
    targets.maxLiftDb = 30.0;

    CompDsp dsp;
    prepare (dsp, targets);

    Stereo silence (9600);
    silence.process (dsp);
    CHECK (*std::max_element (silence.left.begin(), silence.left.end()) == 0.0f);

    auto noise = pink (-90.0, 48000, 3);
    const auto before = rmsDb (noise.left, 24000, 48000);
    noise.process (dsp);
    CHECK (rmsDb (noise.left, 24000, 48000) == Catch::Approx (before).margin (0.1));
}

TEST_CASE ("the GR meter shows reduction only while upward lifts", "[comp][q25]")
{
    auto targets = transparent();
    targets.upward = true;

    CompDsp dsp;
    prepare (dsp, targets);

    // About -57 dBFS RMS: 8.5 dB of lift.
    auto signal = sine (1000.0, 0.002, rate, 24000);
    signal.process (dsp);

    CHECK (dsp.takeGainReduction (0) == 0.0f);
    CHECK (dsp.getDisplayLift() > 5.0f);
}

// 9. Auto threshold ----------------------------------------------------------

TEST_CASE ("auto threshold keeps the reduction steady across input levels", "[comp][q25]")
{
    constexpr int length = (int) (16.0 * rate);
    constexpr int from = (int) (12.0 * rate);

    const auto reductionAt = [] (double levelDb, bool autoOn)
    {
        CompDsp::Targets targets;
        targets.thresholdDb = -24.0;
        targets.ratio = 4.0;
        targets.attackMs = 10.0;
        targets.releaseMs = 150.0;
        targets.autoThreshold = autoOn;

        CompDsp dsp;
        prepare (dsp, targets);

        auto signal = pink (levelDb, length, 11);
        const auto before = rmsDb (signal.left, from, length);
        signal.process (dsp, nullptr, 512);
        return before - rmsDb (signal.left, from, length);
    };

    const double levels[] { -30.0, -18.0, -6.0 };
    double withAuto[3], without[3];

    for (int i = 0; i < 3; ++i)
    {
        withAuto[i] = reductionAt (levels[i], true);
        without[i] = reductionAt (levels[i], false);
    }

    INFO ("with Auto threshold " << withAuto[0] << ", " << withAuto[1] << ", " << withAuto[2]
          << " dB; without " << without[0] << ", " << without[1] << ", " << without[2] << " dB");

    CHECK (std::abs (withAuto[0] - withAuto[2]) < 1.0);
    CHECK (std::abs (withAuto[1] - withAuto[0]) < 1.0);
    CHECK (without[2] - without[0] > 6.0);

    // At the reference level the offset is about zero.
    CHECK (withAuto[1] == Catch::Approx (without[1]).margin (0.2));
}

TEST_CASE ("auto threshold holds through silence and reads the input with feedback detection", "[comp][q25]")
{
    CompDsp::Targets targets;
    targets.thresholdDb = -24.0;
    targets.autoThreshold = true;
    targets.feedback = true;

    CompDsp dsp;
    prepare (dsp, targets);

    auto loud = pink (-6.0, (int) (10.0 * rate), 5);
    loud.process (dsp, nullptr, 512);
    const auto afterLoud = dsp.getAutoThresholdOffset();

    // With feedback the output is compressed, but the offset follows the input's -6 dBFS.
    CHECK (afterLoud > 9.0f);

    Stereo silence ((int) (5.0 * rate));
    silence.process (dsp, nullptr, 512);
    CHECK (dsp.getAutoThresholdOffset() == Catch::Approx (afterLoud).margin (0.1));

    dsp.reset();
    CHECK (dsp.getAutoThresholdOffset() == 0.0f);
}

// 10. Sidechain EQ -----------------------------------------------------------

TEST_CASE ("the sidechain bell raises what the detector hears", "[comp][q25]")
{
    const auto outputFor = [] (double hz, bool preview)
    {
        CompDsp::Targets targets;
        targets.thresholdDb = -40.0;
        targets.ratio = 100.0;
        targets.kneeDb = 0.0;
        targets.rmsMs = 50.0;
        targets.attackMs = 1.0;
        targets.releaseMs = 50.0;
        targets.previewFilter = preview;
        targets.eq[CompDsp::bell] = { 3000.0, 12.0, 1.0 };

        CompDsp dsp;
        prepare (dsp, targets);

        auto signal = sine (hz, 0.1, rate, 24000);
        signal.process (dsp);
        return levelAt (signal.left, 12000, hz, rate);
    };

    // At inf:1 the output sits at the threshold minus the band's gain, so 3 kHz comes out
    // 12 dB lower than 300 Hz.
    CHECK (outputFor (300.0, false) - outputFor (3000.0, false) == Catch::Approx (12.0).margin (0.5));

    // Preview plays the detector's signal, bell included.
    CHECK (outputFor (3000.0, true) - outputFor (300.0, true) == Catch::Approx (12.0).margin (0.5));
}

TEST_CASE ("moving the sidechain bands is smooth and stays finite", "[comp][q25]")
{
    CompDsp::Targets targets;
    targets.thresholdDb = -30.0;
    targets.previewFilter = true;

    CompDsp dsp;
    prepare (dsp, targets);

    auto signal = programme (48000, 21);

    for (int start = 0, step = 0; start < signal.size(); start += 128, ++step)
    {
        const auto t = (double) step / 375.0;
        targets.eq[CompDsp::lowShelf] = { 50.0 + 900.0 * t, -18.0 + 36.0 * t, 0.70710678118654752 };
        targets.eq[CompDsp::bell] = { 10000.0 - 9800.0 * t, 18.0 * std::sin (6.0 * t), 0.3 + 7.0 * t };
        targets.eq[CompDsp::highShelf] = { 20000.0 - 18000.0 * t, step % 100 < 50 ? 0.0 : 12.0, 0.70710678118654752 };
        dsp.setTargets (targets);

        float* main[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (main, 2, nullptr, 0, std::min (128, signal.size() - start));
    }

    for (const auto sample : signal.left)
        REQUIRE (std::isfinite (sample));

    CHECK (*std::max_element (signal.left.begin(), signal.left.end()) < 8.0f);
}

// 11. 8x ---------------------------------------------------------------------

TEST_CASE ("8x reports more latency than 4x and everything at once stays finite", "[comp][q25]")
{
    const auto minimum = GENERATE (false, true);

    auto targets = transparent();
    targets.minimumPhase = minimum;
    targets.oversampling = CompDsp::x4;

    CompDsp four, eight;
    prepare (four, targets);
    targets.oversampling = CompDsp::x8;
    prepare (eight, targets);

    CHECK (eight.getLatencySamples() >= four.getLatencySamples());

    targets.character = CompDsp::warm;
    targets.characterPost = false;
    targets.upward = true;
    targets.autoThreshold = true;
    targets.eq[CompDsp::bell].gainDb = 6.0;
    targets.limitOutput = true;
    targets.thresholdDb = -20.0;
    targets.ratio = 4.0;

    CompDsp everything;
    prepare (everything, targets, 96000.0);

    auto signal = programme (9600, 4);
    signal.process (everything, nullptr, 1000);

    for (const auto sample : signal.left)
        REQUIRE (std::isfinite (sample));
}

// 14. CPU (section 0.4 of the Phase 2.1 brief) -------------------------------

// Hidden from the default run. CI runs it on its own and keeps the output.
TEST_CASE ("comp Q2.5 CPU measurement", "[.measure]")
{
    constexpr int blockSize = 128;
    constexpr int numBlocks = 7500; // 20 s of audio
    constexpr int runs = 5;

    const auto timeOf = [] (const CompDsp::Targets& targets)
    {
        std::vector<double> results;

        for (int run = 0; run < runs; ++run)
        {
            CompDsp dsp;
            prepare (dsp, targets);

            auto signal = programme (blockSize * 64, 99);
            const auto start = std::chrono::steady_clock::now();

            for (int i = 0; i < numBlocks; ++i)
            {
                const auto offset = (i % 64) * blockSize;
                float* channels[] { signal.left.data() + offset, signal.right.data() + offset };
                dsp.process (channels, 2, nullptr, 0, blockSize);
            }

            const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
            results.push_back (100.0 * elapsed.count() / ((double) (numBlocks * blockSize) / rate));
        }

        std::sort (results.begin(), results.end());
        return results[runs / 2];
    };

    const CompDsp::Targets defaults;
    const auto reference = timeOf (defaults);

    auto character = defaults;
    character.oversampling = CompDsp::x4;
    character.character = CompDsp::warm;

    auto everything = character;
    everything.upward = true;
    everything.autoThreshold = true;
    everything.eq[CompDsp::lowShelf].gainDb = 3.0;
    everything.eq[CompDsp::bell].gainDb = 6.0;
    everything.eq[CompDsp::highShelf].gainDb = -3.0;

    auto everything8 = everything;
    everything8.oversampling = CompDsp::x8;

    std::printf ("Comp Q2.5 CPU, 48 kHz, 128 samples, stereo, median of %d runs (ratio to Comp at its defaults):\n", runs);
    std::printf ("  defaults                 %.3f %% (1.00)\n", reference);

    for (const auto& [name, targets] : { std::pair { "Character Warm at 4x     ", character },
                                         std::pair { "everything on at 4x      ", everything },
                                         std::pair { "everything on at 8x      ", everything8 } })
    {
        const auto percent = timeOf (targets);
        std::printf ("  %s%.3f %% (%.2f)\n", name, percent, percent / reference);
    }
}
