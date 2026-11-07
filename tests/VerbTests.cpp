#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <immintrin.h>

#include "Convolver.h"
#include "IrBuilder.h"
#include "IrFiles.h"
#include "VerbChain.h"
#include "VerbEditor.h"
#include "VerbProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numbers>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = std::numbers::pi;

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

ModuleSpec moduleOf (ModuleType type)
{
    ModuleSpec module;
    module.type = type;
    module.seed = 11;

    const auto& settings = verb::settingsOf (type);

    for (size_t i = 0; i < settings.size(); ++i)
        module.values[i] = settings[i].defaultValue;

    return module;
}

ModuleSpec fileModule (std::shared_ptr<const verb::IrAudio> audio, int gainMode, bool mono = false)
{
    auto module = moduleOf (ModuleType::file);
    module.file = std::move (audio);
    module.gainMode = gainMode;
    module.mono = mono;
    return module;
}

std::shared_ptr<verb::IrAudio> makeAudio (const std::vector<float>& left, const std::vector<float>& right, double sampleRate)
{
    auto audio = std::make_shared<verb::IrAudio>();
    audio->sampleRate = sampleRate;
    audio->audio.setSize (2, (int) left.size());
    audio->audio.copyFrom (0, 0, left.data(), (int) left.size());
    audio->audio.copyFrom (1, 0, right.data(), (int) right.size());
    return audio;
}

void setParameter (fancy::FancyProcessor& processor, const char* id, float value)
{
    auto& parameter = *processor.getApvts().getParameter (id);
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
}

void prepareOffline (VerbProcessor& processor, double sampleRate = rate, int blockSize = 512)
{
    processor.setNonRealtime (true);
    processor.prepareToPlay (sampleRate, blockSize);
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
    if (a.getNumSamples() != b.getNumSamples())
        return false;

    for (int ch = 0; ch < 2; ++ch)
        if (! std::equal (a.getReadPointer (ch), a.getReadPointer (ch) + a.getNumSamples(), b.getReadPointer (ch)))
            return false;

    return true;
}

void addReverb (VerbProcessor& processor, float length = 0.5f, float decay = 0.3f)
{
    const auto uid = processor.addModule (ModuleType::reverb, 0);
    processor.setModuleValue (uid, "length", length);
    processor.setModuleValue (uid, "decay", decay);
}

double maxJump (const float* samples, int length)
{
    auto jump = 0.0;

    for (int i = 1; i < length; ++i)
        jump = std::max (jump, (double) std::abs (samples[i] - samples[i - 1]));

    return jump;
}

bool allFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getSample (ch, i)))
                return false;

    return true;
}

double secondsOf (const FILETIME& time)
{
    ULARGE_INTEGER value;
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return (double) value.QuadPart * 1.0e-7;
}

double processCpuSeconds()
{
    FILETIME created, exited, kernel, user;
    GetProcessTimes (GetCurrentProcess(), &created, &exited, &kernel, &user);
    return secondsOf (kernel) + secondsOf (user);
}

double threadCpuSeconds()
{
    FILETIME created, exited, kernel, user;
    GetThreadTimes (GetCurrentThread(), &created, &exited, &kernel, &user);
    return secondsOf (kernel) + secondsOf (user);
}

juce::File scratchFolder (const juce::String& name)
{
    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("fancy-verb-tests").getChildFile (name);
    folder.deleteRecursively();
    folder.createDirectory();
    return folder;
}

bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& audio, double sampleRate)
{
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);

    juce::WavAudioFormat format;
    const auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions {}
                                                            .withSampleRate (sampleRate)
                                                            .withNumChannels (audio.getNumChannels())
                                                            .withBitsPerSample (24));

    return writer != nullptr && writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
}
} // namespace

// Convolver ---------------------------------------------------------------------------------

TEST_CASE ("convolver matches direct convolution", "[verb][convolver]")
{
    const auto mode = GENERATE (Convolver::Mode::zeroLatency, Convolver::Mode::lowCpu);
    const auto irLength = GENERATE (1, 100, 128, 129, 3000, 20000, 130000);
    const auto blockSize = GENERATE (1, 7, 128, 1000, 8192);
    const auto maxBlock = GENERATE (8192, 32768);

    if (blockSize == 1 && irLength > 3000)
        return; // a sample at a time is slow, and long IRs get covered by the other sizes

    const auto irLeft = noise (irLength, 1, irLength / 3.0);
    const auto irRight = noise (irLength, 2, irLength / 3.0);

    constexpr int burst = 1500;
    const auto length = std::min (irLength + burst + 8192, 150000);

    auto inputLeft = noise (burst, 3);
    auto inputRight = noise (burst, 4);
    inputLeft.resize ((size_t) length, 0.0f);
    inputRight.resize ((size_t) length, 0.0f);

    Convolver convolver (irLeft.data(), irRight.data(), irLength, mode, maxBlock);

    std::vector<float> left ((size_t) length, 0.0f), right ((size_t) length, 0.0f);

    for (int start = 0; start < length; start += blockSize)
    {
        const auto count = std::min (blockSize, length - start);
        convolver.process (inputLeft.data() + start, inputRight.data() + start,
                           left.data() + start, right.data() + start, count, true);
    }

    const auto delay = convolver.getLatency();
    auto expectedLeft = direct (inputLeft, irLeft, length);
    auto expectedRight = direct (inputRight, irRight, length);

    auto worst = 0.0;

    for (int i = delay; i < length; ++i)
    {
        worst = std::max (worst, std::abs ((double) left[(size_t) i] - expectedLeft[(size_t) (i - delay)]));
        worst = std::max (worst, std::abs ((double) right[(size_t) i] - expectedRight[(size_t) (i - delay)]));
    }

    for (int i = 0; i < delay; ++i)
        worst = std::max (worst, (double) std::abs (left[(size_t) i]));

    CAPTURE (irLength, blockSize, maxBlock);
    CHECK (worst < 2.0e-2);
    CHECK (convolver.getLateBlocks() == 0);
}

