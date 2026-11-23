// Phase 4 Wave 2 for Fancy Comp: the six-band detector EQ and steeper cuts (item 13) and the link
// modes (item 14). docs/comp.md, "Phase 4 Wave 2: six-band sidechain EQ and steeper cuts", section
// 4, and "Phase 4 Wave 2: link modes", section 4. Models: docs/research/comp-sceq-model.py and
// docs/research/comp-linkmodes-model.py.
#include "CompDsp.h"
#include "CompProcessor.h"
#include "LegacyCompW2Dsp.h"

#include <fancy/dsp/Svf.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <numbers>
#include <random>
#include <vector>

namespace
{
constexpr double pi = std::numbers::pi;
constexpr double rate = 48000.0;

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples, 0.0f), right ((size_t) numSamples, 0.0f) {}

    int size() const { return (int) left.size(); }

    // Processes in blocks; after each, reads the block's largest reduction per channel.
    template <typename Dsp>
    void process (Dsp& dsp, Stereo* key = nullptr, int blockSize = 256, std::vector<float>* reduction0 = nullptr,
                  std::vector<float>* reduction1 = nullptr)
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

            const auto first = dsp.takeGainReduction (0);
            const auto second = dsp.takeGainReduction (1);

            if (reduction0 != nullptr)
                reduction0->push_back (first);

            if (reduction1 != nullptr)
                reduction1->push_back (second);
        }
    }
};

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

Stereo sine (double hz, double amplitude, int numSamples)
{
    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * hz * i / rate));

    return signal;
}

double levelAt (const std::vector<float>& samples, int from, int count, double hz)
{
    std::complex<double> sum = 0.0;

    for (int i = 0; i < count; ++i)
    {
        const auto phase = 2.0 * pi * hz * i / rate;
        sum += (double) samples[(size_t) (from + i)] * std::complex<double> (std::cos (phase), -std::sin (phase));
    }

    return 2.0 * std::abs (sum) / (double) count;
}

template <typename Dsp, typename Targets>
void prepare (Dsp& dsp, const Targets& targets)
{
    dsp.prepare (rate);
    dsp.snapToTargets (targets);
}

// The same settings on v1.7.0's CompDsp and today's.
template <typename Targets>
void configure (Targets& t, int oversampling, int variant)
{
    t.thresholdDb = -24.0;
    t.ratio = 4.0;
    t.kneeDb = 3.0;
    t.attackMs = 5.0;
    t.releaseMs = 120.0;
    t.oversampling = oversampling;
    t.wetGain = 0.8;
    t.dryGain = 0.3;

    switch (variant)
    {
        case 1: t.highpassHz = 150.0; t.lowpassHz = 6000.0; break;
        case 2: t.classic = true; t.autoRelease = true; t.weirdKnee = true; t.detector = 1; break;               // Main L
        case 3: t.feedback = true; t.linkChannels = false; t.preCompMs = 3.0; break;
        case 4: t.detector = 3; t.limitOutput = true; t.minimumPhase = true; t.linkChannels = false; break;      // Sidechain L+R, unlinked
        case 5: t.autoMakeup = true; t.linearRelease = true; t.attack90 = true; t.detector = 6; t.rmsMs = 30.0; break; // Mid
        case 6: t.character = 1; t.characterPost = false; t.driveDb = 12.0; t.detector = 7; break;               // Side
        case 7: t.upward = true; t.optical = true; t.holdMs = 20.0; t.rangeDb = 9.0; t.detector = 5; break;     // Sidechain R
        case 8:
            t.autoThreshold = true;
            t.eq[0].gainDb = -6.0;
            t.eq[1].gainDb = 6.0;
            t.eq[1].frequencyHz = 2500.0;
            t.eq[1].q = 2.0;
            t.eq[2].gainDb = 4.0;
            t.highpassHz = 80.0;
            t.lowpassHz = 9000.0;
            t.detector = 2; // Main R
            break;
        default: break;
    }
}

