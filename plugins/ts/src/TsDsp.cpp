#include "TsDsp.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
constexpr double smoothingMs = 20.0, switchFadeMs = 5.0;
constexpr double butterworthQ = 0.70710678118654752;
constexpr double sidechainSilentDb = -90.0, mainActiveDb = -60.0, sidechainWarningSeconds = 2.0;

double coefficient (double ms, double rate) noexcept
{
    return 1.0 - std::exp (-1.0 / std::max (1.0e-9, ms * 0.001 * rate));
}

double sanitise (float sample) noexcept
{
    return std::isfinite (sample) ? (double) sample : 0.0;
}

double toDb (double gain) noexcept
{
    return 20.0 * std::log10 (std::max (gain, 1.0e-10));
}

int nextPowerOfTwo (int value) noexcept
{
    auto size = 1;

    while (size < value)
        size <<= 1;

    return size;
}
} // namespace

TsDsp::TsDsp() = default;

double TsDsp::softClip (double x) noexcept
{
    constexpr double ceiling = 1.0, knee = 0.5011872336272722;   // 6 dB under
    const auto a = std::abs (x);

    if (a <= knee)
        return x;

    const auto span = 2.0 * (ceiling - knee);
    const auto y = a < knee + span ? a - (a - knee) * (a - knee) / (2.0 * span) : ceiling;
    return x < 0.0 ? -y : y;
}

void TsDsp::prepare (double sampleRate)
{
    rate = sampleRate;

    for (int i = 0; i < 2; ++i)
    {
        oversamplers[(size_t) i] = std::make_unique<Oversampler> (2, (size_t) (i + 1), Oversampler::filterHalfBandPolyphaseIIR, true, true);
        oversamplers[(size_t) i]->initProcessing ((size_t) maxChunk);
        oversamplerLatency[(size_t) i] = juce::roundToInt (oversamplers[(size_t) i]->getLatencyInSamples());
    }

    const auto longest = (int) std::ceil (lookaheadMs.back() * 0.001 * rate) + oversamplerLatency[1] + maxChunk + 8;
    ringSize = nextPowerOfTwo (longest);
    ringMask = ringSize - 1;

    for (auto* rings : { &audioRing, &dryRing })
        for (auto& ring : *rings)
            ring.assign ((size_t) ringSize, 0.0f);

    for (auto* smoother : { &attack, &sustain, &pump, &highpassBlend, &lowpassBlend, &wet, &dry, &output })
        smoother->setTime (smoothingMs, rate);

    bypass.setTime (10.0, rate);
    listen.setTime (10.0, rate);
    switchGain.setTime (switchFadeMs / 5.0, rate);

    lastSpeedMs = lastLengthMs = -1.0;
    snapToTargets (current);
}

void TsDsp::clearDelays() noexcept
{
    for (auto* rings : { &audioRing, &dryRing })
        for (auto& ring : *rings)
            std::fill (ring.begin(), ring.end(), 0.0f);

    for (auto& oversampler : oversamplers)
        if (oversampler != nullptr)
            oversampler->reset();
}

void TsDsp::reset()
{
    clearDelays();

    for (auto& detector : detectors)
        detector.reset();

    for (auto& follower : followers)
        follower = Follower {};

    silentSidechainSeconds = 0.0;
    sidechainMissing.store (false, std::memory_order_relaxed);
}

void TsDsp::snapToTargets (const Targets& targets) noexcept
{
    current = targets;
    active = { std::clamp (targets.lookahead, 0, numLookaheads - 1), targets.clip, std::clamp (targets.oversampling, (int) osOff, (int) os4) };
    holdOffSamples = 0;

    attack.snapTo (targets.attackDb);
    sustain.snapTo (targets.sustainDb);
    pump.snapTo (targets.pump);
    highpassBlend.snapTo (targets.highpassOn ? 1.0 : 0.0);
    lowpassBlend.snapTo (targets.lowpassOn ? 1.0 : 0.0);
    wet.snapTo (targets.wetGain);
    dry.snapTo (targets.dryGain);
    output.snapTo (targets.outputGain);
    bypass.snapTo (targets.bypass ? 1.0 : 0.0);
    listen.snapTo (targets.listen ? 1.0 : 0.0);
    switchGain.snapTo (1.0);

    updateCoefficients();
    reset();
}

int TsDsp::latencyFor (int lookahead, bool clip, int oversampling) const noexcept
{
    const auto look = (int) std::lround (lookaheadMs[(size_t) std::clamp (lookahead, 0, numLookaheads - 1)] * 0.001 * rate);
    return look + (clip && oversampling != osOff ? oversamplerLatency[(size_t) (std::clamp (oversampling, 1, 2) - 1)] : 0);
}

