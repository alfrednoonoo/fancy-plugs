#include "ChorusDsp.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
constexpr double pi = std::numbers::pi;
constexpr double smoothingMs = 20.0, delaySmoothingMs = 50.0, voiceFadeMs = 30.0, offsetGlideMs = 50.0, switchFadeMs = 5.0;
constexpr double butterworthQ = 0.70710678118654752;
constexpr double minDelaySamples = 3.0;          // reads only samples already written
constexpr double feedbackLimit = 4.0;            // a runaway guard only; never reached at |Feedback| <= 90 %
constexpr double ensembleRatio = 6.7, ensembleDepth = 0.07;
constexpr double ageTopHz = 18000.0, ageBottomHz = 5000.0;
constexpr double dcHz = 5.0;
constexpr double duckPeakReleaseMs = 30.0;

double sanitise (float sample) noexcept
{
    return std::isfinite (sample) ? (double) sample : 0.0;
}

int nextPowerOfTwo (int value) noexcept
{
    auto size = 1;

    while (size < value)
        size <<= 1;

    return size;
}

double coefficient (double ms, double rate) noexcept
{
    return 1.0 - std::exp (-1.0 / std::max (1.0e-9, ms * 0.001 * rate));
}

// sin (2 pi x) for x in [0, 1): folded to [-pi/2, pi/2] and a 9th-order odd polynomial (error < 4e-6).
double sinCycle (double x) noexcept
{
    auto t = x - std::floor (x);   // 0 to 1
    t = t < 0.5 ? t : t - 1.0;     // -0.5 to 0.5

    auto a = 2.0 * pi * t;          // -pi to pi

    if (a > 0.5 * pi)
        a = pi - a;
    else if (a < -0.5 * pi)
        a = -pi - a;

    const auto a2 = a * a;
    return a * (1.0 + a2 * (-1.0 / 6.0 + a2 * (1.0 / 120.0 + a2 * (-1.0 / 5040.0 + a2 * (1.0 / 362880.0)))));
}

// A value in -1 to 1 for each whole cycle and seed: the smooth random LFO's points.
double hashToUnit (long long cycle, std::uint32_t seed) noexcept
{
    // The free-running phase wraps every 2^20 cycles, so the sequence does too.
    auto h = (std::uint64_t) (cycle & 0xFFFFF) * 0x9E3779B97F4A7C15ull + (std::uint64_t) seed * 0xBF58476D1CE4E5B9ull;
    h ^= h >> 30;
    h *= 0xBF58476D1CE4E5B9ull;
    h ^= h >> 27;
    h *= 0x94D049BB133111EBull;
    h ^= h >> 31;
    return (double) (h >> 11) * (2.0 / 9007199254740992.0) - 1.0;
}
} // namespace

ChorusDsp::ChorusDsp() = default;

double ChorusDsp::flangeDelayMs (double delayMs) noexcept
{
    const auto position = std::log (std::clamp (delayMs, minDelayMs, maxDelayMs) / minDelayMs) / std::log (maxDelayMs / minDelayMs);
    return flangeMinMs * std::pow (flangeMaxMs / flangeMinMs, position);
}

double ChorusDsp::centreDelayMs (int mode, double delayMs) noexcept
{
    return mode == flange ? flangeDelayMs (delayMs) : delayMs;
}

double ChorusDsp::sweepMs (int mode, double delayMs, double depth) noexcept
{
    const auto centre = centreDelayMs (mode, delayMs);
    const auto wanted = mode == flange ? depth * centre
                      : mode == dimension ? 0.5 * depth * depthRangeMs
                                          : depth * depthRangeMs;

    // Never past the start of the line: short delays sweep less rather than flatten at zero.
    return std::min (wanted, 0.95 * centre);
}

double ChorusDsp::voicePosition (int voice, int numVoices) noexcept
{
    return numVoices <= 1 ? 0.0 : -1.0 + 2.0 * (double) voice / (double) (numVoices - 1);
}

