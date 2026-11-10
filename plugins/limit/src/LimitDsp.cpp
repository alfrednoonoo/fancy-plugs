#include "LimitDsp.h"

#include <cmath>

namespace
{
constexpr double smoothingMs = 20.0, switchFadeMs = 10.0;

double dbToGain (double db) noexcept
{
    return std::pow (10.0, db / 20.0);
}

float peakDb (double gain) noexcept
{
    return gain > 1.0e-10 ? (float) (20.0 * std::log10 (gain)) : -200.0f;
}
} // namespace

fancy::dsp::TruePeakLimiter::Settings LimitDsp::limiterSettings (const Targets& targets, double rate, int factor, double baseRate) noexcept
{
    using Shape = fancy::dsp::TruePeakLimiter::Shape;

    fancy::dsp::TruePeakLimiter::Settings settings;

    // The maximum window keeps the latency a whole number of base-rate samples: the core's
    // delay is the window plus its 8-sample interpolator.
    const auto baseWindow = (int) std::lround (maxLookaheadMs / 1000.0 * baseRate);
    settings.maxWindow = factor * (baseWindow + 8) - 8;
    settings.minWindow = 1;
    settings.window = std::max (1, (int) std::lround (targets.lookaheadMs / 1000.0 * rate));

    const auto releaseScale = targets.style == dense ? 0.5 : 1.0;
    settings.releaseMs = targets.releaseMs * releaseScale;
    settings.slowReleaseMs = std::min (8.0 * targets.releaseMs, 2000.0) * releaseScale;
    settings.autoRelease = targets.autoRelease;
    settings.link = targets.link;
    settings.shape = targets.style == punchy ? Shape::punchy : Shape::smooth;
    settings.truePeak = targets.truePeak;
    settings.alignedSamplePeak = true;

    return settings;
}

fancy::dsp::TruePeakLimiter::Settings LimitDsp::guardSettings (const Targets& targets, double baseRate) noexcept
{
    fancy::dsp::TruePeakLimiter::Settings settings;
    settings.window = settings.maxWindow = std::max (1, (int) std::lround (0.001 * baseRate));
    settings.minWindow = 1;
    settings.releaseMs = 50.0;
    settings.shape = fancy::dsp::TruePeakLimiter::Shape::smooth;
    settings.truePeak = targets.truePeak;
    settings.alignedSamplePeak = true;
    settings.interpolatorTaps = 32;
    settings.interpolatorPhases = 7;
    return settings;
}

void LimitDsp::prepare (double sampleRate)
{
    rate = sampleRate;
    guard.prepare (sampleRate, guardSettings (current, sampleRate));

    for (int index = 0; index < 4; ++index)
    {
        const auto factor = 1 << index;
        limiters[(size_t) index].prepare (sampleRate * factor, limiterSettings (current, sampleRate * factor, factor, sampleRate));
    }

    for (int stages = 1; stages <= 3; ++stages)
    {
        for (int phase = 0; phase < 2; ++phase)
        {
            const auto index = (size_t) ((stages - 1) * 2 + phase);
            const auto linear = phase == 1;

            auto oversampler = std::make_unique<Oversampler> (2);
            oversampler->setUsingIntegerLatency (true);

            for (int stage = 0; stage < stages; ++stage)
            {
                const auto transition = stage == 0 ? 0.06f : 0.12f;
                const auto stopband = -100.0f + 10.0f * (float) stage;

                oversampler->addOversamplingStage (linear ? Oversampler::filterHalfBandFIREquiripple : Oversampler::filterHalfBandPolyphaseIIR,
                                                   transition, stopband, transition, stopband);
            }

            oversampler->initProcessing ((size_t) maxChunk);
            oversamplerLatencies[index] = juce::roundToInt (oversampler->getLatencyInSamples());
            oversamplers[index] = std::move (oversampler);
        }
    }

    auto longest = 0;

    for (int os = osOff; os <= os8; ++os)
        for (const auto linear : { false, true })
            longest = std::max (longest, latencyFor (os, linear));

    dryLength = longest + 1;

    for (auto& line : dryLines)
        line.assign ((size_t) dryLength, 0.0f);

    for (auto& buffer : scratch)
        buffer.assign ((size_t) maxChunk, 0.0f);

    loudness.prepare (sampleRate);

    for (auto* smoother : { &inputGain, &ceilingGain, &outputGain, &bypassMix })
        smoother->setTime (smoothingMs, sampleRate);

    switchGain.setTime (switchFadeMs / 5.0, sampleRate);

    active = { current.oversampling, current.linearPhase };
    reset();
}

