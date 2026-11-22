// Fancy Chorus (docs/chorus.md, "Tests"). The model's figures are from docs/research/chorus-model.py
// and docs/research/chorus-prepush.py. Automation, state, bypass, the offline render, pop-ups,
// presets and Listen run from the suite's shared tests (test 9).

#include "ChorusDsp.h"
#include "ChorusProcessor.h"

#if FANCY_HAS_COMP
 #include "CompProcessor.h"
#endif

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <random>
#include <vector>

namespace
{
constexpr double pi = std::numbers::pi;
constexpr double rate = 48000.0;

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples, 0.0f), right ((size_t) numSamples, 0.0f) {}
    int size() const { return (int) left.size(); }
};

// Wet only, nothing on the wet path but the voices.
ChorusDsp::Targets plainWet()
{
    ChorusDsp::Targets t;
    t.mode = ChorusDsp::classic;
    t.voices = 1;
    t.age = 0.0;
    t.lowCutOn = t.highCutOn = false;
    t.bassMono = false;
    t.wetGain = 1.0;
    t.dryGain = 0.0;
    return t;
}

// Processes in blocks; `change` may alter the targets before each block.
template <typename Change>
Stereo run (ChorusDsp::Targets targets, Stereo signal, int blockSize, Change&& change)
{
    ChorusDsp dsp;
    dsp.setTargets (targets);
    dsp.prepare (rate);

    for (int start = 0; start < signal.size(); start += blockSize)
    {
        const auto count = std::min (blockSize, signal.size() - start);
        change (targets, start);
        dsp.setTargets (targets);
        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, count);
    }

    return signal;
}

Stereo run (const ChorusDsp::Targets& targets, Stereo signal, int blockSize = 333)
{
    return run (targets, std::move (signal), blockSize, [] (ChorusDsp::Targets&, int) {});
}

Stereo noise (int numSamples, float level, unsigned seed = 3)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<float> uniform (-level, level);
    Stereo s (numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        s.left[(size_t) i] = uniform (random);
        s.right[(size_t) i] = uniform (random);
    }

    return s;
}

Stereo sine (int numSamples, double hz, double level)
{
    Stereo s (numSamples);

    for (int i = 0; i < numSamples; ++i)
        s.left[(size_t) i] = s.right[(size_t) i] = (float) (level * std::sin (2.0 * pi * hz * (double) i / rate));

    return s;
}

double peakOf (const std::vector<float>& x, int from = 0, int to = -1)
{
    to = to < 0 ? (int) x.size() : to;
    auto peak = 0.0;

    for (int i = from; i < to; ++i)
        peak = std::max (peak, (double) std::abs (x[(size_t) i]));

    return peak;
}

double rmsOf (const std::vector<double>& x, int from, int to)
{
    auto sum = 0.0;

    for (int i = from; i < to; ++i)
        sum += x[(size_t) i] * x[(size_t) i];

    return std::sqrt (sum / std::max (1, to - from));
}

double toDb (double gain)
{
    return 20.0 * std::log10 (std::max (gain, 1.0e-12));
}

// The frequency of each cycle, from upward zero crossings (interpolated).
std::vector<double> cycleFrequencies (const std::vector<float>& x, int from)
{
    std::vector<double> crossings, frequencies;

    for (int i = from + 1; i < (int) x.size(); ++i)
    {
        const auto a = (double) x[(size_t) i - 1], b = (double) x[(size_t) i];

        if (a < 0.0 && b >= 0.0)
            crossings.push_back ((double) (i - 1) + a / (a - b));
    }

    for (size_t i = 1; i < crossings.size(); ++i)
        frequencies.push_back (rate / (crossings[i] - crossings[i - 1]));

    return frequencies;
}

double cents (double hz, double reference)
{
    return 1200.0 * std::log2 (hz / reference);
}
} // namespace

