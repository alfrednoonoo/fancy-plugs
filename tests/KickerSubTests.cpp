// Fancy Kicker's Sub mode (docs/kicker-sub.md): §6.4 of docs/BUILD_PROMPT_PHASE3_NEW_PLUGINS.md and
// the sweeping-kick test the user added.

#include "KickerDsp.h"
#include "KickerProcessor.h"
#include "LegacyKickerDsp.h"

#if FANCY_HAS_COMP
 #include "CompDsp.h"
#endif

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <fancy/Decibels.h>

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
constexpr double pi = std::numbers::pi;
constexpr double rate = 48000.0;

double toDb (double gain) { return 20.0 * std::log10 (std::max (gain, 1.0e-15)); }

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples, 0.0f), right ((size_t) numSamples, 0.0f) {}
    int size() const { return (int) left.size(); }
};

KickerDsp::Targets subTargets()
{
    KickerDsp::Targets targets;
    targets.sub = true;
    return targets;
}

Stereo run (const KickerDsp::Targets& targets, const Stereo& input, int blockSize = 64)
{
    KickerDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (targets);

    auto output = input;

    for (int start = 0; start < output.size(); start += blockSize)
    {
        float* channels[] { output.left.data() + start, output.right.data() + start };
        dsp.setTargets (targets);
        dsp.process (channels, 2, std::min (blockSize, output.size() - start));
    }

    return output;
}

// A kick: a sine whose pitch falls from `startHz` to `endHz` with time constant `glide`, decaying
// with time constant `decay`, starting at `onset` seconds.
void addKick (Stereo& s, double onset, double amplitude, double startHz, double endHz, double glide, double decay, double length = 0.8)
{
    auto phase = 0.0;
    const auto first = (int) (onset * rate);

    for (int i = first; i < std::min (s.size(), first + (int) (length * rate)); ++i)
    {
        const auto t = (i - first) / rate;
        const auto hz = endHz + (startHz - endHz) * (glide > 0.0 ? std::exp (-t / glide) : 0.0);
        const auto value = amplitude * std::exp (-t / decay) * std::sin (phase);
        phase += 2.0 * pi * hz / rate;
        s.left[(size_t) i] += (float) value;
        s.right[(size_t) i] += (float) value;
    }
}

std::complex<double> dft (const std::vector<float>& x, double hz)
{
    std::complex<double> sum = 0.0;

    for (size_t i = 0; i < x.size(); ++i)
    {
        const auto phase = 2.0 * pi * hz * (double) i / rate;
        sum += (double) x[i] * std::complex<double> (std::cos (phase), -std::sin (phase));
    }

    return sum;
}

std::vector<double> upCrossings (const std::vector<float>& x, int from = 0)
{
    std::vector<double> times;

    for (int i = std::max (1, from); i < (int) x.size(); ++i)
    {
        const auto a = (double) x[(size_t) i - 1], b = (double) x[(size_t) i];

        if (a <= 0.0 && b > 0.0)
            times.push_back (i - 1 + (-a) / (b - a));
    }

    return times;
}
} // namespace

TEST_CASE ("Kicker: Boost is v1.2.0's Kicker, sample for sample", "[kicker][sub]")
{
    const auto sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    INFO ("rate " << sampleRate);

    KickerDsp current;
    LegacyKickerDsp legacy;
    current.prepare (sampleRate);
    legacy.prepare (sampleRate);

    const auto targetsAt = [] (int step)
    {
        KickerDsp::Targets t;
        t.frequencyHz = step % 2 == 0 ? 45.0 : 110.0;
        t.q = step % 3 == 0 ? 1.0 : 4.0;
        t.wetGain = step % 2 == 0 ? 0.25 : 1.4;
        t.dryGain = step % 4 == 0 ? 0.0 : 1.0;
        t.monoWet = step % 3 == 1;
        t.softClip = step % 5 == 2;
        t.bypass = step == 7;
        // Sub's own settings move too, without switching to Sub.
        t.subCutoffHz = step % 2 == 0 ? 60.0 : 180.0;
        t.subGain = step % 2 == 0 ? 0.1 : 1.0;
        t.subMono = step % 2 == 0;
        return t;
    };

    const auto legacyTargets = [] (const KickerDsp::Targets& t)
    {
        LegacyKickerDsp::Targets l;
        l.frequencyHz = t.frequencyHz;
        l.q = t.q;
        l.wetGain = t.wetGain;
        l.dryGain = t.dryGain;
        l.outputGain = t.outputGain;
        l.monoWet = t.monoWet;
        l.softClip = t.softClip;
        l.bypass = t.bypass;
        return l;
    };

    current.snapToTargets (targetsAt (0));
    legacy.snapToTargets (legacyTargets (targetsAt (0)));

    std::mt19937 random (9);
    std::uniform_int_distribution<int> sizes (1, 900);
    std::uniform_real_distribution<float> noise (-0.8f, 0.8f);
    long long differing = 0, compared = 0;

    for (int step = 0; step < 10; ++step)
    {
        for (int done = 0; done < (int) (0.2 * sampleRate);)
        {
            const auto count = std::min (sizes (random), (int) (0.2 * sampleRate) - done);
            std::vector<float> a ((size_t) count * 2), b;

            for (auto& v : a)
                v = noise (random);

            b = a;
            float* currentChannels[] { a.data(), a.data() + count };
            float* legacyChannels[] { b.data(), b.data() + count };

            current.setTargets (targetsAt (step));
            legacy.setTargets (legacyTargets (targetsAt (step)));
            current.process (currentChannels, 2, count);
            legacy.process (legacyChannels, 2, count);

            for (size_t i = 0; i < a.size(); ++i, ++compared)
                differing += a[i] != b[i] ? 1 : 0;

            done += count;
        }
    }

    INFO (differing << " of " << compared << " samples differ");
    CHECK (differing == 0);
}

