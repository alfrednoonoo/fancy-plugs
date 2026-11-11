#pragma once

#include <fancy/dsp/Biquad.h>
#include <fancy/dsp/Svf.h>

#include <algorithm>
#include <cmath>
#include <numbers>

// Fancy Kicker's Sub mode, Divider engine (docs/kicker-sub.md): follows the zero crossings of
// the band below Cutoff and plays a sine at half (or a quarter of) its frequency, locked to
// every second (or fourth) crossing, at the band's own level. Zero latency. No JUCE types.
class SubGenerator
{
public:
    struct Settings
    {
        double cutoffHz = 100.0;
        int divisions = 2;          // 2: -1 oct, 4: -2 oct
        double sensitivityGain = 0.01; // -40 dB
        double lowpassHz = 120.0;
    };

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        attack = 1.0 - std::exp (-1.0 / (0.001 * sampleRate));
        release = 1.0 - std::exp (-1.0 / (0.030 * sampleRate));
        gateStep = 1.0 / (0.005 * sampleRate);
        highPass20.set (20.0, std::numbers::sqrt2 * 0.5, sampleRate);
        subsonic.setButterworthHighPass (15.0, sampleRate);
        reset();
    }

    void reset()
    {
        for (auto& f : detector) f.reset();
        for (auto& f : output) f.reset();
        dcBlock.reset();
        subsonic.reset();

        envelope = 0.0;
        previous = 0.0;
        armed = false;
        candidate = -1.0;
        lastCrossing = -1.0;
        lastPeriod = -1.0;
        previousPeriod = -1.0;
        crossings = 0;
        phase = 0.0;
        increment = 0.0;
        correction = 0.0;
        correctionLeft = 0;
        running = false;
        gate = 0.0;
        now = 0.0;
    }

    // Recomputes the filters. Call when a setting moves; cheap enough to call per sample then.
    void setSettings (const Settings& newSettings) noexcept
    {
        settings = newSettings;
        settings.cutoffHz = std::clamp (settings.cutoffHz, 20.0, 0.45 * sampleRate);
        settings.lowpassHz = std::clamp (settings.lowpassHz, 20.0, 0.45 * sampleRate);

        // 4th-order Butterworth sections.
        detectorCoefficients[0].set (settings.cutoffHz, 0.54119610014619698, sampleRate);
        detectorCoefficients[1].set (settings.cutoffHz, 1.3065629648763766, sampleRate);
        outputCoefficients[0].set (settings.lowpassHz, 0.54119610014619698, sampleRate);
        outputCoefficients[1].set (settings.lowpassHz, 1.3065629648763766, sampleRate);
    }

    // One input sample in, one sub sample out.
    double process (double input) noexcept
    {
        now += 1.0;

        // The detector band: below Cutoff, without DC.
        auto band = dcBlock.tick (highPass20, input).high;
        band = detector[0].tick (detectorCoefficients[0], band).low;
        band = detector[1].tick (detectorCoefficients[1], band).low;

        const auto magnitude = std::abs (band);
        envelope += (magnitude > envelope ? attack : release) * (magnitude - envelope);

        trackCrossings (band);

        // The gate: open while the band is above Sensitivity, fading over 5 ms.
        const auto open = envelope > settings.sensitivityGain;
        gate = std::clamp (gate + (open && running ? gateStep : -gateStep), 0.0, 1.0);

        if (! open && gate <= 0.0)
            running = false;

        auto sub = 0.0;

        if (running)
        {
            auto step = increment;

            if (correctionLeft > 0)
            {
                step += correction;
                --correctionLeft;
            }

            phase += step;
            phase -= std::floor (phase);
            sub = std::sin (2.0 * std::numbers::pi * phase) * envelope * gate;
        }

        sub = output[0].tick (outputCoefficients[0], sub).low;
        sub = output[1].tick (outputCoefficients[1], sub).low;
        return subsonic.process (sub);
    }

    bool isRunning() const noexcept { return running; }
    double getEnvelope() const noexcept { return envelope; }

