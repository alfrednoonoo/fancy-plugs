// Phase 3 Q2.8 for Fancy Verb, push 1: true stereo (docs/verb.md, "Phase 3 Q2.8", section 8).

#include "Convolver.h"
#include "IrBuilder.h"
#include "IrFiles.h"
#include "LegacyVerbConvolver.h"
#include "LegacyVerbIrBuilder.h"
#include "VerbChain.h"
#include "VerbLibrary.h"
#include "VerbProcessor.h"

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
constexpr double rate = 48000.0;

using verb::Convolver;
using verb::ModuleSpec;
using verb::ModuleType;

std::vector<float> noise (int length, unsigned seed, double decaySamples = 0.0)
{
    std::mt19937 generator (seed);
    std::uniform_real_distribution<float> distribution (-1.0f, 1.0f);

    std::vector<float> samples ((size_t) length);

    for (int i = 0; i < length; ++i)
        samples[(size_t) i] = distribution (generator) * (decaySamples > 0.0 ? (float) std::exp (-(double) i / decaySamples) : 1.0f);

    return samples;
}

std::vector<double> direct (const std::vector<float>& input, const std::vector<float>& ir, int length)
{
    std::vector<double> output ((size_t) length, 0.0);

    for (size_t n = 0; n < input.size(); ++n)
        if (input[n] != 0.0f)
            for (size_t k = 0; k < ir.size() && n + k < output.size(); ++k)
                output[n + k] += (double) input[n] * (double) ir[k];

    return output;
}

ModuleSpec moduleOf (ModuleType type, int seed = 11)
{
    ModuleSpec module;
    module.type = type;
    module.seed = seed;

    const auto& settings = verb::settingsOf (type);

    for (size_t i = 0; i < settings.size(); ++i)
        module.values[i] = settings[i].defaultValue;

    return module;
}

std::shared_ptr<verb::IrAudio> makeAudio (const std::vector<std::vector<float>>& channels, double sampleRate)
{
    auto audio = std::make_shared<verb::IrAudio>();
    audio->sampleRate = sampleRate;
    audio->audio.setSize ((int) channels.size(), (int) channels[0].size());

    for (size_t ch = 0; ch < channels.size(); ++ch)
        audio->audio.copyFrom ((int) ch, 0, channels[ch].data(), (int) channels[ch].size());

    return audio;
}

ModuleSpec fileModule (std::shared_ptr<const verb::IrAudio> audio, int gainMode, bool mono)
{
    auto module = moduleOf (ModuleType::file);
    module.file = std::move (audio);
    module.gainMode = gainMode;
    module.mono = mono;
    return module;
}

bool sameAudio (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    if (a.getNumChannels() != b.getNumChannels() || a.getNumSamples() != b.getNumSamples())
        return false;

    for (int ch = 0; ch < a.getNumChannels(); ++ch)
        if (! std::equal (a.getReadPointer (ch), a.getReadPointer (ch) + a.getNumSamples(), b.getReadPointer (ch)))
            return false;

    return true;
}

// Every module type, files of several shapes, and the display handles.
std::vector<verb::ChainSpec> compatibilityChains()
{
    const auto left = noise (30000, 21, 8000.0);
    const auto right = noise (30000, 22, 8000.0);
    const auto third = noise (30000, 23, 8000.0);
    const auto fourth = noise (30000, 24, 8000.0);

    std::vector<verb::ChainSpec> chains;

    {
        verb::ChainSpec chain;
        chain.modules.push_back (moduleOf (ModuleType::reverb));
        chain.modules.push_back (moduleOf (ModuleType::echo));
        auto filter = moduleOf (ModuleType::lpHp);
        filter.values[0] = 6000.0f;
        filter.values[1] = 120.0f;
        chain.modules.push_back (filter);
        chain.modules.push_back (moduleOf (ModuleType::normalize));
        chains.push_back (chain);
    }

    {
        verb::ChainSpec chain;
        chain.modules.push_back (fileModule (makeAudio ({ left, right }, 44100.0), 5, false));
        auto trim = moduleOf (ModuleType::trim);
        trim.values[0] = 10.0f;
        trim.values[2] = 130.0f; // stretch
        trim.values[3] = 20.0f;
        trim.values[4] = 50.0f;
        chain.modules.push_back (trim);
        chain.modules.push_back (moduleOf (ModuleType::reverse));
        chain.trimStart = 0.01;
        chains.push_back (chain);
    }

    {
        verb::ChainSpec chain;
        chain.modules.push_back (fileModule (makeAudio ({ left }, rate), 3, true));
        chain.modules.push_back (moduleOf (ModuleType::reverb, 5));
        chains.push_back (chain);
    }

    {
        // A four-channel file in a module saved before Q2.8: no "ts", so the first two channels.
        verb::ChainSpec chain;
        chain.modules.push_back (fileModule (makeAudio ({ left, right, third, fourth }, rate), 0, false));
        chains.push_back (chain);
    }

    return chains;
}
} // namespace

// 1. Compatibility ------------------------------------------------------------

