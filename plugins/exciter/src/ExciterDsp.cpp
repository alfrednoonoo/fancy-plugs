#include "ExciterDsp.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double shaperLimit = 1.5;
constexpr double maxBias = 0.75;
constexpr double butterworthQ = 0.70710678118654752;
constexpr double adaaThreshold = 1.0e-6;

double biasFor (double character) noexcept
{
    return maxBias * (1.0 - character);
}

double followerCoefficient (double milliseconds, double sampleRate) noexcept
{
    return 1.0 - std::exp (-1.0 / (milliseconds * 0.001 * sampleRate));
}

// Phase 4 Wave 2, Punch (docs/exciter.md, Phase 4 Wave 2, section 10): the transient measure
// runs from 0 at a 2 dB jump of the fast follower over the slow one to 1 at 12 dB.
constexpr double punchDeadZoneDb = 2.0;
constexpr double punchFullDb = 12.0;
} // namespace

ExciterDsp::ExciterDsp() = default;
ExciterDsp::~ExciterDsp() = default;

double ExciterDsp::shape (double x) noexcept
{
    if (std::abs (x) >= shaperLimit)
        return std::copysign (1.0, x);

    return x - 4.0 / 27.0 * x * x * x;
}

double ExciterDsp::shapeAntiderivative (double x) noexcept
{
    const auto magnitude = std::abs (x);

    if (magnitude >= shaperLimit)
        return magnitude - 0.5625;

    const auto squared = x * x;
    return 0.5 * squared - squared * squared / 27.0;
}

void ExciterDsp::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    // Copy 0 is the top band's, copy 1 Band 2's (Phase 4 Wave 2).
    oversampling.prepare (sampleRate, maxChunk, 2, false);
    wetBuffer.setSize (2, maxChunk);
    wetBuffer2.setSize (2, maxChunk);

    delayLength = oversampling.getMaxLatency() + maxChunk + 1;

    for (auto& state : channelStates)
        state.dryDelay.assign ((size_t) delayLength, 0.0f);

    for (auto& state : channelStates)
    {
        state.lowPass.setButterworthLowPass (0.45 * sampleRate, sampleRate);
        state.lowPass2.setButterworthLowPass (0.45 * sampleRate, sampleRate);
    }

    logFrequency.setTime (30.0, sampleRate);
    character.setTime (30.0, sampleRate);
    boost.setTime (20.0, sampleRate);
    harmonics.setTime (20.0, sampleRate);
    mixBack.setTime (20.0, sampleRate);
    output.setTime (20.0, sampleRate);
    bypass.setTime (10.0, sampleRate);
    listenMix.setTime (10.0, sampleRate);

    // Phase 4 Wave 2.
    logFrequency2.setTime (30.0, sampleRate);
    harmonics2.setTime (20.0, sampleRate);
    punch.setTime (20.0, sampleRate);
    logHighCut.setTime (30.0, sampleRate);
    highCutMix.setTime (10.0, sampleRate);
    width.setTime (20.0, sampleRate);

    levelCoefficient = followerCoefficient (5.0, sampleRate);
    fastAttack = followerCoefficient (0.5, sampleRate);
    slowAttack = followerCoefficient (20.0, sampleRate);
    detectorRelease = followerCoefficient (100.0, sampleRate);

    reset();
}

void ExciterDsp::resetCore()
{
    oversampling.reset();

    for (auto& state : channelStates)
    {
        state.preFilter.reset();
        state.postFilter.reset();
        state.lowPass.reset();
        state.shaper = {};
        state.highCut.reset();
        std::fill (state.dryDelay.begin(), state.dryDelay.end(), 0.0f);
    }

    delayWritePosition = 0;

    resetBand2();
    resetPunch();
}

// Phase 4 Wave 2: Band 2 starts from silence, at its Freq 2.
void ExciterDsp::resetBand2()
{
    logFrequency2.snapToTarget();
    updateFilter2();

    for (auto& state : channelStates)
    {
        state.preFilter2.reset();
        state.postFilter2.reset();
        state.lowPass2.reset();
        state.shaper2 = {};
    }

    if (oversampling.getFactor() != off)
        oversampling.get (1).reset();
}

