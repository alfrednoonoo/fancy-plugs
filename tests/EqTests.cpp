#include <fancy/dsp/Analyzer.h>
#include "EqDsp.h"
#include "EqFilters.h"
#include "EqGraph.h"
#include "EqProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <fancy/Decibels.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <numbers>
#include <random>
#include <vector>

namespace
{
constexpr double pi = std::numbers::pi;
constexpr double rate = 48000.0;

using eq::BandSpec;
using eq::BandType;

BandSpec makeBand (BandType type, double hz, double gainDb = 0.0, double q = 1.0, int order = 2)
{
    BandSpec spec;
    spec.type = type;
    spec.frequencyHz = hz;
    spec.gainDb = gainDb;
    spec.q = q;
    spec.order = order;
    spec.on = true;
    return spec;
}

double responseDb (const BandSpec& spec, double sampleRate, double hz)
{
    const auto filter = eq::design (spec, sampleRate);
    return fancy::gainToDb (std::abs (filter.responseAt (2.0 * pi * hz / sampleRate)), -300.0);
}

double analogBellDb (double f0, double gainDb, double q, double hz)
{
    const auto a = std::pow (10.0, gainDb / 40.0);
    const std::complex<double> s (0.0, hz / f0);
    const auto h = (s * s + s * a / q + 1.0) / (s * s + s / (a * q) + 1.0);

    return fancy::gainToDb (std::abs (h), -300.0);
}

struct Audio
{
    std::vector<float> left, right;

    explicit Audio (int numSamples, float value = 0.0f)
        : left ((size_t) numSamples, value), right ((size_t) numSamples, value) {}

    int size() const { return (int) left.size(); }

