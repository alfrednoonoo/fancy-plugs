// Phase 3 Q2.8 for Fancy Verb, push 1: true stereo (docs/verb.md, "Phase 3 Q2.8", section 8).

#include "Convolver.h"
#include "IrBuilder.h"
#include "IrFiles.h"
#include "LegacyVerbConvolver.h"
#include "LegacyVerbIrBuilder.h"
#include "VerbChain.h"
#include "VerbProcessor.h"

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

        processor.setNonRealtime (true);
        processor.prepareToPlay (rate, 512);
        REQUIRE (processor.waitForBuild (20000));

        // Only LR is set: the left input comes out on the right alone.
        const auto out = renderLeftOnly (processor, 30000);
        CHECK (peakOf (out, 0) < 0.6); // the dry left input, at 0.5 peak
        CHECK (peakOf (out, 1) > 0.05);

        // The state keeps it.
        juce::MemoryBlock state;
        processor.getStateInformation (state);

        VerbProcessor copy;
        copy.setStateInformation (state.getData(), (int) state.getSize());
        const auto copied = copy.getChain().getChildWithProperty (verb::ids::uid, uid);
        CHECK ((bool) copied.getProperty (verb::ids::trueStereo, false));
        CHECK (copied[verb::ids::pairPath].toString().isNotEmpty() == pair);
    }

    processor.clearPair (uid);
    const auto cleared = processor.getChain().getChildWithProperty (verb::ids::uid, uid);
    CHECK_FALSE (cleared.hasProperty (verb::ids::trueStereo));
    CHECK_FALSE (cleared.hasProperty (verb::ids::pairPath));
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
}
