// A copy of plugins/exciter/src/ExciterDsp.cpp at commit 189f287, renamed. See LegacyExciterDsp.h.

#include "LegacyExciterDsp.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double shaperLimit = 1.5;
constexpr double maxBias = 0.75;
constexpr double butterworthQ = 0.70710678118654752;
constexpr double switchFadeMs = 2.0;
constexpr double switchSettleSeconds = 0.003;
constexpr double adaaThreshold = 1.0e-6;

double biasFor (double character) noexcept
{
    return maxBias * (1.0 - character);
}
} // namespace

LegacyExciterDsp::LegacyExciterDsp() = default;
LegacyExciterDsp::~LegacyExciterDsp() = default;

double LegacyExciterDsp::shape (double x) noexcept
{
    if (std::abs (x) >= shaperLimit)
        return std::copysign (1.0, x);

    return x - 4.0 / 27.0 * x * x * x;
}

double LegacyExciterDsp::shapeAntiderivative (double x) noexcept
{
    const auto magnitude = std::abs (x);

    if (magnitude >= shaperLimit)
        return magnitude - 0.5625;

    const auto squared = x * x;
    return 0.5 * squared - squared * squared / 27.0;
}

void LegacyExciterDsp::prepare (double newSampleRate)
{
    using Oversampler = juce::dsp::Oversampling<float>;

    sampleRate = newSampleRate;
    latencies[off] = 0;

    for (int factor = 0; factor < numFactors; ++factor)
    {
        auto oversampler = std::make_unique<Oversampler> (2);
        oversampler->setUsingIntegerLatency (true);

        for (int stage = 0; stage <= factor; ++stage)
        {
            const auto transition = stage == 0 ? 0.06f : 0.12f;
            const auto stopband = -100.0f + 10.0f * (float) stage;

            oversampler->addOversamplingStage (Oversampler::filterHalfBandFIREquiripple,
                                               transition, stopband, transition, stopband);
        }

        oversampler->initProcessing ((size_t) maxChunk);
        latencies[(size_t) factor + 1] = juce::roundToInt (oversampler->getLatencyInSamples());
        oversamplers[(size_t) factor] = std::move (oversampler);
    }

    wetBuffer.setSize (2, maxChunk);

    delayLength = *std::max_element (latencies.begin(), latencies.end()) + maxChunk + 1;

    for (auto& state : channelStates)
        state.dryDelay.assign ((size_t) delayLength, 0.0f);

    for (auto& state : channelStates)
        state.lowPass.setButterworthLowPass (0.45 * sampleRate, sampleRate);

    logFrequency.setTime (30.0, sampleRate);
    character.setTime (30.0, sampleRate);
    boost.setTime (20.0, sampleRate);
    harmonics.setTime (20.0, sampleRate);
    mixBack.setTime (20.0, sampleRate);
    output.setTime (20.0, sampleRate);
    bypass.setTime (10.0, sampleRate);
    switchGain.setTime (switchFadeMs, sampleRate);

    reset();
}

void LegacyExciterDsp::resetCore()
{
    for (auto& oversampler : oversamplers)
        if (oversampler != nullptr)
            oversampler->reset();

    for (auto& state : channelStates)
    {
        state.preFilter.reset();
        state.postFilter.reset();
        state.lowPass.reset();
        state.previousInput = 0.0;
        state.previousAntiderivative = 0.0;
        std::fill (state.dryDelay.begin(), state.dryDelay.end(), 0.0f);
    }

    delayWritePosition = 0;
}

void LegacyExciterDsp::reset()
{
    resetCore();
}

void LegacyExciterDsp::setTargets (const Targets& targets)
{
    logFrequency.setTarget (std::log (std::min (targets.frequencyHz, 0.45 * sampleRate)));
    boost.setTarget (targets.boostGain);
    harmonics.setTarget (targets.harmonics);
    mixBack.setTarget (targets.mixBackGain);
    character.setTarget (targets.character);
    output.setTarget (targets.outputGain);
    bypass.setTarget (targets.bypass ? 1.0 : 0.0);
    requestedOversampling = targets.oversampling;
}

void LegacyExciterDsp::snapToTargets (const Targets& targets)
{
    setTargets (targets);

    logFrequency.snapTo (std::log (std::min (targets.frequencyHz, 0.45 * sampleRate)));
    boost.snapTo (targets.boostGain);
    harmonics.snapTo (targets.harmonics);
    mixBack.snapTo (targets.mixBackGain);
    character.snapTo (targets.character);
    output.snapTo (targets.outputGain);
    bypass.snapTo (targets.bypass ? 1.0 : 0.0);
    switchGain.snapTo (1.0);
    switchHoldSamples = 0;

    updateFilter (logFrequency.getCurrent());
    selectOversampling (targets.oversampling);
}

