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

    // With a stereo bus each band's Channel counts (Phase 3 Q2.4); a mono bus treats every band
    // as Stereo.
    void design (const std::array<BandSpec, maxBands>& bands, int lengthIndex, bool stereoBus = true) noexcept;

    // Each channel's signal is replaced by the filtered signal.
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    bool hasFilter() const noexcept { return front >= 0; }
    int getLatencySamples (int lengthIndex) const noexcept;
    int getMaxLatencySamples() const noexcept { return getLatencySamples (numLengths - 1); }

private:
    // How a design's FIRs connect the two channels (Phase 3 Q2.4). Each band's magnitude is real,
    // so the whole EQ is a real 2 x 2 matrix at every frequency, and only its distinct entries
    // need an FIR:
    //   same       one FIR on both channels: every band is Stereo
    //   leftRight  FIR 0 on L and FIR 1 on R: Stereo, Left and Right bands
    //   midSide    FIR 0 on M and FIR 1 on S: Stereo, Mid and Side bands
    //   matrix     FIRs 0-3 are L to L, R to L, L to R and R to R: both kinds mixed
    enum class Routing { same, leftRight, midSide, matrix };

    static constexpr int maxFirs = 4;

    struct Slot
    {
        std::vector<float> spectrum; // maxFirs FIRs, each numPartitions spectra
        int lengthIndex = -1;
        Routing routing = Routing::same;
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
    void render (const Slot& slot, int numChannels, bool toPrevious) noexcept;
    void convolve (const Channel& channel, const float* fir, float* result) const noexcept;
    void convolveBoth (const float* firLeft, const float* firRight, float* result) const noexcept;
    void convolveMidSide (const float* firMid, const float* firSide, float* left, float* right) const noexcept;

    double rate = 48000.0;
    int baseLength = 2048;

    std::array<std::unique_ptr<juce::dsp::FFT>, numLengths> partitionFfts, designFfts;

    std::array<Slot, 4> slots;
    std::array<std::atomic<bool>, 4> slotBusy {};
    std::atomic<int> readySlot { -1 };

    std::array<Channel, 2> channels;
    std::vector<float> designSpectrum, designImpulse, designPartition, window;
    std::array<std::vector<float>, maxFirs> designGains;
    mutable std::vector<float> convolveScratch, convolveScratch2;
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
