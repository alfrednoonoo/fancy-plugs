// Fancy Limit (docs/limit.md) and the shared pieces it adds: the extended TruePeakLimiter,
// LoudnessMeter and TruePeakMeter. The §5.4 tests of docs/BUILD_PROMPT_PHASE3_NEW_PLUGINS.md.

#include "LegacyTruePeakLimiter.h"
#include "LimitDsp.h"
#include "LimitProcessor.h"

#if FANCY_HAS_COMP
 #include "CompDsp.h"
#endif

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <fancy/dsp/LoudnessMeter.h>
#include <fancy/dsp/TruePeakLimiter.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <numbers>
#include <random>
#include <vector>

using Catch::Approx;

namespace
{
constexpr double pi = std::numbers::pi;

double toDb (double gain) { return 20.0 * std::log10 (std::max (gain, 1.0e-12)); }
double fromDb (double db) { return std::pow (10.0, db / 20.0); }

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples, 0.0f), right ((size_t) numSamples, 0.0f) {}
    int size() const { return (int) left.size(); }
};

LimitDsp::Targets defaults()
{
    return {};
}

// Runs the DSP over a copy, in blocks of the given size, or random sizes from 1 to 2048 if 0.
Stereo run (const LimitDsp::Targets& targets, const Stereo& input, double rate, int blockSize = 256)
{
    LimitDsp dsp;
    dsp.setTargets (targets);
    dsp.prepare (rate);

    auto output = input;
    std::mt19937 random (11);
    std::uniform_int_distribution<int> sizes (1, 2048);

    for (int start = 0; start < output.size();)
    {
        const auto count = std::min (blockSize > 0 ? blockSize : sizes (random), output.size() - start);
        float* channels[] { output.left.data() + start, output.right.data() + start };
        dsp.setTargets (targets);
        dsp.process (channels, 2, count);
        start += count;
    }

    return output;
}

// An independent reference: 8x windowed-sinc interpolation, 64 taps per phase.
double referenceTruePeak (const std::vector<float>& x)
{
    constexpr int factor = 8, taps = 64, half = taps / 2;
    std::vector<std::vector<double>> kernel (factor, std::vector<double> (taps));

    for (int phase = 0; phase < factor; ++phase)
    {
        for (int tap = 0; tap < taps; ++tap)
        {
            const auto t = (double) (tap - half + 1) - (double) phase / factor;
            const auto sinc = std::abs (t) < 1.0e-12 ? 1.0 : std::sin (pi * t) / (pi * t);
            const auto kaiserLike = 0.42 + 0.5 * std::cos (pi * t / half) + 0.08 * std::cos (2.0 * pi * t / half);
            kernel[(size_t) phase][(size_t) tap] = sinc * kaiserLike;
        }
    }

    auto peak = 0.0;

    for (int n = taps; n < (int) x.size(); ++n)
    {
        for (int phase = 0; phase < factor; ++phase)
        {
            auto sum = 0.0;

            for (int tap = 0; tap < taps; ++tap)
                sum += kernel[(size_t) phase][(size_t) tap] * (double) x[(size_t) (n - taps + 1 + tap)];

            peak = std::max (peak, std::abs (sum));
        }
    }

    return peak;
}

double samplePeak (const std::vector<float>& x, int from = 0)
{
    auto peak = 0.0;

    for (int i = from; i < (int) x.size(); ++i)
        peak = std::max (peak, (double) std::abs (x[(size_t) i]));

    return peak;
}

Stereo sine (double hz, double amplitude, double seconds, double rate, double phase = 0.0)
{
    Stereo s ((int) (seconds * rate));

    for (int i = 0; i < s.size(); ++i)
        s.left[(size_t) i] = s.right[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * hz * i / rate + phase));

    return s;
}
} // namespace

// The shared core ----------------------------------------------------------------------------

