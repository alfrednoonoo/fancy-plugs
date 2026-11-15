// Phase 3 Q2.6 for Fancy MultiComp: lookahead and channel per band (docs/multicomp.md,
// "Phase 3 Q2.6", section 6).

#include "LegacyMultiCompDsp.h"
#include "MultiCompDsp.h"
#include "MultiCompProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <numbers>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = std::numbers::pi;

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples, 0.0f), right ((size_t) numSamples, 0.0f) {}

    int size() const { return (int) left.size(); }

    template <typename Dsp>
    void process (Dsp& dsp, int blockSize = 256)
    {
        for (int start = 0; start < size(); start += blockSize)
        {
            float* main[] { left.data() + start, right.data() + start };
            const float* key[2] {};
            dsp.process (main, 2, key, 0, std::min (blockSize, size() - start));
        }
    }
};

// Tones, noise and a level swing, different in each channel.
Stereo programme (int numSamples, unsigned seed)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<double> noise (-1.0, 1.0);
    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto envelope = 0.03 + 0.9 * std::pow (0.5 + 0.5 * std::sin (2.0 * pi * 2.0 * i / rate), 3.0);
        const auto tones = std::sin (2.0 * pi * 80.0 * i / rate) + 0.5 * std::sin (2.0 * pi * 900.0 * i / rate)
                         + 0.3 * std::sin (2.0 * pi * 7000.0 * i / rate);

        signal.left[(size_t) i] = (float) (envelope * (0.5 * tones + 0.2 * noise (random)));
        signal.right[(size_t) i] = (float) (envelope * (0.35 * tones + 0.3 * noise (random)));
    }

    return signal;
}

template <typename Targets>
void setBands (Targets& targets)
{
    constexpr double thresholds[] { -24.0, -18.0, -28.0, -32.0 };
    constexpr double ratios[] { 3.0, 0.7, 4.0, 2.0 };

    for (size_t b = 0; b < 4; ++b)
    {
        targets.bands[b].thresholdDb = thresholds[b];
        targets.bands[b].ratio = ratios[b];
        targets.bands[b].attackMs = 5.0;
        targets.bands[b].releaseMs = 80.0;
        targets.bands[b].autoMakeup = b == 2;
    }

    targets.bands[0].topHz = 150.0;
    targets.bands[1].topHz = 1500.0;
    targets.bands[2].topHz = 6000.0;
    targets.bands[3].upward = true;
    targets.wetGain = 0.9;
    targets.dryGain = 0.2;
}

template <typename Dsp, typename Targets>
void prepare (Dsp& dsp, const Targets& targets)
{
    dsp.prepare (rate);
    dsp.setSynchronousDesign (true);
    dsp.snapToTargets (targets);
}

double blockDb (const std::vector<float>& x, int start, int count)
{
    auto sum = 0.0;

    for (int i = start; i < start + count; ++i)
        sum += (double) x[(size_t) i] * x[(size_t) i];

    return 10.0 * std::log10 (std::max (sum / count, 1.0e-30));
}

double toneDb (const std::vector<double>& x, int from, double hz)
{
    const auto count = (int) x.size() - from;
    std::complex<double> sum = 0.0;
    auto windowSum = 0.0;

    for (int i = 0; i < count; ++i)
    {
        const auto window = 0.5 - 0.5 * std::cos (2.0 * pi * i / count);
        sum += x[(size_t) (from + i)] * window * std::polar (1.0, -2.0 * pi * hz * i / rate);
        windowSum += window;
    }

    return 20.0 * std::log10 (std::max (2.0 * std::abs (sum) / windowSum, 1.0e-12));
}
} // namespace

// 1. Compatibility ------------------------------------------------------------------------

TEST_CASE ("with no lookahead and every band on Stereo, MultiComp renders exactly as before Q2.6", "[multicomp][q26]")
{
    const auto crossover = GENERATE (0, 1, 2);
    const auto limit = GENERATE (false, true);

    LegacyMultiCompDsp::Targets old;
    setBands (old);
    old.crossover = (multicomp::CrossoverType) crossover;
    old.limitOutput = limit;

    MultiCompDsp::Targets now;
    setBands (now);
    now.crossover = (multicomp::CrossoverType) crossover;
    now.limitOutput = limit;

    LegacyMultiCompDsp legacy;
    MultiCompDsp current;
    prepare (legacy, old);
    prepare (current, now);

    REQUIRE (current.getLatencySamples() == legacy.getLatencySamples());

    auto a = programme (48000, 3);
    auto b = a;
    a.process (legacy, 333);
    b.process (current, 333);

    INFO ("crossover " << crossover << ", limit " << limit);
    REQUIRE (a.left == b.left);
    REQUIRE (a.right == b.right);
}