TEST_CASE ("a direct IR builds and convolves exactly as before Q2.8", "[verb][q28]")
{
    const auto mode = GENERATE (Convolver::Mode::zeroLatency, Convolver::Mode::lowCpu);
    const auto blockSize = GENERATE (7, 1000);

    auto index = 0;

    for (const auto& chain : compatibilityChains())
    {
        INFO ("chain " << index++);

        const auto built = verb::buildIr (chain, rate);
        const auto old = verb::legacy::buildIr (chain, rate);

        REQUIRE (sameAudio (built.audio, old.audio));
        CHECK (built.preview.numChannels == old.preview.numChannels);
        CHECK (built.preview.columns.size() == old.preview.columns.size());

        for (size_t c = 0; c < built.preview.columns.size(); ++c)
            REQUIRE (built.preview.columns[c].high == old.preview.columns[c].high);

        const auto length = built.audio.getNumSamples();
        Convolver current (built.audio.getReadPointer (0), built.audio.getReadPointer (1), length, mode, 32768);
        const auto legacyMode = mode == Convolver::Mode::zeroLatency ? verb::legacy::Convolver::Mode::zeroLatency
                                                                     : verb::legacy::Convolver::Mode::lowCpu;
        verb::legacy::Convolver legacy (old.audio.getReadPointer (0), old.audio.getReadPointer (1), length, legacyMode, 32768);

        const auto inputLeft = noise (60000, 31);
        const auto inputRight = noise (60000, 32);
        std::vector<float> a (inputLeft.size(), 0.0f), b = a, c = a, d = a;

        for (int start = 0; start < (int) inputLeft.size(); start += blockSize)
        {
            const auto count = std::min (blockSize, (int) inputLeft.size() - start);
            current.process (inputLeft.data() + start, inputRight.data() + start, a.data() + start, b.data() + start, count, true);
            legacy.process (inputLeft.data() + start, inputRight.data() + start, c.data() + start, d.data() + start, count, true);
        }

        REQUIRE (a == c);
        REQUIRE (b == d);
    }
}

// 4. True stereo convolver ----------------------------------------------------

TEST_CASE ("true-stereo convolver matches direct convolution of all four paths", "[verb][q28]")
{
    const auto mode = GENERATE (Convolver::Mode::zeroLatency, Convolver::Mode::lowCpu);
    const auto irLength = GENERATE (100, 3000, 20000);
    const auto blockSize = GENERATE (1, 7, 128, 1000, 8192);

    if (blockSize == 1 && irLength > 3000)
        return;

    const auto ll = noise (irLength, 41, irLength / 3.0);
    const auto rr = noise (irLength, 42, irLength / 3.0);
    const auto rl = noise (irLength, 43, irLength / 3.0);
    const auto lr = noise (irLength, 44, irLength / 3.0);

    constexpr int burst = 1500;
    const auto length = irLength + burst + 8192;

    auto inputLeft = noise (burst, 45);
    auto inputRight = noise (burst, 46);
    inputLeft.resize ((size_t) length, 0.0f);
    inputRight.resize ((size_t) length, 0.0f);

    Convolver convolver (ll.data(), rr.data(), rl.data(), lr.data(), irLength, mode, 32768);
    REQUIRE (convolver.isTrueStereo());

    std::vector<float> left ((size_t) length, 0.0f), right ((size_t) length, 0.0f);

    for (int start = 0; start < length; start += blockSize)
    {
        const auto count = std::min (blockSize, length - start);
        convolver.process (inputLeft.data() + start, inputRight.data() + start, left.data() + start, right.data() + start, count, true);
    }

    const auto delay = convolver.getLatency();
    const auto fromLeftL = direct (inputLeft, ll, length), fromRightL = direct (inputRight, rl, length);
    const auto fromLeftR = direct (inputLeft, lr, length), fromRightR = direct (inputRight, rr, length);

    auto worst = 0.0;

    for (int i = delay; i < length; ++i)
    {
        const auto j = (size_t) (i - delay);
        worst = std::max (worst, std::abs ((double) left[(size_t) i] - (fromLeftL[j] + fromRightL[j])));
        worst = std::max (worst, std::abs ((double) right[(size_t) i] - (fromLeftR[j] + fromRightR[j])));
    }

    CAPTURE (irLength, blockSize);
    CHECK (worst < 2.0e-2);
    CHECK (convolver.getLateBlocks() == 0);
}

TEST_CASE ("a cross path alone sends the left input to the right output only", "[verb][q28]")
{
    const auto mode = GENERATE (Convolver::Mode::zeroLatency, Convolver::Mode::lowCpu);

    constexpr int irLength = 6000;
    const std::vector<float> silent (irLength, 0.0f);
    const auto lr = noise (irLength, 51, 2000.0);

    Convolver convolver (silent.data(), silent.data(), silent.data(), lr.data(), irLength, mode, 32768);

    auto input = noise (2000, 52);
    input.resize (20000, 0.0f);
    const std::vector<float> none (input.size(), 0.0f);
    std::vector<float> left (input.size(), 0.0f), right (input.size(), 0.0f);

    convolver.process (input.data(), none.data(), left.data(), right.data(), (int) input.size(), true);

    auto leftPeak = 0.0, rightPeak = 0.0;

    for (size_t i = 0; i < input.size(); ++i)
    {
        leftPeak = std::max (leftPeak, (double) std::abs (left[i]));
        rightPeak = std::max (rightPeak, (double) std::abs (right[i]));
    }

    CHECK (leftPeak < 1.0e-4);
    CHECK (rightPeak > 1.0);
}

// 5. Files and pairs ----------------------------------------------------------

