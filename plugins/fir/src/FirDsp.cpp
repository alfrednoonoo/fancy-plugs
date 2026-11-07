#include "FirDsp.h"

#include <algorithm>
#include <cmath>

using fir::SpectralEngine;

namespace
{
constexpr double fadeMs = 20.0;

float sanitise (float sample) noexcept
{
    return std::isfinite (sample) ? sample : 0.0f;
}
} // namespace

FirDsp::FirDsp() = default;

FirDsp::~FirDsp()
{
    delete active;
    delete incoming;
    delete waitingToRetire;
    delete built.exchange (nullptr);

    for (auto& slot : retired)
        delete slot.exchange (nullptr);
}

FirDsp::Config FirDsp::normalised (Config config) noexcept
{
    if (config.convolve)
        config.lowLatency = false;

    return config;
}

int FirDsp::pack (const Config& config) noexcept
{
    return config.fftSize | (config.lowLatency ? 1 << 20 : 0) | (config.convolve ? 1 << 21 : 0);
}

FirDsp::Config FirDsp::unpack (int packed) noexcept
{
    Config config;
    config.fftSize = packed & ((1 << 20) - 1);
    config.lowLatency = (packed & (1 << 20)) != 0;
    config.convolve = (packed & (1 << 21)) != 0;
    return config;
}

SpectralEngine* FirDsp::build (const Config& config)
{
    return new SpectralEngine (config, rate, shared);
}

void FirDsp::prepare (double sampleRate, const Targets& targets)
{
    rate = sampleRate;
    current = targets;

    delete active;
    delete incoming;
    delete built.exchange (nullptr);

    incoming = nullptr;
    fading = false;
    requested.store (-1);

    active = build (normalised (targets.engine));

    for (auto& buffer : inputs)
        buffer.assign ((size_t) maxChunk, 0.0f);

    for (int ch = 0; ch < 2; ++ch)
    {
        activeOut[ch].assign ((size_t) maxChunk, 0.0f);
        incomingOut[ch].assign ((size_t) maxChunk, 0.0f);
    }

    // The longest latency any engine can have, so bypass never needs to reallocate.
    bypassLength = SpectralEngine::latencyOf ({ fir::sizes[fir::numSizes - 1], false, false }) + maxChunk + 1;

    for (auto& line : bypassDelay)
        line.assign ((size_t) bypassLength, 0.0f);

    bypassPosition = 0;

    gain.setTime (20.0, rate);
    output.setTime (20.0, rate);
    bypass.setTime (10.0, rate);
    fade.setTime (fadeMs / 5.0, rate);

    setTargets (targets);

    for (auto* smoother : { &gain, &output, &bypass })
        smoother->snapToTarget();
}

void FirDsp::reset()
{
    if (active != nullptr)
        active->reset();

    if (incoming != nullptr)
        incoming->reset();

    for (auto& line : bypassDelay)
        std::fill (line.begin(), line.end(), 0.0f);
}

void FirDsp::setTargets (const Targets& targets) noexcept
{
    current = targets;

    gain.setTarget (targets.gain);
    output.setTarget (targets.outputGain);
    bypass.setTarget (targets.bypass ? 1.0 : 0.0);
}

void FirDsp::retire (SpectralEngine* engine) noexcept
{
    if (engine == nullptr)
        return;

    for (auto& slot : retired)
    {
        SpectralEngine* empty = nullptr;

        if (slot.compare_exchange_strong (empty, engine, std::memory_order_acq_rel))
            return;
    }

    // Every slot is full, so it waits here for the worker to catch up.
    if (waitingToRetire == nullptr)
        waitingToRetire = engine;
}

void FirDsp::serviceBuilds()
{
    for (auto& slot : retired)
        delete slot.exchange (nullptr, std::memory_order_acq_rel);

    const auto wanted = requested.load (std::memory_order_acquire);

    if (wanted < 0 || built.load (std::memory_order_acquire) != nullptr)
        return;

    auto* engine = build (unpack (wanted));
    built.store (engine, std::memory_order_release);
}

