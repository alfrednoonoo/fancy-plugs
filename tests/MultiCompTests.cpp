#include "BandGraph.h"
#include "MultiCompDsp.h"
#include "MultiCompEditor.h"
#include "MultiCompProcessor.h"

#include <fancy/Decibels.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <random>
#include <vector>

using Catch::Approx;
using multicomp::CrossoverType;

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = std::numbers::pi;

// Unity through every band: 1:1 and no gain.
MultiCompDsp::Targets transparent (CrossoverType type, int numBands = 4)
{
    MultiCompDsp::Targets targets;

    targets.crossover = type;
    targets.numBands = numBands;

    constexpr double tops[] { 120.0, 1200.0, 6000.0, 8000.0, 10000.0, 12000.0, 15000.0, 20000.0 };

    for (size_t band = 0; band < targets.bands.size(); ++band)
    {
        targets.bands[band].ratio = 1.0;
        targets.bands[band].topHz = tops[band];
    }

    return targets;
}

void prepare (MultiCompDsp& dsp, const MultiCompDsp::Targets& targets, double sampleRate = rate)
{
    dsp.prepare (sampleRate);
    dsp.setSynchronousDesign (true);
    dsp.snapToTargets (targets);
}

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples), right ((size_t) numSamples) {}

    int size() const { return (int) left.size(); }

    void process (MultiCompDsp& dsp, const MultiCompDsp::Targets& targets, int blockSize = 64, Stereo* sidechain = nullptr)
    {
        for (int position = 0; position < size(); position += blockSize)
        {
            const auto count = std::min (blockSize, size() - position);
            float* channels[] { left.data() + position, right.data() + position };

            dsp.setTargets (targets);

            if (sidechain != nullptr)
            {
                const float* key[] { sidechain->left.data() + position, sidechain->right.data() + position };
                dsp.process (channels, 2, key, 2, count);
            }
            else
            {
                dsp.process (channels, 2, nullptr, 0, count);
            }
        }
    }
};

Stereo sine (int numSamples, double hz, double level)
{
    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) (level * std::sin (2.0 * pi * hz * i / rate));

    return signal;
}

double rmsDb (const std::vector<float>& samples, int from, int to)
{
    auto sum = 0.0;

    for (int i = from; i < to; ++i)
        sum += (double) samples[(size_t) i] * (double) samples[(size_t) i];

    return fancy::gainToDb (std::sqrt (sum / (double) (to - from)), -300.0);
}

double peakOf (const std::vector<float>& samples, int from = 0)
{
    auto peak = 0.0;

    for (int i = from; i < (int) samples.size(); ++i)
        peak = std::max (peak, (double) std::abs (samples[(size_t) i]));

    return peak;
}

double largestStep (const std::vector<float>& samples, int from, int to)
{
    auto largest = 0.0;

    for (int i = from + 1; i < to; ++i)
        largest = std::max (largest, std::abs ((double) samples[(size_t) i] - (double) samples[(size_t) i - 1]));

    return largest;
}

// One second of a sine through the targets, and its level change in dB.
double gainAt (const MultiCompDsp::Targets& targets, double hz, double level = 0.25)
{
    MultiCompDsp dsp;
    prepare (dsp, targets);

    constexpr int numSamples = 48000;

    auto signal = sine (numSamples, hz, level);
    signal.process (dsp, targets);

    const auto latency = dsp.getLatencySamples();

    return rmsDb (signal.left, 24000 + latency / 2, numSamples) - fancy::gainToDb (level / std::sqrt (2.0));
}

void setParameter (MultiCompProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getApvts().getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

std::vector<float> render (MultiCompProcessor& processor, int numSamples)
{
    juce::AudioBuffer<float> buffer (4, numSamples);
    juce::MidiBuffer midi;

    buffer.clear();

    for (int i = 0; i < numSamples; ++i)
    {
        buffer.setSample (0, i, 0.25f * (float) std::sin (2.0 * pi * 220.0 * i / rate));
        buffer.setSample (1, i, 0.25f * (float) std::sin (2.0 * pi * 3310.0 * i / rate));
    }

    processor.processBlock (buffer, midi);

    std::vector<float> samples (buffer.getReadPointer (0), buffer.getReadPointer (0) + numSamples);
    samples.insert (samples.end(), buffer.getReadPointer (1), buffer.getReadPointer (1) + numSamples);
    return samples;
}
} // namespace

// Crossovers --------------------------------------------------------------------------------

TEST_CASE ("crossovers are put in order and kept 1/3 octave apart", "[multicomp]")
{
    double hz[] { 6000.0, 120.0, 1200.0 };

    multicomp::sanitiseCrossovers (hz, 3);

    CHECK (hz[0] == 120.0);
    CHECK (hz[1] == 1200.0);
    CHECK (hz[2] == 6000.0);

    double tight[] { 1000.0, 1001.0, 1002.0 };

    multicomp::sanitiseCrossovers (tight, 3);

    CHECK (tight[1] / tight[0] >= multicomp::minCrossoverRatio * 0.9999);
    CHECK (tight[2] / tight[1] >= multicomp::minCrossoverRatio * 0.9999);

    double high[] { 20000.0, 20000.0, 20000.0, 20000.0, 20000.0, 20000.0, 20000.0 };

    multicomp::sanitiseCrossovers (high, 7);

    CHECK (high[6] <= multicomp::highestHz);
    CHECK (high[0] >= multicomp::lowestHz);

    for (int i = 1; i < 7; ++i)
        CHECK (high[i] / high[i - 1] >= multicomp::minCrossoverRatio * 0.9999);
}

