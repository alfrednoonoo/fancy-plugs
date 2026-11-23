// Phase 4 Wave 2 for Fancy EQ: the Brickwall cut slope (item 15) and the Character output stage
// (item 16), docs/eq.md, sections "Phase 4 Wave 2". The numbers come from
// docs/research/eq-w2-prepush.py, measured the way these tests measure them.

#include "EqDsp.h"
#include "EqProcessor.h"
#include "LegacyEqDsp.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = 3.14159265358979323846;

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples), right ((size_t) numSamples) {}
    int size() const { return (int) left.size(); }
};

template <typename Dsp>
void run (Dsp& dsp, Stereo& signal, int blockSize = 333)
{
    for (int start = 0; start < signal.size(); start += blockSize)
    {
        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, std::min (blockSize, signal.size() - start));
    }
}

Stereo programme (int numSamples, unsigned seed)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<double> noise (-1.0, 1.0);
    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto envelope = 0.05 + 0.6 * std::pow (0.5 + 0.5 * std::sin (2.0 * pi * 3.0 * i / rate), 4.0);
        const auto tone = std::sin (2.0 * pi * 70.0 * i / rate) + 0.4 * std::sin (2.0 * pi * 2700.0 * i / rate);

        signal.left[(size_t) i] = (float) (envelope * (0.7 * tone + 0.2 * noise (random)));
        signal.right[(size_t) i] = (float) (envelope * (0.5 * tone + 0.3 * noise (random)));
    }

    return signal;
}

Stereo sine (double hz, double amplitude, int numSamples)
{
    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * hz * i / rate));

    return signal;
}

eq::BandSpec band (eq::BandType type, double hz, double gainDb = 0.0, double q = 0.707, int order = 2)
{
    eq::BandSpec spec;
    spec.type = type;
    spec.frequencyHz = hz;
    spec.gainDb = gainDb;
    spec.q = q;
    spec.order = order;
    spec.on = true;
    return spec;
}

double responseDb (const eq::BandSpec& spec, double hz)
{
    return 20.0 * std::log10 (std::abs (eq::design (spec, rate).responseAt (2.0 * pi * hz / rate)));
}

template <typename Dsp, typename Targets>
void prepare (Dsp& dsp, const Targets& targets)
{
    dsp.prepare (rate);
    dsp.setSynchronousDesign (true);
    dsp.snapToTargets (targets);
}

// The magnitude of one 1 Hz bin over the last second, Hann-windowed (the model's analyse()).
double binMagnitude (const std::vector<float>& samples, double hz)
{
    const auto length = (int) rate;
    const auto start = samples.size() - (size_t) length;
    std::complex<double> sum {};

    for (int n = 0; n < length; ++n)
    {
        const auto window = 0.5 - 0.5 * std::cos (2.0 * pi * n / (double) (length - 1));
        sum += (double) samples[start + (size_t) n] * window * std::polar (1.0, -2.0 * pi * hz * n / rate);
    }

    return std::abs (sum);
}

double largestStep (const std::vector<float>& samples, size_t from = 0)
{
    double step = 0.0;

    for (size_t i = std::max<size_t> (from, 1); i < samples.size(); ++i)
        step = std::max (step, (double) std::abs (samples[i] - samples[i - 1]));

    return step;
}
} // namespace

// 1. Compatibility ------------------------------------------------------------

