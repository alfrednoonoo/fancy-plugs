#include "LinearPhase.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace eq
{

namespace
{
constexpr int shortestLength = 2048;
constexpr int shortestOrder = 11;

int rateScale (double sampleRate) noexcept
{
    auto scale = 1;

    while (scale < 8 && sampleRate > 50000.0 * scale)
        scale *= 2;

    return scale;
}
} // namespace

void LinearPhase::prepare (double sampleRate)
{
    rate = sampleRate;

    const auto scale = rateScale (sampleRate);
    baseLength = shortestLength * scale;

    const auto scaleOrder = scale == 8 ? 3 : (scale == 4 ? 2 : (scale == 2 ? 1 : 0));

    for (int i = 0; i < numLengths; ++i)
    {
        const auto order = shortestOrder + scaleOrder + i;

        designFfts[(size_t) i] = std::make_unique<juce::dsp::FFT> (order);
        partitionFfts[(size_t) i] = std::make_unique<juce::dsp::FFT> (order - 3); // length / 8 = 2 partitions
    }

    const auto longest = firLength (numLengths - 1);
    const auto longestPartition = partitionSize (numLengths - 1);
    const auto spectrumSize = (longestPartition + 1) * 2;

    for (int i = 0; i < (int) slots.size(); ++i)
    {
        slots[(size_t) i].spectrum.assign ((size_t) (spectrumSize * numPartitions), 0.0f);
        slots[(size_t) i].lengthIndex = -1;
        slotBusy[(size_t) i].store (false);
    }

    readySlot.store (-1);

    for (auto& channel : channels)
    {
        channel.input.assign ((size_t) longestPartition * 2, 0.0f);
        channel.output.assign ((size_t) longestPartition, 0.0f);
        channel.previousOutput.assign ((size_t) longestPartition, 0.0f);
        channel.history.assign ((size_t) (spectrumSize * numPartitions), 0.0f);
    }

    designSpectrum.assign ((size_t) longest * 2, 0.0f);
    designImpulse.assign ((size_t) longest, 0.0f);
    designPartition.assign ((size_t) longestPartition * 4, 0.0f);
    convolveScratch.assign ((size_t) longestPartition * 4, 0.0f);
    forwardScratch.assign ((size_t) longestPartition * 4, 0.0f);

    front = -1;
    fadingFrom = -1;
    fading = false;
    setLength (0);
}

int LinearPhase::getLatencySamples (int lengthIndex) const noexcept
{
    return firLength (lengthIndex) / 2 + partitionSize (lengthIndex);
}

void LinearPhase::clear() noexcept
{
    for (auto& channel : channels)
    {
        std::fill (channel.input.begin(), channel.input.end(), 0.0f);
        std::fill (channel.output.begin(), channel.output.end(), 0.0f);
        std::fill (channel.previousOutput.begin(), channel.previousOutput.end(), 0.0f);
        std::fill (channel.history.begin(), channel.history.end(), 0.0f);
    }

    position = 0;
    historyIndex = 0;
}

void LinearPhase::setLength (int lengthIndex) noexcept
{
    activeLength = lengthIndex;
    partition = partitionSize (lengthIndex);

    if (front >= 0)
        slotBusy[(size_t) front].store (false, std::memory_order_release);

    if (fading && fadingFrom >= 0)
        slotBusy[(size_t) fadingFrom].store (false, std::memory_order_release);

    front = -1;
    fadingFrom = -1;
    fading = false;

    clear();
}

void LinearPhase::design (const std::array<BandSpec, maxBands>& bands, int lengthIndex) noexcept
{
    auto target = readySlot.exchange (-1, std::memory_order_acq_rel);

    if (target < 0)
    {
        for (int i = 0; i < (int) slots.size(); ++i)
        {
            auto expected = false;

            if (slotBusy[(size_t) i].compare_exchange_strong (expected, true, std::memory_order_acq_rel))
            {
                target = i;
                break;
            }
        }
    }

    if (target < 0)
        return;

    const auto length = firLength (lengthIndex);
    const auto half = length / 2;
    const auto blockSize = partitionSize (lengthIndex);
    const auto spectrumSize = (blockSize + 1) * 2;

    std::array<BandFilter, maxBands> filters;

    for (int b = 0; b < maxBands; ++b)
        filters[(size_t) b] = eq::design (bands[(size_t) b], rate);

    // The magnitude of every band together, at each bin.
    std::fill (designSpectrum.begin(), designSpectrum.end(), 0.0f);

    for (int k = 0; k <= half; ++k)
    {
        const auto s = std::sin (std::numbers::pi * (double) k / (double) length);
        const auto sinSquared = s * s;

        auto power = 1.0;

        for (const auto& filter : filters)
            power *= filter.powerAt (sinSquared);

        designSpectrum[(size_t) k * 2] = (float) std::sqrt (power);
    }

    designFfts[(size_t) lengthIndex]->performRealOnlyInverseTransform (designSpectrum.data());

    // Zero-phase response, centred on half the length and windowed.
    for (int n = 0; n < length; ++n)
    {
        const auto phase = 2.0 * std::numbers::pi * (double) n / (double) length;
        const auto blackman = 0.42 - 0.5 * std::cos (phase) + 0.08 * std::cos (2.0 * phase);

        designImpulse[(size_t) n] = designSpectrum[(size_t) ((n + half) % length)] * (float) blackman;
    }

    auto& slot = slots[(size_t) target];
    const auto fftSize = blockSize * 2;

    for (int p = 0; p < numPartitions; ++p)
    {
        std::fill (designPartition.begin(), designPartition.begin() + fftSize * 2, 0.0f);
        std::copy_n (designImpulse.begin() + p * blockSize, blockSize, designPartition.begin());

        partitionFfts[(size_t) lengthIndex]->performRealOnlyForwardTransform (designPartition.data());

        std::copy_n (designPartition.begin(), spectrumSize, slot.spectrum.begin() + p * spectrumSize);
    }

    slot.lengthIndex = lengthIndex;
    readySlot.store (target, std::memory_order_release);
}

void LinearPhase::takeReadySlot() noexcept
{
    if (fading)
        return;

    const auto ready = readySlot.exchange (-1, std::memory_order_acq_rel);

    if (ready < 0)
        return;

    if (slots[(size_t) ready].lengthIndex != activeLength)
    {
        slotBusy[(size_t) ready].store (false, std::memory_order_release);
        return;
    }

    fadingFrom = front;
    fading = front >= 0;
    front = ready;
}

void LinearPhase::convolve (const Channel& channel, const Slot& slot, float* result) const noexcept
{
    const auto spectrumSize = (partition + 1) * 2;
    const auto fftSize = partition * 2;

    std::fill (convolveScratch.begin(), convolveScratch.begin() + fftSize * 2, 0.0f);

    auto* sum = convolveScratch.data();

    for (int p = 0; p < numPartitions; ++p)
    {
        const auto historySlot = (historyIndex - p + numPartitions) % numPartitions;
        const auto* x = channel.history.data() + historySlot * spectrumSize;
        const auto* h = slot.spectrum.data() + p * spectrumSize;

        for (int i = 0; i < spectrumSize; i += 2)
        {
            sum[i] += x[i] * h[i] - x[i + 1] * h[i + 1];
            sum[i + 1] += x[i] * h[i + 1] + x[i + 1] * h[i];
        }
    }

    partitionFfts[(size_t) activeLength]->performRealOnlyInverseTransform (sum);
    std::copy_n (sum + partition, partition, result);
}

void LinearPhase::processPartition (int numChannels) noexcept
{
    takeReadySlot();

    const auto spectrumSize = (partition + 1) * 2;
    const auto fftSize = partition * 2;

    historyIndex = (historyIndex + 1) % numPartitions;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& channel = channels[(size_t) ch];

        std::copy_n (channel.input.begin(), fftSize, forwardScratch.begin());
        std::fill (forwardScratch.begin() + fftSize, forwardScratch.begin() + fftSize * 2, 0.0f);

        partitionFfts[(size_t) activeLength]->performRealOnlyForwardTransform (forwardScratch.data());

        std::copy_n (forwardScratch.begin(), spectrumSize, channel.history.begin() + historyIndex * spectrumSize);

        // The newest samples become the older half of the next window.
        std::copy_n (channel.input.begin() + partition, partition, channel.input.begin());

        if (front < 0)
        {
            std::fill_n (channel.output.begin(), partition, 0.0f);
            continue;
        }

        convolve (channel, slots[(size_t) front], channel.output.data());

        if (fading)
        {
            convolve (channel, slots[(size_t) fadingFrom], channel.previousOutput.data());

            for (int i = 0; i < partition; ++i)
            {
                const auto amount = ((float) i + 0.5f) / (float) partition;
                channel.output[(size_t) i] = channel.previousOutput[(size_t) i]
                                           + amount * (channel.output[(size_t) i] - channel.previousOutput[(size_t) i]);
            }
        }
    }

    if (fading)
    {
        slotBusy[(size_t) fadingFrom].store (false, std::memory_order_release);
        fadingFrom = -1;
        fading = false;
    }
}

void LinearPhase::process (float* const* signal, int numChannels, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& channel = channels[(size_t) ch];
            auto& sample = signal[ch][i];

            channel.input[(size_t) (partition + position)] = sample;
            sample = channel.output[(size_t) position];
        }

        if (++position == partition)
        {
            position = 0;
            processPartition (numChannels);
        }
    }
}

} // namespace eq
