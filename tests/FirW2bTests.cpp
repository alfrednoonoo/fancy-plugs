// Phase 4 Wave 2 for Fancy FIR, items 27 and 28 (docs/fir.md, "Phase 4 Wave 2: ratio curve,
// Mid/Side, and EQ-mode dynamics", section 5): the ratio curve for Compressor mode, with item 29
// folded in, and Mid/Side. The figures come from docs/research/fir-w2b-prepush.py.

#include "Curve.h"
#include "FirDsp.h"
#include "FirProcessor.h"
#include "LegacyFirEngine.h"
#include "SpectralEngine.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr int size = 2048;

void fillCurve (fir::Shared& shared, int curve, float value)
{
    for (auto& item : shared.curves[(size_t) curve])
        item.store (value);

    shared.curveVersions[(size_t) curve].fetch_add (1);
}

// Sets a curve's table from a function of frequency, as the processor publishes it.
template <typename Function>
void setCurve (fir::Shared& shared, int curve, Function&& valueAt)
{
    for (int i = 0; i < fir::curvePoints; ++i)
        shared.curves[(size_t) curve][(size_t) i].store ((float) valueAt (fir::curveHz (i)));

    shared.curveVersions[(size_t) curve].fetch_add (1);
}

struct Stereo
{
    std::vector<float> left, right;
};

template <typename Engine, typename Settings>
Stereo run (Engine& engine, const std::vector<float>& left, const std::vector<float>& right, const Settings& settings, int numChannels = 2)
{
    const auto numSamples = (int) left.size();
    Stereo out { std::vector<float> ((size_t) numSamples), std::vector<float> ((size_t) numSamples) };

    for (int position = 0; position < numSamples; position += 64)
    {
        const auto count = std::min (64, numSamples - position);
        engine.process (left.data() + position, right.data() + position, nullptr, nullptr,
                        out.left.data() + position, out.right.data() + position, numChannels, count, settings, true);
    }

    return out;
}

std::vector<float> noise (int numSamples, unsigned seed, float level = 0.1f)
{
    std::mt19937 random (seed);
    std::normal_distribution<float> gauss (0.0f, level);
    std::vector<float> samples ((size_t) numSamples);

    for (auto& sample : samples)
        sample = gauss (random);

    return samples;
}

double energy (const std::vector<double>& samples, int from, int to)
{
    auto sum = 0.0;

    for (int i = from; i < to; ++i)
        sum += samples[(size_t) i] * samples[(size_t) i];

    return sum;
}

// The model's mix (docs/research/fir-w2b-prepush.py): a bass and three harsh tones on bin
// centres; from 2 s, the harsh tones are 15.6 dB louder and the bass 6 dB.
constexpr double lowHz = 93.75;
constexpr double harshHz[] { 2507.8125, 3000.0, 3609.375 };
constexpr double harshPhases[] { 0.4, 1.9, 3.3 };

std::vector<float> harshMix()
{
    const auto numSamples = (int) rate * 4;
    std::mt19937 random (7);
    std::normal_distribution<double> gauss (0.0, 1.0);
    std::vector<float> samples ((size_t) numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto t = (double) i / rate;
        const auto chorus = t >= 2.0;
        auto harsh = 0.0;

        for (int tone = 0; tone < 3; ++tone)
            harsh += std::sin (2.0 * std::numbers::pi * harshHz[tone] * t + harshPhases[tone]);

        samples[(size_t) i] = (float) ((chorus ? 0.5 : 0.25) * std::sin (2.0 * std::numbers::pi * lowHz * t)
                                       + harsh * (chorus ? 0.12 : 0.02) + 0.003 * gauss (random));
    }

    return samples;
}

double amplitude (const std::vector<float>& samples, double hz, int from, int to)
{
    auto sine = 0.0, cosine = 0.0;

    for (int i = from; i < to; ++i)
    {
        const auto phase = 2.0 * std::numbers::pi * hz * (double) i / rate;
        sine += (double) samples[(size_t) i] * std::sin (phase);
        cosine += (double) samples[(size_t) i] * std::cos (phase);
    }

    return 2.0 * std::hypot (sine, cosine) / (double) (to - from);
}

