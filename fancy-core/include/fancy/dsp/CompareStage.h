#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace fancy::dsp
{

// ITU-R BS.1770 short-term loudness: K-weighting, then the mean square summed over channels,
// over the last 3 s, kept as 30 blocks of 100 ms.
class ShortTermLoudness
{
public:
    static constexpr int numBlocks = 30;
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate)
    {
        // The two K-weighting stages, as BS.1770-4 gives them at 48 kHz, recomputed for the rate.
        {
            const auto k = std::tan (pi * 1681.974450955533 / sampleRate);
            const auto q = 0.7071752369554196;
            const auto vh = std::pow (10.0, 3.999843853973347 / 20.0);
            const auto vb = std::pow (vh, 0.4996667741545416);
            const auto a0 = 1.0 + k / q + k * k;

            shelf = { (vh + vb * k / q + k * k) / a0, 2.0 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0,
                      2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0 };
        }

        {
            const auto k = std::tan (pi * 38.13547087602444 / sampleRate);
            const auto q = 0.5003270373238773;
            const auto a0 = 1.0 + k / q + k * k;

            highPass = { 1.0, -2.0, 1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0 };
        }

        blockLength = std::max (1, (int) std::lround (sampleRate * 0.1));
        reset();
    }

    void reset()
    {
        for (auto& state : states)
            state = {};

        blocks.fill (0.0);
        filled = 0;
        next = 0;
        sum = 0.0;
        count = 0;
    }

    // Adds one sample per channel. Returns true when a 100 ms block has just completed.
    bool add (const float* samples, int numChannels) noexcept
    {
        for (int ch = 0; ch < numChannels && ch < maxChannels; ++ch)
        {
            auto& state = states[(size_t) ch];
            const auto y = state.stage (shelf, 0, (double) samples[ch]);
            const auto z = state.stage (highPass, 1, y);
            sum += z * z;
        }

        if (++count < blockLength)
            return false;

        blocks[(size_t) next] = sum / (double) blockLength;
        next = (next + 1) % numBlocks;
        filled = std::min (filled + 1, numBlocks);
        sum = 0.0;
        count = 0;
        return true;
    }

    // LUFS over the blocks so far, up to 3 s. Very low when nothing has been measured.
    double getLoudness() const noexcept
    {
        if (filled == 0)
            return -200.0;

        auto total = 0.0;

        for (int i = 0; i < filled; ++i)
            total += blocks[(size_t) i];

        return -0.691 + 10.0 * std::log10 (std::max (1.0e-20, total / (double) filled));
    }

private:
    static constexpr double pi = 3.14159265358979323846;

    struct Coefficients
    {
        double b0, b1, b2, a1, a2;
    };

    struct ChannelState
    {
        std::array<double, 2> z1 {}, z2 {};

        double stage (const Coefficients& c, int index, double x) noexcept
        {
            auto& s1 = z1[(size_t) index];
            auto& s2 = z2[(size_t) index];
            const auto y = c.b0 * x + s1;
            s1 = c.b1 * x - c.a1 * y + s2;
            s2 = c.b2 * x - c.a2 * y;
            return y;
        }
    };

    Coefficients shelf {}, highPass {};
    std::array<ChannelState, maxChannels> states {};
    std::array<double, numBlocks> blocks {};
    int blockLength = 4800, filled = 0, next = 0, count = 0;
    double sum = 0.0;
};

// Level-matched Compare (Phase 2.1 W.4). Before the plugin runs, its main input goes into a
// delay line; after, while Compare is on, the output is that input, delayed by the plugin's
// latency, at the gain that matches its loudness to the processed output. The gain is measured
// while Compare is off and frozen when it goes on. Measuring only runs while an editor is open,
// so a closed plugin pays one flag check per block.
class CompareStage
{
public:
    static constexpr int maxChannels = 2;
    static constexpr int delaySize = 1 << 17;    // more than any plugin's latency
    static constexpr float maxGainDb = 24.0f;
    static constexpr double floorLufs = -70.0;
    static constexpr double fadeSeconds = 0.02;

    // Allocates. Call from prepareToPlay.
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;

        for (auto& line : delay)
            line.assign ((size_t) delaySize, 0.0f);

