// Phase 2.1 item A: adaptive noise tracking in Fancy FIR's Subtract mode. Design:
// docs/phase2_1-A.md.

#include "FirProcessor.h"
#include "SpectralEngine.h"

#include <catch2/catch_test_macros.hpp>

#include <fancy/Decibels.h>

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <vector>

using fir::SpectralEngine;

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = 3.14159265358979323846;
constexpr int blockSize = 512;

fir::FrameSettings subtractSettings (int source)
{
    fir::FrameSettings settings;
    settings.mode = fir::subtract;
    settings.source = source;
    settings.adaptSeconds = 2.0;
    return settings;
}

// Pink noise (Paul Kellet's filter) scaled to an RMS level in dBFS.
std::vector<float> pink (int numSamples, double rmsDb, unsigned seed)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<float> white (-1.0f, 1.0f);
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;

    std::vector<float> samples ((size_t) numSamples);
    auto sum = 0.0;

    for (auto& sample : samples)
    {
        const auto w = white (random);
        b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f; b2 = 0.96900f * b2 + w * 0.1538520f;
        b3 = 0.86650f * b3 + w * 0.3104856f; b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
        sample = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
        b6 = w * 0.115926f;
        sum += (double) sample * sample;
    }

    const auto scale = (float) (std::pow (10.0, rmsDb / 20.0) / std::sqrt (sum / numSamples));

    for (auto& sample : samples)
        sample *= scale;

    return samples;
}

std::vector<float> white (int numSamples, double rmsDb, unsigned seed)
{
    std::mt19937 random (seed);
    std::normal_distribution<float> gauss (0.0f, (float) std::pow (10.0, rmsDb / 20.0));
    std::vector<float> samples ((size_t) numSamples);

    for (auto& sample : samples)
        sample = gauss (random);

    return samples;
}

std::vector<float> sine (int numSamples, double hz, double amplitude)
{
    std::vector<float> samples ((size_t) numSamples);

    for (int i = 0; i < numSamples; ++i)
        samples[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * hz * i / rate));

    return samples;
}

std::vector<float> plus (const std::vector<float>& a, const std::vector<float>& b)
{
    auto sum = a;

    for (size_t i = 0; i < sum.size(); ++i)
        sum[i] += b[i];

    return sum;
}

// Runs a stretch through the engine, calling `each` after every block.
std::vector<float> run (SpectralEngine& engine, const std::vector<float>& input, const fir::FrameSettings& settings,
                        const std::function<void (int)>& each = {})
{
    std::vector<float> left (input.size()), right (input.size());

    for (int position = 0; position < (int) input.size(); position += blockSize)
    {
        const auto count = std::min (blockSize, (int) input.size() - position);
        engine.process (input.data() + position, input.data() + position, nullptr, nullptr,
                        left.data() + position, right.data() + position, 2, count, settings, true);

        if (each)
            each (position + count);
    }

    return left;
}

// The average from 100 Hz to 8 kHz, in dB, of a profile on the shared grid.
template <typename Grid>
double gridAverageDb (const Grid& grid)
{
    auto sum = 0.0;
    auto count = 0;

    for (int j = 0; j < fir::profilePoints; ++j)
    {
        const auto hz = fir::profileHz (j);

        if (hz >= 100.0 && hz <= 8000.0)
        {
            sum += 10.0 * std::log10 (std::max ((double) grid[(size_t) j].load(), 1.0e-30));
            ++count;
        }
    }

    return sum / count;
}

// The learned level of a noise, as Learn measures it, on the same grid: the reference.
double learnedLevelDb (const std::vector<float>& noise, int fftSize)
{
    fir::Shared shared;
    SpectralEngine engine ({ fftSize, false, false }, rate, shared);

    shared.learnTarget.store ((int) noise.size() - 2 * fftSize);
    shared.learnRequested.store (true);
    run (engine, noise, subtractSettings (fir::learnedSource));

    REQUIRE (shared.hasProfile.load());
    return gridAverageDb (shared.profile);
}