void setValue (FirProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getApvts().getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}
} // namespace

// 1. Legacy ------------------------------------------------------------------------------------

TEST_CASE ("FIR W2b: with a flat ratio curve and Mid/Side off, every mode renders as v1.6.0's engine", "[fir][w2b]")
{
    const auto left = noise (48000, 1), right = noise (48000, 2, 0.05f);
    const auto mode = GENERATE (fir::eq, fir::gate, fir::compressor, fir::subtract, fir::resonance);
    const auto source = GENERATE (fir::learnedSource, fir::adaptiveSource, fir::bothSources);

    if (mode != fir::subtract && source != fir::learnedSource)
        return;

    INFO ("mode " << mode << ", source " << source);

    fir::Shared sharedNow, sharedOld;

    for (auto* shared : { &sharedNow, &sharedOld })
    {
        setCurve (*shared, fir::eq, [] (double hz) { return hz < 1000.0 ? -3.0 : 2.0; });
        setCurve (*shared, fir::gate, [] (double hz) { return hz < 1000.0 ? -30.0 : -45.0; });
        setCurve (*shared, fir::compressor, [] (double hz) { return -40.0 + 5.0 * std::log10 (hz / 20.0); });

        // A learned profile a little under the noise, for Subtract.
        for (auto& value : shared->profile)
            value.store (2.0e-4f);

        shared->hasProfile.store (true);
        shared->profileVersion.fetch_add (1);
    }

    fir::SpectralEngine now ({ size, false, false }, rate, sharedNow);
    fir::legacy::SpectralEngine old ({ size, false, false }, rate, sharedOld);

    fir::FrameSettings settingsNow;
    fir::legacy::FrameSettings settingsOld;

    // A Ratio of 4:1, not the default, so the flat curve's path is the knob's.
    settingsNow.mode = settingsOld.mode = mode;
    settingsNow.compSlope = settingsOld.compSlope = 0.75;
    settingsNow.compRatio = 4.0;
    settingsNow.source = settingsOld.source = source;
    settingsNow.resDepthDb = settingsOld.resDepthDb = 9.0;

    const auto a = run (now, left, right, settingsNow);
    const auto b = run (old, left, right, settingsOld);

    REQUIRE (a.left == b.left);
    REQUIRE (a.right == b.right);
}

TEST_CASE ("FIR W2b: a v1.7.0 project loads with Mid/Side off and a flat ratio curve", "[fir][w2b]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    FirProcessor saved;
    setValue (saved, "mode", (float) fir::compressor);
    setValue (saved, "compRatio", 3.0f);
    saved.setPoints (fir::compressor, { { 100.0, -20.0 }, { 5000.0, -35.0 } });

    juce::MemoryBlock block;
    saved.getStateInformation (block);

    // v1.7.0 had no midSide parameter, curves 0 to 3 only, and no side profile.
    auto xml = juce::AudioProcessor::getXmlFromBinary (block.getData(), (int) block.getSize());
    REQUIRE (xml != nullptr);

    auto tree = juce::ValueTree::fromXml (*xml);
    auto removed = 0;

    for (int i = tree.getNumChildren(); --i >= 0;)
    {
        if (tree.getChild (i).getProperty ("id").toString() == "midSide")
        {
            tree.removeChild (i, nullptr);
            ++removed;
        }
    }

    auto curves = tree.getChildWithName ("Curves");
    REQUIRE (curves.isValid());

    for (int i = curves.getNumChildren(); --i >= 0;)
        if ((int) curves.getChild (i).getProperty ("index") >= 4)
            curves.removeChild (i, nullptr);

    CHECK (curves.getNumChildren() == 4);
    tree.removeChild (tree.getChildWithName ("SideProfile"), nullptr);

    juce::MemoryBlock older;
    juce::AudioProcessor::copyXmlToBinary (*tree.createXml(), older);

    FirProcessor loaded;
    setValue (loaded, "midSide", 1.0f);
    loaded.setPoints (fir::ratioCurve, { { 1000.0, 1.0 } });
    loaded.setStateInformation (older.getData(), (int) older.getSize());

    CHECK (removed == 1);
    CHECK (loaded.getApvts().getRawParameterValue ("midSide")->load() == 0.0f);
    CHECK_FALSE (loaded.isMidSide());
    CHECK (loaded.getPoints (fir::ratioCurve).empty());
    CHECK (loaded.curveDbAt (fir::ratioCurve, 3000.0) == 0.0);
    CHECK (loaded.curveDbAt (fir::compressor, 5000.0) == Catch::Approx (-35.0));

    const auto targets = loaded.readTargets();
    CHECK_FALSE (targets.engine.midSide);
    CHECK (targets.frame.compRatio == Catch::Approx (3.0));
}

