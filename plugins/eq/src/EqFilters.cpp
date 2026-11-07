#include "EqFilters.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace eq
{

namespace
{
constexpr double pi = std::numbers::pi;
constexpr double maxFrequencyRatio = 0.48;

// Analog section: (n2 s² + n1 s + n0) / (s² + d1 s + d0), with s in radians per sample.
struct Analog
{
    double n2, n1, n0, d1, d0;

    double powerAt (double w) const noexcept
    {
        const auto numeratorRe = n0 - n2 * w * w;
        const auto numeratorIm = n1 * w;
        const auto denominatorRe = d0 - w * w;
        const auto denominatorIm = d1 * w;

        return (numeratorRe * numeratorRe + numeratorIm * numeratorIm)
             / (denominatorRe * denominatorRe + denominatorIm * denominatorIm);
    }
};

struct Poles
{
    double a1, a2;

    // Coefficients of |A|² in the phi basis: A0 phi0 + A1 phi1 + A2 phi2.
    double upper0() const noexcept { return (1.0 + a1 + a2) * (1.0 + a1 + a2); }
    double upper1() const noexcept { return (1.0 - a1 + a2) * (1.0 - a1 + a2); }
    double upper2() const noexcept { return -4.0 * a2; }
};

struct Phi
{
    explicit Phi (double w) noexcept
    {
        const auto s = std::sin (w * 0.5);
        phi1 = s * s;
        phi0 = 1.0 - phi1;
        phi2 = 4.0 * phi0 * phi1;
    }

    double phi0, phi1, phi2;
};

// Impulse-invariant mapping of the analog poles.
Poles mapPoles (double d1, double d0) noexcept
{
    const auto discriminant = d1 * d1 * 0.25 - d0;
    const auto decay = std::exp (-d1 * 0.5);

    if (discriminant < 0.0)
        return { -2.0 * decay * std::cos (std::sqrt (-discriminant)), decay * decay };

    return { -2.0 * decay * std::cosh (std::sqrt (discriminant)), decay * decay };
}

Section invert (const Section& s) noexcept
{
    return { 1.0 / s.b0, s.a1 / s.b0, s.a2 / s.b0, s.b1 / s.b0, s.b2 / s.b0 };
}

// Spectral factor of B0 phi0 + B1 phi1 + B2 phi2.
Section factor (double upper0, double upper1, double upper2, const Poles& poles) noexcept
{
    const auto root0 = std::sqrt (std::max (upper0, 0.0));
    const auto root1 = std::sqrt (std::max (upper1, 0.0));
    const auto w = 0.5 * (root0 + root1);
    const auto b0 = 0.5 * (w + std::sqrt (std::max (w * w + upper2, 0.0)));

    return { b0, 0.5 * (root0 - root1), -upper2 / (4.0 * b0), poles.a1, poles.a2 };
}

// Vicanek, "Matched Second Order Digital Filters". Unity at DC, a peak of `gain` at w.
Section matchedBoost (double w, double gain, double q) noexcept
{
    const auto poles = mapPoles (w / q, w * w);
    const auto a0 = poles.upper0(), a1 = poles.upper1(), a2 = poles.upper2();
    const Phi phi (w);

    const auto r1 = (a0 * phi.phi0 + a1 * phi.phi1 + a2 * phi.phi2) * gain * gain;
    const auto r2 = (-a0 + a1 + 4.0 * (phi.phi0 - phi.phi1) * a2) * gain * gain;

    const auto b2 = (r1 - r2 * phi.phi1 - a0) / (4.0 * phi.phi1 * phi.phi1);
    const auto b1 = r2 + a0 + 4.0 * (phi.phi1 - phi.phi0) * b2;

    return factor (a0, b1, b2, poles);
}

// Vicanek's poles, with the numerator matched at DC and at two points high in the band, so the
// error stays small where a cascade would add it up. Matching at Nyquist
// itself leaves 1 to 2 dB of error around 20 kHz at 44.1 kHz.
Section matchedLowPass (double w, double q) noexcept
{
    const auto poles = mapPoles (w / q, w * w);
    const auto a0 = poles.upper0(), a1 = poles.upper1(), a2 = poles.upper2();

    const Analog analog { 0.0, 0.0, w * w, w / q, w * w };

    const auto target = [&] (const Phi& phi, double wMatch)
    {
        return (a0 * phi.phi0 + a1 * phi.phi1 + a2 * phi.phi2) * analog.powerAt (wMatch) - a0 * phi.phi0;
    };

    const auto wLow = std::clamp (w, 0.6 * pi, 0.7 * pi);
    const auto wHigh = 0.9 * pi;
    const Phi low (wLow), high (wHigh);

    const auto r1 = target (low, wLow);
    const auto r2 = target (high, wHigh);
    const auto determinant = low.phi1 * high.phi2 - low.phi2 * high.phi1;

    return factor (a0,
                   (r1 * high.phi2 - low.phi2 * r2) / determinant,
                   (low.phi1 * r2 - high.phi1 * r1) / determinant,
                   poles);
}

Section matchedHighPass (double w, double q) noexcept
{
    const auto poles = mapPoles (w / q, w * w);
    const Phi phi (w);

    const auto b0 = q * std::sqrt (poles.upper0() * phi.phi0 + poles.upper1() * phi.phi1 + poles.upper2() * phi.phi2) / (4.0 * phi.phi1);

    return { b0, -2.0 * b0, b0, poles.a1, poles.a2 };
}

Section matchedBandPass (double w, double q) noexcept
{
    const auto poles = mapPoles (w / q, w * w);
    const auto a0 = poles.upper0(), a1 = poles.upper1(), a2 = poles.upper2();
    const Phi phi (w);

    const auto r1 = a0 * phi.phi0 + a1 * phi.phi1 + a2 * phi.phi2;
    const auto r2 = -a0 + a1 + 4.0 * (phi.phi0 - phi.phi1) * a2;

    const auto b2Upper = (r1 - r2 * phi.phi1) / (4.0 * phi.phi1 * phi.phi1);
    const auto b1Upper = r2 + 4.0 * (phi.phi1 - phi.phi0) * b2Upper;

    const auto b1 = -0.5 * std::sqrt (std::max (b1Upper, 0.0));
    const auto b0 = 0.5 * (std::sqrt (std::max (b2Upper + b1 * b1, 0.0)) - b1);

    return { b0, b1, -b0 - b1, poles.a1, poles.a2 };
}

Section notch (double w, double q) noexcept
{
    const auto poles = mapPoles (w / q, w * w);
    const auto cosW = std::cos (w);
    const auto scale = (1.0 + poles.a1 + poles.a2) / (2.0 - 2.0 * cosW);

    return { scale, -2.0 * cosW * scale, scale, poles.a1, poles.a2 };
}

Section allPass (double w, double q) noexcept
{
    const auto poles = mapPoles (w / q, w * w);
    return { poles.a2, poles.a1, 1.0, poles.a1, poles.a2 };
}

// Poles mapped, numerator matched to the analog magnitude at DC, wMatch and Nyquist.
Section matchedSection (const Analog& analog, double wMatch) noexcept
{
    const auto poles = mapPoles (analog.d1, analog.d0);
    const auto a0 = poles.upper0(), a1 = poles.upper1(), a2 = poles.upper2();
    const Phi phi (wMatch);

    const auto b0 = a0 * analog.powerAt (0.0);
    const auto b1 = a1 * analog.powerAt (pi);
    const auto r = (a0 * phi.phi0 + a1 * phi.phi1 + a2 * phi.phi2) * analog.powerAt (wMatch);

    return factor (b0, b1, (r - b0 * phi.phi0 - b1 * phi.phi1) / phi.phi2, poles);
}

// Mapping a pole above Nyquist isn't possible, so the lower of the pole and zero radii
// always becomes the digital pole, and the section is inverted when they've been swapped.
Section matchedShelf (double poleRadius, double zeroRadius, double damping, double w, double scale) noexcept
{
    const auto swap = zeroRadius < poleRadius;
    const auto low = swap ? zeroRadius : poleRadius;
    const auto high = swap ? poleRadius : zeroRadius;

    const Analog analog { 1.0, 2.0 * damping * high, high * high, 2.0 * damping * low, low * low };

    auto section = matchedSection (analog, std::min (w, 0.95 * pi));

    if (swap)
        section = invert (section);

    section.b0 *= scale;
    section.b1 *= scale;
    section.b2 *= scale;
    return section;
}

Section firstOrderLowPass (double w) noexcept
{
    const auto a1 = -std::exp (-w);
    const auto dc = 1.0 + a1;
    const auto nyquist = (1.0 - a1) * w / std::hypot (pi, w);

    return { 0.5 * (dc + nyquist), 0.5 * (dc - nyquist), 0.0, a1, 0.0 };
}

Section firstOrderHighPass (double w) noexcept
{
    const auto a1 = -std::exp (-w);
    const auto nyquist = (1.0 - a1) * pi / std::hypot (pi, w);

    return { 0.5 * nyquist, -0.5 * nyquist, 0.0, a1, 0.0 };
}

// (s + zero) / (s + pole), matched at DC and Nyquist, then scaled.
Section firstOrderShelf (double pole, double zero, double scale) noexcept
{
    const auto swap = zero < pole;
    const auto low = swap ? zero : pole;
    const auto high = swap ? pole : zero;

    const auto a1 = -std::exp (-low);
    const auto dc = (1.0 + a1) * high / low;
    const auto nyquist = (1.0 - a1) * std::hypot (pi, high) / std::hypot (pi, low);

    Section section { 0.5 * (dc + nyquist), 0.5 * (dc - nyquist), 0.0, a1, 0.0 };

    if (swap)
        section = invert (section);

    section.b0 *= scale;
    section.b1 *= scale;
    return section;
}

// Damping (1 / 2Q) of each conjugate pair of a Butterworth filter, highest Q last.
struct Butterworth
{
    explicit Butterworth (int order) noexcept
    {
        pairs = order / 2;

        for (int k = 0; k < pairs; ++k)
        {
            const auto angle = (order % 2) != 0 ? pi * (double) (k + 1) / (double) order
                                                : pi * (double) (2 * k + 1) / (double) (2 * order);

            damping[(size_t) k] = std::cos (angle);
        }
    }

    // The last pair has the highest Q, so 0.707 leaves the filter Butterworth.
    void applyResonance (double q) noexcept
    {
        if (pairs > 0)
            damping[(size_t) (pairs - 1)] = std::max (damping[(size_t) (pairs - 1)], 1.0e-6) * butterworthQ / q;
    }

    std::array<double, maxSections> damping {};
    int pairs = 0;
};

void addShelf (BandFilter& filter, const BandSpec& spec, double w, bool high, double gain) noexcept
{
    Butterworth butterworth (spec.order);
    butterworth.applyResonance (spec.q);

    const auto radius = std::pow (gain, 1.0 / (2.0 * spec.order));
    const auto poleRadius = (high ? radius : 1.0 / radius) * w;
    const auto zeroRadius = (high ? 1.0 / radius : radius) * w;

    // A high shelf rises from 1 to gain, and each section carries its own share of that.
    if ((spec.order % 2) != 0)
        filter.sections[(size_t) filter.numSections++]
            = firstOrderShelf (poleRadius, zeroRadius, high ? std::pow (gain, 1.0 / (double) spec.order) : 1.0);

    const auto sectionScale = high ? std::pow (gain, 2.0 / (double) spec.order) : 1.0;

    for (int k = 0; k < butterworth.pairs; ++k)
        filter.sections[(size_t) filter.numSections++]
            = matchedShelf (poleRadius, zeroRadius, butterworth.damping[(size_t) k], w, sectionScale);
}

void addPass (BandFilter& filter, const BandSpec& spec, double w, bool high) noexcept
{
    Butterworth butterworth (spec.order);
    butterworth.applyResonance (spec.q);

    if ((spec.order % 2) != 0)
        filter.sections[(size_t) filter.numSections++] = high ? firstOrderHighPass (w) : firstOrderLowPass (w);

    for (int k = 0; k < butterworth.pairs; ++k)
    {
        const auto q = 0.5 / butterworth.damping[(size_t) k];
        filter.sections[(size_t) filter.numSections++] = high ? matchedHighPass (w, q) : matchedLowPass (w, q);
    }
}

bool isUsable (const Section& s) noexcept
{
    return std::isfinite (s.b0) && std::isfinite (s.b1) && std::isfinite (s.b2)
        && std::isfinite (s.a1) && std::isfinite (s.a2)
        && std::abs (s.a2) < 1.0 && std::abs (s.a1) < 1.0 + s.a2;
}
} // namespace

std::complex<double> BandFilter::responseAt (double radiansPerSample) const noexcept
{
    const auto z1 = std::polar (1.0, -radiansPerSample);
    const auto z2 = z1 * z1;

    std::complex<double> response = 1.0;

    for (int i = 0; i < numSections; ++i)
    {
        const auto& s = sections[(size_t) i];
        response *= (s.b0 + s.b1 * z1 + s.b2 * z2) / (1.0 + s.a1 * z1 + s.a2 * z2);
    }

    return response;
}

double BandFilter::powerAt (double sinSquared) const noexcept
{
    const auto phi1 = sinSquared;
    const auto phi0 = 1.0 - phi1;
    const auto phi2 = 4.0 * phi0 * phi1;

    auto power = 1.0;

    for (int i = 0; i < numSections; ++i)
    {
        const auto& s = sections[(size_t) i];

        const auto numerator = (s.b0 + s.b1 + s.b2) * (s.b0 + s.b1 + s.b2) * phi0
                             + (s.b0 - s.b1 + s.b2) * (s.b0 - s.b1 + s.b2) * phi1
                             - 4.0 * s.b0 * s.b2 * phi2;

        const auto denominator = (1.0 + s.a1 + s.a2) * (1.0 + s.a1 + s.a2) * phi0
                               + (1.0 - s.a1 + s.a2) * (1.0 - s.a1 + s.a2) * phi1
                               - 4.0 * s.a2 * phi2;

        power *= std::max (numerator, 0.0) / std::max (denominator, 1.0e-300);
    }

    return power;
}

BandFilter design (const BandSpec& spec, double sampleRate) noexcept
{
    BandFilter filter;

    if (! spec.on)
        return filter;

    const auto hz = std::clamp (spec.frequencyHz, 1.0, maxFrequencyRatio * sampleRate);
    const auto w = 2.0 * pi * hz / sampleRate;
    const auto gain = std::pow (10.0, spec.gainDb / 20.0);

    switch (spec.type)
    {
        case BandType::band:
        {
            const auto boost = std::pow (10.0, std::abs (spec.gainDb) / 20.0);
            const auto section = matchedBoost (w, boost, spec.q * std::sqrt (boost));
            filter.sections[0] = spec.gainDb < 0.0 ? invert (section) : section;
            filter.numSections = 1;
            break;
        }

        case BandType::notch:
            filter.sections[0] = notch (w, spec.q);
            filter.numSections = 1;
            break;

        case BandType::bandPass:
            filter.sections[0] = matchedBandPass (w, spec.q);
            filter.numSections = 1;
            break;

        case BandType::allPass:
            filter.sections[0] = allPass (w, spec.q);
            filter.numSections = 1;
            break;

        case BandType::lowPass:   addPass (filter, spec, w, false); break;
        case BandType::highPass:  addPass (filter, spec, w, true); break;
        case BandType::lowShelf:  addShelf (filter, spec, w, false, gain); break;
        case BandType::highShelf: addShelf (filter, spec, w, true, gain); break;

        case BandType::tilt:
        {
            addShelf (filter, spec, w, true, gain);

            const auto scale = 1.0 / std::sqrt (gain);
            filter.sections[0].b0 *= scale;
            filter.sections[0].b1 *= scale;
            filter.sections[0].b2 *= scale;
            break;
        }
    }

    for (int i = 0; i < filter.numSections; ++i)
        if (! isUsable (filter.sections[(size_t) i]))
            filter.sections[(size_t) i] = Section {};

    return filter;
}

} // namespace eq