    void process (EqDsp& dsp, int blockSize = 256)
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

double largestStep (const std::vector<float>& samples)
{
    double step = 0.0;

    for (size_t i = 1; i < samples.size(); ++i)
        step = std::max (step, (double) std::abs (samples[i] - samples[i - 1]));

    return step;
}

EqDsp::Targets flatTargets()
{
    return {};
}

void prepare (EqDsp& dsp, const EqDsp::Targets& targets, double sampleRate = rate)
{
    dsp.prepare (sampleRate);
    dsp.setSynchronousDesign (true);
    dsp.snapToTargets (targets);
}

// Runs until a latency change has been fully applied.
void settle (EqDsp& dsp, double sampleRate = rate)
{
    Audio silence ((int) (sampleRate * 0.6));
    silence.process (dsp);
}

void setParameter (EqProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getApvts().getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

std::vector<float> render (EqProcessor& processor, int numSamples)
{
    juce::AudioBuffer<float> buffer (2, numSamples);

    for (int i = 0; i < numSamples; ++i)
        for (int ch = 0; ch < 2; ++ch)
            buffer.setSample (ch, i, 0.4f * (float) std::sin (2.0 * pi * 997.0 * i / rate));

    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);

    return { buffer.getReadPointer (0), buffer.getReadPointer (0) + numSamples };
}

double worstError (const BandSpec& spec, double sampleRate, double hzLimit)
{
    double worst = 0.0;

    const auto reference = spec;

    for (int i = 0; i < 300; ++i)
    {
        const auto hz = 20.0 * std::pow (hzLimit / 20.0, (double) i / 299.0);
        const auto expected = responseDb (reference, 768000.0, hz);

        // Far down a slope the level hardly matters.
        if (expected > -60.0)
            worst = std::max (worst, std::abs (responseDb (spec, sampleRate, hz) - expected));
    }

    return worst;
}
} // namespace

// Filter design -----------------------------------------------------------------------------

TEST_CASE ("18 kHz bell matches the analog prototype", "[eq]")
{
    const auto q = GENERATE (0.7, 1.0, 2.0, 4.0);

    const auto spec = makeBand (BandType::band, 18000.0, 6.0, q);

    for (int i = 0; i < 400; ++i)
    {
        const auto hz = 20.0 * std::pow (1000.0, (double) i / 399.0);

        INFO ("Q " << q << " at " << hz << " Hz");
        REQUIRE (std::abs (responseDb (spec, 44100.0, hz) - analogBellDb (18000.0, 6.0, q, hz)) < 0.5);
    }
}

TEST_CASE ("bell reaches its gain at Freq", "[eq]")
{
    const auto gainDb = GENERATE (-24.0, -6.0, 3.0, 24.0);
    const auto hz = GENERATE (50.0, 1000.0, 10000.0);

    REQUIRE (responseDb (makeBand (BandType::band, hz, gainDb, 2.0), rate, hz) == Catch::Approx (gainDb).margin (0.01));
}

TEST_CASE ("boost and cut mirror exactly", "[eq]")
{
    for (const auto hz : { 100.0, 1000.0, 5000.0, 15000.0 })
    {
        const auto boost = responseDb (makeBand (BandType::band, 1000.0, 9.0, 2.0), rate, hz);
        const auto cut = responseDb (makeBand (BandType::band, 1000.0, -9.0, 2.0), rate, hz);

        REQUIRE (boost == Catch::Approx (-cut).margin (1.0e-9));
    }
}

TEST_CASE ("responses agree across sample rates", "[eq]")
{
    const auto sampleRate = GENERATE (44100.0, 48000.0, 96000.0, 192000.0);

    const BandSpec specs[] {
        makeBand (BandType::band, 1000.0, 12.0, 1.0),
        makeBand (BandType::band, 8000.0, -12.0, 2.0),
        makeBand (BandType::band, 15000.0, 6.0, 1.0),
        makeBand (BandType::lowPass, 5000.0, 0.0, 0.707, 2),
        makeBand (BandType::lowPass, 12000.0, 0.0, 0.707, 4),
        makeBand (BandType::lowPass, 16000.0, 0.0, 2.0, 2),
        makeBand (BandType::highPass, 100.0, 0.0, 0.707, 4),
        makeBand (BandType::highPass, 10000.0, 0.0, 0.707, 2),
        makeBand (BandType::lowShelf, 200.0, 12.0, 0.707, 2),
        makeBand (BandType::highShelf, 8000.0, 12.0, 0.707, 2),
        makeBand (BandType::highShelf, 8000.0, -12.0, 0.707, 2),
    };

    for (const auto& spec : specs)
    {
        INFO ("type " << (int) spec.type << ", " << spec.frequencyHz << " Hz at " << sampleRate);
        REQUIRE (worstError (spec, sampleRate, 20000.0) < 0.5);
    }
}

TEST_CASE ("slopes are as labelled", "[eq]")
{
    const auto order = GENERATE (1, 2, 3, 4, 6, 8);

    const auto high = makeBand (BandType::highPass, 8000.0, 0.0, 0.707, order);
    const auto low = makeBand (BandType::lowPass, 500.0, 0.0, 0.707, order);

    constexpr double sampleRate = 384000.0;

    const auto highSlope = responseDb (high, sampleRate, 1000.0) - responseDb (high, sampleRate, 500.0);
    const auto lowSlope = responseDb (low, sampleRate, 4000.0) - responseDb (low, sampleRate, 8000.0);

    INFO ("order " << order);
    CHECK (highSlope == Catch::Approx (6.02 * order).margin (0.5));
    CHECK (lowSlope == Catch::Approx (6.02 * order).margin (0.5));

    // Butterworth at any order is 3 dB down at Freq.
    CHECK (responseDb (high, sampleRate, 8000.0) == Catch::Approx (-3.01).margin (0.1));
    CHECK (responseDb (low, sampleRate, 500.0) == Catch::Approx (-3.01).margin (0.1));
}

TEST_CASE ("Q adds resonance without changing the slope", "[eq]")
{
    const auto order = GENERATE (2, 4, 8);

    const auto plain = makeBand (BandType::highPass, 2000.0, 0.0, 0.707, order);
    auto resonant = plain;
    resonant.q = 4.0;

    constexpr double sampleRate = 192000.0;

    CHECK (responseDb (resonant, sampleRate, 2000.0) > responseDb (plain, sampleRate, 2000.0) + 10.0);

    const auto plainSlope = responseDb (plain, sampleRate, 100.0) - responseDb (plain, sampleRate, 50.0);
    const auto resonantSlope = responseDb (resonant, sampleRate, 100.0) - responseDb (resonant, sampleRate, 50.0);

    CHECK (resonantSlope == Catch::Approx (plainSlope).margin (0.1));
}

TEST_CASE ("shelves reach their gain and sit halfway at Freq", "[eq]")
{
    const auto order = GENERATE (1, 2, 3, 4, 6, 8);
    const auto gainDb = GENERATE (-12.0, 6.0, 18.0);

    constexpr double sampleRate = 192000.0;

    const auto low = makeBand (BandType::lowShelf, 500.0, gainDb, 0.707, order);
    const auto high = makeBand (BandType::highShelf, 500.0, gainDb, 0.707, order);

    INFO ("order " << order << ", " << gainDb << " dB");

    CHECK (responseDb (low, sampleRate, 1.0) == Catch::Approx (gainDb).margin (0.02));
    CHECK (responseDb (low, sampleRate, 90000.0) == Catch::Approx (0.0).margin (0.05));
    CHECK (responseDb (low, sampleRate, 500.0) == Catch::Approx (gainDb * 0.5).margin (0.1));

    CHECK (responseDb (high, sampleRate, 1.0) == Catch::Approx (0.0).margin (0.02));
    CHECK (responseDb (high, sampleRate, 90000.0) == Catch::Approx (gainDb).margin (0.05));
    CHECK (responseDb (high, sampleRate, 500.0) == Catch::Approx (gainDb * 0.5).margin (0.1));
}

TEST_CASE ("tilt pivots at Freq", "[eq]")
{
    const auto tilt = makeBand (BandType::tilt, 1000.0, 12.0, 0.707, 2);

    CHECK (responseDb (tilt, rate, 5.0) == Catch::Approx (-6.0).margin (0.05));
    CHECK (responseDb (tilt, rate, 1000.0) == Catch::Approx (0.0).margin (0.1));
    CHECK (responseDb (tilt, rate, 20000.0) == Catch::Approx (6.0).margin (0.1));
}

TEST_CASE ("a deep bell stays a bell", "[eq]")
{
    const auto bell = makeBand (BandType::band, 1000.0, -24.0, 2.0);
    const auto notch = makeBand (BandType::notch, 1000.0, 0.0, 2.0);

    // It reaches its gain and goes no lower, unlike a notch.
    CHECK (responseDb (bell, rate, 1000.0) == Catch::Approx (-24.0).margin (0.01));
    CHECK (responseDb (notch, rate, 1000.0) < -120.0);

    CHECK (responseDb (bell, rate, 20.0) > -0.5);
    CHECK (responseDb (notch, rate, 20.0) > -0.5);
}

TEST_CASE ("band pass peaks at unity and all pass stays flat", "[eq]")
{
    const auto bandPass = makeBand (BandType::bandPass, 2000.0, 0.0, 3.0);
    const auto allPass = eq::design (makeBand (BandType::allPass, 2000.0, 0.0, 1.0), rate);

    CHECK (responseDb (bandPass, rate, 2000.0) == Catch::Approx (0.0).margin (0.05));
    CHECK (responseDb (bandPass, rate, 100.0) < -20.0);

    for (const auto hz : { 20.0, 1000.0, 2000.0, 15000.0, 23000.0 })
        CHECK (std::abs (allPass.responseAt (2.0 * pi * hz / rate)) == Catch::Approx (1.0).margin (1.0e-9));
}

TEST_CASE ("every setting gives a stable filter", "[eq]")
{
    const auto type = (BandType) GENERATE (0, 1, 2, 3, 4, 5, 6, 7, 8);
    const auto sampleRate = GENERATE (44100.0, 96000.0, 384000.0);

    for (const auto hz : { 20.0, 60.0, 1000.0, 15000.0, 20000.0, 24000.0 })
    {
        for (const auto q : { 0.1, 0.707, 5.0, 30.0 })
        {
            for (const auto gainDb : { -24.0, -0.1, 0.0, 0.1, 24.0 })
            {
                for (const auto order : eq::slopeOrders)
                {
                    const auto filter = eq::design (makeBand (type, hz, gainDb, q, order), sampleRate);

                    REQUIRE (filter.numSections > 0);

                    for (int s = 0; s < filter.numSections; ++s)
                    {
                        const auto& section = filter.sections[(size_t) s];

                        REQUIRE (std::isfinite (section.b0 + section.b1 + section.b2 + section.a1 + section.a2));
                        REQUIRE (std::abs (section.a2) < 1.0);
                        REQUIRE (std::abs (section.a1) < 1.0 + section.a2);
                    }

                    REQUIRE (std::isfinite (std::abs (filter.responseAt (0.3))));
                }
            }
        }
    }
}

TEST_CASE ("powerAt agrees with the full response", "[eq]")
{
    const auto filter = eq::design (makeBand (BandType::highShelf, 3000.0, 9.0, 1.0, 6), rate);

    for (const auto w : { 0.01, 0.3, 1.5, 3.0 })
    {
        const auto s = std::sin (w * 0.5);
        const auto magnitude = std::abs (filter.responseAt (w));

        CHECK (filter.powerAt (s * s) == Catch::Approx (magnitude * magnitude).epsilon (1.0e-6));
    }
}

// The audio path ----------------------------------------------------------------------------

TEST_CASE ("a flat EQ is transparent", "[eq]")
{
    auto targets = flatTargets();

    for (int b = 0; b < 4; ++b)
        targets.bands[(size_t) b] = makeBand (BandType::band, 1000.0 * (b + 1));

    EqDsp dsp;
    prepare (dsp, targets);

    const auto original = sine (997.0, 0.5, rate, 9600);
    auto signal = original;

    signal.process (dsp);

    double worst = 0.0;

    for (size_t i = 0; i < signal.left.size(); ++i)
        worst = std::max (worst, (double) std::abs (signal.left[i] - original.left[i]));

    REQUIRE (worst < 1.0e-6);
}

TEST_CASE ("audio follows the designed response", "[eq]")
{
    auto targets = flatTargets();
    targets.bands[0] = makeBand (BandType::band, 1000.0, 6.0, 2.0);
    targets.bands[1] = makeBand (BandType::highPass, 100.0, 0.0, 0.707, 4);

    const auto oversampling = GENERATE (0, 1, 2);
    targets.oversampling = oversampling;

    EqDsp dsp;
    prepare (dsp, targets);

    auto signal = sine (1000.0, 0.1, rate, 24000);
    signal.process (dsp);

    INFO ("oversampling " << oversampling);
    REQUIRE (levelAt (signal.left, 12000, 1000.0, rate) == Catch::Approx (fancy::gainToDb (0.1) + 6.0).margin (0.05));
}

TEST_CASE ("gain and output scale the signal", "[eq]")
{
    auto targets = flatTargets();
    targets.gain = 0.5;
    targets.outputGain = 0.5;

    EqDsp dsp;
    prepare (dsp, targets);

    Audio signal (4096, 1.0f);
    signal.process (dsp);

    REQUIRE (signal.left.back() == Catch::Approx (0.25f).margin (1.0e-6));
}

TEST_CASE ("block size doesn't change the eq output", "[eq]")
{
    const auto linearPhase = GENERATE (false, true);

    auto targets = flatTargets();
    targets.bands[0] = makeBand (BandType::band, 1000.0, 6.0, 2.0);
    targets.bands[1] = makeBand (BandType::highShelf, 5000.0, -4.0, 0.707, 4);
    targets.linearPhase = linearPhase;
    targets.firLength = 0;

    const auto run = [&] (int blockSize)
    {
        EqDsp dsp;
        prepare (dsp, targets);

        auto signal = sine (440.0, 0.5, rate, 24000);
        signal.process (dsp, blockSize);
        return signal.left;
    };

    const auto reference = run (64);

    for (const auto blockSize : { 1, 7, 128, 1000, 8192 })
    {
        INFO ("blocks of " << blockSize);
        REQUIRE (run (blockSize) == reference);
    }
}

TEST_CASE ("eq works at every supported sample rate and in mono", "[eq]")
{
    const auto sampleRate = GENERATE (44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0, 384000.0);
    const auto linearPhase = GENERATE (false, true);

    auto targets = flatTargets();
    targets.bands[0] = makeBand (BandType::band, 1000.0, 6.0, 2.0);
    targets.linearPhase = linearPhase;
    targets.firLength = 1;

    EqDsp dsp;
    prepare (dsp, targets, sampleRate);

    auto signal = sine (1000.0, 0.1, sampleRate, (int) sampleRate);

    float* channels[] { signal.left.data() };

    for (int start = 0; start < signal.size(); start += 4096)
        dsp.process (channels, 1, std::min (4096, signal.size() - start)), channels[0] += 4096;

    INFO (sampleRate << " Hz, linear " << linearPhase);
    REQUIRE (levelAt (signal.left, signal.size() / 2, 1000.0, sampleRate) == Catch::Approx (fancy::gainToDb (0.1) + 6.0).margin (0.1));
}

TEST_CASE ("bad input never poisons the eq", "[eq]")
{
    const auto linearPhase = GENERATE (false, true);

    auto targets = flatTargets();
    targets.bands[0] = makeBand (BandType::band, 1000.0, 6.0, 2.0);
    targets.linearPhase = linearPhase;
    targets.firLength = 0;

    EqDsp dsp;
    prepare (dsp, targets);

    Audio bad (2048);
    bad.left[100] = std::numeric_limits<float>::quiet_NaN();
    bad.right[200] = std::numeric_limits<float>::infinity();
    bad.process (dsp);

    auto signal = sine (1000.0, 0.1, rate, 24000);
    signal.process (dsp);

    for (const auto sample : signal.left)
        REQUIRE (std::isfinite (sample));

    REQUIRE (levelAt (signal.left, 12000, 1000.0, rate) > -20.0);
}

TEST_CASE ("automation sweeps are click-free", "[eq][sweep]")
{
    const auto linearPhase = GENERATE (false, true);

    auto targets = flatTargets();
    targets.bands[0] = makeBand (BandType::band, 200.0, 12.0, 2.0);
    targets.linearPhase = linearPhase;
    targets.firLength = 0;

    EqDsp dsp;
    prepare (dsp, targets);

    auto signal = sine (1000.0, 0.1, rate, 48000);

    constexpr int blockSize = 128;

    for (int start = 0; start < signal.size(); start += blockSize)
    {
        const auto position = std::clamp ((double) (start - 4800) / 9600.0, 0.0, 1.0);

        targets.bands[0].frequencyHz = 200.0 * std::pow (40.0, position);
        targets.bands[0].gainDb = 12.0 - 24.0 * position;
        dsp.setTargets (targets);

        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, std::min (blockSize, signal.size() - start));
    }