TEST_CASE ("bands at 1:1 and 0 dB sum flat in every crossover mode", "[multicomp]")
{
    const auto type = GENERATE (CrossoverType::lr24, CrossoverType::lr48, CrossoverType::linear);
    const auto numBands = GENERATE (1, 2, 4, 8);

    const auto targets = transparent (type, numBands);

    for (const auto hz : { 20.0, 31.5, 63.0, 120.0, 125.0, 250.0, 500.0, 1200.0, 2000.0, 6000.0, 8000.0, 12000.0, 16000.0, 20000.0 })
    {
        INFO ("type " << (int) type << ", " << numBands << " bands, " << hz << " Hz");
        CHECK (std::abs (gainAt (targets, hz)) < 0.1);
    }
}

TEST_CASE ("the bands really do split the signal", "[multicomp]")
{
    const auto type = GENERATE (CrossoverType::lr24, CrossoverType::lr48, CrossoverType::linear);

    auto targets = transparent (type);

    // Only band 2 of 120 Hz to 1.2 kHz is left in the mix.
    targets.bands[1].solo = true;

    INFO ("type " << (int) type);
    CHECK (gainAt (targets, 400.0) == Approx (0.0).margin (0.5));
    CHECK (gainAt (targets, 10000.0) < -40.0);
    CHECK (gainAt (targets, 30.0) < -30.0);

    // Linkwitz-Riley crossovers are -6 dB at the crossover, and so is a sinc's cut.
    CHECK (gainAt (targets, 1200.0) == Approx (-6.0).margin (1.0));
}

TEST_CASE ("linear mode nulls against the delayed input", "[multicomp]")
{
    const auto numBands = GENERATE (2, 4, 8);

    const auto targets = transparent (CrossoverType::linear, numBands);

    MultiCompDsp dsp;
    prepare (dsp, targets);

    const auto latency = dsp.getLatencySamples();

    REQUIRE (latency == 2559);

    constexpr int numSamples = 48000;

    std::mt19937 random (5);
    std::uniform_real_distribution<float> noise (-0.5f, 0.5f);

    Stereo signal (numSamples);

    for (auto& sample : signal.left)
        sample = noise (random);

    signal.right = signal.left;

    const auto input = signal.left;

    signal.process (dsp, targets);

    auto errorEnergy = 0.0, inputEnergy = 0.0;

    for (int i = latency + 4096; i < numSamples; ++i)
    {
        const auto error = (double) signal.left[(size_t) i] - (double) input[(size_t) (i - latency)];

        errorEnergy += error * error;
        inputEnergy += (double) input[(size_t) (i - latency)] * (double) input[(size_t) (i - latency)];
    }

    const auto nullDb = 10.0 * std::log10 (errorEnergy / inputEnergy);

    INFO (numBands << " bands null at " << nullDb << " dB");
    CHECK (nullDb < -90.0);
}

TEST_CASE ("reported latency is the measured delay", "[multicomp]")
{
    const auto type = GENERATE (CrossoverType::lr24, CrossoverType::linear);
    const auto limit = GENERATE (false, true);
    const auto sampleRate = GENERATE (44100.0, 48000.0, 96000.0);

    auto targets = transparent (type);
    targets.limitOutput = limit;

    MultiCompDsp dsp;
    prepare (dsp, targets, sampleRate);

    const auto latency = dsp.getLatencySamples();

    Stereo signal (latency + 2000);
    signal.left[0] = signal.right[0] = 0.5f;
    signal.process (dsp, targets);

    if (type == CrossoverType::linear)
    {
        auto peakAt = 0;

        for (int i = 0; i < signal.size(); ++i)
            if (std::abs (signal.left[(size_t) i]) > std::abs (signal.left[(size_t) peakAt]))
                peakAt = i;

        CHECK (peakAt == latency);
    }
    else
    {
        // A minimum-phase all-pass smears the impulse, but it can't start before the latency.
        for (int i = 0; i < latency; ++i)
            REQUIRE (std::abs (signal.left[(size_t) i]) < 1.0e-6f);

        CHECK (std::abs (signal.left[(size_t) latency]) > 0.01f);
    }
}

TEST_CASE ("moving a crossover doesn't click", "[multicomp]")
{
    const auto type = GENERATE (CrossoverType::lr24, CrossoverType::lr48);

    constexpr int numSamples = 48000, sweepStart = 12000, sweepSamples = 9600;
    constexpr double hz = 440.0;

    // The band filters' own slopes change a sine slightly as a crossover passes through it.
    constexpr double margin = 1.25;

    auto targets = transparent (type, 2);

    MultiCompDsp dsp;
    targets.bands[0].topHz = 100.0;
    prepare (dsp, targets);

    auto signal = sine (numSamples, hz, 0.5);
    const auto inputStep = largestStep (signal.left, 0, numSamples);

    for (int position = 0; position < numSamples; position += 64)
    {
        const auto count = std::min (64, numSamples - position);
        float* channels[] { signal.left.data() + position, signal.right.data() + position };

        const auto progress = juce::jlimit (0.0, 1.0, (double) (position - sweepStart) / (double) sweepSamples);

        targets.bands[0].topHz = 100.0 * std::pow (20.0, progress);

        dsp.setTargets (targets);
        dsp.process (channels, 2, nullptr, 0, count);
    }

    const auto largest = largestStep (signal.left, 2048, numSamples);

    INFO ("input step " << inputStep << ", output step " << largest);
    CHECK (largest < inputStep * margin);
}