// Energy between two frequencies, from one large FFT.
double bandEnergy (const std::vector<float>& samples, int start, double lowHz, double highHz)
{
    constexpr int order = 15, size = 1 << order;

    juce::dsp::FFT fft (order);
    std::vector<float> data ((size_t) size * 2, 0.0f);

    for (int i = 0; i < size && start + i < (int) samples.size(); ++i)
        data[(size_t) i] = samples[(size_t) (start + i)] * (float) (0.5 - 0.5 * std::cos (2.0 * pi * i / size));

    fft.performFrequencyOnlyForwardTransform (data.data());

    auto energy = 0.0;

    for (int k = 0; k < size / 2; ++k)
    {
        const auto hz = k * rate / size;

        if (hz >= lowHz && hz <= highHz)
            energy += (double) data[(size_t) k] * data[(size_t) k];
    }

    return energy;
}
} // namespace

TEST_CASE ("Adaptive: the estimate follows a 10 dB rise in noise in Adapt speed, at every FFT size", "[fir][adaptive]")
{
    const auto quiet = pink ((int) rate * 10, -50.0, 1);
    const auto loud = pink ((int) rate * 10, -40.0, 2);
    std::vector<float> input = quiet;
    input.insert (input.end(), loud.begin(), loud.end());

    for (const auto size : { 1024, 2048, 8192 })
    {
        INFO ("FFT " << size);

        const auto quietLevel = learnedLevelDb (pink ((int) rate * 4, -50.0, 3), size);
        const auto loudLevel = learnedLevelDb (pink ((int) rate * 4, -40.0, 4), size);

        fir::Shared shared;
        SpectralEngine engine ({ size, false, false }, rate, shared);

        auto beforeStep = 0.0;
        auto reachedAt = -1.0;

        run (engine, input, subtractSettings (fir::adaptiveSource), [&] (int done)
        {
            if (! shared.hasAdaptive.load())
                return;

            const auto level = gridAverageDb (shared.adaptive);

            if (done <= (int) rate * 10)
                beforeStep = level;
            else if (reachedAt < 0.0 && level >= loudLevel - 2.0)
                reachedAt = (double) (done - (int) rate * 10) / rate;
        });

        INFO ("before the step " << beforeStep << " dB against " << quietLevel << " dB; reached the new level after " << reachedAt << " s");
        CHECK (std::abs (beforeStep - quietLevel) < 2.0);
        CHECK (reachedAt >= 1.5);
        CHECK (reachedAt <= 2.5);
    }
}

TEST_CASE ("Adaptive: a sustained sine isn't learned as noise", "[fir][adaptive]")
{
    const auto numSamples = (int) rate * 30;
    const auto tone = sine (numSamples, 1000.0, 0.1);                       // -20 dBFS
    const auto input = plus (tone, white (numSamples, -60.0, 5));

    fir::Shared shared;
    SpectralEngine engine ({ 2048, false, false }, rate, shared);
    const auto output = run (engine, input, subtractSettings (fir::adaptiveSource));

    const auto latency = engine.getLatency();
    const auto start = numSamples - (1 << 15) - latency - 1;
    const auto change = 10.0 * std::log10 (bandEnergy (output, start + latency, 990.0, 1010.0) / bandEnergy (tone, start, 990.0, 1010.0));

    INFO ("the sine changes by " << change << " dB after 30 s");
    CHECK (std::abs (change) < 0.5);
}

TEST_CASE ("Adaptive: tone bursts over constant noise leave the estimate at the noise", "[fir][adaptive]")
{
    const auto numSamples = (int) rate * 20;
    const auto noise = white (numSamples, -50.0, 6);
    auto bursts = sine (numSamples, 700.0, 0.2);

    // 300 ms on, 300 ms off.
    for (int i = 0; i < numSamples; ++i)
        if ((i / (int) (rate * 0.3)) % 2 == 1)
            bursts[(size_t) i] = 0.0f;

    const auto reference = learnedLevelDb (white ((int) rate * 4, -50.0, 7), 2048);

    fir::Shared shared;
    SpectralEngine engine ({ 2048, false, false }, rate, shared);

    auto worst = 0.0;

    run (engine, plus (noise, bursts), subtractSettings (fir::adaptiveSource), [&] (int done)
    {
        if (done > (int) rate * 4 && shared.hasAdaptive.load())
            worst = std::max (worst, std::abs (gridAverageDb (shared.adaptive) - reference));
    });

    INFO ("the estimate is at most " << worst << " dB from the noise");
    CHECK (worst < 2.0);
}

