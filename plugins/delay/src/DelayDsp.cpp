#include "DelayDsp.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
constexpr double gateMs = 20.0, parameterMs = 30.0, bypassMs = 10.0;
constexpr double crossfadeMs = 30.0, glideMs = 150.0;
constexpr double filterQ = 0.70710678118654752;
constexpr double dcHz = 5.0;
constexpr double silence = 1.0e-15;
constexpr int chunkSize = 256;

constexpr double maxFeedback = 1.0, maxSaturatedFeedback = 1.1;

// Linear up to the knee, then bending towards 1 so a held loop can't grow without limit.
constexpr double safetyKnee = 0.9;

double safetyCurve (double value) noexcept
{
    const auto size = std::abs (value);

    if (size <= safetyKnee)
        return value;

    const auto room = 1.0 - safetyKnee;
    return std::copysign (safetyKnee + room * std::tanh ((size - safetyKnee) / room), value);
}

double shape (double value, double saturation) noexcept
{
    const auto safe = safetyCurve (value);
    return saturation > 0.0 ? safe + (std::tanh (value) - safe) * saturation : safe;
}

double hermite (double newer, double near, double far, double older, double fraction) noexcept
{
    const auto c1 = 0.5 * (far - newer);
    const auto c2 = newer - 2.5 * near + 2.0 * far - 0.5 * older;
    const auto c3 = 0.5 * (older - newer) + 1.5 * (near - far);

    return ((c3 * fraction + c2) * fraction + c1) * fraction + near;
}

int nextPowerOfTwo (int value) noexcept
{
    auto size = 1;

    while (size < value)
        size <<= 1;

    return size;
}
} // namespace

double DelayDsp::tapMs (const TapSpec& spec, double bpm) noexcept
{
    return spec.noteMode ? delay::noteMs (spec.noteLength, bpm) : std::clamp (spec.lengthMs, 0.0, delay::maxDelayMs);
}

void DelayDsp::prepare (double sampleRate)
{
    rate = sampleRate;

    maxDelaySamples = (int) std::ceil (delay::maxDelayMs * 0.001 * rate);
    ringSize = nextPowerOfTwo (maxDelaySamples + 8);
    ringMask = ringSize - 1;
    fadeSamples = std::max (1, (int) std::lround (crossfadeMs * 0.001 * rate));

    dcCoefficient = 1.0 - 2.0 * std::numbers::pi * dcHz / rate;
    glideCoefficient = 1.0 - std::exp (-1000.0 / (glideMs * rate));

    for (auto& tap : taps)
    {
        for (auto& channel : tap.ring)
            channel.assign ((size_t) ringSize, 0.0f);

        for (auto* smoother : { &tap.inGate, &tap.outGate })
            smoother->setTime (gateMs, rate);

        for (auto* smoother : { &tap.feedback, &tap.width, &tap.volume, &tap.panLeft, &tap.panRight, &tap.lowpassMix, &tap.highpassMix,
                                &tap.crushMix, &tap.loopMix, &tap.saturateMix, &tap.pingMix, &tap.startLeft, &tap.lowpassOctaves,
                                &tap.highpassOctaves, &tap.bits })
            smoother->setTime (parameterMs, rate);
    }

    for (auto* smoother : { &wetGain, &dryGain, &outputGain })
        smoother->setTime (parameterMs, rate);

    bypassMix.setTime (bypassMs, rate);

    reset();
}

void DelayDsp::reset()
{
    writeIndex = 0;

    for (auto& tap : taps)
    {
        for (auto& channel : tap.ring)
            std::fill (channel.begin(), channel.end(), 0.0f);

        for (int ch = 0; ch < 2; ++ch)
        {
            tap.lowpass[ch].reset();
            tap.highpass[ch].reset();
            tap.dcInput[ch] = tap.dcOutput[ch] = 0.0;
        }

        tap.fadeLeft = 0;
        tap.quietSamples = 0;
        tap.asleep = true;
    }
}