int LimitDsp::latencyFor (int oversamplingIndex, bool linearPhase) const noexcept
{
    const auto factor = 1 << oversamplingIndex;
    const auto core = limiters[(size_t) oversamplingIndex].getLatencySamples() / factor + guard.getLatencySamples();

    if (oversamplingIndex == osOff)
        return core;

    return core + oversamplerLatencies[(size_t) ((oversamplingIndex - 1) * 2 + (linearPhase ? 1 : 0))];
}

LimitDsp::Oversampler* LimitDsp::oversamplerFor (const Active& a) const noexcept
{
    if (a.oversampling == osOff)
        return nullptr;

    return oversamplers[(size_t) ((a.oversampling - 1) * 2 + (a.linearPhase ? 1 : 0))].get();
}

void LimitDsp::reset()
{
    for (auto& limiter : limiters)
        limiter.reset();

    guard.reset();

    for (auto& oversampler : oversamplers)
        if (oversampler != nullptr)
            oversampler->reset();

    for (auto& line : dryLines)
        std::fill (line.begin(), line.end(), 0.0f);

    dryPosition = 0;
    holdOffSamples = 0;
    noiseState = 0x2545f491u;

    for (auto& error : ditherError)
        error.fill (0.0);

    loudness.requestReset();
    truePeakMeter.requestReset();

    snapToTargets (current);
}

void LimitDsp::snapToTargets (const Targets& targets) noexcept
{
    current = targets;
    active = { targets.oversampling, targets.linearPhase };

    const auto margin = targets.truePeak ? dbToGain (-truePeakMarginDb) : 1.0;
    inputGain.snapTo (dbToGain (-targets.thresholdDb));
    ceilingGain.snapTo (dbToGain (targets.ceilingDb) * margin);
    outputGain.snapTo (targets.outputGain);
    bypassMix.snapTo (targets.bypass ? 1.0 : 0.0);
    switchGain.snapTo (1.0);

    const auto factor = 1 << active.oversampling;
    limiterFor (active).setSettings (limiterSettings (targets, rate * factor, factor, rate));
    guard.setSettings (guardSettings (targets, rate));
}

void LimitDsp::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    for (int start = 0; start < numSamples; start += maxChunk)
    {
        float* pieces[2] { channels[0] + start, numChannels > 1 ? channels[1] + start : nullptr };
        processChunk (pieces, numChannels, std::min (maxChunk, numSamples - start));
    }
}

double LimitDsp::ditherSample (int channel, double x) noexcept
{
    if (current.dither == ditherOff)
        return x;

    const auto bits = current.dither <= dither16Shaped ? 16 : 24;
    const auto shaped = current.dither == dither16Shaped || current.dither == dither24Shaped;
    const auto lsb = std::ldexp (1.0, 1 - bits);

    const auto random = [this]
    {
        noiseState ^= noiseState << 13;
        noiseState ^= noiseState >> 17;
        noiseState ^= noiseState << 5;
        return (double) noiseState / 4294967296.0;
    };

    // TPDF, +-1 LSB. Shaped: second-order error feedback, (1 - z^-1)^2, pushing the noise up.
    auto& error = ditherError[(size_t) channel];
    const auto wanted = shaped ? x - (2.0 * error[0] - error[1]) : x;
    const auto noise = (random() + random() - 1.0) * lsb;
    const auto quantised = std::round ((wanted + noise) / lsb) * lsb;

    if (shaped)
    {
        error[1] = error[0];
        error[0] = quantised - wanted;
    }

    return quantised;
}

