#include <fancy/dsp/Analyzer.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fancy::dsp
{

namespace
{
constexpr float floorDb = -200.0f;
constexpr double pinkDbPerOctave = 4.5;
constexpr double pivotHz = 1000.0;
constexpr double maxSmoothingSeconds = 1.0;

double windowValue (SpectrumAnalyzer::Window window, int n, int size) noexcept
{
    const auto phase = 2.0 * std::numbers::pi * (double) n / (double) size;

    if (window == SpectrumAnalyzer::Window::hann)
        return 0.5 - 0.5 * std::cos (phase);

    return 0.35875 - 0.48829 * std::cos (phase) + 0.14128 * std::cos (2.0 * phase) - 0.01168 * std::cos (3.0 * phase);
}
} // namespace

void SpectrumAnalyzer::configure (double sampleRate, const Settings& settings)
{
    const auto rebuild = ! configured || sampleRate != rate || settings.size != current.size || settings.window != current.window;

    rate = sampleRate;
    current = settings;
    configured = true;

    const auto hopSeconds = (double) (settings.size / settings.hopDivisor) / sampleRate;
    const auto timeConstant = settings.smoothing * maxSmoothingSeconds;

    smoothingCoefficient = timeConstant > 0.0 ? std::exp (-hopSeconds / timeConstant) : 0.0;

    if (! rebuild)
        return;

    const auto fftSize = settings.size * 2; // zero padded, which keeps scalloping small

    fft = std::make_unique<juce::dsp::FFT> (juce::roundToInt (std::log2 ((double) fftSize)));

    windowTable.resize ((size_t) settings.size);

    auto sum = 0.0;

    for (int n = 0; n < settings.size; ++n)
    {
        const auto value = windowValue (settings.window, n, settings.size);
        windowTable[(size_t) n] = (float) value;
        sum += value;
    }

    gainScale = (float) (2.0 / sum);

    input.assign ((size_t) settings.size, 0.0f);
    frame.assign ((size_t) fftSize * 2, 0.0f);
    levels.assign ((size_t) settings.size + 1, floorDb);
    peaks.assign ((size_t) settings.size + 1, floorDb);
    inputFill = 0;
}

void SpectrumAnalyzer::addSamples (const float* samples, int numSamples)
{
    const auto hop = current.size / current.hopDivisor;

    for (int done = 0; done < numSamples;)
    {
        const auto count = std::min (numSamples - done, current.size - inputFill);

        std::copy_n (samples + done, count, input.begin() + inputFill);
        inputFill += count;
        done += count;

        if (inputFill == current.size)
        {
            analyse();

            std::copy (input.begin() + hop, input.end(), input.begin());
            inputFill = current.size - hop;
        }
    }
}

void SpectrumAnalyzer::analyse()
{
    std::fill (frame.begin(), frame.end(), 0.0f);

    for (int n = 0; n < current.size; ++n)
        frame[(size_t) n] = input[(size_t) n] * windowTable[(size_t) n];

    fft->performRealOnlyForwardTransform (frame.data(), true);

    const auto coefficient = (float) smoothingCoefficient;

    for (int bin = 0; bin <= current.size; ++bin)
    {
        const auto re = frame[(size_t) bin * 2], im = frame[(size_t) bin * 2 + 1];
        const auto amplitude = std::sqrt (re * re + im * im) * gainScale;

        auto db = juce::jmax (floorDb, 20.0f * std::log10 (juce::jmax (amplitude, 1.0e-12f)));

        if (current.pinkTilt && bin > 0)
        {
            const auto hz = (double) bin * rate / (double) (current.size * 2);
            db += (float) (pinkDbPerOctave * std::log2 (hz / pivotHz));
        }

        auto& level = levels[(size_t) bin];

        level = level <= floorDb ? db : db + coefficient * (level - db);
        peaks[(size_t) bin] = std::max (peaks[(size_t) bin], level);
    }
}

float SpectrumAnalyzer::interpolate (const std::vector<float>& values, double hz) const noexcept
{
    if (values.empty())
        return floorDb;

    const auto position = std::clamp (hz * (double) (current.size * 2) / rate, 0.0, (double) (values.size() - 1));
    const auto index = std::min ((int) position, (int) values.size() - 2);
    const auto fraction = (float) (position - (double) index);

    return values[(size_t) index] + fraction * (values[(size_t) index + 1] - values[(size_t) index]);
}

float SpectrumAnalyzer::levelAt (double hz) const noexcept
{
    return interpolate (levels, hz);
}

float SpectrumAnalyzer::peakAt (double hz) const noexcept
{
    return interpolate (peaks, hz);
}

float SpectrumAnalyzer::highestBetween (const std::vector<float>& values, double lowHz, double highHz) const noexcept
{
    const auto scale = (double) (current.size * 2) / rate;
    const auto low = (int) std::ceil (lowHz * scale);
    const auto high = (int) std::floor (highHz * scale);

    if (values.empty() || high < low)
        return interpolate (values, 0.5 * (lowHz + highHz));

    auto highest = floorDb;

    for (int bin = std::max (low, 0); bin <= std::min (high, (int) values.size() - 1); ++bin)
        highest = std::max (highest, values[(size_t) bin]);

    return highest;
}

float SpectrumAnalyzer::levelBetween (double lowHz, double highHz) const noexcept
{
    return highestBetween (levels, lowHz, highHz);
}

float SpectrumAnalyzer::peakBetween (double lowHz, double highHz) const noexcept
{
    return highestBetween (peaks, lowHz, highHz);
}

void SpectrumAnalyzer::resetPeaks() noexcept
{
    std::fill (peaks.begin(), peaks.end(), floorDb);
}

} // namespace fancy::dsp
