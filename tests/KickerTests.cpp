#include "KickerDsp.h"
#include "KickerProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <fancy/Decibels.h>

#include <chrono>
#include <cstdio>
#include <limits>
#include <vector>

namespace
{
constexpr double pi = std::numbers::pi;

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples), right ((size_t) numSamples) {}

    int size() const { return (int) left.size(); }

    void process (KickerDsp& dsp, int blockSize = 64)
    {
        for (int start = 0; start < size(); start += blockSize)
        {
            float* channels[] { left.data() + start, right.data() + start };
            dsp.process (channels, 2, std::min (blockSize, size() - start));
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

double peakOf (const std::vector<float>& samples, int from)
{
    double peak = 0.0;

    for (size_t i = (size_t) from; i < samples.size(); ++i)
        peak = std::max (peak, (double) std::abs (samples[i]));

    return peak;
}

KickerDsp makeDsp (double sampleRate, const KickerDsp::Targets& targets)
{
    KickerDsp dsp;
    dsp.prepare (sampleRate);
    dsp.snapToTargets (targets);
    return dsp;
}

void setParameter (KickerProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getApvts().getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

std::vector<float> render (KickerProcessor& processor, int numSamples)
{
    juce::AudioBuffer<float> buffer (2, numSamples);
    juce::MidiBuffer midi;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto sample = (float) (0.5 * std::sin (2.0 * pi * 50.0 * i / 48000.0));
        buffer.setSample (0, i, sample);
        buffer.setSample (1, i, sample);
    }

    processor.processBlock (buffer, midi);

    return { buffer.getReadPointer (0), buffer.getReadPointer (0) + numSamples };
}
} // namespace

TEST_CASE ("wet and dry add in phase at Freq", "[kicker]")
{
    const auto sampleRate = GENERATE (44100.0, 48000.0, 96000.0, 192000.0, 384000.0);

    KickerDsp::Targets targets;
    targets.wetGain = 1.0;
    auto dsp = makeDsp (sampleRate, targets);

    auto signal = sine (50.0, 0.25, sampleRate, (int) sampleRate);
    signal.process (dsp);

    // The band-pass adds no phase at Freq. The 15 Hz high-pass adds about 25 degrees of lead
    // at 50 Hz, so the sum is about 5.8 dB rather than a perfect 6 dB.
    REQUIRE (fancy::gainToDb (peakOf (signal.left, (int) (sampleRate / 2)) / 0.25) == Catch::Approx (5.8).margin (0.2));
}

TEST_CASE ("wet at -inf with dry at 0 dB reproduces the input exactly", "[kicker]")
{
    KickerDsp::Targets targets;
    targets.wetGain = 0.0;
    auto dsp = makeDsp (48000.0, targets);

    const auto input = sine (73.0, 0.5, 48000.0, 4096);
    auto output = input;
    output.process (dsp);

    REQUIRE (output.left == input.left);
}

TEST_CASE ("wet path blocks DC", "[kicker]")
{
    KickerDsp::Targets targets;
    targets.wetGain = fancy::dbToGain (12.0);
    targets.dryGain = 0.0;
    auto dsp = makeDsp (48000.0, targets);

    Stereo signal (48000 * 5);
    std::fill (signal.left.begin(), signal.left.end(), 0.5f);
    std::fill (signal.right.begin(), signal.right.end(), 0.5f);
    signal.process (dsp);

    REQUIRE (peakOf (signal.left, 48000 * 4) < 1.0e-5);
}

TEST_CASE ("frequency sweep is click-free", "[kicker][sweep]")
{
    constexpr double sampleRate = 48000.0;
    constexpr int sweepSamples = 9600; // 200 ms

    KickerDsp::Targets targets;
    targets.frequencyHz = 20.0;
    targets.wetGain = fancy::dbToGain (12.0);
    auto dsp = makeDsp (sampleRate, targets);

    auto signal = sine (60.0, 0.25, sampleRate, 48000);

    // Host-style automation: one new value per 128-sample block, 20 to 240 Hz over 200 ms.
    constexpr int blockSize = 128;

    for (int start = 0; start < signal.size(); start += blockSize)
    {
        const auto position = std::clamp ((double) (start - 4800) / (double) sweepSamples, 0.0, 1.0);
        targets.frequencyHz = 20.0 * std::pow (12.0, position);
        dsp.setTargets (targets);

        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, std::min (blockSize, signal.size() - start));
    }

    double largestStep = 0.0;

    for (size_t i = 1; i < signal.left.size(); ++i)
        largestStep = std::max (largestStep, (double) std::abs (signal.left[i] - signal.left[i - 1]));

    // Peak output is about 1.25. Even a 240 Hz wave at that level moves only about 0.04 per
    // sample, so a click would stand well clear of this limit.
    REQUIRE (largestStep < 0.05);

    for (const auto sample : signal.left)
        REQUIRE (std::isfinite (sample));
}

