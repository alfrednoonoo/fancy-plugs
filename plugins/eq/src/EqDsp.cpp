#include "EqDsp.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr int maxChunk = 512;
constexpr double parameterSmoothingMs = 20.0;
constexpr double mixFadeMs = 10.0;
constexpr double switchFadeMs = 2.0;
constexpr double configDebounceSeconds = 0.05;
constexpr double switchSettleSeconds = 0.02;

// Dynamic bands (Phase 2.1 D).
constexpr double levelWindowSeconds = 0.005;
constexpr double kneeDb = 6.0;
constexpr double fullRangeDb = 12.0;

// How far the level is past Threshold, with a soft knee: 0 below Threshold - 3 dB.
double distancePast (double distanceDb) noexcept
{
    constexpr auto half = kneeDb * 0.5;

    if (distanceDb <= -half)
        return 0.0;

    if (distanceDb < half)
        return (distanceDb + half) * (distanceDb + half) / (2.0 * kneeDb);

    return distanceDb;
}
} // namespace

void EqDsp::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    for (int stages = 1; stages <= numOversamplers; ++stages)
    {
        auto oversampler = std::make_unique<Oversampler> (2);
        oversampler->setUsingIntegerLatency (true);

        for (int stage = 0; stage < stages; ++stage)
        {
            const auto transition = stage == 0 ? 0.06f : 0.12f;
            const auto stopband = -100.0f + 10.0f * (float) stage;

            oversampler->addOversamplingStage (Oversampler::filterHalfBandPolyphaseIIR, transition, stopband, transition, stopband);
        }

        oversampler->initProcessing ((size_t) maxChunk);

        oversamplerLatencies[(size_t) (stages - 1)] = juce::roundToInt (oversampler->getLatencyInSamples());
        oversamplers[(size_t) (stages - 1)] = std::move (oversampler);
    }

    linear.prepare (sampleRate);

    const auto maxLatency = std::max ({ linear.getMaxLatencySamples(), oversamplerLatencies[0], oversamplerLatencies[1] });

    dryLength = maxLatency + maxChunk + 1;

    for (auto& delay : dryDelay)
        delay.assign ((size_t) dryLength, 0.0f);

    dryBuffer.setSize (maxChannels, maxChunk);
    oversamplingBuffer.setSize (maxChannels, maxChunk);

    levelCoefficient = std::exp (-1.0 / (levelWindowSeconds * sampleRate));

    for (auto& detector : detectors)
        detector.designed = false;

    gain.setTime (parameterSmoothingMs, sampleRate);
    output.setTime (parameterSmoothingMs, sampleRate);
    bypass.setTime (10.0, sampleRate);
    switchGain.setTime (switchFadeMs, sampleRate);

    designedLength = -1;

    reset();
}

void EqDsp::reset()
{
    for (auto& band : bands)
        band.state = {};

    for (size_t b = 0; b < detectors.size(); ++b)
    {
        auto& detector = detectors[b];
        detector.state = {};
        detector.meanSquare = {};
        detector.change = 0.0;
        liveChange[b].store (0.0f, std::memory_order_relaxed);
    }

    for (auto& oversampler : oversamplers)
        if (oversampler != nullptr)
            oversampler->reset();

    linear.clear();

    for (auto& delay : dryDelay)
        std::fill (delay.begin(), delay.end(), 0.0f);

    dryPosition = 0;
}

EqDsp::Config EqDsp::configFor (const Targets& targets) const noexcept
{
    Config config;
    config.linearPhase = targets.linearPhase;
    config.firLength = targets.firLength;
    config.oversampling = targets.linearPhase ? 0 : targets.oversampling;
    return config;
}

int EqDsp::latencyOf (const Config& config) const noexcept
{
    if (config.linearPhase)
        return linear.getLatencySamples (config.firLength);

    return config.oversampling > 0 ? oversamplerLatencies[(size_t) (config.oversampling - 1)] : 0;
}

void EqDsp::setCoefficients (Band& band, const eq::BandSpec& spec, bool ramp) noexcept
{
    const auto filter = eq::design (spec, processRate);

    band.numSections = filter.numSections;
    band.moving = ramp;

    for (int s = 0; s < filter.numSections; ++s)
    {
        const auto& section = filter.sections[(size_t) s];
        auto& c = band.coefficients[(size_t) s];
        auto& d = band.increments[(size_t) s];

        if (ramp)
        {
            const auto scale = 1.0 / (double) stepLength;

            d = { (section.b0 - c.b0) * scale, (section.b1 - c.b1) * scale, (section.b2 - c.b2) * scale,
                  (section.a1 - c.a1) * scale, (section.a2 - c.a2) * scale };
        }
        else
        {
            c = { section.b0, section.b1, section.b2, section.a1, section.a2 };
        }
    }
}

