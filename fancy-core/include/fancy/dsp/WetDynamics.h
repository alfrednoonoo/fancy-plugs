#pragma once

#include <algorithm>
#include <cmath>

// Ducking and a level gate for a reverb's wet signal, driven by a detector level (a peak sample
// value). The same arithmetic as Fancy Room's (docs/room.md, decision 7, and its Phase 4 Wave 1
// gate), shared so Fancy Verb has them too (docs/verb.md, Phase 4 Wave 2).
namespace fancy::dsp
{

inline double wetCoefficient (double milliseconds, double rate) noexcept
{
    return 1.0 - std::exp (-1.0 / (milliseconds * 0.001 * rate));
}

// A peak detector sets how far down the wet goes: up to 24 dB x amount, fully above -20 dBFS and
// not at all below -50. The reduction follows in dB with 10 ms attack and 300 ms release.
class WetDucker
{
public:
    static constexpr double depthDb = 24.0, fullDb = -20.0, noneDb = -50.0;

    void prepare (double rate) noexcept
    {
        attack = wetCoefficient (10.0, rate);
        release = wetCoefficient (300.0, rate);
        peakRelease = wetCoefficient (50.0, rate);   // rides over the waveform's zero crossings
        reset();
    }

    void reset() noexcept { envelope = peak = 0.0; }

    // The wet gain for this sample. Exactly 1 while amount is 0 and nothing is left to release.
    double process (double level, double amount) noexcept
    {
        peak = level > peak ? level : peak + (level - peak) * peakRelease;

        if (amount <= 0.0 && envelope <= 0.0)
            return 1.0;

        const auto db = 20.0 * std::log10 (std::max (peak, 1.0e-9));
        const auto wanted = depthDb * amount * std::clamp ((db - noneDb) / (fullDb - noneDb), 0.0, 1.0);
        envelope += (wanted - envelope) * (wanted > envelope ? attack : release);

        if (envelope < 1.0e-6)
            envelope = 0.0;

        return std::pow (10.0, -envelope / 20.0);
    }

    bool isIdle() const noexcept { return envelope <= 0.0; }

private:
    double attack = 1.0, release = 1.0, peakRelease = 1.0;
    double envelope = 0.0, peak = 0.0;
};

// Open while the detector is over the threshold; after Hold under it, a linear fade over Decay.
// It opens again over 1 ms.
class WetGate
{
public:
    void prepare (double newRate) noexcept
    {
        rate = newRate;
        openStep = 1.0 / std::max (1.0, 0.001 * rate);
        reset();
    }

    void reset() noexcept
    {
        gain = 1.0;
        holdCount = 0;
    }

    // The wet gain for this sample. Exactly 1 while off and fully open.
    double process (double level, bool on, double thresholdDb, double holdMs, double decayMs) noexcept
    {
        if (! on && gain >= 1.0)
            return 1.0;

        const auto holdSamples = (int) (std::clamp (holdMs, 10.0, 500.0) * 0.001 * rate);
        const auto closeStep = 1.0 / std::max (1.0, std::clamp (decayMs, 5.0, 500.0) * 0.001 * rate);
        const auto open = ! on || 20.0 * std::log10 (std::max (level, 1.0e-9)) > thresholdDb;

        if (open)
        {
            holdCount = holdSamples;
            gain = std::min (1.0, gain + openStep);
        }
        else if (holdCount > 0)
        {
            --holdCount;
            gain = std::min (1.0, gain + openStep);
        }
        else
        {
            gain = std::max (0.0, gain - closeStep);
        }

        return gain;
    }

    double getGain() const noexcept { return gain; }

private:
    double rate = 48000.0, openStep = 1.0, gain = 1.0;
    int holdCount = 0;
};

} // namespace fancy::dsp