// 2. The model's mix -----------------------------------------------------------------------------

TEST_CASE ("FIR W2b: the ratio curve tames the harsh band and leaves the bass", "[fir][w2b]")
{
    const auto mix = harshMix();
    const auto from = (int) (2.3 * rate), to = (int) (3.9 * rate);

    const auto reductions = [&] (bool withCurve)
    {
        fir::Shared shared;
        setCurve (shared, fir::compressor, [] (double hz) { return hz < 1000.0 ? -9.0 : -31.0; });

        // 4:1 from 2 to 5 kHz and 1:1 (two octaves under the knob's 4:1) elsewhere.
        if (withCurve)
            setCurve (shared, fir::ratioCurve, [] (double hz) { return hz > 2000.0 && hz < 5000.0 ? 0.0 : -2.0; });

        fir::SpectralEngine engine ({ size, false, false }, rate, shared);

        fir::FrameSettings settings;
        settings.mode = fir::compressor;
        settings.compRatio = 4.0;
        settings.compSlope = 0.75;
        settings.compAttackMs = 10.0;
        settings.compReleaseMs = 120.0;

        const auto out = run (engine, mix, mix, settings);
        const auto reduction = [&] (double hz)
        {
            return 20.0 * std::log10 (amplitude (mix, hz, from, to) / amplitude (out.left, hz, from + size, to + size));
        };

        auto harsh = 0.0;

        for (const auto hz : harshHz)
            harsh += reduction (hz) / 3.0;

        return std::pair { harsh, reduction (lowHz) };
    };

    const auto [curveHarsh, curveLow] = reductions (true);
    const auto [oneHarsh, oneLow] = reductions (false);

    // The model: 9.40 dB on the harsh tones either way; the bass 0.00 dB with the curve and
    // 2.25 dB with one ratio.
    INFO ("ratio curve: harsh " << curveHarsh << " dB, bass " << curveLow << " dB; one 4:1: harsh " << oneHarsh << " dB, bass " << oneLow << " dB");
    CHECK (curveHarsh == Catch::Approx (9.0).margin (1.5));
    CHECK (std::abs (curveLow) < 0.2);
    CHECK (oneHarsh == Catch::Approx (9.0).margin (1.5));
    CHECK (oneLow > 1.5);
}