TEST_CASE ("Kicker: a state saved before Sub mode loads in Boost", "[kicker][sub]")
{
    KickerProcessor processor;
    juce::MemoryBlock state;
    processor.getStateInformation (state);

    // Remove Sub's parameters, as a v1.2.0 state has none.
    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    REQUIRE (xml != nullptr);

    for (auto* child = xml->getFirstChildElement(); child != nullptr;)
    {
        auto* next = child->getNextElement();

        if (child->getStringAttribute ("id").startsWith ("sub") || child->getStringAttribute ("id") == "mode")
            xml->removeChildElement (child, true);

        child = next;
    }

    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary (*xml, oldState);

    KickerProcessor loaded;
    auto& mode = *loaded.getApvts().getParameter ("mode");
    mode.setValueNotifyingHost (1.0f);
    loaded.setStateInformation (oldState.getData(), (int) oldState.getSize());
    CHECK (mode.getValue() == 0.0f);
}

TEST_CASE ("Kicker Sub: a kick with an 80 Hz body gains 40 Hz, and nothing above Sub low-pass + 1 octave", "[kicker][sub]")
{
    Stereo input ((int) rate);
    addKick (input, 0.05, 0.8, 80.0, 80.0, 0.0, 0.1);

    auto targets = subTargets();
    const auto output = run (targets, input);

    const auto before = std::abs (dft (input.left, 40.0));
    const auto after = std::abs (dft (output.left, 40.0));
    INFO ("40 Hz: " << toDb (after / before) << " dB");
    CHECK (toDb (after / before) >= 12.0);

    // What Sub added on its own.
    std::vector<float> added ((size_t) input.size());
    for (int i = 0; i < input.size(); ++i)
        added[(size_t) i] = output.left[(size_t) i] - input.left[(size_t) i];

    const auto peak = std::abs (dft (added, 40.0));
    auto worst = 0.0;

    for (auto hz = 240.0; hz <= 4000.0; hz += 2.0)
        worst = std::max (worst, std::abs (dft (added, hz)));

    INFO ("above 240 Hz: " << toDb (worst / peak) << " dB relative to the sub at 40 Hz");
    CHECK (toDb (worst / peak) < -60.0);
}