void ExciterDsp::resetPunch()
{
    detectorLevel = detectorFast = detectorSlow = 0.0;
}

void ExciterDsp::reset()
{
    resetCore();
}

void ExciterDsp::setTargets (const Targets& targets)
{
    logFrequency.setTarget (std::log (std::min (targets.frequencyHz, 0.45 * sampleRate)));
    boost.setTarget (targets.boostGain);
    harmonics.setTarget (targets.harmonics);
    mixBack.setTarget (targets.mixBackGain);
    character.setTarget (targets.character);
    output.setTarget (targets.outputGain);
    bypass.setTarget (targets.bypass ? 1.0 : 0.0);
    listenMix.setTarget (targets.listen ? 1.0 : 0.0);
    requestedOversampling = targets.oversampling;

    // Phase 4 Wave 2.
    logFrequency2.setTarget (std::log (std::min (targets.frequency2Hz, 0.45 * sampleRate)));
    harmonics2.setTarget (targets.harmonics2);
    punch.setTarget (targets.punch);
    width.setTarget (targets.width);

    highCutWanted = targets.highCutOn;
    logHighCut.setTarget (std::log (std::min (targets.highCutHz, 0.45 * sampleRate)));
    highCutMix.setTarget (highCutWanted ? 1.0 : 0.0);

    // A cut that isn't running starts where it's set, rather than gliding from where it was.
    if (! highCutRunning)
        logHighCut.snapToTarget();
}

void ExciterDsp::snapToTargets (const Targets& targets)
{
    setTargets (targets);

    logFrequency.snapTo (std::log (std::min (targets.frequencyHz, 0.45 * sampleRate)));
    boost.snapTo (targets.boostGain);
    harmonics.snapTo (targets.harmonics);
    mixBack.snapTo (targets.mixBackGain);
    character.snapTo (targets.character);
    output.snapTo (targets.outputGain);
    bypass.snapTo (targets.bypass ? 1.0 : 0.0);
    listenMix.snapTo (targets.listen ? 1.0 : 0.0);

    logFrequency2.snapToTarget();
    harmonics2.snapToTarget();
    punch.snapToTarget();
    width.snapToTarget();
    logHighCut.snapToTarget();
    highCutMix.snapToTarget();
    band2Running = punchRunning = highCutRunning = false;

    updateFilter (logFrequency.getCurrent());
    oversampling.select (targets.oversampling, false);
    resetCore();
}

void ExciterDsp::updateFilter (double logHz) noexcept
{
    filter.set (std::exp (logHz), butterworthQ, sampleRate);
}

// Band 2 runs from Freq 2 up to Freq, so Freq 2 stops at Freq.
void ExciterDsp::updateFilter2() noexcept
{
    filter2.set (std::exp (std::min (logFrequency2.getCurrent(), logFrequency.getCurrent())), butterworthQ, sampleRate);
}

// Phase 4 Wave 2, Punch: the added part's weight for one input sample, from a linked level on the
// top band's input. Up to +6 dB on attacks and -6 dB in sustains at 100 %.
double ExciterDsp::nextWeight (double level) noexcept
{
    const auto amount = punch.next();

    detectorLevel += (level - detectorLevel) * levelCoefficient;
    detectorFast += (detectorLevel - detectorFast) * (detectorLevel > detectorFast ? fastAttack : detectorRelease);
    detectorSlow += (detectorLevel - detectorSlow) * (detectorLevel > detectorSlow ? slowAttack : detectorRelease);

    const auto jumpDb = 20.0 * std::log10 ((detectorFast + 1.0e-9) / (detectorSlow + 1.0e-9));
    const auto transient = std::clamp ((jumpDb - punchDeadZoneDb) / (punchFullDb - punchDeadZoneDb), 0.0, 1.0);

    return std::clamp (1.0 + amount * (2.0 * transient - 0.5), 0.5, 2.0);
}

