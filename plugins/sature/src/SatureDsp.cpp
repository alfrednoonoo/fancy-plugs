#include "SatureDsp.h"

#include "SatureGainTable.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace sat = fancy::dsp::saturation;

namespace
{
constexpr double smoothingMs = 20.0;
constexpr double dcCutHz = 5.0;
constexpr double lowestHz = 10.0;

// Lagrange half-sample delays: 4 taps (1.5 samples) for 4x and 8x, 8 taps (3.5 samples) for 2x,
// flat within 0.1 dB to 20 kHz at 44.1 kHz and above.
constexpr std::array<double, 4> lagrange4 { -1.0 / 16.0, 9.0 / 16.0, 9.0 / 16.0, -1.0 / 16.0 };
constexpr std::array<double, 8> lagrange8 { -5.0 / 2048.0, 49.0 / 2048.0, -245.0 / 2048.0, 1225.0 / 2048.0,
                                            1225.0 / 2048.0, -245.0 / 2048.0, 49.0 / 2048.0, -5.0 / 2048.0 };

int tapsFor (int factor) noexcept
{
    return factor == SatureDsp::x2 ? 8 : 4;
}

double dbToGain (double db) noexcept
{
    return std::pow (10.0, db / 20.0);
}
} // namespace

SatureDsp::SatureDsp()
{
    // 8th-order Butterworth: Q = 1 / (2 cos ((2k - 1) pi / 16)).
    for (int k = 0; k < numSections; ++k)
        sectionQ[(size_t) k] = 1.0 / (2.0 * std::cos ((2.0 * k + 1.0) * std::numbers::pi / 16.0));
}

double SatureDsp::tableGainDb (int type, double driveDb, double characterValue) noexcept
{
    const auto d = std::clamp (driveDb, 0.0, (double) (sat::tableDrives - 1));
    const auto c = std::clamp ((characterValue + 1.0) * 10.0, 0.0, (double) (sat::tableCharacters - 1));
    const auto d0 = std::min ((int) d, sat::tableDrives - 2);
    const auto c0 = std::min ((int) c, sat::tableCharacters - 2);
    const auto fd = d - d0, fc = c - c0;
    const auto& table = fancy::sature::autoGainDb[std::clamp (type, 0, (int) sat::numTypes - 1)];

    const auto low = (double) table[d0][c0] + fc * ((double) table[d0][c0 + 1] - (double) table[d0][c0]);
    const auto high = (double) table[d0 + 1][c0] + fc * ((double) table[d0 + 1][c0 + 1] - (double) table[d0 + 1][c0]);
    return low + fd * (high - low);
}

double SatureDsp::toneGainDb (double toneDb) noexcept
{
    const auto t = std::clamp (toneDb + 6.0, 0.0, (double) (sat::tableTones - 1));
    const auto t0 = std::min ((int) t, sat::tableTones - 2);
    const auto ft = t - t0;
    return (double) fancy::sature::toneGainDb[t0] + ft * ((double) fancy::sature::toneGainDb[t0 + 1] - (double) fancy::sature::toneGainDb[t0]);
}

double SatureDsp::effectiveHighHz (const Targets& t) noexcept
{
    return t.lowOn ? std::max (t.highHz, 2.0 * t.lowHz) : t.highHz;
}

void SatureDsp::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    oversampling.prepare (sampleRate, maxChunk, 2, true);
    matchedBuffer.setSize (2, maxChunk);
    midBuffer.setSize (2, maxChunk);

    delayLength = oversampling.getMaxLatency() + 2 + maxChunk + 1;

    for (auto& state : channelStates)
        state.dryDelay.assign ((size_t) delayLength, 0.0f);

    for (auto* smoother : { &trim, &drive, &character, &tone, &logLow, &logHigh, &lowOn, &highOn, &autoGain,
                            &wet, &dry, &output, &dcOn, &typeFade })
        smoother->setTime (smoothingMs, sampleRate);

    bypass.setTime (10.0, sampleRate);
    listen.setTime (10.0, sampleRate);

    dcCoefficient = std::exp (-2.0 * std::numbers::pi * dcCutHz / sampleRate);

    snapToTargets (targets);
}

void SatureDsp::resetCore()
{
    oversampling.reset();

    for (auto& state : channelStates)
    {
        for (auto& section : state.lowCut) section.reset();
        for (auto& section : state.highCut) section.reset();

        state.adaa.reset();
        state.tapeLoss.reset();
        state.tilt.reset();
        state.previousMid = 0.0;
        state.matchedHistory.fill (0.0);
        state.addedDelay.fill (0.0);
        state.dcIn = state.dcOut = 0.0;
        std::fill (state.dryDelay.begin(), state.dryDelay.end(), 0.0f);
    }

    delayWritePosition = 0;
    lastCurve = sat::makeCurve (activeType, character.getCurrent());
}