TEST_CASE ("a linear crossover can move while audio plays", "[multicomp]")
{
    constexpr int numSamples = 48000;

    auto targets = transparent (CrossoverType::linear, 2);

    MultiCompDsp dsp;
    prepare (dsp, targets);

    auto signal = sine (numSamples, 440.0, 0.5);
    const auto inputStep = largestStep (signal.left, 0, numSamples);

    for (int position = 0; position < numSamples; position += 128)
    {
        const auto count = std::min (128, numSamples - position);
        float* channels[] { signal.left.data() + position, signal.right.data() + position };

        targets.bands[0].topHz = 100.0 * std::pow (20.0, (double) position / (double) numSamples);

        dsp.setTargets (targets);
        dsp.process (channels, 2, nullptr, 0, count);
    }

    CHECK (largestStep (signal.left, dsp.getLatencySamples() + 1024, numSamples) < inputStep * 1.1);
}

// Compression -------------------------------------------------------------------------------

TEST_CASE ("a band compresses by its ratio above its threshold", "[multicomp]")
{
    auto targets = transparent (CrossoverType::lr24);

    auto& band = targets.bands[1];
    band.thresholdDb = -30.0;
    band.ratio = 4.0;
    band.kneeDb = 0.0;
    band.attackMs = 1.0;
    band.releaseMs = 50.0;
    band.rmsMs = 0.0;
    band.autoRelease = false;

    // -12 dBFS peak, with the detector at peak level: 18 dB over, so 13.5 dB down.
    CHECK (gainAt (targets, 400.0, 0.25) == Approx (-13.5).margin (2.5));

    // Other bands are untouched.
    CHECK (std::abs (gainAt (targets, 8000.0, 0.25)) < 0.2);
}

TEST_CASE ("a ratio below 1:1 raises the level above the threshold", "[multicomp]")
{
    auto targets = transparent (CrossoverType::lr24);

    auto& band = targets.bands[1];
    band.thresholdDb = -30.0;
    band.ratio = 0.5;
    band.kneeDb = 0.0;
    band.rmsMs = 0.0;

    CHECK (gainAt (targets, 400.0, 0.25) > 6.0);
    CHECK (gainAt (targets, 400.0, 0.25) <= MultiCompDsp::maxExpansionDb + 0.5);

    // Below the threshold nothing happens.
    CHECK (std::abs (gainAt (targets, 400.0, 0.005)) < 0.2);
}

TEST_CASE ("expansion stops at its limit", "[multicomp]")
{
    auto targets = transparent (CrossoverType::lr24, 1);

    auto& band = targets.bands[0];
    band.thresholdDb = -60.0;
    band.ratio = 0.5;
    band.kneeDb = 0.0;
    band.rmsMs = 0.0;
    band.attackMs = 0.0;

    CHECK (gainAt (targets, 1000.0, 0.5) == Approx (MultiCompDsp::maxExpansionDb).margin (0.5));
}

TEST_CASE ("auto make-up comes only from the settings", "[multicomp]")
{
    MultiCompDsp::BandSpec band;
    band.thresholdDb = -24.0;
    band.ratio = 4.0;
    band.kneeDb = 0.0;

    CHECK (MultiCompDsp::autoMakeupDb (band) == Approx (0.5 * 0.75 * 24.0));

    band.ratio = 0.5;
    CHECK (MultiCompDsp::autoMakeupDb (band) == 0.0);

    band.ratio = 1.0;
    CHECK (MultiCompDsp::autoMakeupDb (band) == 0.0);

    // A signal under the threshold still gets exactly that gain, so it can't pump.
    auto targets = transparent (CrossoverType::lr24, 1);

    targets.bands[0].thresholdDb = -24.0;
    targets.bands[0].ratio = 4.0;
    targets.bands[0].kneeDb = 0.0;
    targets.bands[0].autoMakeup = true;

    const auto expected = MultiCompDsp::autoMakeupDb (targets.bands[0]);

    CHECK (gainAt (targets, 1000.0, 0.002) == Approx (expected).margin (0.05));
    CHECK (gainAt (targets, 1000.0, 0.01) == Approx (expected).margin (0.05));
}

