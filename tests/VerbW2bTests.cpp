// Phase 4 Wave 2 for Fancy Verb: Attack for the Reverb generator (item 17) and Extra width
// (item 18). docs/verb.md, "Phase 4 Wave 2: Attack for the Reverb generator" and "Width to 200 %",
// section 4 of each. Test 2's ranges were run through docs/research/verb-attack-width-prepush.py.

#include "IrBuilder.h"
#include "VerbChain.h"
#include "VerbDsp.h"
#include "VerbProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>
#include <numbers>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = std::numbers::pi;

using verb::ModuleSpec;
using verb::ModuleType;

ModuleSpec reverbModule (std::initializer_list<std::pair<const char*, float>> values, int seed = 11)
{
    ModuleSpec module;
    module.type = ModuleType::reverb;
    module.seed = seed;

    const auto& settings = verb::settingsOf (ModuleType::reverb);

    for (size_t i = 0; i < settings.size(); ++i)
        module.values[i] = settings[i].defaultValue;

    for (const auto& [id, value] : values)
        for (size_t i = 0; i < settings.size(); ++i)
            if (juce::String (settings[i].id) == id)
                module.values[i] = value;

    return module;
}

juce::AudioBuffer<float> build (const ModuleSpec& module)
{
    verb::ChainSpec chain;
    chain.modules.push_back (module);
    return verb::buildIr (chain, rate).audio;
}

double raisedCosine (int index, double attackSamples)
{
    return (double) index >= attackSamples ? 1.0 : 0.5 - 0.5 * std::cos (pi * (double) index / attackSamples);
}

std::vector<float> noise (int length, unsigned seed)
{
    std::mt19937 generator (seed);
    std::uniform_real_distribution<float> distribution (-0.5f, 0.5f);
    std::vector<float> samples ((size_t) length);

    for (auto& sample : samples)
        sample = distribution (generator);

    return samples;
}

void setParameter (fancy::FancyProcessor& processor, const char* id, float value)
{
    auto& parameter = *processor.getApvts().getParameter (id);
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
}

void prepareOffline (VerbProcessor& processor)
{
    processor.setNonRealtime (true);
    processor.prepareToPlay (rate, 512);
    REQUIRE (processor.waitForBuild (20000));
}

juce::AudioBuffer<float> render (VerbProcessor& processor, const std::vector<float>& input, int blockSize)
{
    juce::AudioBuffer<float> output (2, (int) input.size());
    output.copyFrom (0, 0, input.data(), (int) input.size());
    output.copyFrom (1, 0, input.data(), (int) input.size());

    juce::MidiBuffer midi;

    for (int start = 0; start < output.getNumSamples(); start += blockSize)
    {
        const auto count = juce::jmin (blockSize, output.getNumSamples() - start);
        juce::AudioBuffer<float> view (output.getArrayOfWritePointers(), 2, start, count);
        processor.processBlock (view, midi);
    }

    return output;
}

bool identical (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    if (a.getNumSamples() != b.getNumSamples() || a.getNumChannels() != b.getNumChannels())
        return false;

    for (int ch = 0; ch < a.getNumChannels(); ++ch)
        if (! std::equal (a.getReadPointer (ch), a.getReadPointer (ch) + a.getNumSamples(), b.getReadPointer (ch)))
            return false;

    return true;
}

// A wet-only processor with a short generated reverb, so the wet signal is stereo.
void setUpWet (VerbProcessor& processor)
{
    const auto uid = processor.addModule (ModuleType::reverb, 0);
    processor.setModuleValue (uid, "length", 0.5f);
    processor.setModuleValue (uid, "decay", 0.3f);
    setParameter (processor, "wet", 0.0f);
    setParameter (processor, "dry", -60.0f);
}
} // namespace

// Item 17: Attack --------------------------------------------------------------------------