CompDsp::Targets previewTargets()
{
    CompDsp::Targets t;
    t.previewFilter = true;
    t.thresholdDb = 0.0;
    return t;
}

// What the detector hears from a sine, through Preview filter, in dB.
double detectorLevelDb (const CompDsp::Targets& targets, double hz)
{
    CompDsp dsp;
    prepare (dsp, targets);

    auto signal = sine (hz, 0.1, (int) rate);
    signal.process (dsp);

    const auto count = (int) (0.25 * rate);
    return 20.0 * std::log10 (levelAt (signal.left, signal.size() - count, count, hz) / 0.1);
}

double meanOver (const std::vector<float>& values, const std::vector<bool>& where)
{
    auto sum = 0.0;
    auto count = 0;

    for (size_t i = 0; i < values.size() && i < where.size(); ++i)
    {
        if (where[i])
        {
            sum += (double) values[i];
            ++count;
        }
    }

    return count > 0 ? sum / (double) count : 0.0;
}

void setParameter (CompProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getApvts().getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

std::vector<float> render (CompProcessor& processor, int numSamples)
{
    juce::AudioBuffer<float> buffer (4, numSamples);
    auto signal = programme (numSamples, 21);

    buffer.clear();
    buffer.copyFrom (0, 0, signal.left.data(), numSamples);
    buffer.copyFrom (1, 0, signal.right.data(), numSamples);

    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);

    std::vector<float> out (buffer.getReadPointer (0), buffer.getReadPointer (0) + numSamples);
    out.insert (out.end(), buffer.getReadPointer (1), buffer.getReadPointer (1) + numSamples);
    return out;
}
} // namespace

// Item 13 ---------------------------------------------------------------------

TEST_CASE ("Comp SC EQ 1: bands 4 to 6 Off, 12 dB/oct cuts and Stereo render exactly as v1.7.0", "[comp][w2][sceq]")
{
    const auto oversampling = GENERATE (0, 1, 2, 3);
    const auto variant = GENERATE (0, 1, 2, 3, 4, 5, 6, 7, 8);

    LegacyCompW2Dsp::Targets old;
    CompDsp::Targets now;
    configure (old, oversampling, variant);
    configure (now, oversampling, variant);

    LegacyCompW2Dsp legacy;
    CompDsp current;
    prepare (legacy, old);
    prepare (current, now);

    REQUIRE (current.getLatencySamples() == legacy.getLatencySamples());

    auto a = programme (24000, 7);
    auto b = a;
    auto keyA = programme (24000, 8);
    auto keyB = keyA;

    a.process (legacy, &keyA, 333);
    b.process (current, &keyB, 333);

    INFO ("oversampling " << oversampling << ", variant " << variant);
    REQUIRE (a.left == b.left);
    REQUIRE (a.right == b.right);
}

TEST_CASE ("Comp SC EQ 1: a v1.7.0 state loads with the new parameters at their defaults, and renders the same", "[comp][w2][sceq]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::StringArray newIds { "scLowType", "scMidType", "scHighType", "scLowQ", "scHighQ",
                                     "sc4Type", "sc4Freq", "sc4Gain", "sc4Q", "sc5Type", "sc5Freq", "sc5Gain", "sc5Q",
                                     "sc6Type", "sc6Freq", "sc6Gain", "sc6Q", "hpSlope", "lpSlope", "linkMode" };

    CompProcessor saved;
    setParameter (saved, "threshold", -22.0f);
    setParameter (saved, "highpass", 120.0f);
    setParameter (saved, "scMidGain", 6.0f);
    setParameter (saved, "scLowGain", -4.0f);

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

    CompProcessor loaded;
    setParameter (loaded, "sc4Type", 2.0f);
    setParameter (loaded, "hpSlope", 2.0f);
    setParameter (loaded, "linkMode", 1.0f);
    setParameter (loaded, "scLowType", 0.0f);

    loaded.setStateInformation (oldState.getData(), (int) oldState.getSize());

    auto& apvts = loaded.getApvts();
    CHECK (apvts.getRawParameterValue ("sc4Type")->load() == 0.0f);
    CHECK (apvts.getRawParameterValue ("hpSlope")->load() == 0.0f);
    CHECK (apvts.getRawParameterValue ("linkMode")->load() == 0.0f);
    CHECK (apvts.getRawParameterValue ("scLowType")->load() == 1.0f);

    saved.prepareToPlay (rate, 4096);
    loaded.prepareToPlay (rate, 4096);

    CHECK (render (saved, 4096) == render (loaded, 4096));
}

