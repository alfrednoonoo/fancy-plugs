#include "SpectralEngine.h"

#include <fancy/dsp/Compression.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <numbers>

namespace fir
{

namespace
{
constexpr double maxExpansionDb = 24.0;
constexpr double powerFloor = 1.0e-20;

// A sine covers about five bins of the analysis window. Each bin's detector takes the loudest
// within this many bins either side, so the whole of a sine opens or ducks together.
constexpr int detectorReach = 2;

// Martin (2001), Table III: M(D), for the bias of the minimum of D frames.
double martinM (double d) noexcept
{
    constexpr double table[][2] { { 1, 0.0 }, { 2, 0.26 }, { 5, 0.48 }, { 8, 0.58 }, { 10, 0.61 }, { 15, 0.668 }, { 20, 0.705 },
                                  { 30, 0.762 }, { 40, 0.8 }, { 60, 0.841 }, { 80, 0.865 }, { 120, 0.89 }, { 140, 0.9 }, { 160, 0.91 } };

    if (d >= 160.0)
        return 0.91;

    for (size_t i = 1; i < std::size (table); ++i)
        if (d <= table[i][0])
            return table[i - 1][1] + (d - table[i - 1][0]) / (table[i][0] - table[i - 1][0]) * (table[i][1] - table[i - 1][1]);

    return 0.0;
}

// Martin's bias compensation B_min(D, Q_eq), with 1/Q_eq as given.
double martinBias (double frames, double qInverse) noexcept
{
    const auto m = martinM (frames);
    const auto qEq = 1.0 / std::max (qInverse, 1.0e-9);
    const auto qTilde = std::max (2.0, (qEq - 2.0 * m) / (1.0 - m));
    return 1.0 + 2.0 * (frames - 1.0) / qTilde;
}

int nextPowerOfTwo (int value) noexcept
{
    auto size = 1;

    while (size < value)
        size <<= 1;

    return size;
}

double hann (int n, int length) noexcept
{
    return 0.5 - 0.5 * std::cos (2.0 * std::numbers::pi * (double) n / (double) length);
}
} // namespace

int SpectralEngine::latencyOf (const Config& config) noexcept
{
    const auto n = config.fftSize;
    const auto staged = n >= firstStagedSize ? n / 8 : 0;

    if (config.convolve)
        return n + staged;

    return (config.lowLatency ? n / 2 : n) + staged;
}

SpectralEngine::SpectralEngine (const Config& config, double sampleRate, Shared& sharedState)
    : engineConfig (config), shared (sharedState), rate (sampleRate)
{
    frameLength = config.fftSize;
    fftLength = config.convolve ? frameLength * 2 : frameLength;
    bins = fftLength / 2 + 1;
    hop = config.convolve ? frameLength : frameLength / 4;
    synthesisLength = config.convolve ? frameLength : (config.lowLatency ? frameLength / 2 : frameLength);
    extra = frameLength >= firstStagedSize ? frameLength / 8 : 0;
    latency = synthesisLength + extra;
    stageInterval = extra / numStages;

    fft = std::make_unique<FFT> (juce::roundToInt (std::log2 ((double) fftLength)));

    analysisWindow.assign ((size_t) frameLength, 1.0f);
    synthesisWindow.assign ((size_t) frameLength, 1.0f);

    if (! config.convolve)
    {
        if (config.lowLatency)
        {
            // An asymmetric pair: their product is a Hann of synthesisLength over the last
            // part of the frame, which overlap-adds to 1 at a hop of half that.
            const auto shortLength = synthesisLength;
            const auto rise = frameLength - shortLength / 2;

            for (int n = 0; n < frameLength; ++n)
            {
                const auto analysis = n < rise ? std::sqrt (hann (n, 2 * rise))
                                               : std::sqrt (hann (n - (frameLength - shortLength), shortLength));

                const auto product = n < frameLength - shortLength ? 0.0 : hann (n - (frameLength - shortLength), shortLength);

                analysisWindow[(size_t) n] = (float) analysis;
                synthesisWindow[(size_t) n] = analysis > 1.0e-9 ? (float) (product / analysis) : 0.0f;
            }
        }
        else
        {
            // Square-root Hann both ways overlap-add to 2 at a quarter of the frame.
            for (int n = 0; n < frameLength; ++n)
            {
                const auto root = std::sqrt (hann (n, frameLength));

                analysisWindow[(size_t) n] = (float) root;
                synthesisWindow[(size_t) n] = (float) (0.5 * root);
            }
        }
    }

    auto windowSum = 0.0, windowEnergy = 0.0;

    for (const auto value : analysisWindow)
    {
        windowSum += (double) value;
        windowEnergy += (double) value * (double) value;
    }

    // A sine at amplitude A reads A, whatever the window and size.
    levelScale = 2.0 / windowSum;
    powerScale = 1.0 / windowEnergy;

    inputSize = nextPowerOfTwo (frameLength + extra + 8);
    inputMask = inputSize - 1;
    outputSize = nextPowerOfTwo (fftLength + latency + hop + 8);
    outputMask = outputSize - 1;

    for (auto& ring : inputRings)
        ring.assign ((size_t) inputSize, 0.0f);

    for (auto& ring : outputRings)
        ring.assign ((size_t) outputSize, 0.0f);

    for (auto& spectrum : spectra)
        spectrum.assign ((size_t) fftLength * 2, 0.0f);

    for (int ch = 0; ch < 2; ++ch)
    {
        gains[ch].assign ((size_t) bins, 1.0f);
        envelopes[ch].assign ((size_t) bins, 0.0f);
        priorSnr[ch].assign ((size_t) bins, 0.0f);
        smoothedKey[ch].assign ((size_t) bins, 0.0f);
        mask[ch].assign ((size_t) bins, 1.0f);
    }

    binHz.resize ((size_t) bins);
    binColumn.resize ((size_t) bins);

    for (int k = 0; k < bins; ++k)
    {
        const auto hz = (double) k * rate / (double) fftLength;

        binHz[(size_t) k] = (float) hz;
        binColumn[(size_t) k] = hz < curveLowHz ? -1 : std::min (displayColumns - 1, (int) (std::log (hz / curveLowHz) / std::log (1000.0) * displayColumns));
    }

    for (auto& curve : curveDb)
        curve.assign ((size_t) bins, 0.0f);

    profilePower.assign ((size_t) bins, 0.0f);
    scratch.assign ((size_t) bins, 0.0f);

    for (auto* track : { &trackPower, &trackSmoothed, &trackNoise_, &trackMoment1, &trackMoment2, &trackQInverse,
                         &trackActMin, &trackSubMin, &trackRingMin, &trackRatio, &trackSort })
        track->assign ((size_t) bins, 0.0f);

    trackRing.assign ((size_t) bins * trackSubwindows, 0.0f);
    trackHold.assign ((size_t) bins, 0);
    detectorLevels.assign ((size_t) bins, 0.0f);
    learnSum.assign ((size_t) bins, 0.0);
}

void SpectralEngine::reset() noexcept
{
    for (auto& ring : inputRings)
        std::fill (ring.begin(), ring.end(), 0.0f);

    for (auto& ring : outputRings)
        std::fill (ring.begin(), ring.end(), 0.0f);

    for (int ch = 0; ch < 2; ++ch)
    {
        std::fill (gains[ch].begin(), gains[ch].end(), 1.0f);
        std::fill (envelopes[ch].begin(), envelopes[ch].end(), 0.0f);
        std::fill (priorSnr[ch].begin(), priorSnr[ch].end(), 0.0f);
        std::fill (smoothedKey[ch].begin(), smoothedKey[ch].end(), 0.0f);
    }

    time = 0;
    frameEnd = -1;
    nextStage = numStages;
}

double SpectralEngine::perHop (double milliseconds) const noexcept
{
    return fancy::dsp::timeCoefficient (milliseconds, rate / (double) hop);
}

float SpectralEngine::binPower (const std::vector<float>& spectrum, int bin) const noexcept
{
    const auto re = spectrum[(size_t) bin * 2], im = spectrum[(size_t) bin * 2 + 1];
    return re * re + im * im;
}

void SpectralEngine::measureLevels (const std::vector<float>& spectrum) noexcept
{
    for (int k = 0; k < bins; ++k)
        scratch[(size_t) k] = binPower (spectrum, k);

    for (int k = 0; k < bins; ++k)
    {
        auto loudest = 0.0f;

        for (int j = std::max (0, k - detectorReach); j <= std::min (bins - 1, k + detectorReach); ++j)
            loudest = std::max (loudest, scratch[(size_t) j]);

        const auto level = (double) loudest * levelScale * levelScale;
        detectorLevels[(size_t) k] = (float) (10.0 * std::log10 (std::max (level, powerFloor)));
    }
}

void SpectralEngine::refreshCurve (int curve) noexcept
{
    const auto version = shared.curveVersions[(size_t) curve].load (std::memory_order_acquire);

    if (version == seenCurveVersion[curve])
        return;

    seenCurveVersion[curve] = version;

    const auto& table = shared.curves[(size_t) curve];

    for (int k = 0; k < bins; ++k)
    {
        const auto position = curvePosition (std::max ((double) binHz[(size_t) k], curveLowHz));
        const auto index = std::min ((int) position, curvePoints - 2);
        const auto fraction = (float) (position - (double) index);

        const auto low = table[(size_t) index].load (std::memory_order_relaxed);
        const auto high = table[(size_t) index + 1].load (std::memory_order_relaxed);

        curveDb[curve][(size_t) k] = low + fraction * (high - low);
    }
}

void SpectralEngine::refreshProfile() noexcept
{
    const auto version = shared.profileVersion.load (std::memory_order_acquire);

    if (version == seenProfileVersion)
        return;

    seenProfileVersion = version;
    profileReady = shared.hasProfile.load (std::memory_order_acquire);

    const auto step = profileHighHz / (double) (profilePoints - 1);

    for (int k = 0; k < bins; ++k)
    {
        const auto position = std::min ((double) binHz[(size_t) k] / step, (double) (profilePoints - 1));
        const auto index = std::min ((int) position, profilePoints - 2);
        const auto fraction = (float) (position - (double) index);

        const auto low = shared.profile[(size_t) index].load (std::memory_order_relaxed);
        const auto high = shared.profile[(size_t) index + 1].load (std::memory_order_relaxed);

        profilePower[(size_t) k] = low + fraction * (high - low);
    }
}

void SpectralEngine::analyse (int channel) noexcept
{
    auto& spectrum = spectra[channel];
    const auto& ring = inputRings[channel];

    // The frame ends at frameEnd, exclusive.
    for (int n = 0; n < frameLength; ++n)
        spectrum[(size_t) n] = ring[(size_t) ((frameEnd - frameLength + n) & inputMask)] * analysisWindow[(size_t) n];

    std::fill (spectrum.begin() + frameLength, spectrum.end(), 0.0f);

    fft->performRealOnlyForwardTransform (spectrum.data(), true);
}

void SpectralEngine::learnFrame() noexcept
{
    if (shared.learnRequested.exchange (false, std::memory_order_acq_rel))
    {
        std::fill (learnSum.begin(), learnSum.end(), 0.0);
        learnFrames = 0;
        shared.learnedSamples.store (0, std::memory_order_relaxed);
        shared.learning.store (true, std::memory_order_release);
    }

    if (! shared.learning.load (std::memory_order_acquire))
        return;

    for (int k = 0; k < bins; ++k)
    {
        auto power = 0.0;

        for (int ch = 0; ch < frameChannels; ++ch)
            power += (double) binPower (spectra[ch], k);

        learnSum[(size_t) k] += power * powerScale / (double) frameChannels;
    }

    ++learnFrames;

    const auto learned = shared.learnedSamples.load (std::memory_order_relaxed) + hop;
    shared.learnedSamples.store (learned, std::memory_order_relaxed);

    if (learned < shared.learnTarget.load (std::memory_order_relaxed))
        return;

    // Done: onto the fixed grid, averaging bins that are finer than it.
    const auto step = profileHighHz / (double) (profilePoints - 1);
    const auto binStep = rate / (double) fftLength;

    for (int j = 0; j < profilePoints; ++j)
    {
        const auto hz = profileHz (j);
        double value;

        if (binStep < step)
        {
            const auto first = std::max (0, (int) std::ceil ((hz - 0.5 * step) / binStep));
            const auto last = std::min (bins - 1, (int) std::floor ((hz + 0.5 * step) / binStep));

            auto sum = 0.0;
            auto count = 0;

            for (int k = first; k <= last; ++k)
            {
                sum += learnSum[(size_t) k];
                ++count;
            }

            value = count > 0 ? sum / (double) count : learnSum[(size_t) std::min (bins - 1, (int) std::lround (hz / binStep))];
        }
        else
        {
            const auto position = std::min (hz / binStep, (double) (bins - 1));
            const auto index = std::min ((int) position, bins - 2);
            const auto fraction = position - (double) index;

            value = learnSum[(size_t) index] + fraction * (learnSum[(size_t) index + 1] - learnSum[(size_t) index]);
        }

        shared.profile[(size_t) j].store ((float) (value / (double) std::max (1, learnFrames)), std::memory_order_relaxed);
    }

    shared.profileSeconds.store ((float) ((double) learned / rate), std::memory_order_relaxed);
    shared.hasProfile.store (true, std::memory_order_release);
    shared.profileVersion.fetch_add (1, std::memory_order_acq_rel);
    shared.learning.store (false, std::memory_order_release);
}

void SpectralEngine::startTracking() noexcept
{
    // From the estimate a previous engine published, else the learned profile, else this frame.
    const auto fromShared = shared.hasAdaptive.load (std::memory_order_acquire);
    const auto step = profileHighHz / (double) (profilePoints - 1);

    // With nothing to start from, each bin starts from the median of this frame's power within
    // 16 bins either side, so a tone or a note in the first frame isn't taken for noise.
    if (! fromShared && ! profileReady)
    {
        constexpr int reach = 16;

        for (int k = 0; k < bins; ++k)
        {
            const auto low = std::max (0, k - reach), high = std::min (bins - 1, k + reach);
            const auto count = high - low + 1;

            std::copy (trackPower.begin() + low, trackPower.begin() + high + 1, trackSort.begin());
            std::nth_element (trackSort.begin(), trackSort.begin() + count / 2, trackSort.begin() + count);
            trackRatio[(size_t) k] = std::min (trackPower[(size_t) k], trackSort[(size_t) (count / 2)]);
        }
    }

    for (int k = 0; k < bins; ++k)
    {
        auto start = (! fromShared && ! profileReady) ? (double) trackRatio[(size_t) k] : (double) trackPower[(size_t) k];

        if (fromShared || profileReady)
        {
            const auto position = std::min ((double) binHz[(size_t) k] / step, (double) (profilePoints - 1));
            const auto index = std::min ((int) position, profilePoints - 2);
            const auto fraction = position - (double) index;

            if (fromShared)
            {
                const auto low = (double) shared.adaptive[(size_t) index].load (std::memory_order_relaxed);
                const auto high = (double) shared.adaptive[(size_t) index + 1].load (std::memory_order_relaxed);
                start = low + fraction * (high - low);
            }
            else
            {
                start = (double) profilePower[(size_t) k];
            }
        }

        start = std::max (start, powerFloor);

        trackNoise_[(size_t) k] = (float) start;
        trackSmoothed[(size_t) k] = (float) start;
        trackMoment1[(size_t) k] = (float) start;
        trackMoment2[(size_t) k] = (float) (start * start);
        trackActMin[(size_t) k] = std::numeric_limits<float>::max();
        trackSubMin[(size_t) k] = std::numeric_limits<float>::max();
        trackRingMin[(size_t) k] = (float) start;
        trackHold[(size_t) k] = 0;

        for (int u = 0; u < trackSubwindows; ++u)
            trackRing[(size_t) (u * bins + k)] = (float) start;
    }

    trackAlphaC = 1.0;
    trackSubFrames = 0;
    trackRingIndex = 0;
    trackPublishCountdown = 0;
    trackStarted = true;
}

void SpectralEngine::trackNoise (bool publishes) noexcept
{
    const auto& settings = frameSettings;
    const auto hopSeconds = (double) hop / rate;

    // Martin's constants are for a 16 ms hop; each is re-derived from its time constant.
    const auto alphaMax = std::exp (-hopSeconds / 0.392);
    const auto alphaMin = std::exp (-hopSeconds / 0.0133);
    const auto betaMax = std::exp (-hopSeconds / 0.0717);
    const auto frames = std::max ((double) trackSubwindows, std::round (settings.adaptSeconds / hopSeconds));
    const auto hangover = (int) std::ceil (0.3 / hopSeconds);

    // The power of this frame, averaged over the channels, in the profile's units.
    auto sumPower = 0.0;

    for (int k = 0; k < bins; ++k)
    {
        auto power = 0.0;

        for (int ch = 0; ch < frameChannels; ++ch)
            power += (double) binPower (spectra[ch], k);

        trackPower[(size_t) k] = (float) (power * powerScale / (double) frameChannels);
        sumPower += (double) trackPower[(size_t) k];
    }

    if (! trackStarted)
        startTracking();

    // Global smoothing correction.
    {
        auto sumSmoothed = 0.0;

        for (int k = 0; k < bins; ++k)
            sumSmoothed += (double) trackSmoothed[(size_t) k];

        const auto ratio = sumSmoothed / std::max (sumPower, powerFloor);
        const auto alphaCTilde = 1.0 / (1.0 + (ratio - 1.0) * (ratio - 1.0));
        trackAlphaC = 0.7 * trackAlphaC + 0.3 * std::max (alphaCTilde, 0.7);
    }

    // Pass 1: optimally smoothed power, its moments, and 1/Q_eq.
    auto sumQInverse = 0.0;

    for (int k = 0; k < bins; ++k)
    {
        const auto noise = std::max ((double) trackNoise_[(size_t) k], powerFloor);
        auto smoothed = (double) trackSmoothed[(size_t) k];
        const auto ratio = smoothed / noise - 1.0;
        const auto alpha = std::clamp (trackAlphaC * alphaMax / (1.0 + ratio * ratio), alphaMin, alphaMax);

        smoothed = alpha * smoothed + (1.0 - alpha) * (double) trackPower[(size_t) k];

        const auto beta = std::min (alpha * alpha, betaMax);
        const auto moment1 = beta * (double) trackMoment1[(size_t) k] + (1.0 - beta) * smoothed;
        const auto moment2 = beta * (double) trackMoment2[(size_t) k] + (1.0 - beta) * smoothed * smoothed;
        const auto variance = std::max (0.0, moment2 - moment1 * moment1);
        const auto qInverse = std::min (0.5, variance / (2.0 * noise * noise));

        trackSmoothed[(size_t) k] = (float) smoothed;
        trackMoment1[(size_t) k] = (float) moment1;
        trackMoment2[(size_t) k] = (float) moment2;
        trackQInverse[(size_t) k] = (float) qInverse;
        trackRatio[(size_t) k] = (float) (smoothed / noise);
        sumQInverse += qInverse;
    }

    // The median ratio, to tell a rise in the whole noise from a tone standing out.
    std::copy (trackRatio.begin(), trackRatio.end(), trackSort.begin());
    const auto middle = trackSort.begin() + bins / 2;
    std::nth_element (trackSort.begin(), middle, trackSort.end());
    const auto median = (double) *middle;

    // Martin's B_c, times a measured calibration: with B_min as published, the estimate reads
    // 3.9-4.5 dB low on white and pink noise at FFT 1024-8192 here, because at 75 % overlap the
    // short-memory variance estimate under-reads the variance of the smoothed power. See
    // docs/phase2_1-A.md.
    constexpr double calibration = 2.6302679918953817; // 4.2 dB
    const auto biasGlobal = (1.0 + 2.12 * std::sqrt (sumQInverse / (double) bins)) * calibration;
    const auto subwindowFrames = (double) trackSubLength;
    const auto boundary = trackSubFrames + 1 >= trackSubLength;

    auto heldBelow8k = 0, binsBelow8k = 0;

    // Pass 2: holding, the minimum search and the estimate.
    for (int k = 0; k < bins; ++k)
    {
        const auto ratio = (double) trackRatio[(size_t) k];
        auto& hold = trackHold[(size_t) k];

        if (ratio > 4.0 && ratio > 4.0 * median)
            hold = hangover;
        else if (hold > 0)
            --hold;

        const auto hz = binHz[(size_t) k];

        if (hz >= curveLowHz && hz < 8000.0f)
        {
            ++binsBelow8k;
            heldBelow8k += hold > 0 ? 1 : 0;
        }

        auto& actMin = trackActMin[(size_t) k];
        auto& subMin = trackSubMin[(size_t) k];

        // A held bin doesn't feed the search, so it keeps the estimate it had.
        if (hold == 0)
        {
            const auto qInverse = (double) trackQInverse[(size_t) k];
            const auto smoothed = (double) trackSmoothed[(size_t) k];

            actMin = std::min (actMin, (float) (smoothed * martinBias (frames, qInverse) * biasGlobal));
            subMin = std::min (subMin, (float) (smoothed * martinBias (subwindowFrames, qInverse) * biasGlobal));
        }

        auto& noise = trackNoise_[(size_t) k];

        if (boundary)
        {
            const auto value = actMin < std::numeric_limits<float>::max() ? actMin : noise;
            trackRing[(size_t) (trackRingIndex * bins + k)] = value;

            auto lowest = std::numeric_limits<float>::max();

            for (int u = 0; u < trackSubwindows; ++u)
                lowest = std::min (lowest, trackRing[(size_t) (u * bins + k)]);

            trackRingMin[(size_t) k] = lowest;
            noise = std::max ((float) powerFloor, lowest);
            actMin = std::numeric_limits<float>::max();
            subMin = std::numeric_limits<float>::max();
        }
        else if (hold == 0)
        {
            noise = std::max ((float) powerFloor, std::min (trackRingMin[(size_t) k], subMin));
        }
    }

    if (boundary)
    {
        trackRingIndex = (trackRingIndex + 1) % trackSubwindows;
        trackSubFrames = 0;
        trackSubLength = std::max (1, (int) std::lround (frames / (double) trackSubwindows));
    }
    else
    {
        ++trackSubFrames;
    }

    if (publishes)
    {
        shared.heldFraction.store (binsBelow8k > 0 ? (float) heldBelow8k / (float) binsBelow8k : 0.0f, std::memory_order_relaxed);

        // At most every 20 ms: the grid is 2049 points.
        if (--trackPublishCountdown <= 0)
        {
            publishTracking();
            trackPublishCountdown = std::max (1, (int) std::lround (0.02 / hopSeconds));
        }
    }
}

void SpectralEngine::publishTracking() noexcept
{
    const auto binStep = rate / (double) fftLength;

    for (int j = 0; j < profilePoints; ++j)
    {
        const auto position = std::min (profileHz (j) / binStep, (double) (bins - 1));
        const auto index = std::min ((int) position, bins - 2);
        const auto fraction = position - (double) index;
        const auto value = (double) trackNoise_[(size_t) index] + fraction * ((double) trackNoise_[(size_t) index + 1] - (double) trackNoise_[(size_t) index]);

        shared.adaptive[(size_t) j].store ((float) value, std::memory_order_relaxed);
    }

    // The average from 100 Hz to 8 kHz, in the graph's dB.
    const auto displayScale = levelScale * levelScale / powerScale;
    auto sum = 0.0;
    auto count = 0;

    for (int k = 0; k < bins; ++k)
    {
        const auto hz = (double) binHz[(size_t) k];

        if (hz >= 100.0 && hz <= 8000.0)
        {
            sum += 10.0 * std::log10 (std::max ((double) trackNoise_[(size_t) k] * displayScale, powerFloor));
            ++count;
        }
    }

    shared.adaptiveFloorDb.store (count > 0 ? (float) (sum / count) : -200.0f, std::memory_order_relaxed);
    shared.hasAdaptive.store (true, std::memory_order_release);
    shared.adaptiveVersion.fetch_add (1, std::memory_order_acq_rel);
}

void SpectralEngine::computeMasks() noexcept
{
    const auto& settings = frameSettings;

    if (frameLearns)
        learnFrame();

    for (int curve = 0; curve < numCurves; ++curve)
        refreshCurve (curve);

    refreshProfile();

    // Phase 2.1 A: the adaptive estimate runs only when it's used, in Subtract. Back on Learned,
    // the heard engine lets it go, so it starts again from the learned profile next time.
    const auto tracking = settings.mode == subtract && settings.source != learnedSource;

    if (tracking)
    {
        trackNoise (frameLearns);
    }
    else if (trackStarted)
    {
        trackStarted = false;

        if (frameLearns)
            shared.hasAdaptive.store (false, std::memory_order_release);
    }

    const auto useKey = frameKey && settings.sidechain && settings.mode != eq;

    double reductionColumns[displayColumns] {};

    for (int ch = 0; ch < frameChannels; ++ch)
    {
        auto& gain = gains[ch];
        auto& result = mask[ch];
        const auto& spectrum = spectra[ch];
        const auto& key = spectra[ch + 2];

        if (settings.mode == eq)
        {
            for (int k = 0; k < bins; ++k)
                result[(size_t) k] = (float) std::pow (10.0, (double) curveDb[eq][(size_t) k] / 20.0);
        }
        else if (settings.mode == gate)
        {
            const auto attack = perHop (settings.gateAttackMs), release = perHop (settings.gateReleaseMs);

            measureLevels (useKey ? key : spectrum);

            for (int k = 0; k < bins; ++k)
            {
                const auto levelDb = (double) detectorLevels[(size_t) k];
                const auto target = levelDb >= (double) curveDb[gate][(size_t) k] ? 1.0 : settings.gateFloorGain;

                auto value = (double) gain[(size_t) k];
                value += (target - value) * (target > value ? attack : release);

                gain[(size_t) k] = (float) value;
                result[(size_t) k] = (float) value;

                const auto column = binColumn[(size_t) k];

                if (column >= 0)
                    reductionColumns[column] = std::max (reductionColumns[column], -20.0 * std::log10 (std::max (value, 1.0e-6)));
            }
        }
        else if (settings.mode == compressor)
        {
            const auto attack = perHop (settings.compAttackMs), release = perHop (settings.compReleaseMs);
            const auto slope = std::abs (settings.compSlope);
            const auto expanding = settings.compSlope < 0.0;

            measureLevels (useKey ? key : spectrum);

            for (int k = 0; k < bins; ++k)
            {
                const auto levelDb = (double) detectorLevels[(size_t) k];
                const auto target = fancy::dsp::computeReduction (levelDb, (double) curveDb[compressor][(size_t) k], slope, 0.0, false);

                auto envelope = (double) envelopes[ch][(size_t) k];
                envelope += (target - envelope) * (target > envelope ? attack : release);
                envelopes[ch][(size_t) k] = (float) envelope;

                const auto changeDb = expanding ? -std::min (envelope, maxExpansionDb) : envelope;
                result[(size_t) k] = (float) std::pow (10.0, -changeDb / 20.0);

                const auto column = binColumn[(size_t) k];

                if (column >= 0)
                    reductionColumns[column] = std::max (reductionColumns[column], changeDb);
            }
        }
        else if (settings.mode == subtract)
        {
            if (! profileReady && ! useKey && ! tracking)
            {
                std::fill (result.begin(), result.end(), 1.0f);
                continue;
            }

            // Signal power, smoothed across ±(Smoothing × 100) Hz.
            const auto halfWidth = std::min (32, (int) std::lround (settings.smoothing * 100.0 / ((double) rate / (double) fftLength)));

            for (int k = 0; k < bins; ++k)
                scratch[(size_t) k] = binPower (spectrum, k) * (float) powerScale;

            const auto dd = std::exp (-((double) hop / rate) / (0.001 * (10.0 + 190.0 * settings.smoothing)));
            const auto keyCoefficient = perHop (50.0);
            const auto release = perHop (settings.subReleaseMs);
            const auto floorGain = settings.reductionGain;

            for (int k = 0; k < bins; ++k)
            {
                auto sum = 0.0;
                auto count = 0;

                for (int j = std::max (0, k - halfWidth); j <= std::min (bins - 1, k + halfWidth); ++j)
                {
                    sum += (double) scratch[(size_t) j];
                    ++count;
                }

                const auto signal = sum / (double) count;

                double noise;

                if (useKey)
                {
                    auto& smoothed = smoothedKey[ch][(size_t) k];
                    smoothed += (float) (((double) binPower (key, k) * powerScale - (double) smoothed) * keyCoefficient);
                    noise = (double) smoothed;
                }
                else if (! tracking)
                {
                    noise = (double) profilePower[(size_t) k] * settings.offsetPower;
                }
                else if (settings.source == adaptiveSource || ! profileReady)
                {
                    noise = (double) trackNoise_[(size_t) k] * settings.offsetPower;
                }
                else
                {
                    // Both: the learned profile is the floor under the adaptive estimate.
                    noise = std::max ((double) profilePower[(size_t) k], (double) trackNoise_[(size_t) k]) * settings.offsetPower;
                }

                noise = std::max (noise, powerFloor);

                const auto posterior = signal / noise;
                const auto previous = (double) gain[(size_t) k];
                const auto prior = dd * previous * previous * (double) priorSnr[ch][(size_t) k] + (1.0 - dd) * std::max (posterior - 1.0, 0.0);

                priorSnr[ch][(size_t) k] = (float) posterior;

                auto value = std::max (floorGain, prior / (1.0 + prior));

                if (value < previous)
                    value = previous + (value - previous) * release;

                gain[(size_t) k] = (float) value;
                result[(size_t) k] = (float) value;

                const auto column = binColumn[(size_t) k];

                if (column >= 0)
                    reductionColumns[column] = std::max (reductionColumns[column], -20.0 * std::log10 (std::max (value, 1.0e-6)));
            }
        }

        if (settings.reduceArtifacts)
        {
            std::copy (result.begin(), result.end(), scratch.begin());

            for (int k = 1; k < bins - 1; ++k)
                result[(size_t) k] = 0.25f * scratch[(size_t) k - 1] + 0.5f * scratch[(size_t) k] + 0.25f * scratch[(size_t) k + 1];
        }

        if (settings.listen && (settings.mode == gate || settings.mode == compressor || settings.mode == subtract))
            for (auto& value : result)
                value = 1.0f - value;
    }

    if (frameLearns)
        for (int column = 0; column < displayColumns; ++column)
            shared.reduction[(size_t) column].store ((float) reductionColumns[column], std::memory_order_relaxed);
}

void SpectralEngine::synthesise (int channel) noexcept
{
    auto& spectrum = spectra[channel];
    const auto& gain = mask[channel];

    for (int k = 0; k < bins; ++k)
    {
        spectrum[(size_t) k * 2] *= gain[(size_t) k];
        spectrum[(size_t) k * 2 + 1] *= gain[(size_t) k];
    }

    fft->performRealOnlyInverseTransform (spectrum.data());

    auto& ring = outputRings[channel];

    for (int n = 0; n < frameLength; ++n)
        ring[(size_t) ((frameEnd - frameLength + n) & outputMask)] += spectrum[(size_t) n] * synthesisWindow[(size_t) n];
}

void SpectralEngine::convolveChannels() noexcept
{
    auto& left = spectra[0];
    const auto& right = spectra[1];

    for (int k = 0; k < bins; ++k)
    {
        const auto a = left[(size_t) k * 2], b = left[(size_t) k * 2 + 1];
        const auto c = right[(size_t) k * 2], d = right[(size_t) k * 2 + 1];

        left[(size_t) k * 2] = a * c - b * d;
        left[(size_t) k * 2 + 1] = a * d + b * c;
    }

    fft->performRealOnlyInverseTransform (left.data());

    for (int n = 0; n < fftLength; ++n)
    {
        const auto position = (size_t) ((frameEnd - frameLength + n) & outputMask);

        outputRings[0][position] += left[(size_t) n];
        outputRings[1][position] += left[(size_t) n];
    }
}

void SpectralEngine::runStage (int stage) noexcept
{
    const auto convolving = engineConfig.convolve && frameChannels > 1;

    switch (stage)
    {
        case 0: analyse (0); break;
        case 1: if (frameChannels > 1) analyse (1); break;
        case 2:
            if (frameKey && ! engineConfig.convolve)
            {
                analyse (2);
                analyse (3);
            }
            break;
        case 3: if (! convolving) computeMasks(); break;
        case 4:
            if (convolving)
                convolveChannels();
            else
                synthesise (0);
            break;
        case 5: if (! convolving && frameChannels > 1) synthesise (1); break;
        default: break;
    }
}

void SpectralEngine::process (const float* inLeft, const float* inRight, const float* keyLeft, const float* keyRight,
                              float* outLeft, float* outRight, int numChannels, int numSamples,
                              const FrameSettings& settings, bool learns) noexcept
{
    const auto hasKey = keyLeft != nullptr && keyRight != nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto position = (size_t) (time & inputMask);

        inputRings[0][position] = inLeft[i];
        inputRings[1][position] = numChannels > 1 ? inRight[i] : inLeft[i];
        inputRings[2][position] = hasKey ? keyLeft[i] : 0.0f;
        inputRings[3][position] = hasKey ? keyRight[i] : 0.0f;

        ++time;

        // A frame is ready every hop, once the first frame has filled.
        if (time >= frameLength && (time - frameLength) % hop == 0)
        {
            // An unfinished frame can't happen, because the stages take half a hop.
            while (nextStage < numStages)
                runStage (nextStage++);

            frameEnd = time;
            nextStage = 0;
            frameChannels = numChannels > 1 ? 2 : 1;
            frameKey = hasKey;
            frameLearns = learns;
            frameSettings = settings;
        }

        while (nextStage < numStages && time >= frameEnd + (long long) (nextStage * stageInterval))
            runStage (nextStage++);

        // The sample that's now a full latency old, read and cleared for later frames.
        const auto readPosition = (size_t) ((time - 1 - latency) & outputMask);

        outLeft[i] = outputRings[0][readPosition];
        outputRings[0][readPosition] = 0.0f;

        if (outRight != nullptr)
        {
            outRight[i] = numChannels > 1 ? outputRings[1][readPosition] : outLeft[i];
            outputRings[1][readPosition] = 0.0f;
        }
    }
}

} // namespace fir