TEST_CASE ("a true-stereo four-channel file builds four paths, and a pair builds the same", "[verb][q28]")
{
    const auto ll = noise (20000, 61, 5000.0), lr = noise (20000, 62, 5000.0);
    const auto rl = noise (20000, 63, 5000.0), rr = noise (20000, 64, 5000.0);
    const auto fileRate = GENERATE (48000.0, 44100.0);

    auto four = fileModule (makeAudio ({ ll, lr, rl, rr }, fileRate), 3, false);
    four.trueStereo = true;

    auto pair = fileModule (makeAudio ({ ll, lr }, fileRate), 3, false);
    pair.trueStereo = true;
    pair.pairPath = "right.wav";
    pair.pairFile = makeAudio ({ rl, rr }, fileRate);

    verb::ChainSpec a, b;
    a.modules.push_back (four);
    b.modules.push_back (pair);

    const auto builtFour = verb::buildIr (a, rate);
    const auto builtPair = verb::buildIr (b, rate);

    REQUIRE (builtFour.audio.getNumChannels() == 4);
    CHECK (builtFour.preview.numChannels == 4);
    CHECK (sameAudio (builtFour.audio, builtPair.audio));

    if (fileRate == rate)
    {
        // Channel order in the build: LL, RR, then RL and LR, with the file's -18 dB trim.
        const auto gain = (float) std::pow (10.0, -18.0 / 20.0);

        CHECK (builtFour.audio.getSample (0, 100) == Catch::Approx (ll[100] * gain).margin (1.0e-6));
        CHECK (builtFour.audio.getSample (1, 100) == Catch::Approx (rr[100] * gain).margin (1.0e-6));
        CHECK (builtFour.audio.getSample (2, 100) == Catch::Approx (rl[100] * gain).margin (1.0e-6));
        CHECK (builtFour.audio.getSample (3, 100) == Catch::Approx (lr[100] * gain).margin (1.0e-6));
    }

    // Modules after the file treat all four paths alike.
    a.modules.push_back (moduleOf (ModuleType::reverse));
    a.modules.push_back (moduleOf (ModuleType::normalize));
    const auto processed = verb::buildIr (a, rate);

    REQUIRE (processed.audio.getNumChannels() == 4);
    CHECK (processed.audio.getSample (3, processed.audio.getNumSamples() - 101) != 0.0f);

    auto peak = 0.0f;

    for (int ch = 0; ch < 4; ++ch)
        peak = std::max (peak, processed.audio.getMagnitude (ch, 0, processed.audio.getNumSamples()));

    CHECK (peak == Catch::Approx (1.0f).margin (1.0e-5));

    // A generator added after a true-stereo file goes to the direct paths only.
    a.modules.push_back (moduleOf (ModuleType::echo));
    CHECK (verb::buildIr (a, rate).audio.getNumChannels() == 4);
}

TEST_CASE ("Mix to mono sums a true-stereo file to one direct IR", "[verb][q28]")
{
    const auto ll = noise (5000, 71), lr = noise (5000, 72), rl = noise (5000, 73), rr = noise (5000, 74);

    auto module = fileModule (makeAudio ({ ll, lr, rl, rr }, rate), 0, true);
    module.trueStereo = true;

    verb::ChainSpec chain;
    chain.modules.push_back (module);

    const auto built = verb::buildIr (chain, rate);

    REQUIRE (built.audio.getNumChannels() == 2);

    for (int i = 0; i < 5000; i += 97)
    {
        const auto expected = ((double) ll[(size_t) i] + lr[(size_t) i] + rl[(size_t) i] + rr[(size_t) i]) * 0.5;
        CHECK (built.audio.getSample (0, i) == Catch::Approx (expected).margin (1.0e-5));
        CHECK (built.audio.getSample (1, i) == built.audio.getSample (0, i));
    }
}

namespace
{
juce::File scratch (const juce::String& name)
{
    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("fancy-verb-q28").getChildFile (name);
    folder.deleteRecursively();
    folder.createDirectory();
    return folder;
}

bool writeWav (const juce::File& file, const std::vector<std::vector<float>>& channels)
{
    juce::AudioBuffer<float> audio ((int) channels.size(), (int) channels[0].size());

    for (size_t ch = 0; ch < channels.size(); ++ch)
        audio.copyFrom ((int) ch, 0, channels[ch].data(), (int) channels[ch].size());

    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);

    juce::WavAudioFormat format;
    const auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions {}
                                                            .withSampleRate (rate)
                                                            .withNumChannels ((int) channels.size())
                                                            .withBitsPerSample (24));

    return writer != nullptr && writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
}

int fileModuleUid (VerbProcessor& processor)
{
    return processor.addModule (ModuleType::file, 0);
}

double peakOf (const juce::AudioBuffer<float>& buffer, int channel)
{
    return buffer.getMagnitude (channel, 0, buffer.getNumSamples());
}

juce::AudioBuffer<float> renderLeftOnly (VerbProcessor& processor, int numSamples)
{
    juce::AudioBuffer<float> buffer (2, numSamples);
    buffer.clear();
    const auto input = noise (2000, 81);

    for (int i = 0; i < 2000; ++i)
        buffer.setSample (0, i, input[(size_t) i] * 0.5f);

    juce::MidiBuffer midi;

    for (int start = 0; start < numSamples; start += 512)
    {
        juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 2, start, std::min (512, numSamples - start));
        processor.processBlock (view, midi);
    }

    return buffer;
}
} // namespace

