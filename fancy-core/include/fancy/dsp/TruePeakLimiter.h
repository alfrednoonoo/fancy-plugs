#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace fancy::dsp
{

// Stereo-linked lookahead brickwall limiter with a fixed ceiling of 1.0. See docs/comp.md.
class TruePeakLimiter
{
public:
    void prepare (double sampleRate)
    {
        window = std::max (8, (int) std::lround (0.002 * sampleRate));
        lookahead = window + interpolatorDelay;
        releaseCoefficient = 1.0 - std::exp (-1.0 / (0.05 * sampleRate));

        for (auto& line : delayLines)
            line.assign ((size_t) lookahead + 1, 0.0);

        minimumValues.assign ((size_t) window + 2, 1.0);
        minimumIndices.assign ((size_t) window + 2, 0);
        averageLine.assign ((size_t) window, 1.0);

        buildInterpolator();
        reset();
    }

    void reset()
    {
        for (auto& line : delayLines)
            std::fill (line.begin(), line.end(), 0.0);

        for (auto& channel : history)
            channel.fill (0.0);

        std::fill (averageLine.begin(), averageLine.end(), 1.0);

        averageSum = (double) window;
        averagePosition = 0;
        delayPosition = 0;
        historyPosition = 0;
        sampleIndex = 0;
        queueHead = queueTail = 0;
        releasing = 1.0;
    }

    int getLatencySamples() const noexcept { return lookahead; }

    // Replaces left and right with the limited, delayed signal. While amount is 0 the
    // signal is only delayed.
    void process (double& left, double& right, double amount) noexcept
    {
        const auto peak = std::max (truePeak (0, left), truePeak (1, right));
        historyPosition = (historyPosition + 1) % historySize;

        const auto required = peak > 1.0 ? 1.0 / peak : 1.0;
        const auto held = slidingMinimum (required);

        releasing = std::min (held, releasing + releaseCoefficient * (1.0 - releasing));

        averageSum += releasing - averageLine[(size_t) averagePosition];
        averageLine[(size_t) averagePosition] = releasing;
        averagePosition = (averagePosition + 1) % window;

        const auto gain = 1.0 + (averageSum / (double) window - 1.0) * amount;

        left = applyDelay (0, left) * gain;
        right = applyDelay (1, right) * gain;

        if (amount > 0.0)
        {
            left = std::clamp (left, -1.0, 1.0);
            right = std::clamp (right, -1.0, 1.0);
        }

        delayPosition = (delayPosition + 1) % (lookahead + 1);
    }

private:
    static constexpr int historySize = 16;
    static constexpr int interpolatorDelay = historySize / 2;
    static constexpr int phases = 3;

    void buildInterpolator()
    {
        for (int phase = 0; phase < phases; ++phase)
        {
            const auto fraction = (double) (phase + 1) / (double) (phases + 1);

            for (int tap = 0; tap < historySize; ++tap)
            {
                const auto x = (double) tap - (double) (historySize / 2 - 1) - fraction;
                const auto sinc = std::sin (std::numbers::pi * x) / (std::numbers::pi * x);
                const auto window2 = 0.5 + 0.5 * std::cos (std::numbers::pi * x / (double) (historySize / 2));

                taps[(size_t) phase][(size_t) tap] = sinc * window2;
            }
        }
    }

    double truePeak (int channel, double in) noexcept
    {
        auto& samples = history[(size_t) channel];
        samples[(size_t) historyPosition] = in;

        auto peak = std::abs (in);

        for (const auto& phase : taps)
        {
            double sum = 0.0;

            for (int tap = 0; tap < historySize; ++tap)
                sum += phase[(size_t) tap] * samples[(size_t) ((historyPosition + 1 + tap) % historySize)];

            peak = std::max (peak, std::abs (sum));
        }

        return peak;
    }

    // Minimum of the last window + 1 values, using a monotonic queue. That is one more than
    // the moving average, so the gain is still at its target when the peak comes out.
    double slidingMinimum (double value) noexcept
    {
        const auto capacity = window + 2;

        while (queueHead != queueTail)
        {
            const auto last = (queueTail + capacity - 1) % capacity;

            if (minimumValues[(size_t) last] < value)
                break;

            queueTail = last;
        }

        minimumValues[(size_t) queueTail] = value;
        minimumIndices[(size_t) queueTail] = sampleIndex;
        queueTail = (queueTail + 1) % capacity;

        if (sampleIndex - minimumIndices[(size_t) queueHead] > window)
            queueHead = (queueHead + 1) % capacity;

        ++sampleIndex;

        return minimumValues[(size_t) queueHead];
    }

    double applyDelay (int channel, double in) noexcept
    {
        auto& line = delayLines[(size_t) channel];
        line[(size_t) delayPosition] = in;
        return line[(size_t) ((delayPosition + 1) % (lookahead + 1))];
    }

    int window = 96, lookahead = 104;
    double releaseCoefficient = 0.001;

    std::array<std::vector<double>, 2> delayLines;
    std::array<std::array<double, historySize>, 2> history {};
    std::array<std::array<double, historySize>, phases> taps {};

    std::vector<double> minimumValues;
    std::vector<long long> minimumIndices;
    std::vector<double> averageLine;

    double averageSum = 0.0, releasing = 1.0;
    int averagePosition = 0, delayPosition = 0, historyPosition = 0;
    int queueHead = 0, queueTail = 0;
    long long sampleIndex = 0;
};

} // namespace fancy::dsp