TEST_CASE ("convolver reset clears everything", "[verb][convolver]")
{
    const auto mode = GENERATE (Convolver::Mode::zeroLatency, Convolver::Mode::lowCpu);

    const auto ir = noise (100000, 5, 30000.0);
    const auto input = noise (30000, 6);

    Convolver convolver (ir.data(), ir.data(), (int) ir.size(), mode, 32768);

    std::vector<float> left (input.size(), 0.0f), right (input.size(), 0.0f);
    convolver.process (input.data(), input.data(), left.data(), right.data(), (int) input.size(), true);

    convolver.reset();

    const std::vector<float> silence (100000, 0.0f);
    std::vector<float> outLeft (silence.size(), 0.0f), outRight (silence.size(), 0.0f);

    convolver.process (silence.data(), silence.data(), outLeft.data(), outRight.data(), (int) silence.size(), true);

    for (const auto sample : outLeft)
        REQUIRE (sample == 0.0f);
}

// Resampling --------------------------------------------------------------------------------

TEST_CASE ("resampler is flat, with images removed", "[verb][resample]")
{
    const auto ratio = GENERATE (48000.0 / 44100.0, 44100.0 / 48000.0, 2.0, 0.5, 4.0);

    constexpr int length = 96000;
    const auto lowerNyquist = 0.5 * std::min (1.0, ratio);

    for (const auto fraction : { 0.05, 0.3, 0.6, 0.85 })
    {
        const auto frequency = fraction * lowerNyquist; // cycles per source sample

        std::vector<double> input ((size_t) length);

        for (int i = 0; i < length; ++i)
            input[(size_t) i] = std::sin (2.0 * pi * frequency * i);

        const auto output = verb::resample (input.data(), length, ratio);

        // Past the filter's start-up and tail.
        const auto margin = (int) (1000 * std::max (1.0, ratio));
        auto energy = 0.0, reference = 0.0;

        for (int i = margin; i < (int) output.size() - margin; ++i)
        {
            const auto wanted = std::sin (2.0 * pi * frequency * i / ratio);
            const auto error = output[(size_t) i] - wanted;
            energy += error * error;
            reference += wanted * wanted;
        }

        CAPTURE (ratio, fraction);
        CHECK (10.0 * std::log10 (energy / reference) < -60.0);
    }

    // A tone above the new Nyquist has to vanish when downsampling.
    if (ratio < 1.0)
    {
        const auto frequency = 0.5 * ratio * 1.1;

        std::vector<double> input ((size_t) length);

        for (int i = 0; i < length; ++i)
            input[(size_t) i] = std::sin (2.0 * pi * frequency * i);

        const auto output = verb::resample (input.data(), length, ratio);

        auto peak = 0.0;

        for (int i = 2000; i < (int) output.size() - 2000; ++i)
            peak = std::max (peak, std::abs (output[(size_t) i]));

        CHECK (20.0 * std::log10 (peak) < -100.0);
    }
}

// Modules -----------------------------------------------------------------------------------

TEST_CASE ("file module applies SRC and trim gain", "[verb][modules]")
{
    const std::vector<float> impulse (4800, 0.0f);
    auto left = impulse, right = impulse;
    left[0] = right[0] = 1.0f;

    verb::ChainSpec chain;
    chain.modules.push_back (fileModule (makeAudio (left, right, 48000.0), 3));

    SECTION ("no SRC gain, -18 dB, at the file's rate")
    {
        const auto built = verb::buildIr (chain, 48000.0);
        CHECK (built.audio.getSample (0, 0) == Catch::Approx (std::pow (10.0, -18.0 / 20.0)).margin (1.0e-5));
        CHECK (built.audio.getNumSamples() == 4800);
    }

    SECTION ("SRC gain keeps the sum of the samples")
    {
        // Away from sample 0, so none of the resampling filter's response is cut off.
        left[0] = right[0] = 0.0f;
        left[1000] = right[1000] = 1.0f;
        chain.modules[0].file = makeAudio (left, right, 48000.0);
        chain.modules[0].gainMode = 4;

        const auto built = verb::buildIr (chain, 96000.0);

        auto sum = 0.0;

        for (int i = 0; i < built.audio.getNumSamples(); ++i)
            sum += built.audio.getSample (0, i);

        // A convolution's level follows the sum, so this one is as loud as at 48 kHz.
        CHECK (sum == Catch::Approx (1.0).margin (0.01));
    }
}

