// Phase 3 Q2.7: Fancy FIR's Resonance mode and CPU stability. See docs/fir.md, "Phase 3 Q2.7",
// and the model the figures come from, docs/research/fir-resonance-model.py.

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <immintrin.h>

#include "FirDsp.h"
#include "FirProcessor.h"
#include "SpectralEngine.h"

#if FANCY_HAS_COMP
 #include "CompProcessor.h"
#endif

#include <fancy/Decibels.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <numbers>
#include <random>
#include <vector>

using Catch::Approx;
using fir::SpectralEngine;

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = std::numbers::pi;
constexpr int signalOrder = 19; // 524288 samples, about 10.9 s
constexpr int signalLength = 1 << signalOrder;
constexpr int settle = 48000;   // the first second is left out while the detector settles

// White or pink noise, band-limited to lowHz to highHz by shaping its spectrum, at 0.1 RMS,
// as the model makes it.
std::vector<float> shapedNoise (bool pink, unsigned seed, double lowHz = 20.0, double highHz = 20000.0)
{
    std::mt19937 random (seed);
    std::normal_distribution<float> gauss (0.0f, 1.0f);

    juce::dsp::FFT fft (signalOrder);
    std::vector<float> data ((size_t) signalLength * 2, 0.0f);

    for (int i = 0; i < signalLength; ++i)
        data[(size_t) i] = gauss (random);

    fft.performRealOnlyForwardTransform (data.data(), true);

    for (int k = 0; k <= signalLength / 2; ++k)
    {
        const auto hz = (double) k * rate / signalLength;
        auto shape = hz >= lowHz && hz <= highHz ? 1.0 : 0.0;

        if (pink)
            shape /= std::sqrt (std::max (hz, 20.0) / 1000.0);

        data[(size_t) k * 2] *= (float) shape;
        data[(size_t) k * 2 + 1] *= (float) shape;
    }

    fft.performRealOnlyInverseTransform (data.data());

    std::vector<float> samples (data.begin(), data.begin() + signalLength);

    auto sum = 0.0;

    for (const auto sample : samples)
        sum += (double) sample * (double) sample;

    const auto scale = 0.1 / std::sqrt (sum / signalLength);

    for (auto& sample : samples)
        sample = (float) (sample * scale);

    return samples;
}

// RBJ peaking EQ: the resonance.
std::vector<float> peaking (const std::vector<float>& input, double hz, double gainDb, double q)
{
    const auto a = std::pow (10.0, gainDb / 40.0);
    const auto w0 = 2.0 * pi * hz / rate;
    const auto alpha = std::sin (w0) / (2.0 * q);
    const auto a0 = 1.0 + alpha / a;

    const double b[] { (1.0 + alpha * a) / a0, -2.0 * std::cos (w0) / a0, (1.0 - alpha * a) / a0 };
    const double d[] { -2.0 * std::cos (w0) / a0, (1.0 - alpha / a) / a0 };

    std::vector<float> output (input.size());
    double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

    for (size_t i = 0; i < input.size(); ++i)
    {
        const auto x = (double) input[i];
        const auto y = b[0] * x + b[1] * x1 + b[2] * x2 - d[0] * y1 - d[1] * y2;

        x2 = x1, x1 = x, y2 = y1, y1 = y;
        output[i] = (float) y;
    }

    return output;
}

struct Run
{
    std::vector<float> left, right;
    int latency = 0;
};

fir::FrameSettings resonanceSettings (double depthDb, double sharpness = 0.5)
{
    fir::FrameSettings settings;
    settings.mode = fir::resonance;
    settings.resDepthDb = depthDb;
    settings.resSharpness = sharpness;
    return settings;
}