TEST_CASE ("choosing a four-channel file or a pair makes the File module true stereo", "[verb][q28]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto folder = scratch ("files");
    const std::vector<float> silent (12000, 0.0f);
    const auto lr = noise (12000, 91, 3000.0);

    const auto fourFile = folder.getChildFile ("four.wav");
    const auto leftFile = folder.getChildFile ("left.wav");
    const auto rightFile = folder.getChildFile ("right.wav");

    REQUIRE (writeWav (fourFile, { silent, lr, silent, silent }));
    REQUIRE (writeWav (leftFile, { silent, lr }));
    REQUIRE (writeWav (rightFile, { silent, silent }));

    VerbProcessor processor;
    const auto uid = fileModuleUid (processor);

    for (const auto pair : { false, true })
    {
        const juce::String label (pair ? "pair" : "four-channel file");
        INFO (label);

        if (pair)
        {
            REQUIRE (processor.loadFile (uid, leftFile));
            REQUIRE_FALSE ((bool) processor.getChain().getChildWithProperty (verb::ids::uid, uid).getProperty (verb::ids::trueStereo, false));
            REQUIRE (processor.loadPair (uid, rightFile));
        }
        else
        {
            REQUIRE (processor.loadFile (uid, fourFile));
        }

        const auto module = processor.getChain().getChildWithProperty (verb::ids::uid, uid);
        CHECK ((bool) module.getProperty (verb::ids::trueStereo, false));
        CHECK_FALSE ((bool) module.getProperty (verb::ids::mono, true));

        {
            INFO ("step: prepare and build");
            processor.setNonRealtime (true);
            processor.prepareToPlay (rate, 512);
            REQUIRE (processor.waitForBuild (20000));
            CHECK (processor.getPreview().numChannels == 4);
        }

        {
            // Only LR is set: the left input comes out on the right alone.
            INFO ("step: render");
            const auto out = renderLeftOnly (processor, 30000);
            CHECK (peakOf (out, 0) < 0.6); // the dry left input, at 0.5 peak
            CHECK (peakOf (out, 1) > 0.05);
        }

        {
            INFO ("step: state");
            juce::MemoryBlock state;
            processor.getStateInformation (state);

            VerbProcessor copy;
            copy.setStateInformation (state.getData(), (int) state.getSize());
            const auto copied = copy.getChain().getChildWithProperty (verb::ids::uid, uid);
            CHECK ((bool) copied.getProperty (verb::ids::trueStereo, false));
            CHECK (copied[verb::ids::pairPath].toString().isNotEmpty() == pair);
            REQUIRE (copy.waitForBuild (20000));
        }
    }

    processor.clearPair (uid);
    const auto cleared = processor.getChain().getChildWithProperty (verb::ids::uid, uid);
    CHECK_FALSE (cleared.hasProperty (verb::ids::trueStereo));
    CHECK_FALSE (cleared.hasProperty (verb::ids::pairPath));
}

// Push 2: the Reverb generator's new settings and the IR library --------------------------

namespace
{
ModuleSpec reverbModule (std::initializer_list<std::pair<const char*, float>> values, int seed = 7)
{
    auto module = moduleOf (ModuleType::reverb, seed);
    const auto& settings = verb::settingsOf (ModuleType::reverb);

    for (const auto& [id, value] : values)
        for (size_t i = 0; i < settings.size(); ++i)
            if (juce::String (settings[i].id) == id)
                module.values[i] = value;

    return module;
}

// RT60 in one octave band, from the Schroeder integral of the band-passed IR, by T20.
double bandRt60 (const float* ir, int length, double centreHz)
{
    // Two cascaded second-order band passes, about an octave wide.
    const auto w = 2.0 * std::numbers::pi * centreHz / rate;
    const auto alpha = std::sin (w) / (2.0 * 1.4);
    const auto a0 = 1.0 + alpha;
    const double b0 = alpha / a0, b2 = -alpha / a0, a1 = -2.0 * std::cos (w) / a0, a2 = (1.0 - alpha) / a0;

    std::vector<double> band ((size_t) length);
    double s1 = 0.0, s2 = 0.0, t1 = 0.0, t2 = 0.0;

    for (int i = 0; i < length; ++i)
    {
        const auto x = (double) ir[i];
        const auto y = b0 * x + s1;
        s1 = -a1 * y + s2;
        s2 = b2 * x - a2 * y;
        const auto z = b0 * y + t1;
        t1 = -a1 * z + t2;
        t2 = b2 * y - a2 * z;
        band[(size_t) i] = z;
    }

    std::vector<double> energy ((size_t) length + 1, 0.0);

    for (int i = length - 1; i >= 0; --i)
        energy[(size_t) i] = energy[(size_t) i + 1] + band[(size_t) i] * band[(size_t) i];

    const auto total = energy[0];
    int start = -1, end = -1;

    for (int i = 0; i < length; ++i)
    {
        const auto db = 10.0 * std::log10 (std::max (energy[(size_t) i] / total, 1.0e-30));

        if (start < 0 && db <= -5.0)
            start = i;

        if (end < 0 && db <= -25.0)
        {
            end = i;
            break;
        }
    }

    if (start < 0 || end <= start)
        return 0.0;

    return 3.0 * (double) (end - start) / rate; // 20 dB of decay, times three
}
} // namespace

TEST_CASE ("Low RT and High RT set the decay in their bands", "[verb][q28][generator]")
{
    verb::ChainSpec chain;
    chain.modules.push_back (reverbModule ({ { "length", 5.0f }, { "decay", 1.0f }, { "density", 100.0f }, { "damping", 0.0f },
                                             { "lowDecay", 2.0f }, { "highDecay", 0.5f }, { "buildUp", 0.0f } }));

    const auto built = verb::buildIr (chain, rate);
    const auto* left = built.audio.getReadPointer (0);
    const auto length = built.audio.getNumSamples();

    const auto low = bandRt60 (left, length, 125.0);
    const auto mid = bandRt60 (left, length, 1000.0);
    const auto high = bandRt60 (left, length, 8000.0);

    INFO ("RT60: 125 Hz " << low << " s, 1 kHz " << mid << " s, 8 kHz " << high << " s");
    CHECK (low == Catch::Approx (2.0).epsilon (0.1));
    CHECK (mid == Catch::Approx (1.0).epsilon (0.1));
    CHECK (high == Catch::Approx (0.5).epsilon (0.1));
}