TEST_CASE ("echo generator places its echoes", "[verb][modules]")
{
    auto echo = moduleOf (ModuleType::echo);
    echo.values = { 10.0f, 30.0f, 10.0f, 0.0f, 0.5f, -6.0f, -18.0f };

    verb::ChainSpec chain;
    chain.modules.push_back (echo);

    const auto built = verb::buildIr (chain, rate);

    REQUIRE (built.preview.echoSeconds.size() == 4);
    CHECK (built.preview.numChannels == 1);

    const std::pair<int, double> expected[] { { 480, -6.0 }, { 960, -10.0 }, { 1440, -14.0 }, { 1920, -18.0 } };

    for (const auto& [position, db] : expected)
        CHECK (built.audio.getSample (0, position) == Catch::Approx (std::pow (10.0, db / 20.0)).margin (1.0e-5));

    auto count = 0;

    for (int i = 0; i < built.audio.getNumSamples(); ++i)
        count += built.audio.getSample (1, i) != 0.0f ? 1 : 0;

    CHECK (count == 4);
}

TEST_CASE ("echo error is seeded and stays in range", "[verb][modules]")
{
    auto echo = moduleOf (ModuleType::echo);
    echo.values = { 50.0f, 1000.0f, 50.0f, 20.0f, 0.0f, -6.0f, -6.0f };

    verb::ChainSpec chain;
    chain.modules.push_back (echo);

    const auto first = verb::buildIr (chain, rate);
    const auto second = verb::buildIr (chain, rate);

    REQUIRE (first.audio.getNumSamples() == second.audio.getNumSamples());
    CHECK (std::equal (first.audio.getReadPointer (0), first.audio.getReadPointer (0) + first.audio.getNumSamples(), second.audio.getReadPointer (0)));
    CHECK (first.preview.numChannels == 2);

    for (int i = 0; i < first.audio.getNumSamples(); ++i)
    {
        if (first.audio.getSample (0, i) == 0.0f)
            continue;

        const auto ms = 1000.0 * i / rate;
        const auto distance = std::abs (std::remainder (ms, 50.0));

        CHECK (distance <= 20.01);
    }
}

TEST_CASE ("lowpass and highpass shape the IR", "[verb][modules]")
{
    auto echo = moduleOf (ModuleType::echo);
    echo.values = { 0.0f, 0.0f, 100.0f, 0.0f, 0.5f, 0.0f, 0.0f };

    auto filter = moduleOf (ModuleType::lpHp);
    filter.values[0] = 1000.0f;
    filter.values[1] = 20.0f;

    // A quiet echo first makes the buffer long enough to hold the filter's tail.
    auto ring = moduleOf (ModuleType::echo);
    ring.values = { 500.0f, 0.0f, 100.0f, 0.0f, 0.5f, -60.0f, -60.0f };

    verb::ChainSpec chain;
    chain.modules = { ring, echo, filter };

    const auto built = verb::buildIr (chain, rate);
    const auto* samples = built.audio.getReadPointer (0);

    const auto magnitudeAt = [&] (double hz)
    {
        auto real = 0.0, imaginary = 0.0;

        for (int i = 0; i < built.audio.getNumSamples(); ++i)
        {
            real += samples[i] * std::cos (2.0 * pi * hz * i / rate);
            imaginary += samples[i] * std::sin (2.0 * pi * hz * i / rate);
        }

        return 20.0 * std::log10 (std::hypot (real, imaginary));
    };

    CHECK (magnitudeAt (1000.0) == Catch::Approx (-3.0).margin (0.5));
    CHECK (magnitudeAt (100.0) == Catch::Approx (0.0).margin (0.5));
    CHECK (magnitudeAt (4000.0) < -40.0);
}

