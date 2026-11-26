// fancy::dsp::StftFrames, the framing moved out of Fancy FIR's SpectralEngine for Fancy EQ's
// spectral dynamics (docs/eq.md, "Phase 4 Wave 3", decision 5 A; test 8): FIR on the shared
// framing renders exactly as v1.7.1's engine, and the framing alone gives back its input.

#include "FirShared.h"
#include "LegacyFirW2Engine.h"
#include "SpectralEngine.h"

#include <fancy/dsp/StftFrames.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;

std::vector<float> noise (int numSamples, unsigned seed, float level = 0.1f)
{
    std::mt19937 random (seed);
    std::normal_distribution<float> gauss (0.0f, level);
    std::vector<float> samples ((size_t) numSamples);

    for (auto& sample : samples)
        sample = gauss (random);

    return samples;
}

template <typename Function>
void setCurve (fir::Shared& shared, int curve, Function&& valueAt)
{
    for (int i = 0; i < fir::curvePoints; ++i)
        shared.curves[(size_t) curve][(size_t) i].store ((float) valueAt (fir::curveHz (i)));

    shared.curveVersions[(size_t) curve].fetch_add (1);
}

void prepareShared (fir::Shared& shared)
{
    setCurve (shared, fir::eq, [] (double hz) { return hz < 1000.0 ? -3.0 : 2.0; });
    setCurve (shared, fir::gate, [] (double hz) { return hz < 1000.0 ? -30.0 : -45.0; });
    setCurve (shared, fir::compressor, [] (double hz) { return -40.0 + 5.0 * std::log10 (hz / 20.0); });

    for (auto& value : shared.profile)
        value.store (2.0e-4f);

    shared.hasProfile.store (true);
    shared.profileVersion.fetch_add (1);
}

struct Stereo
{
    std::vector<float> left, right;
};

template <typename Engine, typename Settings>
Stereo run (Engine& engine, const std::vector<float>& left, const std::vector<float>& right, const std::vector<float>* key,
            const Settings& settings, int numChannels, int blockSize)
{
    const auto numSamples = (int) left.size();
    Stereo out { std::vector<float> ((size_t) numSamples), std::vector<float> ((size_t) numSamples) };

    for (int position = 0; position < numSamples; position += blockSize)
    {
        const auto count = std::min (blockSize, numSamples - position);
        const auto* keyData = key != nullptr ? key->data() + position : nullptr;
        engine.process (left.data() + position, right.data() + position, keyData, keyData,
                        out.left.data() + position, out.right.data() + position, numChannels, count, settings, true);
    }

    return out;
}
} // namespace

TEST_CASE ("StftFrames: FIR on the shared framing renders exactly as v1.7.1's engine", "[fir][stft][w3]")
{
    struct Case
    {
        int size;
        bool lowLatency, convolve, midSide;
        int mode, numChannels;
        bool key;
    };

    const auto c = GENERATE (Case { 2048, false, false, false, fir::eq, 2, false },
                             Case { 2048, true, false, false, fir::gate, 2, false },
                             Case { 1024, false, false, false, fir::compressor, 2, true },
                             Case { 4096, false, false, true, fir::resonance, 2, false },
                             Case { 8192, false, false, false, fir::subtract, 2, false },   // staged
                             Case { 16384, true, false, true, fir::eq, 2, true },          // staged, low latency, Mid/Side
                             Case { 2048, false, true, false, fir::eq, 2, false },          // Convolve L/R
                             Case { 8192, false, true, false, fir::eq, 2, false },          // staged convolve
                             Case { 2048, false, false, false, fir::subtract, 1, false });   // mono

    const auto blockSize = GENERATE (1, 64, 1000);

    if (blockSize == 1 && c.size > 2048)
        return;

    INFO ("size " << c.size << ", low latency " << c.lowLatency << ", convolve " << c.convolve << ", Mid/Side " << c.midSide
                  << ", mode " << c.mode << ", channels " << c.numChannels << ", key " << c.key << ", block " << blockSize);

    const auto left = noise (60000, 1), right = noise (60000, 2, 0.05f), key = noise (60000, 3, 0.2f);

    fir::Shared sharedNow, sharedOld;
    prepareShared (sharedNow);
    prepareShared (sharedOld);

    fir::SpectralEngine now ({ c.size, c.lowLatency, c.convolve, c.midSide }, rate, sharedNow);
    fir::legacyW2::SpectralEngine old ({ c.size, c.lowLatency, c.convolve, c.midSide }, rate, sharedOld);
    REQUIRE (now.getLatency() == old.getLatency());

    fir::FrameSettings settingsNow;
    fir::legacyW2::FrameSettings settingsOld;
    settingsNow.mode = settingsOld.mode = c.mode;
    settingsNow.compSlope = settingsOld.compSlope = 0.75;
    settingsNow.compRatio = settingsOld.compRatio = 4.0;
    settingsNow.resDepthDb = settingsOld.resDepthDb = 9.0;
    settingsNow.sidechain = settingsOld.sidechain = c.key;
    settingsNow.source = settingsOld.source = c.mode == fir::subtract ? fir::bothSources : fir::learnedSource;

    const auto a = run (now, left, right, c.key ? &key : nullptr, settingsNow, c.numChannels, blockSize);
    const auto b = run (old, left, right, c.key ? &key : nullptr, settingsOld, c.numChannels, blockSize);

    REQUIRE (a.left == b.left);
    REQUIRE (a.right == b.right);
}