void FirDsp::updateEngines() noexcept
{
    if (waitingToRetire != nullptr)
    {
        auto* engine = waitingToRetire;
        waitingToRetire = nullptr;
        retire (engine);
    }

    const auto wanted = normalised (current.engine);

    // A newer change replaces one that's still on its way in.
    if (incoming != nullptr && ! (incoming->getConfig() == wanted))
    {
        retire (incoming);
        incoming = nullptr;
        fading = false;
    }

    if (incoming != nullptr || active->getConfig() == wanted)
    {
        if (incoming == nullptr)
            requested.store (-1, std::memory_order_release);

        return;
    }

    SpectralEngine* arrived = nullptr;

    if (synchronous)
    {
        delete built.exchange (nullptr);
        arrived = build (wanted);
    }
    else
    {
        requested.store (pack (wanted), std::memory_order_release);
        arrived = built.exchange (nullptr, std::memory_order_acq_rel);

        if (arrived != nullptr && ! (arrived->getConfig() == wanted))
        {
            retire (arrived);
            arrived = nullptr;
        }
    }

    if (arrived == nullptr)
        return;

    requested.store (-1, std::memory_order_release);

    incoming = arrived;
    warmupLeft = incoming->getLatency();
    fading = false;
    fade.snapTo (0.0);
}

void FirDsp::process (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int numSamples) noexcept
{
    for (int done = 0; done < numSamples;)
    {
        const auto chunk = std::min (numSamples - done, maxChunk);
        processChunk (main, numMain, sidechain, numSidechain, done, chunk);
        done += chunk;
    }
}

void FirDsp::processChunk (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int offset, int numSamples) noexcept
{
    updateEngines();

    const auto hasKey = numSidechain > 0;
    const auto numChannels = std::min (numMain, 2);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto writePosition = (bypassPosition + i) % bypassLength;

        for (int ch = 0; ch < 2; ++ch)
        {
            const auto in = sanitise (main[std::min (ch, numMain - 1)][offset + i]);

            inputs[ch][(size_t) i] = in;
            bypassDelay[ch][(size_t) writePosition] = in;
            inputs[ch + 2][(size_t) i] = hasKey ? sanitise (sidechain[std::min (ch, numSidechain - 1)][offset + i]) : 0.0f;
        }
    }

    const auto* keyLeft = hasKey ? inputs[2].data() : nullptr;
    const auto* keyRight = hasKey ? inputs[3].data() : nullptr;

    active->process (inputs[0].data(), inputs[1].data(), keyLeft, keyRight,
                     activeOut[0].data(), activeOut[1].data(), numChannels, numSamples, current.frame, true);

    if (incoming != nullptr)
    {
        incoming->process (inputs[0].data(), inputs[1].data(), keyLeft, keyRight,
                           incomingOut[0].data(), incomingOut[1].data(), numChannels, numSamples, current.frame, false);
    }

    const auto latency = active->getLatency();

    for (int i = 0; i < numSamples; ++i)
    {
        double out[2] { (double) activeOut[0][(size_t) i], (double) activeOut[1][(size_t) i] };

        if (incoming != nullptr)
        {
            if (! fading && --warmupLeft <= 0)
            {
                fading = true;
                fade.setTarget (1.0);
            }

            if (fading)
            {
                const auto amount = fade.next();

                for (int ch = 0; ch < 2; ++ch)
                    out[ch] += ((double) incomingOut[ch][(size_t) i] - out[ch]) * amount;
            }
        }

        const auto gainNow = gain.next() * output.next();
        const auto bypassAmount = bypass.next();
        const auto readPosition = (bypassPosition + i + bypassLength - latency) % bypassLength;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto delayed = (double) bypassDelay[ch][(size_t) readPosition];
            const auto processed = out[ch] * gainNow;

            main[ch][offset + i] = (float) (bypassAmount >= 1.0 ? delayed : processed + (delayed - processed) * bypassAmount);
        }
    }

    bypassPosition = (bypassPosition + numSamples) % bypassLength;

    if (incoming != nullptr && fading && fade.isSettled())
    {
        retire (active);
        active = incoming;
        incoming = nullptr;
        fading = false;
    }
}