private:
    void trackCrossings (double band) noexcept
    {
        const auto hysteresis = 0.1 * envelope;

        // Where an upward zero crossing happened, between the last sample and this one.
        if (previous <= 0.0 && band > 0.0)
            candidate = now - 1.0 + (-previous) / (band - previous);

        previous = band;

        if (band < -hysteresis)
            armed = true;

        if (! armed || band <= hysteresis || candidate < 0.0 || envelope <= settings.sensitivityGain)
            return;

        // A confirmed crossing.
        armed = false;
        const auto crossing = candidate;
        candidate = -1.0;

        const auto period = lastCrossing >= 0.0 ? crossing - lastCrossing : -1.0;
        lastCrossing = crossing;

        const auto minPeriod = sampleRate / 400.0, maxPeriod = sampleRate / 15.0;
        const auto validPeriod = period >= minPeriod && period <= maxPeriod;

        if (validPeriod)
        {
            previousPeriod = lastPeriod;
            lastPeriod = period;
        }

        // The next period, predicted from the last two: a kick's pitch falls roughly
        // geometrically, so the trend carries on. Clamped, so one odd crossing can't throw it.
        auto usePeriod = lastPeriod > 0.0 ? lastPeriod : sampleRate / (0.8 * settings.cutoffHz);

        if (lastPeriod > 0.0 && previousPeriod > 0.0)
            usePeriod = lastPeriod * std::clamp (lastPeriod / previousPeriod, 0.77, 1.3);
        increment = 1.0 / ((double) settings.divisions * usePeriod);

        // The detector's filters delay the band. Its crossing really happened that much
        // earlier, so the sub lines up with the kick itself.
        const auto delay = groupDelaySamples (sampleRate / usePeriod);
        const auto since = now - crossing + delay;

        if (! running)
        {
            running = true;
            crossings = 0;
            phase = since * increment;
            phase -= std::floor (phase);
            correctionLeft = 0;
            return;
        }

        crossings = (crossings + 1) % settings.divisions;

        // Where a perfect divider's phase would be now, and the error spread over the next period.
        auto target = (double) crossings / (double) settings.divisions + since * increment;
        target -= std::floor (target);

        auto error = target - phase;
        error -= std::round (error);

        correctionLeft = std::max (1, (int) usePeriod);
        correction = error / (double) correctionLeft;
    }

    // The detector's delay at a frequency, from its phase: the 4th-order low-pass at Cutoff and
    // the 2nd-order high-pass at 20 Hz.
    double groupDelaySamples (double hz) const noexcept
    {
        const auto lowPassPhase = [] (double ratio, double q) { return std::atan2 (ratio / q, 1.0 - ratio * ratio); };

        const auto r = hz / settings.cutoffHz;
        const auto lag = lowPassPhase (r, 0.54119610014619698) + lowPassPhase (r, 1.3065629648763766);
        const auto lead = std::numbers::pi - lowPassPhase (hz / 20.0, std::numbers::sqrt2 * 0.5);
        const auto radians = lag - lead;

        // Negative where the 20 Hz high-pass's lead wins: the band's crossing came early.
        return radians / (2.0 * std::numbers::pi * hz) * sampleRate;
    }

    double sampleRate = 48000.0;
    Settings settings;

    fancy::dsp::SvfCoefficients highPass20, detectorCoefficients[2], outputCoefficients[2];
    fancy::dsp::Svf dcBlock, detector[2], output[2];
    fancy::dsp::Biquad subsonic;

    double attack = 0.0, release = 0.0, gateStep = 0.0;
    double envelope = 0.0, previous = 0.0, candidate = -1.0, lastCrossing = -1.0, lastPeriod = -1.0, previousPeriod = -1.0;
    bool armed = false, running = false;
    int crossings = 0;
    double phase = 0.0, increment = 0.0, correction = 0.0;
    int correctionLeft = 0;
    double gate = 0.0;
    double now = 0.0;
};