void DelayDsp::setTapTargets (Tap& tap, const TapSpec& spec, bool anySolo, double bpm, bool smoothTime, bool isMono) noexcept
{
    const auto active = spec.used && spec.on;
    const auto audible = active && (! anySolo || spec.solo);

    tap.inGate.setTarget (active ? 1.0 : 0.0);
    tap.outGate.setTarget (audible ? 1.0 : 0.0);

    const auto feedbackLimit = spec.saturate ? maxSaturatedFeedback : maxFeedback;
    tap.feedback.setTarget (spec.used ? std::clamp (spec.feedback, 0.0, feedbackLimit) : 0.0);

    const auto limitHz = 0.45 * rate;
    tap.lowpassOctaves.setTarget (std::log2 (std::min (spec.lowpassHz, limitHz)));
    tap.highpassOctaves.setTarget (std::log2 (std::min (spec.highpassHz, limitHz)));
    tap.lowpassMix.setTarget (spec.lowpassHz >= lowpassOffHz ? 0.0 : 1.0);
    tap.highpassMix.setTarget (spec.highpassHz <= highpassOffHz ? 0.0 : 1.0);

    tap.bits.setTarget (spec.bits);
    tap.crushMix.setTarget (spec.bits >= fullBits ? 0.0 : 1.0);

    tap.loopMix.setTarget (spec.filtersInLoop ? 1.0 : 0.0);
    tap.saturateMix.setTarget (spec.saturate ? 1.0 : 0.0);
    tap.volume.setTarget (spec.volumeGain);

    const auto width = isMono ? 1.0 : spec.width;
    const auto pan = isMono ? 0.0 : spec.pan;
    const auto pingPong = spec.pingPong && ! isMono;

    tap.width.setTarget (width);
    tap.panLeft.setTarget (std::min (1.0, 1.0 - pan));
    tap.panRight.setTarget (std::min (1.0, 1.0 + pan));
    tap.pingMix.setTarget (pingPong ? 1.0 : 0.0);
    tap.startLeft.setTarget (pan <= 0.0 ? 1.0 : 0.0);

    if (smoothTime && ! tap.smoothTime)
    {
        tap.headA = (int) std::lround (tap.glidePosition);
        tap.fadeLeft = 0;
    }
    else if (! smoothTime && tap.smoothTime)
    {
        tap.glidePosition = (double) (tap.fadeLeft > 0 ? tap.headB : tap.headA);
        tap.headA = tap.fadeLeft > 0 ? tap.headB : tap.headA;
        tap.fadeLeft = 0;
    }

    tap.smoothTime = smoothTime;
    tap.targetDelay = std::clamp ((int) std::lround (tapMs (spec, bpm) * 0.001 * rate), 1, maxDelaySamples);

    if (tap.asleep && active)
    {
        // A sleeping tap's ring is silent, so its heads can go straight to the new time.
        tap.asleep = false;
        tap.quietSamples = 0;
        snapTap (tap);
    }
}

void DelayDsp::snapTap (Tap& tap) noexcept
{
    for (auto* smoother : { &tap.feedback, &tap.width, &tap.volume, &tap.panLeft, &tap.panRight, &tap.inGate, &tap.outGate,
                            &tap.lowpassMix, &tap.highpassMix, &tap.crushMix, &tap.loopMix, &tap.saturateMix, &tap.pingMix,
                            &tap.startLeft, &tap.lowpassOctaves, &tap.highpassOctaves, &tap.bits })
        smoother->snapToTarget();

    tap.headA = tap.headB = tap.targetDelay;
    tap.glidePosition = (double) tap.targetDelay;
    tap.fadeLeft = 0;

    tap.lowpassCoefficients.set (std::exp2 (tap.lowpassOctaves.getCurrent()), filterQ, rate);
    tap.highpassCoefficients.set (std::exp2 (tap.highpassOctaves.getCurrent()), filterQ, rate);
    tap.crushScale = std::exp2 (tap.bits.getCurrent() - 1.0);
}

void DelayDsp::setTargets (const Targets& targets, bool isMono) noexcept
{
    const auto anySolo = std::any_of (targets.taps.begin(), targets.taps.end(),
                                      [] (const TapSpec& spec) { return spec.used && spec.on && spec.solo; });

    for (size_t i = 0; i < taps.size(); ++i)
        setTapTargets (taps[i], targets.taps[i], anySolo, targets.bpm, targets.smoothTime, isMono);

    wetGain.setTarget (targets.wetGain);
    dryGain.setTarget (targets.dryGain);
    outputGain.setTarget (targets.outputGain);
    bypassMix.setTarget (targets.bypass ? 1.0 : 0.0);
}

