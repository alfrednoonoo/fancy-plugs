#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace fancy::dsp
{

// Stereo lookahead brickwall limiter with a fixed ceiling of 1.0. See docs/comp.md, and
// docs/limit.md for the settings Fancy Limit adds. With default Settings it is Fancy Comp's and
// Fancy MultiComp's limiter, sample for sample.
class TruePeakLimiter
{
public:
    // How the gain moves down over the lookahead window.
    enum class Shape
    {
        box,      // one moving average over the window (Comp, MultiComp)
        smooth,   // three cascaded averages, each a third of the window (Limit: Clean, Dense)
        punchy    // two cascaded averages over the last 60 % of the window (Limit: Punchy)
    };

    struct Settings
    {
        // Attack window in samples. 0 means Comp's: max (8, 2 ms).
        int window = 0;

        // The longest window prepare() allows for. It fixes the latency, so a shorter window
        // delays the gain instead of the audio. 0 means the same as the window.
        int maxWindow = 0;

        // The shortest window. Comp keeps 8 samples; Limit allows 1.
        int minWindow = 8;

        double releaseMs = 50.0;

        // A second, slower release with a 30 ms hold, used as the reduction becomes sustained.
        bool autoRelease = false;
        double slowReleaseMs = 400.0;

        // 1 is fully linked; 0 is two independent channels.
        double link = 1.0;

        Shape shape = Shape::box;

        // Off: sample peaks only.
        bool truePeak = true;

        // Also checks the sample the interpolator centres on. Comp's v1.2.0 limiter doesn't, and
        // leaves a sharp sample peak to its final clamp; Limit does, so the gain catches it.
        bool alignedSamplePeak = false;

        // The true-peak interpolator: taps (16 or 32) and phases between samples (3 is 4x, 7 is
        // 8x). Comp's is 16 and 3. Fixed by prepare(): it sets the latency.
        int interpolatorTaps = 16;
        int interpolatorPhases = 3;
    };

    void prepare (double sampleRate)
    {
        prepare (sampleRate, Settings {});
    }

    // Allocates for settings.maxWindow. Not for the audio thread.
    void prepare (double sampleRate, const Settings& settings)
    {
        rate = sampleRate;
        current = settings;

        historySize = settings.interpolatorTaps >= maxTaps ? maxTaps : 16;
        interpolatorDelay = historySize / 2;
        phases = std::clamp (settings.interpolatorPhases, 1, maxPhases);

        window = resolveWindow (settings);
        maxWindow = std::max (window, settings.maxWindow);
        lookahead = maxWindow + interpolatorDelay;
        releaseCoefficient = 1.0 - std::exp (-1.0 / (settings.releaseMs / 1000.0 * sampleRate));
        slowReleaseCoefficient = 1.0 - std::exp (-1.0 / (settings.slowReleaseMs / 1000.0 * sampleRate));
        sustainCoefficients[0] = 1.0 - std::exp (-1.0 / (0.3 * sampleRate));
        sustainCoefficients[1] = 1.0 - std::exp (-1.0 / (0.02 * sampleRate));
        longHold = std::max (maxWindow, (int) std::lround (0.03 * sampleRate));

        for (auto& line : delayLines)
            line.assign ((size_t) lookahead + 1, 0.0);

        for (auto& line : gainDelays)
            line.assign ((size_t) maxWindow + 1, 1.0);

        for (auto& pipeline : pipelines)
        {
            pipeline.shortMinimum.allocate (maxWindow + 2);
            pipeline.longMinimum.allocate (longHold + 2);

            for (auto& stage : pipeline.stages)
                stage.allocate (maxWindow);
        }

        buildInterpolator();
        reset();
    }

    void reset()
    {
        for (auto& line : delayLines)
            std::fill (line.begin(), line.end(), 0.0);

        for (auto& line : gainDelays)
            std::fill (line.begin(), line.end(), 1.0);

        for (auto& channel : history)
            channel.fill (0.0);

        for (auto& pipeline : pipelines)
        {
            pipeline.shortMinimum.clear();
            pipeline.longMinimum.clear();
            pipeline.releasing = pipeline.slow = 1.0;
            pipeline.sustained = pipeline.recent = 0.0;
        }

        configureStages (1.0);

        delayPosition = 0;
        gainDelayPosition = 0;
        historyPosition = 0;
        sampleIndex = 0;
        lastGain = { 1.0, 1.0 };
    }

    // Changes the settings without allocating. The window is limited to the prepared maximum.
    // A new window or shape restarts the smoothing at the current gain.
    void setSettings (const Settings& settings) noexcept
    {
        const auto newWindow = std::min (resolveWindow (settings), maxWindow);
        const auto reshape = newWindow != window || settings.shape != current.shape;

        if (settings.releaseMs != current.releaseMs)
            releaseCoefficient = 1.0 - std::exp (-1.0 / (settings.releaseMs / 1000.0 * rate));

        if (settings.slowReleaseMs != current.slowReleaseMs)
            slowReleaseCoefficient = 1.0 - std::exp (-1.0 / (settings.slowReleaseMs / 1000.0 * rate));

        const auto wasLinked = current.link >= 1.0;
        current = settings;
        window = newWindow;

        // Switching between one linked pipeline and two starts both from the linked state.
        if (wasLinked != (settings.link >= 1.0))
            pipelines[1] = pipelines[0];

        if (reshape)
            configureStages (std::min (lastGain[0], lastGain[1]));
    }

    const Settings& getSettings() const noexcept { return current; }

    int getLatencySamples() const noexcept { return lookahead; }

    // The gain applied to the last sample, per channel, before the final clamp.
    double getLastGain (int channel) const noexcept { return lastGain[(size_t) channel]; }

    // Replaces left and right with the limited, delayed signal. While amount is 0 the
    // signal is only delayed. delayed, if given, receives the delayed input before the gain.
    void process (double& left, double& right, double amount, double* delayed = nullptr) noexcept
    {
        const auto peakLeft = peakOf (0, left);
        const auto peakRight = peakOf (1, right);
        historyPosition = (historyPosition + 1) % historySize;

        std::array<double, 2> gains;

        if (current.link >= 1.0)
        {
            const auto peak = std::max (peakLeft, peakRight);
            const auto required = peak > 1.0 ? 1.0 / peak : 1.0;
            const auto releasing = pipelines[0].step (required, *this);
            gains[0] = gains[1] = pipelines[0].smooth (releasing, current.shape);
        }
        else
        {
            const auto releasingLeft = pipelines[0].step (peakLeft > 1.0 ? 1.0 / peakLeft : 1.0, *this);
            const auto releasingRight = pipelines[1].step (peakRight > 1.0 ? 1.0 / peakRight : 1.0, *this);
            const auto linked = std::min (releasingLeft, releasingRight);

            // In dB: each channel moves the given fraction of the way to the linked gain. The
            // result never rises above the channel's own gain, so its peaks stay held.
            const auto blend = [this, linked] (double own)
            {
                return current.link <= 0.0 ? own : own * std::pow (linked / own, current.link);
            };

            gains[0] = pipelines[0].smooth (blend (releasingLeft), current.shape);
            gains[1] = pipelines[1].smooth (blend (releasingRight), current.shape);
        }

        ++sampleIndex;

        // A window shorter than the maximum: the audio keeps the maximum's delay, so the gain
        // waits for the difference.
        if (const auto wait = maxWindow - window; wait > 0)
        {
            const auto size = maxWindow + 1;

            for (size_t ch = 0; ch < 2; ++ch)
            {
                gainDelays[ch][(size_t) gainDelayPosition] = gains[ch];
                gains[ch] = gainDelays[ch][(size_t) ((gainDelayPosition + size - wait) % size)];
            }

            gainDelayPosition = (gainDelayPosition + 1) % size;
        }

        lastGain = gains;

        const auto delayedLeft = applyDelay (0, left);
        const auto delayedRight = applyDelay (1, right);

        if (delayed != nullptr)
        {
            delayed[0] = delayedLeft;
            delayed[1] = delayedRight;
        }

        left = delayedLeft * (1.0 + (gains[0] - 1.0) * amount);
        right = delayedRight * (1.0 + (gains[1] - 1.0) * amount);

        if (amount > 0.0)
        {
            left = std::clamp (left, -1.0, 1.0);
            right = std::clamp (right, -1.0, 1.0);
        }

        delayPosition = (delayPosition + 1) % (lookahead + 1);
    }

private:
    static constexpr int maxTaps = 32, maxPhases = 7;

    int historySize = 16, interpolatorDelay = 8, phases = 3;

    // Minimum of the last length + 1 values, using a monotonic queue.
    struct SlidingMinimum
    {
        void allocate (int capacityToUse)
        {
            capacity = capacityToUse;
            values.assign ((size_t) capacity, 1.0);
            indices.assign ((size_t) capacity, 0);
            clear();
        }

        void clear() noexcept { head = tail = 0; }

        double push (double value, long long index, int length) noexcept
        {
            while (head != tail)
            {
                const auto last = (tail + capacity - 1) % capacity;

                if (values[(size_t) last] < value)
                    break;

                tail = last;
            }

            values[(size_t) tail] = value;
            indices[(size_t) tail] = index;
            tail = (tail + 1) % capacity;

            // A window that shrank can leave more than one stale entry.
            while (index - indices[(size_t) head] > length)
                head = (head + 1) % capacity;

            return values[(size_t) head];
        }

        std::vector<double> values;
        std::vector<long long> indices;
        int capacity = 2, head = 0, tail = 0;
    };

    // A moving average, written as Comp's original one so the box shape is bit-identical.
    struct MovingAverage
    {
        void allocate (int maximum) { line.assign ((size_t) std::max (1, maximum), 1.0); }

        void start (int newLength, double value) noexcept
        {
            if (line.empty())
                return;

            length = std::max (1, std::min (newLength, (int) line.size()));
            std::fill (line.begin(), line.begin() + length, value);
            sum = value * (double) length;
            position = 0;
        }

        double push (double value) noexcept
        {
            sum += value - line[(size_t) position];
            line[(size_t) position] = value;
            position = (position + 1) % length;
            return sum / (double) length;
        }

        std::vector<double> line;
        double sum = 1.0;
        int length = 1, position = 0;
    };

    struct Pipeline
    {
        // Required gain in, released gain out: still at or below every required gain within the window.
        double step (double required, TruePeakLimiter& owner) noexcept
        {
            const auto held = shortMinimum.push (required, owner.sampleIndex, owner.window);
            releasing = std::min (held, releasing + owner.releaseCoefficient * (1.0 - releasing));

            if (! owner.current.autoRelease)
                return releasing;

            // A long hold and a slow release, weighted by how sustained the reduction is: its
            // 300 ms average against its 20 ms average.
            const auto longHeld = longMinimum.push (required, owner.sampleIndex, owner.longHold);
            slow = std::min (longHeld, slow + owner.slowReleaseCoefficient * (1.0 - slow));

            const auto depth = 1.0 - held;
            sustained += (depth - sustained) * owner.sustainCoefficients[0];
            recent += (depth - recent) * owner.sustainCoefficients[1];

            const auto weight = recent > 1.0e-9 ? std::clamp (sustained / recent, 0.0, 1.0) : 0.0;
            return releasing + (slow - releasing) * weight;
        }

        double smooth (double value, Shape shape) noexcept
        {
            switch (shape)
            {
                case Shape::box:    return stages[0].push (value);
                case Shape::punchy: return stages[1].push (stages[0].push (value));
                case Shape::smooth: break;
            }

            return stages[2].push (stages[1].push (stages[0].push (value)));
        }

        SlidingMinimum shortMinimum, longMinimum;
        std::array<MovingAverage, 3> stages;
        double releasing = 1.0, slow = 1.0, sustained = 0.0, recent = 0.0;
    };

    int resolveWindow (const Settings& settings) const noexcept
    {
        const auto requested = settings.window > 0 ? settings.window : (int) std::lround (0.002 * rate);
        return std::max (settings.minWindow, requested);
    }

    void configureStages (double value) noexcept
    {
        for (auto& pipeline : pipelines)
        {
            switch (current.shape)
            {
                case Shape::box:
                    pipeline.stages[0].start (window, value);
                    break;

                case Shape::punchy:
                {
                    // The two stages' support is a + b - 1 samples, the last 60 % of the window.
                    const auto span = std::max (2, (int) std::lround (0.6 * window) + 1);
                    pipeline.stages[0].start (span / 2, value);
                    pipeline.stages[1].start (span - span / 2, value);
                    break;
                }

                case Shape::smooth:
                {
                    // Support a + b + c - 2 samples: at most the window, so the hold covers it.
                    const auto span = window + 2;
                    pipeline.stages[0].start (span / 3, value);
                    pipeline.stages[1].start (span / 3, value);
                    pipeline.stages[2].start (span - 2 * (span / 3), value);
                    break;
                }
            }
        }
    }

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

    double peakOf (int channel, double in) noexcept
    {
        if (current.truePeak)
            return truePeak (channel, in);

        // The sample the interpolator would centre on, so the timing matches.
        auto& samples = history[(size_t) channel];
        samples[(size_t) historyPosition] = in;
        return std::abs (samples[(size_t) ((historyPosition + historySize - interpolatorDelay) % historySize)]);
    }

    double truePeak (int channel, double in) noexcept
    {
        auto& samples = history[(size_t) channel];
        samples[(size_t) historyPosition] = in;

        auto peak = std::abs (in);

        if (current.alignedSamplePeak)
            peak = std::max (peak, std::abs (samples[(size_t) ((historyPosition + historySize - interpolatorDelay) % historySize)]));

        for (int p = 0; p < phases; ++p)
        {
            const auto& phase = taps[(size_t) p];
            double sum = 0.0;

            for (int tap = 0; tap < historySize; ++tap)
                sum += phase[(size_t) tap] * samples[(size_t) ((historyPosition + 1 + tap) % historySize)];

            peak = std::max (peak, std::abs (sum));
        }

        return peak;
    }

    double applyDelay (int channel, double in) noexcept
    {
        auto& line = delayLines[(size_t) channel];
        line[(size_t) delayPosition] = in;
        return line[(size_t) ((delayPosition + 1) % (lookahead + 1))];
    }

    Settings current;
    double rate = 48000.0;
    int window = 96, maxWindow = 96, lookahead = 104, longHold = 1440;
    double releaseCoefficient = 0.001, slowReleaseCoefficient = 0.0001;
    std::array<double, 2> sustainCoefficients { 0.0001, 0.001 };

    std::array<std::vector<double>, 2> delayLines, gainDelays;
    std::array<std::array<double, maxTaps>, 2> history {};
    std::array<std::array<double, maxTaps>, maxPhases> taps {};
    std::array<Pipeline, 2> pipelines;
    std::array<double, 2> lastGain { 1.0, 1.0 };

    int delayPosition = 0, gainDelayPosition = 0, historyPosition = 0;
    long long sampleIndex = 0;
};

} // namespace fancy::dsp
