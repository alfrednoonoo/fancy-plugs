#include "MultiCompDsp.h"

#include <algorithm>
#include <cmath>

using multicomp::CrossoverType;
using multicomp::maxBands;

namespace
{
constexpr double switchFadeMs = 2.0;
constexpr double switchSettleSeconds = 0.003;
constexpr double configDebounceSeconds = 0.05;
constexpr double parameterMs = 20.0;
constexpr double silenceFloor = 1.0e-30;
constexpr double maxAutoMakeupDb = 24.0;

double sanitise (float sample) noexcept
{
    return std::isfinite (sample) ? (double) sample : 0.0;
}
} // namespace

double MultiCompDsp::slopeFor (double ratio) noexcept
{
    return ratio >= infiniteRatio ? 1.0 : 1.0 - 1.0 / ratio;
}

double MultiCompDsp::autoMakeupDb (const BandSpec& band) noexcept
{
    if (band.ratio <= 1.0)
        return 0.0;

    return std::min (maxAutoMakeupDb, 0.5 * fancy::dsp::computeReduction (0.0, band.thresholdDb, slopeFor (band.ratio), band.kneeDb, false));
}

void MultiCompDsp::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    mainCrossover.prepare (sampleRate, true);
    keyCrossover.prepare (sampleRate, false);
    linearCrossover.prepare (sampleRate);
    limiter.prepare (sampleRate);

    bands.assign ((size_t) (maxBands * 2 * maxChunk), 0.0);
    keyBands.assign ((size_t) (maxBands * 2 * maxChunk), 0.0);

    for (auto& buffer : inputs)
        buffer.assign ((size_t) maxChunk, 0.0f);

    bypassLength = linearCrossover.getLatencySamples() + maxChunk + 1;

    for (int ch = 0; ch < 2; ++ch)
    {
        bypassDelay[ch].assign ((size_t) bypassLength, 0.0f);
        wetMix[ch].assign ((size_t) maxChunk, 0.0);
        dryMix[ch].assign ((size_t) maxChunk, 0.0);
    }

    for (auto& state : bandStates)
        for (auto* smoother : { &state.threshold, &state.slope, &state.knee, &state.gainDb, &state.rms, &state.activeMix, &state.soloGain })
            smoother->setTime (parameterMs, sampleRate);

    for (auto* smoother : { &wet, &dry, &output })
        smoother->setTime (parameterMs, sampleRate);

    bypass.setTime (10.0, sampleRate);
    limiterAmount.setTime (10.0, sampleRate);
    switchGain.setTime (switchFadeMs, sampleRate);

    designNeeded.store (true);

    resetCore();
}

void MultiCompDsp::resetCore() noexcept
{
    mainCrossover.reset();
    keyCrossover.reset();
    limiter.reset();

    for (auto& state : bandStates)
    {
        state.follower.reset();
        state.meanSquare[0] = state.meanSquare[1] = 0.0;
        state.previousOutput[0] = state.previousOutput[1] = 0.0;
    }

    for (auto& line : bypassDelay)
        std::fill (line.begin(), line.end(), 0.0f);

    bypassPosition = 0;
}

void MultiCompDsp::reset()
{
    resetCore();
}

MultiCompDsp::Config MultiCompDsp::configFor (const Targets& targets) const noexcept
{
    Config config;
    config.bands = std::clamp (targets.numBands, 1, maxBands);
    config.crossover = targets.crossover;
    config.limiter = targets.limitOutput;
    return config;
}

int MultiCompDsp::latencyOf (const Config& config) const noexcept
{
    return (config.crossover == CrossoverType::linear ? linearCrossover.getLatencySamples() : 0)
         + (config.limiter ? limiter.getLatencySamples() : 0);
}

void MultiCompDsp::pushCrossovers() noexcept
{
    const auto count = activeConfig.bands - 1;

    for (int i = 0; i < count; ++i)
        crossoverHz[(size_t) i] = current.bands[(size_t) i].topHz;

    multicomp::sanitiseCrossovers (crossoverHz.data(), count);

    mainCrossover.setFrequencies (crossoverHz.data(), count);
    keyCrossover.setFrequencies (crossoverHz.data(), count);
}