double ChorusDsp::lfoValue (int shape, double phaseCycles, std::uint32_t seed) noexcept
{
    if (shape == triangle)
    {
        const auto f = phaseCycles - std::floor (phaseCycles);
        return f < 0.25 ? 4.0 * f : (f < 0.75 ? 2.0 - 4.0 * f : 4.0 * f - 4.0);
    }

    if (shape == smoothRandom)
    {
        const auto cycle = (long long) std::floor (phaseCycles);
        const auto f = phaseCycles - (double) cycle;
        const auto a = hashToUnit (cycle, seed), b = hashToUnit (cycle + 1, seed);
        return a + (b - a) * f * f * (3.0 - 2.0 * f);
    }

    return sinCycle (phaseCycles);
}

double ChorusDsp::voiceLfo (int mode, int shape, double phaseCycles, int voice, int channel) const noexcept
{
    if (mode == doubler)
        return lfoValue (smoothRandom, phaseCycles, (std::uint32_t) (17 + voice * 2 + channel));

    auto value = lfoValue (shape, phaseCycles, (std::uint32_t) (101 + voice));

    if (mode == ensemble)
        value += ensembleDepth * sinCycle (ensembleRatio * phaseCycles + 0.37 * (double) voice);

    return value;
}

void ChorusDsp::prepare (double sampleRate)
{
    rate = sampleRate;

    const auto longestMs = maxDelayMs + depthRangeMs * (1.0 + ensembleDepth) + 2.0;
    lineSize = nextPowerOfTwo ((int) std::ceil (longestMs * 0.001 * rate) + 8);
    lineMask = lineSize - 1;

    for (auto& line : lines)
        line.assign ((size_t) lineSize, 0.0f);

    for (auto* smoother : { &rateSmoother, &depth, &feedback, &width, &age, &lfoPhase, &ducking,
                            &lowCutHz, &highCutHz, &lowCutBlend, &highCutBlend, &bassMonoBlend, &wet, &dry, &output, &normalise })
        smoother->setTime (smoothingMs, rate);

    delay.setTime (delaySmoothingMs, rate);

    for (auto& smoother : voiceGains)
        smoother.setTime (voiceFadeMs, rate);

    for (auto& smoother : voicePositions)
        smoother.setTime (voiceFadeMs, rate);

    for (auto& smoother : voiceOffsets)
        smoother.setTime (offsetGlideMs, rate);

    bypass.setTime (10.0, rate);
    listen.setTime (10.0, rate);
    switchGain.setTime (switchFadeMs / 5.0, rate);

    dcCoefficient = 1.0 - 2.0 * pi * dcHz / rate;
    duckPeakFall = coefficient (duckPeakReleaseMs, rate);
    duckAttack = coefficient (duckAttackMs, rate);
    duckRelease = duckDepthDb / std::max (1.0, duckReleaseMs * 0.001 * rate);   // dB per sample: back in Release

    snapToTargets (current);
}

void ChorusDsp::reset()
{
    for (auto& line : lines)
        std::fill (line.begin(), line.end(), 0.0f);

    for (auto* filters : { &lowCuts, &highCuts, &ageFilters })
        for (auto& filter : *filters)
            filter.reset();

    bassMonoFilter.reset();
    dcIn = dcOut = {};
    duckPeak = duckDb = 0.0;
    noiseState = 0x1234567u;
    writeIndex = 0;
    phase = 0.0;
    aligned = false;
    now = 0;
    columnFill = 0;
}