TEST_CASE ("FIR W2b: the ratio curve's values are octaves of ratio over the knob", "[fir][w2b]")
{
    CHECK (fir::defaultCurveDb (fir::ratioCurve) == 0.0);
    CHECK (fir::defaultCurveDb (fir::sideCurve (fir::ratioCurve)) == 0.0);
    CHECK (fir::defaultCurveDb (fir::sideCurve (fir::gate)) == -60.0);
    CHECK (fir::defaultCurveDb (fir::sideCurve (fir::depthCurve)) == 100.0);

    CHECK (fir::ratioFromCurve (4.0, 0.0) == 4.0);
    CHECK (fir::ratioFromCurve (4.0, -2.0) == 1.0);
    CHECK (fir::ratioFromCurve (4.0, 9.0) == fir::maxRatio);
    CHECK (fir::ratioFromCurve (1.0, -3.0) == fir::minRatio);
    CHECK (fir::curveFromRatio (2.0, 8.0) == Catch::Approx (2.0));
    CHECK (fir::clampCurveValue (fir::ratioCurve, 20.0) == fir::maxRatioOctaves);
    CHECK (fir::clampCurveValue (fir::sideCurve (fir::ratioCurve), -20.0) == -fir::maxRatioOctaves);

    // The knob moves the whole curve: the same curve at 2:1 and at 8:1 is two octaves apart.
    CHECK (fir::ratioFromCurve (8.0, -1.0) / fir::ratioFromCurve (2.0, -1.0) == Catch::Approx (4.0));
}

TEST_CASE ("FIR W2b: a ratio curve under 1:1 expands upward, as the knob does", "[fir][w2b]")
{
    // The knob at 0.5:1 with a flat curve, and the knob at 2:1 with the curve two octaves down:
    // the same ratio everywhere, so the same output to rounding.
    const auto input = noise (48000, 5, 0.2f);

    const auto render = [&] (double knob, float curve)
    {
        fir::Shared shared;
        fillCurve (shared, fir::compressor, -30.0f);
        fillCurve (shared, fir::ratioCurve, curve);
        fir::SpectralEngine engine ({ size, false, false }, rate, shared);

        fir::FrameSettings settings;
        settings.mode = fir::compressor;
        settings.compRatio = knob;
        settings.compSlope = 1.0 - 1.0 / knob;
        return run (engine, input, input, settings).left;
    };

    const auto knob = render (0.5, 0.0f), curve = render (2.0, -2.0f);
    auto difference = 0.0, total = 0.0;

    for (size_t i = 0; i < knob.size(); ++i)
    {
        difference += ((double) knob[i] - (double) curve[i]) * ((double) knob[i] - (double) curve[i]);
        total += (double) knob[i] * (double) knob[i];
    }

    INFO ("difference " << 10.0 * std::log10 (difference / total + 1.0e-30) << " dB");
    CHECK (10.0 * std::log10 (difference / total + 1.0e-30) < -100.0);
}

// 3 and 4. Mid/Side ----------------------------------------------------------------------------

TEST_CASE ("FIR W2b: Mid/Side with only the side's curve changed leaves the mid alone", "[fir][w2b]")
{
    const auto left = noise (48000, 3), right = noise (48000, 4);
    const auto numSamples = (int) left.size();

    // The side 6 dB down in EQ mode; the mid's curve flat.
    fir::Shared sharedMs;
    fillCurve (sharedMs, fir::sideCurve (fir::eq), -6.0f);
    fir::SpectralEngine midSide ({ size, false, false, true }, rate, sharedMs);

    fir::FrameSettings settings;
    settings.mode = fir::eq;

    const auto out = run (midSide, left, right, settings);

    // The reference: the L/R engine given the mid on both channels, processed with the same
    // flat curve (a mask of exactly 1), and the side alone 6 dB down.
    std::vector<float> mid ((size_t) numSamples), side ((size_t) numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        mid[(size_t) i] = 0.5f * (left[(size_t) i] + right[(size_t) i]);
        side[(size_t) i] = 0.5f * (left[(size_t) i] - right[(size_t) i]);
    }

    fir::Shared sharedLr;
    fir::SpectralEngine lr ({ size, false, false }, rate, sharedLr);
    const auto midReference = run (lr, mid, mid, settings).left;

    std::vector<double> midError ((size_t) numSamples), midOut ((size_t) numSamples), sideIn ((size_t) numSamples), sideOut ((size_t) numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto l = (double) out.left[(size_t) i], r = (double) out.right[(size_t) i];
        midOut[(size_t) i] = 0.5 * (l + r);
        midError[(size_t) i] = midOut[(size_t) i] - (double) midReference[(size_t) i];
        sideOut[(size_t) i] = 0.5 * (l - r);
        sideIn[(size_t) i] = i >= size ? (double) side[(size_t) (i - size)] : 0.0;
    }

    const auto from = 2 * size, to = numSamples;
    const auto midErrorDb = 10.0 * std::log10 (energy (midError, from, to) / energy (midOut, from, to) + 1.0e-30);
    const auto sideChangeDb = 10.0 * std::log10 (energy (sideOut, from, to) / energy (sideIn, from, to));

    INFO ("mid's difference from L/R " << midErrorDb << " dB, side changed by " << sideChangeDb << " dB");
    CHECK (midErrorDb < -120.0);
    CHECK (sideChangeDb == Catch::Approx (-6.0).margin (0.3));
}

