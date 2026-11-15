#include "Convolver.h"

#include "ConvolverKernels.h"

#include <fancy/dsp/CpuPause.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace verb
{

namespace
{

constexpr double lateSpinSeconds = 200.0e-6;

struct StageDefinition
{
    int size, irStart, partitions; // partitions 0 means the rest of the IR
    bool background;
};

// Each stage starts where the one before ends. docs/verb.md explains why these sizes line up.
constexpr StageDefinition zeroLatencyStages[] {
    { 128, 128, 3, false },
    { 512, 512, 7, false },
    { 2048, 4096, 6, true },
    { 8192, 16384, 6, true },
    { 32768, 65536, 0, true },
};

constexpr StageDefinition lowCpuStages[] {
    { 2048, 0, 6, true },
    { 8192, 12288, 6, true },
    { 32768, 61440, 0, true },
};

using kernels::dot;
using kernels::multiplyAccumulate;

} // namespace

Convolver::Stage::Stage (int blockSize, int irStart, int numPartitions, bool runsOnWorker,
                         const float* irLeft, const float* irRight, const float* crossToLeft, const float* crossToRight, int irLength)
    : size (blockSize),
      partitions (numPartitions),
      background (runsOnWorker),
      fft (juce::roundToInt (std::log2 ((double) blockSize * 2.0)))
{
    const auto bins = (size_t) size + 1;
    const auto span = (size_t) size;

    spectraLeft.resize (bins * (size_t) partitions);
    spectraRight.resize (bins * (size_t) partitions);
    delayLeft.resize (bins * (size_t) partitions);
    delayRight.resize (bins * (size_t) partitions);
    time.resize (span * 2);
    frequency.resize (span * 2);
    previous.resize (span);
    jobInput.resize (span);
    input.resize (span);
    playing.resize (span);
    result.resize (span);

    const auto n = size * 2;

    // Two real IRs in one complex FFT, separated into their spectra.
    const auto transformPair = [&] (const float* first, const float* second, std::vector<Complex>& firstSpectra, std::vector<Complex>& secondSpectra)
    {
        for (int p = 0; p < partitions; ++p)
        {
            const auto start = irStart + p * size;
            const auto count = std::clamp (irLength - start, 0, size);

            std::fill (time.begin(), time.end(), Complex {});

            for (int i = 0; i < count; ++i)
                time[(size_t) i] = { first[start + i], second[start + i] };

            fft.perform (time.data(), frequency.data(), false);

            auto* left = firstSpectra.data() + (size_t) p * bins;
            auto* right = secondSpectra.data() + (size_t) p * bins;

            for (int k = 0; k <= size; ++k)
            {
                const auto z = frequency[(size_t) k];
                const auto mirrored = std::conj (frequency[(size_t) ((n - k) % n)]);

                left[k] = (z + mirrored) * 0.5f;
                right[k] = (z - mirrored) * Complex { 0.0f, -0.5f };
            }
        }
    };

    transformPair (irLeft, irRight, spectraLeft, spectraRight);

    if (crossToLeft != nullptr && crossToRight != nullptr)
    {
        spectraCrossLeft.resize (bins * (size_t) partitions);
        spectraCrossRight.resize (bins * (size_t) partitions);
        transformPair (crossToLeft, crossToRight, spectraCrossLeft, spectraCrossRight);
    }
}

void Convolver::Stage::clear() noexcept
{
    for (auto* buffer : { &delayLeft, &delayRight, &previous, &input, &playing, &result })
        std::fill (buffer->begin(), buffer->end(), Complex {});

    delayPosition = 0;
    filled = 0;
    state.store (jobIdle, std::memory_order_relaxed);
}

void Convolver::Stage::transform() noexcept
{
    const auto n = size * 2;
    const auto bins = size + 1;
    const auto copyBytes = (size_t) size * sizeof (Complex);

    std::memcpy (time.data(), previous.data(), copyBytes);
    std::memcpy (time.data() + size, jobInput.data(), copyBytes);
    previous.swap (jobInput);

    fft.perform (time.data(), frequency.data(), false);

    auto* newestLeft = delayLeft.data() + (size_t) delayPosition * (size_t) bins;
    auto* newestRight = delayRight.data() + (size_t) delayPosition * (size_t) bins;

    for (int k = 0; k <= size; ++k)
    {
        const auto z = frequency[(size_t) k];
        const auto mirrored = std::conj (frequency[(size_t) ((n - k) % n)]);

        newestLeft[k] = (z + mirrored) * 0.5f;
        newestRight[k] = (z - mirrored) * Complex { 0.0f, -0.5f };
    }

    // time and frequency are free from here on, so they hold the two output spectra.
    std::fill (frequency.begin(), frequency.end(), Complex {});
    std::fill (time.begin(), time.begin() + bins, Complex {});

    auto* sumLeft = frequency.data();
    auto* sumRight = time.data();
    auto position = delayPosition;

    const auto cross = ! spectraCrossLeft.empty();

    for (int p = 0; p < partitions; ++p)
    {
        multiplyAccumulate (sumLeft, spectraLeft.data() + (size_t) p * (size_t) bins,
                            delayLeft.data() + (size_t) position * (size_t) bins, bins);
        multiplyAccumulate (sumRight, spectraRight.data() + (size_t) p * (size_t) bins,
                            delayRight.data() + (size_t) position * (size_t) bins, bins);

        // True stereo: right in to left out, and left in to right out.
        if (cross)
        {
            multiplyAccumulate (sumLeft, spectraCrossLeft.data() + (size_t) p * (size_t) bins,
                                delayRight.data() + (size_t) position * (size_t) bins, bins);
            multiplyAccumulate (sumRight, spectraCrossRight.data() + (size_t) p * (size_t) bins,
                                delayLeft.data() + (size_t) position * (size_t) bins, bins);
        }

        position = position == 0 ? partitions - 1 : position - 1;
    }

    delayPosition = (delayPosition + 1) % partitions;

    // Rebuild the two-channel spectrum as L + iR. The mirrored half goes first, because the
    // lower half overwrites sumLeft.
    for (int k = 1; k < size; ++k)
    {
        const auto left = std::conj (sumLeft[k]);
        const auto right = std::conj (sumRight[k]);
        sumLeft[n - k] = left + Complex { -right.imag(), right.real() };
    }

    for (int k = 0; k <= size; ++k)
    {
        const auto right = sumRight[k];
        sumLeft[k] += Complex { -right.imag(), right.real() };
    }

    fft.perform (frequency.data(), time.data(), true);
    std::memcpy (result.data(), time.data() + size, copyBytes);
}

Convolver::Convolver (const float* irLeft, const float* irRight, int irLength, Mode mode, int maxBlock)
    : Convolver (irLeft, irRight, nullptr, nullptr, irLength, mode, maxBlock)
{
}

Convolver::Convolver (const float* irLeft, const float* irRight, const float* crossToLeft, const float* crossToRight,
                      int irLength, Mode mode, int maxBlock)
    : convolverMode (mode),
      trueStereo (crossToLeft != nullptr && crossToRight != nullptr)
{
    const auto length = std::max (irLength, 0);

    if (! trueStereo)
        crossToLeft = crossToRight = nullptr;

    // A stretch of IR that's silent in every path adds nothing but zeros, so its stage (or the
    // direct taps) is left out. Modulate's tails start silent (Phase 3 Q2.8).
    const auto silent = [&] (int start, int count)
    {
        const auto end = std::min (length, start + count);

        for (const auto* path : { irLeft, irRight, crossToLeft, crossToRight })
            if (path != nullptr)
                for (int i = start; i < end; ++i)
                    if (path[i] != 0.0f)
                        return false;

        return true;
    };

    if (mode == Mode::zeroLatency && length > 0 && ! silent (0, directTaps))
    {
        hasDirect = true;

        for (int i = 0; i < std::min (length, (int) directTaps); ++i)
        {
            directLeft[directTaps - 1 - i] = irLeft[i];
            directRight[directTaps - 1 - i] = irRight[i];

            if (trueStereo)
            {
                directCrossLeft[directTaps - 1 - i] = crossToLeft[i];
                directCrossRight[directTaps - 1 - i] = crossToRight[i];
            }
        }
    }

    const auto addStages = [&] (const auto& definitions)
    {
        for (const auto& definition : definitions)
        {
            if (definition.size > maxBlock || definition.irStart >= length)
                break;

            const auto isLast = definition.partitions == 0 || definition.size == maxBlock;
            const auto needed = (length - definition.irStart + definition.size - 1) / definition.size;
            const auto count = isLast ? needed : std::min (definition.partitions, needed);

            if (silent (definition.irStart, count * definition.size))
                continue;

            stages.push_back (std::make_unique<Stage> (definition.size, definition.irStart, count, definition.background,
                                                       irLeft, irRight, crossToLeft, crossToRight, length));
        }
    };

    if (mode == Mode::zeroLatency)
        addStages (zeroLatencyStages);
    else
        addStages (lowCpuStages);

    if (! stages.empty())
        chunkSize = stages.front()->size;

    for (auto& stage : stages)
        stage->clear();

    if (std::any_of (stages.begin(), stages.end(), [] (const auto& stage) { return stage->background; }))
    {
        worker = std::make_unique<Worker> (*this);
        worker->startThread (juce::Thread::Priority::highest);
    }
}

Convolver::~Convolver()
{
    if (worker != nullptr)
    {
        worker->signalThreadShouldExit();
        stopping.store (true, std::memory_order_release);
        wakeCount.fetch_add (1, std::memory_order_release);
        wakeCount.notify_one();
        worker->stopThread (5000);
    }
}

void Convolver::attachTails (std::unique_ptr<Convolver> tailA, std::unique_ptr<Convolver> tailB,
                             double sampleRate, int seed, double rateHz, double depth, double spread)
{
    modulatedTailA = std::move (tailA);
    modulatedTailB = std::move (tailB);

    for (auto& buffer : scratch)
        buffer.assign ((size_t) scratchSize, 0.0f);

    lfoRate = sampleRate;
    lfoSeed = seed;
    smoothing = 1.0 - std::exp (-1.0 / (0.02 * sampleRate));
    driftSmoothing = 1.0 - std::exp (-1.0 / (0.5 * sampleRate));
    rate = rateHz;
    depthTarget = depthNow = depth;
    spreadTarget = spreadNow = spread;
    resetLfo();
}

void Convolver::setModulation (double rateHz, double depth, double spread) noexcept
{
    rate = rateHz;
    depthTarget = depth;
    spreadTarget = spread;
}

void Convolver::resetLfo() noexcept
{
    phase = drift = driftTarget = 0.0;
    driftCountdown = 0;
    depthNow = depthTarget;
    spreadNow = spreadTarget;
    lfoRandom.setSeed ((juce::int64) lfoSeed);
}

void Convolver::reset() noexcept
{
    if (modulatedTailA != nullptr)
    {
        modulatedTailA->reset();
        modulatedTailB->reset();
        resetLfo();
    }

    for (auto& stage : stages)
    {
        auto expected = (int) jobPosted;
        stage->state.compare_exchange_strong (expected, jobIdle, std::memory_order_acq_rel);

        while (stage->state.load (std::memory_order_acquire) == jobRunning)
            fancy::dsp::cpuPause();

        stage->clear();
    }

    std::memset (historyLeft, 0, sizeof (historyLeft));
    std::memset (historyRight, 0, sizeof (historyRight));
    historyPosition = 0;
    chunkFill = 0;
    lateBlocks.store (0, std::memory_order_relaxed);
}

void Convolver::mixTails (const float* inLeft, const float* inRight,
                          float* outLeft, float* outRight, int numSamples, bool offline) noexcept
{
    constexpr double halfPi = 1.5707963267948966;
    constexpr double twoPi = 6.283185307179586;
    constexpr int driftInterval = 256;

    for (int position = 0; position < numSamples;)
    {
        const auto count = std::min (numSamples - position, (int) scratchSize);

        for (auto& buffer : scratch)
            std::fill (buffer.begin(), buffer.begin() + count, 0.0f);

        modulatedTailA->process (inLeft + position, inRight + position, scratch[0].data(), scratch[1].data(), count, offline);
        modulatedTailB->process (inLeft + position, inRight + position, scratch[2].data(), scratch[3].data(), count, offline);

        for (int i = 0; i < count; ++i)
        {
            // A smoothed random walk nudges the speed, so the motion never repeats exactly.
            if (--driftCountdown <= 0)
            {
                driftTarget = lfoRandom.nextDouble() * 2.0 - 1.0;
                driftCountdown = driftInterval;
            }

            drift += (driftTarget - drift) * driftSmoothing;
            depthNow += (depthTarget - depthNow) * smoothing;
            spreadNow += (spreadTarget - spreadNow) * smoothing;

            phase += twoPi * rate * (1.0 + 0.5 * drift) / lfoRate;

            if (phase >= twoPi)
                phase -= twoPi;

            const auto left = depthNow * (0.5 - 0.5 * std::cos (phase)) * halfPi;
            const auto right = depthNow * (0.5 - 0.5 * std::cos (phase + spreadNow * halfPi)) * halfPi;

            const auto index = (size_t) i;
            outLeft[position + i] += (float) (std::cos (left) * scratch[0][index] + std::sin (left) * scratch[2][index]);
            outRight[position + i] += (float) (std::cos (right) * scratch[1][index] + std::sin (right) * scratch[3][index]);
        }

        position += count;
    }
}

void Convolver::processDirect (const float* inLeft, const float* inRight,
                               float* outLeft, float* outRight, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        // Two copies of the history, so the newest directTaps samples are always contiguous.
        historyLeft[historyPosition] = historyLeft[historyPosition + directTaps] = inLeft[i];
        historyRight[historyPosition] = historyRight[historyPosition + directTaps] = inRight[i];
        historyPosition = (historyPosition + 1) % directTaps;

        outLeft[i] += dot (directLeft, historyLeft + historyPosition, directTaps);
        outRight[i] += dot (directRight, historyRight + historyPosition, directTaps);

        if (trueStereo)
        {
            outLeft[i] += dot (directCrossLeft, historyRight + historyPosition, directTaps);
            outRight[i] += dot (directCrossRight, historyLeft + historyPosition, directTaps);
        }
    }
}