TEST_CASE ("Chorus test 1: one still voice is the input delayed by Delay", "[chorus]")
{
    auto t = plainWet();
    t.depth = 0.0;
    t.feedback = 0.0;

    const auto in = noise ((int) rate, 0.5f);
    const auto out = run (t, in);
    const auto delaySamples = (int) std::lround (t.delayMs * 0.001 * rate);
    auto worst = 0.0;

    for (int i = delaySamples; i < in.size(); ++i)
    {
        worst = std::max (worst, std::abs ((double) out.left[(size_t) i] - (double) in.left[(size_t) i - delaySamples]));
        worst = std::max (worst, std::abs ((double) out.right[(size_t) i] - (double) in.right[(size_t) i - delaySamples]));
    }

    INFO ("worst difference " << toDb (worst / 0.5) << " dB");
    CHECK (toDb (worst / 0.5) < -100.0);
}

TEST_CASE ("Chorus test 2: Rate 0.8 Hz and Depth 45 % detune by about 20 cents", "[chorus]")
{
    auto t = plainWet();
    t.rateHz = 0.8;
    t.depth = 0.45;

    const auto out = run (t, sine ((int) (3.0 * rate), 1000.0, 0.5));
    const auto frequencies = cycleFrequencies (out.left, (int) (0.1 * rate));
    REQUIRE (frequencies.size() > 2000);

    const auto [low, high] = std::minmax_element (frequencies.begin(), frequencies.end());
    const auto down = cents (*low, 1000.0), up = cents (*high, 1000.0);

    INFO ("swing " << down << " to " << up << " cents");
    CHECK (up > 17.0);
    CHECK (up < 23.0);
    CHECK (down < -17.0);
    CHECK (down > -23.0);
}

TEST_CASE ("Chorus test 3: Width 0 is mono; at Width 100 two voices sit at the edges", "[chorus]")
{
    SECTION ("Width 0")
    {
        auto t = plainWet();
        t.voices = 3;
        t.width = 0.0;

        const auto out = run (t, noise ((int) rate, 0.5f));

        auto mono = true;

        for (int i = 0; i < out.size(); ++i)
            mono = mono && out.left[(size_t) i] == out.right[(size_t) i];

        CHECK (mono);
    }

    SECTION ("Width 100, two voices: the left is the first voice alone, the right the second")
    {
        auto t = plainWet();
        t.monoInput = true;
        t.lfoPhaseDeg = 0.0;
        t.spread = 0.5;
        t.width = 1.0;

        const auto in = noise ((int) rate, 0.5f);

        t.voices = 1;
        const auto first = run (t, in);
        t.voices = 2;
        const auto both = run (t, in);

        auto worst = 0.0, apart = 0.0;

        for (int i = 0; i < in.size(); ++i)
        {
            worst = std::max (worst, std::abs ((double) both.left[(size_t) i] - (double) first.left[(size_t) i]));
            apart = std::max (apart, std::abs ((double) both.right[(size_t) i] - (double) first.left[(size_t) i]));
        }

        CHECK (toDb (worst / 0.5) < -100.0);
        CHECK (toDb (apart / 0.5) > -40.0);
    }
}