void SatureDsp::reset()
{
    resetCore();
}

void SatureDsp::updateRateDependent()
{
    const auto factor = oversampling.getFactor();

    processRate = sampleRate * (double) oversampling.getOversamplingRatio();
    tapeCoefficient = sat::TapeLoss::coefficientFor (processRate);
    lastToneDb = 1.0e9; // recompute the tilt for the new rate

    numTaps = tapsFor (factor);
    addedDelaySamples = numTaps / 2 - 1;
    halfDelayTaps.fill (0.0);

    for (int k = 0; k < numTaps; ++k)
        halfDelayTaps[(size_t) k] = numTaps == 8 ? lagrange8[(size_t) k] : lagrange4[(size_t) k];
}

void SatureDsp::setTargets (const Targets& newTargets)
{
    targets = newTargets;

    trim.setTarget (targets.trimGain);
    drive.setTarget (std::clamp (targets.driveDb, 0.0, 36.0));
    character.setTarget (std::clamp (targets.character, -1.0, 1.0));
    tone.setTarget (std::clamp (targets.toneDb, -6.0, 6.0));

    const auto nyquistLimit = 0.45 * sampleRate;

    if (targets.lowOn)
        logLow.setTarget (std::log (std::clamp (targets.lowHz, lowestHz, nyquistLimit)));

    if (targets.highOn)
        logHigh.setTarget (std::log (std::clamp (effectiveHighHz (targets), lowestHz, nyquistLimit)));

    lowOn.setTarget (targets.lowOn ? 1.0 : 0.0);
    highOn.setTarget (targets.highOn ? 1.0 : 0.0);
    autoGain.setTarget (targets.autoGain ? 1.0 : 0.0);
    wet.setTarget (targets.wetGain);
    dry.setTarget (targets.dryGain);
    output.setTarget (targets.outputGain);
    dcOn.setTarget (targets.dcFilter ? 1.0 : 0.0);
    listen.setTarget (targets.listen ? 1.0 : 0.0);
    bypass.setTarget (targets.bypass ? 1.0 : 0.0);

    requestedOversampling = std::clamp (targets.oversampling, (int) off, (int) x8);
    requestedMinimumPhase = targets.minimumPhase;

    const auto type = std::clamp (targets.type, 0, (int) sat::numTypes - 1);

    if (type != activeType)
    {
        // A change during a fade starts again from whichever Type is mostly heard.
        previousType = typeFade.getCurrent() < 0.5 ? previousType : activeType;
        activeType = type;
        typeFade.snapTo (0.0);
        typeFade.setTarget (1.0);
    }
}

void SatureDsp::snapToTargets (const Targets& newTargets)
{
    activeType = previousType = std::clamp (newTargets.type, 0, (int) sat::numTypes - 1);
    setTargets (newTargets);

    if (! targets.lowOn)
        logLow.setTarget (std::log (20.0));

    if (! targets.highOn)
        logHigh.setTarget (std::log (std::min (20000.0, 0.45 * sampleRate)));

    for (auto* smoother : { &trim, &drive, &character, &tone, &logLow, &logHigh, &lowOn, &highOn, &autoGain,
                            &wet, &dry, &output, &dcOn, &listen, &bypass })
        smoother->snapToTarget();

    typeFade.snapTo (1.0);

    oversampling.select (requestedOversampling, requestedMinimumPhase);
    updateRateDependent();
    updateBandFilters (true);
    resetCore();
}

int SatureDsp::latencyFor (int factor, bool minimumPhase) const noexcept
{
    if (factor == off)
        return 0;

    // The half-sample delay's (taps - 1) / 2 samples at the oversampled rate.
    const auto extra = (double) (tapsFor (factor) - 1) * 0.5 / (double) (1 << factor);
    return oversampling.latencyFor (factor, minimumPhase) + (int) std::lround (extra);
}

int SatureDsp::getLatencySamples() const noexcept
{
    return latencyFor (oversampling.getFactor(), oversampling.isMinimumPhase());
}

void SatureDsp::updateBandFilters (bool force)
{
    if (force || ! logLow.isSettled())
    {
        const auto hz = std::exp (logLow.next());

        for (int k = 0; k < numSections; ++k)
            lowCoefficients[(size_t) k].set (hz, sectionQ[(size_t) k], sampleRate);
    }

    if (force || ! logHigh.isSettled())
    {
        const auto hz = std::exp (logHigh.next());

        for (int k = 0; k < numSections; ++k)
            highCoefficients[(size_t) k].set (hz, sectionQ[(size_t) k], sampleRate);
    }
}