TEST_CASE ("a MultiComp state saved before Q2.6 opens with no lookahead and every band on Stereo", "[multicomp][q26]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    MultiCompProcessor fresh;
    juce::MemoryBlock state;
    fresh.getStateInformation (state);

    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    REQUIRE (xml != nullptr);

    for (auto* child = xml->getFirstChildElement(); child != nullptr;)
    {
        auto* next = child->getNextElement();
        const auto id = child->getStringAttribute ("id");

        if (child->hasTagName ("PARAM") && (id == "lookahead" || id.endsWith ("chan")))
            xml->removeChildElement (child, true);

        child = next;
    }

    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary (*xml, oldState);

    MultiCompProcessor loaded;
    loaded.setStateInformation (oldState.getData(), (int) oldState.getSize());

    CHECK (loaded.getApvts().getRawParameterValue ("lookahead")->load() == 0.0f);
    CHECK (loaded.getApvts().getRawParameterValue ("b1chan")->load() == 0.0f);

    auto& presets = loaded.getPresets();
    presets.load (presets.getNames().indexOf ("Lookahead Glue"));
    CHECK (loaded.getApvts().getRawParameterValue ("lookahead")->load() == Catch::Approx (2.0f));
}

// 3. Lookahead ----------------------------------------------------------------------------

TEST_CASE ("lookahead is reported, keeps wet and dry aligned, and catches a step", "[multicomp][q26]")
{
    const auto crossover = GENERATE (0, 2);

    // Alignment: with no compression, wet and dry together are the render without lookahead,
    // 5 ms later. (An LR crossover's bands sum to an all-pass, not to the input itself.)
    {
        MultiCompDsp::Targets targets;
        targets.numBands = 3;
        targets.crossover = (multicomp::CrossoverType) crossover;
        targets.lookaheadMs = 5.0;
        targets.dryGain = 1.0;

        for (auto& band : targets.bands)
            band.ratio = 1.0;

        MultiCompDsp dsp, none;
        prepare (dsp, targets);

        auto withoutLookahead = targets;
        withoutLookahead.lookaheadMs = 0.0;
        prepare (none, withoutLookahead);

        const auto latency = dsp.getLatencySamples();
        CHECK (latency == none.getLatencySamples() + 240); // 5 ms at 48 kHz

        auto signal = programme (24000, 4);
        auto reference = signal;
        signal.process (dsp);
        reference.process (none);

        auto worst = 0.0;

        for (int i = 9600; i < signal.size(); ++i)
        {
            worst = std::max (worst, std::abs ((double) signal.left[(size_t) i] - (double) reference.left[(size_t) (i - 240)]));
            worst = std::max (worst, std::abs ((double) signal.right[(size_t) i] - (double) reference.right[(size_t) (i - 240)]));
        }

        INFO ("worst difference from the render without lookahead, 240 samples later: " << worst);
        CHECK (worst < 1.0e-4);
    }

    // A step: with 10 ms of lookahead a 2 ms attack is done before the step arrives, so the
    // onset is no louder than the steady state.
    const auto overshootDb = [crossover] (double lookaheadMs)
    {
        MultiCompDsp::Targets targets;
        targets.numBands = 1;
        targets.crossover = (multicomp::CrossoverType) crossover;
        targets.lookaheadMs = lookaheadMs;
        targets.bands[0].thresholdDb = -20.0;
        targets.bands[0].ratio = MultiCompDsp::infiniteRatio;
        targets.bands[0].kneeDb = 0.0;
        targets.bands[0].attackMs = 2.0;
        targets.bands[0].rmsMs = 0.0;
        targets.bands[0].autoRelease = false;

        MultiCompDsp dsp;
        prepare (dsp, targets);

        Stereo signal (24000);

        // A square wave, so the level is steady from the first sample and the attack can settle.
        for (int i = 9600; i < signal.size(); ++i)
            signal.left[(size_t) i] = signal.right[(size_t) i] = ((i / 24) % 2 == 0) ? 0.9f : -0.9f;

        signal.process (dsp);

        const auto arrival = 9600 + dsp.getLatencySamples();

        const auto peakOf = [&signal] (int start, int length)
        {
            auto peak = 0.0;

            for (int i = start; i < start + length; ++i)
                peak = std::max (peak, (double) std::abs (signal.left[(size_t) i]));

            return peak;
        };

        // dB of the first 10 ms over the peaks 20 to 40 ms later.
        return 20.0 * std::log10 (peakOf (arrival, 480) / peakOf (arrival + 960, 960));
    };

    const auto without = overshootDb (0.0);
    const auto with = overshootDb (10.0);

    INFO ("overshoot without lookahead " << without << " dB, with 10 ms " << with << " dB");
    CHECK (without > 6.0);
    CHECK (with < 1.0);
}

// 4. Channel ------------------------------------------------------------------------------