TEST_CASE ("normalize, reverse and trim", "[verb][modules]")
{
    auto echo = moduleOf (ModuleType::echo);
    echo.values = { 10.0f, 0.0f, 100.0f, 0.0f, 0.5f, -20.0f, -20.0f };

    auto longer = moduleOf (ModuleType::echo);
    longer.values = { 100.0f, 0.0f, 100.0f, 0.0f, 0.5f, -40.0f, -40.0f };

    verb::ChainSpec chain;
    chain.modules = { echo, longer };

    SECTION ("normalize sets the peak")
    {
        auto normalize = moduleOf (ModuleType::normalize);
        normalize.values[0] = -6.0f;
        chain.modules.push_back (normalize);

        const auto built = verb::buildIr (chain, rate);
        CHECK (built.preview.peakDb == Catch::Approx (-6.0).margin (0.01));
        CHECK (built.audio.getSample (0, 480) == Catch::Approx (std::pow (10.0, -6.0 / 20.0)).margin (1.0e-5));
    }

    SECTION ("reverse flips the buffer")
    {
        chain.modules.push_back (moduleOf (ModuleType::reverse));

        const auto built = verb::buildIr (chain, rate);
        const auto length = built.audio.getNumSamples();

        REQUIRE (length == 4801);
        CHECK (built.audio.getSample (0, 0) == Catch::Approx (0.01).margin (1.0e-5));
        CHECK (built.audio.getSample (0, length - 1 - 480) == Catch::Approx (0.1).margin (1.0e-5));
    }

    SECTION ("trim cuts, fades and applies gain")
    {
        auto trim = moduleOf (ModuleType::trim);
        trim.values = { 5.0f, 0.0f, 100.0f, 0.0f, 0.0f, 6.0f, 0.0f };
        chain.modules.push_back (trim);

        const auto built = verb::buildIr (chain, rate);

        CHECK (built.audio.getNumSamples() == 4801 - 240);
        CHECK (built.audio.getSample (0, 240) == Catch::Approx (0.1 * std::pow (10.0, 6.0 / 20.0)).margin (1.0e-5));
    }

    SECTION ("stretch changes the length")
    {
        auto trim = moduleOf (ModuleType::trim);
        trim.values = { 0.0f, 0.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        chain.modules.push_back (trim);

        CHECK (verb::buildIr (chain, rate).audio.getNumSamples() == 9602);
    }

    SECTION ("the display handles cut last and leave the preview whole")
    {
        chain.trimStart = 0.05;
        chain.trimEnd = 0.09;

        const auto built = verb::buildIr (chain, rate);

        CHECK (built.audio.getNumSamples() == 1920);
        CHECK (built.preview.fullSeconds == Catch::Approx (4801.0 / rate));
        CHECK (built.preview.trimmedSeconds == Catch::Approx (0.04));
    }

    SECTION ("a module that's off is skipped")
    {
        auto normalize = moduleOf (ModuleType::normalize);
        normalize.on = false;
        chain.modules.push_back (normalize);

        CHECK (verb::buildIr (chain, rate).preview.peakDb == Catch::Approx (-20.0).margin (0.01));
    }
}

TEST_CASE ("reverb generator is repeatable and decays at its RT60", "[verb][modules]")
{
    auto reverb = moduleOf (ModuleType::reverb);
    reverb.values = { 2.0f, 0.6f, 100.0f, 0.0f, 100.0f, 0.0f, 0.0f };

    verb::ChainSpec chain;
    chain.modules.push_back (reverb);

    const auto first = verb::buildIr (chain, rate);
    const auto second = verb::buildIr (chain, rate);

    REQUIRE (first.audio.getNumSamples() == 96000);
    CHECK (std::equal (first.audio.getReadPointer (0), first.audio.getReadPointer (0) + 96000, second.audio.getReadPointer (0)));
    CHECK (first.preview.numChannels == 2);

    const auto energyDb = [&] (double startSeconds)
    {
        const auto begin = (int) (startSeconds * rate);
        auto energy = 0.0;

        for (int i = begin; i < begin + 2400; ++i)
            energy += first.audio.getSample (0, i) * first.audio.getSample (0, i);

        return 10.0 * std::log10 (energy);
    };

    // 60 dB in 0.6 s is 30 dB in 0.3 s.
    CHECK (energyDb (0.2) - energyDb (0.5) == Catch::Approx (30.0).margin (3.0));

    chain.modules[0].seed = 12;
    const auto other = verb::buildIr (chain, rate);

    CHECK (! std::equal (first.audio.getReadPointer (0), first.audio.getReadPointer (0) + 96000, other.audio.getReadPointer (0)));

    chain.modules[0].seed = 11;
    chain.modules[0].values[4] = 0.0f;

    CHECK (verb::buildIr (chain, rate).preview.numChannels == 1);
}

// Plugin ------------------------------------------------------------------------------------

TEST_CASE ("a new instance passes audio unchanged", "[verb]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;
    prepareOffline (processor);

    const auto input = noise (48000, 7);
    const auto output = render (processor, input, 512);

    for (int i = 0; i < output.getNumSamples(); ++i)
        REQUIRE (output.getSample (0, i) == Catch::Approx (input[(size_t) i]).margin (1.0e-5));
}

TEST_CASE ("reported latency matches, and dry stays aligned", "[verb]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto lowCpu = GENERATE (false, true);
    const auto sampleRate = GENERATE (44100.0, 48000.0, 192000.0);

    for (int load = 0; load < 2; ++load)
    {
        VerbProcessor processor;
        setParameter (processor, "latency", lowCpu ? 1.0f : 0.0f);
        setParameter (processor, "wet", -60.0f);
        prepareOffline (processor, sampleRate);

        const auto expected = lowCpu ? Convolver::lowCpuLatency : 0;

        CHECK (processor.getLatencySamples() == expected);

        std::vector<float> input (20000, 0.0f);
        input[1000] = 1.0f;

        // The first block lets the new convolver land, because the mute is part of the swap.
        render (processor, std::vector<float> (24000, 0.0f), 512);

        const auto output = render (processor, input, 512);

        auto peak = 0;

        for (int i = 0; i < output.getNumSamples(); ++i)
            if (std::abs (output.getSample (0, i)) > std::abs (output.getSample (0, peak)))
                peak = i;

        CHECK (peak == 1000 + expected);
    }
}

TEST_CASE ("wet level matches the chain's IR", "[verb]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto lowCpu = GENERATE (false, true);

    VerbProcessor processor;
    setParameter (processor, "latency", lowCpu ? 1.0f : 0.0f);
    setParameter (processor, "wet", 0.0f);
    setParameter (processor, "dry", -60.0f);

    const auto uid = processor.addModule (ModuleType::echo, 0);

    for (const auto& [id, value] : { std::pair<const char*, float> { "start", 100.0f }, { "length", 0.0f }, { "startVol", -6.0f } })
        processor.setModuleValue (uid, id, value);

    prepareOffline (processor);
    render (processor, std::vector<float> (24000, 0.0f), 256);

    std::vector<float> input (20000, 0.0f);
    input[0] = 1.0f;

    const auto output = render (processor, input, 256);
    const auto position = 4800 + (lowCpu ? Convolver::lowCpuLatency : 0);

    CHECK (output.getSample (0, position) == Catch::Approx (std::pow (10.0, -6.0 / 20.0)).margin (1.0e-3));
}

TEST_CASE ("offline renders are bit-identical", "[verb]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto makeRender = []
    {
        VerbProcessor processor;
        addReverb (processor);
        setParameter (processor, "predelay", 23.4f);
        prepareOffline (processor);
        return render (processor, noise (96000, 8), 333);
    };

    CHECK (identical (makeRender(), makeRender()));
}

TEST_CASE ("verb automation with an unchanged value is bit-identical", "[verb]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor plain, automated;

    for (auto* processor : { &plain, &automated })
    {
        addReverb (*processor);
        setParameter (*processor, "wet", -7.7f);
        prepareOffline (*processor);
    }

    for (auto* parameter : automated.getParameters())
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->getValue());
        parameter->endChangeGesture();
    }

    REQUIRE (automated.waitForBuild (20000));

    const auto input = noise (48000, 9);

    CHECK (identical (render (plain, input, 512), render (automated, input, 512)));
}

