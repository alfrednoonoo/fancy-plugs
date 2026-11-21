#include "LegacyCompQ25Dsp.h"

#include <fancy/dsp/Upward.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
constexpr double butterworthQ = 0.70710678118654752;
constexpr double switchFadeMs = 2.0;
constexpr double switchSettleSeconds = 0.003;
constexpr double configDebounceSeconds = 0.05;
constexpr double highPassOffHz = 20.5;
constexpr double lowPassOffHz = 23900.0;
constexpr double minDetectorAverageMs = 0.1;

// Phase 3 Q2.5 (docs/comp.md).
constexpr double parameterMs = 20.0;
constexpr double characterFadeMs = 10.0;
constexpr double emphasisDb = 4.0;          // Warm tilts the lows up into the curve, Bright the highs
constexpr double characterDcHz = 5.0;
constexpr double autoAverageMs = 3000.0;
constexpr double autoGateMs = 5.0;
constexpr double autoGatePower = 1.0e-6;    // -60 dBFS: below it the long-term level holds,
constexpr double autoRelativeGate = 1.0e-4; // and 40 dB under the long-term level, so a pause doesn't drag it down
constexpr double autoOffsetMs = 50.0;
constexpr int autoUpdateSamples = 16;
constexpr int eqUpdateSamples = 16;

using fancy::dsp::timeCoefficient;

double dbToGain (double db) noexcept
{
    return std::pow (10.0, db / 20.0);
}

double slopeFor (double ratio) noexcept
{
    return ratio >= LegacyCompQ25Dsp::infiniteRatio ? 1.0 : 1.0 - 1.0 / ratio;
}

double sanitise (float sample) noexcept
{
    return std::isfinite (sample) ? (double) sample : 0.0;
}
} // namespace

LegacyCompQ25Dsp::LegacyCompQ25Dsp() = default;
LegacyCompQ25Dsp::~LegacyCompQ25Dsp() = default;

double LegacyCompQ25Dsp::computeReduction (double levelDb, double thresholdDb, double slope, double kneeDb, bool weird) noexcept
{
    return fancy::dsp::computeReduction (levelDb, thresholdDb, slope, kneeDb, weird);
}

// Character -------------------------------------------------------------------

void LegacyCompQ25Dsp::CharacterStage::prepare (int type, double rate) noexcept
{
    namespace sat = fancy::dsp::saturation;

    curve = sat::Curve {};
    curve.tape = 0.0;

    if (type == bright)
    {
        // Sature's transistor knee: symmetric, so odd harmonics.
        curve.soft = 0.0;
        curve.transistor = 1.0;
    }
    else
    {
        // Sature's soft curve with Tube's later negative half: even harmonics from the start.
        curve.soft = 1.0;
        curve.negativeLimit = sat::tubeNegativeLimit;
    }

    const auto tilt = type == bright ? emphasisDb : -emphasisDb;
    emphasis.set (tilt, rate);
    deEmphasis.set (-tilt, rate);

    blockDc = type == warm;
    dcCoefficient = std::exp (-2.0 * std::numbers::pi * characterDcHz / rate);

    reset();
}

void LegacyCompQ25Dsp::CharacterStage::reset() noexcept
{
    pre.reset();
    post.reset();
    adaa.reset();
    previousU = 0.0;
    dcInput = dcOutput = 0.0;
}

// The stage adds what the curve changes, rather than replacing the signal: the curve's output
// minus its own linear part (the ADAA's two-sample average). Quiet material then passes
// untouched, without the ADAA's half-sample average, and only the added part lags.
double LegacyCompQ25Dsp::CharacterStage::process (double x, double driveGain, double amount) noexcept
{
    const auto u = driveGain * pre.process (emphasis, x);
    const auto shaped = adaa.process (curve, u, false, 0.0);
    const auto linear = 0.5 * (u + previousU);
    previousU = u;

    auto added = post.process (deEmphasis, (shaped - linear) / driveGain);

    if (blockDc)
    {
        // One-pole DC blocker on the added part: Warm's asymmetry makes DC on loud signals.
        const auto y = added - dcInput + dcCoefficient * dcOutput;
        dcInput = added;
        dcOutput = y;
        added = y;
    }

    return x + amount * added;
}

