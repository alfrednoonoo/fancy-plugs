#pragma once

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace verb
{

// Non-uniform partitioned stereo convolver (§5.3). The constructor allocates and does the
// partition FFTs, so build it on a worker thread. process() and reset() never allocate.
// The channels convolve in parallel: left in with left IR, right in with right IR. A true-stereo
// convolver (Phase 3 Q2.8) adds the cross paths, right in to left out and left in to right out.
class Convolver
{
public:
    enum class Mode { zeroLatency, lowCpu };

    static constexpr int directTaps = 128;
    static constexpr int lowCpuLatency = 4096;

    // irRight may equal irLeft. maxBlock caps the largest partition, at 8192 or 32768.
    Convolver (const float* irLeft, const float* irRight, int irLength, Mode mode, int maxBlock);

    // True stereo: crossToLeft is right in to left out (RL), crossToRight left in to right out
    // (LR). Both null gives the convolver above, to the bit.
    Convolver (const float* irLeft, const float* irRight, const float* crossToLeft, const float* crossToRight,
               int irLength, Mode mode, int maxBlock);

    bool isTrueStereo() const noexcept { return trueStereo; }

    // Modulate (Phase 3 Q2.8): this convolver plays the early part, and the two tails are
    // crossfaded at equal power by a slow, seeded LFO. Call before the convolver is offered.
    void attachTails (std::unique_ptr<Convolver> tailA, std::unique_ptr<Convolver> tailB,
                      double sampleRate, int seed, double rateHz, double depth, double spread);

    bool isModulated() const noexcept { return modulatedTailA != nullptr; }

    // Audio thread, before process(). Depth and Spread are smoothed over 20 ms.
    void setModulation (double rateHz, double depth, double spread) noexcept;
    ~Convolver();

    static int latencyOf (Mode mode) noexcept { return mode == Mode::lowCpu ? lowCpuLatency : 0; }

    int getLatency() const noexcept { return latencyOf (convolverMode); }

    // Phase 4 Wave 3 (docs/verb.md): the energy the IR passes (the larger of what reaches either
    // output) and its broadband RT60, measured by the builder, for Shimmer's loop gain, its guard
    // and Freeze's release. Set before the convolver is offered.
    void setLoopInfo (double energy, double rt60Seconds) noexcept
    {
        loopEnergy = energy;
        rt60 = rt60Seconds;
    }

    double getLoopEnergy() const noexcept { return loopEnergy; }
    double getRt60() const noexcept { return rt60; }

    void reset() noexcept;

    // Adds to the output buffers. Any block size. With offline set, late background work is
    // waited for, so the output doesn't depend on thread timing.
    void process (const float* inLeft, const float* inRight,
                  float* outLeft, float* outRight, int numSamples, bool offline) noexcept;

    // Blocks that played silence in one stage because its job was late.
    int getLateBlocks() const noexcept
    {
        return lateBlocks.load (std::memory_order_relaxed)
             + (modulatedTailA != nullptr ? modulatedTailA->getLateBlocks() + modulatedTailB->getLateBlocks() : 0);
    }

    // Background jobs the audio thread had to run itself because the worker hadn't started them.
    int getTakenOverJobs() const noexcept { return takenOverJobs.load (std::memory_order_relaxed); }

private:
    using Complex = juce::dsp::Complex<float>;

    enum JobState { jobIdle, jobPosted, jobRunning, jobDone };

    struct Stage
    {
        Stage (int blockSize, int irStart, int numPartitions, bool runsOnWorker,
               const float* irLeft, const float* irRight, const float* crossToLeft, const float* crossToRight, int irLength);

        void clear() noexcept;
        void transform() noexcept; // consumes jobInput, fills result

        int size;
        int partitions;
        bool background;

        juce::dsp::FFT fft;
        std::vector<Complex> spectraCrossLeft, spectraCrossRight; // true stereo only: RL and LR
        std::vector<Complex> spectraLeft, spectraRight; // partitions × (size + 1)
        std::vector<Complex> delayLeft, delayRight;     // the frequency-domain delay line
        std::vector<Complex> time, frequency;           // 2 × size
        std::vector<Complex> previous, jobInput;        // size
        std::vector<Complex> input, playing, result;    // size; L in real, R in imaginary
        int delayPosition = 0;
        int filled = 0;

        std::atomic<int> state { jobIdle };
    };

    class Worker final : public juce::Thread
    {
    public:
        explicit Worker (Convolver& owner) : Thread ("Fancy Verb convolver"), convolver (owner) {}
        void run() override;

    private:
        Convolver& convolver;
    };

    void processOwn (const float* inLeft, const float* inRight,
                     float* outLeft, float* outRight, int numSamples, bool offline) noexcept;
    void mixTails (const float* inLeft, const float* inRight,
                   float* outLeft, float* outRight, int numSamples, bool offline) noexcept;
    void resetLfo() noexcept;

    void processDirect (const float* inLeft, const float* inRight,
                        float* outLeft, float* outRight, int numSamples) noexcept;
    void finishBlock (Stage& stage, bool offline) noexcept;
    bool collect (Stage& stage, bool offline) noexcept;
    bool runNextJob() noexcept;

    Mode convolverMode;
    bool trueStereo = false;
    double loopEnergy = 1.0, rt60 = 1.0;
    int chunkSize = 2048;
    bool hasDirect = false;

    alignas (16) float directLeft[directTaps] {}; // reversed, so the history reads forwards
    alignas (16) float directRight[directTaps] {};
    alignas (16) float directCrossLeft[directTaps] {};  // true stereo: right history to left out
    alignas (16) float directCrossRight[directTaps] {}; // true stereo: left history to right out
    alignas (16) float historyLeft[directTaps * 2] {};
    alignas (16) float historyRight[directTaps * 2] {};
    int historyPosition = 0;

    std::vector<std::unique_ptr<Stage>> stages; // smallest block first
    int chunkFill = 0;

    std::atomic<std::uint32_t> wakeCount { 0 };
    std::atomic<bool> stopping { false };
    std::atomic<int> lateBlocks { 0 };
    std::atomic<int> takenOverJobs { 0 };
    std::unique_ptr<Worker> worker;

    // Modulate (Phase 3 Q2.8).
    static constexpr int scratchSize = 2048;
    std::unique_ptr<Convolver> modulatedTailA, modulatedTailB;
    std::vector<float> scratch[4]; // A left, A right, B left, B right
    double lfoRate = 48000.0, rate = 0.3, depthTarget = 0.5, spreadTarget = 1.0;
    double depthNow = 0.5, spreadNow = 1.0, smoothing = 0.0, driftSmoothing = 0.0;
    double phase = 0.0, drift = 0.0, driftTarget = 0.0;
    int driftCountdown = 0, lfoSeed = 1;
    juce::Random lfoRandom;
};

} // namespace verb
