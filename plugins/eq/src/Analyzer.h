#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace eq
{

// Single-producer, single-consumer samples from the audio thread to the analyzer thread.
class SampleFifo
{
public:
    static constexpr int capacity = 1 << 17;

    void push (const float* const* channels, int numChannels, int numSamples) noexcept
    {
        auto write = writeIndex.load (std::memory_order_relaxed);
        const auto read = readIndex.load (std::memory_order_acquire);

        if ((int) (write - read) + numSamples > capacity || numChannels <= 0)
            return;

        const auto scale = 1.0f / (float) numChannels;

        for (int i = 0; i < numSamples; ++i)
        {
            auto sum = 0.0f;

            for (int ch = 0; ch < numChannels; ++ch)
                sum += channels[ch][i];

            buffer[write++ & (capacity - 1)] = sum * scale;
        }

        writeIndex.store (write, std::memory_order_release);
    }

    int pop (float* destination, int maxSamples) noexcept
    {
        auto read = readIndex.load (std::memory_order_relaxed);
        const auto available = (int) (writeIndex.load (std::memory_order_acquire) - read);
        const auto count = available < maxSamples ? available : maxSamples;

        for (int i = 0; i < count; ++i)
            destination[i] = buffer[read++ & (capacity - 1)];

        readIndex.store (read, std::memory_order_release);
        return count;
    }

private:
    std::vector<float> buffer = std::vector<float> ((size_t) capacity, 0.0f);
    std::atomic<unsigned int> writeIndex { 0 }, readIndex { 0 };
};

// Calibrated spectrum: a sine at amplitude A reads 20 log10 (A) dB at its frequency.
class SpectrumAnalyzer
{
public:
    enum class Window { hann, blackmanHarris };

    struct Settings
    {
        int size = 8192;
        Window window = Window::blackmanHarris;
        int hopDivisor = 4;     // 2 for 50 % overlap, 4 for 75 %
        double smoothing = 0.5; // 0 to 1
        bool pinkTilt = false;

        bool operator== (const Settings&) const = default;
    };

    // Allocates when the size or window changes, so it runs off the audio thread only.
    void configure (double sampleRate, const Settings& settings);

    void addSamples (const float* samples, int numSamples);

    // dB at a frequency, from the latest spectrum.
    float levelAt (double hz) const noexcept;

    // Highest reading since the last reset, at a frequency.
    float peakAt (double hz) const noexcept;

    // Highest value between two frequencies, so a pixel wider than a bin doesn't hide a peak.
    float levelBetween (double lowHz, double highHz) const noexcept;
    float peakBetween (double lowHz, double highHz) const noexcept;

    void resetPeaks() noexcept;

    int getNumBins() const noexcept { return (int) levels.size(); }

private:
    void analyse();
    float interpolate (const std::vector<float>& values, double hz) const noexcept;
    float highestBetween (const std::vector<float>& values, double lowHz, double highHz) const noexcept;

    double rate = 48000.0;
    Settings current;
    bool configured = false;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> windowTable, input, frame;
    std::vector<float> levels, peaks;
    float gainScale = 1.0f;
    int inputFill = 0;
    double smoothingCoefficient = 0.0;
};

} // namespace eq