// False when the worker is still on the job and the wait ran out. A job nobody has started
// runs here, so a slow worker costs CPU rather than audio.
bool Convolver::collect (Stage& stage, bool offline) noexcept
{
    auto expected = (int) jobPosted;

    if (stage.state.compare_exchange_strong (expected, jobRunning, std::memory_order_acquire))
    {
        takenOverJobs.fetch_add (1, std::memory_order_relaxed);
        stage.transform();
        stage.state.store (jobIdle, std::memory_order_relaxed);
        stage.playing.swap (stage.result);
        return true;
    }

    if (expected == jobRunning)
    {
        const auto limit = juce::Time::getHighResolutionTicks()
                         + (juce::int64) (lateSpinSeconds * (double) juce::Time::getHighResolutionTicksPerSecond());

        while (stage.state.load (std::memory_order_acquire) == jobRunning)
        {
            if (! offline && juce::Time::getHighResolutionTicks() > limit)
                return false;

            fancy::dsp::cpuPause();
        }
    }

    if (stage.state.load (std::memory_order_acquire) == jobDone)
    {
        stage.state.store (jobIdle, std::memory_order_relaxed);
        stage.playing.swap (stage.result);
    }

    return true;
}

void Convolver::finishBlock (Stage& stage, bool offline) noexcept
{
    stage.filled = 0;

    if (! stage.background)
    {
        stage.jobInput.swap (stage.input);
        stage.transform();
        stage.playing.swap (stage.result);
        return;
    }

    if (! collect (stage, offline))
    {
        lateBlocks.fetch_add (1, std::memory_order_relaxed);
        std::fill (stage.playing.begin(), stage.playing.end(), Complex {});
        return;
    }

    stage.jobInput.swap (stage.input);
    stage.state.store (jobPosted, std::memory_order_release);
    wakeCount.fetch_add (1, std::memory_order_release);
    wakeCount.notify_one();
}

