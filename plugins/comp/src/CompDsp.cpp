#include "CompDsp.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double butterworthQ = 0.70710678118654752;
constexpr double switchFadeMs = 2.0;
constexpr double switchSettleSeconds = 0.003;
constexpr double configDebounceSeconds = 0.05;
constexpr double highPassOffHz = 20.5;
constexpr double lowPassOffHz = 23900.0;
constexpr double minDetectorAverageMs = 0.1;

using fancy::dsp::timeCoefficient;

double slopeFor (double ratio) noexcept
{
    return ratio >= CompDsp::infiniteRatio ? 1.0 : 1.0 - 1.0 / ratio;
}

double sanitise (float sample) noexcept
{
    return std::isfinite (sample) ? (double) sample : 0.0;
}
} // namespace

CompDsp::CompDsp() = default;
CompDsp::~CompDsp() = default;

double CompDsp::computeReduction (double levelDb, double thresholdDb, double slope, double kneeDb, bool weird) noexcept
{
    return fancy::dsp::computeReduction (levelDb, thresholdDb, slope, kneeDb, weird);
}

CompDsp::Oversampler* CompDsp::oversamplerFor (const Config& config, bool sidechainPath) const noexcept
{
    if (config.oversampling == off)
        return nullptr;

    const auto index = (size_t) ((config.oversampling - 1) * 2 + (config.minimumPhase ? 1 : 0));
    return sidechainPath ? keyOversamplers[index].get() : mainOversamplers[index].get();
}

int CompDsp::factorOf (const Config& config) const noexcept
{
    return 1 << config.oversampling;
}

int CompDsp::latencyOf (const Config& config) const noexcept
{
    auto latency = config.preCompSamples;

    if (config.oversampling != off)
        latency += oversamplerLatencies[(size_t) ((config.oversampling - 1) * 2 + (config.minimumPhase ? 1 : 0))];

    if (config.limiter)
        latency += limiter.getLatencySamples();

    return latency;
}

CompDsp::Config CompDsp::configFor (const Targets& targets) const noexcept
{
    Config config;
    config.oversampling = targets.oversampling;
    config.minimumPhase = targets.minimumPhase;
    config.limiter = targets.limitOutput;
    config.preCompSamples = (int) std::lround (targets.preCompMs * 0.001 * sampleRate);
    return config;
}

void CompDsp::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    const auto build = [] (int stages, bool minimumPhase)
    {
        auto oversampler = std::make_unique<Oversampler> (2);
        oversampler->setUsingIntegerLatency (true);

        for (int stage = 0; stage < stages; ++stage)
        {
            const auto transition = stage == 0 ? 0.06f : 0.12f;
            const auto stopband = -100.0f + 10.0f * (float) stage;

            oversampler->addOversamplingStage (minimumPhase ? Oversampler::filterHalfBandPolyphaseIIR
                                                            : Oversampler::filterHalfBandFIREquiripple,
                                               transition, stopband, transition, stopband);
        }

        oversampler->initProcessing ((size_t) maxChunk);
        return oversampler;
    };

    int maxOversamplerLatency = 0;

    for (int stages = 1; stages <= 2; ++stages)
    {
        for (int phase = 0; phase < 2; ++phase)
        {
            const auto index = (size_t) ((stages - 1) * 2 + phase);

            mainOversamplers[index] = build (stages, phase == 1);
            keyOversamplers[index] = build (stages, phase == 1);
            oversamplerLatencies[index] = juce::roundToInt (mainOversamplers[index]->getLatencyInSamples());
            maxOversamplerLatency = std::max (maxOversamplerLatency, oversamplerLatencies[index]);
        }
    }

    limiter.prepare (sampleRate);

    mainBuffer.setSize (2, maxChunk);
    keyBuffer.setSize (2, maxChunk);

    const auto maxPreComp = (int) std::ceil (maxPreCompMs * 0.001 * sampleRate);

    dryLength = maxPreComp + maxOversamplerLatency + maxChunk + 1;
    preCompLength = maxPreComp * maxFactor + maxChunk * maxFactor + 1;

    for (auto& state : channels)
    {
        state.dryDelay.assign ((size_t) dryLength, 0.0f);
        state.preCompDelay.assign ((size_t) preCompLength, 0.0f);
    }

    wet.setTime (20.0, sampleRate);
    dry.setTime (20.0, sampleRate);
    output.setTime (20.0, sampleRate);
    bypass.setTime (10.0, sampleRate);
    limiterAmount.setTime (10.0, sampleRate);
    switchGain.setTime (switchFadeMs, sampleRate);

    reset();
}

