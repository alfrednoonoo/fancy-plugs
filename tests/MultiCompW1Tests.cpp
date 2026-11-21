// Phase 4 Wave 1 for Fancy MultiComp: band mute and Learn crossovers (docs/multicomp.md,
// "Phase 4 Wave 1", section 3).

#include "MultiCompDsp.h"
#include "MultiCompProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = 3.14159265358979323846;

double rmsDbAfter (MultiCompDsp::Targets targets, double hz)
{
    MultiCompDsp dsp;
    dsp.prepare (rate);
    dsp.setSynchronousDesign (true);
    dsp.snapToTargets (targets);

    std::vector<float> left ((size_t) rate), right ((size_t) rate);

    for (size_t i = 0; i < left.size(); ++i)
        left[i] = right[i] = (float) (0.25 * std::sin (2.0 * pi * hz * (double) i / rate));

    for (size_t start = 0; start < left.size(); start += 512)
    {
        float* channels[] { left.data() + start, right.data() + start };
        const float* sidechain[] { nullptr, nullptr };
        dsp.process (channels, 2, sidechain, 0, (int) std::min<size_t> (512, left.size() - start));
    }

    double sum = 0.0;

    for (size_t i = left.size() / 2; i < left.size(); ++i)
        sum += (double) left[i] * (double) left[i];

    return 10.0 * std::log10 (std::max (sum / (double) (left.size() / 2), 1.0e-30));
}
} // namespace

TEST_CASE ("A muted band is out of the sum, and the rest stays", "[multicomp][w1]")
{
    MultiCompDsp::Targets targets;
    targets.numBands = 4;
    targets.bands[0].topHz = 120.0;
    targets.bands[1].topHz = 1200.0;
    targets.bands[2].topHz = 5000.0;

    for (auto& band : targets.bands)
        band.thresholdDb = 0.0;   // no compression: only the split is heard

    const auto open = rmsDbAfter (targets, 380.0);
    targets.bands[1].mute = true;
    const auto muted = rmsDbAfter (targets, 380.0);
    const auto elsewhere = rmsDbAfter (targets, 10000.0);

    targets.bands[1].mute = false;
    const auto elsewhereOpen = rmsDbAfter (targets, 10000.0);

    CHECK (muted < open - 30.0);   // the neighbours' 24 dB/oct skirts leak about -37 dB here
    CHECK (elsewhere == Catch::Approx (elsewhereOpen).margin (0.1));
}

TEST_CASE ("Learn splits pink energy evenly in log frequency, and follows the bass", "[multicomp][w1]")
{
    // Pink noise: equal energy in every third octave.
    std::array<double, 30> pink {};
    pink.fill (1.0);

    const auto even = MultiCompProcessor::crossoversFor (pink, 3);
    REQUIRE (even.size() == 2);

    // 30 third octaves from 25 Hz split in three: 10 each, so about 25 * 2^(10/3) and 25 * 2^(20/3).
    CHECK (even[0] == Catch::Approx (25.0 * std::pow (2.0, 10.0 / 3.0 - 1.0 / 6.0)).epsilon (0.1));
    CHECK (even[1] == Catch::Approx (25.0 * std::pow (2.0, 20.0 / 3.0 - 1.0 / 6.0)).epsilon (0.1));

    // Much more bass: the lowest crossover moves down.
    auto bassy = pink;

    for (int b = 0; b < 8; ++b)
        bassy[(size_t) b] = 8.0;

    const auto heavy = MultiCompProcessor::crossoversFor (bassy, 3);
    REQUIRE (heavy.size() == 2);
    CHECK (heavy[0] < even[0] * 0.7);
}