TEST_CASE ("TruePeakLimiter with default settings is the v1.2.0 limiter, sample for sample", "[limit][shared]")
{
    const auto rate = GENERATE (44100.0, 48000.0, 96000.0);

    fancy::dsp::TruePeakLimiter current;
    fancy::legacy::LegacyTruePeakLimiter legacy;
    current.prepare (rate);
    legacy.prepare (rate);

    REQUIRE (current.getLatencySamples() == legacy.getLatencySamples());

    std::mt19937 random (3);
    std::normal_distribution<double> noise (0.0, 1.0);
    auto mismatches = 0;

    for (int i = 0; i < (int) (2.0 * rate); ++i)
    {
        // Bursts up to +40 dB, quiet passages, and the amount ramping as Comp's switch does.
        const auto level = (i / 4000) % 3 == 0 ? 100.0 : ((i / 4000) % 3 == 1 ? 0.3 : 3.0);
        const auto amount = std::clamp ((double) (i % 20000) / 5000.0, 0.0, 1.0);

        auto a = noise (random) * level, b = noise (random) * level;
        auto c = a, d = b;
        current.process (a, b, amount);
        legacy.process (c, d, amount);

        if (a != c || b != d)
            ++mismatches;
    }

    CHECK (mismatches == 0);
}

TEST_CASE ("TruePeakLimiter: a shorter window keeps the latency and still holds the ceiling", "[limit][shared]")
{
    constexpr double rate = 48000.0;
    const auto window = GENERATE (1, 24, 96, 240);
    const auto shape = GENERATE (fancy::dsp::TruePeakLimiter::Shape::box, fancy::dsp::TruePeakLimiter::Shape::smooth,
                                 fancy::dsp::TruePeakLimiter::Shape::punchy);

    fancy::dsp::TruePeakLimiter::Settings settings;
    settings.maxWindow = 240;
    settings.minWindow = 1;
    settings.window = window;
    settings.shape = shape;

    fancy::dsp::TruePeakLimiter limiter;
    limiter.prepare (rate, settings);
    CHECK (limiter.getLatencySamples() == 248);

    std::mt19937 random (5);
    std::normal_distribution<double> noise (0.0, 1.0);
    auto worstGainError = 0.0;

    // The gain applied at each output sample against what that sample needed: never above it,
    // so the final clamp isn't doing the work.

    for (int i = 0; i < 48000; ++i)
    {
        const auto x = noise (random) * ((i / 3000) % 2 == 0 ? 8.0 : 0.2);
        auto left = x, right = x;
        double delayed[2];
        limiter.process (left, right, 1.0, delayed);

        if (i >= limiter.getLatencySamples() && std::abs (delayed[0]) > 1.0e-9)
            worstGainError = std::max (worstGainError, std::abs (delayed[0]) * limiter.getLastGain (0) - 1.0);
    }

    INFO ("window " << window << ", shape " << (int) shape);

    // Sample peaks: the interpolator sees at least the samples themselves.
    CHECK (worstGainError < 1.0e-9);
}

TEST_CASE ("TruePeakLimiter: Link 0 leaves a quiet channel alone; Link 100 % turns it down with the loud one", "[limit][shared]")
{
    constexpr double rate = 48000.0;
    const auto link = GENERATE (0.0, 1.0);

    fancy::dsp::TruePeakLimiter::Settings settings;
    settings.link = link;
    settings.truePeak = false;

    fancy::dsp::TruePeakLimiter limiter;
    limiter.prepare (rate, settings);
    const auto latency = limiter.getLatencySamples();

    std::vector<double> in, out;

    for (int i = 0; i < 24000; ++i)
    {
        auto left = 4.0 * std::sin (2.0 * pi * 100.0 * i / rate);
        auto right = 0.1 * std::sin (2.0 * pi * 300.0 * i / rate);
        in.push_back (right);
        limiter.process (left, right, 1.0);
        out.push_back (right);
    }

    auto worst = 0.0;

    for (int i = latency + 2400; i < (int) out.size(); ++i)
        worst = std::max (worst, std::abs (out[(size_t) i] - in[(size_t) (i - latency)]));

    if (link == 0.0)
        CHECK (worst == 0.0);
    else
        CHECK (worst > 0.05);
}

// §5.4 ---------------------------------------------------------------------------------------

