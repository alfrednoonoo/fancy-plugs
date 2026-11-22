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

        const auto wetLeft = (double) wetNew[0][index] * fadeNew + (double) wetOld[0][index] * fadeOld;
        const auto wetRight = (double) wetNew[1][index] * fadeNew + (double) wetOld[1][index] * fadeOld;

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
        const auto chunk = std::min (numSamples - position, (int) maxChunk);
        processChunk (channels, numChannels, sidechain, numSidechain, position, chunk, offline);
        position += chunk;
    }
}
