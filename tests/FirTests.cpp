#include "Curve.h"
#include "FirDsp.h"
#include "FirEditor.h"
#include "FirGraph.h"
#include "FirProcessor.h"
#include "SpectralEngine.h"

#include <fancy/Decibels.h>
#include <fancy/dsp/Analyzer.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <juce_dsp/juce_dsp.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <numbers>
#include <random>
#include <vector>

using Catch::Approx;
using fir::SpectralEngine;

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = std::numbers::pi;

void setCurve (fir::Shared& shared, int curve, float db)
{
    for (auto& value : shared.curves[(size_t) curve])
        value.store (db);

    shared.curveVersions[(size_t) curve].fetch_add (1);
}

// Each channel run through an engine in blocks.
struct Run
{
    std::vector<float> left, right;
};

Run runEngine (SpectralEngine& engine, const std::vector<float>& inLeft, const std::vector<float>& inRight,
               const fir::FrameSettings& settings, const std::vector<float>* keyLeft = nullptr, int blockSize = 64, bool learns = true)
{
    const auto numSamples = (int) inLeft.size();

    Run run { std::vector<float> ((size_t) numSamples), std::vector<float> ((size_t) numSamples) };

    for (int position = 0; position < numSamples; position += blockSize)
    {
        const auto count = std::min (blockSize, numSamples - position);
        const auto* key = keyLeft != nullptr ? keyLeft->data() + position : nullptr;

        engine.process (inLeft.data() + position, inRight.data() + position, key, key,
                        run.left.data() + position, run.right.data() + position, 2, count, settings, learns);
    }

    return run;
}

std::vector<float> noise (int numSamples, float level, unsigned seed)
{
    std::mt19937 random (seed);
    std::normal_distribution<float> gauss (0.0f, level);

    std::vector<float> samples ((size_t) numSamples);

    for (auto& sample : samples)
        sample = gauss (random);

    return samples;
}

std::vector<float> sine (int numSamples, double hz, double level)
{
    std::vector<float> samples ((size_t) numSamples);

    for (int i = 0; i < numSamples; ++i)
        samples[(size_t) i] = (float) (level * std::sin (2.0 * pi * hz * i / rate));

    return samples;
}

// Energy between two frequencies, from one large FFT of a stretch of signal.
double bandEnergy (const std::vector<float>& samples, int start, double lowHz, double highHz)
{
    constexpr int order = 15, size = 1 << order;

    juce::dsp::FFT fft (order);
    std::vector<float> data ((size_t) size * 2, 0.0f);

    for (int i = 0; i < size && start + i < (int) samples.size(); ++i)
        data[(size_t) i] = samples[(size_t) (start + i)] * (float) (0.5 - 0.5 * std::cos (2.0 * pi * i / size));

    fft.performFrequencyOnlyForwardTransform (data.data());

    auto energy = 0.0;

    for (int bin = (int) (lowHz * size / rate); bin <= (int) (highHz * size / rate); ++bin)
        energy += (double) data[(size_t) bin] * (double) data[(size_t) bin];

    return energy;
}

double rmsDb (const std::vector<float>& samples, int from, int to)
{
    auto sum = 0.0;

    for (int i = from; i < to; ++i)
        sum += (double) samples[(size_t) i] * (double) samples[(size_t) i];

    return fancy::gainToDb (std::sqrt (sum / (double) (to - from)), -300.0);
}

void setParameter (FirProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getApvts().getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

std::vector<float> render (FirProcessor& processor, int numSamples)
{
    juce::AudioBuffer<float> buffer (4, numSamples);
    juce::MidiBuffer midi;

    buffer.clear();

    const auto left = noise (numSamples, 0.1f, 21), right = noise (numSamples, 0.1f, 22);

    for (int i = 0; i < numSamples; ++i)
    {
        buffer.setSample (0, i, left[(size_t) i]);
        buffer.setSample (1, i, right[(size_t) i]);
    }

    processor.processBlock (buffer, midi);

    std::vector<float> samples (buffer.getReadPointer (0), buffer.getReadPointer (0) + numSamples);
    samples.insert (samples.end(), buffer.getReadPointer (1), buffer.getReadPointer (1) + numSamples);
    return samples;
}
} // namespace

// Curves --------------------------------------------------------------------------------------