void MultiCompDsp::setTargets (const Targets& targets) noexcept
{
    current = targets;

    pushCrossovers();

    const auto anySolo = std::any_of (targets.bands.begin(), targets.bands.begin() + activeConfig.bands,
                                      [] (const BandSpec& band) { return band.solo; });

    for (int b = 0; b < maxBands; ++b)
    {
        const auto& band = targets.bands[(size_t) b];
        auto& state = bandStates[(size_t) b];

        state.threshold.setTarget (band.thresholdDb);
        state.slope.setTarget (slopeFor (band.ratio));
        state.knee.setTarget (band.kneeDb);
        state.gainDb.setTarget (band.gainDb + (band.autoMakeup ? autoMakeupDb (band) : 0.0));
        state.rms.setTarget (band.rmsMs);
        state.activeMix.setTarget (band.active ? 1.0 : 0.0);
        state.soloGain.setTarget (! anySolo || band.solo ? 1.0 : 0.0);

        if (band.attackMs != state.lastAttack || band.releaseMs != state.lastRelease
            || band.autoRelease != state.lastAuto || targets.attack90 != state.lastAttack90)
        {
            state.ballistics.set (band.attackMs, band.releaseMs, targets.attack90, band.autoRelease, false, sampleRate);

            state.lastAttack = band.attackMs;
            state.lastRelease = band.releaseMs;
            state.lastAuto = band.autoRelease;
            state.lastAttack90 = targets.attack90;
        }
    }

    wet.setTarget (targets.wetGain);
    dry.setTarget (targets.dryGain);
    output.setTarget (targets.outputGain);
    bypass.setTarget (targets.bypass ? 1.0 : 0.0);
    limiterAmount.setTarget (targets.limitOutput && ! targets.bypass ? 1.0 : 0.0);
}

void MultiCompDsp::applyConfig (const Config& config) noexcept
{
    activeConfig = config;
    activeLatency = latencyOf (config);

    mainCrossover.setType (config.crossover);
    keyCrossover.setType (config.crossover);
    mainCrossover.setBandCount (config.bands);
    keyCrossover.setBandCount (config.bands);

    resetCore();

    pushCrossovers();
    mainCrossover.snap();
    keyCrossover.snap();

    if (config.crossover == CrossoverType::linear)
    {
        linearCrossover.setBandCount (config.bands);
        designNeeded.store (true);
    }
}

void MultiCompDsp::snapToTargets (const Targets& targets) noexcept
{
    current = targets;

    wantedConfig = configFor (targets);
    wantedStableSamples = 0;
    holdSamples = 0;

    applyConfig (wantedConfig);
    setTargets (targets);

    if (wantedConfig.crossover == CrossoverType::linear)
        updateLinearDesign (targets);

    for (auto& state : bandStates)
    {
        for (auto* smoother : { &state.threshold, &state.slope, &state.knee, &state.gainDb, &state.rms, &state.activeMix, &state.soloGain })
            smoother->snapToTarget();

        state.rmsCoefficient = fancy::dsp::timeCoefficient (state.rms.getCurrent(), sampleRate);
    }

    for (auto* smoother : { &wet, &dry, &output, &bypass, &limiterAmount })
        smoother->snapToTarget();

    switchGain.snapTo (1.0);
}

bool MultiCompDsp::isDesignPending (const Targets& targets) const noexcept
{
    if (targets.crossover != CrossoverType::linear)
        return false;

    const auto count = std::clamp (targets.numBands, 1, maxBands);

    std::array<double, multicomp::maxCrossovers> hz {};

    for (int i = 0; i < count - 1; ++i)
        hz[(size_t) i] = targets.bands[(size_t) i].topHz;

    multicomp::sanitiseCrossovers (hz.data(), count - 1);

    return designNeeded.load (std::memory_order_acquire) || count != designedBands || hz != designedHz;
}

void MultiCompDsp::updateLinearDesign (const Targets& targets) noexcept
{
    if (targets.crossover != CrossoverType::linear)
        return;

    const auto count = std::clamp (targets.numBands, 1, maxBands);

    std::array<double, multicomp::maxCrossovers> hz {};

    for (int i = 0; i < count - 1; ++i)
        hz[(size_t) i] = targets.bands[(size_t) i].topHz;

    multicomp::sanitiseCrossovers (hz.data(), count - 1);

    const auto forced = designNeeded.exchange (false, std::memory_order_acq_rel);

    if (! forced && count == designedBands && hz == designedHz)
        return;

    linearCrossover.design (hz.data(), count);

    designedBands = count;
    designedHz = hz;
}