void ChorusDsp::snapToTargets (const Targets& targets) noexcept
{
    current = targets;
    activeMode = std::clamp (targets.mode, (int) classic, (int) flange);
    activeShape = std::clamp (targets.shape, (int) sine, (int) smoothRandom);
    activeSync = targets.sync;

    const auto voices = std::clamp (targets.voices, 1, maxVoices);

    rateSmoother.snapTo (targets.rateHz);
    depth.snapTo (targets.depth);
    delay.snapTo (targets.delayMs);
    feedback.snapTo (std::clamp (targets.feedback, -maxFeedback, maxFeedback));
    width.snapTo (targets.width);
    age.snapTo (targets.age);
    lfoPhase.snapTo (targets.lfoPhaseDeg);
    ducking.snapTo (targets.ducking);
    lowCutHz.snapTo (targets.lowCutHz);
    highCutHz.snapTo (targets.highCutHz);
    lowCutBlend.snapTo (targets.lowCutOn ? 1.0 : 0.0);
    highCutBlend.snapTo (targets.highCutOn ? 1.0 : 0.0);
    bassMonoBlend.snapTo (targets.bassMono ? 1.0 : 0.0);
    wet.snapTo (targets.wetGain);
    dry.snapTo (targets.dryGain);
    output.snapTo (targets.outputGain);
    bypass.snapTo (targets.bypass ? 1.0 : 0.0);
    listen.snapTo (targets.listen ? 1.0 : 0.0);
    switchGain.snapTo (1.0);
    normalise.snapTo (1.0 / std::sqrt ((double) voices));

    for (int v = 0; v < maxVoices; ++v)
    {
        voiceGains[(size_t) v].snapTo (v < voices ? 1.0 : 0.0);
        voicePositions[(size_t) v].snapTo (voicePosition (std::min (v, voices - 1), voices));
        voiceOffsets[(size_t) v].snapTo (offsetFor (targets.spread, v, voices));
    }

    updateFilters (targets.lowCutHz, targets.highCutHz, targets.age);
    reset();
}

void ChorusDsp::updateFilters (double lowCut, double highCut, double ageAmount) noexcept
{
    const auto limit = 0.45 * rate;
    lowCutCoefficients.set (std::clamp (lowCut, 10.0, limit), butterworthQ, rate);
    highCutCoefficients.set (std::clamp (highCut, 10.0, limit), butterworthQ, rate);
    bassMonoCoefficients.set (std::clamp (lowCut, 10.0, limit), butterworthQ, rate);

    const auto ageHz = ageTopHz * std::pow (ageBottomHz / ageTopHz, std::clamp (ageAmount, 0.0, 1.0));
    ageCoefficients.set (std::min (ageHz, limit), butterworthQ, rate);
}

double ChorusDsp::readLine (int channel, double delaySamples) const noexcept
{
    const auto& line = lines[(size_t) channel];
    const auto position = (double) writeIndex - delaySamples;
    const auto whole = std::floor (position);
    const auto f = position - whole;
    const auto index = (int) whole;

    const auto xm1 = (double) line[(size_t) ((index - 1) & lineMask)];
    const auto x0 = (double) line[(size_t) (index & lineMask)];
    const auto x1 = (double) line[(size_t) ((index + 1) & lineMask)];
    const auto x2 = (double) line[(size_t) ((index + 2) & lineMask)];

    if (f == 0.0)
        return x0;

    const auto c1 = 0.5 * (x1 - xm1);
    const auto c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
    const auto c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}