    for (const auto sample : signal.left)
        REQUIRE (std::isfinite (sample));

    // At 4 times the level, a 1 kHz wave moves about 0.052 per sample. A click would stand
    // well clear of this limit.
    INFO ("linear " << linearPhase);
    REQUIRE (largestStep (signal.left) < 0.09);
}

TEST_CASE ("changing band type, slope or power doesn't click", "[eq][sweep]")
{
    auto targets = flatTargets();
    targets.bands[0] = makeBand (BandType::band, 1000.0, 12.0, 1.0);

    EqDsp dsp;
    prepare (dsp, targets);

    auto signal = sine (1000.0, 0.1, rate, 48000);

    constexpr int blockSize = 128;

    for (int start = 0, block = 0; start < signal.size(); start += blockSize, ++block)
    {
        if (block == 40)
            targets.bands[0].type = BandType::lowPass;
        else if (block == 80)
            targets.bands[0].order = 8;
        else if (block == 120)
            targets.bands[0].on = false;
        else if (block == 160)
            targets.bands[0].on = true;
        else if (block == 200)
            targets.bands[0].type = BandType::notch;

        dsp.setTargets (targets);

        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, std::min (blockSize, signal.size() - start));
    }

    REQUIRE (largestStep (signal.left) < 0.09);
}