void TsDsp::updateCoefficients() noexcept
{
    if (current.speedMs != lastSpeedMs || current.lengthMs != lastLengthMs)
    {
        lastSpeedMs = current.speedMs;
        lastLengthMs = current.lengthMs;

        envelopeRelease = coefficient (10.0, rate);
        fastAttack = coefficient (0.1, rate);
        fastRelease = coefficient (20.0, rate);
        slowAttack = coefficient (std::clamp (current.speedMs, 1.0, 50.0), rate);
        slowRelease = coefficient (20.0, rate);
        peakFall = 6.0 / rate;
        lengthSamples = std::max (1.0, std::clamp (current.lengthMs, 20.0, 1000.0) * 0.001 * rate);
    }

    const auto limit = 0.45 * rate;
    highpassCoefficients.set (std::min (std::max (current.highpassHz, 10.0), limit), butterworthQ, rate);
    lowpassCoefficients.set (std::min (std::max (current.lowpassHz, 10.0), limit), butterworthQ, rate);
}

// One follower's gain in dB for this sample (docs/ts.md, "Engine").
double TsDsp::gainDbFor (Follower& f, double x, double attackDb, double sustainDb, double pumpAmount, bool& hitStarted) noexcept
{
    f.envelope = x > f.envelope ? x : f.envelope + (x - f.envelope) * envelopeRelease;
    const auto level = toDb (f.envelope);

    f.fast += (level > f.fast ? fastAttack : fastRelease) * (level - f.fast);
    f.slow += (level > f.slow ? slowAttack : slowRelease) * (level - f.slow);
    f.peak = std::max (level, f.peak - peakFall);

    const auto span = 12.0 - 9.0 * std::clamp (current.sensitivity, 0.0, 1.0);
    const auto deadband = std::max (0.0, current.deadbandDb);
    const auto floorFade = std::clamp ((level - floorDb) / 10.0, 0.0, 1.0);

    const auto a = std::clamp ((f.fast - f.slow - deadband) / span, 0.0, 1.0) * floorFade;

    if (a > 0.0)
    {
        if (! f.inHit)
            hitStarted = true;

        f.inHit = true;
        f.since = 0;
    }
    else
    {
        f.inHit = false;
        ++f.since;
    }

    // Sustain acts from the hit until Length, in Contour's shape (Decisions 1 and 3 A).
    const auto u = std::min (1.0, (double) f.since / lengthSamples);
    const auto window = current.contour == sharp ? std::exp (-5.0 * u) * (1.0 - u)
                      : current.contour == smooth ? 0.5 + 0.5 * std::cos (std::numbers::pi * u)
                                                  : 1.0 - u;

    const auto s = std::clamp ((f.peak - f.fast - deadband) / span, 0.0, 1.0) * window * floorFade;

    f.pump = std::max (f.pump * (1.0 - 1.0 / lengthSamples), a);

    return attackDb * a + sustainDb * s - pumpAmount * 12.0 * f.pump * (1.0 - a);
}

void TsDsp::process (float* const* channels, int numChannels, const float* const* sidechain, int numSidechain, int numSamples) noexcept
{
    numChannels = std::clamp (numChannels, 0, 2);
    numSidechain = sidechain != nullptr ? std::clamp (numSidechain, 0, 2) : 0;

    if (numChannels == 0)
        return;

    for (int done = 0; done < numSamples;)
    {
        const auto chunk = std::min (numSamples - done, maxChunk);
        processChunk (channels, numChannels, sidechain, numSidechain, done, chunk);
        done += chunk;
    }
}