Run runEngine (const std::vector<float>& left, const std::vector<float>& right, const fir::FrameSettings& settings,
               int fftSize = 4096, fir::Shared* sharedState = nullptr, std::vector<float>* columnTrack = nullptr)
{
    fir::Shared localShared;
    auto& shared = sharedState != nullptr ? *sharedState : localShared;

    SpectralEngine engine ({ fftSize, false, false }, rate, shared);

    constexpr int blockSize = 512;
    const auto numSamples = (int) left.size();

    Run run { std::vector<float> ((size_t) numSamples), std::vector<float> ((size_t) numSamples), engine.getLatency() };

    const auto column = (int) (std::log (3000.0 / 20.0) / std::log (1000.0) * fir::displayColumns);

    for (int position = 0; position < numSamples; position += blockSize)
    {
        const auto count = std::min (blockSize, numSamples - position);

        engine.process (left.data() + position, right.data() + position, nullptr, nullptr,
                        run.left.data() + position, run.right.data() + position, 2, count, settings, true);

        if (columnTrack != nullptr)
            columnTrack->push_back (shared.reduction[(size_t) column].load());
    }

    return run;
}

// The gain at hz, in dB, from the averaged cross-spectrum of the output against the input,
// with the output's latency taken out.
double transferDb (const std::vector<float>& output, const std::vector<float>& input, int latency, double hz)
{
    constexpr int order = 13, size = 1 << order;

    juce::dsp::FFT fft (order);
    std::vector<float> x ((size_t) size * 2), y ((size_t) size * 2);

    const auto bin = (int) std::lround (hz * size / rate);
    std::complex<double> cross {};
    auto power = 0.0;

    for (int start = settle; start + latency + size <= (int) output.size(); start += size / 2)
    {
        std::fill (x.begin(), x.end(), 0.0f);
        std::fill (y.begin(), y.end(), 0.0f);

        for (int i = 0; i < size; ++i)
        {
            const auto window = (float) (0.5 - 0.5 * std::cos (2.0 * pi * i / size));
            x[(size_t) i] = input[(size_t) (start + i)] * window;
            y[(size_t) i] = output[(size_t) (start + latency + i)] * window;
        }

        fft.performRealOnlyForwardTransform (x.data(), true);
        fft.performRealOnlyForwardTransform (y.data(), true);

        const std::complex<double> xs (x[(size_t) bin * 2], x[(size_t) bin * 2 + 1]), ys (y[(size_t) bin * 2], y[(size_t) bin * 2 + 1]);
        cross += ys * std::conj (xs);
        power += std::norm (xs);
    }

    return 20.0 * std::log10 (std::abs (cross) / power);
}

// The output's energy against the input's in the octave round centreHz, in dB.
double octaveChangeDb (const std::vector<float>& output, const std::vector<float>& input, int latency, double centreHz)
{
    constexpr int order = 18, size = 1 << order;

    juce::dsp::FFT fft (order);

    const auto energy = [&] (const std::vector<float>& samples, int start)
    {
        std::vector<float> data ((size_t) size * 2, 0.0f);

        for (int i = 0; i < size; ++i)
            data[(size_t) i] = samples[(size_t) (start + i)] * (float) (0.5 - 0.5 * std::cos (2.0 * pi * i / size));

        fft.performFrequencyOnlyForwardTransform (data.data());

        auto sum = 0.0;

        for (int k = (int) std::ceil (centreHz / std::sqrt (2.0) * size / rate); k <= (int) (centreHz * std::sqrt (2.0) * size / rate); ++k)
            sum += (double) data[(size_t) k] * (double) data[(size_t) k];

        return sum;
    };

    return 10.0 * std::log10 (energy (output, settle + latency) / energy (input, settle));
}

constexpr double octaves[] { 31.5, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0 };
} // namespace

// 2. The brief's resonance test -----------------------------------------------------------------

TEST_CASE ("Resonance turns a 3 kHz resonance down by Depth and leaves its surroundings", "[fir][q27]")
{
    const auto depth = GENERATE (3.0, 6.0);

    const auto input = peaking (shapedNoise (true, 1), 3000.0, 12.0, 8.0);
    const auto run = runEngine (input, input, resonanceSettings (depth));

    const auto at3k = transferDb (run.left, input, run.latency, 3000.0);
    const auto at1k5 = transferDb (run.left, input, run.latency, 1500.0);

    std::printf ("fir resonance: Depth %.0f dB, 3 kHz %+.2f dB, 1.5 kHz %+.2f dB\n", depth, at3k, at1k5);

    CHECK (at3k == Approx (-depth).margin (1.0));
    CHECK (std::abs (at1k5) < 0.2);
}