LegacyCompQ25Dsp::Oversampler* LegacyCompQ25Dsp::oversamplerFor (const Config& config, bool sidechainPath) const noexcept
{
    if (config.oversampling == off)
        return nullptr;

    const auto index = (size_t) ((config.oversampling - 1) * 2 + (config.minimumPhase ? 1 : 0));
    return sidechainPath ? keyOversamplers[index].get() : mainOversamplers[index].get();
}

int LegacyCompQ25Dsp::factorOf (const Config& config) const noexcept
{
    return 1 << config.oversampling;
}

int LegacyCompQ25Dsp::latencyOf (const Config& config) const noexcept
{
    auto latency = config.preCompSamples;

    if (config.oversampling != off)
        latency += oversamplerLatencies[(size_t) ((config.oversampling - 1) * 2 + (config.minimumPhase ? 1 : 0))];

    if (config.limiter)
        latency += limiter.getLatencySamples();

    return latency;
}

LegacyCompQ25Dsp::Config LegacyCompQ25Dsp::configFor (const Targets& targets) const noexcept
{
    Config config;
    config.oversampling = targets.oversampling;
    config.minimumPhase = targets.minimumPhase;
    config.limiter = targets.limitOutput;
    config.preCompSamples = (int) std::lround (targets.preCompMs * 0.001 * sampleRate);
    return config;
}

void LegacyCompQ25Dsp::prepare (double newSampleRate)
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

    for (int stages = 1; stages <= maxStages; ++stages)
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

void LegacyCompQ25Dsp::setNewSmootherTimes() noexcept
{
    for (auto* smoother : { &upThreshold, &upSlope, &maxLift, &upFloor, &drive })
        smoother->setTime (parameterMs, internalRate);

    for (auto& band : eqSmoothers)
        for (auto* smoother : { &band.logFrequency, &band.gain, &band.q })
            smoother->setTime (parameterMs, internalRate);

    characterAmount.setTime (characterFadeMs, internalRate);
    autoOffset.setTime (autoOffsetMs, internalRate);

    longTermCoefficient = timeCoefficient (autoAverageMs, internalRate);
    gateCoefficient = timeCoefficient (autoGateMs, internalRate);
}

void LegacyCompQ25Dsp::setNewTargets (const Targets& targets) noexcept
{
    upThreshold.setTarget (targets.upThresholdDb);
    upSlope.setTarget (1.0 - 1.0 / std::max (1.0, targets.upRatio));
    maxLift.setTarget (targets.maxLiftDb);
    upFloor.setTarget (targets.floorDb);
    drive.setTarget (targets.driveDb);

    for (size_t b = 0; b < (size_t) numEqBands; ++b)
    {
        eqSmoothers[b].logFrequency.setTarget (std::log (targets.eq[b].frequencyHz));
        eqSmoothers[b].gain.setTarget (targets.eq[b].gainDb);
        eqSmoothers[b].q.setTarget (targets.eq[b].q);
    }

    if (! targets.autoThreshold)
        autoOffset.setTarget (0.0);

    // Character: a change of Type or Position fades the stage out, switches, and fades in.
    const auto wanted = targets.character;

    if (wanted != activeCharacter || (wanted != characterOff && targets.characterPost != activeCharacterPost))
        characterAmount.setTarget (0.0);
    else
        characterAmount.setTarget (activeCharacter == characterOff ? 0.0 : 1.0);
}