void EqDsp::snapBand (Band& band, const eq::BandSpec& spec) noexcept
{
    band.active = spec;
    band.logFrequency.snapTo (std::log (spec.frequencyHz));
    band.gainDb.snapTo (spec.gainDb);
    band.q.snapTo (std::log (spec.q));
    band.state = {};
    band.appliedChange = 0.0;

    setCoefficients (band, spec, false);
}

bool EqDsp::isDynamic (int band) const noexcept
{
    const auto& spec = current.bands[(size_t) band];
    return current.dynamics[(size_t) band].on && spec.on && eq::supportsDynamics (spec.type) && ! activeConfig.linearPhase;
}

eq::BandSpec EqDsp::effectiveSpec (const eq::BandSpec& spec, bool dynamic) noexcept
{
    // A dynamic notch is a bell whose only gain is the change: it passes until it acts.
    if (! dynamic || spec.type != eq::BandType::notch)
        return spec;

    auto bell = spec;
    bell.type = eq::BandType::band;
    bell.gainDb = 0.0;
    return bell;
}

void EqDsp::runDetectors (const float* const* main, int numMain, const float* const* sidechain, int numSidechain, int numSamples) noexcept
{
    chunkLength = numSamples;

    for (int b = 0; b < eq::maxBands; ++b)
    {
        auto& detector = detectors[(size_t) b];
        auto& changes = chunkChange[(size_t) b];
        const auto dynamic = isDynamic (b);

        // A band that isn't dynamic, and has let go, does nothing here.
        if (! dynamic && detector.change == 0.0)
        {
            std::fill_n (changes.begin(), numSamples, 0.0);
            continue;
        }

        const auto& spec = current.bands[(size_t) b];
        const auto& settings = current.dynamics[(size_t) b];

        // What the band hears: a band pass for a bell or notch, a low or high pass for a shelf.
        eq::BandSpec shape;
        shape.on = true;
        shape.frequencyHz = spec.frequencyHz;
        shape.order = 2;

        switch (spec.type)
        {
            case eq::BandType::lowShelf:  shape.type = eq::BandType::lowPass;  shape.q = eq::butterworthQ; break;
            case eq::BandType::highShelf:
            case eq::BandType::tilt:      shape.type = eq::BandType::highPass; shape.q = eq::butterworthQ; break;
            default:                      shape.type = eq::BandType::bandPass; shape.q = spec.q; break;
        }

        if (! detector.designed || ! (shape == detector.shape))
        {
            detector.filter = eq::design (shape, sampleRate);
            detector.shape = shape;

            // A band pass is used twice, 4th order: at Q 1, a tone two octaves away is then
            // about 24 dB down rather than 12, so it doesn't move the band.
            if (shape.type == eq::BandType::bandPass && detector.filter.numSections == 1)
            {
                detector.filter.sections[1] = detector.filter.sections[0];
                detector.filter.numSections = 2;
            }
            detector.designed = true;
        }

        const auto useSidechain = settings.sidechain && numSidechain > 0;
        const auto* const* source = useSidechain ? sidechain : main;
        const auto numSource = std::min (useSidechain ? numSidechain : numMain, (int) maxChannels);

        const auto attack = std::exp (-1.0 / (std::max (0.01, settings.attackMs) * 0.001 * sampleRate));
        const auto release = std::exp (-1.0 / (std::max (0.01, settings.releaseMs) * 0.001 * sampleRate));
        const auto notch = spec.type == eq::BandType::notch;

        for (int i = 0; i < numSamples; ++i)
        {
            auto loudest = 0.0;

            for (int ch = 0; ch < numSource; ++ch)
            {
                auto x = (double) source[ch][i];

                for (int s = 0; s < detector.filter.numSections; ++s)
                {
                    const auto& c = detector.filter.sections[(size_t) s];
                    auto& z = detector.state[(size_t) ch][(size_t) s];
                    const auto y = c.b0 * x + z[0];
                    z[0] = c.b1 * x - c.a1 * y + z[1];
                    z[1] = c.b2 * x - c.a2 * y;
                    x = y;
                }

                auto& meanSquare = detector.meanSquare[(size_t) ch];
                meanSquare = x * x + levelCoefficient * (meanSquare - x * x);
                loudest = std::max (loudest, meanSquare);
            }

            auto target = 0.0;

            if (dynamic)
            {
                const auto levelDb = 10.0 * std::log10 (loudest + 1.0e-30);
                const auto distance = settings.below ? settings.thresholdDb - levelDb : levelDb - settings.thresholdDb;
                target = settings.rangeDb * std::min (1.0, distancePast (distance) / fullRangeDb);

                if (notch)
                    target = std::min (0.0, target);
            }

            auto& change = detector.change;
            const auto coefficient = std::abs (target) > std::abs (change) ? attack : release;
            change = target + coefficient * (change - target);

            // Letting go ends exactly at 0, so a quiet band is exactly its static self.
            if (target == 0.0 && std::abs (change) < 1.0e-6)
                change = 0.0;

            changes[(size_t) i] = change;
        }

        for (auto& channelState : detector.state)
            for (auto& z : channelState)
                for (auto& value : z)
                    if (! std::isfinite (value))
                        value = 0.0;
    }

    for (int b = 0; b < eq::maxBands; ++b)
        liveChange[(size_t) b].store ((float) detectors[(size_t) b].change, std::memory_order_relaxed);
}

