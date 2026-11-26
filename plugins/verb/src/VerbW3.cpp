#include "VerbW3.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace verb
{

// SpectralFreeze -----------------------------------------------------------------------------

void SpectralFreeze::prepare (double sampleRate)
{
    // 4096 points at 44.1 and 48 kHz, twice that at 88.2 and 96, and so on: the same frame in time.
    auto factor = 1;

    while (sampleRate / 48000.0 >= 1.5 * (double) factor)
        factor *= 2;

    size = baseSize * factor;
    hop = size / 4;
    bins = size / 2 + 1;

    historySize = 1;

    while (historySize < size + (numFrames - 1) * hop + 1)
        historySize <<= 1;

    historyMask = historySize - 1;

    auto order = 0;

    while ((1 << order) < size)
        ++order;

    fft = std::make_unique<juce::dsp::FFT> (order);
    buffer.assign ((size_t) (2 * size), 0.0f);
    window.resize ((size_t) size);

    for (int n = 0; n < size; ++n)
        window[(size_t) n] = (float) (0.5 - 0.5 * std::cos (2.0 * std::numbers::pi * (double) n / (double) size));

    // Whatever scaling this FFT's inverse uses, the frames come back at the level they went in.
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    buffer[0] = 1.0f;
    fft->performRealOnlyForwardTransform (buffer.data(), true);
    fft->performRealOnlyInverseTransform (buffer.data());
    inverseCorrection = std::abs (buffer[0]) > 1.0e-12f ? 1.0 / (double) buffer[0] : 1.0;

    // Random phases add frames by power: a Hann window's mean square is 3/8, and four of them
    // overlapping at a quarter hop sum to 3/2 in power.
    scale = 1.0 / std::sqrt (0.375 * 1.5);

    for (int ch = 0; ch < 2; ++ch)
    {
        history[(size_t) ch].assign ((size_t) historySize, 0.0f);
        accumulator[(size_t) ch].assign ((size_t) size, 0.0f);
        magnitude[(size_t) ch].assign ((size_t) bins, 0.0f);
        phaseRe[(size_t) ch].assign ((size_t) bins, 1.0);
        phaseIm[(size_t) ch].assign ((size_t) bins, 0.0);
    }

    reset();
}

void SpectralFreeze::reset() noexcept
{
    for (int ch = 0; ch < 2; ++ch)
    {
        std::fill (history[(size_t) ch].begin(), history[(size_t) ch].end(), 0.0f);
        std::fill (accumulator[(size_t) ch].begin(), accumulator[(size_t) ch].end(), 0.0f);
        std::fill (magnitude[(size_t) ch].begin(), magnitude[(size_t) ch].end(), 0.0f);
        std::fill (phaseRe[(size_t) ch].begin(), phaseRe[(size_t) ch].end(), 1.0);
        std::fill (phaseIm[(size_t) ch].begin(), phaseIm[(size_t) ch].end(), 0.0);
    }

    historyWrite = 0;
    position = 0;
    seed = 0x9e3779b9u;
}

void SpectralFreeze::push (double left, double right) noexcept
{
    history[0][(size_t) historyWrite] = std::isfinite (left) ? (float) left : 0.0f;
    history[1][(size_t) historyWrite] = std::isfinite (right) ? (float) right : 0.0f;
    historyWrite = (historyWrite + 1) & historyMask;
}

double SpectralFreeze::nextRandom() noexcept
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return (double) seed * (1.0 / 4294967296.0);
}

void SpectralFreeze::capture() noexcept
{
    for (int ch = 0; ch < 2; ++ch)
    {
        auto& power = magnitude[(size_t) ch];
        std::fill (power.begin(), power.end(), 0.0f);

        for (int frame = 0; frame < numFrames; ++frame)
        {
            const auto start = historyWrite - size - (numFrames - 1 - frame) * hop;

            for (int n = 0; n < size; ++n)
                buffer[(size_t) n] = window[(size_t) n] * history[(size_t) ch][(size_t) ((start + n) & historyMask)];

            std::fill (buffer.begin() + size, buffer.end(), 0.0f);
            fft->performRealOnlyForwardTransform (buffer.data(), true);

            for (int k = 0; k < bins; ++k)
            {
                const auto re = (double) buffer[(size_t) (2 * k)], im = (double) buffer[(size_t) (2 * k + 1)];
                const auto squared = re * re + im * im;
                power[(size_t) k] += (float) squared;

                if (frame == numFrames - 1)
                {
                    const auto length = std::sqrt (squared);
                    phaseRe[(size_t) ch][(size_t) k] = length > 1.0e-20 ? re / length : 1.0;
                    phaseIm[(size_t) ch][(size_t) k] = length > 1.0e-20 ? im / length : 0.0;
                }
            }
        }

        for (auto& value : power)
            value = std::sqrt (value / (float) numFrames);

        std::fill (accumulator[(size_t) ch].begin(), accumulator[(size_t) ch].end(), 0.0f);
    }

    position = 0;

    for (int offset = 3; offset >= 0; --offset)
        addFrame (offset);
}