void LimitDsp::processChunk (float* const* channels, int numChannels, int numSamples) noexcept
{
    const auto settings = current;
    auto* left = channels[0];
    auto* right = numChannels > 1 ? channels[1] : nullptr;

    // A change of oversampling fades out, swaps and fades back in once the delays have refilled.
    if (isSwitching())
    {
        switchGain.setTarget (0.0);

        if (switchGain.isSettled())
        {
            active = { settings.oversampling, settings.linearPhase };

            if (auto* oversampler = oversamplerFor (active))
                oversampler->reset();

            limiterFor (active).reset();

            for (auto& line : dryLines)
                std::fill (line.begin(), line.end(), 0.0f);

            holdOffSamples = getLatencySamples();
        }
    }
    else if (holdOffSamples <= 0)
    {
        switchGain.setTarget (1.0);
    }

    const auto factor = 1 << active.oversampling;
    auto& limiter = limiterFor (active);
    limiter.setSettings (limiterSettings (settings, rate * factor, factor, rate));
    guard.setSettings (guardSettings (settings, rate));

    const auto margin = settings.truePeak ? dbToGain (-truePeakMarginDb) : 1.0;
    inputGain.setTarget (dbToGain (-settings.thresholdDb));
    ceilingGain.setTarget (dbToGain (settings.ceilingDb) * margin);
    outputGain.setTarget (settings.outputGain);
    bypassMix.setTarget (settings.bypass ? 1.0 : 0.0);

    // 1. Into the limiter's scale: the signal pushed up by -Threshold, over the ceiling's gain.
    std::array<double, maxChunk> ceilings {}, inputs {};
    std::array<float, maxChunk> reductions {};
    auto* a = scratch[0].data();
    auto* b = scratch[1].data();

    for (int i = 0; i < numSamples; ++i)
    {
        const auto gain = inputGain.next();
        const auto ceiling = ceilingGain.next();
        const auto l = (double) left[i] * gain, r = (right != nullptr ? (double) right[i] : (double) left[i]) * gain;

        ceilings[(size_t) i] = ceiling;
        inputs[(size_t) i] = std::max (std::abs (l), std::abs (r));
        a[i] = (float) (l / ceiling);
        b[i] = (float) (r / ceiling);
        reductions[(size_t) i] = 1.0f;
    }

    // 2. The limiter, at the oversampled rate. Listen keeps what it takes away instead.
    const auto limit = [&] (float& l, float& r, int baseIndex)
    {
        double x = l, y = r;
        double delayed[2];
        limiter.process (x, y, 1.0, delayed);

        if (settings.listen)
        {
            x = delayed[0] - x;
            y = delayed[1] - y;
        }

        l = (float) x;
        r = (float) y;

        const auto gain = (float) std::min (limiter.getLastGain (0), limiter.getLastGain (1));
        reductions[(size_t) baseIndex] = std::min (reductions[(size_t) baseIndex], gain);
    };

    if (auto* oversampler = oversamplerFor (active))
    {
        float* pointers[2] { a, b };
        juce::dsp::AudioBlock<float> block (pointers, 2, (size_t) numSamples);
        auto up = oversampler->processSamplesUp (block);
        auto* upLeft = up.getChannelPointer (0);
        auto* upRight = up.getChannelPointer (1);

        for (size_t j = 0; j < up.getNumSamples(); ++j)
            limit (upLeft[j], upRight[j], (int) j / factor);

        oversampler->processSamplesDown (block);
    }
    else
    {
        for (int i = 0; i < numSamples; ++i)
            limit (a[i], b[i], i);
    }

    // 3. The true-peak guard, back to the ceiling's scale, the final clip at Ceiling, then the meters.
    for (int i = 0; i < numSamples; ++i)
    {
        double l = a[i], r = b[i];
        guard.process (l, r, settings.listen ? 0.0 : 1.0);
        a[i] = (float) l;
        b[i] = (float) r;

        const auto gain = (float) std::min (guard.getLastGain (0), guard.getLastGain (1));
        reductions[(size_t) i] = std::min (reductions[(size_t) i], settings.listen ? 1.0f : gain);
    }

    auto smallestGain = 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto ceiling = ceilings[(size_t) i] / margin;
        auto l = (double) a[i] * ceilings[(size_t) i];
        auto r = (double) b[i] * ceilings[(size_t) i];

        if (! settings.listen)
        {
            l = std::clamp (l, -ceiling, ceiling);
            r = std::clamp (r, -ceiling, ceiling);
        }

        a[i] = (float) l;
        b[i] = (float) r;

        const auto output = std::max (std::abs (l), std::abs (r));
        smallestGain = std::min (smallestGain, reductions[(size_t) i]);

        // History columns, at the base rate.
        columnInput = std::max (columnInput, peakDb (inputs[(size_t) i]));
        columnOutput = std::max (columnOutput, peakDb (output));
        columnReduction = std::max (columnReduction, -peakDb (reductions[(size_t) i]));

        if (++columnFill >= std::max (1, (int) (historySeconds * rate / historyColumns.load (std::memory_order_relaxed))))
        {
            pushHistory (columnInput, columnOutput, columnReduction);
            columnInput = columnOutput = -200.0f;
            columnReduction = 0.0f;
            columnFill = 0;
        }
    }

    loudness.process (a, b, numSamples);
    truePeakMeter.process (a, b, numSamples);

    const auto reductionDb = -peakDb (smallestGain);
    auto shown = reduction.load (std::memory_order_relaxed);

    while (reductionDb > shown && ! reduction.compare_exchange_weak (shown, reductionDb, std::memory_order_relaxed))
    {
    }

    // 4. Dither, the Output knob, the oversampling fade and bypass against the delayed dry signal.
    const auto latency = getLatencySamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const auto fade = switchGain.next();
        const auto out = outputGain.next();
        const auto bypass = bypassMix.next();

        const auto readPosition = (dryPosition + dryLength - latency) % dryLength;
        const float dry[2] { dryLines[0][(size_t) readPosition], dryLines[1][(size_t) readPosition] };

        dryLines[0][(size_t) dryPosition] = left[i];
        dryLines[1][(size_t) dryPosition] = right != nullptr ? right[i] : left[i];
        dryPosition = (dryPosition + 1) % dryLength;

        const auto wetLeft = ditherSample (0, (double) a[i]) * out * fade;
        const auto wetRight = ditherSample (1, (double) b[i]) * out * fade;

        // Bypass sends the dry signal at its own level, with no fade during a switch.
        left[i] = (float) (wetLeft + ((double) dry[0] - wetLeft) * bypass);

        if (right != nullptr)
            right[i] = (float) (wetRight + ((double) dry[1] - wetRight) * bypass);

        if (holdOffSamples > 0)
            --holdOffSamples;
    }

}

void LimitDsp::pushHistory (float inputDb, float outputDb, float reductionDb) noexcept
{
    const auto write = historyWrite.load (std::memory_order_relaxed);
    history[(size_t) write] = { inputDb, outputDb, reductionDb };
    historyWrite.store ((write + 1) % historyCapacity, std::memory_order_release);
}

int LimitDsp::popHistory (HistoryColumn* destination, int maxColumns) noexcept
{
    const auto write = historyWrite.load (std::memory_order_acquire);
    auto read = historyRead.load (std::memory_order_relaxed);
    auto count = 0;

    while (read != write && count < maxColumns)
    {
        destination[count++] = history[(size_t) read];
        read = (read + 1) % historyCapacity;
    }

    historyRead.store (read, std::memory_order_relaxed);
    return count;
}