TEST_CASE ("Early places the image-source reflections", "[verb][q28][generator]")
{
    // The floor reflection in a 10 m Room, worked out by hand: the box is 10 x 7.5 x 4.5 m, the
    // left source at (3, 3, 2.25), the left ear at (7, 3.66, 1.8); the floor image is at z = -2.25.
    const auto direct = std::sqrt (16.0 + 0.66 * 0.66 + 0.45 * 0.45);
    const auto floor = std::sqrt (16.0 + 0.66 * 0.66 + 4.05 * 4.05);
    const auto expected = (floor - direct) / 343.0;

    const auto pattern = verb::earlyPattern (verb::EarlyType::room, 10.0, 0, 0);
    REQUIRE (pattern.size() > 20);

    auto nearest = 1.0;

    for (const auto& reflection : pattern)
        nearest = std::min (nearest, std::abs (reflection.seconds - expected));

    CHECK (nearest * rate < 1.0);
    CHECK (verb::earlyPattern (verb::EarlyType::plate, 10.0, 0, 0).empty());
    CHECK (verb::earlyPattern (verb::EarlyType::hall, 10.0, 0, 0).size() < pattern.size());

    // The first reflection stands out in the built IR.
    verb::ChainSpec chain;
    chain.modules.push_back (reverbModule ({ { "length", 0.5f }, { "decay", 0.3f }, { "damping", 0.0f }, { "size", 10.0f },
                                             { "buildUp", 300.0f }, { "early", 1.0f } }));

    const auto built = verb::buildIr (chain, rate);
    const auto first = (int) std::llround (pattern.front().seconds * rate);
    const auto* left = built.audio.getReadPointer (0);

    auto background = 0.0;

    for (int i = 0; i < 2400; ++i)
        if (std::abs (i - first) > 2)
            background = std::max (background, (double) std::abs (left[i]) * (i < first ? 1.0 : 0.0));

    CHECK (std::abs (left[first]) > 4.0 * background);
}

TEST_CASE ("the generator's True stereo makes four decorrelated paths", "[verb][q28][generator]")
{
    verb::ChainSpec chain;
    chain.modules.push_back (reverbModule ({ { "length", 1.0f }, { "trueStereo", 1.0f } }));

    const auto built = verb::buildIr (chain, rate);
    REQUIRE (built.audio.getNumChannels() == 4);

    const auto correlation = [&built] (int a, int b)
    {
        auto ab = 0.0, aa = 0.0, bb = 0.0;

        for (int i = 0; i < built.audio.getNumSamples(); ++i)
        {
            const auto x = (double) built.audio.getSample (a, i), y = (double) built.audio.getSample (b, i);
            ab += x * y;
            aa += x * x;
            bb += y * y;
        }

        return ab / std::sqrt (aa * bb);
    };

    CHECK (built.audio.getRMSLevel (2, 0, built.audio.getNumSamples()) > 0.3f * built.audio.getRMSLevel (0, 0, built.audio.getNumSamples()));
    CHECK (std::abs (correlation (0, 2)) < 0.2);
    CHECK (std::abs (correlation (1, 3)) < 0.2);
}

TEST_CASE ("the IR library has 32 IRs at one loudness, made from the recipes", "[verb][q28][library]")
{
    const auto& library = verb::Library::builtIn();
    const auto& entries = library.getEntries();

    REQUIRE (entries.size() == 32);

    auto trueStereo = 0;

    for (const auto& entry : entries)
    {
        INFO (entry.key());
        CHECK (verb::Library::categories().contains (entry.category));

        const auto audio = library.load (entry.key());
        REQUIRE (audio != nullptr);
        CHECK (audio->audio.getNumChannels() == entry.numChannels);

        auto finite = true;

        for (int ch = 0; ch < audio->audio.getNumChannels(); ++ch)
            for (int i = 0; i < audio->audio.getNumSamples(); ++i)
                finite = finite && std::isfinite (audio->audio.getSample (ch, i));

        CHECK (finite);

        // Nothing clipped in the 24-bit FLAC.
        for (int ch = 0; ch < audio->audio.getNumChannels(); ++ch)
            CHECK (audio->audio.getMagnitude (ch, 0, audio->audio.getNumSamples()) < 0.999f);

        const auto loudnessDb = 10.0 * std::log10 (verb::outputEnergy (audio->audio, verb::libraryRate, verb::libraryLoudnessSeconds)
                                                   / verb::libraryLoudnessEnergy);
        CHECK (std::abs (loudnessDb) < 0.5);

        trueStereo += entry.isTrueStereo() ? 1 : 0;
    }

    CHECK (trueStereo >= 8);

    // The built-in data is what the recipes make, and making them is repeatable.
    const auto recipes = verb::parseRecipes (juce::File (FANCY_SOURCE_DIR "/plugins/verb/library/recipes.json").loadFileAsString());
    REQUIRE (recipes.size() == 32);

    for (const auto index : { 0, 10, 16, 31 })
    {
        const auto& recipe = recipes[(size_t) index];
        INFO (recipe.name);

        const auto once = verb::renderRecipe (recipe);
        const auto twice = verb::renderRecipe (recipe);
        CHECK (sameAudio (once, twice));

        const auto stored = library.load (recipe.category + "/" + recipe.name);
        REQUIRE (stored != nullptr);
        REQUIRE (stored->audio.getNumSamples() == once.getNumSamples());

        auto worst = 0.0;

        for (int ch = 0; ch < once.getNumChannels(); ++ch)
            for (int i = 0; i < once.getNumSamples(); ++i)
                worst = std::max (worst, std::abs ((double) once.getSample (ch, i) - stored->audio.getSample (ch, i)));

        CHECK (worst < 4.0 / 8388608.0); // within the 24-bit FLAC's step
    }
}

