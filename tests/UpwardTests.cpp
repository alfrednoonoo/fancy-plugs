// Phase 2.1 item U: upward compression, Depth and Time in Fancy MultiComp. Design:
// docs/phase2_1-U.md.

#include "MultiCompDsp.h"
#include "MultiCompProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <fancy/Decibels.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = 3.14159265358979323846;

// One band, full range, nothing downward: only the upward part changes the gain.
MultiCompDsp::Targets oneBand()
{
    MultiCompDsp::Targets targets;
    targets.numBands = 1;

    auto& band = targets.bands[0];
    band.ratio = 1.0;
    band.rmsMs = 50.0;
    band.attackMs = 10.0;
    band.releaseMs = 100.0;
    band.autoRelease = false;
    return targets;
}

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples), right ((size_t) numSamples) {}
    int size() const { return (int) left.size(); }
};

Stereo sine (int numSamples, double hz, double rmsDb)
{
    Stereo signal (numSamples);
    const auto amplitude = std::sqrt (2.0) * std::pow (10.0, rmsDb / 20.0);

    for (int i = 0; i < numSamples; ++i)
        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * hz * i / rate));

    return signal;
}

Stereo render (const MultiCompDsp::Targets& targets, Stereo signal, int blockSize = 64)
{
    MultiCompDsp dsp;
    dsp.prepare (rate);
    dsp.setSynchronousDesign (true);
    dsp.snapToTargets (targets);

    for (int position = 0; position < signal.size(); position += blockSize)
    {
        const auto count = std::min (blockSize, signal.size() - position);
        float* channels[] { signal.left.data() + position, signal.right.data() + position };
        dsp.setTargets (targets);
        dsp.process (channels, 2, nullptr, 0, count);
    }

    return signal;
}

double rmsDb (const std::vector<float>& samples, int from, int to)
{
    auto sum = 0.0;

    for (int i = from; i < to; ++i)
        sum += (double) samples[(size_t) i] * (double) samples[(size_t) i];

    return fancy::gainToDb (std::sqrt (sum / std::max (1, to - from)), -300.0);
}

// The brief's formula, written out here rather than taken from the DSP.
double expectedLift (double x, double upThreshold, double ratio, double maxLift, double floor)
{
    if (x >= upThreshold || x < floor)
        return 0.0;

    const auto lift = std::min (maxLift, (upThreshold - x) * (1.0 - 1.0 / ratio));
    return lift * std::clamp ((x - floor) / 8.0, 0.0, 1.0);
}

// The change one band makes to a steady sine at this RMS level, once settled.
double changeFor (const MultiCompDsp::Targets& targets, double levelDb)
{
    const auto input = sine ((int) rate, 1000.0, levelDb);
    const auto output = render (targets, input);
    const auto from = (int) (rate * 0.75);
    return rmsDb (output.left, from, output.size()) - rmsDb (input.left, from, input.size());
}
} // namespace

TEST_CASE ("Upward: the static curve follows the formula, with Max lift and the Floor fade", "[multicomp][upward]")
{
    auto targets = oneBand();
    auto& band = targets.bands[0];
    band.upward = true;
    band.upThresholdDb = -30.0;
    band.upRatio = 3.0;
    band.maxLiftDb = 15.0;
    band.floorDb = -70.0;

    for (int level = -90; level <= 0; level += 2)
    {
        const auto expected = expectedLift (level, -30.0, 3.0, 15.0, -70.0);
        const auto measured = changeFor (targets, level);

        INFO (level << " dB: expected " << expected << ", measured " << measured);
        CHECK (std::abs (measured - expected) < 0.3);
    }
}

TEST_CASE ("Upward: an Up threshold above the Threshold acts as the Threshold", "[multicomp][upward]")
{
    auto targets = oneBand();
    auto& band = targets.bands[0];
    band.thresholdDb = -30.0;
    band.upward = true;
    band.upThresholdDb = -10.0;
    band.upRatio = 2.0;

    // At -40 dB: (-30 - -40) x (1 - 1/2) = 5 dB, not 15.
    CHECK (std::abs (changeFor (targets, -40.0) - 5.0) < 0.3);
}

