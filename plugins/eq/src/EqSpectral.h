#pragma once

#include "EqFilters.h"

#include <fancy/dsp/StftFrames.h>

#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace eq
{

// Phase 4 Wave 3 (docs/eq.md, "spectral dynamics"): a spectral band's dynamics, per FFT bin.
//
// One STFT of the input (1024 points at 44.1 and 48 kHz, doubling with the rate, square-root
// Hann both ways at a quarter-frame hop: fancy::dsp::StftFrames). Each spectral band reads each
// bin's level (the three bins round it, calibrated so a sine reads its RMS), runs the band's gain
// computer (Threshold, Range, Reach, the 6 dB knee, Above or Below) with its own Attack and
// Release per bin, and weights the change by the band's shape: 1 where the band's curve is
// deepest, 0 where it's flat. The changes of every spectral band are summed per bin in dB, in
// the band's channel (L, R, both, or M and S formed per bin), and applied once.
//
// The output is the input delayed by getLatency(), changed only where a band acts. prepare()
// allocates; process() doesn't.
class SpectralStage
{
public:
    static constexpr int maxChannels = 2;
    static constexpr int curvePoints = 512;

    static int frameLengthFor (double sampleRate) noexcept
    {
        auto factor = 1;

        while (sampleRate / 48000.0 >= 1.5 * (double) factor)
            factor *= 2;

        return 1024 * factor;
    }

    // The graph's points, 20 Hz to 20 kHz in equal steps of log frequency.
    static double curveHz (int point) noexcept { return 20.0 * std::pow (1000.0, (double) point / (double) (curvePoints - 1)); }

    void prepare (double sampleRate);
    void reset() noexcept;

    int getLatency() const noexcept { return frames.getLatency(); }

    // Once per block: the bands, their dynamics, and which of them run here.
    void setBands (const std::array<BandSpec, maxBands>& bands, const std::array<DynamicSpec, maxBands>& dynamics,
                   const std::array<bool, maxBands>& spectral) noexcept;

    // In place: the output is the input delayed by the latency. sidechain may be null.
    void process (float* const* channels, int numChannels, int numSamples, const float* const* sidechain, int numSidechain) noexcept;

    // The band's change at its deepest bin now, in dB (signed), for the graph's "now".
    double getDeepest (int band) const noexcept { return bandStates[(size_t) band].deepest; }

    // The summed change at a graph point, in dB, as of the last frame.
    float getCurveDb (int point) const noexcept { return curve[(size_t) point].load (std::memory_order_relaxed); }

private:
    struct BandState
    {
        BandSpec shape;               // the spec the weights were made for (gain and channel left out)
        bool shaped = false;
        bool running = false;         // spectral, or still letting go
        std::vector<float> weight, change;
        int first = 0, last = -1;     // the bins with any weight
        double deepest = 0.0;
    };

    void runFrame() noexcept;
    void updateWeights (int band) noexcept;
    const std::vector<float>& levelsFor (bool key, int domain) noexcept;

    fancy::dsp::StftFrames frames;
    double rate = 48000.0;
    int bins = 513, hop = 256;
    double calibration = 1.0;

    std::array<BandSpec, maxBands> bandSpecs {};
    std::array<DynamicSpec, maxBands> bandDynamics {};
    std::array<bool, maxBands> spectralBands {};
    std::array<BandState, maxBands> bandStates;

    // The frame in progress.
    int frameChannels = 2;
    bool frameKey = false;
    std::array<std::vector<float>, 4> spectra; // main L, R, key L, R
    std::vector<float> power;
    std::array<std::array<std::vector<float>, 4>, 2> levels; // [main, key][L, R, M, S]
    std::array<std::array<bool, 4>, 2> levelsReady {};
    std::array<std::vector<float>, 4> gainDb; // L, R, M, S

    std::vector<int> curveBin;
    std::vector<float> curveFraction;
    std::array<std::atomic<float>, curvePoints> curve {};
};

} // namespace eq
