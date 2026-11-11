#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

// Fancy Sature's curves (docs/sature.md). Plain C++, so the build-time gain-table generator
// (tools/sature-gaintable) runs exactly the curves the plugin runs.
namespace fancy::dsp::saturation
{

enum Type { tape, tube, transistor, clip, numTypes };

constexpr double maxBias = 0.5;          // the input shift at even 100 %
constexpr double tubeNegativeLimit = 1.4; // Tube's negative half saturates later and softer
constexpr double transistorKnee = 0.5;    // linear to 0.5, knee to 1.5
constexpr double clipKnee = 0.9;          // linear to 0.9, knee to 1.1
constexpr double hardKnee = 0.97;         // what odd 100 % blends towards
constexpr double adaaThreshold = 1.0e-6;

// Tape's high-frequency loss: off up to Drive 12 dB, a one-pole low-pass at 12 kHz fully in
// by Drive 36 dB.
constexpr double tapeLossStartDb = 12.0, tapeLossEndDb = 36.0, tapeLossHz = 12000.0;

// Tone's tilt pivots here.
constexpr double tonePivotHz = 1000.0;

// u / sqrt(1 + (u/a)^2): slope 1 at zero, saturating at ±a.
inline double soft (double u, double a) noexcept
{
    const auto r = u / a;
    return u / std::sqrt (1.0 + r * r);
}

// Its antiderivative, a^2 (sqrt(1 + (u/a)^2) - 1), written without the cancellation near zero.
inline double softIntegral (double u, double a) noexcept
{
    const auto r = u / a;
    return u * u / (std::sqrt (1.0 + r * r) + 1.0);
}

// Linear to k, a quadratic knee to 2 - k, then flat at 1. Slope-continuous.
inline double knee (double u, double k) noexcept
{
    const auto a = std::abs (u);
    const auto end = 2.0 - k;
    const auto y = a <= k ? a : (a >= end ? 1.0 : 1.0 - (end - a) * (end - a) / (4.0 * (1.0 - k)));
    return std::copysign (y, u);
}

inline double kneeIntegral (double u, double k) noexcept
{
    const auto a = std::abs (u);
    const auto end = 2.0 - k;
    const auto w = 1.0 - k;
    const auto linear = std::min (a, k);
    auto result = 0.5 * linear * linear;

    if (a > k)
    {
        const auto t = std::min (a, end);
        const auto span = end - k;
        const auto rest = end - t;
        result += (t - k) - (span * span * span - rest * rest * rest) / (12.0 * w);
    }

    if (a > end)
        result += a - end;

    return result;
}

// A blend of the basic curves: what one Type at one Character is, or a crossfade between two
// Types. Blending curves blends their antiderivatives the same way, so ADAA stays exact.
struct Curve
{
    double soft = 1.0, transistor = 0.0, clip = 0.0, hard = 0.0; // weights, summing to 1
    double negativeLimit = 1.0;                                  // the soft curve's limit below zero
    double bias = 0.0;                                           // added to the input, compensated after
    double tape = 1.0;                                           // how much of Tape's high-frequency loss applies

    double shape (double u) const noexcept
    {
        auto y = 0.0;

        if (soft != 0.0)       y += soft * ::fancy::dsp::saturation::soft (u, u < 0.0 ? negativeLimit : 1.0);
        if (transistor != 0.0) y += transistor * knee (u, transistorKnee);
        if (clip != 0.0)       y += clip * knee (u, clipKnee);
        if (hard != 0.0)       y += hard * knee (u, hardKnee);

        return y;
    }

    double integral (double u) const noexcept
    {
        auto y = 0.0;

        if (soft != 0.0)       y += soft * softIntegral (u, u < 0.0 ? negativeLimit : 1.0);
        if (transistor != 0.0) y += transistor * kneeIntegral (u, transistorKnee);
        if (clip != 0.0)       y += clip * kneeIntegral (u, clipKnee);
        if (hard != 0.0)       y += hard * kneeIntegral (u, hardKnee);

        return y;
    }