        dryMeter.prepare (sampleRate);
        wetMeter.prepare (sampleRate);
        fadeStep = 1.0 / std::max (1.0, sampleRate * fadeSeconds);
        writePosition = 0;
        mix = on.load() ? 1.0 : 0.0;
        wasActive = false;
    }

    // From the message thread.
    void setMeasuring (bool shouldMeasure) noexcept { measuring.store (shouldMeasure); }
    void setOn (bool shouldBeOn) noexcept { on.store (shouldBeOn); }
    bool isOn() const noexcept { return on.load(); }
    bool isMeasuring() const noexcept { return measuring.load(); }

    // The gain applied to the dry signal while on, in dB.
    float getGainDb() const noexcept { return gainDb.load(); }

    // Loudness of the processed and dry signals, for tests.
    double getWetLoudness() const noexcept { return wetLoudness.load(); }
    double getDryLoudness() const noexcept { return dryLoudness.load(); }

    // Before the plugin processes the block: keeps its input.
    void pushInput (const float* const* channels, int numChannels, int numSamples) noexcept
    {
        active = isActive();

        if (! active || delay[0].empty())
        {
            wasActive = false;
            return;
        }

        // A stale delay line would put old audio into the measurement and the first fade.
        if (! wasActive)
        {
            for (auto& line : delay)
                std::fill (line.begin(), line.end(), 0.0f);

            dryMeter.reset();
            wetMeter.reset();
            wasActive = true;
        }

        blockStart = writePosition;
        inputChannels = std::min (numChannels, maxChannels);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto position = (size_t) ((writePosition + i) & (delaySize - 1));

            for (int ch = 0; ch < maxChannels; ++ch)
                delay[(size_t) ch][position] = inputChannels > 0 ? channels[std::min (ch, inputChannels - 1)][i] : 0.0f;
        }

        writePosition = (writePosition + numSamples) & (delaySize - 1);
    }

    // After the plugin processed the block in place.
    void process (float* const* channels, int numChannels, int numSamples, int latencySamples) noexcept
    {
        if (! active || delay[0].empty())
            return;

        const auto outputs = std::min (numChannels, maxChannels);
        const auto latency = std::clamp (latencySamples, 0, delaySize - 1);
        const auto target = on.load() ? 1.0 : 0.0;
        const auto gain = std::pow (10.0, (double) gainDb.load() / 20.0);

        float wet[maxChannels] {}, dry[maxChannels] {};

        for (int i = 0; i < numSamples; ++i)
        {
            const auto position = (size_t) ((blockStart + i - latency) & (delaySize - 1));

            for (int ch = 0; ch < maxChannels; ++ch)
            {
                dry[ch] = delay[(size_t) ch][position];
                wet[ch] = ch < outputs ? channels[ch][i] : wet[0];
            }

            // Measured from what the plugin made, whether or not Compare is on.
            const auto measured = dryMeter.add (dry, std::max (1, outputs)) | wetMeter.add (wet, std::max (1, outputs));

            if (measured && target == 0.0 && mix == 0.0)
                updateGain();

            if (mix != target)
                mix = target > mix ? std::min (target, mix + fadeStep) : std::max (target, mix - fadeStep);

            if (mix == 0.0)
                continue;

            const auto angle = mix * 1.5707963267948966;
            const auto wetGain = (float) std::cos (angle), dryGain = (float) (std::sin (angle) * gain);

            for (int ch = 0; ch < outputs; ++ch)
                channels[ch][i] = mix == 1.0 ? (float) (dry[ch] * gain) : wet[ch] * wetGain + dry[ch] * dryGain;
        }
    }

private:
    bool isActive() const noexcept { return measuring.load() || on.load() || mix > 0.0; }

    void updateGain() noexcept
    {
        const auto wetLufs = wetMeter.getLoudness(), dryLufs = dryMeter.getLoudness();

        wetLoudness.store (wetLufs);
        dryLoudness.store (dryLufs);

        if (wetLufs < floorLufs || dryLufs < floorLufs)
            return;

        gainDb.store (std::clamp ((float) (wetLufs - dryLufs), -maxGainDb, maxGainDb));
    }

    std::array<std::vector<float>, maxChannels> delay;
    ShortTermLoudness dryMeter, wetMeter;
    double sampleRate = 48000.0, fadeStep = 1.0 / 960.0, mix = 0.0;
    int writePosition = 0, blockStart = 0, inputChannels = 0;
    bool active = false, wasActive = false;

    std::atomic<bool> measuring { false }, on { false };
    std::atomic<float> gainDb { 0.0f };
    std::atomic<double> wetLoudness { -200.0 }, dryLoudness { -200.0 };
};

} // namespace fancy::dsp
