#include <fancy/dsp/SwitchedOversampler.h>

namespace fancy::dsp
{

namespace
{
constexpr double switchFadeMs = 2.0;
constexpr double switchSettleSeconds = 0.003;
} // namespace

void SwitchedOversampler::prepare (double newSampleRate, int maxChunk, int copies, bool withMinimumPhase)
{
    sampleRate = newSampleRate;
    numCopies = juce::jlimit (1, maxCopies, copies);
    maxLatency = 0;

    for (int phase = 0; phase < (withMinimumPhase ? 2 : 1); ++phase)
    {
        const auto minimumPhase = phase == 1;

        for (int factor = 0; factor < numFactors; ++factor)
        {
            for (int copy = 0; copy < numCopies; ++copy)
            {
                auto oversampler = std::make_unique<Oversampler> (2);
                oversampler->setUsingIntegerLatency (true);

                for (int stage = 0; stage <= factor; ++stage)
                {
                    const auto transition = stage == 0 ? 0.06f : 0.12f;
                    const auto stopband = -100.0f + 10.0f * (float) stage;

                    oversampler->addOversamplingStage (minimumPhase ? Oversampler::filterHalfBandPolyphaseIIR
                                                                    : Oversampler::filterHalfBandFIREquiripple,
                                                       transition, stopband, transition, stopband);
                }

                oversampler->initProcessing ((size_t) maxChunk);

                const auto index = (size_t) indexFor (factor + 1, minimumPhase);
                latencies[index] = juce::roundToInt (oversampler->getLatencyInSamples());
                maxLatency = juce::jmax (maxLatency, latencies[index]);
                oversamplers[(size_t) copy][index] = std::move (oversampler);
            }
        }
    }

    fade.setTime (switchFadeMs, sampleRate);
}

int SwitchedOversampler::latencyFor (int factor, bool minimumPhase) const noexcept
{
    return factor == off ? 0 : latencies[(size_t) indexFor (factor, minimumPhase)];
}

SwitchedOversampler::Oversampler& SwitchedOversampler::get (int copy) noexcept
{
    return *oversamplers[(size_t) copy][(size_t) indexFor (activeFactor, activeMinimumPhase)];
}

void SwitchedOversampler::reset()
{
    if (activeFactor == off)
        return;

    for (int copy = 0; copy < numCopies; ++copy)
        if (auto& oversampler = oversamplers[(size_t) copy][(size_t) indexFor (activeFactor, activeMinimumPhase)])
            oversampler->reset();
}

void SwitchedOversampler::select (int factor, bool minimumPhase)
{
    activeFactor = factor;
    activeMinimumPhase = factor != off && minimumPhase;
    fade.snapTo (1.0);
    holdSamples = 0;
    reset();
}

bool SwitchedOversampler::beginChunk (int requestedFactor, bool requestedMinimumPhase, int numSamples)
{
    requestedMinimumPhase = requestedFactor != off && requestedMinimumPhase;

    if (requestedFactor != activeFactor || requestedMinimumPhase != activeMinimumPhase)
    {
        fade.setTarget (0.0);

        if (fade.isSettled())
        {
            activeFactor = requestedFactor;
            activeMinimumPhase = requestedMinimumPhase;
            reset();

            // Stay silent until the caller's emptied delay lines have refilled and its cleared
            // filters have settled. Fading in sooner lets the signal restart abruptly, a click.
            holdSamples = getLatency() + juce::roundToInt (switchSettleSeconds * sampleRate);
            return true;
        }
    }
    else if (holdSamples > 0)
    {
        holdSamples -= numSamples;
    }
    else
    {
        // Also recovers when the setting returns to the active one during the fade-out.
        fade.setTarget (1.0);
    }

    return false;
}

} // namespace fancy::dsp