TEST_CASE ("a File module plays a library IR, true stereo where it is", "[verb][q28][library]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;
    const auto uid = processor.addModule (ModuleType::file, 0);

    CHECK_FALSE (processor.loadLibrary (uid, "Halls/No Such Hall"));
    REQUIRE (processor.loadLibrary (uid, "Halls/Concert Hall"));

    auto module = processor.getChain().getChildWithProperty (verb::ids::uid, uid);
    CHECK (module[verb::ids::factory].toString() == "Halls/Concert Hall");
    CHECK ((bool) module.getProperty (verb::ids::trueStereo, false));
    CHECK_FALSE (module.hasProperty (verb::ids::path));

    processor.setNonRealtime (true);
    processor.prepareToPlay (rate, 512);
    REQUIRE (processor.waitForBuild (20000));
    CHECK_FALSE (processor.isMissing (uid));
    CHECK (processor.getPreview().numChannels == 4);

    const auto out = renderLeftOnly (processor, 24000);
    CHECK (peakOf (out, 1) > 0.001); // the left input reaches the right output

    juce::MemoryBlock state;
    processor.getStateInformation (state);
    CHECK (state.getSize() < 64 * 1024); // a reference, not an embedded copy

    VerbProcessor copy;
    copy.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (copy.getChain().getChildWithProperty (verb::ids::uid, uid)[verb::ids::factory].toString() == "Halls/Concert Hall");
    REQUIRE (copy.waitForBuild (20000));

    // Choosing a file afterwards drops the library reference.
    REQUIRE (processor.loadLibrary (uid, "Rooms/Booth"));
    module = processor.getChain().getChildWithProperty (verb::ids::uid, uid);
    CHECK_FALSE ((bool) module.getProperty (verb::ids::trueStereo, false));
}

// Push 3: Modulate -------------------------------------------------------------------------

namespace
{
double correlationOf (const std::vector<double>& a, const std::vector<double>& b, size_t from)
{
    auto ab = 0.0, aa = 0.0, bb = 0.0;

    for (size_t i = from; i < a.size(); ++i)
    {
        ab += a[i] * b[i];
        aa += a[i] * a[i];
        bb += b[i] * b[i];
    }

    return ab / std::sqrt (aa * bb);
}

double blockEnergyDb (const std::vector<double>& x, size_t start, size_t count)
{
    auto sum = 0.0;

    for (size_t i = start; i < std::min (x.size(), start + count); ++i)
        sum += x[i] * x[i];

    return 10.0 * std::log10 (std::max (sum, 1.0e-30));
}

int addModulated (VerbProcessor& processor, float depth)
{
    const auto reverb = processor.addModule (ModuleType::reverb, 0);
    processor.setModuleValue (reverb, "length", 2.0f);
    processor.setModuleValue (reverb, "decay", 1.4f);

    const auto modulate = processor.addModule (ModuleType::modulate, 1);
    processor.setModuleValue (modulate, "depth", depth);
    return modulate;
}

juce::AudioBuffer<float> renderTone (VerbProcessor& processor, double hz, int numSamples)
{
    juce::AudioBuffer<float> buffer (2, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto sample = (float) (0.25 * std::sin (2.0 * std::numbers::pi * hz * i / rate));
        buffer.setSample (0, i, sample);
        buffer.setSample (1, i, sample);
    }

    juce::MidiBuffer midi;

    for (int start = 0; start < numSamples; start += 512)
    {
        juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 2, start, std::min (512, numSamples - start));
        processor.processBlock (view, midi);
    }

    return buffer;
}

double toneLevelDb (const float* samples, int from, int count, double hz)
{
    std::complex<double> sum = 0.0;
    auto windowSum = 0.0;

    for (int i = 0; i < count; ++i)
    {
        const auto window = 0.5 - 0.5 * std::cos (2.0 * std::numbers::pi * i / count);
        const auto phase = 2.0 * std::numbers::pi * hz * i / rate;
        sum += (double) samples[from + i] * window * std::complex<double> (std::cos (phase), -std::sin (phase));
        windowSum += window;
    }

    return 20.0 * std::log10 (std::max (2.0 * std::abs (sum) / windowSum, 1.0e-12));
}
} // namespace

TEST_CASE ("the decorrelated tail keeps the envelope and loses the fine structure", "[verb][q28][modulate]")
{
    const auto length = (int) (2.0 * rate);
    const auto source = noise (length, 111, 0.4 * rate);
    const std::vector<double> tail (source.begin(), source.end());

    const auto other = verb::decorrelate (tail, 5);
    REQUIRE (other.size() == tail.size());

    CHECK (std::abs (correlationOf (tail, other, 0)) < 0.2);

    // The energy envelope over 100 ms blocks, through most of the decay.
    const auto block = (size_t) (0.1 * rate);

    for (size_t start = block; start + block < (size_t) (1.4 * rate); start += block)
    {
        INFO ("block at " << (double) start / rate << " s");
        CHECK (std::abs (blockEnergyDb (tail, start, block) - blockEnergyDb (other, start, block)) < 1.0);
    }

    CHECK (verb::decorrelate (tail, 5) == other); // seeded, so repeatable
}