TEST_CASE ("random parameter changes never produce bad output", "[eq]")
{
    const auto seed = GENERATE (1u, 2u, 3u, 4u, 5u);
    const auto linearPhase = GENERATE (false, true);

    std::mt19937 random (seed);

    const auto uniform = [&random] (double low, double high)
    {
        return std::uniform_real_distribution<double> (low, high) (random);
    };

    const auto randomBand = [&]
    {
        auto band = makeBand ((BandType) (random() % 9),
                              20.0 * std::pow (1200.0, uniform (0.0, 1.0)),
                              uniform (-24.0, 24.0),
                              0.1 * std::pow (300.0, uniform (0.0, 1.0)),
                              eq::slopeOrders[random() % 6]);

        band.on = random() % 4 != 0;
        return band;
    };

    auto targets = flatTargets();
    targets.linearPhase = linearPhase;
    targets.firLength = 0;

    for (auto& band : targets.bands)
        band = randomBand();

    EqDsp dsp;
    prepare (dsp, targets, 44100.0);

    auto signal = sine (440.0, 0.1, 44100.0, 256);

    for (int block = 0; block < 2000; ++block)
    {
        auto buffer = signal;

        for (auto& band : targets.bands)
            if (random() % 3 == 0)
                band = randomBand();

        dsp.setTargets (targets);

        float* channels[] { buffer.left.data(), buffer.right.data() };
        dsp.process (channels, 2, buffer.size());

        for (const auto sample : buffer.left)
            REQUIRE (std::isfinite (sample));
    }
}

