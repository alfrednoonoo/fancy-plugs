#pragma once

#include "Crossover.h"

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace multicomp
{

// Complementary linear-phase bands, run by a uniformly partitioned overlap-save convolver.
//
// design() runs on a worker thread, or on the audio thread in an offline render. It fills a
// spare slot and hands it over through an atomic index. process() picks up a finished slot at
// a partition boundary and crossfades to it for one partition.
class LinearCrossover
{
public:
    static constexpr int numPartitions = 8;
    static constexpr int numChannels = 4; // main L and R, sidechain L and R

    void prepare (double sampleRate);

    // Audio thread. Clears the convolver and waits for a filter with this many bands.
    void setBandCount (int bands) noexcept;

    // crossovers holds bands - 1 sanitised frequencies.
    void design (const double* crossovers, int bands) noexcept;

    // Writes activeBands bands, each channel at (band * 2 + channel) * stride. The sidechain
    // is only filtered when keyBands isn't null.
    void process (const float* left, const float* right, const float* keyLeft, const float* keyRight,
                  double* bands, double* keyBands, int stride, int numSamples) noexcept;

    bool hasFilter() const noexcept { return front >= 0; }
    int getLatencySamples() const noexcept { return partition + (length - 1) / 2; }

private:
    struct Slot
    {
        std::vector<float> spectrum;
        int bands = 0;
    };

    struct Channel
    {
        std::vector<float> input, output;
        std::vector<float> history; // numPartitions spectra
    };

    void processPartition (bool withKey) noexcept;
    void takeReadySlot() noexcept;
    void convolve (const Channel& channel, const Slot& slot, int band, float* result) noexcept;
    void storeBand (Slot& slot, int band) noexcept;
    float* outputOf (Channel& channel, int band) noexcept { return channel.output.data() + band * partition; }

    int partition = 512, length = 4095, spectrumSize = 1026, fftSize = 1024;
    double rate = 48000.0;

    std::unique_ptr<juce::dsp::FFT> fft;

    std::array<Slot, 4> slots;
    std::array<std::atomic<bool>, 4> slotBusy {};
    std::atomic<int> readySlot { -1 };

    std::array<Channel, numChannels> channels;
    std::vector<double> kaiserWindow, previousLow, currentLow, impulse;
    std::vector<float> designScratch, convolveScratch, forwardScratch, previousOutput;

    int activeBands = 4;
    int position = 0;
    int historyIndex = 0;
    int front = -1;
    int fadingFrom = -1;
    bool fading = false;
};

} // namespace multicomp