TEST_CASE ("smooth curves never overshoot their points", "[fir]")
{
    const std::vector<fir::CurvePoint> points { { 100.0, 0.0 }, { 200.0, 6.0 }, { 400.0, 6.0 }, { 800.0, -6.0 }, { 1600.0, -6.0 } };

    for (auto hz = 100.0; hz <= 1600.0; hz *= 1.01)
    {
        const auto db = fir::evaluateCurve (points, true, hz, 0.0);

        REQUIRE (db <= 6.0 + 1.0e-9);
        REQUIRE (db >= -6.0 - 1.0e-9);
    }

    // Flat between equal points, and through every point.
    CHECK (fir::evaluateCurve (points, true, 300.0, 0.0) == Approx (6.0));

    for (const auto& point : points)
        CHECK (fir::evaluateCurve (points, true, point.hz, 0.0) == Approx (point.db));
}

TEST_CASE ("flat curves are straight lines, and ends hold", "[fir]")
{
    const std::vector<fir::CurvePoint> points { { 100.0, 0.0 }, { 400.0, 12.0 } };

    CHECK (fir::evaluateCurve (points, false, 200.0, 0.0) == Approx (6.0));
    CHECK (fir::evaluateCurve (points, false, 20.0, 0.0) == 0.0);
    CHECK (fir::evaluateCurve (points, false, 20000.0, 0.0) == 12.0);
    CHECK (fir::evaluateCurve ({}, true, 1000.0, -30.0) == -30.0);
}

TEST_CASE ("the graph's axes match the mockup's", "[fir]")
{
    constexpr float width = 610.0f, height = 196.0f;

    CHECK (FirGraph::xForHz (20.0, width) == Approx (34.0f));
    CHECK (FirGraph::xForHz (20000.0, width) == Approx (600.0f));
    CHECK (FirGraph::yForDb (24.0f, height, 24.0f, -90.0f) == Approx (10.0f));
    CHECK (FirGraph::yForDb (-90.0f, height, 24.0f, -90.0f) == Approx (174.0f));
    CHECK (FirGraph::dbForY (FirGraph::yForDb (-30.0f, height, 24.0f, -90.0f), height, 24.0f, -90.0f) == Approx (-30.0f));
}

// Latency -------------------------------------------------------------------------------------

TEST_CASE ("reported latency is the measured impulse delay", "[fir]")
{
    const auto size = GENERATE (128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768);
    const auto lowLatency = GENERATE (false, true);
    const auto mode = GENERATE (fir::eq, fir::gate, fir::compressor, fir::subtract, fir::convolve);

    fir::Shared shared;

    // Thresholds where nothing changes: Gate open, Compressor idle.
    setCurve (shared, fir::gate, -300.0f);
    setCurve (shared, fir::compressor, 100.0f);

    SpectralEngine engine ({ size, lowLatency, mode == fir::convolve }, rate, shared);

    fir::FrameSettings settings;
    settings.mode = mode;
    settings.reduceArtifacts = false;

    const auto latency = engine.getLatency();
    const auto numSamples = latency + size * 3;
    const auto impulseAt = size + 37;

    std::vector<float> left ((size_t) numSamples, 0.0f), right ((size_t) numSamples, 0.0f);
    left[(size_t) impulseAt] = 0.5f;

    if (mode == fir::convolve)
    {
        // The right channel is the impulse response, renewed at every block start.
        for (int i = 0; i < numSamples; i += size)
            right[(size_t) i] = 1.0f;
    }
    else
    {
        right[(size_t) impulseAt] = 0.5f;
    }

    const auto run = runEngine (engine, left, right, settings);

    auto peakAt = 0;

    for (int i = 0; i < numSamples; ++i)
        if (std::abs (run.left[(size_t) i]) > std::abs (run.left[(size_t) peakAt]))
            peakAt = i;

    INFO ("size " << size << ", low latency " << lowLatency << ", mode " << mode);
    CHECK (latency == SpectralEngine::latencyOf ({ size, lowLatency, mode == fir::convolve }));
    CHECK (peakAt - impulseAt == latency);
    CHECK (run.left[(size_t) peakAt] == Approx (0.5f).margin (1.0e-3));
}

TEST_CASE ("latency halves with Low latency and the mockup's 2048 is 2048", "[fir]")
{
    CHECK (SpectralEngine::latencyOf ({ 2048, false, false }) == 2048);
    CHECK (SpectralEngine::latencyOf ({ 2048, true, false }) == 1024);
    CHECK (SpectralEngine::latencyOf ({ 32768, false, false }) == 32768 + 4096);
    CHECK (SpectralEngine::latencyOf ({ 32768, true, false }) == 16384 + 4096);
}

