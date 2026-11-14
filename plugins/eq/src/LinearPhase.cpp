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
        slots[(size_t) i].spectrum.assign ((size_t) (spectrumSize * numPartitions * maxFirs), 0.0f);
        slots[(size_t) i].lengthIndex = -1;
        slots[(size_t) i].routing = Routing::same;
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
    convolveScratch2.assign ((size_t) longestPartition * 4, 0.0f);

    for (auto& gains : designGains)
        gains.assign ((size_t) longest / 2 + 1, 0.0f);
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

void LinearPhase::design (const std::array<BandSpec, maxBands>& bands, int lengthIndex, bool stereoBus) noexcept
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
    std::array<eq::Channel, maxBands> bandChannels;

    auto anyLeftRight = false, anyMidSide = false;

    for (int b = 0; b < maxBands; ++b)
    {
        const auto& spec = bands[(size_t) b];
        const auto channel = stereoBus && spec.on ? spec.channel : eq::Channel::stereo;

        filters[(size_t) b] = eq::design (spec, rate);
        bandChannels[(size_t) b] = channel;

        anyLeftRight = anyLeftRight || channel == eq::Channel::left || channel == eq::Channel::right;
        anyMidSide = anyMidSide || channel == eq::Channel::mid || channel == eq::Channel::side;
    }

    const auto routing = anyLeftRight ? (anyMidSide ? Routing::matrix : Routing::leftRight)
                                      : (anyMidSide ? Routing::midSide : Routing::same);
    const auto numFirs = routing == Routing::same ? 1 : (routing == Routing::matrix ? 4 : 2);

    // Every band's magnitude together, at each bin: one gain per FIR.
    for (int k = 0; k <= half; ++k)
    {
        const auto s = std::sin (std::numbers::pi * (double) k / (double) length);
        const auto sinSquared = s * s;

        if (routing == Routing::same)
        {
            auto power = 1.0;

            for (const auto& filter : filters)
                power *= filter.powerAt (sinSquared);

            designGains[0][(size_t) k] = (float) std::sqrt (power);
        }
        else if (routing != Routing::matrix)
        {
            // Two independent paths: L and R, or M and S. A Stereo band is in both.
            const auto first = routing == Routing::leftRight ? eq::Channel::left : eq::Channel::mid;
            auto powerFirst = 1.0, powerSecond = 1.0;

            for (int b = 0; b < maxBands; ++b)
            {
                const auto power = filters[(size_t) b].powerAt (sinSquared);
                const auto channel = bandChannels[(size_t) b];

                if (channel == eq::Channel::stereo || channel == first)
                    powerFirst *= power;

                if (channel == eq::Channel::stereo || channel != first)
                    powerSecond *= power;
            }

            designGains[0][(size_t) k] = (float) std::sqrt (powerFirst);
            designGains[1][(size_t) k] = (float) std::sqrt (powerSecond);
        }
        else
        {
            // The 2 x 2 matrix from (L, R) in to (L, R) out, each band multiplying from the left
            // in band order.
            double t00 = 1.0, t01 = 0.0, t10 = 0.0, t11 = 1.0;

            for (int b = 0; b < maxBands; ++b)
            {
                const auto h = std::sqrt (filters[(size_t) b].powerAt (sinSquared));

                switch (bandChannels[(size_t) b])
                {
                    case eq::Channel::left:  t00 *= h; t01 *= h; break;
                    case eq::Channel::right: t10 *= h; t11 *= h; break;

                    case eq::Channel::mid:
                    case eq::Channel::side:
                    {
                        // Mid is 1/2 [[h+1, h-1], [h-1, h+1]]; Side has the off-diagonal negated.
                        const auto same = 0.5 * (h + 1.0);
                        const auto cross = bandChannels[(size_t) b] == eq::Channel::mid ? 0.5 * (h - 1.0) : 0.5 * (1.0 - h);

                        const auto n00 = same * t00 + cross * t10, n01 = same * t01 + cross * t11;
                        const auto n10 = cross * t00 + same * t10, n11 = cross * t01 + same * t11;

                        t00 = n00; t01 = n01; t10 = n10; t11 = n11;
                        break;
                    }

                    case eq::Channel::stereo:
                    default: t00 *= h; t01 *= h; t10 *= h; t11 *= h; break;
                }
            }

            designGains[0][(size_t) k] = (float) t00;
            designGains[1][(size_t) k] = (float) t01;
            designGains[2][(size_t) k] = (float) t10;
            designGains[3][(size_t) k] = (float) t11;
        }
    }

    auto& slot = slots[(size_t) target];
    const auto fftSize = blockSize * 2;
    const auto firStride = spectrumSize * numPartitions;

    for (int f = 0; f < numFirs; ++f)
    {
        std::fill (designSpectrum.begin(), designSpectrum.end(), 0.0f);

        for (int k = 0; k <= half; ++k)
            designSpectrum[(size_t) k * 2] = designGains[(size_t) f][(size_t) k];

        designFfts[(size_t) lengthIndex]->performRealOnlyInverseTransform (designSpectrum.data());

        // Zero-phase response, centred on half the length and windowed.
        for (int n = 0; n < length; ++n)
        {
            const auto phase = 2.0 * std::numbers::pi * (double) n / (double) length;
            const auto blackman = 0.42 - 0.5 * std::cos (phase) + 0.08 * std::cos (2.0 * phase);

            designImpulse[(size_t) n] = designSpectrum[(size_t) ((n + half) % length)] * (float) blackman;
        }

        for (int p = 0; p < numPartitions; ++p)
        {
            std::fill (designPartition.begin(), designPartition.begin() + fftSize * 2, 0.0f);
            std::copy_n (designImpulse.begin() + p * blockSize, blockSize, designPartition.begin());

            partitionFfts[(size_t) lengthIndex]->performRealOnlyForwardTransform (designPartition.data());

            std::copy_n (designPartition.begin(), spectrumSize, slot.spectrum.begin() + f * firStride + p * spectrumSize);
        }
    }

    slot.lengthIndex = lengthIndex;
    slot.routing = routing;
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