// Every existing slope, on cuts and shelves, in both phase modes, with Character Clean: as v1.6.0
// rendered it (v1.7.0 renders as v1.6.0 at Wave 1's defaults, tests/EqW1Tests.cpp).
TEST_CASE ("EQ Wave 2 at its defaults renders every old slope as before", "[eq][w2]")
{
    const auto variant = GENERATE (0, 1, 2, 3);
    const auto slopeSet = GENERATE (0, 1);

    LegacyEqDsp::Targets old;
    const int orders[2][4] { { 1, 2, 3, 4 }, { 6, 8, 8, 6 } };

    old.bands[0] = band (eq::BandType::highPass, 60.0, 0.0, 0.707, orders[slopeSet][0]);
    old.bands[1] = band (eq::BandType::lowShelf, 150.0, 3.0, 0.707, orders[slopeSet][1]);
    old.bands[2] = band (eq::BandType::band, 1200.0, -4.0, 1.5);
    old.bands[3] = band (eq::BandType::highShelf, 7000.0, 2.5, 0.9, orders[slopeSet][2]);
    old.bands[4] = band (eq::BandType::lowPass, 15000.0, 0.0, 1.2, orders[slopeSet][3]);
    old.bands[5] = band (eq::BandType::tilt, 900.0, 2.0, 0.707, orders[slopeSet][0]);
    old.gain = 0.9;
    old.outputGain = 1.1;

    switch (variant)
    {
        case 1: old.oversampling = 1; break;
        case 2: old.oversampling = 2; break;
        case 3: old.linearPhase = true; old.firLength = 1; break;
        default: break;
    }

    EqDsp::Targets now;
    now.bands = old.bands;
    now.dynamics = old.dynamics;
    now.gain = old.gain;
    now.outputGain = old.outputGain;
    now.firLength = old.firLength;
    now.oversampling = old.oversampling;
    now.linearPhase = old.linearPhase;

    LegacyEqDsp legacy;
    EqDsp current;
    prepare (legacy, old);
    prepare (current, now);

    REQUIRE (current.getLatencySamples() == legacy.getLatencySamples());

    auto a = programme (24000, 9);
    auto b = a;
    run (legacy, a);
    run (current, b);

    INFO ("variant " << variant << ", slopes " << slopeSet);
    REQUIRE (a.left == b.left);
    REQUIRE (a.right == b.right);
}

TEST_CASE ("Slope keeps its old choices and appends Brickwall; Character is Clean by default", "[eq][w2]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    EqProcessor processor;
    auto& apvts = processor.getApvts();

    auto* slope = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("b1slope"));
    REQUIRE (slope != nullptr);
    CHECK (slope->choices == juce::StringArray { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct", "Brickwall (96)" });
    CHECK (slope->getIndex() == 1);

    auto* character = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("character"));
    REQUIRE (character != nullptr);
    CHECK (character->choices == juce::StringArray { "Clean", "Subtle", "Warm" });
    CHECK (processor.readTargets().character == EqDsp::characterClean);

    // A saved 48 dB/oct stays 48; Brickwall is order 16.
    processor.setParameter ("b1type", (float) (int) eq::BandType::highPass);
    processor.setParameter ("b1slope", 5.0f);
    CHECK (processor.readBand (0).order == 8);
    processor.setParameter ("b1slope", 6.0f);
    CHECK (processor.readBand (0).order == 16);

    processor.setParameter ("character", 2.0f);
    CHECK (processor.readTargets().character == EqDsp::characterWarm);
}

// 2. Brickwall, minimum phase -------------------------------------------------

TEST_CASE ("A Brickwall high-pass cuts 32 dB a third of an octave down and leaves the rest", "[eq][w2]")
{
    const auto brickwall = band (eq::BandType::highPass, 100.0, 0.0, 0.707, 16);

    CHECK (eq::design (brickwall, rate).numSections == 8);
    CHECK (responseDb (brickwall, 100.0 / std::cbrt (2.0)) == Catch::Approx (-32.1).margin (1.0));
    CHECK (responseDb (brickwall, 100.0) == Catch::Approx (-3.01).margin (0.2));
    CHECK (responseDb (brickwall, 50.0) < -90.0);

    for (auto hz = 100.0 * std::cbrt (2.0); hz < 20000.0; hz *= 1.05)
    {
        INFO (hz << " Hz");
        REQUIRE (std::abs (responseDb (brickwall, hz)) < 0.05);
    }

    // The low-pass mirrors it.
    const auto low = band (eq::BandType::lowPass, 2000.0, 0.0, 0.707, 16);
    CHECK (responseDb (low, 2000.0 * std::cbrt (2.0)) == Catch::Approx (-32.1).margin (1.0));
    CHECK (std::abs (responseDb (low, 2000.0 / std::cbrt (2.0))) < 0.05);
}