void CompDsp::resetCore()
{
    for (auto& oversampler : mainOversamplers)
        if (oversampler != nullptr)
            oversampler->reset();

    for (auto& oversampler : keyOversamplers)
        if (oversampler != nullptr)
            oversampler->reset();

    for (auto& state : channels)
    {
        state.detector.reset();
        state.follower.reset();
        state.previousOutput = 0.0;
        std::fill (state.dryDelay.begin(), state.dryDelay.end(), 0.0f);
        std::fill (state.preCompDelay.begin(), state.preCompDelay.end(), 0.0f);
    }

    limiter.reset();
    dryPosition = 0;
    preCompPosition = 0;
}

void CompDsp::reset()
{
    resetCore();
}

void CompDsp::updateBallistics() noexcept
{
    ballistics.set (current.attackMs, current.releaseMs, current.attack90, current.autoRelease, current.linearRelease, internalRate);
}

void CompDsp::setTargets (const Targets& targets)
{
    current = targets;

    threshold.setTarget (targets.thresholdDb);
    slope.setTarget (slopeFor (targets.ratio));
    knee.setTarget (targets.kneeDb);
    makeup.setTarget (targets.autoMakeup ? 0.5 * slopeFor (targets.ratio) * -targets.thresholdDb : 0.0);
    rms.setTarget (targets.rmsMs);
    previewAmount.setTarget (targets.previewFilter ? 1.0 : 0.0);

    const auto limit = 0.45 * internalRate;

    logLowPass.setTarget (std::log (std::min (targets.lowpassHz, limit)));
    logHighPass.setTarget (std::log (std::min (targets.highpassHz, limit)));
    lowPassAmount.setTarget (targets.lowpassHz >= lowPassOffHz ? 0.0 : 1.0);
    highPassAmount.setTarget (targets.highpassHz <= highPassOffHz ? 0.0 : 1.0);

    wet.setTarget (targets.wetGain);
    dry.setTarget (targets.dryGain);
    output.setTarget (targets.outputGain);
    bypass.setTarget (targets.bypass ? 1.0 : 0.0);
    limiterAmount.setTarget (targets.limitOutput && ! targets.bypass ? 1.0 : 0.0);

    updateBallistics();
}

void CompDsp::applyConfig (const Config& config)
{
    activeConfig = config;
    activeLatency = latencyOf (config);
    internalRate = sampleRate * (double) factorOf (config);

    for (auto* smoother : { &threshold, &slope, &knee, &makeup, &rms, &previewAmount, &lowPassAmount, &highPassAmount, &logLowPass, &logHighPass })
        smoother->setTime (20.0, internalRate);

    // At high frequencies an instantaneous level swings the envelope follower through a sharp
    // sawtooth on every half cycle. Its harmonics pass the oversampled Nyquist and fold into
    // the gain, where no later filter can reach them. A short average keeps the level steady.
    maxDetectorCoefficient = config.oversampling == off ? 1.0 : timeCoefficient (minDetectorAverageMs, internalRate);

    resetCore();
    setTargets (current);

    lowPassCoefficients.set (std::exp (logLowPass.getCurrent()), butterworthQ, internalRate);
    highPassCoefficients.set (std::exp (logHighPass.getCurrent()), butterworthQ, internalRate);
    rmsCoefficient = timeCoefficient (rms.getCurrent(), internalRate);
}

void CompDsp::snapToTargets (const Targets& targets)
{
    current = targets;

    wantedConfig = configFor (targets);
    wantedStableSamples = 0;
    holdSamples = 0;

    applyConfig (wantedConfig);

    for (auto* smoother : { &threshold, &slope, &knee, &makeup, &rms, &previewAmount, &lowPassAmount, &highPassAmount, &logLowPass, &logHighPass,
                            &wet, &dry, &output, &bypass, &limiterAmount })
        smoother->snapToTarget();

    rmsCoefficient = timeCoefficient (targets.rmsMs, internalRate);

    switchGain.snapTo (1.0);

    lowPassCoefficients.set (std::exp (logLowPass.getCurrent()), butterworthQ, internalRate);
    highPassCoefficients.set (std::exp (logHighPass.getCurrent()), butterworthQ, internalRate);
}

