#pragma once

#include "EqFilters.h"

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace eq
{

// A linear-phase FIR built from the bands' magnitude, run by a uniformly partitioned
// overlap-save convolver.
//
// design() runs on a worker thread, or on the audio thread in an offline render. It fills a
// spare slot and hands it over through an atomic index. process() picks up a finished slot at
// a partition boundary and crossfades to it for one partition.
class LinearPhase
{
public:
    static constexpr int numLengths = 4;
    static constexpr int numPartitions = 16;

    void prepare (double sampleRate);

    // Audio thread. Clears the convolver and waits for a filter of this length.
    void setLength (int lengthIndex) noexcept;
    void clear() noexcept;

    void design (const std::array<BandSpec, maxBands>& bands, int lengthIndex) noexcept;

    // Each channel's signal is replaced by the filtered signal.
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    bool hasFilter() const noexcept { return front >= 0; }
    int getLatencySamples (int lengthIndex) const noexcept;
    int getMaxLatencySamples() const noexcept { return getLatencySamples (numLengths - 1); }

private:
    struct Slot
    {
        std::vector<float> spectrum;
        int lengthIndex = -1;
    };

    struct Channel
    {
        std::vector<float> input, output, previousOutput;
        std::vector<float> history; // numPartitions spectra
    };

    int firLength (int lengthIndex) const noexcept { return baseLength << lengthIndex; }
    int partitionSize (int lengthIndex) const noexcept { return firLength (lengthIndex) / numPartitions; }

    void processPartition (int numChannels) noexcept;
    void takeReadySlot() noexcept;
    void convolve (const Channel& channel, const Slot& slot, float* result) const noexcept;

    double rate = 48000.0;
    int baseLength = 2048;

    std::array<std::unique_ptr<juce::dsp::FFT>, numLengths> partitionFfts, designFfts;

    std::array<Slot, 4> slots;
    std::array<std::atomic<bool>, 4> slotBusy {};
    std::atomic<int> readySlot { -1 };

    std::array<Channel, 2> channels;
    std::vector<float> designSpectrum, designImpulse, designPartition, window;
    mutable std::vector<float> convolveScratch;
    std::vector<float> forwardScratch;

    int activeLength = 0;
    int partition = 256;
    int position = 0;
    int historyIndex = 0;
    int front = -1;
    int fadingFrom = -1;
    bool fading = false;
};

} // namespace eq