void MultiCompDsp::updateConfig (int numSamples) noexcept
{
    const auto wanted = configFor (current);

    if (wanted == wantedConfig)
    {
        wantedStableSamples += numSamples;
    }
    else
    {
        wantedConfig = wanted;
        wantedStableSamples = 0;
    }

    if (wantedConfig == activeConfig)
    {
        // A linear crossover has no output until its first filter arrives.
        const auto ready = activeConfig.crossover != CrossoverType::linear || linearCrossover.hasFilter();

        if (! ready)
            return;

        if (holdSamples > 0)
            holdSamples -= numSamples;
        else
            switchGain.setTarget (1.0);

        return;
    }

    if (wantedStableSamples < (int) (configDebounceSeconds * sampleRate))
        return;

    switchGain.setTarget (0.0);

    if (switchGain.isSettled())
    {
        applyConfig (wantedConfig);

        // Stay silent until the empty delays have refilled and the filters have settled.
        holdSamples = activeLatency + juce::roundToInt (switchSettleSeconds * sampleRate);
    }
}

bool MultiCompDsp::needsKey() const noexcept
{
    if (current.detector >= sideLR)
        return true;

    return std::any_of (current.bands.begin(), current.bands.begin() + activeConfig.bands,
                        [] (const BandSpec& band) { return band.sidechain; });
}

void MultiCompDsp::compressBand (int b, int numSamples, bool keyAvailable) noexcept
{
    const auto& spec = current.bands[(size_t) b];
    auto& state = bandStates[(size_t) b];

    auto* left = bandBuffer (bands, b, 0);
    auto* right = bandBuffer (bands, b, 1);

    const auto* keyLeft = bandBuffer (keyBands, b, 0);
    const auto* keyRight = bandBuffer (keyBands, b, 1);

    const auto listensToKey = current.detector >= sideLR || spec.sidechain;
    const auto channelMode = current.detector % 3; // both, left or right

    auto inputPeak = 0.0;
    auto peakReduction = 0.0;

    auto* wetLeft = wetMix[0].data();
    auto* wetRight = wetMix[1].data();
    auto* dryLeft = dryMix[0].data();
    auto* dryRight = dryMix[1].data();

    for (int i = 0; i < numSamples; ++i)
    {
        const auto thresholdDb = state.threshold.next();
        const auto slope = state.slope.next();
        const auto kneeDb = state.knee.next();
        const auto gainDb = state.gainDb.next();
        const auto activeMix = state.activeMix.next();
        const auto soloGain = state.soloGain.next();

        if (! state.rms.isSettled())
            state.rmsCoefficient = fancy::dsp::timeCoefficient (state.rms.next(), sampleRate);

        const auto inLeft = left[i], inRight = right[i];

        inputPeak = std::max ({ inputPeak, std::abs (inLeft), std::abs (inRight) });

        double sourceLeft, sourceRight;

        if (listensToKey)
        {
            sourceLeft = keyAvailable ? keyLeft[i] : 0.0;
            sourceRight = keyAvailable ? keyRight[i] : 0.0;
        }
        else if (spec.feedback)
        {
            sourceLeft = state.previousOutput[0];
            sourceRight = state.previousOutput[1];
        }
        else
        {
            sourceLeft = inLeft;
            sourceRight = inRight;
        }

        const auto detectorLeft = channelMode == 2 ? sourceRight : sourceLeft;
        const auto detectorRight = channelMode == 1 ? sourceLeft : sourceRight;

        state.meanSquare[0] += state.rmsCoefficient * (detectorLeft * detectorLeft - state.meanSquare[0]);
        state.meanSquare[1] += state.rmsCoefficient * (detectorRight * detectorRight - state.meanSquare[1]);

        // The louder channel sets the gain, so the band's stereo image holds.
        const auto level = 10.0 * std::log10 (std::max ({ state.meanSquare[0], state.meanSquare[1], silenceFloor }));

        // Below 1:1 the slope is negative. The follower works on the size of the change, so
        // that attack still means the change starting.
        const auto target = fancy::dsp::computeReduction (level, thresholdDb, std::abs (slope), kneeDb, false);
        const auto change = std::min (state.follower.follow (state.ballistics, target), maxExpansionDb * 2.0);
        const auto reduction = slope < 0.0 ? -std::min (change, maxExpansionDb) : change;

        const auto totalDb = gainDb - reduction;
        const auto gain = totalDb == 0.0 ? 1.0 : std::pow (10.0, totalDb / 20.0);

        const auto compressedLeft = inLeft * gain;
        const auto compressedRight = inRight * gain;

        state.previousOutput[0] = compressedLeft;
        state.previousOutput[1] = compressedRight;

        peakReduction = std::max (peakReduction, reduction * activeMix);

        wetLeft[i] += (inLeft + (compressedLeft - inLeft) * activeMix) * soloGain;
        wetRight[i] += (inRight + (compressedRight - inRight) * activeMix) * soloGain;

        dryLeft[i] += inLeft;
        dryRight[i] += inRight;
    }

    state.input.report (0, (float) inputPeak);
    state.input.report (1, (float) inputPeak);

    auto stored = state.gainReduction.load (std::memory_order_relaxed);
    const auto newPeak = (float) peakReduction;

    while (newPeak > stored && ! state.gainReduction.compare_exchange_weak (stored, newPeak, std::memory_order_relaxed))
    {
    }
}

