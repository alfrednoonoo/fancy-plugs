#include "ExciterDsp.h"
#include "ExciterProcessor.h"
#include "LegacyExciterDsp.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <fancy/Decibels.h>

#include <chrono>
#include <complex>
#include <cstdio>
#include <limits>
#include <random>
#include <numbers>
#include <vector>

namespace
{
constexpr double pi = std::numbers::pi;

struct Audio
{
    std::vector<float> left, right;

    explicit Audio (int numSamples) : left ((size_t) numSamples), right ((size_t) numSamples) {}

    int size() const { return (int) left.size(); }

    void process (ExciterDsp& dsp, int blockSize = 512)
    {
        for (int start = 0; start < size(); start += blockSize)
        {
            float* channels[] { left.data() + start, right.data() + start };
            dsp.process (channels, 2, std::min (blockSize, size() - start));
        }
    }
};

Audio sine (double hz, double amplitude, double sampleRate, int numSamples)
{
    Audio signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * hz * i / sampleRate));

    return signal;
}

// Level of one frequency in dBFS, Hann windowed over the last part of the signal.
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

double peakOf (const std::vector<float>& samples, int from)
{
    double peak = 0.0;

    for (size_t i = (size_t) from; i < samples.size(); ++i)
        peak = std::max (peak, (double) std::abs (samples[i]));

    return peak;
}

ExciterDsp::Targets excited (int oversampling)
{
    ExciterDsp::Targets targets;
    targets.harmonics = 1.0;
    targets.mixBackGain = 1.0;
    targets.boostGain = fancy::dbToGain (12.0);
    targets.oversampling = oversampling;
    return targets;
}

// Aliases of the harmonics of 10 kHz at 44.1 kHz, up to the 9th. The 2nd harmonic, at
// 20 kHz, is below Nyquist and is wanted.
double worstAliasDb (int oversampling, double character)
{
    constexpr double sampleRate = 44100.0;

    auto targets = excited (oversampling);
    targets.character = character;

    ExciterDsp dsp;
    dsp.prepare (sampleRate);
    dsp.snapToTargets (targets);

    auto signal = sine (10000.0, 1.0, sampleRate, 88200);
    signal.process (dsp);

    double worst = -200.0;

    for (const auto hz : { 14100.0, 4100.0, 5900.0, 15900.0, 18200.0, 8200.0, 1800.0 })
        worst = std::max (worst, levelAt (signal.left, 44100, hz, sampleRate));

    return worst;
}

void setParameter (ExciterProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getApvts().getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

std::vector<float> render (ExciterProcessor& processor, int numSamples)
{
    juce::AudioBuffer<float> buffer (2, numSamples);
    juce::MidiBuffer midi;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto sample = (float) (0.5 * std::sin (2.0 * pi * 3000.0 * i / 48000.0));
        buffer.setSample (0, i, sample);
        buffer.setSample (1, i, sample);
    }

    processor.processBlock (buffer, midi);

    return { buffer.getReadPointer (0), buffer.getReadPointer (0) + numSamples };
}
} // namespace

TEST_CASE ("the shaper and its antiderivative agree", "[exciter]")
{
    CHECK (ExciterDsp::shape (0.0) == 0.0);
    CHECK (ExciterDsp::shape (1.5) == Catch::Approx (1.0));
    CHECK (ExciterDsp::shape (-1000.0) == -1.0);

    constexpr double step = 1.0e-5;

    for (double x = -2.0; x <= 2.0; x += 0.013)
    {
        const auto slope = (ExciterDsp::shapeAntiderivative (x + step) - ExciterDsp::shapeAntiderivative (x - step)) / (2.0 * step);
        CHECK (slope == Catch::Approx (ExciterDsp::shape (x)).margin (1.0e-6));
    }
}

TEST_CASE ("harmonics at 0 is the delayed input exactly", "[exciter]")
{
    const auto oversampling = GENERATE ((int) ExciterDsp::off, (int) ExciterDsp::x2, (int) ExciterDsp::x4, (int) ExciterDsp::x8);

    ExciterDsp::Targets targets;
    targets.oversampling = oversampling;
    targets.boostGain = fancy::dbToGain (24.0);

    ExciterDsp dsp;
    dsp.prepare (48000.0);
    dsp.snapToTargets (targets);

    const auto input = sine (3000.0, 0.8, 48000.0, 8192);
    auto output = input;
    output.process (dsp, 333);

    const auto latency = dsp.getLatencySamples();
    REQUIRE (latency < 2048);

    for (int i = latency; i < input.size(); ++i)
        REQUIRE (output.left[(size_t) i] == input.left[(size_t) (i - latency)]);
}