TEST_CASE ("Adaptive: musical noise is no worse than 1.5 x Learned", "[fir][adaptive]")
{
    constexpr int size = 2048, learnSamples = 96000, testSamples = 240000;

    const auto residualVariance = [&] (int source)
    {
        fir::Shared shared;
        SpectralEngine engine ({ size, false, false }, rate, shared);
        auto settings = subtractSettings (source);
        settings.reductionGain = fancy::dbToGain (-18.0);

        if (source == fir::learnedSource)
        {
            shared.learnTarget.store (learnSamples);
            shared.learnRequested.store (true);
            run (engine, white (learnSamples + size, -26.0, 6), settings);
        }

        const auto input = plus (white (testSamples, -26.0, 7), sine (testSamples, 1000.0, 0.25));
        const auto output = run (engine, input, settings);
        const auto latency = engine.getLatency();

        // Frame-to-frame variance of the residual in dB, in bins away from the sine.
        juce::dsp::FFT fft (11);
        std::vector<double> sum ((size_t) size / 2 + 1, 0.0), squares ((size_t) size / 2 + 1, 0.0);
        auto count = 0;

        for (int start = 120000; start + size + latency < testSamples; start += size / 4, ++count)
        {
            std::vector<float> data ((size_t) size * 2, 0.0f);

            for (int n = 0; n < size; ++n)
                data[(size_t) n] = output[(size_t) (start + latency + n)] * (float) std::sqrt (0.5 - 0.5 * std::cos (2.0 * pi * n / size));

            fft.performRealOnlyForwardTransform (data.data(), true);

            for (int k = 0; k <= size / 2; ++k)
            {
                const auto power = (double) data[(size_t) k * 2] * data[(size_t) k * 2] + (double) data[(size_t) k * 2 + 1] * data[(size_t) k * 2 + 1];
                const auto db = 10.0 * std::log10 (std::max (power, 1.0e-20));
                sum[(size_t) k] += db;
                squares[(size_t) k] += db * db;
            }
        }

        auto total = 0.0;
        auto bins = 0;

        for (int k = 20; k < size / 2 - 20; ++k)
        {
            if (std::abs (k * rate / size - 1000.0) < 300.0)
                continue;

            const auto mean = sum[(size_t) k] / count;
            total += squares[(size_t) k] / count - mean * mean;
            ++bins;
        }

        return total / bins;
    };

    const auto learned = residualVariance (fir::learnedSource);
    const auto adaptive = residualVariance (fir::adaptiveSource);

    std::printf ("fir adaptive: residual variance, Learned %.1f dB^2, Adaptive %.1f dB^2\n", learned, adaptive);
    INFO ("Learned " << learned << " dB^2, Adaptive " << adaptive << " dB^2");
    CHECK (adaptive <= 1.5 * learned);
}

TEST_CASE ("Adaptive: an FFT size change carries the estimate over", "[fir][adaptive]")
{
    fir::Shared shared;
    const auto noise = pink ((int) rate * 6, -45.0, 8);

    SpectralEngine first ({ 2048, false, false }, rate, shared);
    run (first, noise, subtractSettings (fir::adaptiveSource));

    REQUIRE (shared.hasAdaptive.load());
    const auto before = gridAverageDb (shared.adaptive);

    // The new engine starts from the published estimate: after one frame it's the same.
    SpectralEngine second ({ 4096, false, false }, rate, shared);
    run (second, std::vector<float> (noise.begin(), noise.begin() + 4096 * 3), subtractSettings (fir::adaptiveSource));

    const auto after = gridAverageDb (shared.adaptive);
    INFO ("before " << before << " dB, after " << after << " dB");
    CHECK (std::abs (after - before) < 1.0);
}