// 3. The brief's broadband test -----------------------------------------------------------------

TEST_CASE ("Resonance leaves broadband noise within 0.5 dB in every octave", "[fir][q27]")
{
    const auto pink = GENERATE (false, true);
    const auto sharpness = GENERATE (0.0, 0.5, 1.0);

    const auto input = shapedNoise (pink, 2);
    const auto run = runEngine (input, input, resonanceSettings (24.0, sharpness));

    auto worst = 0.0;

    for (const auto hz : octaves)
    {
        const auto change = octaveChangeDb (run.left, input, run.latency, hz);
        worst = std::max (worst, std::abs (change));

        INFO ((pink ? "pink" : "white") << ", Sharpness " << sharpness << ", octave " << hz << " Hz: " << change << " dB");
        CHECK (std::abs (change) < 0.5);
    }

    std::printf ("fir resonance broadband: %s, Sharpness %.0f %%, worst octave %.3f dB\n", pink ? "pink" : "white", sharpness * 100.0, worst);
}

// 4. Band edges are never peaks -----------------------------------------------------------------

TEST_CASE ("Resonance doesn't take a band edge for a peak", "[fir][q27]")
{
    const auto input = shapedNoise (true, 5, 300.0, 8000.0);
    const auto run = runEngine (input, input, resonanceSettings (24.0));

    for (const auto hz : { 500.0, 1000.0, 2000.0, 4000.0 })
    {
        const auto change = octaveChangeDb (run.left, input, run.latency, hz);

        INFO ("octave " << hz << " Hz: " << change << " dB");
        CHECK (std::abs (change) < 0.5);
    }
}

// 5. Range ------------------------------------------------------------------------------------

TEST_CASE ("Resonance leaves a resonance outside Range alone", "[fir][q27]")
{
    const auto input = peaking (shapedNoise (true, 1), 3000.0, 12.0, 8.0);

    auto settings = resonanceSettings (6.0);
    settings.resHighHz = 2000.0;

    const auto outside = runEngine (input, input, settings);
    CHECK (std::abs (transferDb (outside.left, input, outside.latency, 3000.0)) < 0.5);

    settings.resLowHz = 1000.0;
    settings.resHighHz = 10000.0;

    const auto inside = runEngine (input, input, settings);
    CHECK (transferDb (inside.left, input, inside.latency, 3000.0) < -4.0);
}

// 6. Linked ------------------------------------------------------------------------------------

TEST_CASE ("Resonance gives both channels the same gain", "[fir][q27]")
{
    // Exact checks, since a cross-spectrum estimate weights a gain that moves over time
    // differently on two different signals (run 43741706340: -3.54 and -3.08 dB).
    const auto left = peaking (shapedNoise (true, 1), 3000.0, 12.0, 8.0);
    const auto settings = resonanceSettings (6.0);

    // 1. The right channel is the left at half level: if both get the same gain, the right
    //    output is exactly half the left.
    std::vector<float> half (left.size());

    for (size_t i = 0; i < left.size(); ++i)
        half[i] = 0.5f * left[i];

    const auto scaled = runEngine (left, half, settings);

    auto worst = 0.0, peak = 0.0;

    for (size_t i = 0; i < left.size(); ++i)
    {
        worst = std::max (worst, std::abs ((double) scaled.right[i] - 0.5 * (double) scaled.left[i]));
        peak = std::max (peak, std::abs ((double) scaled.left[i]));
    }

    CHECK (worst < 1.0e-5 * peak);
    CHECK (transferDb (scaled.left, left, scaled.latency, 3000.0) < -2.0);

    // 2. A resonance in one channel only is still cut, and swapping the channels swaps the
    //    outputs exactly.
    const auto right = shapedNoise (true, 7);
    const auto run = runEngine (left, right, settings);
    const auto swapped = runEngine (right, left, settings);

    const auto leftGain = transferDb (run.left, left, run.latency, 3000.0);
    std::printf ("fir resonance linked: resonance in the left only, left %+.2f dB at 3 kHz; scaled copy within %.2g of half\n",
                 leftGain, worst / peak);

    CHECK (leftGain < -2.0);
    CHECK (swapped.left == run.right);
    CHECK (swapped.right == run.left);
}