TEST_CASE ("Limit: true peak stays under Ceiling + 0.1 dB, 30 dB over, by an independent 8x meter", "[limit][truepeak]")
{
    const auto rate = GENERATE (44100.0, 48000.0);
    const auto oversampling = GENERATE (0, 1, 2, 3);
    const auto signal = GENERATE (0, 1, 2, 3, 4);

    auto targets = defaults();
    targets.thresholdDb = -30.0;
    targets.ceilingDb = -1.0;
    targets.oversampling = oversampling;

    const auto seconds = 2.0;
    Stereo input ((int) (seconds * rate));
    std::mt19937 random (7);
    std::normal_distribution<double> noise (0.0, 0.1);
    const char* names[] { "997 Hz", "11.025 kHz at a quarter-sample phase", "fs/4 at 45 degrees", "square wave", "dense mix" };

    for (int i = 0; i < input.size(); ++i)
    {
        const auto t = (double) i / rate;
        double x = 0.0;

        switch (signal)
        {
            case 0: x = 0.5 * std::sin (2.0 * pi * 997.0 * t); break;
            case 1: x = 0.5 * std::sin (2.0 * pi * 11025.0 * t + 0.5 * pi * 0.25); break;
            case 2: x = 0.5 * std::sin (2.0 * pi * (rate / 4.0) * t + pi / 4.0); break;
            case 3: x = std::fmod (t * 220.0, 1.0) < 0.5 ? 0.4 : -0.4; break;
            default:
                x = 0.2 * std::sin (2.0 * pi * 55.0 * t) + 0.15 * std::sin (2.0 * pi * 1250.0 * t)
                  + 0.1 * std::sin (2.0 * pi * 7300.0 * t) + noise (random) * ((i / 2205) % 3 == 0 ? 1.0 : 0.3);
                break;
        }

        input.left[(size_t) i] = (float) x;
        input.right[(size_t) i] = (float) (signal == 4 ? -x * 0.8 : x);
    }

    const auto output = run (targets, input, rate);
    const auto peakDb = toDb (std::max (referenceTruePeak (output.left), referenceTruePeak (output.right)));

    INFO (names[signal] << ", " << rate << " Hz, oversampling index " << oversampling << ": true peak " << peakDb << " dBTP");
    CHECK (peakDb <= -1.0 + 0.1);
}

TEST_CASE ("Limit: +200 dB over, no sample passes Ceiling, in every Style and Lookahead", "[limit][ceiling]")
{
    constexpr double rate = 48000.0;
    const auto style = GENERATE (0, 1, 2);
    const auto lookahead = GENERATE (0.0, 2.0, 5.0);
    const auto oversampling = GENERATE (0, 2);

    auto targets = defaults();
    targets.style = style;
    targets.lookaheadMs = lookahead;
    targets.ceilingDb = -1.0;
    targets.oversampling = oversampling;

    auto input = sine (440.0, 1.0e10, 1.0, rate);

    for (int i = 0; i < input.size(); i += 997)
        input.left[(size_t) i] = -3.0e10f;

    const auto output = run (targets, input, rate);
    const auto ceiling = fromDb (-1.0);

    INFO ("style " << style << ", lookahead " << lookahead << " ms, oversampling index " << oversampling);
    CHECK (samplePeak (output.left) <= ceiling + 1.0e-6);
    CHECK (samplePeak (output.right) <= ceiling + 1.0e-6);

    for (const auto value : output.left)
        REQUIRE (std::isfinite (value));
}

TEST_CASE ("Limit: the reported latency doesn't change with Lookahead, and the output stays aligned", "[limit][latency]")
{
    constexpr double rate = 48000.0;
    const auto oversampling = GENERATE (0, 1, 2, 3);

    std::vector<int> latencies, arrivals;

    for (const auto lookahead : { 0.0, 2.0, 5.0 })
    {
        auto targets = defaults();
        targets.lookaheadMs = lookahead;
        targets.oversampling = oversampling;
        targets.ceilingDb = 0.0;
        targets.truePeak = false;

        // Linear phase: its impulse peaks exactly at the reported latency.
        targets.linearPhase = true;

        LimitDsp dsp;
        dsp.setTargets (targets);
        dsp.prepare (rate);
        latencies.push_back (dsp.getLatencySamples());

        Stereo input (8192);
        input.left[1000] = input.right[1000] = 0.25f;

        auto output = input;
        float* channels[] { output.left.data(), output.right.data() };
        dsp.process (channels, 2, output.size());

        const auto peak = std::max_element (output.left.begin(), output.left.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); });
        arrivals.push_back ((int) (peak - output.left.begin()) - 1000);
    }

    INFO ("oversampling index " << oversampling);
    CHECK (latencies[0] == latencies[1]);
    CHECK (latencies[1] == latencies[2]);
    CHECK (arrivals[0] == latencies[0]);
    CHECK (arrivals[1] == latencies[0]);
    CHECK (arrivals[2] == latencies[0]);

    // 5 ms at 48 kHz plus the 8-sample interpolator, plus the oversampling filters.
    CHECK (latencies[0] >= 248);
}

