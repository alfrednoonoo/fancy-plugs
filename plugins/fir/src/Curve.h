#pragma once

#include "FirShared.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace fir
{

struct CurvePoint
{
    double hz = 1000.0, db = 0.0;

    bool operator== (const CurvePoint&) const = default;
};

constexpr int maxCurvePoints = 64;

inline double defaultCurveDb (int curve) noexcept
{
    constexpr double defaults[] { 0.0, -60.0, -30.0 };
    return defaults[std::clamp (curve, 0, numCurves - 1)];
}

// The curve at a frequency. Points must be sorted by frequency. Smooth is monotone cubic in
// log frequency, so it never overshoots a point; flat is straight lines.
inline double evaluateCurve (const std::vector<CurvePoint>& points, bool smooth, double hz, double emptyDb) noexcept
{
    if (points.empty())
        return emptyDb;

    if (hz <= points.front().hz)
        return points.front().db;

    if (hz >= points.back().hz)
        return points.back().db;

    const auto count = (int) points.size();
    const auto x = [&] (int i) { return std::log (points[(size_t) i].hz); };
    const auto y = [&] (int i) { return points[(size_t) i].db; };

    auto segment = 0;

    while (segment < count - 2 && hz >= points[(size_t) segment + 1].hz)
        ++segment;

    const auto x0 = x (segment), x1 = x (segment + 1);
    const auto t = (std::log (hz) - x0) / (x1 - x0);

    if (! smooth || count == 2)
        return y (segment) + (y (segment + 1) - y (segment)) * t;

    // Fritsch-Carlson tangents.
    const auto slope = [&] (int i) { return (y (i + 1) - y (i)) / (x (i + 1) - x (i)); };

    const auto tangent = [&] (int i)
    {
        if (i == 0)
            return slope (0);

        if (i == count - 1)
            return slope (count - 2);

        const auto before = slope (i - 1), after = slope (i);

        if (before * after <= 0.0)
            return 0.0;

        return 2.0 / (1.0 / before + 1.0 / after);
    };

    const auto h = x1 - x0;
    const auto m0 = tangent (segment) * h, m1 = tangent (segment + 1) * h;
    const auto t2 = t * t, t3 = t2 * t;

    return (2.0 * t3 - 3.0 * t2 + 1.0) * y (segment) + (t3 - 2.0 * t2 + t) * m0
         + (-2.0 * t3 + 3.0 * t2) * y (segment + 1) + (t3 - t2) * m1;
}

inline double freehandAt (const std::array<float, curvePoints>& table, double hz) noexcept
{
    const auto position = curvePosition (hz);
    const auto index = std::min ((int) position, curvePoints - 2);
    const auto fraction = position - (double) index;

    return (double) table[(size_t) index] + fraction * (double) (table[(size_t) index + 1] - table[(size_t) index]);
}

inline void sortPoints (std::vector<CurvePoint>& points)
{
    std::sort (points.begin(), points.end(), [] (const CurvePoint& a, const CurvePoint& b) { return a.hz < b.hz; });
}

} // namespace fir
