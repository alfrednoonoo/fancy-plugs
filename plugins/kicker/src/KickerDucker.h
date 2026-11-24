#pragma once

#include <algorithm>
#include <cmath>

// Phase 4: Kicker's sidechain Duck (docs/kicker.md, "Phase 4: sidechain Duck"; modelled in
// docs/research/kicker-duck-model.py). The key's peak, held instantly and released over 30 ms,
// sets how far to duck: fully above -12 dBFS, not at all below -24 dBFS, linearly in dB between,
// so a kick's tail doesn't hold the bass down. The duck in dB then moves towards that over a
// 1 ms attack and Release. Contains no JUCE types, so tests can drive it directly.
class KickerDucker
{
public:
    static constexpr double fullDb = -12.0, noneDb = -24.0;
    static constexpr double attackMs = 1.0, peakReleaseMs = 30.0;

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        attack = coefficient (attackMs);
        peakRelease = coefficient (peakReleaseMs);
        setRelease (releaseMs);
        reset();
    }

    void reset() noexcept
    {
        peak = 0.0;
        envelopeDb = 0.0;
    }

    void setDepth (double db) noexcept { depthDb = std::clamp (db, 0.0, 24.0); }

    void setRelease (double ms) noexcept
    {
        releaseMs = std::clamp (ms, 1.0, 5000.0);
        release = coefficient (releaseMs);
    }

    // Nothing to do: no depth and fully let go. process() would give a gain of exactly 1.
    bool isIdle() const noexcept { return depthDb <= 0.0 && envelopeDb == 0.0; }

    // `key` is the key's level per sample (its largest channel, rectified, or a trigger's pulse).
    // Writes the gain to apply per sample.
    void process (const float* key, float* gain, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const auto level = key != nullptr ? (double) std::abs (key[i]) : 0.0;
            peak = level > peak ? level : peak + (level - peak) * peakRelease;

            const auto db = 20.0 * std::log10 (std::max (peak, 1.0e-9));
            const auto wanted = depthDb * std::clamp ((db - noneDb) / (fullDb - noneDb), 0.0, 1.0);
            envelopeDb += (wanted - envelopeDb) * (wanted > envelopeDb ? attack : release);

            // Let go completely once it's inaudible, so an idle Duck costs nothing.
            if (wanted == 0.0 && envelopeDb < 1.0e-4)
                envelopeDb = 0.0;

            gain[i] = envelopeDb == 0.0 ? 1.0f : (float) std::pow (10.0, -envelopeDb / 20.0);
        }

        if (peak < 1.0e-9)
            peak = 0.0;
    }

    double getEnvelopeDb() const noexcept { return envelopeDb; }

private:
    double coefficient (double ms) const noexcept { return 1.0 - std::exp (-1.0 / (ms * 0.001 * sampleRate)); }

    double sampleRate = 48000.0;
    double depthDb = 0.0, releaseMs = 120.0;
    double attack = 0.02, release = 0.0002, peakRelease = 0.0007;
    double peak = 0.0, envelopeDb = 0.0;
};
