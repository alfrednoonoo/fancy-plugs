#include <fancy/dsp/LinearCrossover.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fancy::dsp
{

namespace
{
constexpr int basePartition = 512;
constexpr double kaiserBeta = 10.06; // about -100 dB in the stopband

// The zeroth-order modified Bessel function, by its power series.
double besselI0 (double x) noexcept
{
    auto sum = 1.0, term = 1.0;

    for (int k = 1; k < 64; ++k)
    {
        term *= (x * 0.5 / (double) k) * (x * 0.5 / (double) k);
        sum += term;

        if (term < 1.0e-16 * sum)
            break;
    }

    return sum;
}
} // namespace

void LinearCrossover::prepare (double sampleRate)
{
    rate = sampleRate;

    // The same time span at every sample rate, in whole doublings.
    auto scale = 1;

    while (rate / (double) scale > 72000.0)
        scale *= 2;

    partition = basePartition * scale;
    length = partition * numPartitions - 1;
    fftSize = partition * 2;
    spectrumSize = (partition + 1) * 2;

    fft = std::make_unique<juce::dsp::FFT> (juce::roundToInt (std::log2 ((double) fftSize)));

    const auto centre = 0.5 * (double) (length - 1);

    kaiserWindow.resize ((size_t) length);

    for (int n = 0; n < length; ++n)
    {
        const auto x = ((double) n - centre) / centre;
        kaiserWindow[(size_t) n] = besselI0 (kaiserBeta * std::sqrt (std::max (0.0, 1.0 - x * x))) / besselI0 (kaiserBeta);
    }

    previousLow.assign ((size_t) length, 0.0);
    currentLow.assign ((size_t) length, 0.0);
    impulse.assign ((size_t) length, 0.0);

    designScratch.assign ((size_t) fftSize * 2, 0.0f);
    convolveScratch.assign ((size_t) fftSize * 2, 0.0f);
    forwardScratch.assign ((size_t) fftSize * 2, 0.0f);
    previousOutput.assign ((size_t) partition, 0.0f);

    for (auto& slot : slots)
    {
        slot.spectrum.assign ((size_t) (spectrumSize * numPartitions * maxBands), 0.0f);
        slot.bands = 0;
    }

    for (auto& busy : slotBusy)
        busy.store (false);

    readySlot.store (-1);

    for (auto& channel : channels)
    {
        channel.input.assign ((size_t) fftSize, 0.0f);
        channel.output.assign ((size_t) (partition * maxBands), 0.0f);
        channel.history.assign ((size_t) (spectrumSize * numPartitions), 0.0f);
    }

    position = 0;
    historyIndex = 0;
    front = -1;
    fadingFrom = -1;
    fading = false;
}

void LinearCrossover::setBandCount (int bands) noexcept
{
    activeBands = bands;

    for (auto& channel : channels)
    {
        std::fill (channel.input.begin(), channel.input.end(), 0.0f);
        std::fill (channel.output.begin(), channel.output.end(), 0.0f);
        std::fill (channel.history.begin(), channel.history.end(), 0.0f);
    }

    position = 0;

    if (front >= 0)
        slotBusy[(size_t) front].store (false, std::memory_order_release);

    if (fadingFrom >= 0)
        slotBusy[(size_t) fadingFrom].store (false, std::memory_order_release);

    front = -1;
    fadingFrom = -1;
    fading = false;
}

void LinearCrossover::storeBand (Slot& slot, int band) noexcept
{
    const auto base = band * numPartitions * spectrumSize;

    for (int p = 0; p < numPartitions; ++p)
    {
        std::fill (designScratch.begin(), designScratch.end(), 0.0f);

        for (int n = 0; n < partition; ++n)
        {
            const auto index = p * partition + n;

            if (index < length)
                designScratch[(size_t) n] = (float) impulse[(size_t) index];
        }

        fft->performRealOnlyForwardTransform (designScratch.data());

        std::copy_n (designScratch.begin(), spectrumSize, slot.spectrum.begin() + base + p * spectrumSize);
    }
}

void LinearCrossover::design (const double* crossovers, int bands) noexcept
{
    auto target = -1;

    for (int i = 0; i < (int) slots.size() && target < 0; ++i)
    {
        auto expected = false;

        if (slotBusy[(size_t) i].compare_exchange_strong (expected, true, std::memory_order_acq_rel))
            target = i;
    }

    if (target < 0)
        return;

    auto& slot = slots[(size_t) target];
    const auto centre = (length - 1) / 2;

    std::fill (previousLow.begin(), previousLow.end(), 0.0);

    for (int band = 0; band < bands; ++band)
    {
        if (band < bands - 1)
        {
            const auto cutoff = crossovers[band] / rate;

            auto sum = 0.0;

            for (int n = 0; n < length; ++n)
            {
                const auto x = (double) (n - centre);
                const auto sinc = n == centre ? 2.0 * cutoff
                                              : std::sin (2.0 * std::numbers::pi * cutoff * x) / (std::numbers::pi * x);

                currentLow[(size_t) n] = sinc * kaiserWindow[(size_t) n];
                sum += currentLow[(size_t) n];
            }

            for (auto& value : currentLow)
                value /= sum;
        }
        else
        {
            // The whole signal, so that the bands add up to a delta.
            std::fill (currentLow.begin(), currentLow.end(), 0.0);
            currentLow[(size_t) centre] = 1.0;
        }

        for (int n = 0; n < length; ++n)
            impulse[(size_t) n] = currentLow[(size_t) n] - previousLow[(size_t) n];

        storeBand (slot, band);
        std::copy (currentLow.begin(), currentLow.end(), previousLow.begin());
    }

    slot.bands = bands;

    const auto replaced = readySlot.exchange (target, std::memory_order_acq_rel);

    if (replaced >= 0)
        slotBusy[(size_t) replaced].store (false, std::memory_order_release);
}

void LinearCrossover::takeReadySlot() noexcept
{
    const auto ready = readySlot.exchange (-1, std::memory_order_acq_rel);

    if (ready < 0)
        return;

    if (slots[(size_t) ready].bands != activeBands)
    {
        slotBusy[(size_t) ready].store (false, std::memory_order_release);
        return;
    }

    fadingFrom = front;
    fading = front >= 0;
    front = ready;
}

void LinearCrossover::convolve (const Channel& channel, const Slot& slot, int band, float* result) noexcept
{
    std::fill (convolveScratch.begin(), convolveScratch.end(), 0.0f);

    auto* sum = convolveScratch.data();
    const auto* bandSpectrum = slot.spectrum.data() + band * numPartitions * spectrumSize;

    for (int p = 0; p < numPartitions; ++p)
    {
        const auto historySlot = (historyIndex - p + numPartitions) % numPartitions;
        const auto* x = channel.history.data() + historySlot * spectrumSize;
        const auto* h = bandSpectrum + p * spectrumSize;

        for (int i = 0; i < spectrumSize; i += 2)
        {
            sum[i] += x[i] * h[i] - x[i + 1] * h[i + 1];
            sum[i + 1] += x[i] * h[i + 1] + x[i + 1] * h[i];
        }
    }

    fft->performRealOnlyInverseTransform (sum);
    std::copy_n (sum + partition, partition, result);
}

void LinearCrossover::processPartition (bool withKey) noexcept
{
    takeReadySlot();

    historyIndex = (historyIndex + 1) % numPartitions;

    const auto channelsToRun = withKey ? numChannels : 2;

    for (int ch = 0; ch < channelsToRun; ++ch)
    {
        auto& channel = channels[(size_t) ch];

        std::copy_n (channel.input.begin(), fftSize, forwardScratch.begin());
        std::fill (forwardScratch.begin() + fftSize, forwardScratch.end(), 0.0f);

        fft->performRealOnlyForwardTransform (forwardScratch.data());

        std::copy_n (forwardScratch.begin(), spectrumSize, channel.history.begin() + historyIndex * spectrumSize);

        // The newest samples become the older half of the next window.
        std::copy_n (channel.input.begin() + partition, partition, channel.input.begin());

        for (int band = 0; band < activeBands; ++band)
        {
            auto* output = outputOf (channel, band);

            if (front < 0)
            {
                std::fill_n (output, partition, 0.0f);
                continue;
            }

            convolve (channel, slots[(size_t) front], band, output);

            if (fading)
            {
                convolve (channel, slots[(size_t) fadingFrom], band, previousOutput.data());

                for (int i = 0; i < partition; ++i)
                {
                    const auto amount = ((float) i + 0.5f) / (float) partition;
                    output[i] = previousOutput[(size_t) i] + amount * (output[i] - previousOutput[(size_t) i]);
                }
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

void LinearCrossover::process (const float* left, const float* right, const float* keyLeft, const float* keyRight,
                               double* bands, double* keyBands, int stride, int numSamples) noexcept
{
    const auto withKey = keyBands != nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        channels[0].input[(size_t) (partition + position)] = left[i];
        channels[1].input[(size_t) (partition + position)] = right[i];

        if (withKey)
        {
            channels[2].input[(size_t) (partition + position)] = keyLeft[i];
            channels[3].input[(size_t) (partition + position)] = keyRight[i];
        }

        for (int band = 0; band < activeBands; ++band)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                bands[(band * 2 + ch) * stride + i] = (double) outputOf (channels[(size_t) ch], band)[position];

                if (withKey)
                    keyBands[(band * 2 + ch) * stride + i] = (double) outputOf (channels[(size_t) ch + 2], band)[position];
            }
        }

        if (++position == partition)
        {
            position = 0;
            processPartition (withKey);
        }
    }
}

} // namespace fancy::dsp