TEST_CASE ("Verb W2: a Reverb module saved before Attack reads Attack 0 and builds as before", "[verb][w2][attack]")
{
    auto tree = verb::makeModule (ModuleType::reverb, 1);
    tree.removeProperty ("attack", nullptr);

    auto chain = verb::makeChain();
    chain.appendChild (tree, nullptr);

    const auto spec = verb::readChain (chain);
    REQUIRE (spec.modules.size() == 1);
    CHECK (spec.modules[0].values[11] == 0.0f);

    auto withAttack = chain.createCopy();
    withAttack.getChild (0).setProperty ("attack", 0.0f, nullptr);

    CHECK (identical (verb::buildIr (spec, rate).audio, verb::buildIr (verb::readChain (withAttack), rate).audio));
}

TEST_CASE ("Verb W2: Attack only fades the generated tail in, with a raised cosine, on either generator", "[verb][w2][attack]")
{
    // Early Off with every Q2.8 setting at its default runs the original generator; Room, Hall,
    // Plate and True stereo run the extended one. Attack mustn't move a module from one to the other.
    const auto early = GENERATE (0.0f, 1.0f, 2.0f, 3.0f);
    const auto trueStereo = GENERATE (0.0f, 1.0f);
    const auto attackMs = 150.0;

    const auto plain = build (reverbModule ({ { "length", 1.0f }, { "early", early }, { "trueStereo", trueStereo } }));
    const auto faded = build (reverbModule ({ { "length", 1.0f }, { "early", early }, { "trueStereo", trueStereo }, { "attack", (float) attackMs } }));

    INFO ("Early " << early << ", True stereo " << trueStereo);
    REQUIRE (plain.getNumChannels() == faded.getNumChannels());
    REQUIRE (plain.getNumSamples() == faded.getNumSamples());

    const auto attackSamples = attackMs * 0.001 * rate;
    auto worst = 0.0;
    auto changedAfter = false;

    for (int ch = 0; ch < plain.getNumChannels(); ++ch)
    {
        for (int i = 0; i < plain.getNumSamples(); ++i)
        {
            const auto before = (double) plain.getSample (ch, i), after = (double) faded.getSample (ch, i);

            if ((double) i >= attackSamples)
                changedAfter = changedAfter || before != after;
            else
                worst = std::max (worst, std::abs (after - before * raisedCosine (i, attackSamples)));
        }
    }

    // Each sample is rounded to float once: well under 1e-6 of a unit-energy IR's samples.
    CHECK (worst < 1.0e-6);
    CHECK_FALSE (changedAfter);

    // Halfway through, the fade is at half level (test 3).
    CHECK (raisedCosine ((int) (attackSamples / 2), attackSamples) == Catch::Approx (0.5).margin (1.0e-4));
}

TEST_CASE ("Verb W2: Attack 150 ms moves the tail's energy peak and takes away clarity", "[verb][w2][attack]")
{
    // Decay 1.8 s is the default. The model puts the peak at 120 to 140 ms and C80's fall at
    // 11.7 to 12.0 dB over 30 seeds; the note's ranges are 110 to 150 ms and 10 to 14 dB.
    const auto plain = build (reverbModule ({}));
    const auto faded = build (reverbModule ({ { "attack", 150.0f } }));

    const auto frame = (int) (0.01 * rate);
    const auto split = (int) (0.08 * rate);

    const auto energy = [] (const juce::AudioBuffer<float>& ir, int from, int to)
    {
        auto sum = 0.0;

        for (int ch = 0; ch < 2; ++ch)
            for (int i = from; i < std::min (to, ir.getNumSamples()); ++i)
                sum += (double) ir.getSample (ch, i) * (double) ir.getSample (ch, i);

        return sum;
    };

    const auto c80 = [&] (const juce::AudioBuffer<float>& ir)
    {
        return 10.0 * std::log10 (energy (ir, 0, split) / energy (ir, split, ir.getNumSamples()));
    };

    auto peakFrame = 0;
    auto peakEnergy = 0.0;

    for (int k = 0; (k + 1) * frame <= faded.getNumSamples(); ++k)
    {
        if (const auto e = energy (faded, k * frame, (k + 1) * frame); e > peakEnergy)
        {
            peakEnergy = e;
            peakFrame = k;
        }
    }

    const auto fall = c80 (plain) - c80 (faded);

    INFO ("peak at " << peakFrame * 10 << " ms, C80 falls " << fall << " dB");
    CHECK (peakFrame * 10 >= 110);
    CHECK (peakFrame * 10 <= 150);
    CHECK (fall >= 10.0);
    CHECK (fall <= 14.0);
}