void LinearPhase::convolve (const Channel& channel, const float* fir, float* result) const noexcept
{
    const auto spectrumSize = (partition + 1) * 2;
    const auto fftSize = partition * 2;

    std::fill (convolveScratch.begin(), convolveScratch.begin() + fftSize * 2, 0.0f);

    auto* sum = convolveScratch.data();

    for (int p = 0; p < numPartitions; ++p)
    {
        const auto historySlot = (historyIndex - p + numPartitions) % numPartitions;
        const auto* x = channel.history.data() + historySlot * spectrumSize;
        const auto* h = fir + p * spectrumSize;

        for (int i = 0; i < spectrumSize; i += 2)
        {
            sum[i] += x[i] * h[i] - x[i + 1] * h[i + 1];
            sum[i + 1] += x[i] * h[i + 1] + x[i + 1] * h[i];
        }
    }

    partitionFfts[(size_t) activeLength]->performRealOnlyInverseTransform (sum);
    std::copy_n (sum + partition, partition, result);
}

// One output of the matrix: L through one FIR plus R through another.
void LinearPhase::convolveBoth (const float* firLeft, const float* firRight, float* result) const noexcept
{
    const auto spectrumSize = (partition + 1) * 2;
    const auto fftSize = partition * 2;

    std::fill (convolveScratch.begin(), convolveScratch.begin() + fftSize * 2, 0.0f);

    auto* sum = convolveScratch.data();

    for (int p = 0; p < numPartitions; ++p)
    {
        const auto historySlot = (historyIndex - p + numPartitions) % numPartitions;
        const auto* l = channels[0].history.data() + historySlot * spectrumSize;
        const auto* r = channels[1].history.data() + historySlot * spectrumSize;
        const auto* hl = firLeft + p * spectrumSize;
        const auto* hr = firRight + p * spectrumSize;

        for (int i = 0; i < spectrumSize; i += 2)
        {
            sum[i] += l[i] * hl[i] - l[i + 1] * hl[i + 1] + r[i] * hr[i] - r[i + 1] * hr[i + 1];
            sum[i + 1] += l[i] * hl[i + 1] + l[i + 1] * hl[i] + r[i] * hr[i + 1] + r[i + 1] * hr[i];
        }
    }

    partitionFfts[(size_t) activeLength]->performRealOnlyInverseTransform (sum);
    std::copy_n (sum + partition, partition, result);
}

