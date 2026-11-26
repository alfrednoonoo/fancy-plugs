#include "LegacyFirW2Engine.h"

#include "Curve.h"

#include <fancy/dsp/Compression.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <numbers>

namespace fir::legacyW2
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

// Martin's bias compensation B_min(D, Q_eq), with 1/Q_eq as given and M(D) worked out once
// per frame, since it depends only on the window.
double martinBias (double frames, double m, double qInverse) noexcept
{
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

    scratch.assign ((size_t) bins, 0.0f);

    for (int domain = 0; domain < 2; ++domain)
    {
        profilePower[domain].assign ((size_t) bins, 0.0f);
        learnSum[domain].assign ((size_t) bins, 0.0);

        auto& t = trackers[domain];

        for (auto* track : { &t.power, &t.smoothed, &t.noise, &t.moment1, &t.moment2, &t.qInverse,
                             &t.actMin, &t.subMin, &t.ringMin, &t.ratio, &t.sort, &t.presence })
            track->assign ((size_t) bins, 0.0f);

        t.prefix.assign ((size_t) bins + 1, 0.0);
        t.ring.assign ((size_t) bins * trackSubwindows, 0.0f);
        t.hold.assign ((size_t) bins, 0);

        resDetector[domain].assign ((size_t) bins, 0.0f);
        resReduction[domain].assign ((size_t) bins, 0.0f);
        resonanceGain[domain].assign ((size_t) bins, 1.0f);
    }

    detectorLevels.assign ((size_t) bins, 0.0f);

    for (auto* state : { &resLevel, &resClipped, &resEnvelope, &resWeight })
        state->assign ((size_t) bins, 0.0f);

    resTiltFactor.assign ((size_t) bins, 1.0f);
    resPrefix.assign ((size_t) bins + 1, 0.0);
    resLow.assign ((size_t) bins, 0);
    resHigh.assign ((size_t) bins, 0);
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

    resStarted[0] = resStarted[1] = false;

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

    // Phase 4 Wave 2: a flat depth curve leaves Resonance on its old path, and a flat ratio curve
    // Compressor mode on its, to the bit.
    const auto domain = curve >= curvesPerDomain ? 1 : 0;

    if (baseCurve (curve) == depthCurve)
        depthFlat[domain] = std::all_of (curveDb[curve].begin(), curveDb[curve].end(), [] (float value) { return value == 100.0f; });

    if (baseCurve (curve) == ratioCurve)
        ratioFlat[domain] = std::all_of (curveDb[curve].begin(), curveDb[curve].end(), [] (float value) { return value == 0.0f; });
}