TEST_CASE ("Brickwall is stable at every frequency, Q and rate", "[eq][w2]")
{
    const auto sampleRate = GENERATE (44100.0, 96000.0, 384000.0);

    for (const auto type : { eq::BandType::highPass, eq::BandType::lowPass })
        for (const auto hz : { 20.0, 60.0, 1000.0, 15000.0, 24000.0 })
            for (const auto q : { 0.1, 0.707, 5.0, 30.0 })
            {
                const auto filter = eq::design (band (type, hz, 0.0, q, 16), sampleRate);
                REQUIRE (filter.numSections == 8);

                for (int s = 0; s < filter.numSections; ++s)
                {
                    const auto& c = filter.sections[(size_t) s];
                    REQUIRE (std::abs (c.a2) < 1.0);
                    REQUIRE (std::abs (c.a1) < 1.0 + c.a2);
                }
            }
}

TEST_CASE ("A shelf or Tilt set to Brickwall stays at 48 dB/oct", "[eq][w2]")
{
    for (const auto type : { eq::BandType::lowShelf, eq::BandType::highShelf, eq::BandType::tilt })
    {
        const auto at48 = eq::design (band (type, 500.0, 6.0, 0.707, 8), rate);
        const auto atBrickwall = eq::design (band (type, 500.0, 6.0, 0.707, 16), rate);

        REQUIRE (atBrickwall.numSections == at48.numSections);

        for (const auto hz : { 50.0, 400.0, 500.0, 700.0, 5000.0 })
            CHECK (std::abs (atBrickwall.responseAt (2.0 * pi * hz / rate)) == Catch::Approx (std::abs (at48.responseAt (2.0 * pi * hz / rate))).epsilon (1.0e-12));
    }
}

// 3. Brickwall, linear phase --------------------------------------------------

TEST_CASE ("Linear phase gives a Brickwall the same magnitude, symmetric", "[eq][w2]")
{
    EqDsp::Targets targets;
    targets.bands[0] = band (eq::BandType::highPass, 1000.0, 0.0, 0.707, 16);
    targets.linearPhase = true;
    targets.firLength = 2; // 8192 at 48 kHz

    EqDsp dsp;
    prepare (dsp, targets);

    const auto latency = dsp.getLatencySamples();
    REQUIRE (latency == 8192 / 2 + 8192 / 16);

    Stereo impulse (latency * 2 + 8192 + 4096);
    impulse.left[0] = impulse.right[0] = 1.0f;
    run (dsp, impulse, 512);

    const auto& h = impulse.left;

    for (int n = 1; n < 4095; ++n)
        REQUIRE (std::abs (h[(size_t) (latency + n)] - h[(size_t) (latency - n)]) < 2.0e-4f);

    const auto filter = eq::design (targets.bands[0], rate);

    for (auto hz = 20.0; hz < 20000.0; hz *= 1.03)
    {
        const auto expected = 20.0 * std::log10 (std::abs (filter.responseAt (2.0 * pi * hz / rate)));

        if (expected < -60.0)
            continue;

        // The DFT of the whole response, with the latency's linear phase taken out.
        std::complex<double> sum {};

        for (size_t n = 0; n < h.size(); ++n)
            sum += (double) h[n] * std::polar (1.0, -2.0 * pi * hz * ((double) n - latency) / rate);

        INFO (hz << " Hz: expected " << expected << " dB");
        REQUIRE (20.0 * std::log10 (std::abs (sum)) == Catch::Approx (expected).margin (0.5));
    }
}

// 4. Changing to Brickwall while playing --------------------------------------

TEST_CASE ("Changing a cut from 48 dB/oct to Brickwall while it plays doesn't click", "[eq][w2][sweep]")
{
    const auto oversampling = GENERATE (0, 2);

    EqDsp::Targets targets;
    targets.bands[0] = band (eq::BandType::highPass, 100.0, 0.0, 0.707, 8);
    targets.oversampling = oversampling;

    EqDsp dsp;
    prepare (dsp, targets);

    // A natural step of 0.1 * 2 pi * 1000 / 48000 = 0.0131 per sample; the fade and the new
    // sections starting from rest add little, as a ring near 100 Hz moves slowly.
    auto signal = sine (1000.0, 0.1, 48000);
    constexpr int blockSize = 128;

    for (int start = 0, block = 0; start < signal.size(); start += blockSize, ++block)
    {
        if (block == 60)
            targets.bands[0].order = 16;
        else if (block == 160)
            targets.bands[0].order = 8;

        dsp.setTargets (targets);

        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, std::min (blockSize, signal.size() - start));
    }

    INFO ("oversampling " << oversampling);
    REQUIRE (largestStep (signal.left, 4000) < 0.025);
}