void ChorusDsp::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    numChannels = std::clamp (numChannels, 0, 2);

    if (numChannels == 0 || numSamples <= 0)
        return;

    const auto settings = current;
    const auto wantedMode = std::clamp (settings.mode, (int) classic, (int) flange);
    const auto wantedShape = std::clamp (settings.shape, (int) sine, (int) smoothRandom);
    const auto voices = std::clamp (settings.voices, 1, maxVoices);
    const auto bpm = std::clamp (settings.bpm, 20.0, 999.0);
    const auto syncBeats = std::max (1.0 / 64.0, settings.syncBeats);

    // Tempo sync follows the host's position while it plays; otherwise the LFO runs free.
    if (activeSync && settings.playing)
    {
        const auto hostPhase = settings.ppq / syncBeats;
        const auto perSample = bpm / 60.0 / syncBeats / rate;

        if (! aligned || std::abs (hostPhase - phase) > 2.0 * perSample + 1.0e-9)
            phase = hostPhase;

        aligned = true;
    }
    else
    {
        aligned = false;
    }

    rateSmoother.setTarget (std::clamp (settings.rateHz, 0.01, 20.0));
    depth.setTarget (std::clamp (settings.depth, 0.0, 1.0));
    delay.setTarget (std::clamp (settings.delayMs, minDelayMs, maxDelayMs));
    feedback.setTarget (std::clamp (settings.feedback, -maxFeedback, maxFeedback));
    width.setTarget (std::clamp (settings.width, 0.0, 1.0));
    age.setTarget (std::clamp (settings.age, 0.0, 1.0));
    lfoPhase.setTarget (std::clamp (settings.lfoPhaseDeg, 0.0, 180.0));
    ducking.setTarget (std::clamp (settings.ducking, 0.0, 1.0));
    lowCutHz.setTarget (settings.lowCutHz);
    highCutHz.setTarget (settings.highCutHz);
    lowCutBlend.setTarget (settings.lowCutOn ? 1.0 : 0.0);
    highCutBlend.setTarget (settings.highCutOn ? 1.0 : 0.0);
    bassMonoBlend.setTarget (settings.bassMono ? 1.0 : 0.0);
    wet.setTarget (settings.wetGain);
    dry.setTarget (settings.dryGain);
    output.setTarget (settings.outputGain);
    bypass.setTarget (settings.bypass ? 1.0 : 0.0);
    listen.setTarget (settings.listen ? 1.0 : 0.0);
    normalise.setTarget (1.0 / std::sqrt ((double) voices));

    for (int v = 0; v < maxVoices; ++v)
    {
        auto& gain = voiceGains[(size_t) v];
        auto& position = voicePositions[(size_t) v];
        auto& offset = voiceOffsets[(size_t) v];
        const auto wantedPosition = voicePosition (std::min (v, voices - 1), voices);
        const auto wantedOffset = offsetFor (std::clamp (settings.spread, 0.0, 1.0), v, voices);

        // A silent voice moves straight to its place, so it fades in where it belongs. A sounding
        // one glides there, and a fading one keeps its motion.
        if (gain.isSettled() && gain.getCurrent() == 0.0)
        {
            position.snapTo (wantedPosition);
            offset.snapTo (wantedOffset);
        }
        else
        {
            position.setTarget (wantedPosition);

            if (v < voices)
                offset.setTarget (wantedOffset);
        }

        gain.setTarget (v < voices ? 1.0 : 0.0);
    }

    const auto switching = wantedMode != activeMode || wantedShape != activeShape || settings.sync != activeSync;

    for (int i = 0; i < numSamples; ++i)
    {
        // Mode, LFO shape and Tempo sync change the voices' motion at once: the wet fades out over
        // a few ms, they switch, and it fades back in.
        if (switching && (wantedMode != activeMode || wantedShape != activeShape || settings.sync != activeSync))
        {
            switchGain.setTarget (0.0);

            if (switchGain.isSettled())
            {
                activeMode = wantedMode;
                activeShape = wantedShape;
                activeSync = settings.sync;
                aligned = false;
                delay.snapToTarget();
                switchGain.setTarget (1.0);
            }
        }

        if ((now & (coefficientInterval - 1)) == 0)
            updateFilters (lowCutHz.getCurrent(), highCutHz.getCurrent(), age.getCurrent());

        double in[2];
        in[0] = sanitise (channels[0][i]);
        in[1] = numChannels > 1 ? sanitise (channels[1][i]) : in[0];

        double feed[2] { in[0], in[1] };

        if (settings.monoInput)
            feed[0] = feed[1] = 0.5 * (in[0] + in[1]);

        // The LFO clock.
        const auto rateHz = rateSmoother.next();
        const auto cyclesPerSample = (activeSync ? bpm / 60.0 / syncBeats : rateHz) / rate;

        const auto depthNow = depth.next();
        const auto delayNow = delay.next();
        const auto feedbackNow = feedback.next();
        const auto widthNow = width.next();
        const auto rightOffset = lfoPhase.next() / 360.0;
        const auto norm = normalise.next();

        const auto centre = centreDelayMs (activeMode, delayNow);
        const auto sweep = sweepMs (activeMode, delayNow, depthNow);
        const auto msToSamples = 0.001 * rate;

        double wetSum[2] {}, feedbackSum[2] {};
        auto gainSum = 0.0;

        for (int v = 0; v < maxVoices; ++v)
        {
            const auto gain = voiceGains[(size_t) v].next();
            const auto position = voicePositions[(size_t) v].next() * widthNow;
            const auto offset = voiceOffsets[(size_t) v].next();

            if (gain == 0.0)
                continue;

            const auto voicePhase = phase + offset;
            double y[2];

            for (int ch = 0; ch < 2; ++ch)
            {
                double lfo;

                if (activeMode == dimension)
                    lfo = (ch == 0 ? 1.0 : -1.0) * voiceLfo (activeMode, activeShape, voicePhase, v, 0);
                else
                    lfo = voiceLfo (activeMode, activeShape, voicePhase + (ch == 0 ? 0.0 : rightOffset), v, ch);

                const auto ms = centre + sweep * lfo;
                y[ch] = readLine (ch, std::max (minDelaySamples, ms * msToSamples));

                if (ch == 0)
                    column.delayMs[(size_t) v] = (float) ms;
            }

            feedbackSum[0] += gain * y[0];
            feedbackSum[1] += gain * y[1];
            gainSum += gain;

            // Narrowed and panned: Width 0 is mono; at the edges a voice sits on one side.
            const auto m = 0.5 * (y[0] + y[1]);
            const auto s = 0.5 * (y[0] - y[1]) * widthNow * (1.0 - std::abs (position));
            double left = 1.0, right = 1.0;

            if (position != 0.0)
            {
                const auto angle = (position + 1.0) * 0.25 * pi;
                left = std::cos (angle) * std::numbers::sqrt2;
                right = std::sin (angle) * std::numbers::sqrt2;
            }

            wetSum[0] += gain * (left * m + s);
            wetSum[1] += gain * (right * m - s);
        }

        // Feedback, DC-blocked, back into the lines with the input.
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto summed = gainSum > 0.0 ? feedbackSum[ch] / gainSum : 0.0;
            const auto blocked = summed - dcIn[(size_t) ch] + dcCoefficient * dcOut[(size_t) ch];
            dcIn[(size_t) ch] = summed;
            dcOut[(size_t) ch] = std::abs (blocked) < 1.0e-20 ? 0.0 : blocked;

            const auto back = std::clamp (feedbackNow * dcOut[(size_t) ch], -feedbackLimit, feedbackLimit);
            lines[(size_t) ch][(size_t) writeIndex] = (float) (feed[ch] + back);
        }

        writeIndex = (writeIndex + 1) & lineMask;

        // The wet path's tone.
        const auto lowBlend = lowCutBlend.next();
        const auto highBlend = highCutBlend.next();
        const auto ageNow = age.next();
        lowCutHz.next();
        highCutHz.next();

        double w[2] { wetSum[0] * norm, wetSum[1] * norm };

        for (int ch = 0; ch < 2; ++ch)
        {
            auto x = w[ch];

            const auto low = lowCuts[(size_t) ch].tick (lowCutCoefficients, x).high;
            x += lowBlend * (low - x);

            const auto high = highCuts[(size_t) ch].tick (highCutCoefficients, x).low;
            x += highBlend * (high - x);

            if (ageNow > 0.0)
            {
                const auto drive = 1.0 + 3.0 * ageNow;
                x = std::tanh (drive * x) / drive;

                const auto aged = ageFilters[(size_t) ch].tick (ageCoefficients, x).low;
                x += std::min (1.0, 10.0 * ageNow) * (aged - x);

                noiseState = noiseState * 1664525u + 1013904223u;
                const auto noise = (double) (noiseState >> 8) * (2.0 / 16777216.0) - 1.0;
                x += noise * std::pow (10.0, (-90.0 + 20.0 * ageNow) / 20.0) * 1.7320508075688772;
            }

            w[ch] = x;
        }

        // Keep bass mono: the wet's side below Low cut is taken out.
        const auto monoBlend = bassMonoBlend.next();
        {
            const auto m = 0.5 * (w[0] + w[1]);
            auto s = 0.5 * (w[0] - w[1]);
            const auto highSide = bassMonoFilter.tick (bassMonoCoefficients, s).high;
            s += monoBlend * (highSide - s);
            w[0] = m + s;
            w[1] = m - s;
        }

        // Ducking from the input's peak (docs/chorus.md, "Decisions").
        const auto duckAmount = ducking.next();
        const auto level = std::max (std::abs (in[0]), std::abs (in[1]));
        duckPeak = level > duckPeak ? level : duckPeak + (level - duckPeak) * duckPeakFall;
        auto duckGain = 1.0;

        if (duckAmount > 0.0 || duckDb > 0.0)
        {
            const auto above = 20.0 * std::log10 (std::max (duckPeak, 1.0e-9)) > duckThresholdDb;
            const auto wanted = above ? duckDepthDb * duckAmount : 0.0;

            if (wanted > duckDb)
                duckDb += (wanted - duckDb) * duckAttack;
            else
                duckDb = std::max (wanted, duckDb - duckRelease);

            if (duckDb < 1.0e-6)
                duckDb = 0.0;

            duckGain = std::pow (10.0, -duckDb / 20.0);
        }

        const auto wetGain = wet.next();
        const auto dryGain = dry.next();
        const auto outputGain = output.next();
        const auto bypassAmount = bypass.next();
        const auto listenAmount = listen.next();
        const auto fade = switchGain.next();

        double out[2];

        for (int ch = 0; ch < 2; ++ch)
        {
            const auto wetPath = w[ch] * duckGain * fade;
            auto mixed = wetPath * wetGain + in[ch] * dryGain;

            if (listenAmount > 0.0)
                mixed += listenAmount * (wetPath - mixed);

            mixed *= outputGain;

            if (bypassAmount > 0.0)
                mixed += (in[ch] - mixed) * bypassAmount;

            out[ch] = mixed;
        }

        if (numChannels > 1)
        {
            channels[0][i] = (float) out[0];
            channels[1][i] = (float) out[1];
        }
        else
        {
            channels[0][i] = (float) (0.5 * (out[0] + out[1]));
        }

        // The display's columns.
        if (++columnFill >= std::max (1, (int) (historySeconds * rate / historyColumns.load (std::memory_order_relaxed))))
        {
            column.voices = voices;
            pushHistory (column);
            columnFill = 0;
        }

        phase += cyclesPerSample;

        // Free-running, the phase wraps after many cycles; a whole number of cycles changes nothing.
        if (! activeSync && phase >= 1048576.0)
            phase -= 1048576.0;

        ++now;
    }
}

void ChorusDsp::pushHistory (const HistoryColumn& value) noexcept
{
    const auto write = historyWrite.load (std::memory_order_relaxed);

    if (write - historyRead.load (std::memory_order_acquire) < historyCapacity)
    {
        history[(size_t) (write & (historyCapacity - 1))] = value;
        historyWrite.store (write + 1, std::memory_order_release);
    }
}

int ChorusDsp::popHistory (HistoryColumn* destination, int maxColumns) noexcept
{
    const auto read = historyRead.load (std::memory_order_relaxed);
    const auto count = std::min (historyWrite.load (std::memory_order_acquire) - read, maxColumns);

    for (int i = 0; i < count; ++i)
        destination[i] = history[(size_t) ((read + i) & (historyCapacity - 1))];

    historyRead.store (read + count, std::memory_order_release);
    return count;
}