void LegacyCompQ25Dsp::activateCharacter() noexcept
{
    activeCharacter = current.character;
    activeCharacterPost = current.characterPost;

    for (auto& state : channels)
        state.character.prepare (activeCharacter, internalRate);

    characterAmount.setTarget (activeCharacter == characterOff ? 0.0 : 1.0);
}

void LegacyCompQ25Dsp::updateEqCoefficients() noexcept
{
    namespace matched = fancy::dsp::matched;

    for (size_t b = 0; b < (size_t) numEqBands; ++b)
    {
        const auto gainDb = eqSmoothers[b].gain.getCurrent();
        const auto active = gainDb != 0.0;

        if (! active)
        {
            // A band at 0 dB is skipped entirely, and starts again from rest.
            if (eqActive[b])
                for (auto& state : channels)
                    state.eq[b] = {};

            eqActive[b] = false;
            continue;
        }

        matched::BandSpec spec;
        spec.type = b == (size_t) lowShelf ? matched::BandType::lowShelf
                  : b == (size_t) highShelf ? matched::BandType::highShelf
                                            : matched::BandType::band;
        spec.frequencyHz = std::exp (eqSmoothers[b].logFrequency.getCurrent());
        spec.gainDb = gainDb;
        spec.q = eqSmoothers[b].q.getCurrent();
        spec.order = 2;
        spec.on = true;

        const auto filter = matched::design (spec, internalRate);
        eqSections[b] = filter.numSections > 0 ? filter.sections[0] : matched::Section {};
        eqActive[b] = true;
    }
}

double LegacyCompQ25Dsp::filterEq (ChannelState& state, double x) noexcept
{
    for (size_t b = 0; b < (size_t) numEqBands; ++b)
    {
        if (! eqActive[b])
            continue;

        const auto& s = eqSections[b];
        auto& z = state.eq[b];

        const auto y = s.b0 * x + z.z1;
        z.z1 = s.b1 * x - s.a1 * y + z.z2;
        z.z2 = s.b2 * x - s.a2 * y;
        x = y;
    }

    return x;
}

void LegacyCompQ25Dsp::resetCore()
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

        state.upFollower.reset();
        state.character.reset();
        state.eq.fill ({});
    }

    // Auto threshold starts from the reference, so its offset begins at 0 dB.
    longTermPower = std::pow (10.0, autoReferenceDb / 10.0);
    gatePower = 0.0;
    autoCountdown = 0;
    autoOffset.snapTo (0.0);
    autoOffsetDisplay.store (0.0f, std::memory_order_relaxed);

    limiter.reset();
    dryPosition = 0;
    preCompPosition = 0;
}

void LegacyCompQ25Dsp::reset()
{
    resetCore();
}

void LegacyCompQ25Dsp::updateBallistics() noexcept
{
    ballistics.set (current.attackMs, current.releaseMs, current.attack90, current.autoRelease, current.linearRelease, internalRate);
}

void LegacyCompQ25Dsp::setTargets (const Targets& targets)
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

    setNewTargets (targets);
    updateBallistics();
}

void LegacyCompQ25Dsp::applyConfig (const Config& config)
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

    setNewSmootherTimes();
    resetCore();
    setTargets (current);

    lowPassCoefficients.set (std::exp (logLowPass.getCurrent()), butterworthQ, internalRate);
    highPassCoefficients.set (std::exp (logHighPass.getCurrent()), butterworthQ, internalRate);
    rmsCoefficient = timeCoefficient (rms.getCurrent(), internalRate);

    // The rate changed: the character's filters and the detector EQ follow it.
    for (auto& state : channels)
        state.character.prepare (activeCharacter, internalRate);

    updateEqCoefficients();
}