TEST_CASE ("Comp SC EQ 2: a band's Type shapes what the detector hears", "[comp][w2][sceq]")
{
    const auto plain = previewTargets();

    SECTION ("Bell on a new band: +12 dB at 3 kHz, little at 300 Hz")
    {
        auto t = plain;
        t.eqTypes[3] = CompDsp::bandBell;
        t.eq[3] = { 3000.0, 12.0, 1.0 };

        CHECK (detectorLevelDb (t, 3000.0) - detectorLevelDb (plain, 3000.0) == Catch::Approx (12.0).margin (0.5));
        CHECK (std::abs (detectorLevelDb (t, 300.0) - detectorLevelDb (plain, 300.0)) < 0.5);
    }

    SECTION ("Band 1 turned into a Bell")
    {
        auto t = plain;
        t.eqTypes[0] = CompDsp::bandBell;
        t.eq[0] = { 3000.0, 12.0, 1.0 };

        CHECK (detectorLevelDb (t, 3000.0) - detectorLevelDb (plain, 3000.0) == Catch::Approx (12.0).margin (0.5));
        CHECK (std::abs (detectorLevelDb (t, 300.0) - detectorLevelDb (plain, 300.0)) < 0.5);
    }

    SECTION ("Low shelf: below its frequency")
    {
        auto t = plain;
        t.eqTypes[4] = CompDsp::bandLowShelf;
        t.eq[4] = { 1000.0, 12.0, 0.70710678118654752 };

        CHECK (detectorLevelDb (t, 100.0) - detectorLevelDb (plain, 100.0) == Catch::Approx (12.0).margin (0.5));
        CHECK (std::abs (detectorLevelDb (t, 10000.0) - detectorLevelDb (plain, 10000.0)) < 0.5);
    }

    SECTION ("High shelf: above its frequency")
    {
        auto t = plain;
        t.eqTypes[5] = CompDsp::bandHighShelf;
        t.eq[5] = { 1000.0, 12.0, 0.70710678118654752 };

        CHECK (detectorLevelDb (t, 10000.0) - detectorLevelDb (plain, 10000.0) == Catch::Approx (12.0).margin (0.5));
        CHECK (std::abs (detectorLevelDb (t, 100.0) - detectorLevelDb (plain, 100.0)) < 0.5);
    }

    SECTION ("Off isn't run, whatever its Gain")
    {
        auto t = plain;
        t.eqTypes[3] = CompDsp::bandOff;
        t.eq[3] = { 3000.0, 12.0, 1.0 };

        CHECK (detectorLevelDb (t, 3000.0) == detectorLevelDb (plain, 3000.0));
    }
}

