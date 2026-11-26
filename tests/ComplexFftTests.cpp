#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <fancy/dsp/ComplexFft.h>

#include <algorithm>
#include <chrono>
#include <complex>
#include <cstdio>
#include <numbers>
#include <random>
#include <vector>

// fancy::dsp::ComplexFft (docs/eq.md and docs/verb.md, "Ratios to Comp"): its own SSE FFT on
// Windows, juce::dsp::FFT (vDSP) on the Mac.

namespace
{
using Complex = juce::dsp::Complex<float>;

// A double-precision reference: recursive radix 2, unscaled both ways.
void referenceFft (std::vector<std::complex<double>>& x, bool inverse)
{
    const auto n = x.size();

    if (n < 2)
        return;

    std::vector<std::complex<double>> even (n / 2), odd (n / 2);

    for (size_t i = 0; i < n / 2; ++i)
    {
        even[i] = x[2 * i];
        odd[i] = x[2 * i + 1];
    }

    referenceFft (even, inverse);
    referenceFft (odd, inverse);

    const auto sign = inverse ? 1.0 : -1.0;

    for (size_t k = 0; k < n / 2; ++k)
    {
        const auto w = std::polar (1.0, sign * 2.0 * std::numbers::pi * (double) k / (double) n) * odd[k];
        x[k] = even[k] + w;
        x[k + n / 2] = even[k] - w;
    }
}

std::vector<Complex> randomSignal (int size, unsigned seed)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<float> value (-1.0f, 1.0f);
    std::vector<Complex> x ((size_t) size);

    for (auto& z : x)
        z = { value (random), value (random) };

    return x;
}
} // namespace

TEST_CASE ("ComplexFft: forward and inverse match a double-precision FFT at every size used", "[fft][w3]")
{
    const auto order = GENERATE (4, 6, 7, 8, 10, 11, 12, 13, 14);
    const auto inverse = GENERATE (false, true);
    const auto size = 1 << order;

    fancy::dsp::ComplexFft fft;
    fft.prepare (order);
    REQUIRE (fft.getSize() == size);

    const auto input = randomSignal (size, (unsigned) (order * 2 + (inverse ? 1 : 0)));
    std::vector<Complex> output ((size_t) size);
    fft.perform (input.data(), output.data(), inverse);

    std::vector<std::complex<double>> expected (input.begin(), input.end());
    referenceFft (expected, inverse);

    // The inverse's scaling is the engine's (1 or 1 / size): fitted, then the error measured.
    std::complex<double> dot {};
    auto energy = 0.0, largest = 0.0;

    for (size_t i = 0; i < expected.size(); ++i)
    {
        dot += std::complex<double> (output[i]) * std::conj (expected[i]);
        energy += std::norm (expected[i]);
        largest = std::max (largest, std::abs (expected[i]));
    }

    const auto scale = dot.real() / energy;
    CAPTURE (order, inverse, scale);

    if (! inverse)
        CHECK (std::abs (scale - 1.0) < 1.0e-5);
    else
        CHECK ((std::abs (scale - 1.0) < 1.0e-5 || std::abs (scale * (double) size - 1.0) < 1.0e-5));

    auto worst = 0.0;

    for (size_t i = 0; i < expected.size(); ++i)
        worst = std::max (worst, std::abs (std::complex<double> (output[i]) / scale - expected[i]));

    CAPTURE (worst, largest);
    CHECK (worst < 1.0e-5 * largest);

    // The own engine also works in place, with the same result.
    if (fancy::dsp::ComplexFft::ownEngine && order >= fancy::dsp::ComplexFft::minOwnOrder)
    {
        auto inPlace = input;
        fft.perform (inPlace.data(), inPlace.data(), inverse);
        CHECK (inPlace == output);
    }
}

TEST_CASE ("complex fft cpu measurement", "[.measure]")
{
    constexpr int runs = 5, repeats = 4000;

    for (const auto order : { 10, 12 })
    {
        const auto size = 1 << order;
        const auto input = randomSignal (size, 3);
        std::vector<Complex> output ((size_t) size);

        const auto time = [&] (auto& perform)
        {
            std::vector<double> values;

            for (int run = 0; run < runs; ++run)
            {
                const auto start = std::chrono::steady_clock::now();

                for (int i = 0; i < repeats; ++i)
                    perform();

                values.push_back (std::chrono::duration<double, std::micro> (std::chrono::steady_clock::now() - start).count() / repeats);
            }

            std::sort (values.begin(), values.end());
            return values[runs / 2];
        };

        fancy::dsp::ComplexFft own;
        own.prepare (order);
        juce::dsp::FFT juceFft (order);

        auto ownPerform = [&] { own.perform (input.data(), output.data(), true); };
        auto jucePerform = [&] { juceFft.perform (input.data(), output.data(), true); };
        const auto ownMicros = time (ownPerform);
        const auto juceMicros = time (jucePerform);

        std::printf ("complex fft cpu (median of %d, %d points, inverse): ComplexFft %.2f us, juce::dsp::FFT %.2f us (%s)\n",
                     runs, size, ownMicros, juceMicros, fancy::dsp::ComplexFft::ownEngine ? "own SSE engine" : "JUCE's engine");
    }

    SUCCEED();
}
