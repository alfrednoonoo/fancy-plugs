// Phase 4 Wave 1 for Fancy FIR (docs/fir.md, "Phase 4 Wave 1", section 3): Resonance tilt,
// Learn for the Gate and Compressor curves, and the legacy check against v1.6.0's engine.

#include "FirLearn.h"
#include "FirProcessor.h"
#include "LegacyFirEngine.h"
#include "SpectralEngine.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;

void setCurve (fir::Shared& shared, int curve, float db)
{
    for (auto& value : shared.curves[(size_t) curve])
        value.store (db);

    shared.curveVersions[(size_t) curve].fetch_add (1);
}

std::vector<float> programme (int numSamples, unsigned seed)
{
    std::mt19937 random (seed);
    std::normal_distribution<float> gauss (0.0f, 0.05f);
    std::vector<float> samples ((size_t) numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto burst = (i / 4800) % 3 == 0 ? 4.0f : 1.0f; // louder every third 100 ms
        samples[(size_t) i] = gauss (random) * burst
                            + 0.2f * (float) std::sin (2.0 * std::numbers::pi * 3000.0 * i / rate)
                            + 0.1f * (float) std::sin (2.0 * std::numbers::pi * 700.0 * i / rate);
    }

    return samples;
}

template <typename Engine, typename Settings>
std::vector<float> run (Engine& engine, const std::vector<float>& left, const std::vector<float>& right, const Settings& settings)
{
    const auto numSamples = (int) left.size();
    std::vector<float> outLeft ((size_t) numSamples), outRight ((size_t) numSamples);

    for (int position = 0; position < numSamples; position += 64)
    {
        const auto count = std::min (64, numSamples - position);
        engine.process (left.data() + position, right.data() + position, nullptr, nullptr,
                        outLeft.data() + position, outRight.data() + position, 2, count, settings, true);
    }

    outLeft.insert (outLeft.end(), outRight.begin(), outRight.end());
    return outLeft;
}
} // namespace

// 1. Legacy -----------------------------------------------------------------------------------

TEST_CASE ("FIR W1: every mode renders as v1.6.0's engine with Tilt at 0", "[fir][w1]")
{
    const auto left = programme (48000, 1), right = programme (48000, 2);

    for (const auto mode : { fir::eq, fir::gate, fir::compressor, fir::subtract, fir::resonance })
    {
        INFO ("mode " << mode);

        fir::Shared sharedNow, sharedOld;

        for (auto* shared : { &sharedNow, &sharedOld })
        {
            setCurve (*shared, fir::eq, 3.0f);
            setCurve (*shared, fir::gate, -40.0f);
            setCurve (*shared, fir::compressor, -30.0f);
        }

        fir::SpectralEngine now ({ 2048, false, false }, rate, sharedNow);
        fir::legacy::SpectralEngine old ({ 2048, false, false }, rate, sharedOld);

        fir::FrameSettings settingsNow;
        settingsNow.mode = mode;
        settingsNow.resDepthDb = 9.0;

        fir::legacy::FrameSettings settingsOld;
        settingsOld.mode = mode;
        settingsOld.resDepthDb = 9.0;

        REQUIRE (run (now, left, right, settingsNow) == run (old, left, right, settingsOld));
    }
}

TEST_CASE ("FIR W1: Resonance tilt is a parameter that defaults to 0 %", "[fir][w1]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    FirProcessor processor;
    auto* tilt = processor.getApvts().getParameter ("resTilt");
    REQUIRE (tilt != nullptr);
    CHECK (processor.getApvts().getRawParameterValue ("resTilt")->load() == 0.0f);
    CHECK (processor.readTargets().frame.resTilt == 0.0);
}

// 2. Tilt -------------------------------------------------------------------------------------