// Character -------------------------------------------------------------------

namespace
{
struct Harmonics
{
    double levelChangeDb = 0.0, thdPercent = 0.0, h2Db = 0.0, h3Db = 0.0;
};

Harmonics measureCharacter (int character, int oversampling, double levelDb)
{
    EqDsp::Targets targets;
    targets.character = character;
    targets.oversampling = oversampling;

    EqDsp dsp;
    prepare (dsp, targets);

    const auto input = sine (1000.0, std::pow (10.0, levelDb / 20.0), 2 * (int) rate);
    auto output = input;
    run (dsp, output, 256);

    std::array<double, 8> h {};

    for (int k = 1; k <= 7; ++k)
        h[(size_t) k] = binMagnitude (output.left, 1000.0 * k);

    auto sumSquares = 0.0;

    for (int k = 2; k <= 7; ++k)
        sumSquares += h[(size_t) k] * h[(size_t) k];

    // Against Clean at the same Oversampling, so the oversampler's own passband doesn't count.
    auto cleanTargets = targets;
    cleanTargets.character = EqDsp::characterClean;

    EqDsp clean;
    prepare (clean, cleanTargets);

    auto reference = input;
    run (clean, reference, 256);

    Harmonics result;
    result.levelChangeDb = 20.0 * std::log10 (h[1] / binMagnitude (reference.left, 1000.0));
    result.thdPercent = 100.0 * std::sqrt (sumSquares) / h[1];
    result.h2Db = 20.0 * std::log10 (h[2] / h[1] + 1.0e-15);
    result.h3Db = 20.0 * std::log10 (h[3] / h[1] + 1.0e-15);
    return result;
}
} // namespace

TEST_CASE ("Character's THD and level match the model", "[eq][w2]")
{
    struct Expected
    {
        int character, oversampling;
        double levelDb, thdPercent, levelChangeDb;
    };

    // docs/research/eq-w2-prepush.py, sections 3 and 4.
    const Expected table[] {
        { EqDsp::characterSubtle, 0, -18.0, 0.025, -0.025 }, { EqDsp::characterSubtle, 0, -12.0, 0.098, -0.044 }, { EqDsp::characterSubtle, 0, -6.0, 0.385, -0.120 },
        { EqDsp::characterWarm, 0, -18.0, 0.108, -0.030 },   { EqDsp::characterWarm, 0, -12.0, 0.425, -0.070 },   { EqDsp::characterWarm, 0, -6.0, 1.603, -0.221 },
        { EqDsp::characterSubtle, 2, -18.0, 0.025, -0.008 }, { EqDsp::characterSubtle, 2, -12.0, 0.099, -0.027 }, { EqDsp::characterSubtle, 2, -6.0, 0.388, -0.103 },
        { EqDsp::characterWarm, 2, -18.0, 0.109, -0.014 },   { EqDsp::characterWarm, 2, -12.0, 0.427, -0.054 },   { EqDsp::characterWarm, 2, -6.0, 1.611, -0.206 },
    };

    for (const auto& row : table)
    {
        const auto measured = measureCharacter (row.character, row.oversampling, row.levelDb);

        INFO ("character " << row.character << ", oversampling " << row.oversampling << ", " << row.levelDb << " dBFS: THD "
                           << measured.thdPercent << " %, level " << measured.levelChangeDb << " dB");
        CHECK (measured.thdPercent == Catch::Approx (row.thdPercent).epsilon (0.1));
        CHECK (measured.levelChangeDb == Catch::Approx (row.levelChangeDb).margin (0.03));
        CHECK (measured.levelChangeDb > -0.25);
    }
}