void LegacyCompQ25Dsp::snapToTargets (const Targets& targets)
{
    current = targets;

    wantedConfig = configFor (targets);
    wantedStableSamples = 0;
    holdSamples = 0;

    // Character goes straight to its Type and Position.
    activeCharacter = targets.character;
    activeCharacterPost = targets.characterPost;

    applyConfig (wantedConfig);

    for (auto* smoother : { &threshold, &slope, &knee, &makeup, &rms, &previewAmount, &lowPassAmount, &highPassAmount, &logLowPass, &logHighPass,
                            &wet, &dry, &output, &bypass, &limiterAmount,
                            &upThreshold, &upSlope, &maxLift, &upFloor, &drive, &characterAmount })
        smoother->snapToTarget();

    for (auto& band : eqSmoothers)
        for (auto* smoother : { &band.logFrequency, &band.gain, &band.q })
            smoother->snapToTarget();

    driveGain = dbToGain (drive.getCurrent());
    eqNeedsUpdate = false;
    eqCountdown = 0;
    updateEqCoefficients();

    rmsCoefficient = timeCoefficient (targets.rmsMs, internalRate);

    switchGain.snapTo (1.0);

    lowPassCoefficients.set (std::exp (logLowPass.getCurrent()), butterworthQ, internalRate);
    highPassCoefficients.set (std::exp (logHighPass.getCurrent()), butterworthQ, internalRate);
}

void LegacyCompQ25Dsp::updateConfig (int numSamples)
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