// Linear phase ------------------------------------------------------------------------------

TEST_CASE ("linear phase is symmetric and matches minimum phase", "[eq]")
{
    const auto lengthIndex = GENERATE (0, 1, 2, 3);

    auto targets = flatTargets();
    targets.bands[0] = makeBand (BandType::band, 1000.0, 6.0, 2.0);
    targets.bands[1] = makeBand (BandType::highShelf, 8000.0, -6.0, 0.707, 2);
    targets.bands[2] = makeBand (BandType::lowShelf, 200.0, 4.0, 0.707, 2);
    targets.linearPhase = true;
    targets.firLength = lengthIndex;

    EqDsp dsp;
    prepare (dsp, targets);

    const auto latency = dsp.getLatencySamples();
    const auto fir = 2048 << lengthIndex;

    CHECK (latency == fir / 2 + fir / 16);

    Audio impulse (latency * 2 + fir + 4096);
    impulse.left[0] = impulse.right[0] = 1.0f;
    impulse.process (dsp);

    const auto peak = std::max_element (impulse.left.begin(), impulse.left.end(),
                                        [] (float a, float b) { return std::abs (a) < std::abs (b); });

    CHECK (std::distance (impulse.left.begin(), peak) == latency);

    for (int n = 1; n < fir / 2 - 1; ++n)
        REQUIRE (std::abs (impulse.left[(size_t) (latency + n)] - impulse.left[(size_t) (latency - n)]) < 2.0e-4f);

    for (const auto hz : { 100.0, 200.0, 500.0, 1000.0, 2000.0, 8000.0, 15000.0 })
    {
        auto expected = 0.0;

        for (int b = 0; b < 3; ++b)
            expected += responseDb (targets.bands[(size_t) b], rate, hz);

        EqDsp tone;
        prepare (tone, targets);

        auto signal = sine (hz, 0.1, rate, 48000);
        signal.process (tone);

        INFO (hz << " Hz, FIR " << fir);
        REQUIRE (levelAt (signal.left, 24000, hz, rate) == Catch::Approx (fancy::gainToDb (0.1) + expected).margin (0.5));
    }
}

TEST_CASE ("linear FIR length scales with the sample rate", "[eq]")
{
    auto targets = flatTargets();
    targets.linearPhase = true;
    targets.firLength = 0;

    for (const auto& [sampleRate, scale] : { std::pair (44100.0, 1), std::pair (48000.0, 1), std::pair (96000.0, 2), std::pair (192000.0, 4), std::pair (384000.0, 8) })
    {
        EqDsp dsp;
        prepare (dsp, targets, sampleRate);

        CHECK (dsp.getLatencySamples() == 1152 * scale);
    }
}

TEST_CASE ("eq latency is reported in every mode and bypass stays aligned", "[eq]")
{
    const auto mode = GENERATE (0, 1, 2, 3);

    auto targets = flatTargets();
    targets.linearPhase = mode == 3;
    targets.oversampling = mode < 3 ? mode : 0;
    targets.firLength = 0;
    targets.bypass = true;

    EqDsp dsp;
    prepare (dsp, targets);

    const auto latency = dsp.getLatencySamples();

    if (mode == 0)
        CHECK (latency == 0);
    else
        CHECK (latency > 0);

    const auto original = sine (997.0, 0.5, rate, 12000);
    auto signal = original;

    signal.process (dsp);

    for (int i = latency; i < signal.size(); ++i)
        REQUIRE (signal.left[(size_t) i] == Catch::Approx (original.left[(size_t) (i - latency)]).margin (1.0e-6));
}

TEST_CASE ("oversampling and filter stay time-aligned with bypass", "[eq]")
{
    const auto oversampling = GENERATE (1, 2);

    auto targets = flatTargets();
    targets.oversampling = oversampling;

    EqDsp dsp;
    prepare (dsp, targets);

    const auto latency = dsp.getLatencySamples();
    const auto original = sine (500.0, 0.5, rate, 24000);

    auto signal = original;
    signal.process (dsp);

    double worst = 0.0;

    for (int i = 4800; i < signal.size(); ++i)
        worst = std::max (worst, (double) std::abs (signal.left[(size_t) i] - original.left[(size_t) (i - latency)]));

    // Half a sample of error would be about 0.016 at this frequency and level.
    REQUIRE (worst < 0.01);
}

TEST_CASE ("changing phase mode while playing is safe", "[eq]")
{
    auto targets = flatTargets();
    targets.bands[0] = makeBand (BandType::band, 1000.0, 3.0, 2.0);

    EqDsp dsp;
    prepare (dsp, targets);

    auto signal = sine (500.0, 0.5, rate, 96000);

    constexpr int blockSize = 128;
    std::vector<int> latencies;

    for (int start = 0, block = 0; start < signal.size(); start += blockSize, ++block)
    {
        if (block == 200)
        {
            targets.linearPhase = true;
            targets.firLength = 0;
        }
        else if (block == 400)
        {
            targets.linearPhase = false;
            targets.oversampling = 1;
        }
        else if (block == 600)
        {
            targets.oversampling = 0;
        }

        dsp.setTargets (targets);

        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, std::min (blockSize, signal.size() - start));

        if (latencies.empty() || latencies.back() != dsp.getLatencySamples())
            latencies.push_back (dsp.getLatencySamples());
    }

    REQUIRE (latencies.size() == 4);
    REQUIRE (latencies[1] == 1152);
    REQUIRE (latencies[2] > 0);
    REQUIRE (latencies[3] == 0);

    for (const auto sample : signal.left)
        REQUIRE (std::abs (sample) < 0.75f);

    // Back to full level after the last change.
    const auto expected = fancy::gainToDb (0.5) + responseDb (targets.bands[0], rate, 500.0);

    REQUIRE (levelAt (signal.left, 84000, 500.0, rate) == Catch::Approx (expected).margin (0.05));
}

