#pragma once

#include <array>
#include <complex>

namespace eq
{

enum class BandType { lowShelf, highShelf, band, notch, lowPass, highPass, bandPass, allPass, tilt };

constexpr int maxBands = 16;
constexpr int maxSections = 4;
constexpr int slopeOrders[] { 1, 2, 3, 4, 6, 8 };
constexpr double butterworthQ = 0.70710678118654752;

constexpr bool hasGain (BandType type) noexcept
{
    return type == BandType::lowShelf || type == BandType::highShelf || type == BandType::band || type == BandType::tilt;
}

constexpr bool hasSlope (BandType type) noexcept
{
    return type == BandType::lowShelf || type == BandType::highShelf || type == BandType::lowPass
        || type == BandType::highPass || type == BandType::tilt;
}

// Which types can be dynamic (Phase 2.1 D). A notch works as a dynamic cut.
constexpr bool supportsDynamics (BandType type) noexcept
{
    return hasGain (type) || type == BandType::notch;
}

// A band's dynamic settings (Phase 2.1 D). Off by default, which leaves the band static.
struct DynamicSpec
{
    bool on = false;
    double thresholdDb = -24.0;
    double rangeDb = -6.0;
    double attackMs = 5.0;
    double releaseMs = 80.0;
    bool below = false;
    bool sidechain = false;

    bool operator== (const DynamicSpec&) const = default;
};

struct BandSpec
{
    BandType type = BandType::band;
    double frequencyHz = 1000.0;
    double gainDb = 0.0;
    double q = 1.0;
    int order = 2;
    bool on = false;

    bool sameShape (const BandSpec& other) const noexcept
    {
        return type == other.type && order == other.order && on == other.on;
    }

    bool operator== (const BandSpec&) const = default;
};

// Normalised so a0 = 1.
struct Section
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
};

struct BandFilter
{
    std::array<Section, maxSections> sections;
    int numSections = 0;

    std::complex<double> responseAt (double radiansPerSample) const noexcept;

    // Squared magnitude at sinSquared = sin²(w / 2), which is cheap across a whole FFT frame.
    double powerAt (double sinSquared) const noexcept;
};

BandFilter design (const BandSpec& spec, double sampleRate) noexcept;

} // namespace eq
