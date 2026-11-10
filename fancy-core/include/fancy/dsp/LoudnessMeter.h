#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <numbers>
#include <vector>

namespace fancy::dsp
{

// ITU-R BS.1770-4 / EBU R128 loudness of a stereo signal: momentary (400 ms), short-term (3 s),
// integrated (gated) and loudness range (EBU Tech 3342). Everything runs on the audio thread
// in fixed storage, so it keeps measuring in an offline render with no editor open. Results
// are read lock-free. See docs/limit.md.
class LoudnessMeter
{
public:
    static constexpr double silenceLufs = -200.0;

    // Allocates. Not for the audio thread.
    void prepare (double sampleRate)
    {
        rate = sampleRate;
        blockLength = std::max (1, (int) std::lround (0.1 * sampleRate));
        designFilters();
        resetNow();
    }

    // Any thread: the audio thread clears everything at the start of its next block.
    void requestReset() noexcept { resetRequested.store (true, std::memory_order_relaxed); }

    // Audio thread. A mono signal passes the same channel twice.
    void process (const float* left, const float* right, int numSamples) noexcept
    {
        if (resetRequested.exchange (false, std::memory_order_relaxed))
            resetNow();

        for (int i = 0; i < numSamples; ++i)
        {
            const auto l = (double) left[i], r = (double) right[i];
            const auto kl = weight (0, l), kr = weight (1, r);

            blockWeighted += kl * kl + kr * kr;
            blockPlain += 0.5 * (l * l + r * r);

            if (++blockFill == blockLength)
                finishBlock();
        }
    }

    double getMomentary() const noexcept { return momentary.load (std::memory_order_relaxed); }
    double getShortTerm() const noexcept { return shortTerm.load (std::memory_order_relaxed); }
    double getIntegrated() const noexcept { return integrated.load (std::memory_order_relaxed); }
    double getRange() const noexcept { return range.load (std::memory_order_relaxed); }

    // Unweighted RMS over the last 300 ms, in dBFS, for the RMS meter ballistics.
    double getRmsDb() const noexcept { return rmsDb.load (std::memory_order_relaxed); }

    static double toLufs (double meanSquare) noexcept
    {
        return meanSquare > 1.0e-20 ? -0.691 + 10.0 * std::log10 (meanSquare) : silenceLufs;
    }

private:
    // 0.1 LU bins from -70 to +10 LUFS, for the gating.
    static constexpr double histogramLow = -70.0, binWidth = 0.1;
    static constexpr int numBins = 800;
    static constexpr int shortBlocks = 30, momentaryBlocks = 4, rmsBlocks = 3;

    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    };

    struct Histogram
    {
        void clear() noexcept
        {
            energy.fill (0.0);
            count.fill (0);
        }

        void add (double meanSquare) noexcept
        {
            const auto lufs = toLufs (meanSquare);

            if (lufs < histogramLow)
                return;

            const auto bin = std::clamp ((int) ((lufs - histogramLow) / binWidth), 0, numBins - 1);
            energy[(size_t) bin] += meanSquare;
            ++count[(size_t) bin];
        }

        int binFor (double lufs) const noexcept
        {
            return std::clamp ((int) std::ceil ((lufs - histogramLow) / binWidth), 0, numBins);
        }

        std::array<double, numBins> energy {};
        std::array<long long, numBins> count {};
    };

    void designFilters()
    {
        // The two K-weighting stages, recomputed for this rate (BS.1770-4, as libebur128 does).
        {
            const auto f0 = 1681.974450955533, gain = 3.999843853973347, q = 0.7071752369554196;
            const auto k = std::tan (std::numbers::pi * f0 / rate);
            const auto vh = std::pow (10.0, gain / 20.0);
            const auto vb = std::pow (vh, 0.4996667741545416);
            const auto a0 = 1.0 + k / q + k * k;

            shelf = { (vh + vb * k / q + k * k) / a0, 2.0 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0,
                      2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0 };
        }

        {
            const auto f0 = 38.13547087602444, q = 0.5003270373238773;
            const auto k = std::tan (std::numbers::pi * f0 / rate);
            const auto a0 = 1.0 + k / q + k * k;

            highpass = { 1.0, -2.0, 1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0 };
        }
    }

    double weight (int channel, double x) noexcept
    {
        auto& s = state[(size_t) channel];

        const auto y1 = shelf.b0 * x + s[0];
        s[0] = shelf.b1 * x - shelf.a1 * y1 + s[1];
        s[1] = shelf.b2 * x - shelf.a2 * y1;

        const auto y2 = highpass.b0 * y1 + s[2];
        s[2] = highpass.b1 * y1 - highpass.a1 * y2 + s[3];
        s[3] = highpass.b2 * y1 - highpass.a2 * y2;

        return y2;
    }

