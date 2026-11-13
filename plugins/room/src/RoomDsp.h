#pragma once

#include <fancy/dsp/Biquad.h>
#include <fancy/dsp/Smoother.h>

#include <array>
#include <cstdint>
#include <vector>

// Fancy Room's engine: a transcription of docs/research/room-model.py, with the same constants
// and the same order of operations (docs/room.md). Per sample:
//   input -> history (Pre-delay) -> early reflections ------------------------------+
//                                -> 4 all-passes per channel -> delay network ------+-> Low cut
//   -> High cut -> Width -> Ducking -> Wet, plus Dry -> Output, with the host's bypass.
//
// Every setting is applied per sample, and what's recomputed every 16 samples (modulation,
// line gains, filters) is on a grid of the absolute sample count, so block sizes never change
// the output.
class RoomDsp
{
public:
    enum Quality { eco, normal, high };

    // The defaults are the model's: wet only, at 0 dB. The processor sets Wet and Dry from its
    // parameters.
    struct Targets
    {
        double predelayMs = 12.0;
        double size = 0.55;        // 0 to 1
        double decay = 1.8;        // RT60, seconds
        double diffusion = 0.70;   // 0 to 1
        double mod = 0.25;         // 0 to 1
        double width = 1.0;        // -1 to 2
        double damping = 0.45;     // 0 to 1
        double lowCutHz = 150.0;
        double highCutHz = 9000.0;
        bool freeze = false;
        double ducking = 0.0;      // 0 to 1
        int quality = normal;
        double erGain = 1.0;
        bool monoSum = false;
        double wetGain = 1.0;
        double dryGain = 0.0;
        double outputGain = 1.0;
        bool bypass = false;
    };

    static constexpr int maxLines = 16;
    static constexpr int erTaps = 16;
    static constexpr int modStep = 16;
    static constexpr double maxPredelayMs = 250.0;
    static constexpr double maxRate = 384000.0;

    RoomDsp();

    void prepare (double sampleRate);
    void reset();

    void setTargets (const Targets& targets);

    // Jumps straight to the targets, for use before processing starts.
    void snapToTargets (const Targets& targets);

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    // Decision 3: Pre-delay + the longest reflection + 1.5 x Decay, which is where the tail is
    // 90 dB down, with about 10 dB to spare (docs/room.md).
    static double tailSeconds (double predelayMs, double size, double decay) noexcept;
    static double longestReflectionSeconds (double size) noexcept;

    // Size 0 to 1 as the factor on every delay length.
    static double sizeScale (double size) noexcept { return 0.25 + 0.75 * size; }

    // How many networks are running: 2 for up to 2 s after a Quality change (decision 4).
    int getNumRunningNetworks() const noexcept;
    int getActiveQuality() const noexcept;

    // One early reflection: its time at Size 100 % and its gain (the model's er_table).
    struct Tap
    {
        double ms = 0.0, gain = 0.0;
    };

    static Tap reflection (int channel, int index) noexcept;

private:
    struct Network
    {
        bool running = false, ringing = false, cubic = false;
        int numLines = 16;
        int mask = 0;
        int quality = normal;
        double inGain = 0.0, outGain = 0.0, hScale = 0.0;
        double ringFade = 1.0;
        int64_t ringSamples = 0;
        int quietSamples = 0;

        std::array<std::vector<float>, maxLines> lines;
        std::array<double, maxLines> lineMs {}, length {}, rate {}, phase {}, sign {}, g {}, bco {}, lp {}, v {}, m0 {}, m1 {};
    };

    void configure (Network& network, int quality);
    void clear (Network& network);
    void updateLengths (bool sizeRounded, bool predelayRounded) noexcept;
    void updateGains (Network& network) noexcept;
    void updateModulation (Network& network, int64_t knot) noexcept;
    void updateFilters() noexcept;
    void runKnot() noexcept;
    double runNetwork (Network& network, const double* diffused, double f, double t, double depth, double& lateRight) noexcept;
    void switchQuality (int quality);

    double sampleRate = 48000.0;
    bool prepared = false;
    Targets targets;

    // History of the input, stereo-interleaved, for Pre-delay and the reflections.
    std::vector<float> history;
    int historyMask = 0;

    std::array<std::array<Tap, erTaps>, 2> reflections {};
    std::array<std::array<double, erTaps>, 2> reflectionOffset {};

    std::array<std::array<std::vector<float>, 4>, 2> allpass;
    std::array<std::array<double, 4>, 2> allpassLength {};
    int allpassMask = 0;

    std::array<Network, 2> networks;
    int activeNetwork = 0;

    std::array<fancy::dsp::Biquad, 2> lowCut, highCut;
    double lastLowLog = 0.0, lastHighLog = 0.0;

    int64_t sampleCount = 0;
    int64_t knotStart = 0;

    double predelaySamples = 0.0;
    double lastSize = -1.0, lastPredelay = -1.0;
    double lastDecay = -1.0, lastDamping = -1.0, gainsForScale = -1.0;
    double freezeAmount = 0.0;
    double freezeStep = 0.0;
    double duckEnvelope = 0.0, duckAttack = 0.0, duckRelease = 0.0; // the reduction in dB, and its smoothing
    double duckPeak = 0.0, duckPeakRelease = 0.0;
    int freezeRampSamples = 960;

    fancy::dsp::Smoother predelay, size, decay, diffusion, mod, width, damping, logLowCut, logHighCut,
        erGain, ducking, wet, dry, output, bypass;
};