TEST_CASE ("wet and dry stay aligned at every oversampling setting", "[exciter]")
{
    const auto oversampling = GENERATE ((int) ExciterDsp::off, (int) ExciterDsp::x2, (int) ExciterDsp::x4, (int) ExciterDsp::x8);

    // Well below clipping and far above Freq, so the wet path is close to the input itself.
    // At 4 kHz the phase of the low-pass and the high-passes nearly cancel, and the bias
    // lowers the wet gain to about -0.6 dB, so the sum is about 5.7 dB rather than 6 dB.
    // Dry and wet a sample apart would fall below this range.
    ExciterDsp::Targets targets;
    targets.harmonics = 1.0;
    targets.mixBackGain = 1.0;
    targets.frequencyHz = 500.0;
    targets.oversampling = oversampling;

    ExciterDsp dsp;
    dsp.prepare (48000.0);
    dsp.snapToTargets (targets);

    auto signal = sine (4000.0, 0.01, 48000.0, 48000);
    signal.process (dsp);

    REQUIRE (fancy::gainToDb (peakOf (signal.left, 24000) / 0.01) == Catch::Approx (5.7).margin (0.3));
}

TEST_CASE ("aliasing is below -80 dBFS at 4x", "[exciter]")
{
    const auto character = GENERATE (0.0, 0.5, 1.0);
    REQUIRE (worstAliasDb (ExciterDsp::x4, character) < -80.0);
}

TEST_CASE ("wet path blocks DC", "[exciter]")
{
    auto targets = excited (ExciterDsp::x2);
    targets.character = 0.0;
    targets.frequencyHz = 500.0;

    ExciterDsp dsp;
    dsp.prepare (48000.0);
    dsp.snapToTargets (targets);

    Audio signal (48000);
    std::fill (signal.left.begin(), signal.left.end(), 0.25f);
    std::fill (signal.right.begin(), signal.right.end(), 0.25f);
    signal.process (dsp);

    REQUIRE (std::abs (signal.left.back() - 0.25f) < 1.0e-5f);
}

TEST_CASE ("character moves between even and odd harmonics", "[exciter]")
{
    constexpr double sampleRate = 48000.0;

    const auto harmonicsFor = [] (double character)
    {
        ExciterDsp::Targets targets;
        targets.harmonics = 1.0;
        targets.mixBackGain = 1.0;
        targets.boostGain = fancy::dbToGain (6.0);
        targets.frequencyHz = 500.0;
        targets.character = character;

        ExciterDsp dsp;
        dsp.prepare (sampleRate);
        dsp.snapToTargets (targets);

        auto signal = sine (1000.0, 0.5, sampleRate, 48000);
        signal.process (dsp);

        return std::pair { levelAt (signal.left, 24000, 2000.0, sampleRate), levelAt (signal.left, 24000, 3000.0, sampleRate) };
    };

    const auto [evenAtEven, oddAtEven] = harmonicsFor (0.0);
    const auto [evenAtOdd, oddAtOdd] = harmonicsFor (1.0);

    CHECK (evenAtEven > oddAtEven);
    CHECK (evenAtEven > -40.0);
    CHECK (evenAtOdd < -100.0);
    CHECK (oddAtOdd > -60.0);
}

TEST_CASE ("output stays bounded at maximum settings", "[exciter]")
{
    auto targets = excited (ExciterDsp::x4);
    targets.boostGain = fancy::dbToGain (24.0);
    targets.mixBackGain = fancy::dbToGain (6.0);

    ExciterDsp dsp;
    dsp.prepare (44100.0);
    dsp.snapToTargets (targets);

    auto signal = sine (5000.0, 1.0, 44100.0, 44100);
    signal.process (dsp);

    // Dry at 1.0 plus a shaped wet path of about 1.0 at +6 dB.
    REQUIRE (peakOf (signal.left, 4410) < 3.5);
}