TEST_CASE ("mono wet path", "[kicker]")
{
    KickerDsp::Targets targets;
    targets.monoWet = true;
    targets.wetGain = 1.0;
    targets.dryGain = 0.0;
    auto dsp = makeDsp (48000.0, targets);

    // Opposite polarity in each channel sums to nothing in the mono wet path.
    auto signal = sine (50.0, 0.5, 48000.0, 48000);

    for (auto& sample : signal.right)
        sample = -sample;

    signal.process (dsp);

    REQUIRE (peakOf (signal.left, 0) < 1.0e-6);
    REQUIRE (peakOf (signal.right, 0) < 1.0e-6);
}

TEST_CASE ("soft clip bounds the output", "[kicker]")
{
    REQUIRE (KickerDsp::softClip (0.5) == 0.5);
    REQUIRE (KickerDsp::softClip (-0.7) == -0.7);
    REQUIRE (KickerDsp::softClip (1000.0) <= 1.0);
    REQUIRE (KickerDsp::softClip (-1000.0) >= -1.0);

    KickerDsp::Targets targets;
    targets.softClip = true;
    targets.wetGain = fancy::dbToGain (12.0);
    targets.dryGain = fancy::dbToGain (12.0);
    auto dsp = makeDsp (48000.0, targets);

    auto signal = sine (50.0, 1.0, 48000.0, 48000);
    signal.process (dsp);

    REQUIRE (peakOf (signal.left, 0) <= 1.0);
}

TEST_CASE ("bad input never poisons the filters", "[kicker]")
{
    auto dsp = makeDsp (48000.0, {});

    Stereo bad (64);
    std::fill (bad.left.begin(), bad.left.end(), std::numeric_limits<float>::quiet_NaN());
    bad.process (dsp);

    auto signal = sine (50.0, 0.5, 48000.0, 4800);
    signal.process (dsp);

    for (const auto sample : signal.left)
        REQUIRE (std::isfinite (sample));
}

TEST_CASE ("block size doesn't change the output", "[kicker]")
{
    KickerDsp::Targets targets;
    auto whole = makeDsp (48000.0, targets);
    auto single = makeDsp (48000.0, targets);

    auto a = sine (63.0, 0.5, 48000.0, 8192);
    auto b = a;

    a.process (whole, 8192);
    b.process (single, 1);

    REQUIRE (a.left == b.left);
}

TEST_CASE ("automation with an unchanged value is bit-identical", "[kicker]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    KickerProcessor plain, automated;
    plain.prepareToPlay (48000.0, 2048);
    automated.prepareToPlay (48000.0, 2048);

    for (const auto* id : { "freq", "wet", "dry", "q", "output" })
    {
        auto* parameter = automated.getApvts().getParameter (id);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->getValue());
        parameter->endChangeGesture();
    }

    REQUIRE (render (plain, 2048) == render (automated, 2048));
}

TEST_CASE ("state round-trips", "[kicker]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    KickerProcessor source;
    setParameter (source, "freq", 87.0f);
    setParameter (source, "wet", -3.0f);
    setParameter (source, "monoBelow", 1.0f);

    juce::MemoryBlock state;
    source.getStateInformation (state);

    KickerProcessor destination;
    destination.setStateInformation (state.getData(), (int) state.getSize());

    auto& apvts = destination.getApvts();
    CHECK (apvts.getRawParameterValue ("freq")->load() == Catch::Approx (87.0f));
    CHECK (apvts.getRawParameterValue ("wet")->load() == Catch::Approx (-3.0f));
    CHECK (apvts.getRawParameterValue ("monoBelow")->load() == 1.0f);
}

