#pragma once

#include "Curve.h"

#include <fancy/dsp/Analyzer.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace fir
{

// Phase 4 Wave 1 (docs/fir.md section 2): Learn for the Gate and Compressor curves. Over 3 s of
// input, each third-octave band's level is read from a calibrated analyzer (the graph's own
// scale: a sine at amplitude A reads 20 log10 A dB), averaged across the band, so the curve is
// smoothed across a third of an octave (Decision 1 A). Then a point per band: the Gate's at the
// band's 20th percentile + 6 dB, the Compressor's at its 80th percentile. Runs on the graph's
// analyzer thread, never the audio thread.
class CurveLearner
{
public:
    static constexpr int numBands = 31;     // 20 Hz to 20 kHz
    static constexpr double seconds = 3.0;
    static constexpr int pointsPerBand = 8;

    static double bandHz (int band) noexcept { return 20.0 * std::pow (2.0, (double) band / 3.0); }

    void start (int curveToLearn, double sampleRate)
    {
        curve = curveToLearn;
        rate = sampleRate > 0.0 ? sampleRate : 48000.0;
        analyzer.configure (rate, {});
        frames.clear();
        frames.reserve ((size_t) (seconds * 40.0));
        samplesSeen = 0;
    }

    void addSamples (const float* samples, int numSamples)
    {
        if (numSamples <= 0)
            return;

        analyzer.addSamples (samples, numSamples);
        samplesSeen += numSamples;

        // Wait for the analyzer's first full window before reading.
        if ((double) samplesSeen < 8192.0)
            return;

        std::vector<float> frame ((size_t) numBands);

        for (int band = 0; band < numBands; ++band)
        {
            const auto centre = bandHz (band);
            auto sum = 0.0;

            for (int i = 0; i < pointsPerBand; ++i)
                sum += (double) analyzer.levelAt (centre * std::pow (2.0, ((double) i + 0.5) / (double) pointsPerBand / 3.0 - 1.0 / 6.0));

            frame[(size_t) band] = (float) (sum / pointsPerBand);
        }

        frames.push_back (std::move (frame));
    }

    float progress() const noexcept { return (float) std::min (1.0, (double) samplesSeen / (seconds * rate)); }
    bool isComplete() const noexcept { return (double) samplesSeen >= seconds * rate && frames.size() >= 4; }

    std::vector<CurvePoint> result() const { return curveFrom (frames, curve, rate); }

    // The curve for frames of band levels: exposed for tests.
    static std::vector<CurvePoint> curveFrom (const std::vector<std::vector<float>>& frames, int curve, double sampleRate)
    {
        std::vector<CurvePoint> points;

        if (frames.empty())
            return points;

        const auto fraction = curve == 1 ? 0.2 : 0.8; // Gate, else Compressor
        const auto lift = curve == 1 ? 6.0 : 0.0;
        std::vector<float> values;

        for (int band = 0; band < numBands; ++band)
        {
            const auto hz = bandHz (band);

            if (hz > sampleRate * 0.45)
                break;

            values.clear();

            for (const auto& frame : frames)
                values.push_back (frame[(size_t) band]);

            std::sort (values.begin(), values.end());
            const auto index = std::min (values.size() - 1, (size_t) std::lround (fraction * (double) (values.size() - 1)));
            points.push_back ({ hz, std::max (-150.0, (double) values[index] + lift) });
        }

        return points;
    }

    int getCurve() const noexcept { return curve; }

private:
    fancy::dsp::SpectrumAnalyzer analyzer;
    std::vector<std::vector<float>> frames;
    double rate = 48000.0;
    long long samplesSeen = 0;
    int curve = 1;
};

} // namespace fir