TEST_CASE ("a flat EQ reconstructs the input exactly", "[fir]")
{
    const auto size = GENERATE (128, 1024, 4096, 16384);
    const auto lowLatency = GENERATE (false, true);

    fir::Shared shared;
    SpectralEngine engine ({ size, lowLatency, false }, rate, shared);

    fir::FrameSettings settings;

    const auto latency = engine.getLatency();
    const auto numSamples = latency + 48000;
    const auto input = noise (numSamples, 0.25f, 3);

    const auto run = runEngine (engine, input, input, settings, nullptr, 100);

    auto errorEnergy = 0.0, inputEnergy = 0.0;

    for (int i = latency + size; i < numSamples; ++i)
    {
        const auto error = (double) run.left[(size_t) i] - (double) input[(size_t) (i - latency)];
        errorEnergy += error * error;
        inputEnergy += (double) input[(size_t) (i - latency)] * (double) input[(size_t) (i - latency)];
    }

    INFO ("size " << size << ", low latency " << lowLatency << ", null " << 10.0 * std::log10 (errorEnergy / inputEnergy) << " dB");
    CHECK (10.0 * std::log10 (errorEnergy / inputEnergy) < -100.0);
}

// EQ ------------------------------------------------------------------------------------------

TEST_CASE ("the EQ curve sets the gain", "[fir]")
{
    fir::Shared shared;
    setCurve (shared, fir::eq, -6.0f);

    SpectralEngine engine ({ 4096, false, false }, rate, shared);

    fir::FrameSettings settings;

    const auto input = sine (48000, 1000.0, 0.25);
    const auto run = runEngine (engine, input, input, settings);

    CHECK (rmsDb (run.left, 24000, 48000) - rmsDb (input, 0, 24000) == Approx (-6.0).margin (0.05));
}

// Dynamics ------------------------------------------------------------------------------------

TEST_CASE ("Gate opens above its threshold and holds Floor below it", "[fir]")
{
    fir::Shared shared;
    setCurve (shared, fir::gate, -30.0f);

    SpectralEngine engine ({ 2048, false, false }, rate, shared);

    fir::FrameSettings settings;
    settings.mode = fir::gate;
    settings.gateFloorGain = 0.1;

    const auto loud = sine (48000, 1000.0, 0.25);
    const auto quiet = sine (48000, 1000.0, 0.005);

    {
        const auto run = runEngine (engine, loud, loud, settings);
        CHECK (std::abs (rmsDb (run.left, 24000, 48000) - rmsDb (loud, 0, 24000)) < 0.2);
    }

    {
        SpectralEngine closed ({ 2048, false, false }, rate, shared);
        const auto run = runEngine (closed, quiet, quiet, settings);
        CHECK (rmsDb (run.left, 24000, 48000) - rmsDb (quiet, 0, 24000) == Approx (-20.0).margin (0.5));
    }
}

TEST_CASE ("release time doesn't depend on the FFT size", "[fir]")
{
    constexpr double releaseMs = 2000.0;

    const auto size = GENERATE (1024, 8192);

    fir::Shared shared;
    setCurve (shared, fir::compressor, -40.0f);

    SpectralEngine engine ({ size, false, false }, rate, shared);

    fir::FrameSettings settings;
    settings.mode = fir::compressor;
    settings.compSlope = 0.99;
    settings.compAttackMs = 0.0;
    settings.compReleaseMs = releaseMs;

    constexpr int loudSamples = 96000, totalSamples = 96000 + 4 * 48000;

    auto input = sine (totalSamples, 1000.0, 0.5);

    for (int i = loudSamples; i < totalSamples; ++i)
        input[(size_t) i] *= 0.002f;

    // The reduction in the 1 kHz column, read every block.
    const auto column = (int) (std::log (1000.0 / 20.0) / std::log (1000.0) * fir::displayColumns);

    std::vector<float> reduction;
    std::vector<float> outLeft (512), outRight (512);

    for (int position = 0; position < totalSamples; position += 512)
    {
        engine.process (input.data() + position, input.data() + position, nullptr, nullptr,
                        outLeft.data(), outRight.data(), 2, 512, settings, true);

        reduction.push_back (shared.reduction[(size_t) column].load());
    }

    const auto blockMs = 512.0 / rate * 1000.0;
    const auto steady = (double) reduction[(size_t) (loudSamples / 512 - 4)];

    REQUIRE (steady > 20.0);

    // From the first block where it's falling, to 63 % of the way down.
    auto start = -1, reached = -1;

    for (int i = loudSamples / 512; i < (int) reduction.size(); ++i)
    {
        if (start < 0 && (double) reduction[(size_t) i] < steady * 0.98)
            start = i;

        if (start >= 0 && (double) reduction[(size_t) i] <= steady * std::exp (-1.0))
        {
            reached = i;
            break;
        }
    }

    REQUIRE (reached > start);

    const auto measured = (reached - start) * blockMs;

    INFO ("size " << size << ", measured " << measured << " ms against " << releaseMs);
    CHECK (measured == Approx (releaseMs).epsilon (0.1));
}