TEST_CASE ("Comp SC EQ 3: 24 and 48 dB/oct are 12 and 36 dB further down an octave below Highpass", "[comp][w2][sceq]")
{
    const auto levels = [] (int slope, bool highPass)
    {
        auto t = previewTargets();

        if (highPass)
        {
            t.highpassHz = 1000.0;
            t.highPassSlope = slope;
            return std::pair { detectorLevelDb (t, 500.0), detectorLevelDb (t, 1000.0) };
        }

        t.lowpassHz = 1000.0;
        t.lowPassSlope = slope;
        return std::pair { detectorLevelDb (t, 2000.0), detectorLevelDb (t, 1000.0) };
    };

    for (const auto highPass : { true, false })
    {
        INFO ((highPass ? "Highpass" : "Lowpass"));

        const auto [octave12, corner12] = levels (CompDsp::slope12, highPass);
        const auto [octave24, corner24] = levels (CompDsp::slope24, highPass);
        const auto [octave48, corner48] = levels (CompDsp::slope48, highPass);

        CHECK (octave12 - octave24 == Catch::Approx (12.0).margin (1.0));
        CHECK (octave12 - octave48 == Catch::Approx (36.0).margin (1.5));

        // The -3 dB point doesn't move.
        CHECK (corner12 == Catch::Approx (-3.01).margin (0.3));
        CHECK (corner24 == Catch::Approx (-3.01).margin (0.3));
        CHECK (corner48 == Catch::Approx (-3.01).margin (0.3));
    }
}

TEST_CASE ("Comp SC EQ 4: a 24 dB/oct cut close to the kick stops the pumping (the model's case 2)", "[comp][w2][sceq]")
{
    // The model's case 2: a bed at -18 dBFS RMS above 200 Hz, a bass line at 98 to 131 Hz to keep
    // (-16 dBFS), a 55 Hz kick at -2 dBFS every 0.5 s, Highpass at 90 Hz. 4:1 at -20 dB, Attack
    // 10 ms, Release 100 ms, RMS 20 ms. The model gives +1.23 to +1.25 dB at 12 dB/oct and +0.15
    // to +0.17 at 24 over five noise beds.
    const auto numSamples = (int) (4.0 * rate);
    Stereo input (numSamples);

    {
        std::mt19937 random (4);
        std::normal_distribution<double> noise;
        fancy::dsp::SvfCoefficients coefficients;
        coefficients.set (200.0, 0.7071, rate);
        fancy::dsp::Svf filter;
        std::vector<double> bed ((size_t) numSamples);
        auto power = 0.0;

        for (auto& value : bed)
        {
            value = filter.tick (coefficients, noise (random)).high;
            power += value * value;
        }

        const auto scale = std::pow (10.0, -18.0 / 20.0) / std::sqrt (power / (double) numSamples);
        const double notes[] { 98.0, 110.0, 123.0, 131.0 };

        for (int i = 0; i < numSamples; ++i)
        {
            const auto note = notes[(i / (int) (0.25 * rate)) % 4];
            const auto inNote = i % (int) (0.25 * rate);
            auto value = bed[(size_t) i] * scale + std::pow (10.0, -16.0 / 20.0) * std::sin (2.0 * pi * note * inNote / rate);

            const auto inKick = i % (int) (0.5 * rate);

            if (inKick < (int) (0.3 * rate))
                value += std::pow (10.0, -2.0 / 20.0) * std::sin (2.0 * pi * 55.0 * inKick / rate) * std::exp (-inKick / (0.1 * rate));

            input.left[(size_t) i] = input.right[(size_t) i] = (float) value;
        }
    }

    constexpr int block = 48;
    const auto numBlocks = (numSamples + block - 1) / block;
    std::vector<bool> after ((size_t) numBlocks), before ((size_t) numBlocks);

    for (int b = 0; b < numBlocks; ++b)
    {
        const auto start = b * block;
        const auto inKick = start % (int) (0.5 * rate);
        after[(size_t) b] = inKick < (int) (0.1 * rate);
        before[(size_t) b] = inKick >= (int) (0.4 * rate) && start > (int) (0.2 * rate);
    }

    const auto pumping = [&] (int slope)
    {
        CompDsp::Targets t;
        t.thresholdDb = -20.0;
        t.ratio = 4.0;
        t.kneeDb = 0.0;
        t.attackMs = 10.0;
        t.releaseMs = 100.0;
        t.rmsMs = 20.0;
        t.highpassHz = 90.0;
        t.highPassSlope = slope;

        CompDsp dsp;
        prepare (dsp, t);

        auto signal = input;
        std::vector<float> reduction;
        signal.process (dsp, nullptr, block, &reduction);
        return meanOver (reduction, after) - meanOver (reduction, before);
    };

    const auto at12 = pumping (CompDsp::slope12);
    const auto at24 = pumping (CompDsp::slope24);
    std::printf ("Comp SC EQ 4: the reduction jumps %+.2f dB on each kick at 12 dB/oct, %+.2f dB at 24\n", at12, at24);

    CHECK (at12 > 1.0);
    CHECK (at24 < 0.5);
}