// 7. Ballistics --------------------------------------------------------------------------------

TEST_CASE ("Resonance's Attack and Release are times in ms", "[fir][q27]")
{
    // A 3 kHz tone over pink noise from 2 s to 6 s; the reduction in its display column is read
    // every block. Release is timed from the first block that falls to 63 % of the way down.
    // Attack is timed from 10 % to 63 % of the way up, so a stray bin of the noise can't start
    // the clock: for an exponential that's ln (0.9 / 0.37) = 0.889 of the time constant.
    const auto size = GENERATE (1024, 8192);

    constexpr double attackMs = 1000.0, releaseMs = 2000.0;
    constexpr int blockSize = 512;

    auto input = shapedNoise (true, 3);

    for (int i = 2 * 48000; i < 6 * 48000; ++i)
        input[(size_t) i] += (float) (0.3 * std::sin (2.0 * pi * 3000.0 * i / rate));

    auto settings = resonanceSettings (12.0);
    settings.resAttackMs = attackMs;
    settings.resReleaseMs = releaseMs;

    fir::Shared shared;
    std::vector<float> track;
    runEngine (input, input, settings, size, &shared, &track);

    const auto blockMs = 1000.0 * blockSize / rate;
    const auto steady = (double) track[(size_t) (5.5 * 48000 / blockSize)];

    REQUIRE (steady > 8.0);

    const auto timeTo = [&] (int from, bool rising)
    {
        auto start = -1;

        for (int i = from; i < (int) track.size(); ++i)
        {
            const auto value = (double) track[(size_t) i];

            if (start < 0 && (rising ? value > steady * 0.1 : value < steady * 0.98))
                start = i;

            if (start >= 0 && (rising ? value >= steady * (1.0 - std::exp (-1.0)) : value <= steady * std::exp (-1.0)))
                return (i - start) * blockMs;
        }

        return -1.0;
    };

    const auto attack = timeTo (2 * 48000 / blockSize - 2, true);
    const auto release = timeTo (6 * 48000 / blockSize - 2, false);

    std::printf ("fir resonance ballistics: size %d, attack 63 %% after %.0f ms (set %.0f), release 63 %% after %.0f ms (set %.0f)\n",
                 size, attack, attackMs, release, releaseMs);

    CHECK (release == Approx (releaseMs).epsilon (0.1));

    // At 8192 points a frame is 170 ms long, so the target itself ramps in over a frame and the
    // attack reads short; the ballistics are checked at 1024, where the model gives 848 ms.
    if (size == 1024)
        CHECK (attack == Approx (attackMs * std::log (0.9 / std::exp (-1.0))).epsilon (0.1));
}

// 8. Listen ------------------------------------------------------------------------------------

TEST_CASE ("Resonance's Listen is exactly what's taken out", "[fir][q27]")
{
    const auto input = peaking (shapedNoise (true, 1), 3000.0, 12.0, 8.0);

    auto settings = resonanceSettings (8.0);
    const auto heard = runEngine (input, input, settings);

    settings.listen = true;
    const auto removed = runEngine (input, input, settings);

    auto residual = 0.0, reference = 0.0;

    for (int i = settle; i + heard.latency < (int) input.size(); ++i)
    {
        const auto sum = (double) heard.left[(size_t) (i + heard.latency)] + (double) removed.left[(size_t) (i + heard.latency)];
        const auto difference = sum - (double) input[(size_t) i];

        residual += difference * difference;
        reference += (double) input[(size_t) i] * (double) input[(size_t) i];
    }

    const auto residualDb = 10.0 * std::log10 (residual / reference);
    std::printf ("fir resonance listen: output + removed against the input, residual %.1f dB\n", residualDb);

    // Within 0.01 dB of the input is a residual below about -60 dB.
    CHECK (residualDb < -60.0);

    // And something was removed.
    CHECK (transferDb (heard.left, input, heard.latency, 3000.0) < -4.0);
}