TEST_CASE ("a 1 kHz sidechain reduces only the bins near 1 kHz", "[fir]")
{
    fir::Shared shared;
    setCurve (shared, fir::compressor, -40.0f);

    SpectralEngine engine ({ 4096, false, false }, rate, shared);

    fir::FrameSettings settings;
    settings.mode = fir::compressor;
    settings.compSlope = 0.9;
    settings.compAttackMs = 1.0;
    settings.sidechain = true;

    constexpr int numSamples = 96000;

    const auto main = noise (numSamples, 0.05f, 4);
    const auto key = sine (numSamples, 1000.0, 0.5);

    const auto run = runEngine (engine, main, main, settings, &key);

    const auto latency = engine.getLatency();
    const auto start = 48000;

    const auto change = [&] (double low, double high)
    {
        return 10.0 * std::log10 (bandEnergy (run.left, start, low, high) / bandEnergy (main, start - latency, low, high));
    };

    INFO ("near 1 kHz " << change (950.0, 1050.0) << " dB, 4-6 kHz " << change (4000.0, 6000.0) << " dB, 200-400 Hz " << change (200.0, 400.0) << " dB");
    CHECK (change (950.0, 1050.0) < -10.0);
    CHECK (std::abs (change (4000.0, 6000.0)) < 0.5);
    CHECK (std::abs (change (200.0, 400.0)) < 0.5);
}

// Subtract ------------------------------------------------------------------------------------

TEST_CASE ("learning captures a fixed length and stops by itself", "[fir]")
{
    fir::Shared shared;
    SpectralEngine engine ({ 2048, false, false }, rate, shared);

    fir::FrameSettings settings;
    settings.mode = fir::subtract;

    shared.learnTarget.store (48000);
    shared.learnRequested.store (true);

    const auto input = noise (96000, 0.05f, 5);
    runEngine (engine, input, input, settings);

    CHECK_FALSE (shared.learning.load());
    CHECK (shared.hasProfile.load());
    CHECK (shared.profileSeconds.load() == Approx (1.0f).margin (0.05f));

    // It doesn't go on learning the music.
    const auto version = shared.profileVersion.load();
    const auto music = sine (48000, 440.0, 0.5);
    runEngine (engine, music, music, settings);

    CHECK (shared.profileVersion.load() == version);
}