TEST_CASE ("Comp SC EQ 5: Preview filter plays the new bands and slopes", "[comp][w2][sceq]")
{
    auto t = previewTargets();
    t.highpassHz = 1000.0;
    t.highPassSlope = CompDsp::slope48;
    t.eqTypes[3] = CompDsp::bandBell;
    t.eq[3] = { 3000.0, 12.0, 1.0 };

    // 48 dB/oct an octave down is about -48 dB; the bell lifts 3 kHz by 12.
    CHECK (detectorLevelDb (t, 500.0) < -45.0);
    CHECK (detectorLevelDb (t, 3000.0) == Catch::Approx (12.0).margin (0.6));
}

// Item 14 ---------------------------------------------------------------------

namespace
{
// The model's mix: a vocal in the mid in phrases (1 s of every 1.5 s at -12 dBFS RMS, -40 in the
// gaps), guitars in the side at a steady -20. 4:1 at -24 dB, Attack 10 ms, Release 150 ms, RMS 30 ms.
struct VocalMix
{
    static constexpr int block = 48;

    VocalMix() : input ((int) (6.0 * rate))
    {
        std::mt19937 random (2);
        std::normal_distribution<double> noise;

        for (int i = 0; i < input.size(); ++i)
        {
            const auto t = (double) i / rate;
            const auto singing = std::fmod (t, 1.5) < 1.0;
            const auto mid = noise (random) * std::pow (10.0, (singing ? -12.0 : -40.0) / 20.0);
            const auto side = noise (random) * std::pow (10.0, -20.0 / 20.0);

            input.left[(size_t) i] = (float) (mid + side);
            input.right[(size_t) i] = (float) (mid - side);
        }

        const auto numBlocks = (input.size() + block - 1) / block;

        for (int b = 0; b < numBlocks; ++b)
        {
            const auto t = (double) (b * block) / rate;
            const auto singing = std::fmod (t, 1.5) < 1.0;
            sing.push_back (singing && t > 0.3);
            gap.push_back (! singing && t > 0.3);
        }
    }

    static CompDsp::Targets targets (int mode, bool linked)
    {
        CompDsp::Targets t;
        t.thresholdDb = -24.0;
        t.ratio = 4.0;
        t.kneeDb = 0.0;
        t.attackMs = 10.0;
        t.releaseMs = 150.0;
        t.rmsMs = 30.0;
        t.linkChannels = linked;
        t.linkMode = mode;
        return t;
    }

    Stereo input;
    std::vector<bool> sing, gap;
};
} // namespace