TEST_CASE ("Warm leads with the 2nd harmonic and keeps DC out", "[eq][w2]")
{
    const auto warm = measureCharacter (EqDsp::characterWarm, 0, -12.0);
    INFO ("H2 " << warm.h2Db << " dB, H3 " << warm.h3Db << " dB");
    CHECK (warm.h2Db > warm.h3Db + 3.0);

    const auto subtle = measureCharacter (EqDsp::characterSubtle, 0, -12.0);
    CHECK (subtle.h2Db < -150.0);

    // A loud asymmetric case: 0.9 peak at 100 Hz, the mean of the last second.
    EqDsp::Targets targets;
    targets.character = EqDsp::characterWarm;

    EqDsp dsp;
    prepare (dsp, targets);

    auto signal = sine (100.0, 0.9, 3 * (int) rate);
    run (dsp, signal, 256);

    auto sum = 0.0;

    for (size_t i = signal.left.size() - (size_t) rate; i < signal.left.size(); ++i)
        sum += (double) signal.left[i];

    const auto dc = std::abs (sum / rate);
    INFO ("DC " << 20.0 * std::log10 (dc + 1.0e-20) << " dBFS");
    CHECK (dc < 1.0e-4);
}

TEST_CASE ("Warm at 4x leaves no alias above -90 dBFS below 20 kHz", "[eq][w2]")
{
    EqDsp::Targets targets;
    targets.character = EqDsp::characterWarm;
    targets.oversampling = 2;

    EqDsp dsp;
    prepare (dsp, targets);

    constexpr int order = 16;
    constexpr int size = 1 << order;
    const auto settle = (int) rate / 2;

    auto signal = sine (10000.0, std::pow (10.0, -6.0 / 20.0), settle + size);
    run (dsp, signal, 512);

    std::vector<float> fft ((size_t) size * 2, 0.0f);

    for (int n = 0; n < size; ++n)
        fft[(size_t) n] = signal.left[(size_t) (settle + n)] * (float) (0.5 - 0.5 * std::cos (2.0 * pi * n / size));

    juce::dsp::FFT transform (order);
    transform.performFrequencyOnlyForwardTransform (fft.data(), true);

    auto worstDb = -400.0;
    auto worstHz = 0.0;

    for (int k = 1; k < size / 2; ++k)
    {
        const auto hz = k * rate / size;

        if (hz < 50.0 || hz > 19800.0 || std::abs (hz - 10000.0) < 200.0)
            continue;

        const auto db = 20.0 * std::log10 ((double) fft[(size_t) k] * 4.0 / size + 1.0e-20);

        if (db > worstDb)
        {
            worstDb = db;
            worstHz = hz;
        }
    }

    INFO ("largest at " << worstHz << " Hz: " << worstDb << " dBFS");
    CHECK (worstDb < -90.0);
}

TEST_CASE ("Switching Character while it plays fades without a click", "[eq][w2][sweep]")
{
    const auto oversampling = GENERATE (0, 2);

    EqDsp::Targets targets;
    targets.oversampling = oversampling;

    EqDsp dsp;
    prepare (dsp, targets);

    // A natural step of 0.5 * 2 pi * 1000 / 48000 = 0.0653 per sample, and Warm's own is the
    // same (docs/research/eq-w2-prepush.py). The stage is gentle enough that even a hard switch
    // stays under this; the test guards against a reset or a dropped sample in the fade.
    auto signal = sine (1000.0, 0.5, 48000);
    constexpr int blockSize = 128;
    const int changes[] { EqDsp::characterWarm, EqDsp::characterSubtle, EqDsp::characterClean, EqDsp::characterWarm, EqDsp::characterClean };

    for (int start = 0, block = 0; start < signal.size(); start += blockSize, ++block)
    {
        if (block % 60 == 30 && block / 60 < 5)
            targets.character = changes[block / 60];

        dsp.setTargets (targets);

        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, std::min (blockSize, signal.size() - start));
    }

    INFO ("oversampling " << oversampling);
    REQUIRE (largestStep (signal.left, 2000) < 0.07);
}