TEST_CASE ("Verb W2: changing Attack while playing rebuilds without a click", "[verb][w2][attack]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;
    setUpWet (processor);
    prepareOffline (processor);

    std::vector<float> sine (96000);

    for (size_t i = 0; i < sine.size(); ++i)
        sine[i] = 0.25f * (float) std::sin (2.0 * pi * 300.0 * (double) i / rate);

    const auto maxJump = [] (const float* samples, int from, int to)
    {
        auto jump = 0.0;

        for (int i = from + 1; i < to; ++i)
            jump = std::max (jump, (double) std::abs (samples[i] - samples[i - 1]));

        return jump;
    };

    const auto settled = render (processor, sine, 128);
    const auto steadyJump = maxJump (settled.getReadPointer (0), 48000, 96000);

    processor.setModuleValue (processor.getChain().getChild (0)[verb::ids::uid], "attack", 150.0f);
    REQUIRE (processor.waitForBuild (20000));

    const auto swapped = render (processor, sine, 128);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < swapped.getNumSamples(); ++i)
            REQUIRE (std::isfinite (swapped.getSample (ch, i)));

    CHECK (maxJump (swapped.getReadPointer (0), 0, swapped.getNumSamples()) < steadyJump * 3.0 + 0.01);
}

// Item 18: Extra width ---------------------------------------------------------------------

TEST_CASE ("Verb W2: a project saved before Extra width renders as before, at every Center and Width", "[verb][w2][width]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto center = GENERATE (-100.0f, -30.0f, 0.0f, 55.0f);
    const auto width = GENERATE (0.0f, 37.0f, 100.0f);

    VerbProcessor source;
    setUpWet (source);
    setParameter (source, "center", center);
    setParameter (source, "width", width);

    // The saved state without Extra width, as a v1.7.0 project has it.
    juce::MemoryBlock state;
    source.getStateInformation (state);
    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    REQUIRE (xml != nullptr);

    for (auto* child = xml->getFirstChildElement(); child != nullptr;)
    {
        auto* next = child->getNextElement();

        if (child->hasTagName ("PARAM") && child->getStringAttribute ("id") == "extraWidth")
            xml->removeChildElement (child, true);

        child = next;
    }

    juce::MemoryBlock legacy;
    juce::AudioProcessor::copyXmlToBinary (*xml, legacy);

    VerbProcessor restored;
    restored.setStateInformation (legacy.getData(), (int) legacy.getSize());
    CHECK (restored.getApvts().getRawParameterValue ("extraWidth")->load() == 100.0f);

    for (auto* processor : { &source, &restored })
        prepareOffline (*processor);

    const auto input = noise (24000, 21);

    INFO ("Center " << center << ", Width " << width);
    CHECK (identical (render (source, input, 512), render (restored, input, 512)));
}