TEST_CASE ("corrupted state is ignored", "[kicker]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    KickerProcessor processor;

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    SECTION ("truncated")
    {
        processor.setStateInformation (state.getData(), (int) state.getSize() / 2);
    }

    SECTION ("random bytes")
    {
        juce::Random random (1234);

        for (size_t i = 0; i < state.getSize(); ++i)
            state[i] = (char) random.nextInt (256);

        processor.setStateInformation (state.getData(), (int) state.getSize());
    }

    SECTION ("wrong plugin, bad values and unknown parameters")
    {
        juce::XmlElement wrongPlugin ("SomethingElse");
        juce::MemoryBlock wrongState;
        juce::AudioProcessor::copyXmlToBinary (wrongPlugin, wrongState);
        processor.setStateInformation (wrongState.getData(), (int) wrongState.getSize());

        juce::XmlElement damaged ("FancyKicker");

        const auto addParameter = [&damaged] (const char* id, const char* value)
        {
            auto* element = damaged.createNewChildElement ("PARAM");
            element->setAttribute ("id", id);
            element->setAttribute ("value", value);
        };

        addParameter ("freq", "1e30");
        addParameter ("wet", "nan");
        addParameter ("notAParameter", "1");

        juce::MemoryBlock damagedState;
        juce::AudioProcessor::copyXmlToBinary (damaged, damagedState);
        processor.setStateInformation (damagedState.getData(), (int) damagedState.getSize());

        CHECK (processor.getApvts().getRawParameterValue ("freq")->load() <= 240.0f);
    }

    processor.prepareToPlay (48000.0, 512);
    CHECK (render (processor, 512).size() == 512);
}

TEST_CASE ("factory presets apply and reset unlisted parameters", "[kicker]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    KickerProcessor processor;
    setParameter (processor, "output", -6.0f);

    auto& presets = processor.getPresets();
    presets.load (presets.getNames().indexOf ("Deep Sub"));

    auto& apvts = processor.getApvts();
    CHECK (presets.getCurrentName() == "Deep Sub");
    CHECK (apvts.getRawParameterValue ("freq")->load() == Catch::Approx (42.0f));
    CHECK (apvts.getRawParameterValue ("monoBelow")->load() == 1.0f);
    CHECK (apvts.getRawParameterValue ("output")->load() == Catch::Approx (0.0f));
}

TEST_CASE ("reports zero latency", "[kicker]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    KickerProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    REQUIRE (processor.getLatencySamples() == 0);
}

// Hidden from the default run. CI runs these on their own and keeps the output.
TEST_CASE ("kicker CPU measurement", "[.measure]")
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 128;
    constexpr int numBlocks = 37500; // 100 s of audio

    KickerDsp::Targets targets;
    targets.monoWet = true;
    targets.softClip = true;
    auto dsp = makeDsp (sampleRate, targets);

    auto signal = sine (50.0, 0.5, sampleRate, blockSize);
    float* channels[] { signal.left.data(), signal.right.data() };

    const auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < numBlocks; ++i)
        dsp.process (channels, 2, blockSize);

    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    const auto audioSeconds = (double) (numBlocks * blockSize) / sampleRate;

    std::printf ("Kicker CPU at 48 kHz, 128 samples, stereo: %.4f %% (target < 0.5 %%)\n", 100.0 * elapsed.count() / audioSeconds);
}

TEST_CASE ("kicker sweep measurement", "[.measure]")
{
    for (const auto hz : { 20.0, 30.0, 50.0, 70.0, 100.0, 240.0 })
    {
        KickerDsp::Targets targets;
        targets.wetGain = 1.0;
        targets.dryGain = 0.0;
        auto dsp = makeDsp (48000.0, targets);

        auto signal = sine (hz, 0.25, 48000.0, 96000);
        signal.process (dsp);

        std::printf ("Wet-only response, Freq 50 Hz, Q 2, tone %6.1f Hz: %+7.2f dB\n", hz, fancy::gainToDb (peakOf (signal.left, 48000) / 0.25));
    }
}