void SpectralFreeze::addFrame (int offset) noexcept
{
    // One random step per bin, turning both channels alike.
    for (int k = 0; k < bins; ++k)
    {
        const auto angle = (2.0 * nextRandom() - 1.0) * std::numbers::pi;
        const auto c = std::cos (angle), s = std::sin (angle);

        for (int ch = 0; ch < 2; ++ch)
        {
            auto& re = phaseRe[(size_t) ch][(size_t) k];
            auto& im = phaseIm[(size_t) ch][(size_t) k];
            const auto newRe = re * c - im * s;
            const auto newIm = re * s + im * c;
            const auto norm = 1.5 - 0.5 * (newRe * newRe + newIm * newIm);
            re = newRe * norm;
            im = newIm * norm;
        }
    }

    const auto skip = offset * hop;
    const auto gain = (float) (inverseCorrection * scale);

    for (int ch = 0; ch < 2; ++ch)
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);

        for (int k = 0; k < bins; ++k)
        {
            const auto m = (double) magnitude[(size_t) ch][(size_t) k];
            const auto edge = k == 0 || k == bins - 1;
            buffer[(size_t) (2 * k)] = (float) (m * phaseRe[(size_t) ch][(size_t) k]);
            buffer[(size_t) (2 * k + 1)] = edge ? 0.0f : (float) (m * phaseIm[(size_t) ch][(size_t) k]);
        }

        fft->performRealOnlyInverseTransform (buffer.data());

        auto& sum = accumulator[(size_t) ch];

        for (int n = skip; n < size; ++n)
            sum[(size_t) (n - skip)] += buffer[(size_t) n] * window[(size_t) n] * gain;
    }
}

void SpectralFreeze::next (double& left, double& right) noexcept
{
    left = (double) accumulator[0][(size_t) position];
    right = (double) accumulator[1][(size_t) position];

    if (++position < hop)
        return;

    for (auto& sum : accumulator)
    {
        std::copy (sum.begin() + hop, sum.end(), sum.begin());
        std::fill (sum.end() - hop, sum.end(), 0.0f);
    }

    position = 0;
    addFrame (0);
}

// ShimmerLoop --------------------------------------------------------------------------------

void ShimmerLoop::prepare (double sampleRate, int maxPiece)
{
    rate = sampleRate;
    loopDelay = std::clamp ((int) std::lround (baseDelay * rate / 48000.0), 32, maxPiece);

    ringSize = 1;

    while (ringSize < loopDelay + maxPiece + 1)
        ringSize <<= 1;

    ringMask = ringSize - 1;

    for (auto& channel : ring)
        channel.assign ((size_t) ringSize, 0.0f);

    inputPeak.assign ((size_t) maxPiece, 0.0f);

    highpass.set (highpassHz, 0.70710678118654752, rate);
    shifter.prepare (rate);
    shifter.setMode (fancy::dsp::PitchShifter::Mode::plain);

    rmsCoefficient = 1.0 - std::exp (-1.0 / (rmsMs * 0.001 * rate));
    attackCoefficient = 1.0 - std::exp (-1.0 / (attackMs * 0.001 * rate));
    releaseCoefficient = 1.0 - std::exp (-1.0 / (releaseMs * 0.001 * rate));

    reset();
}

void ShimmerLoop::reset() noexcept
{
    for (auto& channel : ring)
        std::fill (channel.begin(), channel.end(), 0.0f);

    for (auto& filter : highpassFilters)
        filter.reset();

    shifter.reset();
    position = 0;
    inputEnvelope = feedbackEnvelope = peak = 0.0;
    guardGain = 1.0;
    holdLeft = 0;
}

void ShimmerLoop::addFeedback (float* left, float* right, int numSamples, double rt60Seconds) noexcept
{
    const auto rt60 = std::max (0.05, rt60Seconds) * rate;
    const auto hold = (int) std::min (rt60, 60.0 * rate);
    const auto letGo = std::pow (10.0, -6.0 / rt60); // in power: 60 dB per RT60

    for (int i = 0; i < numSamples; ++i)
    {
        const auto l = (double) left[i], r = (double) right[i];
        inputEnvelope += (0.5 * (l * l + r * r) - inputEnvelope) * rmsCoefficient;

        if (inputEnvelope >= peak)
        {
            peak = inputEnvelope;
            holdLeft = hold;
        }
        else if (holdLeft > 0)
        {
            --holdLeft;
        }
        else
        {
            peak *= letGo;
        }

        inputPeak[(size_t) i] = (float) peak;

        const auto at = (size_t) ((position + i) & ringMask);
        left[i] += ring[0][at];
        right[i] += ring[1][at];
        ring[0][at] = ring[1][at] = 0.0f;
    }
}

void ShimmerLoop::feed (double left, double right, int index, double ratio, double gain) noexcept
{
    const double in[2] { highpassFilters[0].tick (highpass, left).high, highpassFilters[1].tick (highpass, right).high };
    double shifted[2];
    shifter.process (in, shifted, 2, ratio);

    const auto l = gain * shifted[0], r = gain * shifted[1];
    feedbackEnvelope += (0.5 * (l * l + r * r) - feedbackEnvelope) * rmsCoefficient;

    const auto reference = (double) inputPeak[(size_t) index];
    const auto wanted = feedbackEnvelope > reference && feedbackEnvelope > 1.0e-30 ? std::sqrt (reference / feedbackEnvelope) : 1.0;
    guardGain += (wanted - guardGain) * (wanted < guardGain ? attackCoefficient : releaseCoefficient);

    const auto at = (size_t) ((position + loopDelay) & ringMask);
    const auto outLeft = l * guardGain, outRight = r * guardGain;
    ring[0][at] = std::isfinite (outLeft) ? (float) outLeft : 0.0f;
    ring[1][at] = std::isfinite (outRight) ? (float) outRight : 0.0f;

    position = (position + 1) & ringMask;
}

} // namespace verb