TEST_CASE ("Upward: Depth 0 % gives the band sum, whatever the bands do", "[multicomp][upward]")
{
    MultiCompDsp::Targets squashed;
    squashed.numBands = 4;

    for (auto& band : squashed.bands)
    {
        band.thresholdDb = -30.0;
        band.ratio = 8.0;
        band.upward = true;
        band.upThresholdDb = -50.0;
        band.upRatio = 4.0;
        band.maxLiftDb = 20.0;
    }

    squashed.depth = 0.0;

    MultiCompDsp::Targets plain;
    plain.numBands = 4;

    for (auto& band : plain.bands)
        band.ratio = 1.0;

    std::mt19937 random (4);
    std::uniform_real_distribution<float> noise (-0.5f, 0.5f);
    Stereo input ((int) rate);

    for (int i = 0; i < input.size(); ++i)
        input.left[(size_t) i] = input.right[(size_t) i] = noise (random) * (float) (0.05 + 0.95 * (0.5 + 0.5 * std::sin (2.0 * pi * 3.0 * i / rate)));

    const auto a = render (squashed, input);
    const auto b = render (plain, input);

    for (int start = 0; start + 4800 <= a.size(); start += 4800)
        CHECK (std::abs (rmsDb (a.left, start, start + 4800) - rmsDb (b.left, start, start + 4800)) < 0.01);
}

TEST_CASE ("Upward: Time 200 % doubles the attack and release times", "[multicomp][upward]")
{
    // A level step through a downward band with peak detection: from -40 dB to -10 dB and back.
    const auto timesFor = [] (double time)
    {
        auto targets = oneBand();
        auto& band = targets.bands[0];
        band.thresholdDb = -30.0;
        band.ratio = 4.0;
        band.kneeDb = 0.0;
        band.rmsMs = 0.0;
        band.attackMs = 20.0;
        band.releaseMs = 200.0;
        targets.time = time;

        // Long enough after each step for a 400 ms release to settle.
        Stereo input ((int) (rate * 5.0));

        for (int i = 0; i < input.size(); ++i)
            input.left[(size_t) i] = input.right[(size_t) i] = (i >= (int) rate && i < (int) (rate * 2.0)) ? 0.316f : 0.01f;

        const auto output = render (targets, input);

        std::vector<double> changeDb ((size_t) input.size());

        for (int i = 0; i < input.size(); ++i)
            changeDb[(size_t) i] = fancy::gainToDb (std::abs ((double) output.left[(size_t) i] / (double) input.left[(size_t) i]), -300.0);

        const auto timeTo63 = [&] (int from, int to)
        {
            const auto start = changeDb[(size_t) from - 1], end = changeDb[(size_t) to - 1];
            const auto mark = start + 0.632 * (end - start);

            for (int i = from; i < to; ++i)
                if ((end < start) ? changeDb[(size_t) i] <= mark : changeDb[(size_t) i] >= mark)
                    return 1000.0 * (i - from) / rate;

            return -1.0;
        };

        return std::make_pair (timeTo63 ((int) rate, (int) (rate * 2.0)), timeTo63 ((int) (rate * 2.0), input.size()));
    };

    const auto [attack1, release1] = timesFor (1.0);
    const auto [attack2, release2] = timesFor (2.0);

    INFO ("attack " << attack1 << " ms -> " << attack2 << " ms, release " << release1 << " ms -> " << release2 << " ms");
    REQUIRE (attack1 > 0.0);
    REQUIRE (release1 > 0.0);
    CHECK (std::abs (attack2 / attack1 - 2.0) < 0.2);
    CHECK (std::abs (release2 / release1 - 2.0) < 0.2);
}