// Analyzer ----------------------------------------------------------------------------------

TEST_CASE ("analyzer reads a -12 dBFS sine correctly", "[eq]")
{
    const auto size = GENERATE (4096, 8192, 16384);
    const auto window = GENERATE (fancy::dsp::SpectrumAnalyzer::Window::hann, fancy::dsp::SpectrumAnalyzer::Window::blackmanHarris);
    const auto hz = GENERATE (1000.0, 997.0, 1234.5, 8000.3);

    fancy::dsp::SpectrumAnalyzer analyzer;

    fancy::dsp::SpectrumAnalyzer::Settings settings;
    settings.size = size;
    settings.window = window;
    settings.smoothing = 0.0;

    analyzer.configure (rate, settings);

    const auto amplitude = fancy::dbToGain (-12.0);
    const auto signal = sine (hz, amplitude, rate, size * 4);

    analyzer.addSamples (signal.left.data(), signal.size());

    INFO (size << " at " << hz << " Hz");
    REQUIRE (analyzer.levelBetween (hz * 0.97, hz * 1.03) == Catch::Approx (-12.0f).margin (0.5f));

    // And the line falls away well clear of the tone.
    REQUIRE (analyzer.levelAt (hz * 2.0) < -80.0f);
}

TEST_CASE ("analyzer tilt, smoothing and peak hold", "[eq]")
{
    fancy::dsp::SpectrumAnalyzer::Settings settings;
    settings.smoothing = 0.0;
    settings.window = fancy::dsp::SpectrumAnalyzer::Window::hann;

    const auto tone = sine (2000.0, 0.5, rate, 32768);

    SECTION ("pink tilt adds 4.5 dB per octave about 1 kHz")
    {
        fancy::dsp::SpectrumAnalyzer plain, tilted;

        plain.configure (rate, settings);
        settings.pinkTilt = true;
        tilted.configure (rate, settings);

        plain.addSamples (tone.left.data(), tone.size());
        tilted.addSamples (tone.left.data(), tone.size());

        CHECK (tilted.levelBetween (1950.0, 2050.0) - plain.levelBetween (1950.0, 2050.0) == Catch::Approx (4.5f).margin (0.1f));
    }

    SECTION ("peak hold keeps the highest reading")
    {
        fancy::dsp::SpectrumAnalyzer analyzer;
        analyzer.configure (rate, settings);
        analyzer.addSamples (tone.left.data(), tone.size());

        const Audio silence (32768);
        analyzer.addSamples (silence.left.data(), silence.size());

        CHECK (analyzer.levelBetween (1950.0, 2050.0) < -60.0f);
        CHECK (analyzer.peakBetween (1950.0, 2050.0) == Catch::Approx (fancy::gainToDb (0.5)).margin (0.5));

        analyzer.resetPeaks();
        CHECK (analyzer.peakBetween (1950.0, 2050.0) < -60.0f);
    }

    SECTION ("smoothing slows the display")
    {
        settings.smoothing = 1.0;

        fancy::dsp::SpectrumAnalyzer analyzer;
        analyzer.configure (rate, settings);

        const Audio silence (8192);
        analyzer.addSamples (tone.left.data(), tone.size());

        const auto before = analyzer.levelBetween (1950.0, 2050.0);
        analyzer.addSamples (silence.left.data(), silence.size());

        CHECK (analyzer.levelBetween (1950.0, 2050.0) > before - 30.0f);
    }
}

TEST_CASE ("analyzer FIFO sums to mono", "[eq]")
{
    auto fifo = std::make_unique<fancy::dsp::SampleFifo>();

    const float left[] { 1.0f, 0.5f, 0.0f };
    const float right[] { 1.0f, -0.5f, 1.0f };
    const float* channels[] { left, right };

    fifo->push (channels, 2, 3);

    float out[4] {};

    REQUIRE (fifo->pop (out, 4) == 3);
    CHECK (out[0] == 1.0f);
    CHECK (out[1] == 0.0f);
    CHECK (out[2] == 0.5f);
    CHECK (fifo->pop (out, 4) == 0);
}

TEST_CASE ("graph axis matches the Figma gridlines", "[eq]")
{
    constexpr double width = 733.0;

    CHECK (xForHz (20.0, width) == Catch::Approx (0.0).margin (0.01));
    CHECK (xForHz (24000.0, width) == Catch::Approx (width).margin (0.01));
    CHECK (xForHz (50.0, width) == Catch::Approx (49.5).margin (2.0));
    CHECK (xForHz (100.0, width) == Catch::Approx (103.5).margin (2.0));
    CHECK (xForHz (200.0, width) == Catch::Approx (169.5).margin (2.0));
    CHECK (xForHz (500.0, width) == Catch::Approx (268.5).margin (2.0));
    CHECK (xForHz (20000.0, width) == Catch::Approx (711.5).margin (2.0));
    CHECK (hzForX (xForHz (1234.0, width), width) == Catch::Approx (1234.0).margin (0.01));
}