TEST_CASE ("changing the chain during playback doesn't click", "[verb][swap]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;
    addReverb (processor);
    setParameter (processor, "wet", 0.0f);
    setParameter (processor, "dry", -60.0f);
    prepareOffline (processor);

    juce::AudioBuffer<float> before;

    std::vector<float> sine (96000);

    for (size_t i = 0; i < sine.size(); ++i)
        sine[i] = 0.25f * (float) std::sin (2.0 * pi * 300.0 * (double) i / rate);

    const auto settled = render (processor, sine, 128);
    const auto steadyJump = maxJump (settled.getReadPointer (0) + 48000, 48000);

    const auto uid = processor.getChain().getChild (0)[verb::ids::uid];
    processor.setModuleValue (uid, "decay", 1.2f);

    const auto swapped = render (processor, sine, 128);

    CHECK (allFinite (swapped));
    CHECK (maxJump (swapped.getReadPointer (0), swapped.getNumSamples()) < steadyJump * 3.0 + 0.01);
}

TEST_CASE ("output fades in after a reset", "[verb][swap]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;
    addReverb (processor);
    setParameter (processor, "wet", 0.0f);
    setParameter (processor, "dry", -60.0f);
    prepareOffline (processor);

    render (processor, noise (48000, 10), 128);

    processor.reset();

    const std::vector<float> dc (2400, 0.5f);
    const auto output = render (processor, dc, 128);

    auto rise = 0.0f;

    for (int i = 0; i < 48; ++i)
        rise = std::max (rise, std::abs (output.getSample (0, i)));

    auto later = 0.0f;

    for (int i = 960; i < 2400; ++i)
        later = std::max (later, std::abs (output.getSample (0, i)));

    CHECK (rise < later);
}

TEST_CASE ("pre-delay delays the wet path", "[verb]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;

    const auto uid = processor.addModule (ModuleType::echo, 0);
    processor.setModuleValue (uid, "start", 0.0f);
    processor.setModuleValue (uid, "length", 0.0f);
    processor.setModuleValue (uid, "startVol", 0.0f);

    setParameter (processor, "wet", 0.0f);
    setParameter (processor, "dry", -60.0f);
    setParameter (processor, "predelay", 100.0f);
    prepareOffline (processor);

    render (processor, std::vector<float> (24000, 0.0f), 256);

    std::vector<float> input (10000, 0.0f);
    input[100] = 1.0f;

    const auto output = render (processor, input, 256);

    CHECK (output.getSample (0, 100 + 4800) == Catch::Approx (1.0f).margin (1.0e-3));
}