TEST_CASE ("attack and release follow Fancy Comp's convention", "[multicomp]")
{
    constexpr double attackMs = 50.0;

    auto targets = transparent (CrossoverType::lr24, 1);

    auto& band = targets.bands[0];
    band.thresholdDb = -60.0;
    band.ratio = 100.0;
    band.kneeDb = 0.0;
    band.rmsMs = 0.0;
    band.attackMs = attackMs;
    band.autoRelease = false;

    MultiCompDsp dsp;
    prepare (dsp, targets);

    auto signal = sine (48000, 1000.0, 0.5);
    signal.process (dsp, targets);

    const auto window = [&] (double ms) { return (int) (ms * 0.001 * rate); };

    const auto start = rmsDb (signal.left, 0, window (2.0));
    const auto settled = rmsDb (signal.left, window (900.0), window (1000.0));
    const auto atTimeConstant = rmsDb (signal.left, window (attackMs - 1.0), window (attackMs + 1.0));

    // One time constant covers about 63 % of the change, in dB.
    CHECK ((start - atTimeConstant) / (start - settled) == Approx (0.632).margin (0.08));
}

TEST_CASE ("active off bypasses a band and keeps it in the mix", "[multicomp]")
{
    auto targets = transparent (CrossoverType::lr24);

    auto& band = targets.bands[1];
    band.thresholdDb = -40.0;
    band.ratio = 10.0;
    band.kneeDb = 0.0;
    band.gainDb = 6.0;
    band.active = false;

    CHECK (std::abs (gainAt (targets, 400.0)) < 0.1);
}

TEST_CASE ("band gain applies to its band", "[multicomp]")
{
    auto targets = transparent (CrossoverType::lr24);

    targets.bands[1].gainDb = -6.0;

    // The neighbouring bands, which stay at 0 dB, still reach 400 Hz at about -39 dB, and that
    // adds 0.17 dB to a band turned down by 6.
    CHECK (gainAt (targets, 400.0) == Approx (-6.0).margin (0.25));
    CHECK (std::abs (gainAt (targets, 12000.0)) < 0.1);
}

TEST_CASE ("Wet and Dry are independent and Dry stays in phase", "[multicomp]")
{
    auto targets = transparent (CrossoverType::lr24);

    targets.wetGain = 0.5;
    targets.dryGain = 0.5;

    // Half of each adds back to unity at any frequency, including at a crossover.
    for (const auto hz : { 120.0, 400.0, 1200.0, 6000.0 })
        CHECK (std::abs (gainAt (targets, hz)) < 0.1);

    targets.wetGain = 0.0;
    targets.dryGain = 1.0;

    // Dry ignores the bands' settings.
    targets.bands[1].gainDb = -24.0;

    CHECK (std::abs (gainAt (targets, 400.0)) < 0.1);
}

// Sidechain ---------------------------------------------------------------------------------

TEST_CASE ("a kick on the sidechain ducks only the bands with Sidechain on", "[multicomp]")
{
    constexpr int numSamples = 48000;

    const auto type = GENERATE (CrossoverType::lr24, CrossoverType::linear);

    auto targets = transparent (type);

    for (auto& band : targets.bands)
    {
        band.thresholdDb = -30.0;
        band.ratio = 10.0;
        band.kneeDb = 0.0;
        band.rmsMs = 1.0;
        band.attackMs = 1.0;
    }

    targets.bands[0].sidechain = true;
    targets.bands[3].sidechain = true;

    // A loud low tone on the sidechain, which only reaches band 1's detector.
    auto kick = sine (numSamples, 60.0, 0.5);

    const auto levelWith = [&] (double hz)
    {
        MultiCompDsp dsp;
        prepare (dsp, targets);

        auto signal = sine (numSamples, hz, 0.02);
        signal.process (dsp, targets, 64, &kick);

        return rmsDb (signal.left, 24000 + dsp.getLatencySamples() / 2, numSamples) - fancy::gainToDb (0.02 / std::sqrt (2.0));
    };

    INFO ("type " << (int) type);

    // Band 1 has Sidechain on and the kick is in its range.
    CHECK (levelWith (50.0) < -6.0);

    // Bands 2 and 3 listen to the main input, which is under their thresholds.
    CHECK (std::abs (levelWith (400.0)) < 0.3);
    CHECK (std::abs (levelWith (3000.0)) < 0.3);

    // Band 4 has it on, but the kick isn't in its range.
    CHECK (std::abs (levelWith (10000.0)) < 0.3);
}

TEST_CASE ("a Sidechain detector makes every band listen to the sidechain", "[multicomp]")
{
    constexpr int numSamples = 48000;

    auto targets = transparent (CrossoverType::lr24, 1);

    targets.bands[0].thresholdDb = -30.0;
    targets.bands[0].ratio = 10.0;
    targets.bands[0].kneeDb = 0.0;
    targets.detector = MultiCompDsp::sideLR;

    auto key = sine (numSamples, 1000.0, 0.5);

    MultiCompDsp dsp;
    prepare (dsp, targets);

    auto signal = sine (numSamples, 1000.0, 0.02);
    signal.process (dsp, targets, 64, &key);

    CHECK (rmsDb (signal.left, 24000, numSamples) < fancy::gainToDb (0.02 / std::sqrt (2.0)) - 6.0);
}

TEST_CASE ("listening to a missing sidechain is silence, not a crash", "[multicomp]")
{
    auto targets = transparent (CrossoverType::lr24);

    // Hard enough to show if the band were hearing anything at all.
    targets.bands[1].thresholdDb = -40.0;
    targets.bands[1].ratio = 10.0;
    targets.bands[1].kneeDb = 0.0;

    SECTION ("From the header")
    {
        targets.detector = MultiCompDsp::sideLR;
        CHECK (std::abs (gainAt (targets, 400.0)) < 0.1);
    }

    SECTION ("From the band")
    {
        targets.bands[1].sidechain = true;
        CHECK (std::abs (gainAt (targets, 400.0)) < 0.1);
    }

    SECTION ("And the same band does compress the main input")
    {
        CHECK (gainAt (targets, 400.0) < -6.0);
    }
}