// Processor ---------------------------------------------------------------------------------

TEST_CASE ("eq automation with an unchanged value is bit-identical", "[eq]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    EqProcessor plain, automated;

    for (auto* processor : { &plain, &automated })
    {
        setParameter (*processor, "b2gain", 6.0f);
        setParameter (*processor, "b2freq", 1234.5f);
        processor->setNonRealtime (true);
        processor->prepareToPlay (rate, 2048);
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

    REQUIRE (render (plain, 2048) == render (automated, 2048));
}

TEST_CASE ("log-scale toggle keeps Hz and moves the host value", "[eq]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    EqProcessor processor;
    setParameter (processor, "b2freq", 1000.0f);

    auto* frequency = processor.getApvts().getParameter ("b2freq");
    const auto logPosition = frequency->getValue();
    const auto defaultLog = frequency->getDefaultValue();

    setParameter (processor, "logFreq", 0.0f);

    CHECK (processor.getApvts().getRawParameterValue ("b2freq")->load() == Catch::Approx (1000.0f).margin (0.1f));
    CHECK (frequency->getValue() == Catch::Approx ((1000.0f - 20.0f) / 23980.0f).margin (1.0e-4f));
    CHECK (frequency->getValue() < logPosition);

    // Resetting a band gives the same Hz whichever mapping is in use.
    CHECK (frequency->convertFrom0to1 (frequency->getDefaultValue()) == Catch::Approx (500.0f).margin (0.1f));
    CHECK (frequency->getDefaultValue() != defaultLog);

    setParameter (processor, "logFreq", 1.0f);

    CHECK (processor.getApvts().getRawParameterValue ("b2freq")->load() == Catch::Approx (1000.0f).margin (0.1f));
    CHECK (frequency->getValue() == Catch::Approx (logPosition).margin (1.0e-4f));
}

TEST_CASE ("bands can be added until all 16 are used, and removed", "[eq]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    EqProcessor processor;

    CHECK (processor.isBandUsed (3));
    CHECK_FALSE (processor.isBandUsed (4));

    for (int band = 4; band < eq::maxBands; ++band)
        CHECK (processor.addBand (2000.0f, 3.0f) == band);

    CHECK (processor.addBand (2000.0f, 3.0f) == -1);

    const auto spec = processor.readBand (10);
    CHECK (spec.on);
    CHECK (spec.type == BandType::band);
    CHECK (spec.frequencyHz == Catch::Approx (2000.0));
    CHECK (spec.gainDb == Catch::Approx (3.0));

    processor.removeBand (10);

    CHECK_FALSE (processor.readBand (10).on);
    CHECK (processor.addBand (500.0f, 0.0f) == 10);
}

TEST_CASE ("eq state round-trips", "[eq]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    EqProcessor source;
    setParameter (source, "b6used", 1.0f);
    setParameter (source, "b6freq", 3210.0f);
    setParameter (source, "b6type", 4.0f);
    setParameter (source, "b6slope", 5.0f);
    setParameter (source, "gain", -7.3f);
    setParameter (source, "phase", 1.0f);
    setParameter (source, "compact", 1.0f);
    setParameter (source, "logFreq", 0.0f);

    juce::MemoryBlock state;
    source.getStateInformation (state);

    EqProcessor destination;
    destination.setStateInformation (state.getData(), (int) state.getSize());

    auto& apvts = destination.getApvts();
    CHECK (destination.isBandUsed (5));
    CHECK (apvts.getRawParameterValue ("b6freq")->load() == Catch::Approx (3210.0f));
    CHECK (apvts.getRawParameterValue ("b6type")->load() == 4.0f);
    CHECK (apvts.getRawParameterValue ("b6slope")->load() == 5.0f);
    CHECK (apvts.getRawParameterValue ("gain")->load() == Catch::Approx (-7.3f));
    CHECK (apvts.getRawParameterValue ("phase")->load() == 1.0f);
    CHECK (apvts.getRawParameterValue ("compact")->load() == 1.0f);
    CHECK (apvts.getRawParameterValue ("logFreq")->load() == 0.0f);
}

TEST_CASE ("corrupted eq state is ignored", "[eq]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    EqProcessor processor;

    juce::MemoryBlock state;
    processor.getStateInformation (state);
    processor.setStateInformation (state.getData(), (int) state.getSize() / 2);

    juce::Random random (9876);

    for (size_t i = 0; i < state.getSize(); ++i)
        state[i] = (char) random.nextInt (256);

    processor.setStateInformation (state.getData(), (int) state.getSize());

    juce::XmlElement damaged ("FancyEq");

    const auto addParameter = [&damaged] (const char* id, const char* value)
    {
        auto* element = damaged.createNewChildElement ("PARAM");
        element->setAttribute ("id", id);
        element->setAttribute ("value", value);
    };

    addParameter ("b1freq", "1e30");
    addParameter ("b2gain", "nan");
    addParameter ("b3type", "900");
    addParameter ("notAParameter", "1");

    juce::MemoryBlock damagedState;
    juce::AudioProcessor::copyXmlToBinary (damaged, damagedState);
    processor.setStateInformation (damagedState.getData(), (int) damagedState.getSize());

    CHECK (processor.getApvts().getRawParameterValue ("b1freq")->load() <= 24000.0f);
    CHECK ((int) processor.readBand (2).type <= 8);

    processor.prepareToPlay (rate, 512);
    CHECK (render (processor, 512).size() == 512);
}

TEST_CASE ("eq factory presets all load", "[eq]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    EqProcessor processor;
    processor.setNonRealtime (true);
    processor.prepareToPlay (rate, 512);

    auto& presets = processor.getPresets();

    for (int i = 0; i < presets.getNames().size(); ++i)
    {
        presets.load (i);

        for (const auto sample : render (processor, 512))
            REQUIRE (std::isfinite (sample));
    }

    presets.load (presets.getNames().indexOf ("Low Cut"));

    CHECK (processor.readBand (0).type == BandType::highPass);
    CHECK (processor.readBand (0).order == 4);
}

TEST_CASE ("eq bus layouts are mono and stereo", "[eq]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    EqProcessor processor;
    juce::AudioProcessor::BusesLayout layout;

    // The second input is the optional sidechain for dynamic bands (Phase 2.1 D).
    layout.inputBuses = { juce::AudioChannelSet::stereo(), juce::AudioChannelSet::disabled() };
    layout.outputBuses = { juce::AudioChannelSet::stereo() };
    CHECK (processor.checkBusesLayoutSupported (layout));

    layout.inputBuses = { juce::AudioChannelSet::mono(), juce::AudioChannelSet::disabled() };
    layout.outputBuses = { juce::AudioChannelSet::mono() };
    CHECK (processor.checkBusesLayoutSupported (layout));

    layout.inputBuses = { juce::AudioChannelSet::stereo(), juce::AudioChannelSet::disabled() };
    layout.outputBuses = { juce::AudioChannelSet::mono() };
    CHECK_FALSE (processor.checkBusesLayoutSupported (layout));
}

// Hidden from the default run. CI runs these on their own and keeps the output.
TEST_CASE ("eq measurements", "[.measure]")
{
    constexpr int blockSize = 128;
    constexpr int numBlocks = 18750; // 50 s of audio

    const auto measure = [&] (EqDsp::Targets targets, const char* name)
    {
        EqDsp dsp;
        prepare (dsp, targets);

        auto signal = sine (440.0, 0.5, rate, blockSize);
        float* channels[] { signal.left.data(), signal.right.data() };

        const auto start = std::chrono::steady_clock::now();

        for (int i = 0; i < numBlocks; ++i)
            dsp.process (channels, 2, blockSize);

        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
        const auto percent = 100.0 * elapsed.count() / ((double) (numBlocks * blockSize) / rate);

        std::printf ("EQ %-22s latency %5d samples (%6.1f ms), CPU %.4f %% at 48 kHz, 128 samples, stereo\n",
                     name, dsp.getLatencySamples(), 1000.0 * dsp.getLatencySamples() / rate, percent);
    };

    auto targets = flatTargets();

    for (int b = 0; b < 8; ++b)
        targets.bands[(size_t) b] = makeBand (b == 0 ? BandType::lowShelf : (b == 7 ? BandType::highShelf : BandType::band),
                                              60.0 * std::pow (2.0, b), b % 2 == 0 ? 4.0 : -4.0, 1.0);

    measure (targets, "8 bands (target < 1 %)");

    targets.oversampling = 1;
    measure (targets, "8 bands, 2x");

    targets.oversampling = 2;
    measure (targets, "8 bands, 4x");

    targets.oversampling = 0;
    targets.linearPhase = true;

    const char* const lengths[] { "linear 2048", "linear 4096", "linear 8192", "linear 16384" };

    for (int i = 0; i < 4; ++i)
    {
        targets.firLength = i;
        measure (targets, lengths[i]);
    }

    double bellError = 0.0;

    for (int i = 0; i < 400; ++i)
    {
        const auto hz = 20.0 * std::pow (1000.0, (double) i / 399.0);
        bellError = std::max (bellError, std::abs (responseDb (makeBand (BandType::band, 18000.0, 6.0, 1.0), 44100.0, hz) - analogBellDb (18000.0, 6.0, 1.0, hz)));
    }

    std::printf ("18 kHz +6 dB bell, Q 1, 44.1 kHz: worst error against analog up to 20 kHz %.3f dB (target 0.5 dB)\n", bellError);

    for (const auto sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        std::printf ("Rate %6.0f Hz, worst error below 20 kHz: bell 8 kHz %.3f dB, low pass 12 kHz %.3f dB, high shelf 8 kHz %.3f dB\n", sampleRate,
                     worstError (makeBand (BandType::band, 8000.0, -12.0, 2.0), sampleRate, 20000.0),
                     worstError (makeBand (BandType::lowPass, 12000.0, 0.0, 0.707, 4), sampleRate, 20000.0),
                     worstError (makeBand (BandType::highShelf, 8000.0, 12.0, 0.707, 2), sampleRate, 20000.0));
    }

    for (const auto hz : { 1000.0, 997.0, 1234.5 })
    {
        fancy::dsp::SpectrumAnalyzer analyzer;

        fancy::dsp::SpectrumAnalyzer::Settings settings;
        settings.smoothing = 0.0;

        analyzer.configure (rate, settings);

        const auto signal = sine (hz, fancy::dbToGain (-12.0), rate, settings.size * 4);
        analyzer.addSamples (signal.left.data(), signal.size());

        std::printf ("Analyzer, -12 dBFS sine at %.1f Hz reads %.2f dB\n", hz, analyzer.levelBetween (hz * 0.97, hz * 1.03));
    }
}