TEST_CASE ("center and width act on the wet signal", "[verb]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;

    const auto uid = processor.addModule (ModuleType::reverb, 0);
    processor.setModuleValue (uid, "length", 0.2f);
    processor.setModuleValue (uid, "decay", 0.1f);

    setParameter (processor, "wet", 0.0f);
    setParameter (processor, "dry", -60.0f);

    SECTION ("zero width is mono")
    {
        setParameter (processor, "width", 0.0f);
        prepareOffline (processor);

        const auto output = render (processor, noise (24000, 11), 128);

        for (int i = 12000; i < 24000; ++i)
            REQUIRE (output.getSample (0, i) == Catch::Approx (output.getSample (1, i)).margin (1.0e-6));
    }

    SECTION ("center full left silences the right")
    {
        setParameter (processor, "center", -100.0f);
        prepareOffline (processor);

        const auto output = render (processor, noise (24000, 11), 128);

        for (int i = 12000; i < 24000; ++i)
            REQUIRE (output.getSample (1, i) == Catch::Approx (0.0f).margin (1.0e-6));
    }
}

TEST_CASE ("bypass passes delayed dry at any setting", "[verb]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;
    addReverb (processor);
    setParameter (processor, "latency", 1.0f);
    setParameter (processor, "dry", -20.0f);
    setParameter (processor, "output", 6.0f);
    setParameter (processor, "bypass", 1.0f);
    prepareOffline (processor);

    render (processor, std::vector<float> (24000, 0.0f), 512);

    const auto input = noise (48000, 12);
    const auto output = render (processor, input, 512);

    for (int i = Convolver::lowCpuLatency; i < output.getNumSamples(); ++i)
        REQUIRE (output.getSample (0, i) == Catch::Approx (input[(size_t) (i - Convolver::lowCpuLatency)]).margin (1.0e-5));
}

TEST_CASE ("mono bus and odd block sizes work", "[verb]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;
    addReverb (processor);

    REQUIRE (processor.setBusesLayout ({ { juce::AudioChannelSet::mono() }, { juce::AudioChannelSet::mono() } }));

    prepareOffline (processor, 44100.0);

    juce::AudioBuffer<float> buffer (1, 9000);
    juce::MidiBuffer midi;

    const auto input = noise (9000, 13);
    buffer.copyFrom (0, 0, input.data(), 9000);

    for (int start = 0, step = 1; start < 9000; step = (step * 3) % 8191 + 1)
    {
        const auto count = juce::jmin (step, 9000 - start);
        juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 1, start, count);
        processor.processBlock (view, midi);
        start += count;
    }

    CHECK (allFinite (buffer));
}

TEST_CASE ("extreme settings stay finite", "[verb]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::mt19937 random (1234);

    for (int run = 0; run < 12; ++run)
    {
        VerbProcessor processor;
        prepareOffline (processor);

        const auto types = verb::allModuleTypes;

        for (int i = 0; i < 5; ++i)
        {
            const auto type = types[random() % types.size()];
            const auto uid = processor.addModule (type, i);

            for (const auto& setting : verb::settingsOf (type))
            {
                const auto position = (float) (random() % 3) * 0.5f;
                processor.setModuleValue (uid, setting.id, juce::jmin (setting.high, setting.low + (setting.high - setting.low) * position));
            }
        }

        for (auto* parameter : processor.getParameters())
            if (parameter->isAutomatable() && parameter != processor.getBypassParameter())
                parameter->setValueNotifyingHost ((float) (random() % 3) * 0.5f);

        REQUIRE (processor.waitForBuild (60000));

        auto input = noise (8192, (unsigned) run);
        input[100] = std::numeric_limits<float>::quiet_NaN();

        CHECK (allFinite (render (processor, input, 1024)));
    }
}

// State -------------------------------------------------------------------------------------

TEST_CASE ("chain and settings survive a state round trip", "[verb][state]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor source;

    addReverb (source, 0.8f, 0.5f);

    const auto echo = source.addModule (ModuleType::echo, 1);
    source.setModuleValue (echo, "spacing", 77.0f);
    source.setModuleOn (echo, false);
    source.setTrim (0.01, 0.5);

    setParameter (source, "predelay", 42.0f);
    setParameter (source, "wet", -3.5f);

    juce::MemoryBlock state;
    source.getStateInformation (state);

    VerbProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());

    const auto chain = restored.getChain();

    REQUIRE (chain.getNumChildren() == 2);
    CHECK (chain.getChild (0)[verb::ids::type].toString() == "reverb");
    CHECK ((float) chain.getChild (0)["decay"] == Catch::Approx (0.5f));
    CHECK ((float) chain.getChild (1)["spacing"] == Catch::Approx (77.0f));
    CHECK (! (bool) chain.getChild (1)[verb::ids::on]);
    CHECK ((double) chain[verb::ids::trimEnd] == Catch::Approx (0.5));
    CHECK (restored.getApvts().getRawParameterValue ("predelay")->load() == Catch::Approx (42.0f));

    for (auto* processor : { &source, &restored })
        prepareOffline (*processor);

    const auto input = noise (48000, 14);

    CHECK (identical (render (source, input, 512), render (restored, input, 512)));
}

