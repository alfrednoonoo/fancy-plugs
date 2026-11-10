#pragma once

// The synthetic voice and measurements Fancy DS's §2.4 tests use, shared with Fancy DS-Mini's.

#include <fancy/dsp/Svf.h>

#include <cmath>
#include <random>
#include <vector>

namespace deesser_test
{
constexpr double rate = 48000.0;

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples, 0.0f), right ((size_t) numSamples, 0.0f) {}
    int size() const { return (int) left.size(); }
};

struct Segment
{
    int start, end;
};

// A synthetic voice: a 150 Hz sawtooth through three vowel formants (700, 1200 and 2500 Hz),
// with "s" bursts of noise band-passed to 5-9 kHz. Each 100 ms burst follows 400 ms of vowel.
// The bursts' RMS is `burstDb` against the vowel's.
struct Voice
{
    Stereo audio;
    std::vector<Segment> bursts, vowels;

    Voice (double levelDb, double burstDb = -6.0, double seconds = 4.0) : audio ((int) (seconds * rate))
    {
        const auto n = audio.size();
        std::vector<double> vowel ((size_t) n), noise ((size_t) n);

        fancy::dsp::SvfCoefficients formants[3], lowEdge, highEdge;
        formants[0].set (700.0, 5.0, rate);
        formants[1].set (1200.0, 6.0, rate);
        formants[2].set (2500.0, 8.0, rate);
        lowEdge.set (5000.0, 0.7071, rate);
        highEdge.set (9000.0, 0.7071, rate);

        fancy::dsp::Svf formantFilters[3], lowFilters[2], highFilters[2];
        std::mt19937 random (17);
        std::normal_distribution<double> white (0.0, 1.0);
        auto phase = 0.0;

        for (int i = 0; i < n; ++i)
        {
            phase = std::fmod (phase + 150.0 / rate, 1.0);
            const auto saw = 2.0 * phase - 1.0;

            auto v = 0.0;

            for (int f = 0; f < 3; ++f)
                v += formantFilters[f].tick (formants[f], saw).band * (f == 0 ? 1.0 : 0.5);

            vowel[(size_t) i] = v;

            // Fourth-order band-pass: two high-passes at 5 kHz, two low-passes at 9 kHz.
            auto s = white (random);
            s = lowFilters[1].tick (lowEdge, lowFilters[0].tick (lowEdge, s).high).high;
            s = highFilters[1].tick (highEdge, highFilters[0].tick (highEdge, s).low).low;
            noise[(size_t) i] = s;
        }

        const auto rms = [] (const std::vector<double>& x)
        {
            auto sum = 0.0;
            for (const auto value : x) sum += value * value;
            return std::sqrt (sum / (double) x.size());
        };

        const auto vowelScale = std::pow (10.0, levelDb / 20.0) / rms (vowel);
        const auto burstScale = vowelScale * rms (vowel) / rms (noise) * std::pow (10.0, burstDb / 20.0);
        const auto period = (int) (0.5 * rate), burstLength = (int) (0.1 * rate);

        for (int i = 0; i < n; ++i)
        {
            const auto inBurst = i % period >= period - burstLength;
            const auto x = inBurst ? noise[(size_t) i] * burstScale : vowel[(size_t) i] * vowelScale;
            audio.left[(size_t) i] = audio.right[(size_t) i] = (float) x;
        }

        // Skip the first period, while the detector settles, and 50 ms after each burst for the release.
        for (int start = period; start + period <= n; start += period)
        {
            bursts.push_back ({ start + period - burstLength, start + period });
            vowels.push_back ({ start + (int) (0.05 * rate), start + period - burstLength });
        }
    }
};

// Output against input energy over the segments, in dB, the output read `latency` later.
inline double changeDb (const Stereo& input, const Stereo& output, const std::vector<Segment>& segments, int latency = 0)
{
    auto in = 0.0, out = 0.0;

    for (const auto& segment : segments)
    {
        for (int i = segment.start; i < segment.end && i + latency < output.size(); ++i)
        {
            in += (double) input.left[(size_t) i] * input.left[(size_t) i];
            out += (double) output.left[(size_t) (i + latency)] * output.left[(size_t) (i + latency)];
        }
    }

    return 10.0 * std::log10 (out / in);
}
} // namespace deesser_test