TEST_CASE ("changing oversampling while playing is safe", "[exciter]")
{
    constexpr double sampleRate = 48000.0;

    auto targets = excited (ExciterDsp::off);
    ExciterDsp dsp;
    dsp.prepare (sampleRate);
    dsp.snapToTargets (targets);

    const auto offLatency = dsp.getLatencySamples();
    REQUIRE (offLatency == 0);

    auto signal = sine (1000.0, 0.25, sampleRate, 48000);
    constexpr int blockSize = 128;
    const int settings[] { ExciterDsp::x4, ExciterDsp::x8, ExciterDsp::x2, ExciterDsp::off };

    for (int start = 0, block = 0; start < signal.size(); start += blockSize, ++block)
    {
        if (block % 40 == 20)
        {
            targets.oversampling = settings[(block / 40) % 4];
            dsp.setTargets (targets);
        }

        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, std::min (blockSize, signal.size() - start));
    }

    double largestStep = 0.0;

    for (size_t i = 1; i < signal.left.size(); ++i)
    {
        REQUIRE (std::isfinite (signal.left[i]));
        largestStep = std::max (largestStep, (double) std::abs (signal.left[i] - signal.left[i - 1]));
    }

    // Peak output is about 1.25 at 1 kHz, about 0.16 per sample. A hard switch would jump
    // by the full signal level.
    REQUIRE (largestStep < 0.3);
}

TEST_CASE ("returning to the same oversampling during the fade doesn't leave it silent", "[exciter]")
{
    constexpr double sampleRate = 48000.0;

    auto targets = excited (ExciterDsp::x4);
    ExciterDsp dsp;
    dsp.prepare (sampleRate);
    dsp.snapToTargets (targets);

    auto first = sine (1000.0, 0.25, sampleRate, 128);
    targets.oversampling = ExciterDsp::x8;
    dsp.setTargets (targets);
    first.process (dsp, 128);

    targets.oversampling = ExciterDsp::x4;
    dsp.setTargets (targets);

    auto second = sine (1000.0, 0.25, sampleRate, 24000);
    second.process (dsp);

    REQUIRE (peakOf (second.left, 12000) > 0.2);
}

TEST_CASE ("exciter frequency sweep is click-free", "[exciter]")
{
    constexpr double sampleRate = 48000.0;

    auto targets = excited (ExciterDsp::x2);
    targets.boostGain = 1.0;
    targets.frequencyHz = 500.0;

    ExciterDsp dsp;
    dsp.prepare (sampleRate);
    dsp.snapToTargets (targets);

    auto signal = sine (4000.0, 0.25, sampleRate, 48000);
    constexpr int blockSize = 128;

    for (int start = 0; start < signal.size(); start += blockSize)
    {
        const auto position = std::clamp ((double) (start - 4800) / 9600.0, 0.0, 1.0);
        targets.frequencyHz = 500.0 * std::pow (32.0, position);
        dsp.setTargets (targets);

        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, std::min (blockSize, signal.size() - start));
    }

    double largestStep = 0.0;

    for (size_t i = 1; i < signal.left.size(); ++i)
        largestStep = std::max (largestStep, (double) std::abs (signal.left[i] - signal.left[i - 1]));

    // A clean 4 kHz wave at peak 0.5 moves about 0.26 per sample.
    REQUIRE (largestStep < 0.4);
}

TEST_CASE ("exciter block size doesn't change the output", "[exciter]")
{
    const auto targets = excited (ExciterDsp::x4);

    ExciterDsp whole, single;
    whole.prepare (48000.0);
    whole.snapToTargets (targets);
    single.prepare (48000.0);
    single.snapToTargets (targets);

    auto a = sine (3000.0, 0.5, 48000.0, 8192);
    auto b = a;

    a.process (whole, 8192);
    b.process (single, 1);

    for (size_t i = 0; i < a.left.size(); ++i)
        REQUIRE (a.left[i] == Catch::Approx (b.left[i]).margin (1.0e-6));
}

TEST_CASE ("bad input never poisons the exciter", "[exciter]")
{
    ExciterDsp dsp;
    dsp.prepare (48000.0);
    dsp.snapToTargets (excited (ExciterDsp::x4));

    Audio bad (64);
    std::fill (bad.left.begin(), bad.left.end(), std::numeric_limits<float>::quiet_NaN());
    std::fill (bad.right.begin(), bad.right.end(), std::numeric_limits<float>::infinity());
    bad.process (dsp);

    auto signal = sine (3000.0, 0.5, 48000.0, 4800);
    signal.process (dsp);

    for (const auto sample : signal.left)
        REQUIRE (std::isfinite (sample));
}