TEST_CASE ("damaged verb state can't crash the plugin", "[verb][state]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;

    SECTION ("garbage bytes")
    {
        const auto junk = noise (4000, 15);
        processor.setStateInformation (junk.data(), (int) (junk.size() * sizeof (float)));
    }

    SECTION ("bad modules and values")
    {
        auto state = processor.getApvts().copyState();
        auto chain = state.getChildWithName (verb::ids::chain);

        juce::ValueTree unknown (verb::ids::module);
        unknown.setProperty (verb::ids::type, "flanger", nullptr);
        chain.appendChild (unknown, nullptr);

        auto module = verb::makeModule (ModuleType::echo, 1);
        module.setProperty ("spacing", std::numeric_limits<double>::quiet_NaN(), nullptr);
        module.setProperty ("length", 1.0e12, nullptr);
        chain.appendChild (module, nullptr);

        auto twin = verb::makeModule (ModuleType::echo, 1);
        chain.appendChild (twin, nullptr);

        chain.appendChild (juce::ValueTree ("SOMETHING"), nullptr);
        chain.setProperty (verb::ids::trimStart, std::numeric_limits<double>::infinity(), nullptr);

        auto file = verb::makeModule (ModuleType::file, 5);
        file.setProperty (verb::ids::flac, "not base64 at all !!!", nullptr);
        file.setProperty (verb::ids::rate, -5.0, nullptr);
        file.setProperty (verb::ids::path, "relative\\path.wav", nullptr);
        chain.appendChild (file, nullptr);

        REQUIRE (processor.applyState (state));

        const auto clean = processor.getChain();

        CHECK (clean.getNumChildren() == 3);
        CHECK ((int) clean.getChild (0)[verb::ids::uid] != (int) clean.getChild (1)[verb::ids::uid]);
    }

    SECTION ("a state with no chain")
    {
        juce::ValueTree bare ("FANCY_VERB");
        REQUIRE (processor.applyState (bare));
        CHECK (processor.getChain().isValid());
    }

    prepareOffline (processor);
    CHECK (allFinite (render (processor, noise (4096, 16), 512)));
}

TEST_CASE ("impulse files load, embed, move and go missing", "[verb][files]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto folder = scratchFolder ("files");
    const auto file = folder.getChildFile ("Room.wav");

    const auto left = noise (24000, 17, 6000.0);
    const auto right = noise (24000, 18, 6000.0);

    REQUIRE (writeWav (file, makeAudio (left, right, 44100.0)->audio, 44100.0));

    const auto saved = [&] (bool embed)
    {
        VerbProcessor processor;
        setParameter (processor, "irStorage", embed ? 0.0f : 2.0f);

        const auto uid = processor.addModule (ModuleType::file, 0);
        REQUIRE (processor.loadFile (uid, file));
        REQUIRE (processor.waitForBuild (20000));

        CHECK (processor.getPreview().peakDb > -40.0);
        CHECK (! processor.isMissing (uid));

        juce::MemoryBlock state;
        processor.getStateInformation (state);
        return state;
    };

    const auto load = [&] (const juce::MemoryBlock& state)
    {
        auto processor = std::make_unique<VerbProcessor>();
        processor->setIrFolder ({});
        processor->setStateInformation (state.getData(), (int) state.getSize());
        REQUIRE (processor->waitForBuild (20000));
        return processor;
    };

    const auto uidOf = [] (VerbProcessor& processor) { return (int) processor.getChain().getChild (0)[verb::ids::uid]; };

    const auto embedded = saved (true);
    const auto pathOnly = saved (false);

    SECTION ("FLAC keeps the audio to 24 bits")
    {
        const auto audio = verb::readAudioFile (file);
        const auto copy = verb::decodeFlac (verb::encodeFlac (*audio), 44100.0);

        REQUIRE (copy != nullptr);
        REQUIRE (copy->audio.getNumSamples() == 24000);
        CHECK (copy->sampleRate == 44100.0);

        for (int i = 0; i < 24000; ++i)
            REQUIRE (copy->audio.getSample (1, i) == Catch::Approx (audio->audio.getSample (1, i)).margin (1.0e-6));
    }

    SECTION ("the embedded copy works once the file is gone")
    {
        REQUIRE (file.deleteFile());

        const auto processor = load (embedded);
        CHECK (! processor->isMissing (uidOf (*processor)));
        CHECK (processor->getPreview().peakDb > -40.0);
    }

    SECTION ("a path-only IR falls back to the absolute path")
    {
        const auto processor = load (pathOnly);
        CHECK (! processor->isMissing (uidOf (*processor)));
    }

    SECTION ("the IR folder finds a moved file by name and hash")
    {
        const auto other = scratchFolder ("moved");
        REQUIRE (file.moveFileTo (other.getChildFile ("Room.wav")));

        VerbProcessor processor;
        processor.setIrFolder (other);
        processor.setStateInformation (pathOnly.getData(), (int) pathOnly.getSize());
        REQUIRE (processor.waitForBuild (20000));

        CHECK (! processor.isMissing (uidOf (processor)));

        processor.setIrFolder ({});
    }

    SECTION ("a file with no source left is reported missing")
    {
        REQUIRE (file.deleteFile());

        const auto processor = load (pathOnly);
        CHECK (processor->isMissing (uidOf (*processor)));
        CHECK (processor->getPreview().columns.empty());
    }

    SECTION ("files sit next to their siblings")
    {
        REQUIRE (writeWav (folder.getChildFile ("Another.wav"), makeAudio (left, right, 44100.0)->audio, 44100.0));

        const auto siblings = verb::audioFilesBeside (file);

        REQUIRE (siblings.size() == 2);
        CHECK (siblings[0].getFileName() == "Another.wav");
    }
}