// M = (L+R)/2 through one FIR and S = (L-R)/2 through the other, and back to L and R. The sum and
// difference are taken on the spectra, so the history stays in L and R whatever the routing.
void LinearPhase::convolveMidSide (const float* firMid, const float* firSide, float* left, float* right) const noexcept
{
    const auto spectrumSize = (partition + 1) * 2;
    const auto fftSize = partition * 2;

    std::fill (convolveScratch.begin(), convolveScratch.begin() + fftSize * 2, 0.0f);
    std::fill (convolveScratch2.begin(), convolveScratch2.begin() + fftSize * 2, 0.0f);

    auto* mid = convolveScratch.data();
    auto* side = convolveScratch2.data();

    for (int p = 0; p < numPartitions; ++p)
    {
        const auto historySlot = (historyIndex - p + numPartitions) % numPartitions;
        const auto* l = channels[0].history.data() + historySlot * spectrumSize;
        const auto* r = channels[1].history.data() + historySlot * spectrumSize;
        const auto* hm = firMid + p * spectrumSize;
        const auto* hs = firSide + p * spectrumSize;

        for (int i = 0; i < spectrumSize; i += 2)
        {
            const auto mRe = l[i] + r[i], mIm = l[i + 1] + r[i + 1];
            const auto sRe = l[i] - r[i], sIm = l[i + 1] - r[i + 1];

            mid[i] += mRe * hm[i] - mIm * hm[i + 1];
            mid[i + 1] += mRe * hm[i + 1] + mIm * hm[i];
            side[i] += sRe * hs[i] - sIm * hs[i + 1];
            side[i + 1] += sRe * hs[i + 1] + sIm * hs[i];
        }
    }

    partitionFfts[(size_t) activeLength]->performRealOnlyInverseTransform (mid);
    partitionFfts[(size_t) activeLength]->performRealOnlyInverseTransform (side);

    for (int i = 0; i < partition; ++i)
    {
        const auto m = mid[partition + i], s = side[partition + i];

        left[i] = 0.5f * (m + s);
        right[i] = 0.5f * (m - s);
    }
}

void LinearPhase::render (const Slot& slot, int numChannels, bool toPrevious) noexcept
{
    const auto firStride = (partition + 1) * 2 * numPartitions;
    const auto* firs = slot.spectrum.data();

    float* destination[2] { toPrevious ? channels[0].previousOutput.data() : channels[0].output.data(),
                            toPrevious ? channels[1].previousOutput.data() : channels[1].output.data() };

    // A mono bus only ever gets one-FIR designs; if one of another kind is still around from
    // before the bus was known, its first FIR stands in.
    const auto routing = numChannels < 2 ? Routing::same : slot.routing;

    switch (routing)
    {
        case Routing::leftRight:
            convolve (channels[0], firs, destination[0]);
            convolve (channels[1], firs + firStride, destination[1]);
            break;

        case Routing::midSide:
            convolveMidSide (firs, firs + firStride, destination[0], destination[1]);
            break;

        case Routing::matrix:
            convolveBoth (firs, firs + firStride, destination[0]);
            convolveBoth (firs + 2 * firStride, firs + 3 * firStride, destination[1]);
            break;

        case Routing::same:
        default:
            for (int ch = 0; ch < numChannels; ++ch)
                convolve (channels[(size_t) ch], firs, destination[ch]);

            break;
    }
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
            std::fill_n (channel.output.begin(), partition, 0.0f);
    }

    if (front < 0)
        return;

    render (slots[(size_t) front], numChannels, false);

    if (! fading)
        return;

    render (slots[(size_t) fadingFrom], numChannels, true);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& channel = channels[(size_t) ch];

        for (int i = 0; i < partition; ++i)
        {
            const auto amount = ((float) i + 0.5f) / (float) partition;
            channel.output[(size_t) i] = channel.previousOutput[(size_t) i]
                                       + amount * (channel.output[(size_t) i] - channel.previousOutput[(size_t) i]);
        }
    }

    slotBusy[(size_t) fadingFrom].store (false, std::memory_order_release);
    fadingFrom = -1;
    fading = false;
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