// Limiter -----------------------------------------------------------------------------------

TEST_CASE ("Limit output holds the ceiling, as a multiband limiter", "[multicomp]")
{
    const auto type = GENERATE (CrossoverType::lr24, CrossoverType::linear);

    auto targets = transparent (type);

    for (auto& band : targets.bands)
    {
        band.thresholdDb = -6.0;
        band.ratio = MultiCompDsp::infiniteRatio;
        band.kneeDb = 0.0;
        band.attackMs = 0.0;
        band.releaseMs = 50.0;
        band.rmsMs = 0.0;
        band.gainDb = 12.0;
    }

    targets.limitOutput = true;

    MultiCompDsp dsp;
    prepare (dsp, targets);

    constexpr int numSamples = 96000;

    std::mt19937 random (6);
    std::uniform_real_distribution<float> noise (-2.0f, 2.0f);

    Stereo signal (numSamples);

    for (auto& sample : signal.left)
        sample = noise (random);

    signal.right = signal.left;
    signal.process (dsp, targets);

    CHECK (peakOf (signal.left) <= 1.0);
    CHECK (peakOf (signal.left) > 0.5);
}

// Host behaviour ----------------------------------------------------------------------------

TEST_CASE ("every sample rate and block size stays flat", "[multicomp]")
{
    const auto sampleRate = GENERATE (44100.0, 96000.0, 192000.0, 384000.0);
    const auto blockSize = GENERATE (1, 64, 8192);
    const auto type = GENERATE (CrossoverType::lr24, CrossoverType::linear);

    const auto targets = transparent (type);

    MultiCompDsp dsp;
    prepare (dsp, targets, sampleRate);

    const auto numSamples = dsp.getLatencySamples() + 20000;
    const auto hz = 1000.0;

    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) (0.25 * std::sin (2.0 * pi * hz * i / sampleRate));

    signal.process (dsp, targets, blockSize);

    INFO ("rate " << sampleRate << ", block " << blockSize);
    CHECK (rmsDb (signal.left, numSamples - 12000, numSamples) == Approx (fancy::gainToDb (0.25 / std::sqrt (2.0))).margin (0.1));
}

TEST_CASE ("variable block sizes change nothing", "[multicomp]")
{
    const auto type = GENERATE (CrossoverType::lr48, CrossoverType::linear);

    auto targets = transparent (type);

    targets.bands[1].thresholdDb = -30.0;
    targets.bands[1].ratio = 4.0;

    const auto renderIn = [&] (const std::vector<int>& blocks)
    {
        MultiCompDsp dsp;
        prepare (dsp, targets);

        auto signal = sine (20000, 400.0, 0.25);
        auto position = 0;

        for (size_t i = 0; position < signal.size(); ++i)
        {
            const auto count = std::min (blocks[i % blocks.size()], signal.size() - position);
            float* channels[] { signal.left.data() + position, signal.right.data() + position };

            dsp.setTargets (targets);
            dsp.process (channels, 2, nullptr, 0, count);
            position += count;
        }

        return signal.left;
    };

    CHECK (renderIn ({ 512 }) == renderIn ({ 1, 7, 64, 1000, 3, 8192, 300 }));
}

TEST_CASE ("mono works", "[multicomp]")
{
    const auto targets = transparent (CrossoverType::lr24);

    MultiCompDsp dsp;
    prepare (dsp, targets);

    auto signal = sine (48000, 400.0, 0.25);
    float* channels[] { signal.left.data() };

    dsp.setTargets (targets);
    dsp.process (channels, 1, nullptr, 0, 48000);

    CHECK (rmsDb (signal.left, 24000, 48000) == Approx (fancy::gainToDb (0.25 / std::sqrt (2.0))).margin (0.1));
}

TEST_CASE ("changing the band count or crossover fades rather than clicks", "[multicomp]")
{
    constexpr int numSamples = 48000;

    auto targets = transparent (CrossoverType::lr24, 4);

    MultiCompDsp dsp;
    prepare (dsp, targets);

    auto signal = sine (numSamples, 440.0, 0.5);
    const auto inputStep = largestStep (signal.left, 0, numSamples);

    for (int position = 0; position < numSamples; position += 64)
    {
        const auto count = std::min (64, numSamples - position);
        float* channels[] { signal.left.data() + position, signal.right.data() + position };

        targets.numBands = position >= 16000 ? 6 : 4;
        targets.crossover = position >= 32000 ? CrossoverType::lr48 : CrossoverType::lr24;

        dsp.setTargets (targets);
        dsp.process (channels, 2, nullptr, 0, count);
    }

    // A 2 ms fade over a sine adds a little to its slope.
    CHECK (largestStep (signal.left, 1024, numSamples) < inputStep * 2.0);
    CHECK (rmsDb (signal.left, 44000, numSamples) == Approx (fancy::gainToDb (0.5 / std::sqrt (2.0))).margin (0.2));
}