void LegacyCompQ25Dsp::compress (float* const* signal, const float* const* key, int numSamples)
{
    const auto factor = factorOf (activeConfig);
    const auto preCompDelay = activeConfig.preCompSamples * factor;

    double peakReduction[2] { 0.0, 0.0 };
    auto peakLift = 0.0;
    auto lastLevel = -100.0;

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

        // Phase 3 Q2.5 parameters. Settled smoothers cost a comparison each.
        const auto upThresholdDb = upThreshold.next();
        const auto upSlopeNow = upSlope.next();
        const auto maxLiftDb = maxLift.next();
        const auto floorDb = upFloor.next();

        if (! drive.isSettled())
            driveGain = dbToGain (drive.next());

        auto eqMoving = false;

        for (auto& band : eqSmoothers)
        {
            band.logFrequency.next();
            band.gain.next();
            band.q.next();
            eqMoving = eqMoving || ! band.logFrequency.isSettled() || ! band.gain.isSettled() || ! band.q.isSettled();
        }

        if (eqMoving)
        {
            // Coefficients every 16 samples while a band moves (§4.2), and once more when it lands.
            if (--eqCountdown <= 0)
            {
                updateEqCoefficients();
                eqCountdown = eqUpdateSamples;
            }

            eqNeedsUpdate = true;
        }
        else if (eqNeedsUpdate)
        {
            updateEqCoefficients();
            eqNeedsUpdate = false;
            eqCountdown = 0;
        }

        // Character switches Type or Position once it has faded out.
        if (activeCharacter != current.character || (current.character != characterOff && activeCharacterPost != current.characterPost))
            if (characterAmount.isSettled() && characterAmount.getCurrent() == 0.0)
                activateCharacter();

        const auto characterMix = characterAmount.next();
        const auto characterOn = activeCharacter != characterOff && characterMix != 0.0;

        // Pre: a saturator before the compressor, so the detector hears it (Decision 1 A).
        if (characterOn && ! activeCharacterPost)
            for (int ch = 0; ch < 2; ++ch)
                signal[ch][i] = (float) channels[(size_t) ch].character.process ((double) signal[ch][i], driveGain, characterMix);

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
            filtered[ch] = filterEq (state, filtered[ch]);
            level[ch] = state.detector.levelDb (filtered[ch], std::min (rmsCoefficient, maxDetectorCoefficient));
        }

        if (current.linkChannels)
        {
            const auto loudest = std::max (level[0], level[1]);

            level[0] += (loudest - level[0]) * current.linkAmount;
            level[1] += (loudest - level[1]) * current.linkAmount;
        }

        lastLevel = std::max (level[0], level[1]);

        // Auto threshold: the detector signal's long-term level against the reference. With
        // feedback detection it reads the input, not the output, so the loop can't run away.
        if (current.autoThreshold)
        {
            const auto feedsBack = current.feedback && (current.detector < sideLR || current.detector > sideR);
            const auto a = feedsBack ? (double) signal[0][i] : filtered[0];
            const auto b = feedsBack ? (double) signal[1][i] : filtered[1];
            const auto power = 0.5 * (a * a + b * b);

            gatePower += gateCoefficient * (power - gatePower);

            if (gatePower > autoGatePower && gatePower > autoRelativeGate * longTermPower)
                longTermPower += longTermCoefficient * (power - longTermPower);

            if (--autoCountdown <= 0)
            {
                const auto levelDb = 10.0 * std::log10 (std::max (longTermPower, 1.0e-30));
                autoOffset.setTarget (std::clamp (levelDb - autoReferenceDb, -maxAutoOffsetDb, maxAutoOffsetDb));
                autoCountdown = autoUpdateSamples;
            }
        }

        const auto offsetDb = autoOffset.next();
        const auto effectiveThreshold = offsetDb == 0.0 ? thresholdDb : thresholdDb + offsetDb;

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
                const auto kneeFloorDb = effectiveThreshold - kneeDb;
                const auto smoothed = state.follower.follow (ballistics, std::max (level[ch], kneeFloorDb) - kneeFloorDb) + kneeFloorDb;

                reduction = computeReduction (smoothed, effectiveThreshold, slopeNow, kneeDb, current.weirdKnee);
            }
            else
            {
                reduction = state.follower.follow (ballistics, computeReduction (level[ch], effectiveThreshold, slopeNow, kneeDb, current.weirdKnee));
            }

            peakReduction[ch] = std::max (peakReduction[ch], reduction);

            // Upward: its own follower on the same ballistics, as in MultiComp. Classic's
            // smoothed level starts at the knee, so the lift always reads the level itself.
            auto lift = 0.0;

            if (current.upward || state.upFollower.envelope != 0.0)
            {
                auto liftTarget = 0.0;

                if (current.upward)
                    liftTarget = fancy::dsp::upward::liftFor (
                        level[ch], effectiveThreshold,
                        { upThresholdDb + offsetDb, fancy::dsp::upward::ratioFromSlope (upSlopeNow), maxLiftDb, floorDb });

                lift = state.upFollower.follow (ballistics, liftTarget);

                if (! current.upward && lift < 1.0e-6)
                    lift = state.upFollower.envelope = 0.0;
            }

            peakLift = std::max (peakLift, lift);

            state.preCompDelay[(size_t) writePosition] = signal[ch][i];
            const auto delayed = (double) state.preCompDelay[(size_t) readPosition];

            const auto gain = lift == 0.0 ? std::pow (10.0, -reduction / 20.0) * makeupGain
                                          : std::pow (10.0, (lift - reduction) / 20.0) * makeupGain;
            auto compressed = delayed * gain;

            if (characterOn && activeCharacterPost)
                compressed = state.character.process (compressed, driveGain, characterMix);

            state.previousOutput = compressed;
            signal[ch][i] = (float) (compressed + (filtered[ch] - compressed) * preview);
        }
    }

    preCompPosition = (preCompPosition + numSamples) % preCompLength;

    autoOffsetDisplay.store ((float) autoOffset.getCurrent(), std::memory_order_relaxed);
    levelDisplay.store ((float) std::max (lastLevel, -200.0), std::memory_order_relaxed);
    liftDisplay.store ((float) peakLift, std::memory_order_relaxed);

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

void LegacyCompQ25Dsp::process (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int numSamples)
{
    for (int done = 0; done < numSamples;)
    {
        const auto chunk = std::min (numSamples - done, maxChunk);
        processChunk (main, numMain, sidechain, numSidechain, done, chunk);
        done += chunk;
    }
}

void LegacyCompQ25Dsp::processChunk (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int offset, int numSamples)
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