TEST_CASE ("Chorus test 4: Feedback +90 % is stable and DC can't build up", "[chorus]")
{
    for (const auto mode : { (int) ChorusDsp::classic, (int) ChorusDsp::flange })
    {
        auto t = plainWet();
        t.mode = mode;
        t.voices = 3;
        t.feedback = 0.9;

        const auto length = (int) (60.0 * rate);
        auto in = noise (length, 0.25f);

        // Half a second of DC at the end: the feedback path's blocker keeps it from building up.
        for (int i = length - (int) (0.5 * rate); i < length; ++i)
            in.left[(size_t) i] = in.right[(size_t) i] = 0.25f;

        const auto out = run (t, in, 512);

        auto finite = true;

        for (int i = 0; i < length; ++i)
            finite = finite && std::isfinite (out.left[(size_t) i]) && std::isfinite (out.right[(size_t) i]);

        CHECK (finite);

        const auto growth = toDb (std::max (peakOf (out.left), peakOf (out.right)) / 0.25);
        INFO ("mode " << mode << ": peak " << growth << " dB over the input's");
        CHECK (growth < 25.0);

        // Without the blocker the DC would grow by 1 / (1 - 0.9), to 2.5 per voice.
        auto mean = 0.0;
        const auto from = length - (int) (0.1 * rate);

        for (int i = from; i < length; ++i)
            mean += 0.5 * ((double) out.left[(size_t) i] + (double) out.right[(size_t) i]);

        mean /= (double) (length - from);
        INFO ("mode " << mode << ": DC at the end " << mean);
        CHECK (std::abs (mean) < 0.6);
    }
}

TEST_CASE ("Chorus test 5: Tempo sync, 1/4 at 120 BPM is 2 Hz and the LFO follows the host", "[chorus]")
{
    // 1/4 is one quarter note per LFO cycle.
    CHECK (ChorusProcessor::noteBeats (12) == 1.0);
    CHECK (ChorusProcessor::noteName (12) == "1/4");
    CHECK (ChorusProcessor::noteBeats (ChorusProcessor::defaultNote) == 4.0);

    auto t = plainWet();
    t.sync = true;
    t.syncBeats = 1.0;
    t.bpm = 120.0;
    t.depth = 0.45;

    SECTION ("free-running at the tempo: 2 Hz")
    {
        const auto out = run (t, sine ((int) (2.1 * rate), 1000.0, 0.5));
        const auto frequencies = cycleFrequencies (out.left, (int) (0.1 * rate));

        // The detune crosses zero twice a cycle: 2 Hz over 2 s is 8 times.
        auto crossings = 0;

        for (size_t i = 1; i < frequencies.size(); ++i)
            if ((frequencies[i - 1] - 1000.0) * (frequencies[i] - 1000.0) < 0.0)
                ++crossings;

        CHECK (crossings >= 7);
        CHECK (crossings <= 9);
    }

    SECTION ("playing: one quarter note later in the song is the same phase, half of one isn't")
    {
        const auto in = noise ((int) rate, 0.5f);
        constexpr int blockSize = 480;
        const auto beatsPerBlock = 2.0 * blockSize / rate;   // 120 BPM

        const auto from = [&] (double startPpq)
        {
            auto targets = t;
            targets.playing = true;
            return run (targets, in, blockSize, [&] (ChorusDsp::Targets& next, int start)
                        { next.ppq = startPpq + beatsPerBlock * (double) start / blockSize; });
        };

        const auto a = from (8.0), b = from (9.0), c = from (8.5);
        auto same = 0.0, different = 0.0;

        for (int i = (int) (0.05 * rate); i < in.size(); ++i)
        {
            same = std::max (same, std::abs ((double) a.left[(size_t) i] - (double) b.left[(size_t) i]));
            different = std::max (different, std::abs ((double) a.left[(size_t) i] - (double) c.left[(size_t) i]));
        }

        CHECK (toDb (same / 0.5) < -90.0);
        CHECK (toDb (different / 0.5) > -40.0);
    }
}