TEST_CASE ("StftFrames: unchanged frames give back the input, delayed by the latency", "[stft][w3]")
{
    const auto lowLatency = GENERATE (false, true);
    const auto size = GENERATE (1024, 2048);

    fancy::dsp::StftFrames frames;
    frames.prepare ({ size, lowLatency, false, 0, 1 });

    std::vector<float> spectrum ((size_t) frames.getSpectrumSize());
    const auto input = noise (20000, 4);
    std::vector<float> output (input.size());

    for (size_t i = 0; i < input.size(); ++i)
    {
        frames.write (0, input[i]);
        frames.advance ([&] (int)
                        {
                            frames.analyse (0, spectrum);
                            frames.synthesise (0, spectrum);
                        },
                        [] {});
        output[i] = frames.read (0);
        frames.clear (0);
    }

    const auto latency = (size_t) frames.getLatency();
    CHECK (latency == (size_t) (lowLatency ? size / 2 : size));

    auto worst = 0.0;

    for (auto i = latency + (size_t) size; i < input.size(); ++i)
        worst = std::max (worst, std::abs ((double) output[i] - (double) input[i - latency]));

    CAPTURE (worst);
    CHECK (worst < 1.0e-5);
}

TEST_CASE ("StftFrames: the paired calls match analyse() and synthesise() on each channel", "[stft][w3]")
{
    const auto lowLatency = GENERATE (false, true);
    const auto size = GENERATE (1024, 2048);

    fancy::dsp::StftFrames single, paired;
    single.prepare ({ size, lowLatency, false, 0, 1 });
    paired.prepare ({ size, lowLatency, false, 0, 1 });

    const auto spectrumSize = (size_t) single.getSpectrumSize();
    std::vector<float> a (spectrumSize), b (spectrumSize), pa (spectrumSize), pb (spectrumSize);
    const auto left = noise (20000, 5), right = noise (20000, 6, 0.3f);
    std::vector<float> singleOut[2] { std::vector<float> (left.size()), std::vector<float> (left.size()) };
    std::vector<float> pairedOut[2] { std::vector<float> (left.size()), std::vector<float> (left.size()) };

    // A different gain per bin and channel, edges included, so the outputs differ from the input.
    const auto shape = [&] (std::vector<float>& spectrum, int channel)
    {
        for (int k = 0; k < single.getBins(); ++k)
        {
            const auto g = 0.5f + 0.5f * std::cos ((float) (k * (channel + 1)) * 0.01f);
            spectrum[(size_t) (2 * k)] *= g;
            spectrum[(size_t) (2 * k + 1)] *= g;
        }
    };

    auto worstSpectrum = 0.0, largest = 0.0;

    for (size_t i = 0; i < left.size(); ++i)
    {
        single.write (0, left[i]);
        single.write (1, right[i]);
        paired.write (0, left[i]);
        paired.write (1, right[i]);

        single.advance ([&] (int)
                        {
                            single.analyse (0, a);
                            single.analyse (1, b);
                            shape (a, 0);
                            shape (b, 1);
                            single.synthesise (0, a);
                            single.synthesise (1, b);
                        },
                        [] {});

        paired.advance ([&] (int)
                        {
                            paired.analysePair (0, 1, pa, pb);
                            shape (pa, 0);
                            shape (pb, 1);

                            for (size_t j = 0; j < (size_t) (2 * single.getBins()); ++j)
                            {
                                worstSpectrum = std::max ({ worstSpectrum, std::abs ((double) pa[j] - (double) a[j]), std::abs ((double) pb[j] - (double) b[j]) });
                                largest = std::max ({ largest, std::abs ((double) a[j]), std::abs ((double) b[j]) });
                            }

                            paired.synthesisePair (0, 1, pa, pb);
                        },
                        [] {});

        for (int ch = 0; ch < 2; ++ch)
        {
            singleOut[ch][i] = single.read (ch);
            single.clear (ch);
            pairedOut[ch][i] = paired.read (ch);
            paired.clear (ch);
        }
    }

    auto worstOutput = 0.0;

    for (int ch = 0; ch < 2; ++ch)
        for (size_t i = 0; i < left.size(); ++i)
            worstOutput = std::max (worstOutput, std::abs ((double) pairedOut[ch][i] - (double) singleOut[ch][i]));

    CAPTURE (worstSpectrum, largest, worstOutput);
    REQUIRE (largest > 1.0);
    CHECK (worstSpectrum < 1.0e-5 * largest);
    CHECK (worstOutput < 1.0e-5);
}