TEST_CASE ("subtraction leaves the sine and doesn't warble", "[fir]")
{
    constexpr int size = 2048, learnSamples = 96000, testSamples = 192000;
    constexpr float noiseLevel = 0.05f;

    fir::Shared shared;
    SpectralEngine engine ({ size, false, false }, rate, shared);

    fir::FrameSettings settings;
    settings.mode = fir::subtract;
    settings.reductionGain = fancy::dbToGain (-18.0);
    settings.smoothing = 0.35;
    settings.subReleaseMs = 80.0;

    shared.learnTarget.store (learnSamples);
    shared.learnRequested.store (true);

    const auto learnNoise = noise (learnSamples + size, noiseLevel, 6);
    runEngine (engine, learnNoise, learnNoise, settings);

    REQUIRE (shared.hasProfile.load());

    const auto testNoise = noise (testSamples, noiseLevel, 7);
    const auto tone = sine (testSamples, 1000.0, 0.25);

    std::vector<float> input ((size_t) testSamples);

    for (int i = 0; i < testSamples; ++i)
        input[(size_t) i] = testNoise[(size_t) i] + tone[(size_t) i];

    const auto run = runEngine (engine, input, input, settings);
    const auto latency = engine.getLatency();

    // The sine keeps its level.
    const auto sineChange = 10.0 * std::log10 (bandEnergy (run.left, 96000, 990.0, 1010.0) / bandEnergy (tone, 96000 - latency, 990.0, 1010.0));

    INFO ("sine change " << sineChange << " dB");
    CHECK (std::abs (sineChange) < 0.5);

    // Plain magnitude subtraction, as the reference for musical noise: the same frames, a gain
    // of max (floor, 1 - noise / signal) and nothing else.
    juce::dsp::FFT fft (11);

    std::vector<double> window ((size_t) size);
    auto energy = 0.0;

    for (int n = 0; n < size; ++n)
    {
        window[(size_t) n] = std::sqrt (0.5 - 0.5 * std::cos (2.0 * pi * n / size));
        energy += window[(size_t) n] * window[(size_t) n];
    }

    const auto spectrumOf = [&] (const std::vector<float>& samples, int start)
    {
        std::vector<float> data ((size_t) size * 2, 0.0f);

        for (int n = 0; n < size; ++n)
            data[(size_t) n] = samples[(size_t) (start + n)] * (float) window[(size_t) n];

        fft.performRealOnlyForwardTransform (data.data(), true);

        std::vector<double> power ((size_t) size / 2 + 1);

        for (int k = 0; k <= size / 2; ++k)
            power[(size_t) k] = ((double) data[(size_t) k * 2] * data[(size_t) k * 2] + (double) data[(size_t) k * 2 + 1] * data[(size_t) k * 2 + 1]) / energy;

        return power;
    };

    // Noise per bin, from the same noise before the test.
    std::vector<double> noisePower ((size_t) size / 2 + 1, 0.0);
    auto frames = 0;

    for (int start = 0; start + size < learnSamples; start += size / 4, ++frames)
    {
        const auto power = spectrumOf (learnNoise, start);

        for (size_t k = 0; k < power.size(); ++k)
            noisePower[k] += power[k];
    }

    for (auto& value : noisePower)
        value /= (double) frames;

    // Frame-to-frame variance of the residual in dB, in bins well away from the sine.
    const auto floorGain = fancy::dbToGain (-18.0);

    const auto variance = [&] (bool plain)
    {
        std::vector<double> sum ((size_t) size / 2 + 1, 0.0), squares ((size_t) size / 2 + 1, 0.0);
        auto count = 0;

        for (int start = 96000; start + size < testSamples - size; start += size / 4, ++count)
        {
            auto power = plain ? spectrumOf (input, start) : spectrumOf (run.left, start + latency);

            if (plain)
                for (size_t k = 0; k < power.size(); ++k)
                    power[k] *= std::pow (std::max (floorGain, 1.0 - noisePower[k] / std::max (power[k], 1.0e-20)), 2.0);

            for (size_t k = 0; k < power.size(); ++k)
            {
                const auto db = 10.0 * std::log10 (std::max (power[k], 1.0e-20));
                sum[k] += db;
                squares[k] += db * db;
            }
        }

        auto total = 0.0;
        auto bins = 0;

        for (int k = 20; k < size / 2 - 20; ++k)
        {
            const auto hz = (double) k * rate / size;

            if (std::abs (hz - 1000.0) < 300.0)
                continue;

            const auto mean = sum[(size_t) k] / count;
            total += squares[(size_t) k] / count - mean * mean;
            ++bins;
        }

        return total / bins;
    };

    const auto plainVariance = variance (true);
    const auto wienerVariance = variance (false);

    INFO ("residual variance: plain " << plainVariance << " dB^2, Fancy FIR " << wienerVariance << " dB^2");
    CHECK (wienerVariance < 0.7 * plainVariance);
}

TEST_CASE ("Subtract with no profile passes the signal", "[fir]")
{
    fir::Shared shared;
    SpectralEngine engine ({ 1024, false, false }, rate, shared);

    fir::FrameSettings settings;
    settings.mode = fir::subtract;

    const auto input = noise (48000, 0.1f, 8);
    const auto run = runEngine (engine, input, input, settings);

    CHECK (rmsDb (run.left, 24000, 48000) == Approx (rmsDb (input, 24000 - engine.getLatency(), 48000 - engine.getLatency())).margin (0.05));
}

TEST_CASE ("Listen gives the removed part", "[fir]")
{
    fir::Shared shared;
    setCurve (shared, fir::gate, -30.0f);

    fir::FrameSettings settings;
    settings.mode = fir::gate;
    settings.gateFloorGain = 0.0;
    settings.reduceArtifacts = false;

    const auto quiet = sine (48000, 1000.0, 0.005);

    SpectralEngine plain ({ 2048, false, false }, rate, shared);
    const auto gated = runEngine (plain, quiet, quiet, settings);

    settings.listen = true;

    SpectralEngine listening ({ 2048, false, false }, rate, shared);
    const auto removed = runEngine (listening, quiet, quiet, settings);

    // Everything was below the threshold, so the removed part is all of it.
    CHECK (rmsDb (gated.left, 24000, 48000) < rmsDb (quiet, 0, 24000) - 40.0);
    CHECK (rmsDb (removed.left, 24000, 48000) == Approx (rmsDb (quiet, 0, 24000)).margin (0.2));
}

// Analysis ------------------------------------------------------------------------------------

TEST_CASE ("the analysis reads a -12 dBFS sine as -12 dB", "[fir]")
{
    fancy::dsp::SpectrumAnalyzer analyzer;
    analyzer.configure (rate, {});

    const auto tone = sine (48000, 1000.0, fancy::dbToGain (-12.0));
    analyzer.addSamples (tone.data(), (int) tone.size());

    CHECK (analyzer.levelBetween (970.0, 1030.0) == Approx (-12.0f).margin (0.5f));
}