// 1. Compatibility -------------------------------------------------------------------------------

namespace
{
std::vector<float> renderNoise (FirProcessor& processor, int numSamples)
{
    juce::AudioBuffer<float> buffer (4, numSamples);
    juce::MidiBuffer midi;

    buffer.clear();

    std::mt19937 random (21);
    std::normal_distribution<float> gauss (0.0f, 0.1f);

    for (int i = 0; i < numSamples; ++i)
    {
        buffer.setSample (0, i, gauss (random));
        buffer.setSample (1, i, gauss (random));
    }

    processor.processBlock (buffer, midi);

    std::vector<float> samples (buffer.getReadPointer (0), buffer.getReadPointer (0) + numSamples);
    samples.insert (samples.end(), buffer.getReadPointer (1), buffer.getReadPointer (1) + numSamples);
    return samples;
}

void setValue (FirProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getApvts().getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

const char* const resonanceIds[] { "resDepth", "resSharpness", "resAttack", "resRelease", "resLow", "resHigh" };
} // namespace

TEST_CASE ("a v1.4.0 state keeps its mode and renders as before", "[fir][q27]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto mode = GENERATE (fir::eq, fir::gate, fir::compressor, fir::subtract, fir::convolve);

    // A v1.4.0 state: today's, without the Q2.7 parameters.
    FirProcessor saved;
    setValue (saved, "mode", (float) mode);
    setValue (saved, "compRatio", 3.0f);

    juce::MemoryBlock block;
    saved.getStateInformation (block);

    auto xml = juce::AudioProcessor::getXmlFromBinary (block.getData(), (int) block.getSize());
    REQUIRE (xml != nullptr);

    auto removed = 0;

    for (const auto* id : resonanceIds)
        for (auto* child = xml->getFirstChildElement(); child != nullptr; child = child->getNextElement())
            if (child->getStringAttribute ("id") == id)
            {
                xml->removeChildElement (child, true);
                ++removed;
                break;
            }

    juce::MemoryBlock oldBlock;
    juce::AudioProcessor::copyXmlToBinary (*xml, oldBlock);

    FirProcessor loaded;
    loaded.setStateInformation (oldBlock.getData(), (int) oldBlock.getSize());

    INFO ("mode " << mode << ", removed " << removed << " parameters from the state");
    CHECK (removed == 6);
    CHECK (loaded.getMode() == mode);

    for (const auto* id : resonanceIds)
    {
        auto* parameter = loaded.getApvts().getParameter (id);
        CHECK (parameter->getValue() == Approx (parameter->getDefaultValue()));
    }

    // The new parameters do nothing outside Resonance: an old mode renders the same whatever
    // they're set to.
    FirProcessor changed;
    changed.setStateInformation (oldBlock.getData(), (int) oldBlock.getSize());
    setValue (changed, "resDepth", 20.0f);
    setValue (changed, "resSharpness", 90.0f);
    setValue (changed, "resLow", 500.0f);
    setValue (changed, "resHigh", 4000.0f);

    for (auto* processor : { &loaded, &changed })
    {
        processor->setNonRealtime (true);
        processor->prepareToPlay (rate, 48000);
    }

    REQUIRE (renderNoise (loaded, 48000) == renderNoise (changed, 48000));
}

TEST_CASE ("Resonance's latency is the measured impulse delay", "[fir][q27]")
{
    const auto size = GENERATE (512, 4096, 16384);
    const auto lowLatency = GENERATE (false, true);

    fir::Shared shared;
    SpectralEngine engine ({ size, lowLatency, false }, rate, shared);

    auto settings = resonanceSettings (12.0);
    settings.reduceArtifacts = false;

    const auto latency = engine.getLatency();
    const auto numSamples = latency + size * 3;
    const auto impulseAt = size + 37;

    std::vector<float> input ((size_t) numSamples, 0.0f), left ((size_t) numSamples), right ((size_t) numSamples);
    input[(size_t) impulseAt] = 0.5f;

    for (int position = 0; position < numSamples; position += 64)
    {
        const auto count = std::min (64, numSamples - position);
        engine.process (input.data() + position, input.data() + position, nullptr, nullptr,
                        left.data() + position, right.data() + position, 2, count, settings, true);
    }

    const auto peak = std::max_element (left.begin(), left.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); });

    INFO ("size " << size << ", low latency " << lowLatency);
    CHECK (peak - left.begin() == impulseAt + latency);
}