void EqDsp::applyConfig (const Config& config)
{
    activeConfig = config;
    activeLatency.store (latencyOf (config), std::memory_order_relaxed);

    processRate = sampleRate * (double) (1 << config.oversampling);

    const auto stepRate = processRate / (double) stepLength;

    for (int b = 0; b < eq::maxBands; ++b)
    {
        auto& band = bands[(size_t) b];

        band.logFrequency.setTime (parameterSmoothingMs, stepRate);
        band.gainDb.setTime (parameterSmoothingMs, stepRate);
        band.q.setTime (parameterSmoothingMs, stepRate);
        band.mixStep = 1.0 / std::max (1.0, mixFadeMs * 0.001 * processRate);

        const auto& spec = current.bands[(size_t) b];
        snapBand (band, spec);
        band.mix = band.mixTarget = spec.on ? 1.0 : 0.0;
    }

    stepPosition = 0;

    for (auto& oversampler : oversamplers)
        oversampler->reset();

    if (config.linearPhase)
    {
        linear.setLength (config.firLength);
        designRequested.store (true);
    }

    for (auto& delay : dryDelay)
        std::fill (delay.begin(), delay.end(), 0.0f);

    dryPosition = 0;
}

void EqDsp::snapToTargets (const Targets& targets)
{
    current = targets;

    wantedConfig = configFor (targets);
    wantedStableSamples = 0;
    holdSamples = 0;

    applyConfig (wantedConfig);

    updateLinearDesign (targets);

    gain.snapTo (targets.gain);
    output.snapTo (targets.outputGain);
    bypass.snapTo (targets.bypass ? 1.0 : 0.0);
    switchGain.snapTo (1.0);
}

bool EqDsp::isDesignPending (const Targets& targets) noexcept
{
    const juce::SpinLock::ScopedLockType lock (designLock);

    return targets.linearPhase
        && (designRequested.load() || designedLength != targets.firLength || designedBands != targets.bands);
}

void EqDsp::updateLinearDesign (const Targets& targets) noexcept
{
    const juce::SpinLock::ScopedLockType lock (designLock);

    if (! targets.linearPhase)
    {
        designedLength = -1;
        return;
    }

    const auto requested = designRequested.exchange (false);

    if (! requested && designedLength == targets.firLength && designedBands == targets.bands)
        return;

    linear.design (targets.bands, targets.firLength);

    designedBands = targets.bands;
    designedLength = targets.firLength;
}

void EqDsp::updateConfig (int numSamples) noexcept
{
    const auto wanted = configFor (current);

    if (wanted == wantedConfig)
    {
        wantedStableSamples = std::min (wantedStableSamples + numSamples, 1 << 30);
    }
    else
    {
        wantedConfig = wanted;
        wantedStableSamples = 0;
    }

    if (wantedConfig == activeConfig)
    {
        if (holdSamples > 0)
            holdSamples -= numSamples;
        else if (! activeConfig.linearPhase || linear.hasFilter())
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
        holdSamples = activeLatency.load (std::memory_order_relaxed) + juce::roundToInt (switchSettleSeconds * sampleRate);
    }
}