void DelayDsp::snapToTargets (const Targets& targets, bool isMono) noexcept
{
    setTargets (targets, isMono);

    for (auto& tap : taps)
        snapTap (tap);

    for (auto* smoother : { &wetGain, &dryGain, &outputGain, &bypassMix })
        smoother->snapToTarget();
}

float DelayDsp::readHead (const Tap& tap, int channel, int writeAt, int head) const noexcept
{
    return tap.ring[channel][(size_t) ((writeAt - head) & ringMask)];
}

float DelayDsp::readGlide (const Tap& tap, int channel, int writeAt) const noexcept
{
    const auto position = tap.glidePosition;
    const auto whole = (int) position;
    const auto fraction = position - (double) whole;

    if (fraction == 0.0)
        return readHead (tap, channel, writeAt, whole);

    // Hermite needs a sample either side, and the newest one must already be written.
    const auto head = std::max (2, whole);
    const auto& ring = tap.ring[channel];

    return (float) hermite (ring[(size_t) ((writeAt - head + 1) & ringMask)],
                            ring[(size_t) ((writeAt - head) & ringMask)],
                            ring[(size_t) ((writeAt - head - 1) & ringMask)],
                            ring[(size_t) ((writeAt - head - 2) & ringMask)],
                            fraction);
}

void DelayDsp::updateCoefficients (Tap& tap) noexcept
{
    if (! tap.lowpassOctaves.isSettled())
        tap.lowpassCoefficients.set (std::exp2 (tap.lowpassOctaves.next()), filterQ, rate);

    if (! tap.highpassOctaves.isSettled())
        tap.highpassCoefficients.set (std::exp2 (tap.highpassOctaves.next()), filterQ, rate);

    if (! tap.bits.isSettled())
        tap.crushScale = std::exp2 (tap.bits.next() - 1.0);
}