TEST_CASE ("Comp link 2: Mid/Side on a mono signal is Stereo's output, and the side stays silent", "[comp][w2][link]")
{
    const auto linked = GENERATE (true, false);
    const auto oversampling = GENERATE (0, 2);

    auto stereoTargets = CompDsp::Targets {};
    stereoTargets.thresholdDb = -20.0;
    stereoTargets.linkChannels = linked;
    stereoTargets.oversampling = oversampling;
    auto midSideTargets = stereoTargets;
    midSideTargets.linkMode = CompDsp::linkMidSide;

    auto mono = programme (24000, 5);
    mono.right = mono.left;

    CompDsp stereo, midSide;
    prepare (stereo, stereoTargets);
    prepare (midSide, midSideTargets);

    auto a = mono;
    auto b = mono;
    a.process (stereo);
    b.process (midSide);

    auto largestDifference = 0.0, largestSide = 0.0;

    for (int i = 0; i < a.size(); ++i)
    {
        largestDifference = std::max (largestDifference, (double) std::abs (a.left[(size_t) i] - b.left[(size_t) i]));
        largestSide = std::max (largestSide, (double) std::abs (b.left[(size_t) i] - b.right[(size_t) i]));
    }

    INFO ("linked " << linked << ", oversampling " << oversampling);
    CHECK (largestDifference < 1.0e-6); // -120 dB
    CHECK (largestSide < 1.0e-6);
}

TEST_CASE ("Comp link 3: Mid/Side keeps the width steady while Stereo breathes with the vocal", "[comp][w2][link]")
{
    // The model: the side's gain moves 3.9 dB between singing and gaps in Stereo, linked; 0.0 in Mid/Side.
    const VocalMix mix;

    const auto sideMovement = [&mix] (int mode, bool linked)
    {
        CompDsp dsp;
        prepare (dsp, VocalMix::targets (mode, linked));

        auto signal = mix.input;
        std::vector<float> first, second;
        signal.process (dsp, nullptr, VocalMix::block, &first, &second);
        return meanOver (second, mix.sing) - meanOver (second, mix.gap);
    };

    const auto stereo = sideMovement (CompDsp::linkStereo, true);
    const auto midSide = sideMovement (CompDsp::linkMidSide, false);
    std::printf ("Comp link 3: the side's reduction moves %.2f dB with the vocal in Stereo, %.2f dB in Mid/Side\n", stereo, midSide);

    CHECK (stereo > 3.0);
    CHECK (std::abs (midSide) < 0.5);
}

TEST_CASE ("Comp link 4: M>S passes the mid and turns the side down by the mid's level; S>M the other way", "[comp][w2][link]")
{
    const VocalMix mix;

    for (const auto mode : { CompDsp::linkMidToSide, CompDsp::linkSideToMid })
    {
        CompDsp dsp;
        prepare (dsp, VocalMix::targets (mode, false));

        auto signal = mix.input;
        std::vector<float> mid, side;
        signal.process (dsp, nullptr, VocalMix::block, &mid, &side);

        const auto passed = mode == CompDsp::linkMidToSide ? 0 : 1;
        auto largestError = 0.0;

        for (int i = 0; i < signal.size(); ++i)
        {
            const auto in = (double) mix.input.left[(size_t) i], inOther = (double) mix.input.right[(size_t) i];
            const auto out = (double) signal.left[(size_t) i], outOther = (double) signal.right[(size_t) i];
            const auto expected = passed == 0 ? 0.5 * (in + inOther) : 0.5 * (in - inOther);
            const auto got = passed == 0 ? 0.5 * (out + outOther) : 0.5 * (out - outOther);
            largestError = std::max (largestError, std::abs (got - expected));
        }

        INFO ((mode == CompDsp::linkMidToSide ? "M>S" : "S>M"));
        CHECK (largestError < 1.0e-6);
        CHECK (*std::max_element ((passed == 0 ? mid : side).begin(), (passed == 0 ? mid : side).end()) == 0.0f);

        if (mode == CompDsp::linkMidToSide)
        {
            // The model: the side at -8.9 dB while the vocal sings, -3.3 in the gaps.
            CHECK (meanOver (side, mix.sing) > 6.0);
            CHECK (meanOver (side, mix.sing) - meanOver (side, mix.gap) > 3.0);
        }
        else
        {
            // The model: the mid at a steady -3.1 dB from the side's level.
            CHECK (meanOver (mid, mix.sing) == Catch::Approx (3.0).margin (1.5));
            CHECK (std::abs (meanOver (mid, mix.sing) - meanOver (mid, mix.gap)) < 0.5);
        }
    }
}