void MultiCompDsp::process (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int numSamples) noexcept
{
    for (int done = 0; done < numSamples;)
    {
        const auto chunk = std::min (numSamples - done, maxChunk);
        processChunk (main, numMain, sidechain, numSidechain, done, chunk);
        done += chunk;
    }
}

void MultiCompDsp::processChunk (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int offset, int numSamples) noexcept
{
    updateConfig (numSamples);

    if (synchronousDesign)
        updateLinearDesign (current);

    const auto keyAvailable = numSidechain > 0 && needsKey();

    for (int i = 0; i < numSamples; ++i)
    {
        const auto writePosition = (bypassPosition + i) % bypassLength;

        for (int ch = 0; ch < 2; ++ch)
        {
            const auto in = sanitise (main[std::min (ch, numMain - 1)][offset + i]);

            inputs[ch][(size_t) i] = (float) in;
            bypassDelay[ch][(size_t) writePosition] = (float) in;

            if (keyAvailable)
                inputs[ch + 2][(size_t) i] = (float) sanitise (sidechain[std::min (ch, numSidechain - 1)][offset + i]);
        }
    }

    if (activeConfig.crossover == CrossoverType::linear)
    {
        linearCrossover.process (inputs[0].data(), inputs[1].data(), inputs[2].data(), inputs[3].data(),
                                 bands.data(), keyAvailable ? keyBands.data() : nullptr, maxChunk, numSamples);
    }
    else
    {
        mainCrossover.process (inputs[0].data(), inputs[1].data(), bands.data(), maxChunk, numSamples);

        if (keyAvailable)
            keyCrossover.process (inputs[2].data(), inputs[3].data(), keyBands.data(), maxChunk, numSamples);
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        std::fill_n (wetMix[ch].begin(), numSamples, 0.0);
        std::fill_n (dryMix[ch].begin(), numSamples, 0.0);
    }

    for (int b = 0; b < activeConfig.bands; ++b)
        compressBand (b, numSamples, keyAvailable);

    const auto crossoverLatency = activeConfig.crossover == CrossoverType::linear ? linearCrossover.getLatencySamples() : 0;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto wetGain = wet.next();
        const auto dryGain = dry.next();
        const auto outputGain = output.next();
        const auto bypassAmount = bypass.next();
        const auto limit = limiterAmount.next();
        const auto fade = switchGain.next();
        const auto readPosition = (bypassPosition + i + bypassLength - crossoverLatency) % bypassLength;

        double out[2];

        for (int ch = 0; ch < 2; ++ch)
        {
            const auto delayed = (double) bypassDelay[ch][(size_t) readPosition];
            const auto mixed = (wetMix[ch][(size_t) i] * wetGain + dryMix[ch][(size_t) i] * dryGain) * outputGain;

            out[ch] = bypassAmount >= 1.0 ? delayed : mixed + (delayed - mixed) * bypassAmount;
        }

        if (activeConfig.limiter)
            limiter.process (out[0], out[1], limit);

        for (int ch = 0; ch < numMain; ++ch)
            main[ch][offset + i] = (float) (out[ch] * fade);
    }

    bypassPosition = (bypassPosition + numSamples) % bypassLength;
}