    void resetNow() noexcept
    {
        for (auto& s : state)
            s.fill (0.0);

        blocks.fill (0.0);
        plainBlocks.fill (0.0);
        blockWeighted = blockPlain = 0.0;
        blockFill = 0;
        blocksSeen = 0;
        blockPosition = 0;
        momentaryHistogram.clear();
        shortHistogram.clear();

        momentary.store (silenceLufs, std::memory_order_relaxed);
        shortTerm.store (silenceLufs, std::memory_order_relaxed);
        integrated.store (silenceLufs, std::memory_order_relaxed);
        range.store (0.0, std::memory_order_relaxed);
        rmsDb.store (silenceLufs, std::memory_order_relaxed);
    }

    double meanOfLast (const std::array<double, shortBlocks>& ring, int count) const noexcept
    {
        const auto available = (int) std::min<long long> (count, blocksSeen);
        auto sum = 0.0;

        for (int i = 1; i <= available; ++i)
            sum += ring[(size_t) ((blockPosition + shortBlocks - i) % shortBlocks)];

        return sum / (double) (count * blockLength);
    }

    void finishBlock() noexcept
    {
        blocks[(size_t) blockPosition] = blockWeighted;
        plainBlocks[(size_t) blockPosition] = blockPlain;
        blockPosition = (blockPosition + 1) % shortBlocks;
        ++blocksSeen;
        blockWeighted = blockPlain = 0.0;
        blockFill = 0;

        const auto momentaryPower = meanOfLast (blocks, momentaryBlocks);
        const auto shortPower = meanOfLast (blocks, shortBlocks);
        const auto plainPower = meanOfLast (plainBlocks, rmsBlocks);

        momentary.store (toLufs (momentaryPower), std::memory_order_relaxed);
        shortTerm.store (blocksSeen >= momentaryBlocks ? toLufs (shortPower) : silenceLufs, std::memory_order_relaxed);
        rmsDb.store (plainPower > 1.0e-20 ? 10.0 * std::log10 (plainPower) : silenceLufs, std::memory_order_relaxed);

        // Gating blocks: 400 ms every 100 ms (75 % overlap) for integrated loudness; short-term
        // values every 100 ms for the range, once the first 3 s are in.
        if (blocksSeen >= momentaryBlocks)
        {
            momentaryHistogram.add (momentaryPower);
            integrated.store (gatedLoudness(), std::memory_order_relaxed);
        }

        if (blocksSeen >= shortBlocks)
        {
            shortHistogram.add (shortPower);
            range.store (loudnessRange(), std::memory_order_relaxed);
        }
    }

    // BS.1770-4: the mean of the blocks above -70 LUFS and above that mean minus 10 LU.
    double gatedLoudness() const noexcept
    {
        const auto& h = momentaryHistogram;
        auto energy = 0.0;
        long long count = 0;

        for (int bin = 0; bin < numBins; ++bin)
        {
            energy += h.energy[(size_t) bin];
            count += h.count[(size_t) bin];
        }

        if (count == 0)
            return silenceLufs;

        const auto relativeGate = toLufs (energy / (double) count) - 10.0;
        energy = 0.0;
        count = 0;

        for (int bin = h.binFor (relativeGate); bin < numBins; ++bin)
        {
            energy += h.energy[(size_t) bin];
            count += h.count[(size_t) bin];
        }

        return count > 0 ? toLufs (energy / (double) count) : silenceLufs;
    }

    // EBU Tech 3342: the 10th to 95th percentile of the short-term values above -70 LUFS and
    // above their mean minus 20 LU.
    double loudnessRange() const noexcept
    {
        const auto& h = shortHistogram;
        auto energy = 0.0;
        long long count = 0;

        for (int bin = 0; bin < numBins; ++bin)
        {
            energy += h.energy[(size_t) bin];
            count += h.count[(size_t) bin];
        }

        if (count == 0)
            return 0.0;

        const auto first = h.binFor (toLufs (energy / (double) count) - 20.0);
        long long total = 0;

        for (int bin = first; bin < numBins; ++bin)
            total += h.count[(size_t) bin];

        if (total == 0)
            return 0.0;

        const auto percentile = [&] (double fraction)
        {
            const auto target = (long long) std::ceil (fraction * (double) total);
            long long seen = 0;

            for (int bin = first; bin < numBins; ++bin)
            {
                seen += h.count[(size_t) bin];

                if (seen >= std::max (1LL, target))
                    return histogramLow + ((double) bin + 0.5) * binWidth;
            }

            return histogramLow + ((double) numBins - 0.5) * binWidth;
        };

        return std::max (0.0, percentile (0.95) - percentile (0.10));
    }