TEST_CASE ("Modulate splits the IR so early + tail is the whole", "[verb][q28][modulate]")
{
    verb::ChainSpec chain;
    chain.modules.push_back (reverbModule ({ { "length", 1.5f }, { "trueStereo", 1.0f } }));

    const auto plain = verb::buildIr (chain, rate);

    auto modulate = moduleOf (ModuleType::modulate);
    chain.modules.push_back (modulate);
    const auto split = verb::buildIr (chain, rate);

    REQUIRE (split.modulation.on);
    REQUIRE (split.audio.getNumChannels() == 4);
    REQUIRE (split.tailA.getNumChannels() == 4);
    CHECK (split.modulation.rateHz == Catch::Approx (0.3));
    CHECK (split.modulation.depth == Catch::Approx (0.5));

    const auto start = (int) (0.08 * rate);

    for (int ch = 0; ch < 4; ++ch)
    {
        for (int i = 0; i < plain.audio.getNumSamples(); i += 37)
            REQUIRE (split.audio.getSample (ch, i) + split.tailA.getSample (ch, i) == Catch::Approx (plain.audio.getSample (ch, i)).margin (1.0e-6));

        // The early part is untouched, and the tails start at Start.
        CHECK (split.tailB.getMagnitude (ch, 0, start) == 0.0f);
        CHECK (split.audio.getMagnitude (ch, start + (int) (0.011 * rate), plain.audio.getNumSamples() - start - (int) (0.011 * rate)) == 0.0f);
    }

    // The preview shows the whole IR.
    CHECK (split.preview.columns.size() == plain.preview.columns.size());
}

TEST_CASE ("Modulate at depth 0 sounds like no Modulate, and its renders repeat", "[verb][q28][modulate]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto input = noise (48000, 121);
    std::vector<juce::AudioBuffer<float>> outputs;

    // No Modulate, depth 0, depth 100, and depth 100 again in a fresh instance.
    for (const auto variant : { 0, 1, 2, 3 })
    {
        VerbProcessor processor;
        const auto reverb = processor.addModule (ModuleType::reverb, 0);
        processor.setModuleValue (reverb, "length", 2.0f);

        if (variant > 0)
        {
            const auto modulate = processor.addModule (ModuleType::modulate, 1);
            processor.setModuleValue (modulate, "depth", variant == 1 ? 0.0f : 100.0f);
        }

        processor.setNonRealtime (true);
        processor.prepareToPlay (rate, 512);
        REQUIRE (processor.waitForBuild (20000));

        // Past the start-up fade-in first, so the burst plays at full level.
        renderLeftOnly (processor, 48000);
        outputs.push_back (renderLeftOnly (processor, 96000));
    }

    CHECK (sameAudio (outputs[2], outputs[3]));

    // Depth 0: the early part and the unmoved tail add up to the whole, within float rounding.
    auto worst = 0.0, peak = 0.0;

    for (int ch = 0; ch < 2; ++ch)
    {
        for (int i = 0; i < 96000; ++i)
        {
            worst = std::max (worst, (double) std::abs (outputs[1].getSample (ch, i) - outputs[0].getSample (ch, i)));
            peak = std::max (peak, (double) std::abs (outputs[0].getSample (ch, i)));
        }
    }

    INFO ("peak " << peak << ", depth 0 against none " << worst);
    CHECK (peak > 0.1);
    CHECK (worst < peak * 1.0e-4);

    // Depth 100: the tail moves. The reverb isn't true stereo, so the left input's tail is on the left.
    auto moved = 0.0;

    for (int i = 0; i < 96000; ++i)
        moved = std::max (moved, (double) std::abs (outputs[2].getSample (0, i) - outputs[0].getSample (0, i)));

    INFO ("depth 100 against none " << moved);
    CHECK (moved > peak * 1.0e-3);
}

TEST_CASE ("Modulate doesn't change pitch", "[verb][q28][modulate]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;
    addModulated (processor, 100.0f);

    processor.setNonRealtime (true);
    processor.prepareToPlay (rate, 512);
    REQUIRE (processor.waitForBuild (20000));

    const auto length = (int) (12.0 * rate);
    const auto out = renderTone (processor, 1000.0, length);

    // Eight seconds of the steady state: everything beyond 2 Hz from the tone stays 60 dB down.
    const auto from = (int) (4.0 * rate), count = (int) (8.0 * rate);
    const auto* left = out.getReadPointer (0);
    const auto tone = toneLevelDb (left, from, count, 1000.0);

    for (const auto offset : { 3.0, 5.0, 10.0, 20.0, 50.0 })
    {
        INFO ("offset " << offset << " Hz");
        CHECK (toneLevelDb (left, from, count, 1000.0 + offset) < tone - 60.0);
        CHECK (toneLevelDb (left, from, count, 1000.0 - offset) < tone - 60.0);
    }
}

TEST_CASE ("Modulate's Rate and Depth act without a rebuild, and one Modulate is the limit", "[verb][q28][modulate]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;
    const auto modulate = addModulated (processor, 50.0f);

    processor.setNonRealtime (true);
    processor.prepareToPlay (rate, 512);
    REQUIRE (processor.waitForBuild (20000));

    const auto revision = processor.getBuildRevision();

    processor.setModuleValue (modulate, "rate", 1.2f);
    processor.setModuleValue (modulate, "depth", 80.0f);
    processor.setModuleValue (modulate, "spread", 20.0f);
    REQUIRE (processor.waitForBuild (20000));
    CHECK (processor.getBuildRevision() == revision);

    processor.setModuleValue (modulate, "start", 200.0f);
    REQUIRE (processor.waitForBuild (20000));
    CHECK (processor.getBuildRevision() != revision);

    CHECK (processor.addModule (ModuleType::modulate, 0) == 0);

    // The state keeps the settings.
    juce::MemoryBlock state;
    processor.getStateInformation (state);

    VerbProcessor copy;
    copy.setStateInformation (state.getData(), (int) state.getSize());
    const auto restored = copy.getChain().getChildWithProperty (verb::ids::uid, modulate);
    CHECK ((float) restored.getProperty ("rate") == Catch::Approx (1.2f));
    CHECK ((float) restored.getProperty ("depth") == Catch::Approx (80.0f));
    REQUIRE (copy.waitForBuild (20000));
}