    bool sameShape (const Curve& other) const noexcept
    {
        return soft == other.soft && transistor == other.transistor && clip == other.clip && hard == other.hard
            && negativeLimit == other.negativeLimit;
    }
};

// character: -1 is even 100 %, +1 odd 100 %. The even half shifts the input (asymmetry, even
// harmonics); the odd half removes all asymmetry and blends towards a symmetric hard knee
// (Decision 4).
inline Curve makeCurve (int type, double character) noexcept
{
    Curve c;
    c.soft = (type == tape || type == tube) ? 1.0 : 0.0;
    c.transistor = type == transistor ? 1.0 : 0.0;
    c.clip = type == clip ? 1.0 : 0.0;
    c.hard = 0.0;
    c.negativeLimit = type == tube ? tubeNegativeLimit : 1.0;
    c.tape = type == tape ? 1.0 : 0.0;

    const auto even = std::max (0.0, -character);
    const auto odd = std::max (0.0, character);

    c.soft *= 1.0 - odd;
    c.transistor *= 1.0 - odd;
    c.clip *= 1.0 - odd;
    c.hard = odd;
    c.negativeLimit += (1.0 - c.negativeLimit) * odd;
    c.bias = maxBias * even;

    return c;
}

inline Curve blend (const Curve& from, const Curve& to, double t) noexcept
{
    if (t <= 0.0) return from;
    if (t >= 1.0) return to;

    const auto mix = [t] (double a, double b) { return a + (b - a) * t; };

    Curve c;
    c.soft = mix (from.soft, to.soft);
    c.transistor = mix (from.transistor, to.transistor);
    c.clip = mix (from.clip, to.clip);
    c.hard = mix (from.hard, to.hard);
    c.negativeLimit = mix (from.negativeLimit, to.negativeLimit);
    c.bias = mix (from.bias, to.bias);
    c.tape = mix (from.tape, to.tape);
    return c;
}

// First-order antiderivative anti-aliasing of a curve with its bias. The result is the curve
// at the midpoint of each step, so it lags the input by half a sample, and the bias's own
// output (its DC) is taken off.
struct Adaa
{
    double previousInput = 0.0;
    double previousIntegral = 0.0;

    void reset() noexcept { previousInput = previousIntegral = 0.0; }

    // shapeChanged: the curve's shape differs from the previous sample's, so the stored
    // antiderivative is recomputed with the new one. dc is curve.shape (curve.bias).
    double process (const Curve& curve, double u, bool shapeChanged, double dc) noexcept
    {
        const auto in = u + curve.bias;

        if (shapeChanged)
            previousIntegral = curve.integral (previousInput);

        const auto integral = curve.integral (in);
        const auto difference = in - previousInput;

        const auto y = std::abs (difference) < adaaThreshold ? curve.shape (0.5 * (in + previousInput))
                                                             : (integral - previousIntegral) / difference;

        previousInput = in;
        previousIntegral = integral;
        return y - dc;
    }
};

// One-pole low-pass for Tape's loss, mixed in by amount (0 leaves the signal untouched).
struct TapeLoss
{
    double state = 0.0;

    void reset() noexcept { state = 0.0; }

    double process (double x, double coefficient, double amount) noexcept
    {
        if (amount == 0.0)
        {
            // Follows the input, so it starts from where the signal is when the loss comes in.
            state = x;
            return x;
        }

        state += coefficient * (x - state);
        return x + amount * (state - x);
    }

    static double coefficientFor (double sampleRate) noexcept
    {
        return 1.0 - std::exp (-2.0 * std::numbers::pi * tapeLossHz / sampleRate);
    }

    static double amountFor (double driveDb, double tapeWeight) noexcept
    {
        return tapeWeight * std::clamp ((driveDb - tapeLossStartDb) / (tapeLossEndDb - tapeLossStartDb), 0.0, 1.0);
    }
};

// A first-order tilt pivoting at 1 kHz. totalDb is the whole tilt: the top rises by half of
// it and the bottom falls by half (Decision 5).
struct TiltCoefficients
{
    double b0 = 1.0, b1 = 0.0, a1 = 0.0;
    bool flat = true;

    void set (double totalDb, double sampleRate) noexcept
    {
        flat = totalDb == 0.0;

        const auto g = std::pow (10.0, totalDb / 40.0);
        const auto w = std::tan (std::numbers::pi * std::min (tonePivotHz, 0.45 * sampleRate) / sampleRate);
        const auto a0 = 1.0 + w * g;

        b0 = (g + w) / a0;
        b1 = (w - g) / a0;
        a1 = (w * g - 1.0) / a0;
    }
};

struct Tilt
{
    double state = 0.0;

    void reset() noexcept { state = 0.0; }

    double process (const TiltCoefficients& c, double x) noexcept
    {
        if (c.flat)
        {
            // A flat tilt is the identity, whose state is zero.
            state = 0.0;
            return x;
        }

        const auto y = c.b0 * x + state;
        state = c.b1 * x - c.a1 * y;
        return y;
    }
};

// The auto-gain table's grid (Decision 6).
constexpr int tableDrives = 37;      // 0 to 36 dB in 1 dB steps
constexpr int tableCharacters = 21;  // -100 to +100 % in steps of 10
constexpr int tableTones = 13;       // -6 to +6 dB in 1 dB steps
constexpr double tableNoiseRms = 0.125892541179416721; // -18 dBFS
constexpr double tableRate = 48000.0;

} // namespace fancy::dsp::saturation