void TsDsp::processChunk (float* const* channels, int numChannels, const float* const* sidechain, int numSidechain, int offset, int numSamples) noexcept
{
    const auto settings = current;
    const Active wanted { std::clamp (settings.lookahead, 0, numLookaheads - 1), settings.clip, std::clamp (settings.oversampling, (int) osOff, (int) os4) };

    // A change of latency fades out, swaps and fades back in, as a deliberate setting.
    if (wanted.lookahead != active.lookahead || wanted.clip != active.clip || wanted.oversampling != active.oversampling)
    {
        switchGain.setTarget (0.0);

        if (switchGain.isSettled())
        {
            active = wanted;
            clearDelays();
            holdOffSamples = latencyFor (active);
        }
    }
    else if (holdOffSamples <= 0)
    {
        switchGain.setTarget (1.0);
    }

    updateCoefficients();

    attack.setTarget (settings.attackDb);
    sustain.setTarget (settings.sustainDb);
    pump.setTarget (std::clamp (settings.pump, 0.0, 1.0));
    highpassBlend.setTarget (settings.highpassOn ? 1.0 : 0.0);
    lowpassBlend.setTarget (settings.lowpassOn ? 1.0 : 0.0);
    wet.setTarget (settings.wetGain);
    dry.setTarget (settings.dryGain);
    output.setTarget (settings.outputGain);
    bypass.setTarget (settings.bypass ? 1.0 : 0.0);
    listen.setTarget (settings.listen ? 1.0 : 0.0);

    const auto lookSamples = (int) std::lround (lookaheadMs[(size_t) active.lookahead] * 0.001 * rate);
    const auto latency = latencyFor (active);
    const auto useSidechain = settings.detector == sidechainLR && numSidechain > 0;
    auto mainPeak = 0.0, sidechainPeak = 0.0;

    // 1. The detector, the gain, and the Lookahead-delayed audio with the gain on it.
    for (int i = 0; i < numSamples; ++i)
    {
        double in[2], key[2];

        for (int ch = 0; ch < 2; ++ch)
        {
            in[ch] = sanitise (channels[std::min (ch, numChannels - 1)][offset + i]);
            key[ch] = useSidechain ? sanitise (sidechain[std::min (ch, numSidechain - 1)][offset + i]) : 0.0;
            mainPeak = std::max (mainPeak, std::abs (in[ch]));
            sidechainPeak = std::max (sidechainPeak, std::abs (key[ch]));
        }

        double source[2] { in[0], in[1] };

        if (useSidechain)
        {
            source[0] = key[0];
            source[1] = key[1];
        }
        else if (settings.detector == mid || settings.detector == side)
        {
            source[0] = source[1] = settings.detector == mid ? 0.5 * (in[0] + in[1]) : 0.5 * (in[0] - in[1]);
        }

        const auto highBlend = highpassBlend.next();
        const auto lowBlend = lowpassBlend.next();
        double filtered[2];

        for (int ch = 0; ch < 2; ++ch)
            filtered[ch] = detectors[ch].filter (source[ch], highpassCoefficients, lowpassCoefficients, highBlend, lowBlend);

        const auto attackDb = attack.next();
        const auto sustainDb = sustain.next();
        const auto pumpAmount = pump.next();

        double gainDb[2];
        auto hit = false;

        if (settings.link)
        {
            gainDb[0] = gainDb[1] = gainDbFor (followers[0], std::max (std::abs (filtered[0]), std::abs (filtered[1])), attackDb, sustainDb, pumpAmount, hit);
        }
        else
        {
            for (int ch = 0; ch < 2; ++ch)
                gainDb[ch] = gainDbFor (followers[ch], std::abs (filtered[ch]), attackDb, sustainDb, pumpAmount, hit);
        }

        // Exactly 0 dB is exactly unity, so the defaults pass the audio through to the bit.
        const double gain[2] { gainDb[0] == 0.0 ? 1.0 : std::pow (10.0, gainDb[0] / 20.0),
                               gainDb[1] == 0.0 ? 1.0 : std::pow (10.0, gainDb[1] / 20.0) };

        const auto write = (int) (now & ringMask);
        const auto readShaped = (int) ((now - lookSamples) & ringMask);
        const auto readDry = (int) ((now - latency) & ringMask);
        double delayed[2];

        for (int ch = 0; ch < 2; ++ch)
        {
            audioRing[(size_t) ch][(size_t) write] = (float) in[ch];
            dryRing[(size_t) ch][(size_t) write] = (float) in[ch];
            delayed[ch] = (double) audioRing[(size_t) ch][(size_t) readShaped];
            delayedDry[(size_t) ch][(size_t) i] = dryRing[(size_t) ch][(size_t) readDry];
        }

        double out[2];

        if (settings.channels == midOnly || settings.channels == sideOnly)
        {
            const auto g = 0.5 * (gain[0] + gain[1]);
            const auto m = 0.5 * (delayed[0] + delayed[1]), s = 0.5 * (delayed[0] - delayed[1]);
            const auto newM = settings.channels == midOnly ? m * g : m;
            const auto newS = settings.channels == sideOnly ? s * g : s;
            out[0] = newM + newS;
            out[1] = newM - newS;
        }
        else
        {
            out[0] = delayed[0] * gain[0];
            out[1] = delayed[1] * gain[1];
        }

        shaped[0][(size_t) i] = (float) out[0];
        shaped[1][(size_t) i] = (float) out[1];

        // History columns.
        columnInput = std::max (columnInput, (float) toDb (std::max (std::abs (in[0]), std::abs (in[1]))));
        const auto shownGain = (float) (std::abs (gainDb[0]) >= std::abs (gainDb[1]) ? gainDb[0] : gainDb[1]);

        if (std::abs (shownGain) > std::abs (columnGain))
            columnGain = shownGain;

        columnHit = columnHit || hit;
        ++now;
    }

    // 2. Clip output, oversampled while Oversampling isn't Off (Decision 2 A).
    if (active.clip)
    {
        if (active.oversampling == osOff)
        {
            for (auto& channel : shaped)
                for (int i = 0; i < numSamples; ++i)
                    channel[(size_t) i] = (float) softClip ((double) channel[(size_t) i]);
        }
        else
        {
            float* pointers[] { shaped[0].data(), shaped[1].data() };
            juce::dsp::AudioBlock<float> block (pointers, 2, (size_t) numSamples);
            auto& oversampler = *oversamplers[(size_t) (active.oversampling - 1)];
            auto up = oversampler.processSamplesUp (block);

            for (size_t ch = 0; ch < 2; ++ch)
            {
                auto* data = up.getChannelPointer (ch);

                for (size_t j = 0; j < up.getNumSamples(); ++j)
                    data[j] = (float) softClip ((double) data[j]);
            }

            oversampler.processSamplesDown (block);

            // The downsampler can overshoot the curve's ceiling a little; nothing passes 0 dBFS.
            for (auto& channel : shaped)
                for (int i = 0; i < numSamples; ++i)
                    channel[(size_t) i] = std::clamp (channel[(size_t) i], -1.0f, 1.0f);
        }
    }

    // 3. Wet and Dry, Listen to change (the change itself), Output, bypass and the switch fade.
    for (int i = 0; i < numSamples; ++i)
    {
        const auto wetGain = wet.next();
        const auto dryGain = dry.next();
        const auto outputGain = output.next();
        const auto bypassAmount = bypass.next();
        const auto listenAmount = listen.next();
        const auto fade = switchGain.next();
        auto loudest = 0.0;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto processed = (double) shaped[(size_t) ch][(size_t) i];
            const auto dryIn = (double) delayedDry[(size_t) ch][(size_t) i];

            auto mixed = processed * wetGain + dryIn * dryGain;

            if (listenAmount > 0.0)
                mixed += listenAmount * ((processed - dryIn) - mixed);

            mixed *= outputGain;

            if (bypassAmount > 0.0)
                mixed += (dryIn - mixed) * bypassAmount;

            if (fade != 1.0)
                mixed *= fade;

            channels[ch][offset + i] = (float) mixed;
            loudest = std::max (loudest, std::abs (mixed));
        }

        columnOutput = std::max (columnOutput, (float) toDb (loudest));

        if (++columnFill >= std::max (1, (int) (historySeconds * rate / historyColumns.load (std::memory_order_relaxed))))
        {
            pushHistory (columnInput, columnOutput, columnGain, columnHit);
            columnInput = columnOutput = -200.0f;
            columnGain = 0.0f;
            columnHit = false;
            columnFill = 0;
        }

        if (holdOffSamples > 0)
            --holdOffSamples;
    }

    // A sidechain chosen but silent while the main input plays.
    const auto blockSeconds = (double) numSamples / rate;
    const auto wantsSidechain = settings.detector == sidechainLR;

    if (wantsSidechain && toDb (sidechainPeak) < sidechainSilentDb && toDb (mainPeak) > mainActiveDb)
        silentSidechainSeconds += blockSeconds;
    else if (! wantsSidechain || toDb (sidechainPeak) >= sidechainSilentDb)
        silentSidechainSeconds = 0.0;

    sidechainMissing.store (silentSidechainSeconds >= sidechainWarningSeconds, std::memory_order_relaxed);
}

void TsDsp::pushHistory (float inputDb, float outputDb, float gainDb, bool hit) noexcept
{
    const auto write = historyWrite.load (std::memory_order_relaxed);

    if (write - historyRead.load (std::memory_order_acquire) < historyCapacity)
    {
        history[(size_t) (write & (historyCapacity - 1))] = { inputDb, outputDb, gainDb, hit };
        historyWrite.store (write + 1, std::memory_order_release);
    }
}

int TsDsp::popHistory (HistoryColumn* destination, int maxColumns) noexcept
{
    const auto read = historyRead.load (std::memory_order_relaxed);
    const auto count = std::min (historyWrite.load (std::memory_order_acquire) - read, maxColumns);

    for (int i = 0; i < count; ++i)
        destination[i] = history[(size_t) ((read + i) & (historyCapacity - 1))];

    historyRead.store (read + count, std::memory_order_release);
    return count;
}