TEST_CASE ("factory presets build their chains", "[verb][state]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;
    auto& presets = processor.getPresets();

    const auto names = presets.getNames();
    REQUIRE (names.size() >= 6);

    for (int i = 0; i < 6; ++i)
    {
        presets.load (i);
        REQUIRE (processor.waitForBuild (60000));

        CAPTURE (names[i]);

        if (i == 0)
            CHECK (processor.getChain().getNumChildren() == 0);
        else
            CHECK (processor.getPreview().peakDb > -80.0);
    }
}

TEST_CASE ("rack editing keeps the chain consistent", "[verb][state]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;

    const auto first = processor.addModule (ModuleType::lpHp, 0);
    const auto second = processor.addModule (ModuleType::echo, 1);
    const auto third = processor.addModule (ModuleType::normalize, 1);

    const auto chain = processor.getChain();

    CHECK ((int) chain.getChild (1)[verb::ids::uid] == third);

    processor.moveModule (first, 2);
    CHECK ((int) chain.getChild (2)[verb::ids::uid] == first);

    processor.removeModule (third);
    CHECK (chain.getNumChildren() == 2);
    CHECK ((int) chain.getChild (0)[verb::ids::uid] == second);

    for (int i = 0; i < verb::maxModules + 2; ++i)
        processor.addModule (ModuleType::reverse, 0);

    CHECK (chain.getNumChildren() == verb::maxModules);
    CHECK (processor.addModule (ModuleType::reverse, 0) == 0);
}

TEST_CASE ("the editor opens with a full chain and an empty one", "[verb][ui]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;

    {
        const std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        REQUIRE (editor != nullptr);
        processor.editorBeingDeleted (editor.get());
    }

    for (const auto type : verb::allModuleTypes)
        processor.addModule (type, 0);

    const std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
    REQUIRE (editor != nullptr);
    processor.editorBeingDeleted (editor.get());
}

// Measurements ------------------------------------------------------------------------------

TEST_CASE ("verb measurements", "[.measure]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    // Paced at real time, because running flat out starves the worker of the head start it
    // has in a host. CPU is everything the process used, except this thread's own waiting.
    const auto measure = [] (bool lowCpu, const char* name)
    {
        const auto left = noise (192000, 20, 40000.0);
        const auto right = noise (192000, 21, 40000.0);

        Convolver convolver (left.data(), right.data(), 192000, lowCpu ? Convolver::Mode::lowCpu : Convolver::Mode::zeroLatency, 32768);

        const auto input = noise (128, 22);
        std::vector<float> outLeft (128), outRight (128);

        using Clock = std::chrono::steady_clock;

        constexpr int blocks = 7500; // 20 s
        const auto blockTime = std::chrono::duration_cast<Clock::duration> (std::chrono::duration<double> (128.0 / rate));

        const auto cpuBefore = processCpuSeconds();
        const auto threadBefore = threadCpuSeconds();
        const auto start = Clock::now();

        auto deadline = start;
        auto active = 0.0, worst = 0.0;
        auto overBudget = 0;

        for (int block = 0; block < blocks; ++block)
        {
            const auto blockStart = Clock::now();
            convolver.process (input.data(), input.data(), outLeft.data(), outRight.data(), 128, false);

            const auto used = std::chrono::duration<double> (Clock::now() - blockStart).count();
            active += used;
            worst = std::max (worst, used);
            overBudget += used > 128.0 / rate ? 1 : 0;

            deadline += blockTime;

            while (Clock::now() < deadline)
                _mm_pause();
        }

        const auto wall = std::chrono::duration<double> (Clock::now() - start).count();
        const auto workers = (processCpuSeconds() - cpuBefore) - (threadCpuSeconds() - threadBefore);

        std::printf ("verb 4 s stereo IR, %s: %.2f %% of one core (audio thread %.2f %%, worker %.2f %%), worst block %.0f us of 2667 us, %d of %d blocks over budget, %d jobs taken over, late blocks %d\n",
                     name, 100.0 * (active + workers) / wall, 100.0 * active / wall, 100.0 * workers / wall, worst * 1.0e6,
                     overBudget, blocks, convolver.getTakenOverJobs(), convolver.getLateBlocks());
    };

    measure (false, "Zero latency (target < 5 %)");
    measure (true, "Low CPU");
}