void SpectralEngine::refreshProfile() noexcept
{
    const auto version = shared.profileVersion.load (std::memory_order_acquire);

    if (version == seenProfileVersion)
        return;

    seenProfileVersion = version;
    profileReady[0] = shared.hasProfile.load (std::memory_order_acquire);
    profileReady[1] = shared.hasSideProfile.load (std::memory_order_acquire);

    const auto step = profileHighHz / (double) (profilePoints - 1);

    for (int domain = 0; domain < 2; ++domain)
    {
        const auto& source = domain == 0 ? shared.profile : shared.sideProfile;

        for (int k = 0; k < bins; ++k)
        {
            const auto position = std::min ((double) binHz[(size_t) k] / step, (double) (profilePoints - 1));
            const auto index = std::min ((int) position, profilePoints - 2);
            const auto fraction = (float) (position - (double) index);

            const auto low = source[(size_t) index].load (std::memory_order_relaxed);
            const auto high = source[(size_t) index + 1].load (std::memory_order_relaxed);

            profilePower[domain][(size_t) k] = low + fraction * (high - low);
        }
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
        for (auto& sum : learnSum)
            std::fill (sum.begin(), sum.end(), 0.0);

        learnFrames = 0;
        shared.learnedSamples.store (0, std::memory_order_relaxed);
        shared.learning.store (true, std::memory_order_release);
    }

    if (! shared.learning.load (std::memory_order_acquire))
        return;

    // Phase 4 Wave 2: in Mid/Side, each domain learns from its own channel.
    const auto midSide = midSideFrame();

    for (int k = 0; k < bins; ++k)
    {
        if (midSide)
        {
            for (int ch = 0; ch < 2; ++ch)
                learnSum[ch][(size_t) k] += (double) binPower (spectra[ch], k) * powerScale;

            continue;
        }

        auto power = 0.0;

        for (int ch = 0; ch < frameChannels; ++ch)
            power += (double) binPower (spectra[ch], k);

        learnSum[0][(size_t) k] += power * powerScale / (double) frameChannels;
    }

    ++learnFrames;

    const auto learned = shared.learnedSamples.load (std::memory_order_relaxed) + hop;
    shared.learnedSamples.store (learned, std::memory_order_relaxed);

    if (learned < shared.learnTarget.load (std::memory_order_relaxed))
        return;

    // Done: onto the fixed grid, averaging bins that are finer than it.
    const auto step = profileHighHz / (double) (profilePoints - 1);
    const auto binStep = rate / (double) fftLength;

    for (int domain = 0; domain < (midSide ? 2 : 1); ++domain)
    {
        const auto& sums = learnSum[domain];
        auto& target = domain == 0 ? shared.profile : shared.sideProfile;

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
                    sum += sums[(size_t) k];
                    ++count;
                }

                value = count > 0 ? sum / (double) count : sums[(size_t) std::min (bins - 1, (int) std::lround (hz / binStep))];
            }
            else
            {
                const auto position = std::min (hz / binStep, (double) (bins - 1));
                const auto index = std::min ((int) position, bins - 2);
                const auto fraction = position - (double) index;

                value = sums[(size_t) index] + fraction * (sums[(size_t) index + 1] - sums[(size_t) index]);
            }

            target[(size_t) j].store ((float) (value / (double) std::max (1, learnFrames)), std::memory_order_relaxed);
        }
    }

    shared.profileSeconds.store ((float) ((double) learned / rate), std::memory_order_relaxed);
    shared.hasProfile.store (true, std::memory_order_release);

    if (midSide)
    {
        shared.sideProfileSeconds.store ((float) ((double) learned / rate), std::memory_order_relaxed);
        shared.hasSideProfile.store (true, std::memory_order_release);
    }

    shared.profileVersion.fetch_add (1, std::memory_order_acq_rel);
    shared.learning.store (false, std::memory_order_release);
}

void SpectralEngine::startTracking (int domain) noexcept
{
    auto& t = trackers[domain];
    const auto& adaptive = domain == 0 ? shared.adaptive : shared.sideAdaptive;
    const auto ready = profileReady[domain];
    const auto& profile = profilePower[domain];

    // From the estimate a previous engine published, else the learned profile, else this frame.
    const auto fromShared = (domain == 0 ? shared.hasAdaptive : shared.hasSideAdaptive).load (std::memory_order_acquire);
    const auto step = profileHighHz / (double) (profilePoints - 1);

    // With nothing to start from, each bin starts from the median of this frame's power within
    // 16 bins either side, so a tone or a note in the first frame isn't taken for noise.
    if (! fromShared && ! ready)
    {
        constexpr int reach = 16;

        for (int k = 0; k < bins; ++k)
        {
            const auto low = std::max (0, k - reach), high = std::min (bins - 1, k + reach);
            const auto count = high - low + 1;

            std::copy (t.power.begin() + low, t.power.begin() + high + 1, t.sort.begin());
            std::nth_element (t.sort.begin(), t.sort.begin() + count / 2, t.sort.begin() + count);
            t.ratio[(size_t) k] = std::min (t.power[(size_t) k], t.sort[(size_t) (count / 2)]);
        }
    }

    for (int k = 0; k < bins; ++k)
    {
        auto start = (! fromShared && ! ready) ? (double) t.ratio[(size_t) k] : (double) t.power[(size_t) k];

        if (fromShared || ready)
        {
            const auto position = std::min ((double) binHz[(size_t) k] / step, (double) (profilePoints - 1));
            const auto index = std::min ((int) position, profilePoints - 2);
            const auto fraction = position - (double) index;

            if (fromShared)
            {
                const auto low = (double) adaptive[(size_t) index].load (std::memory_order_relaxed);
                const auto high = (double) adaptive[(size_t) index + 1].load (std::memory_order_relaxed);
                start = low + fraction * (high - low);
            }
            else
            {
                start = (double) profile[(size_t) k];
            }
        }

        start = std::max (start, powerFloor);

        t.noise[(size_t) k] = (float) start;
        t.smoothed[(size_t) k] = (float) start;
        t.moment1[(size_t) k] = (float) start;
        t.moment2[(size_t) k] = (float) (start * start);
        t.actMin[(size_t) k] = std::numeric_limits<float>::max();
        t.subMin[(size_t) k] = std::numeric_limits<float>::max();
        t.ringMin[(size_t) k] = (float) start;
        t.presence[(size_t) k] = (float) start;
        t.hold[(size_t) k] = 0;

        for (int u = 0; u < trackSubwindows; ++u)
            t.ring[(size_t) (u * bins + k)] = (float) start;
    }

    t.alphaC = 1.0;
    t.subFrames = 0;
    t.ringIndex = 0;
    t.publishCountdown = 0;
    t.started = true;
}