TEST_CASE ("Character back to Clean is the EQ to the bit", "[eq][w2]")
{
    EqDsp::Targets targets;
    targets.bands[0] = band (eq::BandType::band, 800.0, 4.0, 1.0);

    EqDsp plain, switched;
    prepare (plain, targets);
    prepare (switched, targets);

    auto a = programme (24000, 4);
    auto b = a;

    // Warm for 100 ms, then Clean: once the 10 ms fade is over the two render the same.
    run (plain, a, 128);

    for (int start = 0, block = 0; start < b.size(); start += 128, ++block)
    {
        targets.character = block < 38 ? EqDsp::characterWarm : EqDsp::characterClean;
        switched.setTargets (targets);

        float* channels[] { b.left.data() + start, b.right.data() + start };
        switched.process (channels, 2, std::min (128, b.size() - start));
    }

    const auto from = (size_t) (128 * 38 + 4 * 128 + (int) (0.011 * rate));
    REQUIRE (std::equal (a.left.begin() + (std::ptrdiff_t) from, a.left.end(), b.left.begin() + (std::ptrdiff_t) from));
}

// 5. CPU ------------------------------------------------------------------------

TEST_CASE ("eq w2 CPU measurement", "[.measure]")
{
    constexpr int blockSize = 128;
    constexpr int numBlocks = 18750; // 50 s of audio

    const auto measure = [&] (const EqDsp::Targets& targets)
    {
        std::vector<double> runs;

        for (int r = 0; r < 5; ++r)
        {
            EqDsp dsp;
            prepare (dsp, targets);

            auto signal = sine (440.0, 0.5, blockSize);
            const auto original = signal;
            float* channels[] { signal.left.data(), signal.right.data() };

            const auto start = std::chrono::steady_clock::now();

            for (int i = 0; i < numBlocks; ++i)
            {
                signal = original;
                channels[0] = signal.left.data();
                channels[1] = signal.right.data();
                dsp.process (channels, 2, blockSize);
            }

            const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
            runs.push_back (100.0 * elapsed.count() / ((double) (numBlocks * blockSize) / rate));
        }

        std::sort (runs.begin(), runs.end());
        return runs[2];
    };

    EqDsp::Targets bells;

    for (int b = 0; b < eq::maxBands; ++b)
        bells.bands[(size_t) b] = band (eq::BandType::band, 40.0 * std::pow (1.3, b), b % 2 == 0 ? 3.0 : -3.0, 2.0);

    EqDsp::Targets brickwalls;

    for (int b = 0; b < eq::maxBands; ++b)
        brickwalls.bands[(size_t) b] = b % 2 == 0 ? band (eq::BandType::highPass, 20.0 + 2.0 * b, 0.0, 0.707, 16)
                                                  : band (eq::BandType::lowPass, 20000.0 - 100.0 * b, 0.0, 0.707, 16);

    EqDsp::Targets clean;
    auto warm = clean;
    warm.character = EqDsp::characterWarm;
    auto warm4x = warm;
    warm4x.oversampling = 2;
    auto clean4x = clean;
    clean4x.oversampling = 2;

    const auto bellsCpu = measure (bells);
    const auto brickwallCpu = measure (brickwalls);
    const auto cleanCpu = measure (clean);
    const auto warmCpu = measure (warm);
    const auto clean4xCpu = measure (clean4x);
    const auto warm4xCpu = measure (warm4x);

    std::printf ("eq w2 cpu (median of 5, 48 kHz, 128 samples, stereo): 24 Brickwall cuts %.3f %% (target < 1.5 %% for 24 bands), "
                 "24 bells %.3f %% in the same run (%.2f x)\n",
                 brickwallCpu, bellsCpu, brickwallCpu / bellsCpu);
    std::printf ("eq w2 cpu, Character (no bands): Clean %.3f %%, Warm %.3f %% (adds %.3f %%); at 4x Clean %.3f %%, Warm %.3f %% (adds %.3f %%)\n",
                 cleanCpu, warmCpu, warmCpu - cleanCpu, clean4xCpu, warm4xCpu, warm4xCpu - clean4xCpu);
}