TEST_CASE ("Upward: the Floor keeps silence and noise at -90 dB where they are", "[multicomp][upward]")
{
    auto targets = oneBand();
    targets.bands[0].upward = true;
    targets.bands[0].upRatio = 10.0;
    targets.bands[0].maxLiftDb = 30.0;

    // Silence stays silent.
    const auto silence = render (targets, Stereo ((int) rate));
    CHECK (std::all_of (silence.left.begin(), silence.left.end(), [] (float x) { return x == 0.0f; }));

    // Noise at -90 dB isn't lifted.
    std::mt19937 random (6);
    std::normal_distribution<float> noise (0.0f, (float) std::pow (10.0, -90.0 / 20.0));
    Stereo input ((int) rate);

    for (int i = 0; i < input.size(); ++i)
        input.left[(size_t) i] = input.right[(size_t) i] = noise (random);

    const auto output = render (targets, input);
    CHECK (rmsDb (output.left, (int) (rate / 2), output.size()) - rmsDb (input.left, (int) (rate / 2), input.size()) < 0.01);
}

TEST_CASE ("Upward: Big squash on pink noise and a drum loop stays finite and never over 0 dBFS with Limit output", "[multicomp][upward]")
{
    MultiCompProcessor processor;
    const auto names = processor.getPresets().getFactoryNames();
    const auto index = names.indexOf ("Big squash");
    REQUIRE (index >= 0);

    processor.getPresets().load (index);
    processor.getApvts().getParameter ("limit")->setValueNotifyingHost (1.0f);

    CHECK (processor.getNumBands() == 3);
    CHECK (std::abs (processor.readBand (0).topHz - 88.0) < 0.5);
    CHECK (std::abs (processor.readBand (1).topHz - 2500.0) < 1.0);
    CHECK (processor.readBand (2).upward);

    processor.setRateAndBufferSizeDetails (rate, 512);
    processor.prepareToPlay (rate, 512);

    const auto run = [&processor] (const std::function<float (int)>& source)
    {
        auto finite = true;
        auto peak = 0.0f;
        auto atCeiling = 0;
        juce::AudioBuffer<float> block (juce::jmax (2, processor.getTotalNumInputChannels()), 512);
        juce::MidiBuffer midi;

        for (int start = 0; start < (int) (rate * 6.0); start += 512)
        {
            block.clear();

            for (int i = 0; i < 512; ++i)
                block.setSample (0, i, source (start + i)), block.setSample (1, i, block.getSample (0, i));

            processor.processBlock (block, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                {
                    finite = finite && std::isfinite (block.getSample (ch, i));
                    peak = std::max (peak, std::abs (block.getSample (ch, i)));
                    atCeiling += std::abs (block.getSample (ch, i)) >= 1.0f ? 1 : 0;
                }
        }

        std::printf ("big squash: peak %.4f, %d samples at the 0 dBFS ceiling
", peak, atCeiling);
        return std::make_pair (finite, peak);
    };

    // Pink noise (Paul Kellet), loud.
    std::mt19937 random (9);
    std::uniform_real_distribution<float> white (-1.0f, 1.0f);
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;

    const auto [pinkFinite, pinkPeak] = run ([&] (int)
    {
        const auto w = white (random);
        b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f; b2 = 0.96900f * b2 + w * 0.1538520f;
        b3 = 0.86650f * b3 + w * 0.3104856f; b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
        const auto out = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f) * 0.25f;
        b6 = w * 0.115926f;
        return out;
    });

    // A drum loop: a kick (decaying 55 Hz) on every beat and noise snares on 2 and 4, at 120 bpm.
    const auto [drumFinite, drumPeak] = run ([&] (int i)
    {
        const auto beat = (int) (rate / 2.0);
        const auto t = (double) (i % beat) / rate;
        const auto kick = 0.9 * std::exp (-t * 18.0) * std::sin (2.0 * pi * 55.0 * t);
        const auto snare = ((i / beat) % 2 == 1) ? 0.6 * std::exp (-t * 30.0) * white (random) : 0.0;
        return (float) (kick + snare);
    });

    CHECK (pinkFinite);
    CHECK (drumFinite);
    // The shared limiter's ceiling is exactly 0 dBFS, with a final clamp there: never over it.
    CHECK (pinkPeak <= 1.0f);
    CHECK (drumPeak <= 1.0f);

    processor.releaseResources();
}

TEST_CASE ("Upward: older states load with Upward off, Depth and Time at 100 %, and render bit-identically", "[multicomp][upward][compat]")
{
    MultiCompProcessor original;
    original.setParameter ("b2thr", -24.0f);
    original.setParameter ("b2ratio", 4.0f);

    juce::MemoryBlock saved;
    original.getStateInformation (saved);

    auto tree = juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize()));

    for (int i = tree.getNumChildren(); --i >= 0;)
    {
        const auto id = tree.getChild (i).getProperty ("id").toString();
        const auto isBand = id.startsWithChar ('b') && juce::CharacterFunctions::isDigit (id[1]);

        if (id == "depth" || id == "time"
            || (isBand && (id.endsWith ("up") || id.endsWith ("uthr") || id.endsWith ("uratio") || id.endsWith ("lift") || id.endsWith ("floor"))))
            tree.removeChild (i, nullptr);
    }

    tree.setProperty ("stateVersion", 2, nullptr);
    juce::MemoryBlock older;
    juce::AudioProcessor::copyXmlToBinary (*tree.createXml(), older);

    MultiCompProcessor edited;
    edited.getApvts().getParameter ("depth")->setValueNotifyingHost (0.3f);
    edited.getApvts().getParameter ("b2up")->setValueNotifyingHost (1.0f);
    edited.setStateInformation (older.getData(), (int) older.getSize());

    const auto targets = edited.readTargets();
    CHECK (targets.depth == 1.0);
    CHECK (targets.time == 1.0);

    for (int b = 0; b < multicomp::maxBands; ++b)
        CHECK_FALSE (targets.bands[(size_t) b].upward);

    MultiCompProcessor current;
    current.setStateInformation (saved.getData(), (int) saved.getSize());

    const auto renderProcessor = [] (MultiCompProcessor& processor)
    {
        processor.setRateAndBufferSizeDetails (rate, 512);
        processor.prepareToPlay (rate, 512);

        juce::AudioBuffer<float> all (4, (int) rate);
        all.clear();
        std::mt19937 noise (3);
        std::uniform_real_distribution<float> sample (-0.5f, 0.5f);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < all.getNumSamples(); ++i)
                all.setSample (ch, i, sample (noise));

        juce::MidiBuffer midi;

        for (int start = 0; start < all.getNumSamples(); start += 512)
        {
            juce::AudioBuffer<float> block (all.getArrayOfWritePointers(), processor.getTotalNumInputChannels(), start, 512);
            processor.processBlock (block, midi);
        }

        processor.releaseResources();
        return all;
    };

    const auto expected = renderProcessor (current);
    const auto actual = renderProcessor (edited);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < expected.getNumSamples(); ++i)
            if (expected.getSample (ch, i) != actual.getSample (ch, i))
                FAIL ("sample " << i << " on channel " << ch << " differs");
}