void SpectralEngine::trackNoise (int domain, bool publishes) noexcept
{
    auto& t = trackers[domain];

    const auto& settings = frameSettings;
    const auto hopSeconds = (double) hop / rate;

    // Martin's constants are for a 16 ms hop; each is re-derived from its time constant.
    const auto alphaMax = std::exp (-hopSeconds / 0.392);
    const auto alphaMin = std::exp (-hopSeconds / 0.0133);
    const auto betaMax = std::exp (-hopSeconds / 0.0717);
    // Following a rise takes the 8 sub-windows plus the one the change landed in, the frame and
    // the smoothing: about 9.5 sub-windows. So each is Adapt speed / 9.5, and a rise is followed
    // in about Adapt speed at every FFT size.
    const auto frames = std::max ((double) trackSubwindows, std::round (settings.adaptSeconds * (double) trackSubwindows / 9.5 / hopSeconds));
    const auto hangover = (int) std::ceil (0.3 / hopSeconds);

    // The power of this frame, averaged over the channels, in the profile's units.
    auto sumPower = 0.0;

    // In Mid/Side, the domain's own channel; otherwise both, averaged.
    const auto firstChannel = midSideFrame() ? domain : 0;
    const auto numTracked = midSideFrame() ? 1 : frameChannels;

    for (int k = 0; k < bins; ++k)
    {
        auto power = 0.0;

        for (int ch = firstChannel; ch < firstChannel + numTracked; ++ch)
            power += (double) binPower (spectra[ch], k);

        t.power[(size_t) k] = (float) (power * powerScale / (double) numTracked);
        sumPower += (double) t.power[(size_t) k];
    }

    if (! t.started)
        startTracking (domain);

    // Global smoothing correction.
    {
        auto sumSmoothed = 0.0;

        for (int k = 0; k < bins; ++k)
            sumSmoothed += (double) t.smoothed[(size_t) k];

        const auto ratio = sumSmoothed / std::max (sumPower, powerFloor);
        const auto alphaCTilde = 1.0 / (1.0 + (ratio - 1.0) * (ratio - 1.0));
        t.alphaC = 0.7 * t.alphaC + 0.3 * std::max (alphaCTilde, 0.7);
    }

    // Pass 1: optimally smoothed power, its moments, and 1/Q_eq.
    auto sumQInverse = 0.0;

    for (int k = 0; k < bins; ++k)
    {
        const auto noise = std::max ((double) t.noise[(size_t) k], powerFloor);
        auto smoothed = (double) t.smoothed[(size_t) k];
        const auto ratio = smoothed / noise - 1.0;
        const auto alpha = std::clamp (t.alphaC * alphaMax / (1.0 + ratio * ratio), alphaMin, alphaMax);

        smoothed = alpha * smoothed + (1.0 - alpha) * (double) t.power[(size_t) k];

        const auto beta = std::min (alpha * alpha, betaMax);
        const auto moment1 = beta * (double) t.moment1[(size_t) k] + (1.0 - beta) * smoothed;
        const auto moment2 = beta * (double) t.moment2[(size_t) k] + (1.0 - beta) * smoothed * smoothed;
        const auto variance = std::max (0.0, moment2 - moment1 * moment1);
        const auto qInverse = std::min (0.5, variance / (2.0 * noise * noise));

        t.smoothed[(size_t) k] = (float) smoothed;
        t.moment1[(size_t) k] = (float) moment1;
        t.moment2[(size_t) k] = (float) moment2;
        t.qInverse[(size_t) k] = (float) qInverse;
        sumQInverse += qInverse;
    }

    // Holding: the power over 200 ms, against the average 4 to 16 bins below and 4 to 16 bins
    // above. A tone or a note stands over both; noise, even steeply sloped, and a rise in the
    // whole noise don't. It doesn't look at the estimate, so its dips can't cause a hold.
    {
        const auto presenceCoefficient = std::exp (-hopSeconds / 0.2);

        t.prefix[0] = 0.0;

        for (int k = 0; k < bins; ++k)
        {
            auto& presence = t.presence[(size_t) k];
            presence = (float) (presenceCoefficient * (double) presence + (1.0 - presenceCoefficient) * (double) t.power[(size_t) k]);
            t.prefix[(size_t) k + 1] = t.prefix[(size_t) k] + (double) presence;
        }
    }

    const auto sideMean = [this, &t] (int first, int last)
    {
        first = std::max (0, first);
        last = std::min (bins - 1, last);
        return last >= first ? (t.prefix[(size_t) last + 1] - t.prefix[(size_t) first]) / (double) (last - first + 1) : 0.0;
    };

    // Martin's B_c, times a measured calibration: with B_min as published, the estimate reads
    // 3.9-4.5 dB low on white and pink noise at FFT 1024-8192 here, because at 75 % overlap the
    // short-memory variance estimate under-reads the variance of the smoothed power. See
    // docs/phase2_1-A.md.
    constexpr double calibration = 2.6302679918953817; // 4.2 dB
    const auto biasGlobal = (1.0 + 2.12 * std::sqrt (sumQInverse / (double) bins)) * calibration;
    const auto subwindowFrames = (double) t.subLength;
    const auto boundary = t.subFrames + 1 >= t.subLength;
    const auto mWindow = martinM (frames), mSubwindow = martinM (subwindowFrames);

    auto heldBelow8k = 0, binsBelow8k = 0;

    // Pass 2: holding, the minimum search and the estimate.
    for (int k = 0; k < bins; ++k)
    {
        auto& hold = t.hold[(size_t) k];
        const auto reference = std::max (sideMean (k - 16, k - 4), sideMean (k + 4, k + 16));

        if (reference > 0.0 && (double) t.presence[(size_t) k] > 4.0 * reference)
            hold = hangover;
        else if (hold > 0)
            --hold;

        const auto hz = binHz[(size_t) k];

        if (hz >= curveLowHz && hz < 8000.0f)
        {
            ++binsBelow8k;
            heldBelow8k += hold > 0 ? 1 : 0;
        }

        auto& actMin = t.actMin[(size_t) k];
        auto& subMin = t.subMin[(size_t) k];

        // A held bin doesn't feed the search, so it keeps the estimate it had.
        if (hold == 0)
        {
            const auto qInverse = (double) t.qInverse[(size_t) k];
            const auto smoothed = (double) t.smoothed[(size_t) k];

            actMin = std::min (actMin, (float) (smoothed * martinBias (frames, mWindow, qInverse) * biasGlobal));
            subMin = std::min (subMin, (float) (smoothed * martinBias (subwindowFrames, mSubwindow, qInverse) * biasGlobal));
        }

        auto& noise = t.noise[(size_t) k];

        if (boundary)
        {
            const auto value = actMin < std::numeric_limits<float>::max() ? actMin : noise;
            t.ring[(size_t) (t.ringIndex * bins + k)] = value;

            auto lowest = std::numeric_limits<float>::max();

            for (int u = 0; u < trackSubwindows; ++u)
                lowest = std::min (lowest, t.ring[(size_t) (u * bins + k)]);

            t.ringMin[(size_t) k] = lowest;
            noise = std::max ((float) powerFloor, lowest);
            actMin = std::numeric_limits<float>::max();
            subMin = std::numeric_limits<float>::max();
        }
        else if (hold == 0)
        {
            noise = std::max ((float) powerFloor, std::min (t.ringMin[(size_t) k], subMin));
        }
    }

    if (boundary)
    {
        t.ringIndex = (t.ringIndex + 1) % trackSubwindows;
        t.subFrames = 0;
        t.subLength = std::max (1, (int) std::lround (frames / (double) trackSubwindows));
    }
    else
    {
        ++t.subFrames;
    }

    if (publishes && domain == shownDomain())
        shared.heldFraction.store (binsBelow8k > 0 ? (float) heldBelow8k / (float) binsBelow8k : 0.0f, std::memory_order_relaxed);

    if (publishes)
    {
        // At most every 20 ms: the grid is 2049 points.
        if (--t.publishCountdown <= 0)
        {
            publishTracking (domain);
            t.publishCountdown = std::max (1, (int) std::lround (0.02 / hopSeconds));
        }
    }
}