void CompDsp::updateConfig (int numSamples)
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

void CompDsp::compress (float* const* signal, const float* const* key, int numSamples)
{
    const auto factor = factorOf (activeConfig);
    const auto preCompDelay = activeConfig.preCompSamples * factor;

    double peakReduction[2] { 0.0, 0.0 };

    for (int i = 0; i < numSamples; ++i)
    {
        if (! logLowPass.isSettled())
            lowPassCoefficients.set (std::exp (logLowPass.next()), butterworthQ, internalRate);

        if (! logHighPass.isSettled())
            highPassCoefficients.set (std::exp (logHighPass.next()), butterworthQ, internalRate);

        const auto thresholdDb = threshold.next();
        const auto slopeNow = slope.next();
        const auto kneeDb = knee.next();
        const auto makeupDb = makeup.next();
        const auto makeupGain = makeupDb == 0.0 ? 1.0 : std::pow (10.0, makeupDb / 20.0);
        const auto preview = previewAmount.next();
        const auto lowPassBlend = lowPassAmount.next();
        const auto highPassBlend = highPassAmount.next();

        if (! rms.isSettled())
            rmsCoefficient = timeCoefficient (rms.next(), internalRate);

        const auto mainLeft = current.feedback ? channels[0].previousOutput : (double) signal[0][i];
        const auto mainRight = current.feedback ? channels[1].previousOutput : (double) signal[1][i];
        const auto keyLeft = (double) key[0][i];
        const auto keyRight = (double) key[1][i];

        double detector[2];

        switch (current.detector)
        {
            case mainL:   detector[0] = detector[1] = mainLeft; break;
            case mainR:   detector[0] = detector[1] = mainRight; break;
            case sideLR:  detector[0] = keyLeft; detector[1] = keyRight; break;
            case sideL:   detector[0] = detector[1] = keyLeft; break;
            case sideR:   detector[0] = detector[1] = keyRight; break;
            case mid:     detector[0] = detector[1] = 0.5 * (mainLeft + mainRight); break;
            case side:    detector[0] = detector[1] = 0.5 * (mainLeft - mainRight); break;
            case mainLR:
            default:      detector[0] = mainLeft; detector[1] = mainRight; break;
        }

        double level[2];
        double filtered[2];

        for (int ch = 0; ch < 2; ++ch)
        {
            auto& state = channels[(size_t) ch];

            filtered[ch] = state.detector.filter (detector[ch], highPassCoefficients, lowPassCoefficients, highPassBlend, lowPassBlend);
            level[ch] = state.detector.levelDb (filtered[ch], std::min (rmsCoefficient, maxDetectorCoefficient));
        }

        if (current.linkChannels)
        {
            const auto loudest = std::max (level[0], level[1]);

            level[0] += (loudest - level[0]) * current.linkAmount;
            level[1] += (loudest - level[1]) * current.linkAmount;
        }

        const auto writePosition = (preCompPosition + i) % preCompLength;
        const auto readPosition = (writePosition + preCompLength - preCompDelay) % preCompLength;

        for (int ch = 0; ch < 2; ++ch)
        {
            auto& state = channels[(size_t) ch];

            double reduction;

            if (current.classic)
            {
                // The smoothed level feeds the gain computer. Levels are measured from the
                // bottom of the knee, because smoothing in dB from silence would add a long
                // delay before the level even reached the threshold.
                const auto floorDb = thresholdDb - kneeDb;
                const auto smoothed = state.follower.follow (ballistics, std::max (level[ch], floorDb) - floorDb) + floorDb;

                reduction = computeReduction (smoothed, thresholdDb, slopeNow, kneeDb, current.weirdKnee);
            }
            else
            {
                reduction = state.follower.follow (ballistics, computeReduction (level[ch], thresholdDb, slopeNow, kneeDb, current.weirdKnee));
            }

            peakReduction[ch] = std::max (peakReduction[ch], reduction);

            state.preCompDelay[(size_t) writePosition] = signal[ch][i];
            const auto delayed = (double) state.preCompDelay[(size_t) readPosition];

            const auto gain = std::pow (10.0, -reduction / 20.0) * makeupGain;
            const auto compressed = delayed * gain;

            state.previousOutput = compressed;
            signal[ch][i] = (float) (compressed + (filtered[ch] - compressed) * preview);
        }
    }

    preCompPosition = (preCompPosition + numSamples) % preCompLength;

    for (int ch = 0; ch < 2; ++ch)
    {
        auto& peak = gainReduction[(size_t) ch];
        auto stored = peak.load (std::memory_order_relaxed);
        const auto newPeak = (float) peakReduction[ch];

        while (newPeak > stored && ! peak.compare_exchange_weak (stored, newPeak, std::memory_order_relaxed))
        {
        }
    }
}