int ExciterDsp::getLatencySamples() const noexcept
{
    return oversampling.getLatency();
}

void ExciterDsp::process (float* const* channels, int numChannels, int numSamples)
{
    for (int done = 0; done < numSamples;)
    {
        const auto chunk = std::min (numSamples - done, maxChunk);
        processChunk (channels, numChannels, done, chunk);
        done += chunk;
    }
}

// The bias of each input sample, held across its oversampled samples. Both bands use it.
void ExciterDsp::fillBias (int baseSamples)
{
    for (int i = 0; i < baseSamples; ++i)
    {
        chunkBias[(size_t) i] = biasFor (character.getCurrent());
        character.next();
    }
}

void ExciterDsp::shapeBlock (juce::dsp::AudioBlock<float> block, int baseSamples, Adaa ChannelState::* shaper)
{
    const auto numSamples = (int) block.getNumSamples();
    const auto factor = std::max (1, numSamples / std::max (1, baseSamples));

    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
    {
        auto* samples = block.getChannelPointer (ch);
        auto& state = channelStates[ch].*shaper;

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

void ExciterDsp::processChunk (float* const* channels, int numChannels, int offset, int numSamples)
{
    if (oversampling.beginChunk (requestedOversampling, false, numSamples))
        resetCore();

    const auto delay = oversampling.getLatency();

    // Phase 4 Wave 2: Band 2, Punch, the High cut and Width run only while they do something.
    const auto wantBand2 = ! (harmonics2.isSettled() && harmonics2.getCurrent() == 0.0);

    if (wantBand2 && ! band2Running)
        resetBand2();

    band2Running = wantBand2;

    const auto wantPunch = ! (punch.isSettled() && punch.getCurrent() == 0.0);

    if (wantPunch && ! punchRunning)
        resetPunch();

    punchRunning = wantPunch;

    const auto wantHighCut = ! (highCutMix.isSettled() && highCutMix.getCurrent() == 0.0);

    if (wantHighCut && ! highCutRunning)
    {
        highCutFilter.set (std::exp (logHighCut.getCurrent()), butterworthQ, sampleRate);

        for (auto& state : channelStates)
            state.highCut.reset();
    }

    highCutRunning = wantHighCut;

    const auto widthRunning = numChannels > 1 && ! (width.isSettled() && width.getCurrent() == 1.0);

    for (int i = 0; i < numSamples; ++i)
    {
        auto moved = false;

        if (! logFrequency.isSettled())
        {
            updateFilter (logFrequency.next());
            moved = true;
        }

        chunkFilters[(size_t) i] = filter;

        if (band2Running)
        {
            if (! logFrequency2.isSettled())
            {
                logFrequency2.next();
                moved = true;
            }

            if (moved)
                updateFilter2();

            chunkFilters2[(size_t) i] = filter2;
        }

        const auto gain = boost.next();
        const auto writePosition = (delayWritePosition + i) % delayLength;
        auto level = 0.0;

        for (int ch = 0; ch < 2; ++ch)
        {
            const auto raw = ch < numChannels ? channels[ch][offset + i] : 0.0f;
            const auto in = std::isfinite (raw) ? raw : 0.0f;

            auto& state = channelStates[(size_t) ch];
            state.dryDelay[(size_t) writePosition] = in;

            const auto high = state.preFilter.tick (filter, (double) in).high;
            wetBuffer.setSample (ch, i, (float) (high * gain));

            // Band 2: from Freq 2 up to Freq, what the top band's high-pass leaves.
            if (band2Running)
                wetBuffer2.setSample (ch, i, (float) ((state.preFilter2.tick (filter2, (double) in).high - high) * gain));

            level = std::max (level, std::abs (high));
        }

        chunkWeight[(size_t) i] = punchRunning ? nextWeight (level) : 1.0;
    }

    fillBias (numSamples);

    auto block = juce::dsp::AudioBlock<float> (wetBuffer).getSubBlock (0, (size_t) numSamples);
    auto block2 = juce::dsp::AudioBlock<float> (wetBuffer2).getSubBlock (0, (size_t) numSamples);

    if (oversampling.getFactor() == off)
    {
        shapeBlock (block, numSamples, &ChannelState::shaper);

        if (band2Running)
            shapeBlock (block2, numSamples, &ChannelState::shaper2);
    }
    else
    {
        auto& oversampler = oversampling.get (0);
        const juce::dsp::AudioBlock<const float> constBlock (block);

        shapeBlock (oversampler.processSamplesUp (constBlock), numSamples, &ChannelState::shaper);
        oversampler.processSamplesDown (block);

        if (band2Running)
        {
            auto& oversampler2 = oversampling.get (1);
            const juce::dsp::AudioBlock<const float> constBlock2 (block2);

            shapeBlock (oversampler2.processSamplesUp (constBlock2), numSamples, &ChannelState::shaper2);
            oversampler2.processSamplesDown (block2);
        }
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const auto harmonicsAmount = harmonics.next();
        const auto mixBackGain = mixBack.next();
        auto amount = harmonicsAmount * mixBackGain;
        auto amount2 = band2Running ? harmonics2.next() * mixBackGain : 0.0;

        if (punchRunning)
        {
            amount *= chunkWeight[(size_t) i];
            amount2 *= chunkWeight[(size_t) i];
        }

        const auto outputGain = output.next();
        const auto bypassAmount = bypass.next();
        const auto fade = oversampling.nextFade();
        const auto listening = listenMix.isSettled() && listenMix.getCurrent() == 0.0 ? 0.0 : listenMix.next();
        const auto readPosition = (delayWritePosition + i + delayLength - delay) % delayLength;

        // What Exciter adds, per channel: the top band, then Band 2.
        std::array<double, 2> added {};

        for (int ch = 0; ch < 2; ++ch)
        {
            auto& state = channelStates[(size_t) ch];

            const auto post = state.postFilter.tick (chunkFilters[(size_t) i], (double) wetBuffer.getSample (ch, i)).high;
            const auto wet = state.lowPass.process (post);
            added[(size_t) ch] = wet * amount;

            if (ch == 0 && topBandTap != nullptr)
                topBandTap[offset + i] = (float) added[0];

            if (band2Running)
            {
                const auto post2 = state.postFilter2.tick (chunkFilters2[(size_t) i], (double) wetBuffer2.getSample (ch, i)).high;
                added[(size_t) ch] += state.lowPass2.process (post2) * amount2;
            }
        }

        // Phase 4 Wave 2: the High cut on the added part, faded in and out over 10 ms.
        if (highCutRunning)
        {
            if (! logHighCut.isSettled())
                highCutFilter.set (std::exp (logHighCut.next()), butterworthQ, sampleRate);

            const auto mix = highCutMix.next();

            for (int ch = 0; ch < 2; ++ch)
            {
                const auto low = channelStates[(size_t) ch].highCut.tick (highCutFilter, added[(size_t) ch]).low;
                added[(size_t) ch] += (low - added[(size_t) ch]) * mix;
            }
        }

        // Phase 4 Wave 2: Width scales the added part's side.
        if (widthRunning)
        {
            const auto mid = 0.5 * (added[0] + added[1]);
            const auto side = 0.5 * (added[0] - added[1]) * width.next();
            added[0] = mid + side;
            added[1] = mid - side;
        }

        for (int ch = 0; ch < std::min (numChannels, 2); ++ch)
        {
            const auto dry = (double) channelStates[(size_t) ch].dryDelay[(size_t) readPosition];
            auto out = (dry + added[(size_t) ch]) * outputGain;

            // Phase 4 Wave 1: Listen plays only the added part, before Output.
            if (listening > 0.0)
                out += (added[(size_t) ch] - out) * listening;

            channels[ch][offset + i] = (float) ((out + (dry - out) * bypassAmount) * fade);
        }
    }

    delayWritePosition = (delayWritePosition + numSamples) % delayLength;
}