// CPU (section 5) --------------------------------------------------------------------------------

namespace
{
// Thread cycles per second, measured by spinning: used to turn a block's cycles into time.
double cyclesPerSecond()
{
    ULONG64 startCycles = 0, endCycles = 0;
    const auto start = std::chrono::steady_clock::now();
    QueryThreadCycleTime (GetCurrentThread(), &startCycles);

    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds (200))
        _mm_pause();

    QueryThreadCycleTime (GetCurrentThread(), &endCycles);
    const std::chrono::duration<double> wall = std::chrono::steady_clock::now() - start;

    return (double) (endCycles - startCycles) / wall.count();
}

struct Timing
{
    double meanPercent = 0.0, cyclesMeanPercent = 0.0, p99Percent = 0.0, worstPercent = 0.0;
    int preempted = 0;
};

// One run: paced at real time, at time-critical priority, as the Verb's benchmark is. Each
// block is timed by the clock and by the thread's own cycles; a block whose clock time is more
// than 200 us over its cycle time was preempted.
template <typename ProcessBlock>
Timing timeRun (ProcessBlock&& processBlock, int numBlocks, int blockSize, double cycleRate)
{
    using Clock = std::chrono::steady_clock;

    const auto blockSeconds = (double) blockSize / rate;
    const auto blockTime = std::chrono::duration_cast<Clock::duration> (std::chrono::duration<double> (blockSeconds));

    std::vector<double> wall;
    wall.reserve ((size_t) numBlocks);

    auto cycleSeconds = 0.0;
    Timing timing;

    SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    auto deadline = Clock::now();

    for (int block = 0; block < numBlocks; ++block)
    {
        ULONG64 cyclesBefore = 0, cyclesAfter = 0;
        QueryThreadCycleTime (GetCurrentThread(), &cyclesBefore);
        const auto start = Clock::now();

        processBlock (block);

        const std::chrono::duration<double> used = Clock::now() - start;
        QueryThreadCycleTime (GetCurrentThread(), &cyclesAfter);

        const auto cycles = (double) (cyclesAfter - cyclesBefore) / cycleRate;

        wall.push_back (used.count());
        cycleSeconds += cycles;

        if (used.count() - cycles > 200.0e-6)
            ++timing.preempted;

        deadline += blockTime;

        while (Clock::now() < deadline)
            _mm_pause();
    }

    SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_NORMAL);

    auto total = 0.0;

    for (const auto value : wall)
        total += value;

    auto sorted = wall;
    std::sort (sorted.begin(), sorted.end());

    timing.meanPercent = 100.0 * total / ((double) numBlocks * blockSeconds);
    timing.cyclesMeanPercent = 100.0 * cycleSeconds / ((double) numBlocks * blockSeconds);
    timing.p99Percent = 100.0 * sorted[(size_t) (0.99 * (double) (numBlocks - 1))] / blockSeconds;
    timing.worstPercent = 100.0 * sorted.back() / blockSeconds;
    return timing;
}

// The median of each figure over the runs.
Timing median (std::vector<Timing> runs)
{
    const auto pick = [&] (auto member)
    {
        std::vector<double> values;

        for (const auto& run : runs)
            values.push_back ((double) (run.*member));

        std::sort (values.begin(), values.end());
        return values[values.size() / 2];
    };

    Timing result;
    result.meanPercent = pick (&Timing::meanPercent);
    result.cyclesMeanPercent = pick (&Timing::cyclesMeanPercent);
    result.p99Percent = pick (&Timing::p99Percent);
    result.worstPercent = pick (&Timing::worstPercent);
    result.preempted = (int) pick (&Timing::preempted);
    return result;
}
} // namespace