TEST_CASE ("Adaptive: Learned is bit-identical, and older states load as Learned", "[fir][adaptive][compat]")
{
    // The same engine output whether the tracker exists or not: Learned never runs it.
    {
        const auto input = plus (white ((int) rate * 4, -40.0, 9), sine ((int) rate * 4, 500.0, 0.2));

        fir::Shared a, b;
        SpectralEngine learnedA ({ 2048, false, false }, rate, a), learnedB ({ 2048, false, false }, rate, b);

        for (auto* shared : { &a, &b })
        {
            shared->learnTarget.store (48000);
            shared->learnRequested.store (true);
        }

        auto settings = subtractSettings (fir::learnedSource);
        const auto outA = run (learnedA, input, settings);

        settings.adaptSeconds = 7.0; // ignored in Learned
        const auto outB = run (learnedB, input, settings);

        CHECK (outA == outB);
    }

    FirProcessor original;
    original.getApvts().getParameter ("mode")->setValueNotifyingHost (original.getApvts().getParameter ("mode")->convertTo0to1 (3.0f));

    juce::MemoryBlock saved;
    original.getStateInformation (saved);

    auto tree = juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize()));

    for (int i = tree.getNumChildren(); --i >= 0;)
    {
        const auto id = tree.getChild (i).getProperty ("id").toString();

        if (id == "source" || id == "adaptSpeed")
            tree.removeChild (i, nullptr);
    }

    tree.setProperty ("stateVersion", 2, nullptr);
    juce::MemoryBlock older;
    juce::AudioProcessor::copyXmlToBinary (*tree.createXml(), older);

    FirProcessor edited;
    edited.getApvts().getParameter ("source")->setValueNotifyingHost (1.0f);
    edited.setStateInformation (older.getData(), (int) older.getSize());

    CHECK (edited.getApvts().getRawParameterValue ("source")->load() == 0.0f);
    CHECK (edited.getApvts().getRawParameterValue ("adaptSpeed")->load() == 2.0f);
}

// CPU, by the method in §0.4: Adaptive at FFT 2048 against Learned.
TEST_CASE ("Adaptive CPU measurement", "[.measure]")
{
    const auto measure = [] (int source)
    {
        std::vector<double> runs;
        const auto input = white ((int) rate * 10, -40.0, 11);

        for (int attempt = 0; attempt < 5; ++attempt)
        {
            fir::Shared shared;
            SpectralEngine engine ({ 2048, false, false }, rate, shared);

            if (source == fir::learnedSource)
            {
                // A profile, so Learned does its full work.
                for (auto& value : shared.profile)
                    value.store (1.0e-6f);

                shared.hasProfile.store (true);
                shared.profileVersion.fetch_add (1);
            }

            const auto settings = subtractSettings (source);
            std::vector<float> left (128), right (128);
            std::chrono::duration<double> spent {};

            for (int position = 0; position + 128 <= (int) input.size(); position += 128)
            {
                const auto start = std::chrono::steady_clock::now();
                engine.process (input.data() + position, input.data() + position, nullptr, nullptr, left.data(), right.data(), 2, 128, settings, true);
                spent += std::chrono::steady_clock::now() - start;
            }

            runs.push_back (100.0 * spent.count() / 10.0);
        }

        std::sort (runs.begin(), runs.end());
        return runs[2];
    };

    const auto learned = measure (fir::learnedSource);
    const auto adaptive = measure (fir::adaptiveSource);

    std::printf ("fir adaptive cpu (median of 5, 48 kHz, 128 samples, stereo, FFT 2048): Learned %.3f %%, Adaptive %.3f %% (%.2f x, target < 1.5 x)\n",
                 learned, adaptive, adaptive / learned);
}