void Convolver::process (const float* inLeft, const float* inRight,
                         float* outLeft, float* outRight, int numSamples, bool offline) noexcept
{
    processOwn (inLeft, inRight, outLeft, outRight, numSamples, offline);

    if (modulatedTailA != nullptr)
        mixTails (inLeft, inRight, outLeft, outRight, numSamples, offline);
}

void Convolver::processOwn (const float* inLeft, const float* inRight,
                            float* outLeft, float* outRight, int numSamples, bool offline) noexcept
{
    for (int position = 0; position < numSamples;)
    {
        const auto chunk = std::min (numSamples - position, chunkSize - chunkFill);

        if (hasDirect)
            processDirect (inLeft + position, inRight + position, outLeft + position, outRight + position, chunk);

        for (auto& stagePointer : stages)
        {
            auto& stage = *stagePointer;

            for (int i = 0; i < chunk; ++i)
            {
                stage.input[(size_t) (stage.filled + i)] = { inLeft[position + i], inRight[position + i] };

                const auto played = stage.playing[(size_t) (stage.filled + i)];
                outLeft[position + i] += played.real();
                outRight[position + i] += played.imag();
            }

            stage.filled += chunk;

            if (stage.filled == stage.size)
                finishBlock (stage, offline);
        }

        chunkFill = (chunkFill + chunk) % chunkSize;
        position += chunk;
    }
}

bool Convolver::runNextJob() noexcept
{
    for (auto& stage : stages)
    {
        if (! stage->background)
            continue;

        auto expected = (int) jobPosted;

        if (stage->state.compare_exchange_strong (expected, jobRunning, std::memory_order_acquire))
        {
            stage->transform();
            stage->state.store (jobDone, std::memory_order_release);
            return true;
        }
    }

    return false;
}

void Convolver::Worker::run()
{
    while (! convolver.stopping.load (std::memory_order_acquire))
    {
        const auto seen = convolver.wakeCount.load (std::memory_order_acquire);

        if (! convolver.runNextJob())
            convolver.wakeCount.wait (seen, std::memory_order_acquire);
    }
}

} // namespace verb
