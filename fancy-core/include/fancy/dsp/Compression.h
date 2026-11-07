#pragma once

#include <algorithm>
#include <cmath>

// The compressor parts shared by Fancy Comp and Fancy MultiComp. See docs/comp.md.
namespace fancy::dsp
{

inline double timeCoefficient (double milliseconds, double sampleRate) noexcept
{
    const auto samples = milliseconds * 0.001 * sampleRate;
    return samples > 0.0 ? 1.0 - std::exp (-1.0 / samples) : 1.0;
}

// The static curve. Returns gain reduction as a positive number of dB. With weird on, the
// knee starts at the threshold rather than being centred on it.
inline double computeReduction (double levelDb, double thresholdDb, double slope, double kneeDb, bool weird) noexcept
{
    const auto over = levelDb - thresholdDb;

    if (kneeDb <= 0.0)
        return over > 0.0 ? slope * over : 0.0;

    if (weird)
    {
        if (over <= 0.0)
            return 0.0;

        return over >= kneeDb ? slope * (over - 0.5 * kneeDb)
                              : slope * over * over / (2.0 * kneeDb);
    }

    const auto halfKnee = 0.5 * kneeDb;

    if (over <= -halfKnee)
        return 0.0;

    if (over >= halfKnee)
        return slope * over;

    const auto x = over + halfKnee;
    return slope * x * x / (2.0 * kneeDb);
}

struct Ballistics
{
    void set (double attackMs, double releaseMs, bool attack90, bool autoReleaseOn, bool linearReleaseOn, double sampleRate) noexcept
    {
        constexpr double autoReleaseRatio = 0.1;
        constexpr double attack90Divisor = 2.302585;
        constexpr double decibelsPerNeper = 8.685889638;

        attack = timeCoefficient (attack90 ? attackMs / attack90Divisor : attackMs, sampleRate);
        release = timeCoefficient (releaseMs, sampleRate);
        fastRelease = timeCoefficient (releaseMs * autoReleaseRatio, sampleRate);

        // A constant rate of one neper per time constant.
        const auto releaseSamples = releaseMs * 0.001 * sampleRate;
        linearStep = releaseSamples > 0.0 ? decibelsPerNeper / releaseSamples : 1.0e9;

        autoRelease = autoReleaseOn;
        linearRelease = linearReleaseOn;
    }

    double attack = 1.0, release = 1.0, fastRelease = 1.0, linearStep = 1.0;
    bool autoRelease = false, linearRelease = false;
};

// Attack towards a higher target, release towards a lower one. Auto release runs at a tenth
// of the set time down to a slow envelope, which follows at the set time.
struct EnvelopeFollower
{
    void reset() noexcept
    {
        envelope = slowEnvelope = 0.0;
    }

    double follow (const Ballistics& times, double target) noexcept
    {
        if (target > envelope)
            envelope += times.attack * (target - envelope);
        else if (times.autoRelease)
            envelope += times.fastRelease * (std::max (target, slowEnvelope) - envelope);
        else if (times.linearRelease)
            envelope = std::max (target, envelope - times.linearStep);
        else
            envelope += times.release * (target - envelope);

        if (times.autoRelease)
            slowEnvelope += times.release * (target - slowEnvelope);

        return envelope;
    }

    double envelope = 0.0, slowEnvelope = 0.0;
};

} // namespace fancy::dsp