void LegacyExciterDsp::updateFilter (double logHz) noexcept
{
    filter.set (std::exp (logHz), butterworthQ, sampleRate);
}

void LegacyExciterDsp::selectOversampling (int index)
{
    activeOversampling = index;
    activeLatency = latencies[(size_t) index];
    resetCore();
}

int LegacyExciterDsp::getLatencySamples() const noexcept
{
    return activeLatency;
}

void LegacyExciterDsp::process (float* const* channels, int numChannels, int numSamples)
{
    for (int done = 0; done < numSamples;)
    {
        const auto chunk = std::min (numSamples - done, maxChunk);
        processChunk (channels, numChannels, done, chunk);
        done += chunk;
    }
}

void LegacyExciterDsp::shapeBlock (juce::dsp::AudioBlock<float> block, int baseSamples)
{
    const auto numSamples = (int) block.getNumSamples();
    const auto factor = std::max (1, numSamples / std::max (1, baseSamples));

    // The bias of each input sample, held across its oversampled samples.
    for (int i = 0; i < baseSamples; ++i)
    {
        chunkBias[(size_t) i] = biasFor (character.getCurrent());
        character.next();
    }

    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
    {
        auto* samples = block.getChannelPointer (ch);
        auto& state = channelStates[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const auto bias = chunkBias[(size_t) std::min (baseSamples - 1, i / factor)];
            const auto in = (double) samples[i] + bias;
            const auto antiderivative = shapeAntiderivative (in);
            const auto difference = in - state.previousInput;

            const auto shaped = std::abs (difference) < adaaThreshold
                                  ? shape (0.5 * (in + state.previousInput))
                                  : (antiderivative - state.previousAntiderivative) / difference;

            state.previousInput = in;
            state.previousAntiderivative = antiderivative;

            samples[i] = (float) (shaped - shape (bias));
        }
    }
}

void LegacyExciterDsp::processChunk (float* const* channels, int numChannels, int offset, int numSamples)
{
    if (requestedOversampling != activeOversampling)
    {
        switchGain.setTarget (0.0);

        if (switchGain.isSettled())
        {
            selectOversampling (requestedOversampling);

            // Stay silent until the empty delay line has refilled and the cleared filters
            // have settled. Fading in sooner lets the signal restart abruptly, a click.
            switchHoldSamples = activeLatency + juce::roundToInt (switchSettleSeconds * sampleRate);
        }
    }
    else if (switchHoldSamples > 0)
    {
        switchHoldSamples -= numSamples;
    }
    else
    {
        // Also recovers when the setting returns to the active one during the fade-out.
        switchGain.setTarget (1.0);
    }

    const auto delay = activeLatency;

    for (int i = 0; i < numSamples; ++i)
    {
        if (! logFrequency.isSettled())
            updateFilter (logFrequency.next());

        chunkFilters[(size_t) i] = filter;

        const auto gain = boost.next();
        const auto writePosition = (delayWritePosition + i) % delayLength;

        for (int ch = 0; ch < 2; ++ch)
        {
            const auto raw = ch < numChannels ? channels[ch][offset + i] : 0.0f;
            const auto in = std::isfinite (raw) ? raw : 0.0f;

            auto& state = channelStates[(size_t) ch];
            state.dryDelay[(size_t) writePosition] = in;

            wetBuffer.setSample (ch, i, (float) (state.preFilter.tick (filter, (double) in).high * gain));
        }
    }

    auto block = juce::dsp::AudioBlock<float> (wetBuffer).getSubBlock (0, (size_t) numSamples);

    if (activeOversampling == off)
    {
        shapeBlock (block, numSamples);
    }
    else
    {
        auto& oversampler = *oversamplers[(size_t) activeOversampling - 1];
        const juce::dsp::AudioBlock<const float> constBlock (block);

        shapeBlock (oversampler.processSamplesUp (constBlock), numSamples);
        oversampler.processSamplesDown (block);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const auto amount = harmonics.next() * mixBack.next();
        const auto outputGain = output.next();
        const auto bypassAmount = bypass.next();
        const auto fade = switchGain.next();
        const auto readPosition = (delayWritePosition + i + delayLength - delay) % delayLength;

        for (int ch = 0; ch < 2; ++ch)
        {
            auto& state = channelStates[(size_t) ch];

            const auto post = state.postFilter.tick (chunkFilters[(size_t) i], (double) wetBuffer.getSample (ch, i)).high;
            const auto wet = state.lowPass.process (post);

            if (ch >= numChannels)
                continue;

            const auto dry = (double) state.dryDelay[(size_t) readPosition];
            const auto out = (dry + wet * amount) * outputGain;

            channels[ch][offset + i] = (float) ((out + (dry - out) * bypassAmount) * fade);
        }
    }

    delayWritePosition = (delayWritePosition + numSamples) % delayLength;
}