TEST_CASE ("Limit: a 40 Hz sine with 10 dB of reduction stays under 1 % THD with Auto release", "[limit][thd]")
{
    constexpr double rate = 48000.0;

    auto targets = defaults();
    targets.thresholdDb = -10.0;
    targets.ceilingDb = -1.0;
    targets.autoRelease = true;

    const auto output = run (targets, sine (40.0, fromDb (-1.0), 6.0, rate), rate);

    // The last 2 s, 80 whole periods, so every harmonic sits on a bin.
    const auto length = (int) (2.0 * rate);
    const auto start = output.size() - length;

    const auto magnitude = [&] (double hz)
    {
        std::complex<double> sum;

        for (int i = 0; i < length; ++i)
            sum += (double) output.left[(size_t) (start + i)] * std::polar (1.0, -2.0 * pi * hz * i / rate);

        return std::abs (sum);
    };

    const auto fundamental = magnitude (40.0);
    auto harmonics = 0.0;

    for (int h = 2; h <= 20; ++h)
        harmonics += std::pow (magnitude (40.0 * h), 2.0);

    const auto thd = 100.0 * std::sqrt (harmonics) / fundamental;
    const auto reductionDb = toDb (fromDb (-1.0) * fromDb (10.0)) - toDb (samplePeak (output.left, start));

    std::printf ("limit thd: 40 Hz, %.1f dB of reduction, Auto release: %.3f %%\n", reductionDb, thd);
    INFO ("THD " << thd << " %, reduction " << reductionDb << " dB");
    CHECK (reductionDb > 9.0);
    CHECK (thd < 1.0);
}

TEST_CASE ("Limit: Listen plays what the limiter removes", "[limit]")
{
    constexpr double rate = 48000.0;

    auto targets = defaults();
    targets.thresholdDb = -12.0;
    targets.oversampling = LimitDsp::osOff;

    const auto input = sine (200.0, 0.5, 1.0, rate);
    const auto limited = run (targets, input, rate);

    targets.listen = true;
    const auto removed = run (targets, input, rate);

    // Limited plus removed is the input pushed up 12 dB, delayed.
    LimitDsp dsp;
    dsp.prepare (rate);
    const auto latency = dsp.getLatencySamples();
    auto worst = 0.0;

    for (int i = latency + 4800; i < input.size(); ++i)
    {
        const auto expected = (double) input.left[(size_t) (i - latency)] * fromDb (12.0);
        worst = std::max (worst, std::abs ((double) limited.left[(size_t) i] + (double) removed.left[(size_t) i] - expected));
    }

    CHECK (worst < 1.0e-3);
}

TEST_CASE ("Limit: dither is deterministic and stays within one step of the signal", "[limit][dither]")
{
    constexpr double rate = 48000.0;

    auto targets = defaults();
    targets.oversampling = LimitDsp::osOff;
    targets.dither = LimitDsp::dither16;

    const auto input = sine (1000.0, 0.25, 0.5, rate);
    const auto first = run (targets, input, rate, 0);
    const auto second = run (targets, input, rate, 512);

    CHECK (first.left == second.left);

    // Every output is a whole number of 16-bit steps.
    const auto step = std::ldexp (1.0, -15);

    for (int i = 0; i < 2000; ++i)
    {
        const auto ratio = (double) first.left[(size_t) i] / step;
        REQUIRE (std::abs (ratio - std::round (ratio)) < 1.0e-3);
    }
}