TEST_CASE ("exciter works at every supported sample rate and in mono", "[exciter]")
{
    const auto sampleRate = GENERATE (44100.0, 96000.0, 192000.0, 384000.0);

    ExciterDsp dsp;
    dsp.prepare (sampleRate);
    dsp.snapToTargets (excited (ExciterDsp::x8));

    auto signal = sine (3000.0, 0.5, sampleRate, 8192);
    float* channels[] { signal.left.data() };
    dsp.process (channels, 1, signal.size());

    for (const auto sample : signal.left)
        REQUIRE (std::isfinite (sample));
}

TEST_CASE ("exciter automation with an unchanged value is bit-identical", "[exciter]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    ExciterProcessor plain, automated;

    for (auto* processor : { &plain, &automated })
    {
        setParameter (*processor, "harmonics", 50.0f);
        processor->prepareToPlay (48000.0, 2048);
    }

    for (const auto* id : { "freq", "clipBoost", "harmonics", "mixBack", "character", "output", "oversampling" })
    {
        auto* parameter = automated.getApvts().getParameter (id);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->getValue());
        parameter->endChangeGesture();
    }

    REQUIRE (render (plain, 2048) == render (automated, 2048));
}

TEST_CASE ("exciter state round-trips and updates latency", "[exciter]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    ExciterProcessor source;
    setParameter (source, "freq", 3500.0f);
    setParameter (source, "harmonics", 42.0f);
    setParameter (source, "oversampling", 1.0f);

    juce::MemoryBlock state;
    source.getStateInformation (state);

    ExciterProcessor destination;
    destination.setStateInformation (state.getData(), (int) state.getSize());

    auto& apvts = destination.getApvts();
    CHECK (apvts.getRawParameterValue ("freq")->load() == Catch::Approx (3500.0f));
    CHECK (apvts.getRawParameterValue ("harmonics")->load() == Catch::Approx (42.0f));
    CHECK (apvts.getRawParameterValue ("oversampling")->load() == 1.0f);

    destination.prepareToPlay (48000.0, 512);
    const auto latencyAt2x = destination.getLatencySamples();
    CHECK (latencyAt2x > 0);

    setParameter (destination, "oversampling", 0.0f);

    for (int i = 0; i < 40; ++i)
        render (destination, 512);

    CHECK (destination.getLatencySamples() == 0);
}

TEST_CASE ("corrupted exciter state is ignored", "[exciter]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    ExciterProcessor processor;

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    processor.setStateInformation (state.getData(), (int) state.getSize() / 2);

    juce::Random random (4321);

    for (size_t i = 0; i < state.getSize(); ++i)
        state[i] = (char) random.nextInt (256);

    processor.setStateInformation (state.getData(), (int) state.getSize());

    processor.prepareToPlay (48000.0, 512);
    CHECK (render (processor, 512).size() == 512);
}

TEST_CASE ("exciter factory presets apply", "[exciter]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    ExciterProcessor processor;
    auto& presets = processor.getPresets();
    presets.load (presets.getNames().indexOf ("Warm Top"));

    auto& apvts = processor.getApvts();
    CHECK (presets.getCurrentName() == "Warm Top");
    CHECK (apvts.getRawParameterValue ("freq")->load() == Catch::Approx (5000.0f));
    CHECK (apvts.getRawParameterValue ("character")->load() == 0.0f);
}

// Hidden from the default run. CI runs these on their own and keeps the output.
TEST_CASE ("exciter CPU measurement", "[.measure]")
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 128;
    constexpr int numBlocks = 18750; // 50 s of audio

    for (const auto oversampling : { (int) ExciterDsp::off, (int) ExciterDsp::x2, (int) ExciterDsp::x4, (int) ExciterDsp::x8 })
    {
        ExciterDsp dsp;
        dsp.prepare (sampleRate);
        dsp.snapToTargets (excited (oversampling));

        auto signal = sine (3000.0, 0.5, sampleRate, blockSize);
        float* channels[] { signal.left.data(), signal.right.data() };

        const auto start = std::chrono::steady_clock::now();

        for (int i = 0; i < numBlocks; ++i)
            dsp.process (channels, 2, blockSize);

        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
        const auto audioSeconds = (double) (numBlocks * blockSize) / sampleRate;

        std::printf ("Exciter CPU at 48 kHz, 128 samples, stereo, oversampling index %d: %.4f %% (target without oversampling < 0.5 %%)\n",
                     oversampling, 100.0 * elapsed.count() / audioSeconds);
    }
}