void SpectralEngine::publishTracking (int domain) noexcept
{
    const auto& t = trackers[domain];
    auto& adaptive = domain == 0 ? shared.adaptive : shared.sideAdaptive;

    const auto binStep = rate / (double) fftLength;

    for (int j = 0; j < profilePoints; ++j)
    {
        const auto position = std::min (profileHz (j) / binStep, (double) (bins - 1));
        const auto index = std::min ((int) position, bins - 2);
        const auto fraction = position - (double) index;
        const auto value = (double) t.noise[(size_t) index] + fraction * ((double) t.noise[(size_t) index + 1] - (double) t.noise[(size_t) index]);

        adaptive[(size_t) j].store ((float) value, std::memory_order_relaxed);
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
            sum += 10.0 * std::log10 (std::max ((double) t.noise[(size_t) k] * displayScale, powerFloor));
            ++count;
        }
    }

    if (domain == shownDomain())
        shared.adaptiveFloorDb.store (count > 0 ? (float) (sum / count) : -200.0f, std::memory_order_relaxed);
    (domain == 0 ? shared.hasAdaptive : shared.hasSideAdaptive).store (true, std::memory_order_release);
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
    const auto domains = midSideFrame() ? 2 : 1;

    if (tracking)
    {
        for (int domain = 0; domain < domains; ++domain)
            trackNoise (domain, frameLearns);
    }
    else if (trackers[0].started || trackers[1].started)
    {
        trackers[0].started = trackers[1].started = false;

        if (frameLearns)
        {
            shared.hasAdaptive.store (false, std::memory_order_release);
            shared.hasSideAdaptive.store (false, std::memory_order_release);
        }
    }

    const auto useKey = frameKey && settings.sidechain && settings.mode != eq && settings.mode != resonance;

    double reductionColumns[displayColumns] {};

    // Resonance is linked: one gain for both channels, worked out once (in Mid/Side, once for
    // each domain).
    if (settings.mode == resonance)
    {
        for (int domain = 0; domain < domains; ++domain)
            computeResonance (domain, reductionColumns);
    }
    else
    {
        resStarted[0] = resStarted[1] = false;
    }

    for (int ch = 0; ch < frameChannels; ++ch)
    {
        auto& gain = gains[ch];
        auto& result = mask[ch];
        const auto& spectrum = spectra[ch];
        const auto& key = spectra[ch + 2];

        // Phase 4 Wave 2 (item 28): in Mid/Side, the side's curves, profile and estimate for
        // channel 1; the graph's reduction is the shown domain's.
        const auto domain = domainOf (ch);
        const auto shows = domain == shownDomain();

        if (settings.mode == eq)
        {
            const auto& curve = curveDb[curveFor (eq, ch)];

            for (int k = 0; k < bins; ++k)
                result[(size_t) k] = (float) std::pow (10.0, (double) curve[(size_t) k] / 20.0);
        }
        else if (settings.mode == gate)
        {
            const auto attack = perHop (settings.gateAttackMs), release = perHop (settings.gateReleaseMs);
            const auto& threshold = curveDb[curveFor (gate, ch)];

            measureLevels (useKey ? key : spectrum);

            for (int k = 0; k < bins; ++k)
            {
                const auto levelDb = (double) detectorLevels[(size_t) k];
                const auto target = levelDb >= (double) threshold[(size_t) k] ? 1.0 : settings.gateFloorGain;

                auto value = (double) gain[(size_t) k];
                value += (target - value) * (target > value ? attack : release);

                gain[(size_t) k] = (float) value;
                result[(size_t) k] = (float) value;

                const auto column = binColumn[(size_t) k];

                if (column >= 0 && shows)
                    reductionColumns[column] = std::max (reductionColumns[column], -20.0 * std::log10 (std::max (value, 1.0e-6)));
            }
        }
        else if (settings.mode == compressor)
        {
            const auto attack = perHop (settings.compAttackMs), release = perHop (settings.compReleaseMs);
            const auto& threshold = curveDb[curveFor (compressor, ch)];

            // Phase 4 Wave 2 (item 27): with a ratio curve, each bin's ratio is the knob's times
            // the curve's; flat, the old path, to the bit.
            const auto& ratios = curveDb[curveFor (ratioCurve, ch)];
            const auto perBin = ! ratioFlat[domain];

            measureLevels (useKey ? key : spectrum);

            for (int k = 0; k < bins; ++k)
            {
                auto binSlope = settings.compSlope;

                if (perBin)
                    binSlope = 1.0 - 1.0 / ratioFromCurve (settings.compRatio, (double) ratios[(size_t) k]);

                const auto slope = std::abs (binSlope);
                const auto expanding = binSlope < 0.0;

                const auto levelDb = (double) detectorLevels[(size_t) k];
                const auto target = fancy::dsp::computeReduction (levelDb, (double) threshold[(size_t) k], slope, 0.0, false);

                auto envelope = (double) envelopes[ch][(size_t) k];
                envelope += (target - envelope) * (target > envelope ? attack : release);
                envelopes[ch][(size_t) k] = (float) envelope;

                const auto changeDb = expanding ? -std::min (envelope, maxExpansionDb) : envelope;
                result[(size_t) k] = (float) std::pow (10.0, -changeDb / 20.0);

                const auto column = binColumn[(size_t) k];

                if (column >= 0 && shows)
                    reductionColumns[column] = std::max (reductionColumns[column], changeDb);
            }
        }
        else if (settings.mode == resonance)
        {
            std::copy (resonanceGain[domain].begin(), resonanceGain[domain].end(), result.begin());
        }
        else if (settings.mode == subtract)
        {
            if (! profileReady[domain] && ! useKey && ! tracking)
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
                    noise = (double) profilePower[domain][(size_t) k] * settings.offsetPower;
                }
                else if (settings.source == adaptiveSource || ! profileReady[domain])
                {
                    noise = (double) trackers[domain].noise[(size_t) k] * settings.offsetPower;
                }
                else
                {
                    // Both: the learned profile is the floor under the adaptive estimate.
                    noise = std::max ((double) profilePower[domain][(size_t) k], (double) trackers[domain].noise[(size_t) k]) * settings.offsetPower;
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

                if (column >= 0 && shows)
                    reductionColumns[column] = std::max (reductionColumns[column], -20.0 * std::log10 (std::max (value, 1.0e-6)));
            }
        }

        if (settings.reduceArtifacts)
        {
            std::copy (result.begin(), result.end(), scratch.begin());

            for (int k = 1; k < bins - 1; ++k)
                result[(size_t) k] = 0.25f * scratch[(size_t) k - 1] + 0.5f * scratch[(size_t) k] + 0.25f * scratch[(size_t) k + 1];
        }

        if (settings.listen && (settings.mode == gate || settings.mode == compressor || settings.mode == subtract || settings.mode == resonance))
            for (auto& value : result)
                value = 1.0f - value;
    }

    if (frameLearns)
        for (int column = 0; column < displayColumns; ++column)
            shared.reduction[(size_t) column].store ((float) reductionColumns[column], std::memory_order_relaxed);
}