void CompDsp::process (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int numSamples)
{
    for (int done = 0; done < numSamples;)
    {
        const auto chunk = std::min (numSamples - done, maxChunk);
        processChunk (main, numMain, sidechain, numSidechain, done, chunk);
        done += chunk;
    }
}

void CompDsp::processChunk (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int offset, int numSamples)
{
    updateConfig (numSamples);

    const auto dryDelayLength = activeLatency - (activeConfig.limiter ? limiter.getLatencySamples() : 0);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto writePosition = (dryPosition + i) % dryLength;

        for (int ch = 0; ch < 2; ++ch)
        {
            const auto mainChannel = std::min (ch, numMain - 1);
            const auto in = sanitise (main[mainChannel][offset + i]);

            channels[(size_t) ch].dryDelay[(size_t) writePosition] = (float) in;
            mainBuffer.setSample (ch, i, (float) in);

            const auto keySample = numSidechain > 0 ? sanitise (sidechain[std::min (ch, numSidechain - 1)][offset + i]) : 0.0;
            keyBuffer.setSample (ch, i, (float) keySample);
        }
    }

    auto mainBlock = juce::dsp::AudioBlock<float> (mainBuffer).getSubBlock (0, (size_t) numSamples);
    auto keyBlock = juce::dsp::AudioBlock<float> (keyBuffer).getSubBlock (0, (size_t) numSamples);

    if (auto* oversampler = oversamplerFor (activeConfig, false))
    {
        const juce::dsp::AudioBlock<const float> constMain (mainBlock), constKey (keyBlock);

        auto upMain = oversampler->processSamplesUp (constMain);

        float* signal[] { upMain.getChannelPointer (0), upMain.getChannelPointer (1) };
        const float* key[] { signal[0], signal[1] };

        if (current.detector == sideLR || current.detector == sideL || current.detector == sideR)
        {
            auto upKey = oversamplerFor (activeConfig, true)->processSamplesUp (constKey);

            key[0] = upKey.getChannelPointer (0);
            key[1] = upKey.getChannelPointer (1);
        }

        compress (signal, key, (int) upMain.getNumSamples());

        oversampler->processSamplesDown (mainBlock);
    }
    else
    {
        float* signal[] { mainBuffer.getWritePointer (0), mainBuffer.getWritePointer (1) };
        const float* key[] { keyBuffer.getReadPointer (0), keyBuffer.getReadPointer (1) };

        compress (signal, key, numSamples);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const auto wetGain = wet.next();
        const auto dryGain = dry.next();
        const auto outputGain = output.next();
        const auto bypassAmount = bypass.next();
        const auto limit = limiterAmount.next();
        const auto fade = switchGain.next();
        const auto readPosition = (dryPosition + i + dryLength - dryDelayLength) % dryLength;

        double out[2];

        for (int ch = 0; ch < 2; ++ch)
        {
            const auto delayedDry = (double) channels[(size_t) ch].dryDelay[(size_t) readPosition];
            const auto mixed = ((double) mainBuffer.getSample (ch, i) * wetGain + delayedDry * dryGain) * outputGain;

            out[ch] = bypassAmount >= 1.0 ? delayedDry : mixed + (delayedDry - mixed) * bypassAmount;
        }

        if (activeConfig.limiter)
            limiter.process (out[0], out[1], limit);

        for (int ch = 0; ch < numMain; ++ch)
            main[ch][offset + i] = (float) (out[ch] * fade);
    }

    dryPosition = (dryPosition + numSamples) % dryLength;
}
