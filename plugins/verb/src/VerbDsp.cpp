#include "VerbDsp.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>

using verb::Convolver;

VerbDsp::~VerbDsp()
{
    dropConvolvers();
}

void VerbDsp::dropConvolvers() noexcept
{
    delete active;
    delete outgoing;
    delete incoming.exchange (nullptr);
    delete retired.exchange (nullptr);

    active = nullptr;
    outgoing = nullptr;
    fadeLeft = 0;
}

void VerbDsp::prepare (double newSampleRate, int newLatency)
{
    if (newSampleRate != sampleRate)
        dropConvolvers();

    sampleRate = newSampleRate;
    latency.store (newLatency, std::memory_order_relaxed);

    predelayLength = (int) std::ceil (0.5 * sampleRate) + maxChunk + 4;
    dryLength = Convolver::lowCpuLatency + maxChunk + 4;

    for (auto& ring : predelayRing)
        ring.assign ((size_t) predelayLength, 0.0f);

    for (auto& ring : dryRing)
        ring.assign ((size_t) dryLength, 0.0f);

    for (auto& ring : keyRing)
        ring.assign ((size_t) dryLength, 0.0f);

    ducker.prepare (sampleRate);
    wetGate.prepare (sampleRate);
    duckAmount.setTime (20.0, sampleRate);

    predelay.setTime (40.0, sampleRate);

    for (auto* smoother : { &center, &width, &wet, &dry, &output, &bypass })
        smoother->setTime (20.0, sampleRate);

    switchGain.setTime (3.0, sampleRate);

    // Phase 4 Wave 3: Freeze and Shimmer. The loop gain moves through a convolver crossfade.
    freezer.prepare (sampleRate);
    loop.prepare (sampleRate, maxChunk);
    loopGain.setTime (crossfadeMs / 4.0, sampleRate);

    reset();
}

void VerbDsp::startFade (double milliseconds) noexcept
{
    fadeTotal = std::max (1, (int) (milliseconds * 0.001 * sampleRate));
    fadeLeft = fadeTotal;
}

void VerbDsp::reset() noexcept
{
    if (active != nullptr)
        active->reset();

    if (outgoing != nullptr)
        outgoing->reset();

    for (auto& ring : predelayRing)
        std::fill (ring.begin(), ring.end(), 0.0f);

    for (auto& ring : dryRing)
        std::fill (ring.begin(), ring.end(), 0.0f);

    for (auto& ring : keyRing)
        std::fill (ring.begin(), ring.end(), 0.0f);

    ducker.reset();
    wetGate.reset();

    // Phase 4 Wave 3.
    freezer.reset();
    loop.reset();
    freezeState = FreezeState::idle;
    freezeIn = 0.0;
    freezeLevel = liveGain = inputGain = 1.0;
    loopRunning = false;
    loopLeft = 0;
    snapLoopGain = true;
    loopGain.snapTo (0.0);

    predelayWrite = dryWrite = 0;
    switchGain.snapTo (1.0);
    lateBlocks.store (0, std::memory_order_relaxed);

    // Only the new convolver is left to play, so the fade comes up from silence (§5.3).
    if (outgoing != nullptr)
    {
        delete retired.exchange (outgoing);
        outgoing = nullptr;
    }

    startFade (fadeInMs);
}

void VerbDsp::snapToTargets (const Targets& targets) noexcept
{
    current = targets;

    predelay.snapTo (std::round (targets.predelayMs * 0.001 * sampleRate));
    center.snapTo (targets.center);
    width.snapTo (targets.width);
    wet.snapTo (targets.wet);
    dry.snapTo (targets.dry);
    output.snapTo (targets.output);
    bypass.snapTo (targets.bypass ? 1.0 : 0.0);
    duckAmount.snapTo (std::clamp (targets.ducking, 0.0, 1.0));
    snapLoopGain = true;
}

bool VerbDsp::loopWanted() const noexcept
{
    return current.shimmer > 0.0 || loopGain.getCurrent() > 0.0 || loopLeft > 0;
}