// Phase 3 Q2.7: Resonance mode -----------------------------------------------------------------
//
// Modelled first in docs/research/fir-resonance-model.py; the constants are the model's.

namespace
{
constexpr double resonanceToleranceDb = 4.0; // how far a bin must stand above its envelope
constexpr double resonanceDetectorMs = 80.0; // the detector's own smoothing
constexpr int resonanceCore = 2;             // bins either side left out of a bin's envelope
constexpr int resonancePasses = 2;           // extra passes with levels clipped at envelope + tolerance
constexpr double widestOctaves = 2.0, narrowestOctaves = 1.0 / 3.0;
} // namespace

void SpectralEngine::prepareResonanceWindows (double sharpness) noexcept
{
    resWindowSharpness = sharpness;

    const auto octaves = widestOctaves * std::pow (narrowestOctaves / widestOctaves, std::clamp (sharpness, 0.0, 1.0));
    const auto factor = std::pow (2.0, octaves / 2.0) - 1.0;
    const auto binWidth = rate / (double) fftLength;

    for (int k = 0; k < bins; ++k)
    {
        const auto half = std::max (resonanceCore + 4, (int) std::lround ((double) binHz[(size_t) k] * factor / binWidth));

        resLow[(size_t) k] = std::max (0, k - half);
        resHigh[(size_t) k] = std::min (bins - 1, k + half);
    }
}