void SatureDsp::process (float* const* channels, int numChannels, int numSamples)
{
    for (int done = 0; done < numSamples;)
    {
        const auto chunk = std::min (numSamples - done, maxChunk);
        processChunk (channels, numChannels, done, chunk);
        done += chunk;
    }
}

void SatureDsp::processChunk (float* const* channels, int numChannels, int offset, int numSamples)
{
    if (oversampling.beginChunk (requestedOversampling, requestedMinimumPhase, numSamples))
    {
        updateRateDependent();
        resetCore();
    }

    const auto latency = getLatencySamples();

    // Settings for each input sample, and the band split.
    for (int i = 0; i < numSamples; ++i)
    {
        updateBandFilters (false);

        const auto t = typeFade.next();
        const auto characterValue = character.next();
        const auto driveDb = drive.next();
        const auto toneDb = tone.next();
        const auto autoAmount = autoGain.next();

        const auto curve = t >= 1.0 ? sat::makeCurve (activeType, characterValue)
                                    : sat::blend (sat::makeCurve (previousType, characterValue), sat::makeCurve (activeType, characterValue), t);

        chunkShapeChanged[(size_t) i] = ! curve.sameShape (lastCurve);
        chunkDc[(size_t) i] = (chunkShapeChanged[(size_t) i] || curve.bias != lastCurve.bias || i == 0) ? curve.shape (curve.bias) : chunkDc[(size_t) i - 1];
        chunkCurve[(size_t) i] = curve;
        lastCurve = curve;

        chunkDrive[(size_t) i] = dbToGain (driveDb);
        chunkTape[(size_t) i] = sat::TapeLoss::amountFor (driveDb, curve.tape);

        if (toneDb != lastToneDb)
        {
            tiltCoefficients.set (toneDb, processRate);
            lastToneDb = toneDb;
        }

        chunkTilt[(size_t) i] = tiltCoefficients;

        auto gainDb = 0.0;

        if (autoAmount > 0.0)
        {
            const auto table = t >= 1.0 ? tableGainDb (activeType, driveDb, characterValue)
                                        : tableGainDb (previousType, driveDb, characterValue) * (1.0 - t)
                                              + tableGainDb (activeType, driveDb, characterValue) * t;
            gainDb = autoAmount * (table + toneGainDb (toneDb));
        }

        chunkGain[(size_t) i] = gainDb == 0.0 ? 1.0 : dbToGain (gainDb);

        const auto trimGain = trim.next();
        const auto lowAmount = lowOn.next();
        const auto highAmount = highOn.next();
        const auto writePosition = (delayWritePosition + i) % delayLength;

        for (int ch = 0; ch < 2; ++ch)
        {
            const auto raw = ch < numChannels ? channels[ch][offset + i] : 0.0f;
            const auto in = std::isfinite (raw) ? raw : 0.0f;

            auto& state = channelStates[(size_t) ch];
            state.dryDelay[(size_t) writePosition] = in;

            const auto x = (double) in * trimGain;

            auto cut = x;
            for (int k = 0; k < numSections; ++k)
                cut = state.lowCut[(size_t) k].tick (lowCoefficients[(size_t) k], cut).high;

            auto mid = lowAmount == 0.0 ? x : x + lowAmount * (cut - x);

            auto top = mid;
            for (int k = 0; k < numSections; ++k)
                top = state.highCut[(size_t) k].tick (highCoefficients[(size_t) k], top).low;

            mid = highAmount == 0.0 ? mid : mid + highAmount * (top - mid);

            matchedBuffer.setSample (ch, i, (float) x);
            midBuffer.setSample (ch, i, (float) mid);
        }
    }

    if (oversampling.getFactor() == off)
        processBaseRate (numSamples);
    else
        processOversampled (numSamples);

    // matchedBuffer now holds m, midBuffer holds added.
    for (int i = 0; i < numSamples; ++i)
    {
        const auto wetGain = wet.next();
        const auto dryGain = dry.next();
        const auto outputGain = output.next();
        const auto dcAmount = dcOn.next();
        const auto listenAmount = listen.next();
        const auto bypassAmount = bypass.next();
        const auto fade = oversampling.nextFade();
        const auto readPosition = (delayWritePosition + i + delayLength - latency) % delayLength;

        for (int ch = 0; ch < 2; ++ch)
        {
            auto& state = channelStates[(size_t) ch];

            const auto added = (double) midBuffer.getSample (ch, i);
            const auto blocked = added - state.dcIn + dcCoefficient * state.dcOut;
            state.dcIn = added;
            state.dcOut = blocked;

            if (ch >= numChannels)
                continue;

            const auto addedOut = added + dcAmount * (blocked - added);
            const auto matched = (double) matchedBuffer.getSample (ch, i);

            const auto normal = (matched + addedOut) * wetGain + matched * dryGain;
            const auto heard = (normal + listenAmount * (addedOut * wetGain - normal)) * outputGain;
            const auto dryIn = (double) state.dryDelay[(size_t) readPosition];

            channels[ch][offset + i] = (float) ((heard + (dryIn - heard) * bypassAmount) * fade);
        }
    }

    delayWritePosition = (delayWritePosition + numSamples) % delayLength;
}