TEST_CASE ("fir Q2.7 CPU measurement", "[.measure]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    constexpr int blockSize = 128, numBlocks = 2000, runs = 5; // about 5.3 s a run

    const auto cycleRate = cyclesPerSecond();

    std::vector<float> noiseLeft ((size_t) blockSize * 64), noiseRight ((size_t) blockSize * 64);
    std::mt19937 random (12);
    std::uniform_real_distribution<float> noiseSource (-0.25f, 0.25f);

    for (size_t i = 0; i < noiseLeft.size(); ++i)
        noiseLeft[i] = noiseSource (random), noiseRight[i] = noiseSource (random);

    const auto timeMode = [&] (int mode)
    {
        std::vector<Timing> results;

        for (int run = 0; run < runs; ++run)
        {
            FirDsp dsp;
            dsp.setSynchronousBuilds (true);

            FirDsp::Targets targets;
            targets.frame.mode = mode;
            targets.engine = { 4096, false, mode == fir::convolve };
            dsp.prepare (rate, targets);

            float left[blockSize], right[blockSize];
            float* channels[] { left, right };

            results.push_back (timeRun ([&] (int block)
                                        {
                                            const auto offset = (size_t) ((block % 64) * blockSize);
                                            std::copy_n (noiseLeft.begin() + (std::ptrdiff_t) offset, blockSize, left);
                                            std::copy_n (noiseRight.begin() + (std::ptrdiff_t) offset, blockSize, right);
                                            dsp.setTargets (targets);
                                            dsp.process (channels, 2, nullptr, 0, blockSize);
                                        },
                                        numBlocks, blockSize, cycleRate));
        }

        return median (results);
    };

    const auto print = [] (const char* name, const Timing& timing, double reference)
    {
        std::printf ("fir q27 cpu, %s: mean %.3f %% (by thread cycles %.3f %%), p99 block %.1f %%, worst block %.1f %% of its time, "
                     "%d preempted blocks, %.2f x Fancy Comp\n",
                     name, timing.meanPercent, timing.cyclesMeanPercent, timing.p99Percent, timing.worstPercent, timing.preempted,
                     reference > 0.0 ? timing.meanPercent / reference : 0.0);
    };

    auto compPercent = 0.0;

   #if FANCY_HAS_COMP
    {
        std::vector<Timing> results;

        for (int run = 0; run < runs; ++run)
        {
            CompProcessor comp;
            comp.prepareToPlay (rate, blockSize);

            juce::AudioBuffer<float> buffer (juce::jmax (comp.getTotalNumInputChannels(), comp.getTotalNumOutputChannels(), 2), blockSize);
            juce::MidiBuffer midi;

            results.push_back (timeRun ([&] (int block)
                                        {
                                            const auto offset = (size_t) ((block % 64) * blockSize);
                                            buffer.clear();
                                            buffer.copyFrom (0, 0, noiseLeft.data() + offset, blockSize);
                                            buffer.copyFrom (1, 0, noiseRight.data() + offset, blockSize);
                                            comp.processBlock (buffer, midi);
                                        },
                                        numBlocks, blockSize, cycleRate));
        }

        const auto timing = median (results);
        compPercent = timing.meanPercent;
        print ("Fancy Comp at its defaults (reference)", timing, compPercent);
    }
   #endif

    const char* const names[] { "EQ", "Gate", "Compressor", "Subtract", "Convolve L/R", "Resonance" };
    double compressorMode = 0.0, resonanceMode = 0.0;

    for (int mode = 0; mode < fir::numModes; ++mode)
    {
        const auto timing = timeMode (mode);
        print (names[mode], timing, compPercent);

        if (mode == fir::compressor)
            compressorMode = timing.meanPercent;

        if (mode == fir::resonance)
            resonanceMode = timing.meanPercent;
    }

    std::printf ("fir q27 cpu: 48 kHz, 128 samples, FFT 4096, median of %d paced runs at time-critical priority; Resonance %.3f %% "
                 "(target < 3 %%), %.2f x Compressor mode (target < 1.3 x)\n",
                 runs, resonanceMode, compressorMode > 0.0 ? resonanceMode / compressorMode : 0.0);

    SUCCEED();
}
