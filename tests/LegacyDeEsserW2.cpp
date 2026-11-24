#include "LegacyDeEsserW2.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
constexpr double smoothingMs = 20.0, switchFadeMs = 10.0;
constexpr int filterInterval = 16;

double toDb (double gain) noexcept
{
    return gain > 1.0e-10 ? 20.0 * std::log10 (gain) : -200.0;
}

double fromDb (double db) noexcept
{
    return std::pow (10.0, db / 20.0);
}
} // namespace

namespace fancy::dsp
{

double LegacyDeEsserW2::lookaheadMs (int choice) noexcept
{
    switch (choice)
    {
        case lookahead1: return 1.0;
        case lookahead2: return 2.0;
        case lookahead5: return 5.0;
        default:         return 0.0;
    }
}

double LegacyDeEsserW2::qForWidth (double octaves) noexcept
{
    const auto ratio = std::pow (2.0, octaves);
    return std::sqrt (ratio) / (ratio - 1.0);
}

LegacyDeEsserW2::Edges LegacyDeEsserW2::edgesFor (const Targets& targets) noexcept
{
    Edges edges;

    if (targets.shelf)
    {
        edges.low = std::clamp (targets.freqHz, minHz, maxHz);
        edges.high = 0.0;
        edges.bands = 2;
        return edges;
    }

    const auto half = std::pow (2.0, 0.5 * targets.widthOctaves);
    edges.high = std::min (targets.freqHz * half, 20000.0);
    edges.low = std::min (targets.freqHz / half, edges.high / 1.26);
    edges.bands = 3;
    return edges;
}

// Phase 4 Wave 2 (Decision 3 A): with two bands, five linear-phase bands (four with a High shelf),
// the Low band's upper edge held under the High band's lower edge so they never overlap.
LegacyDeEsserW2::Edges LegacyDeEsserW2::dualEdgesFor (const Targets& targets) noexcept
{
    auto edges = edgesFor (targets);
    const auto half = std::pow (2.0, 0.5 * targets.lowWidthOctaves);
    const auto lowHz = std::clamp (targets.lowFreqHz, lowMinHz, lowMaxHz);

    edges.lowBandHigh = std::min (lowHz * half, edges.low / 1.26);
    edges.lowBandLow = std::max (20.0, std::min (lowHz / half, edges.lowBandHigh / 1.26));
    edges.bands += 2;
    return edges;
}

void LegacyDeEsserW2::prepare (double sampleRate)
{
    rate = sampleRate;

    crossover.prepare (sampleRate);

    const auto maxLookahead = (int) std::lround (lookaheadMs (lookahead5) * 0.001 * sampleRate);
    const auto crossoverLatency = crossover.getLatencySamples();

    lookaheadSize = maxLookahead + 1;
    rawSize = crossoverLatency + 1;
    gainSize = crossoverLatency + 1;
    drySize = maxLookahead + crossoverLatency + 1;

    for (auto& line : lookaheadLines) line.assign ((size_t) lookaheadSize, 0.0f);
    for (auto& line : rawLines) line.assign ((size_t) rawSize, 0.0f);
    for (auto& line : dryLines) line.assign ((size_t) drySize, 0.0f);
    for (auto& line : gainLines) line.assign ((size_t) gainSize, 1.0);
    for (auto& line : lowGainLines) line.assign ((size_t) gainSize, 1.0);
    for (auto& buffer : crossoverInput) buffer.assign ((size_t) maxChunk, 0.0f);

    bandScratch.assign ((size_t) (5 * 2 * maxChunk), 0.0);

    holdSamples = 0.001 * sampleRate;
    fallPerSample = 20.0 / sampleRate;
    rmsCoefficient = fancy::dsp::timeCoefficient (50.0, sampleRate);

    for (auto* smoother : { &logFreq, &width, &lowLogFreq, &lowWidth, &outputGain, &bypassMix })
        smoother->setTime (smoothingMs, sampleRate);

    switchGain.setTime (switchFadeMs / 5.0, sampleRate);

    reset();
}

int LegacyDeEsserW2::latencyFor (int lookahead, bool linear) const noexcept
{
    return (int) std::lround (lookaheadMs (lookahead) * 0.001 * rate) + (linear ? crossover.getLatencySamples() : 0);
}

void LegacyDeEsserW2::reset()
{
    for (auto* lines : { &lookaheadLines, &rawLines, &dryLines })
        for (auto& line : *lines)
            std::fill (line.begin(), line.end(), 0.0f);

    for (auto* lines : { &gainLines, &lowGainLines })
        for (auto& line : *lines)
            std::fill (line.begin(), line.end(), 1.0);

    lookaheadPosition = rawPosition = dryPosition = gainPosition = 0;

    for (auto& filter : detectors) filter.reset();
    for (auto& stages : extraDetectors) for (auto& filter : stages) filter.reset();
    for (auto& filter : splits) filter.reset();
    for (auto& follower : followers) follower.reset();
    resetLow();

    peakDb = { -200.0, -200.0 };
    filterCountdown = 0;
    meanSquare = { 0.0, 0.0 };
    holdLeft = { 0, 0 };
    holdOffSamples = 0;

    snapToTargets (current);
}

void LegacyDeEsserW2::snapToTargets (const Targets& targets) noexcept
{
    current = targets;

    activeLookahead = targets.lookahead;
    activeLinear = targets.linearPhase;
    activeShelf = targets.shelf;
    activeDual = targets.dual;

    lowLogFreq.snapTo (std::log2 (std::clamp (targets.lowFreqHz, lowMinHz, lowMaxHz)));
    lowWidth.snapTo (targets.lowWidthOctaves);
    updateLowFilters();

    logFreq.snapTo (std::log2 (std::clamp (targets.freqHz, minHz, maxHz)));
    width.snapTo (targets.widthOctaves);
    outputGain.snapTo (targets.outputGain);
    bypassMix.snapTo (targets.bypass ? 1.0 : 0.0);
    switchGain.snapTo (1.0);
    updateFilters();

    crossover.setBandCount ((targets.dual ? dualEdgesFor (targets) : edgesFor (targets)).bands);
    designNeeded.store (true, std::memory_order_release);

    if (synchronousDesign)
        updateLinearDesign (targets);
}

void LegacyDeEsserW2::continueFrom (const LegacyDeEsserW2& other) noexcept
{
    detectors = other.detectors;
    extraDetectors = other.extraDetectors;
    splits = other.splits;
    followers = other.followers;
    peakDb = other.peakDb;
    meanSquare = other.meanSquare;
    holdLeft = other.holdLeft;
    filterCountdown = other.filterCountdown;
    lowDetectors = other.lowDetectors;
    lowExtraDetectors = other.lowExtraDetectors;
    lowSplits = other.lowSplits;
    lowFollowers = other.lowFollowers;
    lowPeakDb = other.lowPeakDb;
    lowHoldLeft = other.lowHoldLeft;
    lowRunning = other.lowRunning;
    lowGainLines = other.lowGainLines;

    // The same sizes, so these copy without allocating.
    lookaheadLines = other.lookaheadLines;
    rawLines = other.rawLines;
    dryLines = other.dryLines;
    gainLines = other.gainLines;
    lookaheadPosition = other.lookaheadPosition;
    rawPosition = other.rawPosition;
    dryPosition = other.dryPosition;
    gainPosition = other.gainPosition;
}

void LegacyDeEsserW2::updateFilters() noexcept
{
    const auto hz = std::exp2 (logFreq.getCurrent());
    bandQ = current.shelf ? 0.7071 : qForWidth (width.getCurrent());
    warpedFreq = std::tan (std::numbers::pi * hz / rate);
    coefficients.set (hz, bandQ, rate);
}

void LegacyDeEsserW2::updateLowFilters() noexcept
{
    const auto hz = std::exp2 (lowLogFreq.getCurrent());
    lowBandQ = qForWidth (lowWidth.getCurrent());
    lowWarpedFreq = std::tan (std::numbers::pi * hz / rate);
    lowCoefficients.set (hz, lowBandQ, rate);
}

void LegacyDeEsserW2::resetLow() noexcept
{
    for (auto& filter : lowDetectors) filter.reset();
    for (auto& stages : lowExtraDetectors) for (auto& filter : stages) filter.reset();
    for (auto& filter : lowSplits) filter.reset();
    for (auto& follower : lowFollowers) follower.reset();

    lowPeakDb = { -200.0, -200.0 };
    lowHoldLeft = { 0, 0 };
    lowRunning = false;
}

void LegacyDeEsserW2::updateLinearDesign (const Targets& targets) noexcept
{
    if (! targets.linearPhase)
        return;

    const auto edges = targets.dual ? dualEdgesFor (targets) : edgesFor (targets);
    const auto forced = designNeeded.exchange (false, std::memory_order_acq_rel);

    if (! forced && edges == designedEdges)
        return;

    if (targets.dual)
    {
        const double hz[] { edges.lowBandLow, edges.lowBandHigh, edges.low, edges.high };
        crossover.design (hz, edges.bands);
    }
    else
    {
        const double hz[] { edges.low, edges.high };
        crossover.design (hz, edges.bands);
    }
    designedEdges = edges;
}

bool LegacyDeEsserW2::isDesignPending (const Targets& targets) const noexcept
{
    return targets.linearPhase && (designNeeded.load (std::memory_order_acquire) || (targets.dual ? dualEdgesFor (targets) : edgesFor (targets)) != designedEdges);
}

void LegacyDeEsserW2::process (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int numSamples) noexcept
{
    for (int start = 0; start < numSamples; start += maxChunk)
    {
        float* mainPieces[2] { main[0] + start, numMain > 1 ? main[1] + start : nullptr };
        const float* sidePieces[2] { numSidechain > 0 ? sidechain[0] + start : nullptr, numSidechain > 1 ? sidechain[1] + start : nullptr };
        processChunk (mainPieces, numMain, sidePieces, numSidechain, std::min (maxChunk, numSamples - start));
    }
}

void LegacyDeEsserW2::processChunk (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int numSamples) noexcept
{
    const auto settings = current;
    auto* left = main[0];
    auto* right = numMain > 1 ? main[1] : nullptr;

    // Lookahead, the split filter and, with the linear split, the shape change the latency or
    // the crossover: they fade out, swap and fade back in.
    const auto wantsSwitch = settings.lookahead != activeLookahead || settings.linearPhase != activeLinear
                          || (activeLinear && settings.shelf != activeShelf) || (activeLinear && settings.dual != activeDual);

    if (wantsSwitch)
    {
        switchGain.setTarget (0.0);

        if (switchGain.isSettled())
        {
            activeLookahead = settings.lookahead;
            activeLinear = settings.linearPhase;
            activeShelf = settings.shelf;
            activeDual = settings.dual;

            for (auto* lines : { &lookaheadLines, &rawLines, &dryLines })
                for (auto& line : *lines)
                    std::fill (line.begin(), line.end(), 0.0f);

            for (auto* lines : { &gainLines, &lowGainLines })
                for (auto& line : *lines)
                    std::fill (line.begin(), line.end(), 1.0);

            if (activeLinear)
            {
                crossover.setBandCount ((settings.dual ? dualEdgesFor (settings) : edgesFor (settings)).bands);
                designNeeded.store (true, std::memory_order_release);
            }

            holdOffSamples = getLatencySamples();
        }
    }
    else
    {
        activeShelf = settings.shelf;
        activeDual = settings.dual;

        // A linear split has no output until its first filter arrives.
        if (holdOffSamples <= 0 && (! activeLinear || crossover.hasFilter()))
            switchGain.setTarget (1.0);
    }

    if (synchronousDesign && activeLinear)
        updateLinearDesign (settings);

    ballistics.set (settings.attackMs, settings.releaseMs, false, false, false, rate);
    logFreq.setTarget (std::log2 (std::clamp (settings.freqHz, minHz, maxHz)));
    width.setTarget (settings.widthOctaves);
    lowLogFreq.setTarget (std::log2 (std::clamp (settings.lowFreqHz, lowMinHz, lowMaxHz)));
    lowWidth.setTarget (settings.lowWidthOctaves);
    outputGain.setTarget (settings.outputGain);
    bypassMix.setTarget (settings.bypass ? 1.0 : 0.0);

    const auto twoDetectors = settings.detector == mainLR || settings.detector == sideLR;
    const auto lookaheadDelay = (int) std::lround (lookaheadMs (activeLookahead) * 0.001 * rate);
    const auto crossoverDelay = activeLinear ? crossover.getLatencySamples() : 0;

    std::array<std::array<float, maxChunk>, 2> delayed {};
    std::array<std::array<double, maxChunk>, 2> gains {}, lowGains {};

    // The split filter's settings at each sample: step 2 runs after step 1 has moved them on,
    // and reading them there would tie the sound to where the host's blocks start.
    std::array<fancy::dsp::SvfCoefficients, maxChunk> splitCoefficients;
    std::array<double, maxChunk> splitWarped, splitQ, lowSplitWarped, lowSplitQ;
    auto chunkReduction = 0.0, chunkLowReduction = 0.0;

    // The Low band runs while Dual band is on, and after until its reduction has let go.
    if (settings.dual)
        lowRunning = true;

    const auto lowInChunk = lowRunning;

    // 1. The detector and the gain, and the audio through the lookahead.
    for (int i = 0; i < numSamples; ++i)
    {
        if (--filterCountdown <= 0)
        {
            logFreq.next();
            width.next();

            for (int skip = 1; skip < filterInterval; ++skip)
            {
                logFreq.next();
                width.next();
            }

            updateFilters();

            if (lowRunning)
            {
                for (int skip = 0; skip < filterInterval; ++skip)
                {
                    lowLogFreq.next();
                    lowWidth.next();
                }

                updateLowFilters();
            }

            filterCountdown = filterInterval;
        }

        lowSplitWarped[(size_t) i] = lowWarpedFreq;
        lowSplitQ[(size_t) i] = lowBandQ;

        splitCoefficients[(size_t) i] = coefficients;
        splitWarped[(size_t) i] = warpedFreq;
        splitQ[(size_t) i] = bandQ;

        const auto l = (double) left[i];
        const auto r = right != nullptr ? (double) right[i] : l;
        const auto sl = sidechain[0] != nullptr ? (double) sidechain[0][i] : 0.0;
        const auto sr = sidechain[1] != nullptr ? (double) sidechain[1][i] : sl;

        std::array<double, 2> sources {};

        switch (settings.detector)
        {
            case mainLR:   sources = { l, r }; break;
            case mainMid:  sources[0] = 0.5 * (l + r); break;
            case mainSide: sources[0] = 0.5 * (l - r); break;
            case mainL:    sources[0] = l; break;
            case mainR:    sources[0] = r; break;
            case sideLR:   sources = { sl, sr }; break;
            case sideL:    sources[0] = sl; break;
            default:       sources[0] = sr; break;
        }

        std::array<double, 2> levels {}, fullLevels {};

        for (int c = 0; c < (twoDetectors ? 2 : 1); ++c)
        {
            const auto out = detectors[(size_t) c].tick (coefficients, sources[(size_t) c]);
            auto filtered = settings.shelf ? out.high : coefficients.k * out.band;

            // Phase 4 Wave 1: further identical stages for 24 and 48 dB/oct.
            for (int stage = 0; stage < std::min (settings.slopeStages, 4) - 1; ++stage)
            {
                const auto more = extraDetectors[(size_t) c][(size_t) stage].tick (coefficients, filtered);
                filtered = settings.shelf ? more.high : coefficients.k * more.band;
            }
            const auto db = toDb (std::abs (filtered));

            // Peak, held for 1 ms, then falling at 20 dB/s.
            if (db >= peakDb[(size_t) c])
            {
                peakDb[(size_t) c] = db;
                holdLeft[(size_t) c] = (int) holdSamples;
            }
            else if (holdLeft[(size_t) c] > 0)
            {
                --holdLeft[(size_t) c];
            }
            else
            {
                peakDb[(size_t) c] = std::max (db, peakDb[(size_t) c] - fallPerSample);
            }

            meanSquare[(size_t) c] += (sources[(size_t) c] * sources[(size_t) c] - meanSquare[(size_t) c]) * rmsCoefficient;
            const auto fullDb = std::max (fullBandFloorDb, 10.0 * std::log10 (meanSquare[(size_t) c] + 1.0e-30));

            levels[(size_t) c] = settings.relative ? peakDb[(size_t) c] - fullDb : peakDb[(size_t) c];
            fullLevels[(size_t) c] = fullDb;
        }

        if (! twoDetectors || settings.link)
            levels[0] = levels[1] = twoDetectors ? std::max (levels[0], levels[1]) : levels[0];

        for (size_t c = 0; c < 2; ++c)
        {
            const auto target = std::min (fancy::dsp::computeReduction (levels[c], settings.thresholdDb, ratioSlope, kneeDb, false), -settings.rangeDb);
            const auto smoothed = followers[c].follow (ballistics, std::max (0.0, target));
            gains[c][(size_t) i] = fromDb (-smoothed);
            chunkReduction = std::max (chunkReduction, smoothed);
        }

        // Phase 4 Wave 2: the Low band, detected from the same sources (Decision 2 A).
        if (lowRunning)
        {
            std::array<double, 2> lowLevels {};

            for (int c = 0; c < (twoDetectors ? 2 : 1); ++c)
            {
                const auto out = lowDetectors[(size_t) c].tick (lowCoefficients, sources[(size_t) c]);
                auto filtered = lowCoefficients.k * out.band;

                for (int stage = 0; stage < std::min (settings.slopeStages, 4) - 1; ++stage)
                    filtered = lowCoefficients.k * lowExtraDetectors[(size_t) c][(size_t) stage].tick (lowCoefficients, filtered).band;

                const auto db = toDb (std::abs (filtered));

                if (db >= lowPeakDb[(size_t) c])
                {
                    lowPeakDb[(size_t) c] = db;
                    lowHoldLeft[(size_t) c] = (int) holdSamples;
                }
                else if (lowHoldLeft[(size_t) c] > 0)
                {
                    --lowHoldLeft[(size_t) c];
                }
                else
                {
                    lowPeakDb[(size_t) c] = std::max (db, lowPeakDb[(size_t) c] - fallPerSample);
                }

                lowLevels[(size_t) c] = settings.relative ? lowPeakDb[(size_t) c] - fullLevels[(size_t) c] : lowPeakDb[(size_t) c];
            }

            if (! twoDetectors || settings.link)
                lowLevels[0] = lowLevels[1] = twoDetectors ? std::max (lowLevels[0], lowLevels[1]) : lowLevels[0];

            auto released = true;
            auto mostLowDb = 0.0;

            for (size_t c = 0; c < 2; ++c)
            {
                const auto target = settings.dual
                                        ? std::min (fancy::dsp::computeReduction (lowLevels[c], settings.lowThresholdDb, ratioSlope, kneeDb, false), -settings.lowRangeDb)
                                        : 0.0;
                const auto smoothed = lowFollowers[c].follow (ballistics, std::max (0.0, target));
                lowGains[c][(size_t) i] = fromDb (-smoothed);
                chunkLowReduction = std::max (chunkLowReduction, smoothed);
                mostLowDb = std::max (mostLowDb, smoothed);
                released = released && smoothed < 1.0e-6;
            }

            // The display's values: the reduction straight from the followers (already in dB), and
            // the meter's level converted once.
            const auto shownLow = std::max (lowLevels[0], lowLevels[1]);
            const auto shownLowGain = (float) fromDb (std::min (shownLow, 0.0));
            lowLevelMeter.report (0, shownLowGain);
            lowLevelMeter.report (1, shownLowGain);
            columnLowLevel = std::max (columnLowLevel, (float) shownLow);
            columnLowReduction = std::max (columnLowReduction, (float) mostLowDb);

            // Off and let go: exactly unity from here, and the band stops.
            if (! settings.dual && released)
            {
                lowGains[0][(size_t) i] = lowGains[1][(size_t) i] = 1.0;
                resetLow();
            }
        }
        else
        {
            lowGains[0][(size_t) i] = lowGains[1][(size_t) i] = 1.0;
        }

        const auto shownLevel = std::max (levels[0], levels[1]);
        levelMeter.report (0, (float) fromDb (std::min (shownLevel, 0.0)));
        levelMeter.report (1, (float) fromDb (std::min (shownLevel, 0.0)));

        columnLevel = std::max (columnLevel, (float) shownLevel);
        columnReduction = std::max (columnReduction, (float) std::max (-20.0 * std::log10 (gains[0][(size_t) i]), -20.0 * std::log10 (gains[1][(size_t) i])));

        if (++columnFill >= std::max (1, (int) (historySeconds * rate / historyColumns.load (std::memory_order_relaxed))))
        {
            pushHistory ({ columnLevel, columnReduction, columnLowLevel, columnLowReduction });
            columnLevel = columnLowLevel = -200.0f;
            columnReduction = columnLowReduction = 0.0f;
            columnFill = 0;
        }

        // The audio, delayed by the lookahead; the detector isn't.
        const float input[2] { (float) l, (float) r };

        for (size_t c = 0; c < 2; ++c)
        {
            auto& line = lookaheadLines[c];
            line[(size_t) lookaheadPosition] = input[c];
            delayed[c][(size_t) i] = line[(size_t) ((lookaheadPosition + lookaheadSize - lookaheadDelay) % lookaheadSize)];
        }

        lookaheadPosition = (lookaheadPosition + 1) % lookaheadSize;
    }

    // 2. The split, then Listen, Output, the switch fade and bypass.
    if (activeLinear)
        crossover.process (delayed[0].data(), delayed[1].data(), nullptr, nullptr, bandScratch.data(), nullptr, maxChunk, numSamples);

    const auto totalLatency = lookaheadDelay + crossoverDelay;

    for (int i = 0; i < numSamples; ++i)
    {
        std::array<double, 2> outputs {}, references {};

        for (size_t c = 0; c < 2; ++c)
        {
            auto reference = (double) delayed[c][(size_t) i];
            auto gain = gains[c][(size_t) i];
            auto lowGain = lowGains[c][(size_t) i];
            double band, lowBand = 0.0;

            if (activeLinear)
            {
                // The raw signal and the gain wait for the crossover.
                auto& raw = rawLines[c];
                raw[(size_t) rawPosition] = (float) reference;
                reference = raw[(size_t) ((rawPosition + rawSize - crossoverDelay) % rawSize)];

                auto& gainLine = gainLines[c];
                gainLine[(size_t) gainPosition] = gain;
                gain = gainLine[(size_t) ((gainPosition + gainSize - crossoverDelay) % gainSize)];

                if (activeDual)
                {
                    // Five bands (four with a shelf): Low is band 1, High band 3.
                    auto& lowLine = lowGainLines[c];
                    lowLine[(size_t) gainPosition] = lowGain;
                    lowGain = lowLine[(size_t) ((gainPosition + gainSize - crossoverDelay) % gainSize)];

                    band = bandScratch[(size_t) ((3 * 2 + (int) c) * maxChunk + i)];
                    lowBand = bandScratch[(size_t) ((1 * 2 + (int) c) * maxChunk + i)];
                }
                else
                {
                    band = bandScratch[(size_t) ((1 * 2 + (int) c) * maxChunk + i)];
                    lowGain = 1.0;
                }
            }
            else if (activeShelf)
            {
                band = splits[c].tick (splitCoefficients[(size_t) i], reference).high;
            }
            else
            {
                // The bell widens as it cuts deeper (Q times the square root of the gain), so
                // Width marks where the cut is half its depth in dB, as an EQ's bell does.
                const auto warped = splitWarped[(size_t) i];
                fancy::dsp::SvfCoefficients bell;
                bell.k = 1.0 / (splitQ[(size_t) i] * std::sqrt (std::max (gain, 1.0e-6)));
                bell.a1 = 1.0 / (1.0 + warped * (warped + bell.k));
                bell.a2 = warped * bell.a1;
                bell.a3 = warped * bell.a2;
                band = bell.k * splits[c].tick (bell, reference).band;
            }

            // At a gain of exactly 1, (gain - 1) is exactly 0 and the output is the input.
            auto processed = settings.wide ? reference * gain : reference + (gain - 1.0) * band;

            // Phase 4 Wave 2: the Low band's cut, after the High band's (Split), or the deeper of
            // the two (Wide). Not run at all while the band is off.
            if (lowInChunk)
            {
                if (settings.wide)
                {
                    processed = reference * std::min (gain, lowGain);
                }
                else if (activeLinear)
                {
                    processed += (lowGain - 1.0) * lowBand;
                }
                else
                {
                    const auto warped = lowSplitWarped[(size_t) i];
                    fancy::dsp::SvfCoefficients bell;
                    bell.k = 1.0 / (lowSplitQ[(size_t) i] * std::sqrt (std::max (lowGain, 1.0e-6)));
                    bell.a1 = 1.0 / (1.0 + warped * (warped + bell.k));
                    bell.a2 = warped * bell.a1;
                    bell.a3 = warped * bell.a2;
                    lowBand = bell.k * lowSplits[c].tick (bell, processed).band;
                    processed += (lowGain - 1.0) * lowBand;
                }
            }
            references[c] = reference;
            outputs[c] = settings.listen ? reference - processed : processed;
        }

        if (activeLinear)
        {
            rawPosition = (rawPosition + 1) % rawSize;
            gainPosition = (gainPosition + 1) % gainSize;
        }

        const auto fade = switchGain.next();
        const auto out = outputGain.next();
        const auto bypass = bypassMix.next();

        // Written before it's read, so a latency of 0 reads this very sample.
        dryLines[0][(size_t) dryPosition] = left[i];
        dryLines[1][(size_t) dryPosition] = right != nullptr ? right[i] : left[i];

        const auto readPosition = (dryPosition + drySize - totalLatency) % drySize;
        const float dry[2] { dryLines[0][(size_t) readPosition], dryLines[1][(size_t) readPosition] };
        dryPosition = (dryPosition + 1) % drySize;

        const auto wetLeft = outputs[0] * out * fade;
        const auto wetRight = outputs[1] * out * fade;

        // Fully bypassed, the dry signal passes untouched.
        left[i] = bypass >= 1.0 ? dry[0] : (float) (wetLeft + ((double) dry[0] - wetLeft) * bypass);

        if (right != nullptr)
            right[i] = bypass >= 1.0 ? dry[1] : (float) (wetRight + ((double) dry[1] - wetRight) * bypass);

        if (holdOffSamples > 0)
            --holdOffSamples;
    }

    // The editor's meters.
    auto shown = reduction.load (std::memory_order_relaxed);
    const auto reductionDb = (float) chunkReduction;

    while (reductionDb > shown && ! reduction.compare_exchange_weak (shown, reductionDb, std::memory_order_relaxed))
    {
    }

    auto shownLow = lowReduction.load (std::memory_order_relaxed);
    const auto lowReductionDb = (float) chunkLowReduction;

    while (lowReductionDb > shownLow && ! lowReduction.compare_exchange_weak (shownLow, lowReductionDb, std::memory_order_relaxed))
    {
    }
}

void LegacyDeEsserW2::pushHistory (const HistoryColumn& column) noexcept
{
    const auto write = historyWrite.load (std::memory_order_relaxed);
    history[(size_t) write] = column;
    historyWrite.store ((write + 1) % historyCapacity, std::memory_order_release);
}

int LegacyDeEsserW2::popHistory (HistoryColumn* destination, int maxColumns) noexcept
{
    const auto write = historyWrite.load (std::memory_order_acquire);
    auto read = historyRead.load (std::memory_order_relaxed);
    auto count = 0;

    while (read != write && count < maxColumns)
    {
        destination[count++] = history[(size_t) read];
        read = (read + 1) % historyCapacity;
    }

    historyRead.store (read, std::memory_order_relaxed);
    return count;
}

} // namespace fancy::dsp