// CPU, by the method in §0.4 of the Phase 2.1 brief: 4 bands, Upward off and on.
TEST_CASE ("Upward CPU measurement", "[.measure]")
{
    const auto run = [] (bool upward)
    {
        MultiCompDsp::Targets targets;

        for (auto& band : targets.bands)
        {
            band.thresholdDb = -24.0;
            band.ratio = 4.0;
            band.upward = upward;
        }

        std::vector<double> runs;

        for (int attempt = 0; attempt < 5; ++attempt)
        {
            MultiCompDsp dsp;
            dsp.prepare (rate);
            dsp.snapToTargets (targets);

            std::mt19937 random (8);
            std::uniform_real_distribution<float> noise (-0.5f, 0.5f);
            float left[128], right[128];
            float* channels[] { left, right };
            std::chrono::duration<double> spent {};

            for (int done = 0; done < (int) (10.0 * rate); done += 128)
            {
                for (int i = 0; i < 128; ++i)
                {
                    left[i] = noise (random);
                    right[i] = noise (random);
                }

                const auto start = std::chrono::steady_clock::now();
                dsp.process (channels, 2, nullptr, 0, 128);
                spent += std::chrono::steady_clock::now() - start;
            }

            runs.push_back (100.0 * spent.count() / 10.0);
        }

        std::sort (runs.begin(), runs.end());
        return runs[2];
    };

    const auto off = run (false);
    const auto on = run (true);

    std::printf ("upward cpu (median of 5, 48 kHz, 128 samples, stereo, 4 bands LR 24): Upward off %.3f %%, Upward on %.3f %%\n", off, on);
}