// Loudness: synthesised signals after EBU Tech 3341 and 3342 ---------------------------------

namespace
{
struct Segment
{
    double seconds, dbfs;
};

struct Reading
{
    double momentary, shortTerm, integrated, range;

    double getMomentary() const { return momentary; }
    double getShortTerm() const { return shortTerm; }
    double getIntegrated() const { return integrated; }
    double getRange() const { return range; }
};

Reading measure (const std::vector<Segment>& segments, double rate = 48000.0)
{
    fancy::dsp::LoudnessMeter meter;
    meter.prepare (rate);

    std::vector<float> block (4800);
    double phase = 0.0;

    for (const auto& segment : segments)
    {
        const auto amplitude = fromDb (segment.dbfs);
        auto left = (int) std::lround (segment.seconds * rate);

        while (left > 0)
        {
            const auto count = std::min (left, (int) block.size());

            for (int i = 0; i < count; ++i)
            {
                block[(size_t) i] = (float) (amplitude * std::sin (phase));
                phase += 2.0 * pi * 1000.0 / rate;
            }

            meter.process (block.data(), block.data(), count);
            left -= count;
        }
    }

    return { meter.getMomentary(), meter.getShortTerm(), meter.getIntegrated(), meter.getRange() };
}
} // namespace

TEST_CASE ("LoudnessMeter: integrated loudness of the EBU Tech 3341 stereo sine cases", "[limit][loudness]")
{
    const auto rate = GENERATE (44100.0, 48000.0);

    CHECK (measure ({ { 20.0, -23.0 } }, rate).getIntegrated() == Approx (-23.0).margin (0.1));
    CHECK (measure ({ { 20.0, -33.0 } }, rate).getIntegrated() == Approx (-33.0).margin (0.1));
    CHECK (measure ({ { 10.0, -36.0 }, { 60.0, -23.0 }, { 10.0, -36.0 } }, rate).getIntegrated() == Approx (-23.0).margin (0.1));
    CHECK (measure ({ { 10.0, -72.0 }, { 10.0, -36.0 }, { 60.0, -23.0 }, { 10.0, -36.0 }, { 10.0, -72.0 } }, rate).getIntegrated()
           == Approx (-23.0).margin (0.1));
    CHECK (measure ({ { 20.0, -26.0 }, { 20.1, -20.0 }, { 20.0, -26.0 } }, rate).getIntegrated() == Approx (-23.0).margin (0.1));

    // Momentary and short-term of a steady tone.
    const auto steady = measure ({ { 5.0, -23.0 } }, rate);
    CHECK (steady.getMomentary() == Approx (-23.0).margin (0.1));
    CHECK (steady.getShortTerm() == Approx (-23.0).margin (0.1));
}

TEST_CASE ("LoudnessMeter: loudness range of the EBU Tech 3342 cases", "[limit][loudness]")
{
    CHECK (measure ({ { 20.0, -20.0 }, { 20.0, -30.0 } }).getRange() == Approx (10.0).margin (1.0));
    CHECK (measure ({ { 20.0, -20.0 }, { 20.0, -15.0 } }).getRange() == Approx (5.0).margin (1.0));
    CHECK (measure ({ { 20.0, -40.0 }, { 20.0, -20.0 } }).getRange() == Approx (20.0).margin (1.0));
    CHECK (measure ({ { 20.0, -50.0 }, { 20.0, -35.0 }, { 20.0, -20.0 }, { 20.0, -35.0 }, { 20.0, -50.0 } }).getRange()
           == Approx (15.0).margin (1.0));
}

TEST_CASE ("TruePeakMeter: sines read their true amplitude within 0.2 dB", "[limit][loudness]")
{
    constexpr double rate = 48000.0;
    const auto hz = GENERATE (997.0, 12000.0, 15000.0);
    const auto phase = GENERATE (0.0, pi / 4.0, pi / 3.0);

    fancy::dsp::TruePeakMeter meter;
    std::vector<float> x (48000);

    for (int i = 0; i < (int) x.size(); ++i)
        x[(size_t) i] = (float) (0.5 * std::sin (2.0 * pi * hz * i / rate + phase));

    meter.process (x.data(), x.data(), (int) x.size());

    INFO (hz << " Hz, phase " << phase);
    CHECK (meter.getPeakDb() == Approx (toDb (0.5)).margin (0.2));
}