TEST_CASE ("Comp link 4: M>S gives the mid it leaves alone Auto make-up as well", "[comp][w2][link]")
{
    // Auto make-up adds half of what the curve takes at 0 dBFS: 0.5 x 0.75 x 24 = 9 dB.
    const VocalMix mix;
    auto t = VocalMix::targets (CompDsp::linkMidToSide, false);
    t.autoMakeup = true;

    CompDsp dsp;
    prepare (dsp, t);

    auto signal = mix.input;
    signal.process (dsp, nullptr, VocalMix::block);

    auto inPower = 0.0, outPower = 0.0;

    for (int i = (int) rate; i < signal.size(); ++i)
    {
        const auto in = 0.5 * ((double) mix.input.left[(size_t) i] + (double) mix.input.right[(size_t) i]);
        const auto out = 0.5 * ((double) signal.left[(size_t) i] + (double) signal.right[(size_t) i]);
        inPower += in * in;
        outPower += out * out;
    }

    CHECK (10.0 * std::log10 (outPower / inPower) == Catch::Approx (9.0).margin (0.1));
}

TEST_CASE ("Comp link 5:Sidechain L+R in Mid/Side: the key's mid drives the mid and its side the side", "[comp][w2][link]")
{
    const auto keyInSide = GENERATE (false, true);

    auto t = CompDsp::Targets {};
    t.thresholdDb = -30.0;
    t.linkChannels = false;
    t.detector = CompDsp::sideLR;
    t.linkMode = CompDsp::linkMidSide;

    CompDsp dsp;
    prepare (dsp, t);

    auto main = programme (24000, 3);

    // A key with only a mid (left = right) or only a side (left = -right), at -6 dBFS.
    auto key = sine (200.0, 0.5, 24000);

    if (keyInSide)
        for (auto& sample : key.right)
            sample = -sample;

    std::vector<float> mid, side;
    main.process (dsp, &key, 480, &mid, &side);

    const auto driven = *std::max_element ((keyInSide ? side : mid).begin() + 10, (keyInSide ? side : mid).end());
    const auto quiet = *std::max_element ((keyInSide ? mid : side).begin() + 10, (keyInSide ? mid : side).end());

    INFO ((keyInSide ? "key in the side" : "key in the mid"));
    CHECK (driven > 5.0f);
    CHECK (quiet < 0.01f);
}

// CPU -------------------------------------------------------------------------

TEST_CASE ("comp six-band detector EQ and Mid/Side CPU measurement", "[.measure]")
{
    constexpr int blockSize = 128;
    constexpr int numBlocks = 7500; // 20 s
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

    auto bands = defaults;
    bands.highpassHz = 80.0;
    bands.lowpassHz = 12000.0;
    bands.highPassSlope = CompDsp::slope48;
    bands.lowPassSlope = CompDsp::slope48;

    for (size_t b = 0; b < (size_t) CompDsp::numDetectorBands; ++b)
    {
        bands.eqTypes[b] = CompDsp::bandBell;
        bands.eq[b].gainDb = 3.0;
    }

    auto midSide = defaults;
    midSide.linkMode = CompDsp::linkMidSide;

    const auto withBands = timeOf (bands);
    const auto withMidSide = timeOf (midSide);

    std::printf ("Comp six-band detector EQ and link modes CPU, 48 kHz, 128 samples, stereo, median of %d runs:\n", runs);
    std::printf ("  defaults                         %.3f %%\n", reference);
    std::printf ("  six bands, 48 dB/oct cuts        %.3f %%, %+.1f %% (target < +15 %%)\n", withBands, 100.0 * (withBands / reference - 1.0));
    std::printf ("  Mid/Side                         %.3f %%, %+.1f %% (target < +10 %%)\n", withMidSide, 100.0 * (withMidSide / reference - 1.0));
}