// Engine changes ------------------------------------------------------------------------------

TEST_CASE ("changing size mid-stream ends on the new latency without a step", "[fir]")
{
    constexpr int numSamples = 96000, changeAt = 24000;

    FirDsp dsp;
    dsp.setSynchronousBuilds (true);

    FirDsp::Targets targets;
    targets.engine.fftSize = 4096;

    dsp.prepare (rate, targets);

    CHECK (dsp.getLatencySamples() == 4096);

    const auto input = sine (numSamples, 440.0, 0.25);
    auto left = input, right = input;

    auto largest = 0.0;

    for (int position = 0; position < numSamples; position += 128)
    {
        if (position >= changeAt)
            targets.engine.fftSize = 1024;

        float* channels[] { left.data() + position, right.data() + position };

        dsp.setTargets (targets);
        dsp.process (channels, 2, nullptr, 0, 128);
    }

    for (int i = 4096 + 1; i < numSamples; ++i)
        largest = std::max (largest, std::abs ((double) left[(size_t) i] - (double) left[(size_t) i - 1]));

    CHECK (dsp.getLatencySamples() == 1024);
    CHECK (largest < 0.25 * 2.0 * pi * 440.0 / rate * 1.2);

    for (int i = 60000; i < numSamples; ++i)
        REQUIRE (left[(size_t) i] == Approx (input[(size_t) (i - 1024)]).margin (1.0e-4));
}

TEST_CASE ("every sample rate and block size reconstructs", "[fir]")
{
    const auto sampleRate = GENERATE (44100.0, 96000.0, 192000.0, 384000.0);
    const auto blockSize = GENERATE (1, 64, 8192);

    FirDsp dsp;
    dsp.setSynchronousBuilds (true);

    FirDsp::Targets targets;
    targets.engine.fftSize = 2048;

    dsp.prepare (sampleRate, targets);

    const auto latency = dsp.getLatencySamples();
    const auto numSamples = latency + 30000;

    const auto input = noise (numSamples, 0.2f, 9);
    auto left = input, right = input;

    for (int position = 0; position < numSamples; position += blockSize)
    {
        const auto count = std::min (blockSize, numSamples - position);
        float* channels[] { left.data() + position, right.data() + position };

        dsp.setTargets (targets);
        dsp.process (channels, 2, nullptr, 0, count);
    }

    for (int i = latency + 4096; i < numSamples; ++i)
        REQUIRE (left[(size_t) i] == Approx (input[(size_t) (i - latency)]).margin (1.0e-4));
}

TEST_CASE ("mono passes, including Convolve L/R", "[fir]")
{
    const auto mode = GENERATE (fir::eq, fir::convolve);

    FirDsp dsp;
    dsp.setSynchronousBuilds (true);

    FirDsp::Targets targets;
    targets.engine.fftSize = 1024;
    targets.engine.convolve = mode == fir::convolve;
    targets.frame.mode = mode;

    dsp.prepare (rate, targets);

    const auto latency = dsp.getLatencySamples();
    const auto input = noise (20000, 0.2f, 10);
    auto samples = input;

    float* channels[] { samples.data() };

    dsp.setTargets (targets);
    dsp.process (channels, 1, nullptr, 0, (int) samples.size());

    for (int i = latency + 2048; i < (int) samples.size(); ++i)
        REQUIRE (samples[(size_t) i] == Approx (input[(size_t) (i - latency)]).margin (1.0e-4));
}