TEST_CASE ("bypass fades to the delayed input", "[multicomp]")
{
    constexpr int numSamples = 48000;

    const auto type = GENERATE (CrossoverType::lr24, CrossoverType::linear);

    auto targets = transparent (type);

    MultiCompDsp dsp;
    prepare (dsp, targets);

    const auto latency = dsp.getLatencySamples();

    auto signal = sine (numSamples, 440.0, 0.5);
    const auto input = signal.left;

    for (int position = 0; position < numSamples; position += 64)
    {
        const auto count = std::min (64, numSamples - position);
        float* channels[] { signal.left.data() + position, signal.right.data() + position };

        targets.bypass = position >= 12000;

        dsp.setTargets (targets);
        dsp.process (channels, 2, nullptr, 0, count);
    }

    for (int i = 24000; i < numSamples; ++i)
        REQUIRE (signal.left[(size_t) i] == Approx (input[(size_t) (i - latency)]).margin (1.0e-5));
}

TEST_CASE ("random settings stay finite", "[multicomp]")
{
    std::mt19937 random (7);
    std::uniform_real_distribution<double> unit (0.0, 1.0);

    const auto chance = [&] { return unit (random) < 0.5; };

    MultiCompDsp dsp;
    dsp.prepare (rate);
    dsp.setSynchronousDesign (true);

    MultiCompDsp::Targets targets;
    dsp.snapToTargets (targets);

    float left[1024], right[1024], keyLeft[1024], keyRight[1024];
    float* channels[] { left, right };
    const float* key[] { keyLeft, keyRight };

    for (int round = 0; round < 300; ++round)
    {
        for (auto& band : targets.bands)
        {
            band.topHz = 20.0 * std::pow (1000.0, unit (random));
            band.thresholdDb = -60.0 * unit (random);
            band.ratio = 0.5 * std::pow (200.0, unit (random));
            band.kneeDb = 24.0 * unit (random);
            band.attackMs = 500.0 * unit (random) * unit (random);
            band.releaseMs = 5000.0 * unit (random) * unit (random);
            band.rmsMs = 250.0 * unit (random) * unit (random);
            band.gainDb = 48.0 * unit (random) - 24.0;
            band.active = chance();
            band.solo = unit (random) < 0.1;
            band.autoMakeup = chance();
            band.autoRelease = chance();
            band.feedback = chance();
            band.sidechain = chance();
        }

        targets.numBands = 1 + (int) (unit (random) * 8.0);
        targets.detector = (int) (unit (random) * 6.0);
        targets.crossover = (CrossoverType) (int) (unit (random) * 3.0);
        targets.limitOutput = chance();
        targets.attack90 = chance();

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

TEST_CASE ("multicomp automation with an unchanged value is bit-identical", "[multicomp]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    MultiCompProcessor plain, automated;

    for (auto* processor : { &plain, &automated })
    {
        setParameter (*processor, "b2thr", -30.0f);
        setParameter (*processor, "b2ratio", 3.3f);
        setParameter (*processor, "b1top", 234.5f);
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

TEST_CASE ("multicomp state survives a save and load", "[multicomp]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    MultiCompProcessor source, restored;

    setParameter (source, "b2thr", -20.0f);
    setParameter (source, "b3ratio", 8.0f);
    setParameter (source, "b1top", 200.0f);
    setParameter (source, "bands", 5.0f);
    setParameter (source, "crossover", 1.0f);
    setParameter (source, "wet", -3.0f);

    juce::MemoryBlock state;
    source.getStateInformation (state);
    restored.setStateInformation (state.getData(), (int) state.getSize());

    CHECK (restored.readBand (1).thresholdDb == Approx (-20.0));
    CHECK (restored.readBand (2).ratio == Approx (8.0).margin (0.01));
    CHECK (restored.readBand (0).topHz == Approx (200.0));
    CHECK (restored.getNumBands() == 5);

    source.setNonRealtime (true);
    restored.setNonRealtime (true);
    source.prepareToPlay (rate, 4800);
    restored.prepareToPlay (rate, 4800);

    CHECK (render (source, 4800) == render (restored, 4800));
}

TEST_CASE ("damaged multicomp state is ignored", "[multicomp]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    MultiCompProcessor processor;
    const char junk[] { 1, 2, 3, 4, 5, 6, 7, 8 };

    processor.setStateInformation (junk, (int) sizeof (junk));

    CHECK (processor.getNumBands() == 4);
}

TEST_CASE ("every multicomp preset loads and plays", "[multicomp]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    MultiCompProcessor processor;
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
}

TEST_CASE ("splitting a band keeps its settings and doesn't move its neighbours", "[multicomp]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    MultiCompProcessor processor;

    setParameter (processor, "b2thr", -20.0f);
    setParameter (processor, "b3thr", -30.0f);

    const auto added = processor.splitBand (1, 500.0);

    CHECK (added == 2);
    CHECK (processor.getNumBands() == 5);
    CHECK (processor.bandTopHz (0) == Approx (120.0));
    CHECK (processor.bandTopHz (1) == Approx (500.0));
    CHECK (processor.bandTopHz (2) == Approx (1200.0));
    CHECK (processor.bandTopHz (3) == Approx (6000.0));
    CHECK (processor.readBand (1).thresholdDb == Approx (-20.0));
    CHECK (processor.readBand (2).thresholdDb == Approx (-20.0));
    CHECK (processor.readBand (3).thresholdDb == Approx (-30.0));
}

TEST_CASE ("a band can't be split past 8 or into less than 1/3 octave", "[multicomp]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    MultiCompProcessor processor;

    // 120 Hz to 150 Hz is too narrow for two 1/3 octave gaps.
    processor.setCrossover (0, 120.0);
    setParameter (processor, "b2top", 150.0f);

    CHECK (processor.splitBand (1, 130.0) == -1);

    while (processor.getNumBands() < multicomp::maxBands)
    {
        auto widest = 0;

        for (int band = 1; band < processor.getNumBands(); ++band)
            if (processor.bandTopHz (band) / processor.bandBottomHz (band) > processor.bandTopHz (widest) / processor.bandBottomHz (widest))
                widest = band;

        REQUIRE (processor.splitBand (widest, std::sqrt (processor.bandBottomHz (widest) * processor.bandTopHz (widest))) >= 0);
    }

    CHECK (processor.splitBand (0, 60.0) == -1);
}

TEST_CASE ("removing a band gives its range to the band above", "[multicomp]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    MultiCompProcessor processor;

    setParameter (processor, "b3thr", -30.0f);
    setParameter (processor, "b4thr", -40.0f);

    processor.removeBand (1);

    CHECK (processor.getNumBands() == 3);
    CHECK (processor.bandTopHz (0) == Approx (120.0));
    CHECK (processor.bandTopHz (1) == Approx (6000.0));
    CHECK (processor.readBand (1).thresholdDb == Approx (-30.0));
    CHECK (processor.readBand (2).thresholdDb == Approx (-40.0));

    for (int i = 0; i < 5; ++i)
        processor.removeBand (0);

    CHECK (processor.getNumBands() == 1);
}

TEST_CASE ("a crossover drag stops at its neighbours", "[multicomp]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    MultiCompProcessor processor;

    processor.setCrossover (1, 100000.0);

    CHECK (processor.bandTopHz (1) == Approx (6000.0 / multicomp::minCrossoverRatio).margin (1.0));

    processor.setCrossover (1, 1.0);

    CHECK (processor.bandTopHz (1) == Approx (120.0 * multicomp::minCrossoverRatio).margin (1.0));
}

TEST_CASE ("Link bands moves the others by the same distance", "[multicomp]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    MultiCompProcessor processor;

    setParameter (processor, "b1thr", -10.0f);
    setParameter (processor, "b2thr", -20.0f);
    setParameter (processor, "b3thr", -30.0f);
    setParameter (processor, "b4thr", -40.0f);

    const auto gesture = [&] (float db) { processor.setParameter ("b2thr", db); };

    SECTION ("Off, only one band moves")
    {
        gesture (-26.0f);

        CHECK (processor.readBand (0).thresholdDb == Approx (-10.0));
        CHECK (processor.readBand (1).thresholdDb == Approx (-26.0));
    }

    SECTION ("On, all move together, and stop at a limit")
    {
        setParameter (processor, "link", 1.0f);

        gesture (-26.0f);

        CHECK (processor.readBand (0).thresholdDb == Approx (-16.0).margin (0.11));
        CHECK (processor.readBand (1).thresholdDb == Approx (-26.0));
        CHECK (processor.readBand (2).thresholdDb == Approx (-36.0).margin (0.11));
        CHECK (processor.readBand (3).thresholdDb == Approx (-46.0).margin (0.11));

        gesture (-46.0f);

        CHECK (processor.readBand (3).thresholdDb == -60.0);
    }

    SECTION ("Alt edits one band")
    {
        setParameter (processor, "link", 1.0f);

        juce::ModifierKeys::currentModifiers = juce::ModifierKeys (juce::ModifierKeys::altModifier);

        gesture (-26.0f);

        juce::ModifierKeys::currentModifiers = {};

        CHECK (processor.readBand (0).thresholdDb == Approx (-10.0));
        CHECK (processor.readBand (1).thresholdDb == Approx (-26.0));
    }

    SECTION ("Automation and presets aren't repeated across bands")
    {
        setParameter (processor, "link", 1.0f);

        // No gesture, as when a host plays automation.
        setParameter (processor, "b2thr", -26.0f);

        CHECK (processor.readBand (0).thresholdDb == Approx (-10.0));
    }

    SECTION ("It covers Ratio, Attack, Release and Gain")
    {
        setParameter (processor, "link", 1.0f);

        for (const auto* suffix : { "ratio", "att", "rel", "gain" })
        {
            auto& first = *processor.getApvts().getParameter (MultiCompProcessor::id (0, suffix));
            auto& second = *processor.getApvts().getParameter (MultiCompProcessor::id (1, suffix));

            const auto before = first.getValue();

            second.beginChangeGesture();
            second.setValueNotifyingHost (second.getValue() + 0.05f);
            second.endChangeGesture();

            INFO (suffix);
            CHECK (first.getValue() == Approx (before + 0.05f).margin (0.01));
        }
    }

    SECTION ("It leaves unused bands alone")
    {
        setParameter (processor, "link", 1.0f);

        gesture (-26.0f);

        CHECK (processor.readBand (5).thresholdDb == 0.0);
    }
}

TEST_CASE ("multicomp values can be typed", "[multicomp]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    MultiCompProcessor processor;

    const auto typed = [&] (const char* id, const char* text)
    {
        const juce::RangedAudioParameter& parameter = *processor.getApvts().getParameter (id);
        return parameter.convertFrom0to1 (parameter.getValueForText (text));
    };

    CHECK (typed ("b1thr", "-18") == Approx (-18.0f));
    CHECK (typed ("b1thr", "-18.5 dB") == Approx (-18.5f));
    CHECK (typed ("b1ratio", "3:1") == Approx (3.0f).margin (0.01));
    CHECK (typed ("b1ratio", "4") == Approx (4.0f).margin (0.01));
    CHECK (typed ("b1ratio", "0.5:1") == Approx (0.5f).margin (0.01));
    CHECK (typed ("b1ratio", "1:2") == Approx (0.5f).margin (0.01));
    CHECK (typed ("b1ratio", "inf") == (float) MultiCompDsp::infiniteRatio);
    CHECK (typed ("b1knee", "6") == Approx (6.0f));
    CHECK (typed ("b1att", "10 ms") == Approx (10.0f));
    CHECK (typed ("b1rel", "1,5") == Approx (1.5f));
    CHECK (typed ("b1gain", "+2") == Approx (2.0f));
    CHECK (typed ("b1gain", "-3.5 dB") == Approx (-3.5f));
    CHECK (typed ("b1top", "2.5k") == Approx (2500.0f).margin (0.1f));
    CHECK (typed ("wet", "-inf") == -60.0f);
}

TEST_CASE ("the graph's axes match the mockup's", "[multicomp]")
{
    constexpr float width = 610.0f, height = 190.0f;

    CHECK (BandGraph::xForHz (20.0, width) == Approx (34.0f));
    CHECK (BandGraph::xForHz (20000.0, width) == Approx (600.0f));
    CHECK (BandGraph::yForDb (0.0f, height) == Approx (12.0f));
    CHECK (BandGraph::yForDb (-60.0f, height) == Approx (168.0f));
    CHECK (BandGraph::hzForX (BandGraph::xForHz (1234.0, width), width) == Approx (1234.0).margin (0.1));
    CHECK (BandGraph::dbForY (BandGraph::yForDb (-24.0f, height), height) == Approx (-24.0f));
}

TEST_CASE ("the multicomp editor opens", "[multicomp]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    MultiCompProcessor processor;
    processor.prepareToPlay (rate, 512);

    const std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());

    REQUIRE (editor != nullptr);
    CHECK (editor->getWidth() == 760);

    processor.editorBeingDeleted (editor.get());
}

// Measurements ------------------------------------------------------------------------------

TEST_CASE ("multicomp CPU measurement", "[.measure]")
{
    constexpr int blockSize = 128, blocks = 4000;

    const auto measure = [] (CrossoverType type, int numBands, bool limiter)
    {
        auto targets = transparent (type, numBands);

        for (auto& band : targets.bands)
        {
            band.thresholdDb = -30.0;
            band.ratio = 3.0;
        }

        targets.limitOutput = limiter;

        MultiCompDsp dsp;
        prepare (dsp, targets);

        float left[blockSize], right[blockSize];
        float* channels[] { left, right };

        std::mt19937 random (8);
        std::uniform_real_distribution<float> noise (-0.25f, 0.25f);

        std::chrono::duration<double> spent {};

        for (int block = 0; block < blocks; ++block)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                left[i] = noise (random);
                right[i] = noise (random);
            }

            const auto start = std::chrono::steady_clock::now();

            dsp.setTargets (targets);
            dsp.process (channels, 2, nullptr, 0, blockSize);

            spent += std::chrono::steady_clock::now() - start;
        }

        return 100.0 * spent.count() / ((double) (blocks * blockSize) / rate);
    };

    std::printf ("multicomp cpu: LR 24 4 bands %.2f %%, with limiter %.2f %%, 8 bands %.2f %%\n",
                 measure (CrossoverType::lr24, 4, false), measure (CrossoverType::lr24, 4, true), measure (CrossoverType::lr24, 8, false));
    std::printf ("multicomp cpu: LR 48 4 bands %.2f %%, 8 bands %.2f %%\n",
                 measure (CrossoverType::lr48, 4, false), measure (CrossoverType::lr48, 8, false));
    std::printf ("multicomp cpu: Linear 4 bands %.2f %%, 8 bands %.2f %%\n",
                 measure (CrossoverType::linear, 4, false), measure (CrossoverType::linear, 8, false));

    SUCCEED();
}

TEST_CASE ("multicomp flatness measurement", "[.measure]")
{
    for (const auto type : { CrossoverType::lr24, CrossoverType::lr48, CrossoverType::linear })
    {
        const auto targets = transparent (type, 8);

        auto worst = 0.0;

        for (auto hz = 20.0; hz <= 20000.0; hz *= 1.26)
            worst = std::max (worst, std::abs (gainAt (targets, hz)));

        std::printf ("multicomp flatness: type %d, 8 bands, worst %.4f dB\n", (int) type, worst);
    }

    SUCCEED();
}
