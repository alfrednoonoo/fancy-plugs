#pragma once

#include <fancy/dsp/Svf.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace fancy::dsp
{

// A delay-line pitch shifter, shared by Fancy Delay's pitched taps and Verb's and Room's shimmer
// (docs/delay.md, "Phase 4 Wave 3", section 3; model docs/research/delay-w3-prepush.py).
//
// Two heads read the line at the new speed, each across a grain of 40 ms / |1 - ratio| (at most
// 400 ms), faded in and out by a raised cosine; a new grain starts as the other reaches its middle,
// so the two sum to 1. The heads' delay moves through a 40 ms window, so the average delay is the
// same at every ratio: getAverageDelaySamples(). Before the line, a fourth-order low-pass at
// 0.45 x the rate / the ratio keeps a rising shift from raising anything past Nyquist.
//
// Aligned mode moves each new grain's start by up to 10 ms to where it best matches the grain
// fading out, so a held note's two heads add in phase instead of wavering. The match compares 10 ms
// of what both heads read: as far ahead as is already written (about 10 ms for a rising shift,
// whose heads start far back), otherwise the stretch just before now. It tries every 4th sample
// (at 48 kHz) of the search, comparing every 8th sample of the 10 ms, then sample by sample near
// the best, and prefers the middle of the search among equal matches. Both channels share one
// match, on their sum, so the stereo image holds still. Plain mode skips it.
//
// Buffers are allocated in prepare(); process() allocates nothing.
class PitchShifter
{
public:
    enum class Mode { plain, aligned };

    static constexpr int maxChannels = 2;
    static constexpr double windowMs = 40.0, searchMs = 10.0, minimumMs = 1.0, maxGrainMs = 400.0, matchMs = 10.0;
    static constexpr double maxRatio = 2.0;
    static constexpr double centreBias = 0.05; // of the match score, at the ends of the search

    void prepare (double sampleRate)
    {
        rate = sampleRate;
        window = windowMs * 0.001 * rate;
        search = searchMs * 0.001 * rate;
        minimum = minimumMs * 0.001 * rate;
        centre = minimum + 0.5 * search + 0.5 * window;
        maxGrain = maxGrainMs * 0.001 * rate;

        // The coarse match reads every `stride` samples, 4 at 48 kHz, so its cost doesn't grow
        // with the rate; the fine one steps by a quarter of that.
        stride = std::max (4, (int) std::lround (4.0 * rate / 48000.0));
        fineStep = std::max (1, stride / 4);
        matchCount = std::max (8, (int) (matchMs * 0.001 * rate) / stride);
        fineCount = std::max (8, (int) (0.5 * matchMs * 0.001 * rate) / fineStep);

        // The longest read: the far end of the window and search, plus the match's look back.
        maxDelay = minimum + search + window + maxRatio * matchMs * 0.001 * rate + 8.0;

        size = 1;

        while (size < (int) maxDelay + 16)
            size <<= 1;

        mask = size - 1;

        for (auto& channel : line)
            channel.assign ((size_t) size, 0.0f);

        sumLine.assign ((size_t) size, 0.0f);
        target.assign ((size_t) std::max (matchCount, fineCount) + 1, 0.0);
        reset();
    }

    void setMode (Mode newMode) noexcept { mode = newMode; }

    // The heads' average delay, the same at every ratio.
    double getAverageDelaySamples() const noexcept { return centre; }

    void reset() noexcept
    {
        for (auto& channel : line)
            std::fill (channel.begin(), channel.end(), 0.0f);

        std::fill (sumLine.begin(), sumLine.end(), 0.0f);

        for (auto& filter : lowA)
            filter.reset();

        for (auto& filter : lowB)
            filter.reset();

        for (auto& head : heads)
            head = {};

        write = 0;
        countdown = 0;
        started = false;
        filterRatio = -1.0;
        filterCountdown = 0;
    }

    // One sample of each channel in, one out. ratio is 2^(semitones / 12), 0.5 to 2.
    void process (const double* in, double* out, int numChannels, double ratio) noexcept
    {
        numChannels = std::clamp (numChannels, 1, maxChannels);
        ratio = std::clamp (ratio, 1.0 / maxRatio, maxRatio);

        if (ratio != filterRatio && --filterCountdown <= 0)
        {
            const auto cutoff = 0.45 * rate / std::max (1.0, ratio);
            filterA.set (cutoff, 0.54119610014619698, rate);
            filterB.set (cutoff, 1.3065629648763766, rate);
            filterRatio = ratio;
            filterCountdown = 16;
        }

        auto sum = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto low = lowB[ch].tick (filterB, lowA[ch].tick (filterA, in[ch]).low).low;
            const auto value = std::isfinite (low) ? (float) low : 0.0f;
            line[(size_t) ch][(size_t) write] = value;
            sum += value;
        }

        sumLine[(size_t) write] = sum;

        write = (write + 1) & mask;

        if (! started)
        {
            // The first grain begins in its middle, so the output starts at full level.
            started = true;
            startHead (heads[0], grainLength (ratio), centre);
            heads[0].k = heads[0].length / 2;
            heads[0].phaseCos = -1.0;
            heads[0].phaseSin = 0.0;
            countdown = 0;
        }

        if (--countdown <= 0)
            startGrain (ratio);

        double mixed[maxChannels] {};
        auto gainSum = 0.0;

        for (auto& head : heads)
        {
            if (! head.active)
                continue;

            const auto gain = 0.5 - 0.5 * head.phaseCos;

            for (int ch = 0; ch < numChannels; ++ch)
                mixed[ch] += gain * read (ch, head.delay);

            gainSum += gain;

            head.delay = std::clamp (head.delay - (ratio - 1.0), 2.0, maxDelay);

            // The raised cosine's phase as a rotating phasor, pulled back onto the circle.
            const auto c = head.phaseCos * head.stepCos - head.phaseSin * head.stepSin;
            const auto s = head.phaseSin * head.stepCos + head.phaseCos * head.stepSin;
            const auto norm = 1.5 - 0.5 * (c * c + s * s);
            head.phaseCos = c * norm;
            head.phaseSin = s * norm;

            if (++head.k >= head.length)
                head.active = false;
        }

        // Equal grains sum to 1. While the ratio glides, neighbouring grains differ in length,
        // so the sum is divided out; below 0.5 (the very first grain) it fades in instead.
        const auto scale = 1.0 / std::max (gainSum, 0.5);

        for (int ch = 0; ch < numChannels; ++ch)
            out[ch] = mixed[ch] * scale;
    }

private:
    struct Head
    {
        bool active = false;
        int k = 0, length = 1;
        double delay = 0.0;
        double phaseCos = 1.0, phaseSin = 0.0, stepCos = 1.0, stepSin = 0.0;
    };

    static double hermite (double newer, double near, double far, double older, double fraction) noexcept
    {
        const auto c1 = 0.5 * (far - newer);
        const auto c2 = newer - 2.5 * near + 2.0 * far - 0.5 * older;
        const auto c3 = 0.5 * (older - newer) + 1.5 * (near - far);

        return ((c3 * fraction + c2) * fraction + c1) * fraction + near;
    }

    // The newest sample is at delay 1.
    double read (int ch, double delay) const noexcept
    {
        const auto whole = (int) delay;
        const auto fraction = delay - (double) whole;
        const auto& data = line[(size_t) ch];

        return hermite (data[(size_t) ((write - whole + 1) & mask)], data[(size_t) ((write - whole) & mask)],
                        data[(size_t) ((write - whole - 1) & mask)], data[(size_t) ((write - whole - 2) & mask)], fraction);
    }

    // The channels' sum at a delay, linearly interpolated, for the match. The sum is kept as its
    // own line, so the match reads one line however many channels there are.
    double readSum (double delay) const noexcept
    {
        const auto whole = (int) delay;
        const auto fraction = delay - (double) whole;
        const auto a = (double) sumLine[(size_t) ((write - whole) & mask)];
        const auto b = (double) sumLine[(size_t) ((write - whole - 1) & mask)];

        return a + (b - a) * fraction;
    }

    int grainLength (double ratio) const noexcept
    {
        const auto away = std::abs (1.0 - ratio);
        const auto length = away * maxGrain > window ? window / away : maxGrain;
        return std::max (4, (int) std::lround (length));
    }

    void startHead (Head& head, int length, double delay) noexcept
    {
        head.active = true;
        head.k = 0;
        head.length = length;
        head.delay = std::clamp (delay, 2.0, maxDelay);

        const auto step = 2.0 * std::numbers::pi / (double) length;
        head.stepCos = std::cos (step);
        head.stepSin = std::sin (step);
        head.phaseCos = 1.0;
        head.phaseSin = 0.0;
    }

    // How well a grain starting at `delay` matches the outgoing head: the normalised correlation of
    // `count` reads `spacing` samples apart, from `first` samples after now (negative: before).
    double score (double delay, double ratio, int first, int count, int spacing, double targetEnergy) const noexcept
    {
        auto product = 0.0, energy = 0.0;

        for (int j = 0; j < count; ++j)
        {
            const auto value = readSum (delay - ratio * (double) (first + spacing * j));
            product += value * target[(size_t) j];
            energy += value * value;
        }

        return product / std::sqrt (energy * targetEnergy + 1.0e-30);
    }

    double fillTarget (double outgoingDelay, double ratio, int first, int count, int spacing) noexcept
    {
        auto energy = 0.0;

        for (int j = 0; j < count; ++j)
        {
            const auto value = readSum (outgoingDelay - ratio * (double) (first + spacing * j));
            target[(size_t) j] = value;
            energy += value * value;
        }

        return energy;
    }

    void startGrain (double ratio) noexcept
    {
        auto* next = ! heads[0].active ? &heads[0] : (! heads[1].active ? &heads[1] : nullptr);

        if (next == nullptr)
        {
            // Both still sounding while the ratio glides: wait for one to finish.
            countdown = 1;
            return;
        }

        const auto* outgoing = next == &heads[0] ? &heads[1] : &heads[0];
        const auto length = grainLength (ratio);
        const auto travel = (ratio - 1.0) * (double) length;   // how far the delay falls over the grain
        const auto direction = ratio > 1.0 ? -1.0 : 1.0;

        // The start delay for a shift `shift` (0 to search) from the earliest start; the middle
        // of the search keeps the average delay at `centre`.
        const auto startDelay = [&] (double shift) { return centre + 0.5 * travel + direction * (shift - 0.5 * search); };

        auto best = 0.5 * search;

        if (mode == Mode::aligned && outgoing->active)
        {
            // The match spans `span` samples, ending as far ahead as every read is already written
            // (a rising shift's heads start far enough back to look about 10 ms ahead; a falling
            // shift's only behind), and starting no more than `span` before now.
            const auto span = matchCount * stride;
            const auto nearest = std::min ({ outgoing->delay, startDelay (0.0), startDelay (search) });
            const auto last = std::min (span, (int) ((nearest - 3.0) / ratio));
            const auto first = last - span;

            // Equal matches, as every period of a steady tone is, go to the middle of the search,
            // which keeps the average delay where getAverageDelaySamples() says.
            const auto bias = [this] (double shift) { return centreBias * std::abs (shift - 0.5 * search) / (0.5 * search); };

            // The coarse pass reads every second stride, which matches as well (the pre-push) for
            // half the work; the fine pass then reads every fineStep.
            const auto coarseCount = matchCount / 2, coarseSpacing = 2 * stride;
            const auto energy = fillTarget (outgoing->delay, ratio, first, coarseCount, coarseSpacing);

            if (energy > 1.0e-12)
            {
                auto bestScore = -1.0e9;

                for (double shift = 0.0; shift <= search + 1.0e-9; shift += (double) stride)
                {
                    if (const auto value = score (startDelay (shift), ratio, first, coarseCount, coarseSpacing, energy) - bias (shift); value > bestScore)
                    {
                        bestScore = value;
                        best = shift;
                    }
                }

                const auto fineEnergy = fillTarget (outgoing->delay, ratio, first, fineCount, fineStep);
                const auto coarse = best;
                bestScore = -1.0e9;

                for (auto shift = std::max (0.0, coarse - (double) (stride - 1)); shift <= std::min (search, coarse + (double) (stride - 1)) + 1.0e-9; shift += (double) fineStep)
                {
                    if (const auto value = score (startDelay (shift), ratio, first, fineCount, fineStep, fineEnergy) - bias (shift); value > bestScore)
                    {
                        bestScore = value;
                        best = shift;
                    }
                }
            }
        }

        startHead (*next, length, startDelay (best));
        countdown = std::max (1, length / 2);
    }

    double rate = 48000.0, window = 1920.0, search = 480.0, minimum = 48.0, centre = 1248.0, maxGrain = 19200.0, maxDelay = 4000.0;
    int stride = 4, fineStep = 1, matchCount = 120, fineCount = 240;
    int size = 0, mask = 0, write = 0, countdown = 0, filterCountdown = 0;
    bool started = false;
    double filterRatio = -1.0;
    Mode mode = Mode::aligned;

    std::array<std::vector<float>, maxChannels> line;
    std::vector<float> sumLine;
    std::vector<double> target;
    std::array<Head, 2> heads;

    SvfCoefficients filterA, filterB;
    std::array<Svf, maxChannels> lowA, lowB;
};

} // namespace fancy::dsp