TEST_CASE ("Kicker Sub: a kick falling from 120 to 50 Hz is followed, half its frequency, without dips", "[kicker][sub][sweep]")
{
    Stereo input ((int) rate);
    constexpr double onset = 0.05, glide = 0.03, decay = 0.2;
    addKick (input, onset, 0.8, 120.0, 50.0, glide, decay);

    auto targets = subTargets();
    targets.dryGain = 0.0;
    targets.subGain = 1.0;
    const auto sub = run (targets, input);

    // The input's frequency averaged between two times, from its phase.
    const auto inputPhase = [&] (double seconds)
    {
        const auto t = std::max (0.0, seconds - onset);
        return 2.0 * pi * (50.0 * t + 70.0 * glide * (1.0 - std::exp (-t / glide)));
    };

    const auto crossings = upCrossings (sub.left, (int) (onset * rate));
    REQUIRE (crossings.size() > 6);

    auto worstError = 0.0;
    auto checked = 0;

    // Skip the first period, while it locks on; stop where the kick has faded below -40 dBFS.
    for (size_t k = 2; k < crossings.size(); ++k)
    {
        const auto t1 = crossings[k - 1] / rate, t2 = crossings[k] / rate;

        if (0.8 * std::exp (-(t2 - onset) / decay) < 0.02)
            break;

        const auto subHz = 1.0 / (t2 - t1);
        const auto expected = 0.5 * (inputPhase (t2) - inputPhase (t1)) / (2.0 * pi * (t2 - t1));
        worstError = std::max (worstError, std::abs (subHz / expected - 1.0));
        ++checked;
    }

    INFO ("checked " << checked << " periods, worst frequency error " << 100.0 * worstError << " %");
    CHECK (checked >= 4);
    CHECK (worstError <= 0.05);

    // Peaks of each half cycle, against the kick's own decay over the same time.
    std::vector<std::pair<double, double>> peaks;
    {
        auto from = crossings[1];

        for (size_t k = 2; k < crossings.size(); ++k)
        {
            auto peak = 0.0;
            for (int i = (int) from; i < (int) crossings[k]; ++i)
                peak = std::max (peak, (double) std::abs (sub.left[(size_t) i]));

            peaks.push_back ({ crossings[k] / rate, peak });
            from = crossings[k];
        }
    }

    auto worstDip = 0.0;

    for (size_t k = 1; k < peaks.size(); ++k)
    {
        if (0.8 * std::exp (-(peaks[k].first - onset) / decay) < 0.02)
            break;

        const auto expected = peaks[k - 1].second * std::exp (-(peaks[k].first - peaks[k - 1].first) / decay);
        worstDip = std::min (worstDip, toDb (peaks[k].second / expected));
    }

    INFO ("worst dip " << worstDip << " dB");
    CHECK (worstDip > -3.0);
}

TEST_CASE ("Kicker Sub: noise at -50 dBFS with Sensitivity -40 dB makes no sub", "[kicker][sub]")
{
    Stereo input ((int) rate);
    std::mt19937 random (4);
    std::normal_distribution<double> noise (0.0, std::pow (10.0, -50.0 / 20.0));

    for (int i = 0; i < input.size(); ++i)
        input.left[(size_t) i] = input.right[(size_t) i] = (float) noise (random);

    auto targets = subTargets();
    targets.dryGain = 0.0;
    targets.subGain = fancy::dbToGain (6.0);
    const auto output = run (targets, input);

    auto peak = 0.0;
    for (auto v : output.left)
        peak = std::max (peak, (double) std::abs (v));

    INFO ("sub peak " << toDb (peak) << " dBFS");
    CHECK (toDb (peak) < -90.0);
}

TEST_CASE ("Kicker Sub: no clicks, through note-offs and re-triggers", "[kicker][sub][clicks]")
{
    Stereo input ((int) (2.0 * rate));
    addKick (input, 0.05, 0.8, 90.0, 55.0, 0.02, 0.15, 0.8);
    addKick (input, 0.40, 0.8, 90.0, 55.0, 0.02, 0.15, 0.12); // cut off abruptly: a note-off
    addKick (input, 0.60, 0.8, 90.0, 55.0, 0.02, 0.15, 0.8);
    addKick (input, 0.68, 0.8, 90.0, 55.0, 0.02, 0.15, 0.8);  // re-triggered while the last one sounds
    addKick (input, 1.30, 0.3, 70.0, 70.0, 0.0, 0.05, 0.3);

    auto targets = subTargets();
    targets.dryGain = 0.0;
    targets.subGain = 1.0;
    const auto sub = run (targets, input);

    const auto largestStep = [&] (int from, int to)
    {
        auto step = 0.0;
        for (int i = std::max (1, from); i < std::min (to, sub.size()); ++i)
            step = std::max (step, (double) std::abs (sub.left[(size_t) i] - sub.left[(size_t) i - 1]));
        return step;
    };

    // Its own steady largest step: inside the first kick's body.
    const auto steady = largestStep ((int) (0.07 * rate), (int) (0.2 * rate));
    const auto overall = largestStep (0, sub.size());
    INFO ("steady " << steady << ", overall " << overall);
    CHECK (overall <= 1.5 * steady);
}

TEST_CASE ("Kicker: switching mode while playing doesn't click", "[kicker][sub][clicks]")
{
    Stereo input ((int) (1.5 * rate));
    for (int k = 0; k < 6; ++k)
        addKick (input, 0.05 + 0.25 * k, 0.6, 85.0, 55.0, 0.02, 0.12, 0.25);

    KickerDsp dsp;
    dsp.prepare (rate);
    auto targets = KickerDsp::Targets {};
    dsp.snapToTargets (targets);

    auto output = input;
    auto steadyBoost = 0.0, steadySub = 0.0, atSwitch = 0.0;

    for (int start = 0; start < output.size(); start += 64)
    {
        const auto t = start / rate;
        targets.sub = (t >= 0.5 && t < 1.0);
        dsp.setTargets (targets);

        float* channels[] { output.left.data() + start, output.right.data() + start };
        const auto count = std::min (64, output.size() - start);
        dsp.process (channels, 2, count);

        for (int i = std::max (1, start); i < start + count; ++i)
        {
            const auto step = (double) std::abs (output.left[(size_t) i] - output.left[(size_t) i - 1]);
            const auto ti = i / rate;

            if (ti < 0.45) steadyBoost = std::max (steadyBoost, step);
            else if (ti > 0.55 && ti < 0.95) steadySub = std::max (steadySub, step);

            if (std::abs (ti - 0.5) < 0.03 || std::abs (ti - 1.0) < 0.03)
                atSwitch = std::max (atSwitch, step);
        }
    }

    INFO ("Boost " << steadyBoost << ", Sub " << steadySub << ", at the switches " << atSwitch);
    CHECK (atSwitch <= 1.5 * std::max (steadyBoost, steadySub));
}

