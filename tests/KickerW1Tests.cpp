// Phase 4 Wave 1 for Fancy Kicker: Sub length, Make-room and polarity (docs/kicker.md,
// "Phase 4 Wave 1", section 8).

#include "KickerDsp.h"
#include "KickerProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = 3.14159265358979323846;

// One kick with a long tail: an 80 Hz body decaying over a second.
std::vector<float> longKick (int numSamples)
{
    std::vector<float> signal ((size_t) numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal[(size_t) i] = (float) (0.6 * std::exp (-(double) i / (1.0 * rate)) * std::sin (2.0 * pi * 80.0 * i / rate));

    return signal;
}

// Sub mode with the source silent (Dry -inf), so the output is the generated sub alone.
KickerDsp::Targets subOnly()
{
    KickerDsp::Targets targets;
    targets.sub = true;
    targets.dryGain = 0.0;
    targets.subGain = 1.0;
    return targets;
}

std::vector<float> run (const KickerDsp::Targets& targets, std::vector<float> signal)
{
    KickerDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    auto right = signal;

    for (size_t start = 0; start < signal.size(); start += 256)
    {
        float* channels[] { signal.data() + start, right.data() + start };
        dsp.process (channels, 2, (int) std::min<size_t> (256, signal.size() - start));
    }

    return signal;
}

double peakDb (const std::vector<float>& samples, int from, int to)
{
    auto peak = 0.0;

    for (int i = from; i < to; ++i)
        peak = std::max (peak, std::abs ((double) samples[(size_t) i]));

    return 20.0 * std::log10 (std::max (peak, 1.0e-12));
}
} // namespace

TEST_CASE ("Sub length ends the sub before the kick's tail does", "[kicker][w1]")
{
    const auto input = longKick ((int) rate);

    auto targets = subOnly();
    const auto full = run (targets, input);

    targets.subLengthMs = 150.0;
    const auto shortened = run (targets, input);

    // 150 ms + the 20 ms fade, then 130 ms for the sub output's 15 Hz DC filter to ring out after
    // the fade (it rings at about -56 dB 30 ms after it, CI run 44949179446).
    const auto after = (int) (0.3 * rate);

    INFO ("full sub after 200 ms: " << peakDb (full, after, after + 4800) << " dB; with Sub length: " << peakDb (shortened, after, after + 4800) << " dB");
    CHECK (peakDb (full, after, after + 4800) > -40.0);
    CHECK (peakDb (shortened, after, (int) rate) < peakDb (full, after, after + 4800) - 60.0);
    CHECK (peakDb (shortened, 2400, 6000) == Catch::Approx (peakDb (full, 2400, 6000)).margin (0.01));
}

TEST_CASE ("Invert sub flips the generated sub exactly", "[kicker][w1]")
{
    const auto input = longKick ((int) (0.5 * rate));

    auto targets = subOnly();
    const auto normal = run (targets, input);

    targets.subPolarity = true;
    const auto flipped = run (targets, input);

    auto worst = 0.0;

    for (size_t i = 0; i < input.size(); ++i)
        worst = std::max (worst, std::abs ((double) normal[i] + (double) flipped[i]));

    CHECK (20.0 * std::log10 (std::max (worst, 1.0e-12)) < -90.0);
}

TEST_CASE ("Make-room cuts the source kick's lowest end in Sub mode", "[kicker][w1]")
{
    std::vector<float> lowTone ((size_t) rate);

    for (size_t i = 0; i < lowTone.size(); ++i)
        lowTone[i] = (float) (0.2 * std::sin (2.0 * pi * 30.0 * (double) i / rate));

    KickerDsp::Targets targets;
    targets.sub = true;
    targets.subGain = 0.0;   // the source alone
    targets.roomHz = 60.0;

    const auto plain = run (targets, lowTone);
    targets.roomCut = true;
    const auto cut = run (targets, lowTone);

    const auto half = (int) (rate / 2);
    CHECK (peakDb (cut, half, (int) rate) < peakDb (plain, half, (int) rate) - 9.0);
}