TEST_CASE ("random settings stay finite", "[fir]")
{
    std::mt19937 random (11);
    std::uniform_real_distribution<double> unit (0.0, 1.0);

    FirDsp dsp;
    dsp.setSynchronousBuilds (true);

    FirDsp::Targets targets;
    dsp.prepare (rate, targets);

    float left[1024], right[1024], keyLeft[1024], keyRight[1024];
    float* channels[] { left, right };
    const float* key[] { keyLeft, keyRight };

    for (int round = 0; round < 120; ++round)
    {
        for (int curve = 0; curve < fir::numCurves; ++curve)
            setCurve (dsp.shared, curve, (float) (unit (random) * 150.0 - 120.0));

        auto& frame = targets.frame;

        frame.mode = (int) (unit (random) * 5.0);
        frame.gateFloorGain = unit (random);
        frame.gateAttackMs = unit (random) * 200.0;
        frame.gateReleaseMs = unit (random) * 2000.0;
        frame.compSlope = 1.0 - 1.0 / (0.5 * std::pow (200.0, unit (random)));
        frame.compAttackMs = unit (random) * 200.0;
        frame.compReleaseMs = unit (random) * 2000.0;
        frame.reductionGain = unit (random);
        frame.offsetPower = std::pow (10.0, unit (random) * 4.0 - 2.0);
        frame.smoothing = unit (random);
        frame.subReleaseMs = 10.0 + unit (random) * 490.0;
        frame.reduceArtifacts = unit (random) < 0.5;
        frame.listen = unit (random) < 0.3;
        frame.sidechain = unit (random) < 0.5;

        targets.engine.fftSize = fir::sizes[(int) (unit (random) * 6.0)];
        targets.engine.lowLatency = unit (random) < 0.5;
        targets.engine.convolve = frame.mode == fir::convolve;

        if (unit (random) < 0.1)
            dsp.shared.learnRequested.store (true), dsp.shared.learnTarget.store (2048);

        for (int i = 0; i < 1024; ++i)
        {
            left[i] = (float) (unit (random) * 2.0 - 1.0);
            right[i] = (float) (unit (random) * 2.0 - 1.0);
            keyLeft[i] = (float) (unit (random) * 2.0 - 1.0);
            keyRight[i] = (float) (unit (random) * 2.0 - 1.0);
        }

        dsp.setTargets (targets);
        dsp.process (channels, 2, key, 2, 1024);

        for (int i = 0; i < 1024; ++i)
            REQUIRE ((std::isfinite (left[i]) && std::isfinite (right[i])));
    }
}

// Processor ---------------------------------------------------------------------------------

TEST_CASE ("fir automation with an unchanged value is bit-identical", "[fir]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    FirProcessor plain, automated;

    for (auto* processor : { &plain, &automated })
    {
        setParameter (*processor, "mode", (float) fir::compressor);
        setParameter (*processor, "compRatio", 3.3f);
        processor->setNonRealtime (true);
        processor->prepareToPlay (rate, 48000);
    }

    for (auto* parameter : automated.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
        const auto before = automated.getApvts().getRawParameterValue (ranged->getParameterID())->load();

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->getValue());
        parameter->endChangeGesture();

        INFO (ranged->getParameterID());
        CHECK (automated.getApvts().getRawParameterValue (ranged->getParameterID())->load() == before);
    }

    REQUIRE (render (plain, 48000) == render (automated, 48000));
}

TEST_CASE ("curves and the noise profile survive a save and load bit-identical", "[fir]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    FirProcessor source, restored;

    source.setPoints (fir::eq, { { 100.0, 3.0 }, { 1000.0, -6.0 }, { 8000.0, 2.5 } });
    source.setPoints (fir::gate, { { 500.0, -45.0 } });

    std::array<float, fir::curvePoints> table {};

    for (int i = 0; i < fir::curvePoints; ++i)
        table[(size_t) i] = (float) (i % 37) * 0.25f - 4.0f;

    source.setFreehand (fir::compressor, table);

    // A learned profile, as the audio thread would leave it.
    auto& shared = source.getShared();

    for (int i = 0; i < fir::profilePoints; ++i)
        shared.profile[(size_t) i].store (1.0e-7f * (float) (i + 1) / 3.0f);

    shared.profileSeconds.store (2.0f);
    shared.hasProfile.store (true);

    setParameter (source, "mode", (float) fir::subtract);

    juce::MemoryBlock state;
    source.getStateInformation (state);
    restored.setStateInformation (state.getData(), (int) state.getSize());

    CHECK (restored.getPoints (fir::eq) == source.getPoints (fir::eq));
    CHECK (restored.getPoints (fir::gate) == source.getPoints (fir::gate));
    CHECK (restored.getFreehand (fir::compressor) == table);
    CHECK (restored.getMode() == fir::subtract);

    REQUIRE (restored.getShared().hasProfile.load());
    CHECK (restored.getShared().profileSeconds.load() == 2.0f);

    for (int i = 0; i < fir::profilePoints; ++i)
    {
        const auto a = source.getShared().profile[(size_t) i].load();
        const auto b = restored.getShared().profile[(size_t) i].load();

        REQUIRE (std::memcmp (&a, &b, sizeof (float)) == 0);
    }
}

TEST_CASE ("a state with no profile loads as having none", "[fir]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    FirProcessor source, restored;

    restored.getShared().hasProfile.store (true);

    juce::MemoryBlock state;
    source.getStateInformation (state);
    restored.setStateInformation (state.getData(), (int) state.getSize());

    CHECK_FALSE (restored.getShared().hasProfile.load());
}