TEST_CASE ("FIR W1: at +100 % tilt the depth at 16 kHz is twice that at 1 kHz", "[fir][w1]")
{
    using fir::SpectralEngine;

    CHECK (SpectralEngine::tiltFactor (1000.0, 1.0) == Catch::Approx (1.0));
    CHECK (SpectralEngine::tiltFactor (16000.0, 1.0) == Catch::Approx (2.0).epsilon (0.1));
    CHECK (SpectralEngine::tiltFactor (62.5, 1.0) == Catch::Approx (0.5).epsilon (0.1));
    CHECK (SpectralEngine::tiltFactor (16000.0, -1.0) == Catch::Approx (0.5).epsilon (0.1));
    CHECK (SpectralEngine::tiltFactor (16000.0, 0.0) == 1.0);

    // In the engine: a resonance at 12 kHz is cut more with the tilt towards the highs.
    std::vector<float> input (96000);
    std::mt19937 random (9);
    std::normal_distribution<float> gauss (0.0f, 0.02f);

    for (size_t i = 0; i < input.size(); ++i)
        input[i] = gauss (random) + 0.3f * (float) std::sin (2.0 * std::numbers::pi * 12000.0 * (double) i / rate);

    const auto levelAt12k = [&] (double tilt)
    {
        fir::Shared shared;
        fir::SpectralEngine engine ({ 2048, false, false }, rate, shared);
        fir::FrameSettings settings;
        settings.mode = fir::resonance;
        settings.resDepthDb = 6.0;
        settings.resTilt = tilt;

        const auto out = run (engine, input, input, settings);
        auto sine = 0.0, cosine = 0.0;

        for (int i = 48000; i < 96000; ++i)
        {
            const auto phase = 2.0 * std::numbers::pi * 12000.0 * (double) i / rate;
            sine += (double) out[(size_t) i] * std::sin (phase);
            cosine += (double) out[(size_t) i] * std::cos (phase);
        }

        return 10.0 * std::log10 (sine * sine + cosine * cosine);
    };

    const auto flat = levelAt12k (0.0), tilted = levelAt12k (1.0);
    INFO ("12 kHz tone, flat " << flat << " dB, tilted " << tilted << " dB");
    CHECK (tilted < flat - 1.0);
}

// 3. Learn ------------------------------------------------------------------------------------

TEST_CASE ("FIR W1: Learn places the Gate curve 6 dB over the noise and under the tone", "[fir][w1]")
{
    // White noise at -50 dBFS, and a 2 kHz tone that plays 1 s in every 1.5 s.
    const auto noiseLevel = std::pow (10.0, -50.0 / 20.0);
    std::mt19937 random (11);
    std::normal_distribution<double> gauss (0.0, noiseLevel);

    const auto feed = [&] (fir::CurveLearner& learner, bool withTone)
    {
        constexpr int chunk = 1600;
        std::vector<float> block (chunk);
        auto n = 0;

        while (! learner.isComplete())
        {
            for (int i = 0; i < chunk; ++i, ++n)
            {
                const auto toneOn = withTone && (n % 72000) < 48000;
                block[(size_t) i] = (float) (gauss (random) + (toneOn ? 0.3 * std::sin (2.0 * std::numbers::pi * 2000.0 * n / rate) : 0.0));
            }

            learner.addSamples (block.data(), chunk);
        }
    };

    fir::CurveLearner noiseOnly, learner;
    noiseOnly.start (fir::gate, rate); // the noise alone: its 20th percentile, + 6
    feed (noiseOnly, false);
    learner.start (fir::gate, rate);
    feed (learner, true);

    // The noise's level per band: the 20th percentile of the noise alone.
    auto noise = noiseOnly.result();

    for (auto& point : noise)
        point.db -= 6.0;
    const auto gate = learner.result();
    REQUIRE (gate.size() == noise.size());
    REQUIRE (gate.size() >= 29);

    for (size_t band = 0; band < gate.size(); ++band)
    {
        const auto hz = gate[band].hz;

        // Few bins below 500 Hz; and the analyzer smears the tone (70 dB over the noise) up to
        // about an octave and a half either side (CI run 44949179446), so those bands are
        // checked against the tone below.
        if (hz < 500.0 || std::abs (std::log2 (hz / 2000.0)) < 1.6)
            continue;

        INFO (hz << " Hz: curve " << gate[band].db << " dB, noise " << noise[band].db << " dB");
        CHECK (gate[band].db - noise[band].db == Catch::Approx (6.0).margin (1.0));
    }

    // At the tone's band the curve stays far under the tone (-10.5 dB as the analyzer reads a
    // sine of 0.3).
    for (const auto& point : gate)
        if (std::abs (std::log2 (point.hz / 2000.0)) < 0.1)
            CHECK (point.db < 20.0 * std::log10 (0.3) - 20.0);

    // The Compressor learns its 80th percentile, which is over the Gate's 20th.
    learner.start (fir::compressor, rate);
    feed (learner, true);
    const auto compressor = learner.result();
    REQUIRE (compressor.size() == gate.size());

    for (size_t band = 0; band < gate.size(); ++band)
        if (gate[band].hz > 500.0 && std::abs (std::log2 (gate[band].hz / 2000.0)) >= 1.6)
            CHECK (compressor[band].db > noise[band].db - 0.5);
}