TEST_CASE ("the Q2.8 factory presets build", "[verb][q28][presets]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;
    auto& presets = processor.getPresets();
    const auto names = presets.getNames();

    for (const auto* name : { "Library Hall", "Library Plate", "True Stereo Room", "Modulated Hall", "Endless" })
    {
        INFO (name);
        const auto index = names.indexOf (name);
        REQUIRE (index >= 0);

        presets.load (index);
        REQUIRE (processor.waitForBuild (60000));
        CHECK (processor.getPreview().peakDb > -80.0);
        CHECK (processor.getPreview().numChannels == 4);

        for (const auto module : processor.getChain())
            CHECK_FALSE (processor.isMissing ((int) module[verb::ids::uid]));
    }
}

// 8. CPU (section 0.4) ----------------------------------------------------------

TEST_CASE ("verb Q2.8 true stereo CPU", "[.measure]")
{
    constexpr int blockSize = 128;
    constexpr int numBlocks = 7500; // 20 s
    constexpr int runs = 5;
    const auto irLength = (int) (3.0 * rate);

    const auto ll = noise (irLength, 101, irLength / 4.0), rr = noise (irLength, 102, irLength / 4.0);
    const auto rl = noise (irLength, 103, irLength / 4.0), lr = noise (irLength, 104, irLength / 4.0);

    for (const auto mode : { Convolver::Mode::zeroLatency, Convolver::Mode::lowCpu })
    {
        double results[2] {};

        for (int trueStereo = 0; trueStereo < 2; ++trueStereo)
        {
            std::vector<double> times;

            for (int run = 0; run < runs; ++run)
            {
                Convolver convolver (ll.data(), rr.data(), trueStereo ? rl.data() : nullptr, trueStereo ? lr.data() : nullptr,
                                     irLength, mode, 32768);

                const auto input = noise (blockSize, 105);
                std::vector<float> left (blockSize), right (blockSize);
                const auto start = std::chrono::steady_clock::now();

                for (int i = 0; i < numBlocks; ++i)
                    convolver.process (input.data(), input.data(), left.data(), right.data(), blockSize, true);

                const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
                times.push_back (100.0 * elapsed.count() / ((double) (numBlocks * blockSize) / rate));
            }

            std::sort (times.begin(), times.end());
            results[trueStereo] = times[runs / 2];
        }

        std::printf ("verb q28 cpu, 3 s IR, %s (median of %d, 48 kHz, 128 samples, process time incl. waits): direct %.3f %%, true stereo %.3f %% (%.2f x, target < 1.6 x)\n",
                     mode == Convolver::Mode::zeroLatency ? "Zero latency" : "Low CPU", runs, results[0], results[1], results[1] / results[0]);
    }

    // Modulate: the early part (to Start, 80 ms, plus a 10 ms fade) and two tails.
    const auto start = (int) (0.09 * rate);
    const std::vector<float> early (ll.begin(), ll.begin() + start), earlyRight (rr.begin(), rr.begin() + start);
    std::vector<float> tailLeft (ll), tailRight (rr);
    std::fill (tailLeft.begin(), tailLeft.begin() + start, 0.0f);
    std::fill (tailRight.begin(), tailRight.begin() + start, 0.0f);

    for (const auto mode : { Convolver::Mode::zeroLatency, Convolver::Mode::lowCpu })
    {
        double results[2] {};

        for (int modulated = 0; modulated < 2; ++modulated)
        {
            std::vector<double> times;

            for (int run = 0; run < runs; ++run)
            {
                std::unique_ptr<Convolver> convolver;

                if (modulated == 0)
                {
                    convolver = std::make_unique<Convolver> (ll.data(), rr.data(), irLength, mode, 32768);
                }
                else
                {
                    convolver = std::make_unique<Convolver> (early.data(), earlyRight.data(), start, mode, 32768);
                    convolver->attachTails (std::make_unique<Convolver> (tailLeft.data(), tailRight.data(), irLength, mode, 32768),
                                            std::make_unique<Convolver> (tailRight.data(), tailLeft.data(), irLength, mode, 32768),
                                            rate, 3, 0.3, 0.5, 1.0);
                }

                const auto input = noise (blockSize, 106);
                std::vector<float> left (blockSize), right (blockSize);
                const auto begin = std::chrono::steady_clock::now();

                for (int i = 0; i < numBlocks; ++i)
                    convolver->process (input.data(), input.data(), left.data(), right.data(), blockSize, true);

                const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - begin;
                times.push_back (100.0 * elapsed.count() / ((double) (numBlocks * blockSize) / rate));
            }

            std::sort (times.begin(), times.end());
            results[modulated] = times[runs / 2];
        }

        std::printf ("verb q28 cpu, 3 s IR, %s (median of %d): direct %.3f %%, modulated %.3f %% (%.2f x, target < 1.8 x)\n",
                     mode == Convolver::Mode::zeroLatency ? "Zero latency" : "Low CPU", runs, results[0], results[1], results[1] / results[0]);
    }
}