void EqDsp::updateBands (int baseIndex) noexcept
{
    for (int b = 0; b < eq::maxBands; ++b)
    {
        auto& band = bands[(size_t) b];
        const auto dynamic = isDynamic (b);
        const auto wanted = effectiveSpec (current.bands[(size_t) b], dynamic);

        // The detector's change at this point of the chunk, at the host rate.
        const auto change = chunkLength > 0 ? chunkChange[(size_t) b][(size_t) std::clamp (baseIndex, 0, chunkLength - 1)] : 0.0;

        if (! wanted.sameShape (band.active))
        {
            // A change of shape can't be smoothed, so it's made while the band is out of the path.
            // The coefficients hold still meanwhile: a ramp left running would overshoot its
            // target and leave the stable region.
            band.mixTarget = 0.0;
            band.moving = false;

            if (band.mix <= 0.0)
                snapBand (band, wanted);

            continue;
        }

        if (! band.active.on)
        {
            band.moving = false;
            continue;
        }

        band.mixTarget = 1.0;

        const auto logFrequency = std::log (wanted.frequencyHz);
        const auto logQ = std::log (wanted.q);

        band.logFrequency.setTarget (logFrequency);
        band.gainDb.setTarget (wanted.gainDb);
        band.q.setTarget (logQ);

        if (band.logFrequency.isSettled() && band.gainDb.isSettled() && band.q.isSettled() && change == band.appliedChange)
        {
            if (band.moving)
            {
                band.active = wanted;
                band.active.gainDb += change;
                setCoefficients (band, band.active, false);
            }

            continue;
        }

        auto spec = wanted;
        spec.frequencyHz = std::exp (band.logFrequency.next());
        spec.gainDb = band.gainDb.next() + change;
        spec.q = std::exp (band.q.next());

        band.appliedChange = change;
        band.active = spec;
        setCoefficients (band, spec, true);
    }
}

void EqDsp::runBand (Band& band, int numChannels, int numSamples) noexcept
{
    const auto mixing = band.mix < 1.0 || band.mixTarget < 1.0;

    if (mixing)
        for (int ch = 0; ch < numChannels; ++ch)
            std::copy_n (work[ch], numSamples, unfiltered[ch]);

    for (int s = 0; s < band.numSections; ++s)
    {
        const auto start = band.coefficients[(size_t) s];
        const auto& d = band.increments[(size_t) s];

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& state = band.state[(size_t) ch][(size_t) s];
            auto z1 = state[0], z2 = state[1];
            auto* data = work[ch];

            if (band.moving)
            {
                auto c = start;

                for (int i = 0; i < numSamples; ++i)
                {
                    const auto in = data[i];
                    const auto out = c.b0 * in + z1;

                    z1 = c.b1 * in - c.a1 * out + z2;
                    z2 = c.b2 * in - c.a2 * out;
                    data[i] = out;

                    c.b0 += d.b0;
                    c.b1 += d.b1;
                    c.b2 += d.b2;
                    c.a1 += d.a1;
                    c.a2 += d.a2;
                }

                if (ch == numChannels - 1)
                    band.coefficients[(size_t) s] = c;
            }
            else
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    const auto in = data[i];
                    const auto out = start.b0 * in + z1;

                    z1 = start.b1 * in - start.a1 * out + z2;
                    z2 = start.b2 * in - start.a2 * out;
                    data[i] = out;
                }
            }

            state[0] = std::isfinite (z1) ? z1 : 0.0;
            state[1] = std::isfinite (z2) ? z2 : 0.0;
        }
    }

    if (! mixing)
        return;

    const auto increment = band.mixTarget > band.mix ? band.mixStep : -band.mixStep;
    auto mix = band.mix;

    for (int i = 0; i < numSamples; ++i)
    {
        mix = std::clamp (mix + increment, 0.0, 1.0);

        for (int ch = 0; ch < numChannels; ++ch)
            work[ch][i] = unfiltered[ch][i] + mix * (work[ch][i] - unfiltered[ch][i]);
    }

    band.mix = mix;
}