TEST_CASE ("exciter latency and aliasing measurement", "[.measure]")
{
    const char* const names[] { "Off", "2x", "4x", "8x" };

    for (int oversampling = 0; oversampling < 4; ++oversampling)
    {
        ExciterDsp::Targets targets;
        targets.oversampling = oversampling;

        ExciterDsp dsp;
        dsp.prepare (48000.0);
        dsp.snapToTargets (targets);

        std::printf ("Oversampling %-3s: latency %3d samples, worst alias (10 kHz, 0 dBFS, Harmonics 100 %%, Boost 12 dB, 44.1 kHz) %7.1f dBFS at Character 50 %%, %7.1f dBFS at Character 0 %%\n",
                     names[oversampling], dsp.getLatencySamples(), worstAliasDb (oversampling, 0.5), worstAliasDb (oversampling, 0.0));
    }
}

TEST_CASE ("Exciter: the oversampler moved to fancy-core changes nothing, sample for sample", "[exciter][shared]")
{
    // docs/sature.md, Decision 3: a copy of ExciterDsp from before the move runs alongside the
    // current one, through every oversampling setting, switches between them and gliding
    // settings, at three rates and random block sizes.
    const auto sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    INFO ("rate " << sampleRate);

    ExciterDsp current;
    LegacyExciterDsp legacy;

    const auto targetsAt = [] (int step)
    {
        ExciterDsp::Targets targets;
        targets.frequencyHz = step % 2 == 0 ? 3000.0 : 6500.0;
        targets.boostGain = fancy::dbToGain (step % 3 == 0 ? 6.0 : 18.0);
        targets.harmonics = 0.3 + 0.1 * (step % 5);
        targets.mixBackGain = 0.8;
        targets.character = step % 2 == 0 ? 0.2 : 0.9;
        targets.oversampling = step % 4;
        targets.bypass = step == 9;
        return targets;
    };

    const auto legacyTargets = [] (const ExciterDsp::Targets& t)
    {
        LegacyExciterDsp::Targets l;
        l.frequencyHz = t.frequencyHz;
        l.boostGain = t.boostGain;
        l.harmonics = t.harmonics;
        l.mixBackGain = t.mixBackGain;
        l.character = t.character;
        l.outputGain = t.outputGain;
        l.oversampling = t.oversampling;
        l.bypass = t.bypass;
        return l;
    };

    current.prepare (sampleRate);
    legacy.prepare (sampleRate);
    current.snapToTargets (targetsAt (2));
    legacy.snapToTargets (legacyTargets (targetsAt (2)));

    std::mt19937 random (5);
    std::uniform_int_distribution<int> sizes (1, 700);
    std::uniform_real_distribution<float> noise (-0.7f, 0.7f);

    long long compared = 0, differing = 0;

    for (int step = 0; step < 12; ++step)
    {
        const auto targets = targetsAt (step);
        const auto length = (int) (0.15 * sampleRate);

        for (int done = 0; done < length;)
        {
            const auto count = std::min (sizes (random), length - done);
            std::vector<float> a ((size_t) count * 2), b;

            for (auto& v : a)
                v = noise (random);

            b = a;

            float* currentChannels[] { a.data(), a.data() + count };
            float* legacyChannels[] { b.data(), b.data() + count };

            current.setTargets (targets);
            legacy.setTargets (legacyTargets (targets));
            current.process (currentChannels, 2, count);
            legacy.process (legacyChannels, 2, count);

            CHECK (current.getLatencySamples() == legacy.getLatencySamples());

            for (size_t i = 0; i < a.size(); ++i)
            {
                ++compared;
                differing += a[i] != b[i] ? 1 : 0;
            }

            done += count;
        }
    }

    INFO (differing << " of " << compared << " samples differ");
    CHECK (differing == 0);
}