TEST_CASE ("Verb W2: Extra width 200 % doubles the wet side and leaves the mono sum", "[verb][w2][width]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor normal, wide;

    for (auto* processor : { &normal, &wide })
        setUpWet (*processor);

    setParameter (wide, "extraWidth", 200.0f);

    for (auto* processor : { &normal, &wide })
        prepareOffline (*processor);

    const auto input = noise (48000, 22);
    const auto a = render (normal, input, 512);
    const auto b = render (wide, input, 512);

    auto sideA = 0.0, sideB = 0.0, worstMono = 0.0, peak = 0.0;

    for (int i = 12000; i < 48000; ++i)
    {
        const auto la = (double) a.getSample (0, i), ra = (double) a.getSample (1, i);
        const auto lb = (double) b.getSample (0, i), rb = (double) b.getSample (1, i);

        sideA += (la - ra) * (la - ra);
        sideB += (lb - rb) * (lb - rb);
        worstMono = std::max (worstMono, std::abs ((la + ra) - (lb + rb)) * 0.5);
        peak = std::max (peak, std::abs (la));
    }

    const auto sideGainDb = 10.0 * std::log10 (sideB / sideA);

    INFO ("side " << sideGainDb << " dB, mono sum differs by up to " << worstMono << " (peak " << peak << ")");
    REQUIRE (sideA > 0.0);
    CHECK (sideGainDb == Catch::Approx (20.0 * std::log10 (2.0)).margin (0.05));

    // Each channel is rounded to float before the sum, so the mono sum matches to float rounding.
    CHECK (worstMono < 1.0e-6 * std::max (1.0, peak));
}

TEST_CASE ("Verb W2: Extra width moves the side smoothly, as Width does", "[verb][w2][width]")
{
    // Two identical wet paths; one has its width moved from 1 to 2 at 0.5 s. The side's ratio
    // between them is the smoother's value: no step, then 20 ms one-pole to 2.
    std::mt19937 random (5);
    std::normal_distribution<float> gaussian;
    std::vector<float> irLeft (24000), irRight (24000);

    for (size_t i = 0; i < irLeft.size(); ++i)
    {
        const auto envelope = 0.05f * (float) std::exp (-6.907755 * (double) i / rate / 0.4);
        irLeft[i] = envelope * gaussian (random);
        irRight[i] = envelope * gaussian (random);
    }

    const auto mode = verb::Convolver::Mode::zeroLatency;
    VerbDsp fixed, moved;

    VerbDsp::Targets targets;
    targets.wet = 1.0;
    targets.dry = 0.0;

    for (auto* dsp : { &fixed, &moved })
    {
        dsp->prepare (rate, verb::Convolver::latencyOf (mode));
        dsp->offer (std::make_unique<verb::Convolver> (irLeft.data(), irRight.data(), (int) irLeft.size(), mode, 8192));
        dsp->snapToTargets (targets);
    }

    const auto input = noise (48000, 23);
    auto a = std::vector<std::vector<float>> { input, input };
    auto b = a;
    constexpr int block = 480, change = 24000;

    for (int start = 0; start < (int) input.size(); start += block)
    {
        if (start == change)
        {
            auto wider = targets;
            wider.width = 2.0;
            moved.setTargets (wider);
        }

        float* channelsA[] { a[0].data() + start, a[1].data() + start };
        float* channelsB[] { b[0].data() + start, b[1].data() + start };
        fixed.process (channelsA, 2, block, true);
        moved.process (channelsB, 2, block, true);
    }

    const auto ratioAt = [&] (int from, int to)
    {
        // The side's gain over a short stretch, by least squares, so tiny sides don't count much.
        auto ab = 0.0, aa = 0.0;

        for (int i = from; i < to; ++i)
        {
            const auto sa = (double) a[0][(size_t) i] - (double) a[1][(size_t) i];
            const auto sb = (double) b[0][(size_t) i] - (double) b[1][(size_t) i];
            ab += sa * sb;
            aa += sa * sa;
        }

        return ab / aa;
    };

    const auto samplesPerMs = rate * 0.001;
    const auto expected = [&] (double ms) { return 2.0 - std::exp (-ms / 20.0); };

    CHECK (ratioAt (change - 480, change) == Catch::Approx (1.0).margin (1.0e-4));
    CHECK (ratioAt (change, change + 24) == Catch::Approx (1.0).margin (0.03));                       // the first 0.5 ms
    CHECK (ratioAt (change + (int) (19.5 * samplesPerMs), change + (int) (20.5 * samplesPerMs)) == Catch::Approx (expected (20.0)).margin (0.03));
    CHECK (ratioAt (change + (int) (200 * samplesPerMs), change + (int) (220 * samplesPerMs)) == Catch::Approx (2.0).margin (1.0e-3));
}