TEST_CASE ("FIR W2b: a dual-mono input renders in Mid/Side exactly as in L/R", "[fir][w2b]")
{
    // The side is exactly zero, and every mode keeps it at zero; the mid is the input itself.
    const auto mode = GENERATE (fir::eq, fir::gate, fir::compressor, fir::resonance);
    const auto input = noise (48000, 6);

    INFO ("mode " << mode);

    const auto render = [&] (bool midSide)
    {
        fir::Shared shared;
        fillCurve (shared, fir::eq, 3.0f);
        fillCurve (shared, fir::gate, -30.0f);
        fillCurve (shared, fir::compressor, -35.0f);
        fillCurve (shared, fir::ratioCurve, 1.0f);

        // The side's curves, which have nothing to work on.
        fillCurve (shared, fir::sideCurve (fir::eq), -12.0f);
        fillCurve (shared, fir::sideCurve (fir::gate), 0.0f);

        fir::SpectralEngine engine ({ size, false, false, midSide }, rate, shared);

        fir::FrameSettings settings;
        settings.mode = mode;
        return run (engine, input, input, settings);
    };

    const auto lr = render (false), ms = render (true);

    REQUIRE (lr.left == ms.left);
    REQUIRE (lr.right == ms.right);
}

TEST_CASE ("FIR W2b: a mono input ignores Mid/Side", "[fir][w2b]")
{
    const auto input = noise (24000, 8);

    const auto render = [&] (bool midSide)
    {
        fir::Shared shared;
        fillCurve (shared, fir::eq, -4.0f);
        fillCurve (shared, fir::sideCurve (fir::eq), 12.0f);
        fir::SpectralEngine engine ({ size, false, false, midSide }, rate, shared);

        fir::FrameSettings settings;
        settings.mode = fir::eq;
        return run (engine, input, input, settings, 1).left;
    };

    REQUIRE (render (false) == render (true));
}

TEST_CASE ("FIR W2b: Learn in Mid/Side gives the mid and the side their own profiles", "[fir][w2b]")
{
    // Hiss only in the side: the mid's profile stays near nothing, the side's holds the hiss.
    const auto hiss = noise (96000, 9, 0.05f);
    std::vector<float> left (hiss.size()), right (hiss.size());

    for (size_t i = 0; i < hiss.size(); ++i)
    {
        left[i] = hiss[i];
        right[i] = -hiss[i];
    }

    fir::Shared shared;
    shared.learnTarget.store ((int) rate);
    shared.learnRequested.store (true);

    fir::SpectralEngine engine ({ size, false, false, true }, rate, shared);

    fir::FrameSettings settings;
    settings.mode = fir::subtract;
    run (engine, left, right, settings);

    REQUIRE (shared.hasProfile.load());
    REQUIRE (shared.hasSideProfile.load());

    auto mid = 0.0, side = 0.0;

    for (int i = 100; i < 1500; ++i)
    {
        mid += (double) shared.profile[(size_t) i].load();
        side += (double) shared.sideProfile[(size_t) i].load();
    }

    INFO ("mid " << mid << ", side " << side);
    CHECK (side > 0.0);
    CHECK (mid < side * 1.0e-6);
}