// Phase 4 Wave 3: Freeze turned on. From a release, the frozen sound carries on from the level it
// had reached, so nothing jumps.
void VerbDsp::startFreeze() noexcept
{
    freezer.capture();

    if (freezeState == FreezeState::releasing)
        freezeIn = std::asin (std::clamp (std::sin (freezeIn * juce::MathConstants<double>::halfPi) * freezeLevel, 0.0, 1.0))
                 / juce::MathConstants<double>::halfPi;
    else
        freezeIn = 0.0;

    freezeLevel = 1.0;
    freezeState = FreezeState::holding;
}

void VerbDsp::offer (std::unique_ptr<Convolver> convolver)
{
    delete incoming.exchange (convolver.release());
}

std::unique_ptr<Convolver> VerbDsp::takeRetired() noexcept
{
    return std::unique_ptr<Convolver> (retired.exchange (nullptr));
}

void VerbDsp::acceptIncoming() noexcept
{
    auto* next = incoming.load (std::memory_order_acquire);

    if (next == nullptr || outgoing != nullptr || next->getLatency() == latency.load (std::memory_order_relaxed))
        switchGain.setTarget (1.0); // also ends a mute if the mode was changed back

    if (next == nullptr || outgoing != nullptr)
        return;

    if (next->getLatency() != latency.load (std::memory_order_relaxed))
    {
        // The dry path has to jump, so everything mutes first. switchGain comes back below.
        switchGain.setTarget (0.0);

        if (! switchGain.isSettled() || retired.load (std::memory_order_acquire) != nullptr)
            return;

        incoming.store (nullptr, std::memory_order_release);
        retired.store (active, std::memory_order_release);
        active = next;
        latency.store (next->getLatency(), std::memory_order_relaxed);
        switchGain.setTarget (1.0);
        startFade (fadeInMs);
        return;
    }

    if (retired.load (std::memory_order_acquire) != nullptr)
        return;

    incoming.store (nullptr, std::memory_order_release);
    outgoing = active;
    active = next;
    startFade (outgoing != nullptr ? crossfadeMs : fadeInMs);
}

