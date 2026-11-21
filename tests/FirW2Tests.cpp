// Phase 4 Wave 2 for Fancy FIR: Resonance depth nodes (docs/fir.md, "Phase 4 Wave 2", section 4).
// The legacy check is FirW1Tests' "every mode renders as v1.6.0's engine": a flat depth curve,
// which is what Shared and the processor start with, takes the old path.

#include "Curve.h"
#include "FirProcessor.h"
#include "SpectralEngine.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;

void setCurve (fir::Shared& shared, int curve, float value)
{
    for (auto& item : shared.curves[(size_t) curve])
        item.store (value);

    shared.curveVersions[(size_t) curve].fetch_add (1);
}

// A 12 kHz resonance over quiet noise, through Resonance; the tone's level in the second half.
double toneLevel (float depthPercent, double tilt, double depthDb = 4.0)
{
    std::vector<float> input (96000);
    std::mt19937 random (9);
    std::normal_distribution<float> gauss (0.0f, 0.02f);

    for (size_t i = 0; i < input.size(); ++i)
        input[i] = gauss (random) + 0.3f * (float) std::sin (2.0 * std::numbers::pi * 12000.0 * (double) i / rate);

    fir::Shared shared;
    setCurve (shared, fir::depthCurve, depthPercent);
    fir::SpectralEngine engine ({ 2048, false, false }, rate, shared);

    fir::FrameSettings settings;
    settings.mode = fir::resonance;
    settings.resDepthDb = depthDb;
    settings.resTilt = tilt;

    std::vector<float> left (input.size()), right (input.size());

    for (size_t position = 0; position < input.size(); position += 64)
        engine.process (input.data() + position, input.data() + position, nullptr, nullptr,
                        left.data() + position, right.data() + position, 2, 64, settings, true);

    auto sine = 0.0, cosine = 0.0;

    for (size_t i = 48000; i < left.size(); ++i)
    {
        const auto phase = 2.0 * std::numbers::pi * 12000.0 * (double) i / rate;
        sine += (double) left[i] * std::sin (phase);
        cosine += (double) left[i] * std::cos (phase);
    }

    return 10.0 * std::log10 (sine * sine + cosine * cosine);
}
} // namespace

TEST_CASE ("FIR W2: the depth curve scales how far a resonance is cut", "[fir][w2]")
{
    const auto uncut = toneLevel (100.0f, 0.0, 0.0);
    const auto flat = toneLevel (100.0f, 0.0);
    const auto doubled = toneLevel (200.0f, 0.0);
    const auto none = toneLevel (0.0f, 0.0);

    INFO ("uncut " << uncut << ", flat " << flat - uncut << " dB, 200 % " << doubled - uncut << " dB, 0 % " << none - uncut << " dB");
    CHECK (flat - uncut == Catch::Approx (-4.0).margin (1.0));
    CHECK (doubled - uncut == Catch::Approx (2.0 * (flat - uncut)).margin (1.0));
    CHECK (std::abs (none - uncut) < 0.2);
}

TEST_CASE ("FIR W2: the depth curve and Tilt multiply", "[fir][w2]")
{
    // Tilt +100 % at 12 kHz is a factor of about 1.86; a curve of 100 / 1.86 % cancels it.
    const auto cancel = (float) (100.0 / fir::SpectralEngine::tiltFactor (12000.0, 1.0));
    const auto flat = toneLevel (100.0f, 0.0);
    const auto both = toneLevel (cancel, 1.0);

    INFO ("flat " << flat << ", tilt with the cancelling curve " << both);
    CHECK (both == Catch::Approx (flat).margin (0.3));
}

TEST_CASE ("FIR W2: depth nodes are saved, restored and kept in range", "[fir][w2]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    CHECK (fir::clampCurveValue (fir::depthCurve, 350.0) == 200.0);
    CHECK (fir::clampCurveValue (fir::depthCurve, -5.0) == 0.0);
    CHECK (fir::clampCurveValue (fir::gate, 100.0) == 48.0);
    CHECK (fir::defaultCurveDb (fir::depthCurve) == 100.0);

    FirProcessor processor;
    CHECK (processor.curveDbAt (fir::depthCurve, 1000.0) == 100.0);

    processor.setPoints (fir::depthCurve, { { 300.0, 50.0 }, { 3000.0, 180.0 } });

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    FirProcessor copy;
    copy.setStateInformation (state.getData(), (int) state.getSize());

    const auto& points = copy.getPoints (fir::depthCurve);
    REQUIRE (points.size() == 2);
    CHECK (points[0].db == Catch::Approx (50.0));
    CHECK (points[1].hz == Catch::Approx (3000.0));
    CHECK (points[1].db == Catch::Approx (180.0));
    CHECK (copy.curveDbAt (fir::depthCurve, 3000.0) == Catch::Approx (180.0));

    copy.resetCurve (fir::depthCurve);
    CHECK (copy.getPoints (fir::depthCurve).empty());
    CHECK (copy.curveDbAt (fir::depthCurve, 3000.0) == 100.0);
}