void EqDsp::filterChunk (float* const* channels, int numChannels, int offset, int numSamples) noexcept
{
    if (stepPosition == 0)
        updateBands (offset >> activeConfig.oversampling);

    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < numSamples; ++i)
            work[ch][i] = (double) channels[ch][offset + i];

    for (auto& band : bands)
        if (band.numSections > 0 && (band.mix > 0.0 || band.mixTarget > 0.0))
            runBand (band, numChannels, numSamples);

    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < numSamples; ++i)
            channels[ch][offset + i] = (float) work[ch][i];

    stepPosition = (stepPosition + numSamples) % stepLength;
}

void EqDsp::filterBlock (float* const* channels, int numChannels, int numSamples) noexcept
{
    for (int done = 0; done < numSamples;)
    {
        const auto chunk = std::min (numSamples - done, stepLength - stepPosition);
        filterChunk (channels, numChannels, done, chunk);
        done += chunk;
    }
}

void EqDsp::processChunk (float* const* channels, int numChannels, int offset, int numSamples,
                          const float* const* sidechain, int numSidechain) noexcept
{
    updateConfig (numSamples);

    if (activeConfig.linearPhase && synchronousDesign.load (std::memory_order_relaxed))
        updateLinearDesign (current);

    const auto delay = activeLatency.load (std::memory_order_relaxed);

    float* signal[maxChannels] {};

    for (int ch = 0; ch < numChannels; ++ch)
    {
        signal[ch] = channels[ch] + offset;

        auto& ring = dryDelay[(size_t) ch];
        auto* dry = dryBuffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            auto sample = signal[ch][i];

            if (! std::isfinite (sample))
                sample = signal[ch][i] = 0.0f;

            const auto writePosition = (dryPosition + i) % dryLength;

            ring[(size_t) writePosition] = sample;
            dry[i] = ring[(size_t) ((writePosition - delay + dryLength) % dryLength)];
        }
    }

    dryPosition = (dryPosition + numSamples) % dryLength;

    // The dynamic bands' detectors hear the input before the EQ, or the sidechain.
    {
        const float* sidechainChunk[maxChannels] {};
        const auto sidechainChannels = std::min (numSidechain, (int) maxChannels);

        for (int ch = 0; ch < sidechainChannels; ++ch)
            sidechainChunk[ch] = sidechain[ch] + offset;

        runDetectors (signal, numChannels, sidechainChunk, sidechain != nullptr ? sidechainChannels : 0, numSamples);
    }

    if (activeConfig.linearPhase)
    {
        linear.process (signal, numChannels, numSamples);
    }
    else if (activeConfig.oversampling > 0)
    {
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            if (ch < numChannels)
                oversamplingBuffer.copyFrom (ch, 0, signal[ch], numSamples);
            else
                oversamplingBuffer.clear (ch, 0, numSamples);
        }

        auto& oversampler = *oversamplers[(size_t) (activeConfig.oversampling - 1)];

        juce::dsp::AudioBlock<float> block (oversamplingBuffer.getArrayOfWritePointers(), (size_t) maxChannels, (size_t) numSamples);

        const juce::dsp::AudioBlock<const float> constBlock (block);
        auto up = oversampler.processSamplesUp (constBlock);

        float* upChannels[maxChannels] { up.getChannelPointer (0), up.getChannelPointer (1) };
        filterBlock (upChannels, numChannels, (int) up.getNumSamples());

        oversampler.processSamplesDown (block);

        for (int ch = 0; ch < numChannels; ++ch)
            std::copy_n (oversamplingBuffer.getReadPointer (ch), numSamples, signal[ch]);
    }
    else
    {
        filterBlock (signal, numChannels, numSamples);
    }

    gain.setTarget (current.gain);
    output.setTarget (current.outputGain);
    bypass.setTarget (current.bypass ? 1.0 : 0.0);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto wetGain = gain.next() * output.next();
        const auto bypassAmount = bypass.next();
        const auto fade = switchGain.next();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto wet = (double) signal[ch][i] * wetGain;
            const auto dry = (double) dryBuffer.getReadPointer (ch)[i];

            signal[ch][i] = (float) ((wet + bypassAmount * (dry - wet)) * fade);
        }
    }
}

void EqDsp::process (float* const* channels, int numChannels, int numSamples,
                     const float* const* sidechain, int numSidechain) noexcept
{
    numChannels = std::min (numChannels, (int) maxChannels);

    for (int done = 0; done < numSamples;)
    {
        const auto chunk = std::min (numSamples - done, maxChunk);
        processChunk (channels, numChannels, done, chunk, sidechain, numSidechain);
        done += chunk;
    }
}