TEST_CASE ("damaged fir state is ignored", "[fir]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    FirProcessor processor;
    const char junk[] { 1, 2, 3, 4, 5, 6, 7, 8 };

    processor.setStateInformation (junk, (int) sizeof (junk));

    CHECK (processor.getMode() == fir::eq);
}

TEST_CASE ("every fir preset loads and plays", "[fir]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    FirProcessor processor;
    processor.setNonRealtime (true);

    auto& presets = processor.getPresets();
    const auto names = presets.getNames();

    REQUIRE (names.size() >= 6);

    for (int i = 0; i < names.size(); ++i)
    {
        presets.load (i);
        processor.prepareToPlay (rate, 48000);

        for (const auto sample : render (processor, 48000))
            REQUIRE (std::isfinite (sample));
    }

    presets.load (names.indexOf ("Gentle Tilt"));

    CHECK (processor.curveDbAt (fir::eq, 20.0) == Approx (-3.0));
    CHECK (processor.curveDbAt (fir::eq, 20000.0) == Approx (3.0));
}

TEST_CASE ("switching to Freehand starts from the points", "[fir]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    FirProcessor processor;

    processor.setPoints (fir::eq, { { 100.0, 0.0 }, { 1000.0, -12.0 } });
    setParameter (processor, "edit", (float) fir::freehand);

    CHECK (processor.curveDbAt (fir::eq, 1000.0) == Approx (-12.0).margin (0.1));
    CHECK (processor.curveDbAt (fir::eq, 50.0) == Approx (0.0).margin (0.1));
}

TEST_CASE ("fir values can be typed", "[fir]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    FirProcessor processor;

    const auto typed = [&] (const char* id, const char* text)
    {
        const juce::RangedAudioParameter& parameter = *processor.getApvts().getParameter (id);
        return parameter.convertFrom0to1 (parameter.getValueForText (text));
    };

    CHECK (typed ("compRatio", "3:1") == Approx (3.0f).margin (0.01));
    CHECK (typed ("compRatio", "0.5:1") == Approx (0.5f).margin (0.01));
    CHECK (typed ("reduction", "-24") == Approx (-24.0f));
    CHECK (typed ("offset", "+3.5 dB") == Approx (3.5f));
    CHECK (typed ("smoothing", "50 %") == Approx (50.0f));
    CHECK (typed ("subRelease", "120 ms") == Approx (120.0f));
    CHECK (typed ("gateFloor", "-inf") == -90.0f);
    CHECK (typed ("gain", "-6") == Approx (-6.0f));
    CHECK (typed ("learnLength", "3 s") == Approx (3.0f));
}

TEST_CASE ("the fir editor opens", "[fir]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    FirProcessor processor;
    processor.prepareToPlay (rate, 512);

    const std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());

    REQUIRE (editor != nullptr);
    CHECK (editor->getWidth() == 760);

    processor.editorBeingDeleted (editor.get());
}

// Measurements ------------------------------------------------------------------------------

TEST_CASE ("fir CPU measurement", "[.measure]")
{
    constexpr int blockSize = 128, blocks = 4000;

    const auto measure = [] (int mode, int size, bool lowLatency)
    {
        FirDsp dsp;
        dsp.setSynchronousBuilds (true);

        FirDsp::Targets targets;
        targets.frame.mode = mode;
        targets.engine = { size, lowLatency, mode == fir::convolve };

        dsp.prepare (rate, targets);

        float left[blockSize], right[blockSize];
        float* channels[] { left, right };

        std::mt19937 random (12);
        std::uniform_real_distribution<float> noiseSource (-0.25f, 0.25f);

        std::chrono::duration<double> spent {}, worst {};

        for (int block = 0; block < blocks; ++block)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                left[i] = noiseSource (random);
                right[i] = noiseSource (random);
            }

            const auto start = std::chrono::steady_clock::now();

            dsp.setTargets (targets);
            dsp.process (channels, 2, nullptr, 0, blockSize);

            const auto took = std::chrono::steady_clock::now() - start;
            spent += took;
            worst = std::max (worst, std::chrono::duration<double> (took));
        }

        const auto blockSeconds = (double) blockSize / rate;
        std::printf ("fir cpu: mode %d, size %d%s: %.2f %%, worst block %.0f %% of its time\n",
                     mode, size, lowLatency ? " low latency" : "", 100.0 * spent.count() / ((double) blocks * blockSeconds),
                     100.0 * worst.count() / blockSeconds);
    };

    for (const auto mode : { fir::eq, fir::gate, fir::compressor, fir::subtract, fir::convolve })
        measure (mode, 4096, false);

    measure (fir::subtract, 4096, true);
    measure (fir::subtract, 32768, false);
    measure (fir::eq, 128, false);

    SUCCEED();
}