void SpectralEngine::prepareResonanceRange (double lowHz, double highHz) noexcept
{
    resRangeLow = lowHz;
    resRangeHigh = highHz;

    // Full inside Low to High, fading to nothing over half an octave outside.
    for (int k = 0; k < bins; ++k)
    {
        const auto hz = std::max ((double) binHz[(size_t) k], 1.0);
        const auto outside = std::max (std::log2 (lowHz / hz), std::log2 (hz / highHz));

        resWeight[(size_t) k] = (float) std::clamp (1.0 - 2.0 * outside, 0.0, 1.0);
    }
}

double SpectralEngine::tiltFactor (double hz, double tilt) noexcept
{
    return std::pow (2.0, std::clamp (tilt, -1.0, 1.0) * std::log2 (std::max (hz, 1.0) / 1000.0) / 4.0);
}

void SpectralEngine::prepareResonanceTilt (double tilt) noexcept
{
    resTiltShown = tilt;

    for (int k = 0; k < bins; ++k)
        resTiltFactor[(size_t) k] = (float) tiltFactor ((double) binHz[(size_t) k], tilt);
}

void SpectralEngine::computeResonance (int domain, double* reductionColumns) noexcept
{
    const auto& settings = frameSettings;
    auto& detectorState = resDetector[domain];
    auto& reductionState = resReduction[domain];
    auto& started = resStarted[domain];

    // Linked over both channels, or in Mid/Side the domain's own channel.
    const auto firstChannel = midSideFrame() ? domain : 0;
    const auto numListened = midSideFrame() ? 1 : frameChannels;
    const auto shows = domain == shownDomain();

    if (settings.resSharpness != resWindowSharpness)
        prepareResonanceWindows (settings.resSharpness);

    if (settings.resLowHz != resRangeLow || settings.resHighHz != resRangeHigh)
        prepareResonanceRange (settings.resLowHz, settings.resHighHz);

    // Detector: the channels' power together, averaged over ±1 bin, then smoothed.
    const auto calibration = levelScale * levelScale / (double) numListened;

    for (int k = 0; k < bins; ++k)
    {
        auto power = 0.0;

        for (int ch = firstChannel; ch < firstChannel + numListened; ++ch)
            power += (double) binPower (spectra[ch], k);

        scratch[(size_t) k] = (float) (power * calibration);
    }

    const auto detector = perHop (resonanceDetectorMs);

    if (! started)
        std::fill (reductionState.begin(), reductionState.end(), 0.0f);

    for (int k = 0; k < bins; ++k)
    {
        // At the ends the missing neighbour counts as zero, as in the model's "same" convolution.
        const auto below = k > 0 ? (double) scratch[(size_t) k - 1] : 0.0;
        const auto above = k < bins - 1 ? (double) scratch[(size_t) k + 1] : 0.0;
        const auto reach = (below + (double) scratch[(size_t) k] + above) / 3.0;

        auto& value = detectorState[(size_t) k];
        value = started ? (float) ((double) value + (reach - (double) value) * detector) : (float) reach;

        resLevel[(size_t) k] = (float) (10.0 * std::log10 (std::max ((double) value, powerFloor)));
    }

    started = true;

    // Envelope: the louder side's mean level, without the core; then again with levels clipped
    // at envelope + tolerance, so a peak doesn't lift its own envelope.
    std::copy (resLevel.begin(), resLevel.end(), resClipped.begin());

    for (int pass = 0; pass <= resonancePasses; ++pass)
    {
        resPrefix[0] = 0.0;

        for (int k = 0; k < bins; ++k)
            resPrefix[(size_t) k + 1] = resPrefix[(size_t) k] + (double) resClipped[(size_t) k];

        for (int k = 0; k < bins; ++k)
        {
            const auto lo = resLow[(size_t) k], hi = resHigh[(size_t) k];
            const auto leftEnd = k - resonanceCore - 1, rightStart = k + resonanceCore + 1;
            const auto leftCount = leftEnd - lo + 1, rightCount = hi - rightStart + 1;

            auto envelope = -std::numeric_limits<double>::infinity();

            if (leftCount > 0)
                envelope = (resPrefix[(size_t) leftEnd + 1] - resPrefix[(size_t) lo]) / (double) leftCount;

            if (rightCount > 0)
                envelope = std::max (envelope, (resPrefix[(size_t) hi + 1] - resPrefix[(size_t) rightStart]) / (double) rightCount);

            if (! std::isfinite (envelope))
                envelope = (double) resLevel[(size_t) k];

            resEnvelope[(size_t) k] = (float) envelope;
        }

        for (int k = 0; k < bins; ++k)
            resClipped[(size_t) k] = std::min (resLevel[(size_t) k], resEnvelope[(size_t) k] + (float) resonanceToleranceDb);
    }

    const auto attack = perHop (settings.resAttackMs), release = perHop (settings.resReleaseMs);
    const auto depth = std::max (0.0, settings.resDepthDb);

    // Phase 4 Wave 1: with a tilt, each bin's Depth is scaled; at 0 the old path, to the bit.
    // Phase 4 Wave 2: and by the depth curve's nodes, when it isn't flat.
    const auto tilted = settings.resTilt != 0.0;
    const auto noded = ! depthFlat[domain];
    const auto& depthNodes = curveDb[domain == 1 ? sideCurve (depthCurve) : depthCurve];

    if (tilted && settings.resTilt != resTiltShown)
        prepareResonanceTilt (settings.resTilt);

    for (int k = 0; k < bins; ++k)
    {
        const auto excess = (double) resLevel[(size_t) k] - (double) resEnvelope[(size_t) k] - resonanceToleranceDb;
        auto binDepth = tilted ? depth * (double) resTiltFactor[(size_t) k] : depth;

        if (noded)
            binDepth *= std::clamp ((double) depthNodes[(size_t) k], 0.0, maxDepthPercent) * 0.01;
        const auto target = std::clamp (excess, 0.0, binDepth) * (double) resWeight[(size_t) k];

        auto reduction = (double) reductionState[(size_t) k];
        reduction += (target - reduction) * (target > reduction ? attack : release);
        reductionState[(size_t) k] = (float) reduction;

        resonanceGain[domain][(size_t) k] = (float) std::pow (10.0, -reduction / 20.0);

        const auto column = binColumn[(size_t) k];

        if (column >= 0 && shows)
            reductionColumns[column] = std::max (reductionColumns[column], reduction);
    }
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

    // Phase 4 Wave 2 (item 28): mid and side in, left and right out. The sidechain is turned to
    // mid and side too, so each domain's detector hears its own domain.
    const auto midSide = engineConfig.midSide && ! engineConfig.convolve && numChannels > 1;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto position = (size_t) (time & inputMask);

        if (midSide)
        {
            inputRings[0][position] = 0.5f * (inLeft[i] + inRight[i]);
            inputRings[1][position] = 0.5f * (inLeft[i] - inRight[i]);
            inputRings[2][position] = hasKey ? 0.5f * (keyLeft[i] + keyRight[i]) : 0.0f;
            inputRings[3][position] = hasKey ? 0.5f * (keyLeft[i] - keyRight[i]) : 0.0f;
        }
        else
        {
            inputRings[0][position] = inLeft[i];
            inputRings[1][position] = numChannels > 1 ? inRight[i] : inLeft[i];
            inputRings[2][position] = hasKey ? keyLeft[i] : 0.0f;
            inputRings[3][position] = hasKey ? keyRight[i] : 0.0f;
        }

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

        if (midSide)
        {
            const auto mid = outputRings[0][readPosition], side = outputRings[1][readPosition];

            outLeft[i] = mid + side;

            if (outRight != nullptr)
                outRight[i] = mid - side;

            outputRings[0][readPosition] = 0.0f;
            outputRings[1][readPosition] = 0.0f;
            continue;
        }

        outLeft[i] = outputRings[0][readPosition];
        outputRings[0][readPosition] = 0.0f;

        if (outRight != nullptr)
        {
            outRight[i] = numChannels > 1 ? outputRings[1][readPosition] : outLeft[i];
            outputRings[1][readPosition] = 0.0f;
        }
    }
}

} // namespace fir::legacyW2
