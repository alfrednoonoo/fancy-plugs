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
// The channels convolve in parallel: left in with left IR, right in with right IR.
class Convolver
{
public:
    enum class Mode { zeroLatency, lowCpu };

    static constexpr int directTaps = 128;
    static constexpr int lowCpuLatency = 4096;

    // irRight may equal irLeft. maxBlock caps the largest partition, at 8192 or 32768.
    Convolver (const float* irLeft, const float* irRight, int irLength, Mode mode, int maxBlock);
    ~Convolver();

    static int latencyOf (Mode mode) noexcept { return mode == Mode::lowCpu ? lowCpuLatency : 0; }

    int getLatency() const noexcept { return latencyOf (convolverMode); }

    void reset() noexcept;

    // Adds to the output buffers. Any block size. With offline set, late background work is
    // waited for, so the output doesn't depend on thread timing.
    void process (const float* inLeft, const float* inRight,
                  float* outLeft, float* outRight, int numSamples, bool offline) noexcept;

    // Blocks that played silence in one stage because its job was late.
    int getLateBlocks() const noexcept { return lateBlocks.load (std::memory_order_relaxed); }

private:
    using Complex = juce::dsp::Complex<float>;

    enum JobState { jobIdle, jobPosted, jobRunning, jobDone };

    struct Stage
    {
        Stage (int blockSize, int irStart, int numPartitions, bool runsOnWorker,
               const float* irLeft, const float* irRight, int irLength);

        void clear() noexcept;
        void transform() noexcept; // consumes jobInput, fills result

        int size;
        int partitions;
        bool background;

        juce::dsp::FFT fft;
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

    void processDirect (const float* inLeft, const float* inRight,
                        float* outLeft, float* outRight, int numSamples) noexcept;
    void finishBlock (Stage& stage, bool offline) noexcept;
    bool collect (Stage& stage, bool offline) noexcept;
    bool runNextJob() noexcept;

    Mode convolverMode;
    int chunkSize = 2048;
    bool hasDirect = false;

    alignas (16) float directLeft[directTaps] {}; // reversed, so the history reads forwards
    alignas (16) float directRight[directTaps] {};
    alignas (16) float historyLeft[directTaps * 2] {};
    alignas (16) float historyRight[directTaps * 2] {};
    int historyPosition = 0;

    std::vector<std::unique_ptr<Stage>> stages; // smallest block first
    int chunkFill = 0;

    std::atomic<std::uint32_t> wakeCount { 0 };
    std::atomic<bool> stopping { false };
    std::atomic<int> lateBlocks { 0 };
    std::unique_ptr<Worker> worker;
};

} // namespace verb