    double rate = 48000.0;
    int blockLength = 4800, blockFill = 0, blockPosition = 0;
    long long blocksSeen = 0;
    double blockWeighted = 0.0, blockPlain = 0.0;

    Biquad shelf, highpass;
    std::array<std::array<double, 4>, 2> state {};
    std::array<double, shortBlocks> blocks {}, plainBlocks {};
    Histogram momentaryHistogram, shortHistogram;

    std::atomic<bool> resetRequested { false };
    std::atomic<double> momentary { silenceLufs }, shortTerm { silenceLufs }, integrated { silenceLufs }, range { 0.0 },
        rmsDb { silenceLufs };
};

// The highest true peak of a stereo signal since the last reset: a 4x polyphase windowed-sinc
// interpolator, 24 taps per phase, as BS.1770-4 Annex 2 describes (its own coefficients, not
// the standard's table). Read lock-free.
class TruePeakMeter
{
public:
    static constexpr int factor = 4, tapsPerPhase = 24;

    TruePeakMeter()
    {
        constexpr auto half = tapsPerPhase / 2;

        for (int phase = 0; phase < factor; ++phase)
        {
            const auto fraction = (double) phase / (double) factor;

            for (int tap = 0; tap < tapsPerPhase; ++tap)
            {
                const auto x = (double) (tap - half + 1) - fraction;
                const auto sinc = std::abs (x) < 1.0e-12 ? 1.0 : std::sin (std::numbers::pi * x) / (std::numbers::pi * x);
                const auto w = 0.5 + 0.5 * std::cos (std::numbers::pi * x / (double) half);
                taps[(size_t) phase][(size_t) tap] = sinc * w;
            }

            auto sum = 0.0;

            for (const auto tap : taps[(size_t) phase])
                sum += std::abs (tap);

            largestGain = std::max (largestGain, sum);
        }

        resetNow();
    }

    void requestReset() noexcept { resetRequested.store (true, std::memory_order_relaxed); }

    // Audio thread.
    void process (const float* left, const float* right, int numSamples) noexcept
    {
        if (resetRequested.exchange (false, std::memory_order_relaxed))
            resetNow();

        for (int i = 0; i < numSamples; ++i)
        {
            push (0, (double) left[i]);
            push (1, (double) right[i]);
            position = (position + 1) % tapsPerPhase;
        }

        peakDb.store (peak > 1.0e-10 ? 20.0 * std::log10 (peak) : -200.0, std::memory_order_relaxed);
    }

    // For tests and one-off measurements.
    double measure (double x, int channel) noexcept
    {
        push (channel, x);

        if (channel == 1)
            position = (position + 1) % tapsPerPhase;

        return peak;
    }

    double getPeakDb() const noexcept { return peakDb.load (std::memory_order_relaxed); }

private:
    void resetNow() noexcept
    {
        for (auto& h : history)
            h.fill (0.0);

        peak = 0.0;
        position = 0;
        peakDb.store (-200.0, std::memory_order_relaxed);
    }

    void push (int channel, double x) noexcept
    {
        // Stored twice, tapsPerPhase apart, so the window reads in one straight run.
        auto& h = history[(size_t) channel];
        h[(size_t) position] = h[(size_t) (position + tapsPerPhase)] = x;

        const auto* window = h.data() + position + 1;
        auto loudest = 0.0;

        for (int tap = 0; tap < tapsPerPhase; ++tap)
            loudest = std::max (loudest, std::abs (window[tap]));

        // Nothing between these samples can beat the peak so far.
        if (loudest * largestGain <= peak)
            return;

        for (const auto& phase : taps)
        {
            double sum = 0.0;

            for (int tap = 0; tap < tapsPerPhase; ++tap)
                sum += phase[(size_t) tap] * window[tap];

            peak = std::max (peak, std::abs (sum));
        }
    }

    std::array<std::array<double, tapsPerPhase>, factor> taps {};
    std::array<std::array<double, 2 * tapsPerPhase>, 2> history {};
    double largestGain = 1.0;
    double peak = 0.0;
    int position = 0;
    std::atomic<bool> resetRequested { false };
    std::atomic<double> peakDb { -200.0 };
};

} // namespace fancy::dsp