TEST_CASE ("FIR W2b: the ratio curve, the side's curves and the side's profile survive saving", "[fir][w2b]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    FirProcessor processor;
    setValue (processor, "midSide", 1.0f);
    processor.setPoints (fir::ratioCurve, { { 2200.0, 0.0 }, { 6000.0, -2.0 } });
    processor.setPoints (fir::sideCurve (fir::eq), { { 300.0, -5.0 } });

    auto& shared = processor.getShared();

    for (int i = 0; i < fir::profilePoints; ++i)
        shared.sideProfile[(size_t) i].store (1.0e-6f * (float) (i + 1));

    shared.hasSideProfile.store (true);
    shared.sideProfileSeconds.store (2.5f);

    juce::MemoryBlock block;
    processor.getStateInformation (block);

    FirProcessor copy;
    copy.setStateInformation (block.getData(), (int) block.getSize());

    CHECK (copy.isMidSide());
    CHECK (copy.getPoints (fir::ratioCurve) == processor.getPoints (fir::ratioCurve));
    CHECK (copy.curveDbAt (fir::sideCurve (fir::eq), 300.0) == Catch::Approx (-5.0));
    CHECK (copy.curveDbAt (fir::eq, 300.0) == 0.0);
    CHECK (copy.getShared().hasSideProfile.load());
    CHECK (copy.getShared().sideProfileSeconds.load() == 2.5f);
    CHECK (copy.getShared().sideProfile[10].load() == 1.0e-6f * 11.0f);
    CHECK_FALSE (copy.getShared().hasProfile.load());

    // Clear removes both profiles.
    copy.clearProfile();
    CHECK_FALSE (copy.getShared().hasSideProfile.load());
}

TEST_CASE ("FIR W2b: the Dynamic de-harsh preset", "[fir][w2b]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    FirProcessor processor;
    const auto entries = processor.getPresets().getEntries();
    const auto entry = std::find_if (entries.begin(), entries.end(), [] (const auto& e) { return ! e.isUser && e.name == "Dynamic de-harsh"; });

    REQUIRE (entry != entries.end());
    CHECK (entry->category == "Dynamics");
    processor.getPresets().load (*entry);

    CHECK (processor.getMode() == fir::compressor);
    CHECK (processor.getKnobRatio() == Catch::Approx (4.0));
    CHECK (fir::ratioFromCurve (processor.getKnobRatio(), processor.curveDbAt (fir::ratioCurve, 3000.0)) == Catch::Approx (4.0));
    CHECK (fir::ratioFromCurve (processor.getKnobRatio(), processor.curveDbAt (fir::ratioCurve, 100.0)) == Catch::Approx (1.0));
    CHECK (processor.curveDbAt (fir::compressor, 3000.0) < processor.curveDbAt (fir::compressor, 500.0));
}

TEST_CASE ("FIR W2b: switching Mid/Side crossfades to a new engine at the same latency", "[fir][w2b]")
{
    FirDsp dsp;
    dsp.setSynchronousBuilds (true);

    FirDsp::Targets targets;
    targets.engine = { size, false, false };
    dsp.prepare (rate, targets);

    const auto left = noise (48000, 10), right = noise (48000, 11);
    std::vector<float> outLeft (left), outRight (right);
    const auto latency = dsp.getLatencySamples();

    for (int position = 0; position < 48000; position += 256)
    {
        if (position == 12000)
        {
            targets.engine.midSide = true;
            dsp.setTargets (targets);
        }

        float* channels[] { outLeft.data() + position, outRight.data() + position };
        dsp.process (channels, 2, nullptr, 0, 256);
    }

    CHECK (dsp.getLatencySamples() == latency);
    CHECK_FALSE (dsp.isChanging());

    // Flat curves both ways: the output is the input, delayed, through the switch.
    auto worst = 0.0;

    for (int i = latency; i < 48000; ++i)
        worst = std::max (worst, std::abs ((double) outLeft[(size_t) i] - (double) left[(size_t) (i - latency)]));

    CHECK (worst < 1.0e-4);
}