void DelayDsp::processTap (Tap& tap, const float* inLeft, const float* inRight, double* wetLeft, double* wetRight, int numSamples) noexcept
{
    auto writeAt = writeIndex;
    auto wroteSound = false;

    for (int i = 0; i < numSamples; ++i)
    {
        updateCoefficients (tap);

        const auto feedback = tap.feedback.next();
        const auto inGate = tap.inGate.next();
        const auto outGate = tap.outGate.next();
        const auto lowpassMix = tap.lowpassMix.next();
        const auto highpassMix = tap.highpassMix.next();
        const auto crushMix = tap.crushMix.next();
        const auto loopMix = tap.loopMix.next();
        const auto saturation = tap.saturateMix.next();
        const auto pingMix = tap.pingMix.next();
        const auto startLeft = tap.startLeft.next();
        const auto width = tap.width.next();
        const auto volume = tap.volume.next() * outGate;
        const auto panLeft = tap.panLeft.next();
        const auto panRight = tap.panRight.next();

        // Read heads ------------------------------------------------------------------------

        if (tap.smoothTime && tap.fadeLeft == 0 && tap.headA != tap.targetDelay)
        {
            tap.headB = tap.targetDelay;
            tap.fadeLeft = fadeSamples;
        }

        double read[2];

        if (tap.smoothTime)
        {
            for (int ch = 0; ch < 2; ++ch)
                read[ch] = (double) readHead (tap, ch, writeAt, tap.headA);

            if (tap.fadeLeft > 0)
            {
                const auto position = 1.0 - (double) tap.fadeLeft / (double) fadeSamples;

                for (int ch = 0; ch < 2; ++ch)
                    read[ch] += ((double) readHead (tap, ch, writeAt, tap.headB) - read[ch]) * position;

                if (--tap.fadeLeft == 0)
                    tap.headA = tap.headB;
            }
        }
        else
        {
            for (int ch = 0; ch < 2; ++ch)
                read[ch] = (double) readGlide (tap, ch, writeAt);

            const auto target = (double) tap.targetDelay;

            if (tap.glidePosition != target)
            {
                tap.glidePosition += (target - tap.glidePosition) * glideCoefficient;

                if (std::abs (target - tap.glidePosition) < 0.001)
                    tap.glidePosition = target;
            }
        }

        // Filters, resolution and shaping ---------------------------------------------------

        const auto shaped = feedback > 0.0 || saturation > 0.0;

        double output[2], loop[2];

        for (int ch = 0; ch < 2; ++ch)
        {
            auto value = read[ch];

            if (lowpassMix > 0.0)
                value += (tap.lowpass[ch].tick (tap.lowpassCoefficients, value).low - value) * lowpassMix;
            else
                tap.lowpass[ch].reset();

            if (highpassMix > 0.0)
                value += (tap.highpass[ch].tick (tap.highpassCoefficients, value).high - value) * highpassMix;
            else
                tap.highpass[ch].reset();

            if (crushMix > 0.0)
                value += (std::round (value * tap.crushScale) / tap.crushScale - value) * crushMix;

            const auto looped = read[ch] + (value - read[ch]) * loopMix;

            output[ch] = shaped ? shape (value, saturation) : value;
            loop[ch] = shaped ? shape (looped, saturation) : looped;

            const auto blocked = loop[ch] - tap.dcInput[ch] + dcCoefficient * tap.dcOutput[ch];
            tap.dcInput[ch] = loop[ch];
            tap.dcOutput[ch] = blocked;
            loop[ch] = blocked * feedback;
        }

        // Back into the ring ----------------------------------------------------------------

        const auto left = (double) inLeft[i] * inGate;
        const auto right = (double) inRight[i] * inGate;

        auto writeLeft = left + loop[0];
        auto writeRight = right + loop[1];

        if (pingMix > 0.0)
        {
            const auto mono = 0.5 * (left + right);
            writeLeft += (mono * startLeft + loop[1] - writeLeft) * pingMix;
            writeRight += (mono * (1.0 - startLeft) + loop[0] - writeRight) * pingMix;
        }

        if (feedback > 0.0)
        {
            writeLeft = safetyCurve (writeLeft);
            writeRight = safetyCurve (writeRight);
        }

        if (! std::isfinite (writeLeft) || std::abs (writeLeft) < silence)
            writeLeft = 0.0;

        if (! std::isfinite (writeRight) || std::abs (writeRight) < silence)
            writeRight = 0.0;

        wroteSound = wroteSound || writeLeft != 0.0 || writeRight != 0.0;

        tap.ring[0][(size_t) writeAt] = (float) writeLeft;
        tap.ring[1][(size_t) writeAt] = (float) writeRight;
        writeAt = (writeAt + 1) & ringMask;

        // Out to the mix --------------------------------------------------------------------

        const auto mid = 0.5 * (output[0] + output[1]);
        const auto side = 0.5 * (output[0] - output[1]) * width;
        const auto gainLeft = panLeft + (1.0 - panLeft) * pingMix;
        const auto gainRight = panRight + (1.0 - panRight) * pingMix;

        wetLeft[i] += (mid + side) * volume * gainLeft;
        wetRight[i] += (mid - side) * volume * gainRight;
    }

    // Once a whole ring's worth of silence has gone in, the tap has nothing left to play.
    tap.quietSamples = wroteSound ? 0 : tap.quietSamples + numSamples;

    if (tap.quietSamples >= ringSize && tap.inGate.isSettled() && tap.inGate.getCurrent() == 0.0)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            tap.lowpass[ch].reset();
            tap.highpass[ch].reset();
            tap.dcInput[ch] = tap.dcOutput[ch] = 0.0;
        }

        tap.asleep = true;
    }
}

void DelayDsp::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    double wetLeft[chunkSize], wetRight[chunkSize];

    for (int start = 0; start < numSamples;)
    {
        const auto count = std::min (chunkSize, numSamples - start);

        float* const left = channels[0] + start;
        float* const right = numChannels > 1 ? channels[1] + start : left;

        std::fill_n (wetLeft, count, 0.0);
        std::fill_n (wetRight, count, 0.0);

        for (auto& tap : taps)
            if (! tap.asleep)
                processTap (tap, left, right, wetLeft, wetRight, count);

        for (int i = 0; i < count; ++i)
        {
            const auto wet = wetGain.next();
            const auto dry = dryGain.next();
            const auto output = outputGain.next();
            const auto bypass = bypassMix.next();

            const auto processedLeft = ((double) left[i] * dry + wetLeft[i] * wet) * output;
            left[i] = (float) (processedLeft + ((double) left[i] - processedLeft) * bypass);

            if (numChannels > 1)
            {
                const auto processedRight = ((double) right[i] * dry + wetRight[i] * wet) * output;
                right[i] = (float) (processedRight + ((double) right[i] - processedRight) * bypass);
            }
        }

        writeIndex = (writeIndex + count) & ringMask;
        start += count;
    }
}