TEST_CASE ("Chorus test 6: Keep bass mono takes the wet's side out below Low cut", "[chorus]")
{
    constexpr double bassHz = 60.0;
    const auto length = (int) (1.0 * rate);

    // Pure side at 60 Hz, then pure mid.
    Stereo side (length), mid (length);

    for (int i = 0; i < length; ++i)
    {
        const auto x = (float) (0.5 * std::sin (2.0 * pi * bassHz * (double) i / rate));
        side.left[(size_t) i] = x;
        side.right[(size_t) i] = -x;
        mid.left[(size_t) i] = mid.right[(size_t) i] = x;
    }

    const auto measure = [&] (const Stereo& in, bool bassMono, bool sideOut)
    {
        auto t = plainWet();
        t.voices = 3;
        t.lowCutOn = true;
        t.lowCutHz = 150.0;
        t.bassMono = bassMono;

        const auto out = run (t, in);
        std::vector<double> part ((size_t) length);

        for (int i = 0; i < length; ++i)
            part[(size_t) i] = 0.5 * ((double) out.left[(size_t) i] + (sideOut ? -1.0 : 1.0) * (double) out.right[(size_t) i]);

        return rmsOf (part, length / 2, length);
    };

    const auto sideDrop = toDb (measure (side, true, true) / measure (side, false, true));
    const auto midChange = toDb (measure (mid, true, false) / measure (mid, false, false));

    INFO ("side at 60 Hz: " << sideDrop << " dB; mid: " << midChange << " dB");
    CHECK (sideDrop < -12.0);
    CHECK (std::abs (midChange) < 0.1);
}

TEST_CASE ("Chorus test 7: no clicks when Delay, Voices or Mode change", "[chorus]")
{
    const auto blockSize = GENERATE (1, 64, 333, 1024);
    const auto rateHz = GENERATE (0.3, 3.0);

    // A 200 Hz tone; a jump in the output would be a step far bigger than the tone's own.
    const auto length = (int) (3.5 * rate);
    const auto in = sine (length, 200.0, 0.5);
    const auto inputStep = 0.5 * 2.0 * pi * 200.0 / rate;

    auto t = plainWet();
    t.voices = 3;
    t.rateHz = rateHz;

    const auto at = [] (double seconds) { return (int) (seconds * rate); };

    const auto out = run (t, in, blockSize, [&] (ChorusDsp::Targets& next, int start)
    {
        next.delayMs = start >= at (0.5) ? 20.0 : 5.0;
        next.voices = start >= at (1.5) ? 1 : (start >= at (1.0) ? 6 : 3);
        next.mode = start >= at (3.0) ? ChorusDsp::dimension
                  : start >= at (2.5) ? ChorusDsp::doubler
                  : start >= at (2.0) ? ChorusDsp::flange
                                      : ChorusDsp::classic;
    });

    auto largest = 0.0;

    for (int i = 1; i < length; ++i)
        for (const auto* channel : { &out.left, &out.right })
            largest = std::max (largest, std::abs ((double) (*channel)[(size_t) i] - (double) (*channel)[(size_t) i - 1]));

    INFO ("block " << blockSize << ", rate " << rateHz << " Hz: largest step " << largest << ", " << largest / inputStep << " x the input's");
    CHECK (largest < 6.0 * inputStep);
}

TEST_CASE ("Chorus test 7: Changing Mode leaves Voices as it was", "[chorus]")
{
    ChorusProcessor processor;
    auto* voices = processor.getApvts().getParameter ("voices");
    auto* mode = processor.getApvts().getParameter ("mode");
    REQUIRE (voices != nullptr);
    REQUIRE (mode != nullptr);

    voices->setValueNotifyingHost (voices->convertTo0to1 (5.0f));

    for (const auto chosen : { ChorusDsp::classic, ChorusDsp::dimension, ChorusDsp::flange, ChorusDsp::ensemble })
    {
        mode->setValueNotifyingHost (mode->convertTo0to1 ((float) chosen));
        CHECK (juce::roundToInt (processor.read ("voices")) == 5);
    }
}