void SatureDsp::processBaseRate (int numSamples)
{
    // Oversampling Off: the curve without ADAA, so nothing lags and the matched path is the
    // input itself (docs/sature.md, "As built").
    for (int ch = 0; ch < 2; ++ch)
    {
        auto& state = channelStates[(size_t) ch];
        auto* mid = midBuffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto& curve = chunkCurve[(size_t) i];
            const auto in = (double) mid[i];

            auto y = curve.shape (chunkDrive[(size_t) i] * in + curve.bias) - chunkDc[(size_t) i];
            y = state.tapeLoss.process (y, tapeCoefficient, chunkTape[(size_t) i]);
            y = state.tilt.process (chunkTilt[(size_t) i], y) * chunkGain[(size_t) i];

            mid[i] = (float) (y - in);
        }
    }
}

void SatureDsp::processOversampled (int numSamples)
{
    const auto ratio = oversampling.getOversamplingRatio();

    juce::dsp::AudioBlock<float> matchedBlock (matchedBuffer);
    juce::dsp::AudioBlock<float> midBlock (midBuffer);
    matchedBlock = matchedBlock.getSubBlock (0, (size_t) numSamples);
    midBlock = midBlock.getSubBlock (0, (size_t) numSamples);

    auto& matchedOversampler = oversampling.get (0);
    auto& midOversampler = oversampling.get (1);

    auto matchedUp = matchedOversampler.processSamplesUp (juce::dsp::AudioBlock<const float> (matchedBlock));
    auto midUp = midOversampler.processSamplesUp (juce::dsp::AudioBlock<const float> (midBlock));

    const auto upSamples = (int) midUp.getNumSamples();

    for (int ch = 0; ch < 2; ++ch)
    {
        auto& state = channelStates[(size_t) ch];

        // The matched path: a half-sample delay that lines it up with the ADAA, flat to 20 kHz.
        auto* m = matchedUp.getChannelPointer ((size_t) ch);

        for (int j = 0; j < upSamples; ++j)
        {
            for (int k = numTaps - 1; k > 0; --k)
                state.matchedHistory[(size_t) k] = state.matchedHistory[(size_t) k - 1];

            state.matchedHistory[0] = (double) m[j];

            auto sum = 0.0;
            for (int k = 0; k < numTaps; ++k)
                sum += halfDelayTaps[(size_t) k] * state.matchedHistory[(size_t) k];

            m[j] = (float) sum;
        }

        // The saturated path, minus its own linear reference (the ADAA's two-sample average), so
        // at low drive `added` is zero whatever the band filter does.
        auto* s = midUp.getChannelPointer ((size_t) ch);

        for (int j = 0; j < upSamples; ++j)
        {
            const auto i = std::min (numSamples - 1, j / ratio);
            const auto& curve = chunkCurve[(size_t) i];
            const auto in = (double) s[j];

            auto y = state.adaa.process (curve, chunkDrive[(size_t) i] * in, chunkShapeChanged[(size_t) i] && j % ratio == 0, chunkDc[(size_t) i]);
            y = state.tapeLoss.process (y, tapeCoefficient, chunkTape[(size_t) i]);
            y = state.tilt.process (chunkTilt[(size_t) i], y) * chunkGain[(size_t) i];

            const auto reference = 0.5 * (in + state.previousMid);
            state.previousMid = in;

            auto difference = y - reference;

            // Delay by the half-sample filter's whole samples, so both paths line up.
            for (int k = 0; k < addedDelaySamples; ++k)
                std::swap (difference, state.addedDelay[(size_t) k]);

            s[j] = (float) difference;
        }
    }

    matchedOversampler.processSamplesDown (matchedBlock);
    midOversampler.processSamplesDown (midBlock);
}