// The plugin ---------------------------------------------------------------------------------

TEST_CASE ("Limit: state round-trips and the Target dropdown's values", "[limit]")
{
    LimitProcessor processor;
    CHECK (processor.getTargetLufs() == Approx (-14.0f));

    processor.setParameter ("target", (float) LimitProcessor::custom);
    processor.setParameter ("customTarget", -12.5f);
    CHECK (processor.getTargetLufs() == Approx (-12.5f));

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    LimitProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (restored.getTargetLufs() == Approx (-12.5f));
}

// CPU, by the Phase 2.1 §0.4 method ----------------------------------------------------------

namespace
{
template <typename Function>
double medianOfFive (Function&& measureOnce)
{
    std::vector<double> runs;

    for (int i = 0; i < 5; ++i)
        runs.push_back (measureOnce());

    std::sort (runs.begin(), runs.end());
    return runs[2];
}

constexpr int measureBlock = 128, measureBlocks = 6000;

double limitCpu (const LimitDsp::Targets& targets)
{
    constexpr double rate = 48000.0;

    LimitDsp dsp;
    dsp.setTargets (targets);
    dsp.prepare (rate);

    float left[measureBlock], right[measureBlock];
    float* channels[] { left, right };
    std::mt19937 random (8);
    std::uniform_real_distribution<float> noise (-0.25f, 0.25f);
    std::chrono::duration<double> spent {};

    for (int block = 0; block < measureBlocks; ++block)
    {
        for (int i = 0; i < measureBlock; ++i)
        {
            left[i] = noise (random);
            right[i] = noise (random);
        }

        const auto start = std::chrono::steady_clock::now();
        dsp.process (channels, 2, measureBlock);
        spent += std::chrono::steady_clock::now() - start;
    }

    return 100.0 * spent.count() / ((double) (measureBlocks * measureBlock) / rate);
}
} // namespace

TEST_CASE ("limit CPU measurement", "[.measure]")
{
    auto pushed = defaults();
    pushed.thresholdDb = -12.0;

    auto off = pushed;
    off.oversampling = LimitDsp::osOff;

    auto eight = pushed;
    eight.oversampling = LimitDsp::os8;

    const auto limit = medianOfFive ([&] { return limitCpu (pushed); });
    const auto limitOff = medianOfFive ([&] { return limitCpu (off); });
    const auto limitEight = medianOfFive ([&] { return limitCpu (eight); });

    std::printf ("limit cpu (median of 5): 4x + true peak, 12 dB pushed (target < 2 %%) %.3f %%, oversampling off %.3f %%, 8x %.3f %%\n",
                 limit, limitOff, limitEight);

   #if FANCY_HAS_COMP
    const auto comp = medianOfFive ([]
    {
        CompDsp dsp;
        dsp.prepare (48000.0);
        dsp.snapToTargets (CompDsp::Targets {});

        float left[measureBlock], right[measureBlock];
        float* channels[] { left, right };
        std::mt19937 random (8);
        std::uniform_real_distribution<float> noise (-0.25f, 0.25f);
        std::chrono::duration<double> spent {};

        for (int block = 0; block < measureBlocks; ++block)
        {
            for (int i = 0; i < measureBlock; ++i)
            {
                left[i] = noise (random);
                right[i] = noise (random);
            }

            const auto start = std::chrono::steady_clock::now();
            dsp.process (channels, 2, nullptr, 0, measureBlock);
            spent += std::chrono::steady_clock::now() - start;
        }

        return 100.0 * spent.count() / ((double) (measureBlocks * measureBlock) / 48000.0);
    });

    std::printf ("limit cpu reference: Fancy Comp default %.3f %% in the same run; limit 4x / comp = %.2f, off / comp = %.2f, 8x / comp = %.2f\n",
                 comp, limit / comp, limitOff / comp, limitEight / comp);
   #endif

    SUCCEED();
}