TEST_CASE ("Chorus test 8: Ducking 100 % takes the wet down 24 dB while the input is loud", "[chorus]")
{
    // A tone at -12 dBFS for 1 s, then at -40 dBFS: under the threshold, so the wet comes back.
    const auto length = (int) (2.0 * rate), loudEnd = (int) rate;
    Stereo in (length);

    for (int i = 0; i < length; ++i)
        in.left[(size_t) i] = in.right[(size_t) i] = (float) ((i < loudEnd ? 0.25 : 0.01) * std::sin (2.0 * pi * 440.0 * (double) i / rate));

    auto t = plainWet();
    const auto reference = run (t, in);
    t.ducking = 1.0;
    const auto ducked = run (t, in);

    // The gain applied, in dB, over 10 ms windows.
    const auto gainAt = [&] (double seconds)
    {
        const auto from = (int) (seconds * rate), to = from + (int) (0.01 * rate);
        std::vector<double> a ((size_t) length), b ((size_t) length);

        for (int i = from; i < to; ++i)
        {
            a[(size_t) i] = ducked.left[(size_t) i];
            b[(size_t) i] = reference.left[(size_t) i];
        }

        return toDb (rmsOf (a, from, to) / rmsOf (b, from, to));
    };

    INFO ("loud " << gainAt (0.5) << " dB; 100 ms after " << gainAt (1.1) << " dB; 330 ms after " << gainAt (1.33) << " dB");
    CHECK (std::abs (gainAt (0.5) + 24.0) < 1.0);
    CHECK (gainAt (1.1) < -3.0);
    CHECK (gainAt (1.33) > -1.0);
}

// CPU, by §0.4 of the Phase 2.1 brief: five runs in one build, the median, and the ratio to Fancy
// Comp at its defaults, measured as the workflow test does. Decision 4 A: under 1.5 x at the
// defaults (3 voices), under 3 x at 6 voices.
TEST_CASE ("Chorus CPU measurement", "[.measure]")
{
    constexpr double seconds = 10.0;
    constexpr int measureBlock = 128;

    const auto cpu = [] (fancy::FancyProcessor& processor)
    {
        std::vector<double> runs;

        for (int pass = 0; pass < 5; ++pass)
        {
            processor.setRateAndBufferSizeDetails (rate, measureBlock);
            processor.prepareToPlay (rate, measureBlock);
            processor.waitForBackgroundWork (20000);

            juce::AudioBuffer<float> block (juce::jmax (2, processor.getTotalNumInputChannels()), measureBlock);
            juce::MidiBuffer midi;
            std::mt19937 random (8);
            std::uniform_real_distribution<float> uniform (-0.25f, 0.25f);
            std::chrono::duration<double> spent {};

            for (int done = 0; done < (int) (seconds * rate); done += measureBlock)
            {
                block.clear();

                for (int ch = 0; ch < processor.getMainBusNumInputChannels(); ++ch)
                    for (int i = 0; i < measureBlock; ++i)
                        block.setSample (ch, i, uniform (random));

                midi.clear();
                const auto start = std::chrono::steady_clock::now();
                processor.processBlock (block, midi);
                spent += std::chrono::steady_clock::now() - start;
            }

            processor.releaseResources();
            runs.push_back (100.0 * spent.count() / seconds);
        }

        std::sort (runs.begin(), runs.end());
        return runs[2];
    };

    double reference = 0.0;

   #if FANCY_HAS_COMP
    {
        CompProcessor comp;
        reference = cpu (comp);
    }
   #endif

    ChorusProcessor defaults;
    const auto plain = cpu (defaults);

    ChorusProcessor six;
    auto* voices = six.getApvts().getParameter ("voices");
    voices->setValueNotifyingHost (voices->convertTo0to1 (6.0f));
    const auto sixVoices = cpu (six);

    const auto ratio = [reference] (double value) { return reference > 0.0 ? value / reference : 0.0; };

    std::printf ("chorus cpu (median of 5, 48 kHz, 128 samples, stereo): Fancy Comp default %.3f %%; Chorus default (3 voices) %.3f %% "
                 "(%.2f x Comp, target < 1.5 x); 6 voices %.3f %% (%.2f x Comp, target < 3 x)\n",
                 reference, plain, ratio (plain), sixVoices, ratio (sixVoices));
}