void VerbDsp::processChunk (float* const* channels, int numChannels, const float* const* sidechain, int numSidechain, int offset, int numSamples,
                            bool offline) noexcept
{
    acceptIncoming();

    const auto targetDelay = std::round (current.predelayMs * 0.001 * sampleRate);

    predelay.setTarget (targetDelay);
    center.setTarget (current.center);
    width.setTarget (current.width);
    wet.setTarget (current.wet);
    dry.setTarget (current.dry);
    output.setTarget (current.output);
    bypass.setTarget (current.bypass ? 1.0 : 0.0);
    duckAmount.setTarget (std::clamp (current.ducking, 0.0, 1.0));

    const auto dryDelay = latency.load (std::memory_order_relaxed);

    // Phase 4 Wave 3: Shimmer's loop gain, g = Shimmer x 0.9 / sqrt (E), and Freeze's edges.
    const auto rt60 = active != nullptr ? active->getRt60() : 1.0;
    {
        const auto target = active != nullptr ? std::clamp (current.shimmer, 0.0, 1.0) * 0.9 / std::sqrt (std::max (active->getLoopEnergy(), 1.0e-12)) : 0.0;

        if (snapLoopGain && active != nullptr)
        {
            loopGain.snapTo (target);
            snapLoopGain = false;
        }
        else
        {
            loopGain.setTarget (target);
        }
    }

    const auto loopOn = loopWanted();

    if (loopOn && ! loopRunning)
        loop.reset();

    loopRunning = loopOn;

    if (current.freeze && freezeState != FreezeState::holding)
        startFreeze();
    else if (! current.freeze && freezeState == FreezeState::holding)
        freezeState = FreezeState::releasing;

    // Phase 4 Wave 2: Ducking and the Gate run only while on, or still letting go.
    const auto dynamics = current.ducking > 0.0 || ! duckAmount.isSettled() || duckAmount.getCurrent() > 0.0 || ! ducker.isIdle()
                       || current.gate || wetGate.getGain() < 1.0;
    const auto useKey = current.duckSidechain && numSidechain > 0 && sidechain != nullptr;

    // The sidechain, delayed by the latency as the dry path is, so the detector acts on the wet
    // signal it's meant for (the model's finding).
    if (useKey)
    {
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            const auto* in = sidechain[std::min (ch, numSidechain - 1)] + offset;
            auto& ring = keyRing[(size_t) ch];

            for (int i = 0; i < numSamples; ++i)
            {
                const auto write = (dryWrite + i) % dryLength;
                ring[(size_t) write] = std::isfinite (in[i]) ? in[i] : 0.0f;
                delayedKey[(size_t) ch][(size_t) i] = ring[(size_t) ((write - dryDelay + dryLength) % dryLength)];
            }
        }
    }

    for (int ch = 0; ch < maxChannels; ++ch)
    {
        auto* in = channels[std::min (ch, numChannels - 1)] + offset;
        auto& ring = dryRing[(size_t) ch];

        for (int i = 0; i < numSamples; ++i)
        {
            if (! std::isfinite (in[i]))
                in[i] = 0.0f;

            const auto write = (dryWrite + i) % dryLength;
            ring[(size_t) write] = in[i];
            delayedDry[(size_t) ch][(size_t) i] = ring[(size_t) ((write - dryDelay + dryLength) % dryLength)];
        }
    }

    dryWrite = (dryWrite + numSamples) % dryLength;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto delay = predelay.next();
        const auto whole = (int) delay;
        const auto fraction = (float) (delay - whole);

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            auto& ring = predelayRing[(size_t) ch];
            ring[(size_t) predelayWrite] = channels[std::min (ch, numChannels - 1)][offset + i];

            const auto first = (predelayWrite - whole + predelayLength) % predelayLength;
            const auto second = (first - 1 + predelayLength) % predelayLength;
            const auto a = ring[(size_t) first];

            wetInput[(size_t) ch][(size_t) i] = fraction == 0.0f ? a : a + (ring[(size_t) second] - a) * fraction;
        }

        predelayWrite = (predelayWrite + 1) % predelayLength;
    }

    // Phase 4 Wave 3: the loop's feedback joins the input, and Freeze mutes both over 20 ms.
    if (loopRunning)
        loop.addFeedback (wetInput[0].data(), wetInput[1].data(), numSamples, rt60);

    const auto inputStep = 1.0 / (inputFadeMs * 0.001 * sampleRate);

    if (freezeState == FreezeState::holding || inputGain < 1.0)
    {
        const auto target = freezeState == FreezeState::holding ? 0.0 : 1.0;

        for (int i = 0; i < numSamples; ++i)
        {
            inputGain = target < inputGain ? std::max (target, inputGain - inputStep) : std::min (target, inputGain + inputStep);
            wetInput[0][(size_t) i] *= (float) inputGain;
            wetInput[1][(size_t) i] *= (float) inputGain;
        }
    }

    for (auto* buffer : { &wetNew, &wetOld })
        for (auto& channel : *buffer)
            std::fill (channel.begin(), channel.begin() + numSamples, 0.0f);

    if (active != nullptr && active->isModulated())
        active->setModulation (current.modRate, current.modDepth, current.modSpread);

    if (outgoing != nullptr && outgoing->isModulated())
        outgoing->setModulation (current.modRate, current.modDepth, current.modSpread);

    if (active != nullptr)
    {
        active->process (wetInput[0].data(), wetInput[1].data(), wetNew[0].data(), wetNew[1].data(), numSamples, offline);
        lateBlocks.store (active->getLateBlocks(), std::memory_order_relaxed);
    }

    if (outgoing != nullptr)
        outgoing->process (wetInput[0].data(), wetInput[1].data(), wetOld[0].data(), wetOld[1].data(), numSamples, offline);

    const auto mono = numChannels == 1;

    // Phase 4 Wave 3: the crossfade into the frozen sound, and its fall at the IR's RT60.
    const auto freezeStep = 1.0 / (freezeFadeMs * 0.001 * sampleRate);
    const auto releaseDecay = std::pow (10.0, -3.0 / (std::max (0.05, rt60) * sampleRate));

    for (int i = 0; i < numSamples; ++i)
    {
        const auto index = (size_t) i;

        auto fadeNew = 1.0, fadeOld = 0.0;

        if (fadeLeft > 0)
        {
            const auto angle = (1.0 - (double) fadeLeft / (double) fadeTotal) * juce::MathConstants<double>::halfPi;
            fadeNew = std::sin (angle);
            fadeOld = std::cos (angle);
            --fadeLeft;
        }

        auto wetLeft = (double) wetNew[0][index] * fadeNew + (double) wetOld[0][index] * fadeOld;
        auto wetRight = (double) wetNew[1][index] * fadeNew + (double) wetOld[1][index] * fadeOld;

        // Phase 4 Wave 3: Shimmer takes the convolver's output; Freeze replaces it.
        if (loopRunning)
            loop.feed (wetLeft, wetRight, i, current.shimmerRatio, loopGain.next());

        if (freezeState != FreezeState::idle)
        {
            double frozenLeft = 0.0, frozenRight = 0.0;
            freezer.next (frozenLeft, frozenRight);

            if (freezeState == FreezeState::holding)
            {
                freezeIn = std::min (1.0, freezeIn + freezeStep);
                liveGain = std::min (liveGain, std::cos (freezeIn * juce::MathConstants<double>::halfPi));
            }
            else
            {
                liveGain = std::min (1.0, liveGain + inputStep);
                freezeLevel *= releaseDecay;
            }

            const auto frozenGain = std::sin (freezeIn * juce::MathConstants<double>::halfPi) * freezeLevel;
            wetLeft = wetLeft * liveGain + frozenLeft * frozenGain;
            wetRight = wetRight * liveGain + frozenRight * frozenGain;

            if (freezeState == FreezeState::releasing && freezeLevel < 1.0e-5 && liveGain >= 1.0)
            {
                freezeState = FreezeState::idle;
                freezeIn = 0.0;
                freezeLevel = 1.0;
            }
        }

        freezer.push (wetLeft, wetRight);

        const auto pan = center.next();
        const auto mid = (wetLeft + wetRight) * 0.5;
        const auto side = (wetLeft - wetRight) * 0.5 * width.next();

        auto wetGain = wet.next();
        const auto dryGain = dry.next();

        // Phase 4 Wave 2: Ducking and the Gate, on the latency-delayed main input or sidechain.
        if (dynamics)
        {
            const auto& source = useKey ? delayedKey : delayedDry;
            const auto level = std::max (std::abs ((double) source[0][index]), std::abs ((double) source[mono && ! useKey ? 0 : 1][index]));
            wetGain *= ducker.process (level, duckAmount.next());
            wetGain *= wetGate.process (level, current.gate, current.gateThresholdDb, current.gateHoldMs, current.gateDecayMs);
        }
        const auto outputGain = output.next();
        const auto bypassed = bypass.next();
        const auto mute = switchGain.next();

        const auto left = (mid + side) * std::min (1.0, 1.0 - pan) * wetGain;
        const auto right = (mid - side) * std::min (1.0, 1.0 + pan) * wetGain;

        const auto mix = [&] (double wetSide, double dryIn)
        {
            return (float) (((wetSide + dryIn * dryGain) * outputGain * (1.0 - bypassed) + dryIn * bypassed) * mute);
        };

        if (mono)
        {
            channels[0][offset + i] = mix ((left + right) * 0.5, (double) delayedDry[0][index]);
        }
        else
        {
            channels[0][offset + i] = mix (left, (double) delayedDry[0][index]);
            channels[1][offset + i] = mix (right, (double) delayedDry[1][index]);
        }
    }

    if (loopRunning)
        loopLeft = loopGain.getCurrent() > 0.0 ? loop.getLoopDelay() : std::max (0, loopLeft - numSamples);

    if (fadeLeft == 0 && outgoing != nullptr && retired.load (std::memory_order_acquire) == nullptr)
    {
        retired.store (outgoing, std::memory_order_release);
        outgoing = nullptr;
    }
}

void VerbDsp::process (float* const* channels, int numChannels, const float* const* sidechain, int numSidechain, int numSamples, bool offline) noexcept
{
    numChannels = std::min (numChannels, (int) maxChannels);
    numSidechain = std::min (numSidechain, (int) maxChannels);

    for (int position = 0; position < numSamples;)
    {
        // Phase 4 Wave 3: while Shimmer's loop runs, the convolver takes at most the loop's delay
        // at a time, so what it feeds back is ready before it's needed.
        const auto limit = loopWanted() ? loop.getLoopDelay() : (int) maxChunk;
        const auto chunk = std::min (numSamples - position, limit);
        processChunk (channels, numChannels, sidechain, numSidechain, position, chunk, offline);
        position += chunk;
    }
}