TEST_CASE ("Kicker Sub: zero latency, and random block sizes give the same output", "[kicker][sub]")
{
    KickerProcessor processor;
    processor.getApvts().getParameter ("mode")->setValueNotifyingHost (1.0f);
    processor.prepareToPlay (rate, 512);
    CHECK (processor.getLatencySamples() == 0);

    Stereo input ((int) rate);
    addKick (input, 0.05, 0.8, 110.0, 50.0, 0.03, 0.2);
    addKick (input, 0.55, 0.8, 110.0, 50.0, 0.03, 0.2);

    const auto fixed = run (subTargets(), input, 512);

    KickerDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (subTargets());
    auto random = input;
    std::mt19937 generator (17);
    std::uniform_int_distribution<int> sizes (1, 2048);

    for (int start = 0; start < random.size();)
    {
        const auto count = std::min (sizes (generator), random.size() - start);
        float* channels[] { random.left.data() + start, random.right.data() + start };
        dsp.setTargets (subTargets());
        dsp.process (channels, 2, count);
        start += count;
    }

    CHECK (fixed.left == random.left);
    CHECK (fixed.right == random.right);
}

TEST_CASE ("kicker sub measurements", "[.measure]")
{
    // The sweep's figures, and CPU at 48 kHz, 128-sample blocks, the median of 5.
    constexpr int block = 128, blocks = 6000;

    const auto measure = [&] (const KickerDsp::Targets& targets)
    {
        std::vector<double> runs;

        for (int r = 0; r < 5; ++r)
        {
            KickerDsp dsp;
            dsp.prepare (rate);
            dsp.snapToTargets (targets);

            Stereo signal (block * blocks);
            for (int k = 0; k * 0.3 < signal.size() / rate; ++k)
                addKick (signal, 0.3 * k, 0.7, 110.0, 50.0, 0.03, 0.15, 0.3);

            std::chrono::duration<double> spent {};

            for (int b = 0; b < blocks; ++b)
            {
                float* channels[] { signal.left.data() + b * block, signal.right.data() + b * block };
                const auto start = std::chrono::steady_clock::now();
                dsp.process (channels, 2, block);
                spent += std::chrono::steady_clock::now() - start;
            }

            runs.push_back (100.0 * spent.count() / ((double) (blocks * block) / rate));
        }

        std::sort (runs.begin(), runs.end());
        return runs[2];
    };

    auto stereo = subTargets();
    stereo.subMono = false;

    const auto boost = measure (KickerDsp::Targets {});
    const auto sub = measure (subTargets());
    const auto subStereo = measure (stereo);

    std::printf ("kicker cpu (median of 5): Boost %.3f %%, Sub (Divider, Mono sub) %.3f %% (target < 0.5 %%), Sub with Mono sub off %.3f %%\n",
                 boost, sub, subStereo);

   #if FANCY_HAS_COMP
    std::vector<double> runs;

    for (int r = 0; r < 5; ++r)
    {
        CompDsp comp;
        comp.prepare (rate);
        comp.snapToTargets (CompDsp::Targets {});

        float left[block], right[block];
        float* channels[] { left, right };
        std::mt19937 random (8);
        std::uniform_real_distribution<float> noise (-0.25f, 0.25f);
        std::chrono::duration<double> spent {};

        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < block; ++i)
            {
                left[i] = noise (random);
                right[i] = noise (random);
            }

            const auto start = std::chrono::steady_clock::now();
            comp.process (channels, 2, nullptr, 0, block);
            spent += std::chrono::steady_clock::now() - start;
        }

        runs.push_back (100.0 * spent.count() / ((double) (blocks * block) / rate));
    }

    std::sort (runs.begin(), runs.end());
    std::printf ("kicker cpu reference: Fancy Comp default %.3f %%; Sub / Comp = %.2f\n", runs[2], sub / runs[2]);
   #endif

    SUCCEED();
}