TEST_CASE ("a Mid band leaves the sides alone, and a Side band the middle", "[multicomp][q26]")
{
    const auto channel = GENERATE ((int) MultiCompDsp::mid, (int) MultiCompDsp::side);

    MultiCompDsp::Targets targets;
    targets.numBands = 1;
    targets.bands[0].thresholdDb = -60.0;
    targets.bands[0].ratio = MultiCompDsp::infiniteRatio;
    targets.bands[0].channel = channel;

    MultiCompDsp dsp;
    prepare (dsp, targets);

    // The middle: 300 Hz at 0.05; the sides: 500 Hz at 0.5.
    constexpr int length = 48000;
    Stereo signal (length);

    for (int i = 0; i < length; ++i)
    {
        const auto m = 0.05 * std::sin (2.0 * pi * 300.0 * i / rate);
        const auto s = 0.5 * std::sin (2.0 * pi * 500.0 * i / rate);
        signal.left[(size_t) i] = (float) (m + s);
        signal.right[(size_t) i] = (float) (m - s);
    }

    const auto measure = [] (const Stereo& x, bool wantMid, double hz)
    {
        std::vector<double> part ((size_t) x.size());

        for (size_t i = 0; i < part.size(); ++i)
            part[i] = wantMid ? 0.5 * (x.left[i] + x.right[i]) : 0.5 * (x.left[i] - x.right[i]);

        return toneDb (part, x.size() / 2, hz);
    };

    const auto midBefore = measure (signal, true, 300.0), sideBefore = measure (signal, false, 500.0);
    signal.process (dsp);
    const auto midAfter = measure (signal, true, 300.0), sideAfter = measure (signal, false, 500.0);

    INFO ("mid " << midBefore << " -> " << midAfter << " dB, side " << sideBefore << " -> " << sideAfter << " dB");

    if (channel == MultiCompDsp::mid)
    {
        CHECK (std::abs (sideAfter - sideBefore) < 0.01);
        CHECK (midAfter < midBefore - 6.0);
    }
    else
    {
        CHECK (std::abs (midAfter - midBefore) < 0.01);
        CHECK (sideAfter < sideBefore - 6.0);
    }
}

TEST_CASE ("one-channel bands and lookahead stay finite on a mono bus and with a sidechain", "[multicomp][q26]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    MultiCompProcessor processor;
    auto& apvts = processor.getApvts();

    for (const auto& [id, value] : { std::pair { "b1chan", 3.0f }, std::pair { "b2chan", 4.0f }, std::pair { "b3chan", 1.0f },
                                     std::pair { "b4chan", 2.0f }, std::pair { "lookahead", 7.5f }, std::pair { "b2sc", 1.0f } })
        apvts.getParameter (id)->setValueNotifyingHost (apvts.getParameter (id)->convertTo0to1 (value));

    processor.prepareToPlay (rate, 512);

    juce::AudioBuffer<float> buffer (4, 512);
    juce::MidiBuffer midi;
    juce::Random random (5);

    for (int block = 0; block < 40; ++block)
    {
        for (int ch = 0; ch < 4; ++ch)
            for (int i = 0; i < 512; ++i)
                buffer.setSample (ch, i, random.nextFloat() * 2.0f - 1.0f);

        processor.processBlock (buffer, midi);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                REQUIRE (std::isfinite (buffer.getSample (ch, i)));
    }
}

// 5. CPU (section 0.4 of the Phase 2.1 brief) -------------------------------------------------

TEST_CASE ("multicomp Q2.6 CPU measurement", "[.measure]")
{
    constexpr int blockSize = 128;
    constexpr int numBlocks = 7500; // 20 s
    constexpr int runs = 5;

    const auto timeOf = [] (const MultiCompDsp::Targets& targets)
    {
        std::vector<double> results;

        for (int run = 0; run < runs; ++run)
        {
            MultiCompDsp dsp;
            prepare (dsp, targets);

            // Each block is a fresh copy of the programme, so every setting sees the same input.
            const auto signal = programme (blockSize * 64, 77);
            std::vector<float> left ((size_t) blockSize), right ((size_t) blockSize);
            const auto start = std::chrono::steady_clock::now();

            for (int i = 0; i < numBlocks; ++i)
            {
                const auto offset = (size_t) ((i % 64) * blockSize);
                std::copy_n (signal.left.begin() + (std::ptrdiff_t) offset, blockSize, left.begin());
                std::copy_n (signal.right.begin() + (std::ptrdiff_t) offset, blockSize, right.begin());
                float* channels[] { left.data(), right.data() };
                const float* key[2] {};
                dsp.process (channels, 2, key, 0, blockSize);
            }

            const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
            results.push_back (100.0 * elapsed.count() / ((double) (numBlocks * blockSize) / rate));
        }

        std::sort (results.begin(), results.end());
        return results[runs / 2];
    };

    MultiCompDsp::Targets plain;
    setBands (plain);
    plain.bands[3].upward = false;

    auto everything = plain;
    everything.bands[3].upward = true;
    everything.bands[0].channel = MultiCompDsp::mid;
    everything.bands[2].channel = MultiCompDsp::side;
    everything.lookaheadMs = 5.0;

    const auto plainPercent = timeOf (plain);
    const auto everythingPercent = timeOf (everything);

    std::printf ("multicomp q26 cpu, 4 bands LR 24, 48 kHz, 128 samples, median of %d: plain %.3f %%, with upward, Mid/Side bands and 5 ms lookahead %.3f %%\n",
                 runs, plainPercent, everythingPercent);
}
