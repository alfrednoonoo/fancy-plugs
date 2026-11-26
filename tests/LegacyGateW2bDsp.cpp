#include "LegacyGateW2bDsp.h"

#include <fancy/dsp/Compression.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
constexpr double butterworthQ = 0.70710678118654752;
constexpr double parameterMs = 20.0, filterMs = 30.0, preOpenSmoothingMs = 50.0;
constexpr double switchFadeMs = 5.0;
constexpr double highpassOffEdgeHz = 20.5, lowpassOffEdgeHz = 21900.0;
constexpr double sidechainSilentDb = -90.0, mainActiveDb = -60.0, sidechainWarningSeconds = 2.0;
constexpr int fixedVelocity = 100;

double sanitise (float sample) noexcept
{
    return std::isfinite (sample) ? (double) sample : 0.0;
}

int nextPowerOfTwo (int value) noexcept
{
    auto size = 1;

    while (size < value)
        size <<= 1;

    return size;
}

float whiteNoise (std::uint32_t& state) noexcept
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;

    return (float) ((double) state / 2147483648.0 - 1.0);
}

double peakDb (double peak) noexcept
{
    return 20.0 * std::log10 (std::max (peak, 1.0e-10));
}
} // namespace

// Machine ---------------------------------------------------------------------------------

LegacyGateW2bDsp::Machine::Event LegacyGateW2bDsp::Machine::step (double levelDb, double openDb, double closeDb, double attackStep, double releaseStep, int holdSamples) noexcept
{
    // A re-trigger needs the level to have been under Threshold since the last opening.
    if (levelDb < openDb)
        armed = true;

    const auto trigger = armed && levelDb >= openDb;

    switch (state)
    {
        case closed:
            if (trigger)
            {
                state = opening;
                armed = false;
                return opened;
            }

            break;

        case closing:
            if (trigger)
            {
                state = opening;
                armed = false;
                return retriggered;
            }

            position -= releaseStep;

            if (position <= 0.0)
            {
                position = 0.0;
                state = closed;
                return shut;
            }

            break;

        case opening:
            position += attackStep;

            if (position >= 1.0)
            {
                position = 1.0;
                state = open;
            }

            break;

        case open:
            if (levelDb < closeDb)
            {
                state = hold;
                holdLeft = holdSamples;
            }

            break;

        case hold:
            if (trigger)
            {
                state = open;
                armed = false;
            }
            else if (levelDb >= closeDb)
            {
                state = open;
            }
            else if (--holdLeft <= 0)
            {
                state = closing;
            }

            break;
    }

    return none;
}

LegacyGateW2bDsp::Machine::Event LegacyGateW2bDsp::Machine::stepTransient (bool hit, double attackStep, double releaseStep, int holdSamples) noexcept
{
    // A hit opens it, or re-opens it from where it is, with no jump.
    if (hit)
    {
        const auto wasClosed = state == closed;
        state = opening;
        holdLeft = holdSamples;
        return wasClosed ? opened : retriggered;
    }

    switch (state)
    {
        case opening:
            position += attackStep;

            if (position >= 1.0)
            {
                position = 1.0;
                state = hold;
            }

            break;

        case open:
        case hold:
            // Hold runs out whatever the level, so a held sound can't keep it open.
            if (--holdLeft <= 0)
                state = closing;

            break;

        case closing:
            position -= releaseStep;

            if (position <= 0.0)
            {
                position = 0.0;
                state = closed;
                return shut;
            }

            break;

        case closed:
            break;
    }

    return none;
}

// Helpers ---------------------------------------------------------------------------------

int LegacyGateW2bDsp::velocityFor (double peakDbValue) noexcept
{
    const auto amount = std::clamp ((peakDbValue + 60.0) / 60.0, 0.0, 1.0);
    return std::clamp (1 + (int) std::lround (amount * 126.0), 1, 127);
}

double LegacyGateW2bDsp::noteLatenessMs (const Targets& targets) noexcept
{
    if (! targets.midi || ! targets.velocityFromLevel)
        return 0.0;

    if (! targets.lookahead)
        return targets.velocityWindowMs;

    return std::max (0.0, targets.velocityWindowMs - (lookaheadMs - targets.preOpenMs));
}

// Setup -------------------------------------------------------------------------------------

void LegacyGateW2bDsp::prepare (double sampleRate)
{
    rate = sampleRate;
    trigger.prepare (sampleRate);
    trigger.setPulse (true);
    lookaheadSamples = (int) std::lround (lookaheadMs * 0.001 * rate);
    ringSize = nextPowerOfTwo (lookaheadSamples + 8);
    ringMask = ringSize - 1;

    for (int ch = 0; ch < 2; ++ch)
    {
        audioRing[ch].assign ((size_t) ringSize, 0.0f);
        filteredRing[ch].assign ((size_t) ringSize, 0.0f);
        gainRing[ch].assign ((size_t) ringSize, 0.0f);
    }

    openRing.assign ((size_t) ringSize, 0.0f);

    for (auto* smoother : { &logHighpass, &logLowpass, &highpassBlend, &lowpassBlend, &rms })
        smoother->setTime (filterMs, rate);

    for (auto* smoother : { &wet, &dry, &noise, &output, &curveRange })
        smoother->setTime (parameterMs, rate);

    duckBlend.setTime (switchFadeMs / 5.0, rate);
    upBlend.setTime (switchFadeMs / 5.0, rate);

    bypass.setTime (10.0, rate);
    switchGain.setTime (switchFadeMs / 5.0, rate);
    preOpen.setTime (preOpenSmoothingMs, rate);

    reset();
}

void LegacyGateW2bDsp::clearRings() noexcept
{
    for (int ch = 0; ch < 2; ++ch)
    {
        std::fill (audioRing[ch].begin(), audioRing[ch].end(), 0.0f);
        std::fill (filteredRing[ch].begin(), filteredRing[ch].end(), 0.0f);
        std::fill (gainRing[ch].begin(), gainRing[ch].end(), 0.0f);
    }

    std::fill (openRing.begin(), openRing.end(), 0.0f);
}

void LegacyGateW2bDsp::reset()
{
    heldNotes = 0;
    numIncomingNotes = 0;
    trigger.reset();
    expanderDb[0] = expanderDb[1] = 0.0;

    for (int ch = 0; ch < 2; ++ch)
    {
        transientLevel[ch] = transientAverage[ch] = 0.0;
        transientArmed[ch] = true;
        boostDb[ch] = heldBoostDb[ch] = 0.0;
        boostHoldLeft[ch] = 0;
    }

    for (auto& bleed : bleedLevels)
        bleed.reset();

    clearRings();

    for (int ch = 0; ch < 2; ++ch)
    {
        detectors[ch].reset();
        machines[ch].reset();
        cachedPosition[ch] = -1.0;
    }

    queued = 0;
    noteSounding = false;
    velocityPending = false;
    lastOnTime = -1;
    silentSidechainSeconds = 0.0;
    sidechainMissing.store (false);
}

void LegacyGateW2bDsp::setTargets (const Targets& targets) noexcept
{
    current = targets;

    trigger.setStep (targets.syncStepBeats);
    trigger.setShape (targets.syncShape);

    const auto limit = 0.45 * rate;

    logHighpass.setTarget (std::log (std::min (targets.highpassHz, limit)));
    logLowpass.setTarget (std::log (std::min (targets.lowpassHz, limit)));
    highpassBlend.setTarget (targets.highpassHz <= highpassOffEdgeHz ? 0.0 : 1.0);
    lowpassBlend.setTarget (targets.lowpassHz >= lowpassOffEdgeHz ? 0.0 : 1.0);
    rms.setTarget (targets.rmsMs);

    wet.setTarget (targets.wetGain);
    dry.setTarget (targets.dryGain);
    noise.setTarget (targets.noiseGain);
    output.setTarget (targets.outputGain);
    bypass.setTarget (targets.bypass ? 1.0 : 0.0);
    preOpen.setTarget (std::clamp (targets.preOpenMs, 0.0, lookaheadMs) * 0.001 * rate);
    curveRange.setTarget (targets.rangeDb <= rangeFloorDb ? 0.0 : std::pow (10.0, std::min (0.0, targets.rangeDb) / 20.0));
    duckBlend.setTarget (targets.duck ? 1.0 : 0.0);
    upBlend.setTarget (targets.upward ? 1.0 : 0.0);
}

void LegacyGateW2bDsp::snapToTargets (const Targets& targets) noexcept
{
    setTargets (targets);

    for (auto* smoother : { &logHighpass, &logLowpass, &highpassBlend, &lowpassBlend, &rms, &wet, &dry, &noise, &output, &bypass, &preOpen,
                            &curveRange, &duckBlend, &upBlend })
        smoother->snapToTarget();

    activeLookahead = targets.lookahead;
    holdOffSamples = 0;
    switchGain.snapTo (1.0);
    cachedPosition[0] = cachedPosition[1] = -1.0;
    updateCurveRangeDb();

    updateFilters();
    rmsCoefficient = fancy::dsp::timeCoefficient (rms.getCurrent(), rate);
}

void LegacyGateW2bDsp::updateFilters() noexcept
{
    highpassCoefficients.set (std::exp (logHighpass.getCurrent()), butterworthQ, rate);
    lowpassCoefficients.set (std::exp (logLowpass.getCurrent()), butterworthQ, rate);
}

// Gain --------------------------------------------------------------------------------------

void LegacyGateW2bDsp::updateCurveRangeDb() noexcept
{
    const auto gain = curveRange.getCurrent();
    curveRangeDb = gain >= 1.0 ? 0.0 : std::max (curveFloorDb, 20.0 * std::log10 (std::max (gain, 1.0e-12)));
}

double LegacyGateW2bDsp::gainFor (double position) noexcept
{
    const auto range = curveRangeDb;
    const auto blend = duckBlend.getCurrent();

    // Fully shut at -inf is exact silence, not -120 dB.
    if (curveRange.isSettled() && curveRange.getCurrent() <= 0.0 && duckBlend.isSettled()
        && ((blend == 0.0 && position <= 0.0) || (blend == 1.0 && position >= 1.0)))
        return 0.0;

    const auto shaped = 0.5 - 0.5 * std::cos (std::numbers::pi * position);
    const auto toGain = [] (double db) { return db == 0.0 ? 1.0 : std::pow (10.0, db / 20.0); };

    const auto gateGain = blend < 1.0 ? toGain (range * (1.0 - shaped)) : 0.0;
    const auto duckGain = blend > 0.0 ? toGain (range * shaped) : 0.0;

    return gateGain + (duckGain - gateGain) * blend;
}

// MIDI --------------------------------------------------------------------------------------

void LegacyGateW2bDsp::schedule (std::int64_t time, bool on, int velocity) noexcept
{
    if (queued >= maxQueuedEvents)
        return;

    // Kept in time order.
    auto index = queued;

    while (index > 0 && queue[(size_t) index - 1].time > time)
    {
        queue[(size_t) index] = queue[(size_t) index - 1];
        --index;
    }

    queue[(size_t) index] = { time, on, activeNote, activeChannel, velocity };
    ++queued;
}

void LegacyGateW2bDsp::midiEvent (Machine::Event event, std::int64_t detectorTime) noexcept
{
    // Where the change is heard: Pre-open ahead of the delayed audio with Lookahead on.
    const auto heardOffset = activeLookahead ? (std::int64_t) (lookaheadSamples - (int) std::lround (preOpen.getCurrent())) : 0;
    const auto heard = detectorTime + heardOffset;

    if (event == Machine::opened || event == Machine::retriggered)
    {
        if (noteSounding || velocityPending)
        {
            schedule (std::max (heard, lastOnTime + 1), false, 0);
            noteSounding = false;
            velocityPending = false;
        }

        activeNote = current.note;
        activeChannel = current.channel;

        if (current.velocityFromLevel)
        {
            velocityPending = true;
            velocityPeak = 0.0;
            velocityHeard = heard;
            velocityDue = detectorTime + (std::int64_t) std::max (1L, std::lround (current.velocityWindowMs * 0.001 * rate));
        }
        else
        {
            schedule (heard, true, fixedVelocity);
            lastOnTime = heard;
            noteSounding = true;
        }
    }
    else if (event == Machine::shut)
    {
        // A note still waiting for its velocity goes out first, then its note-off.
        if (velocityPending)
        {
            const auto onTime = std::max (velocityHeard, velocityDue);
            schedule (onTime, true, velocityFor (peakDb (velocityPeak)));
            lastOnTime = onTime;
            velocityPending = false;
            noteSounding = true;
        }

        if (noteSounding)
        {
            schedule (std::max (heard, lastOnTime + 1), false, 0);
            noteSounding = false;
        }
    }
}

void LegacyGateW2bDsp::flushEvents (juce::MidiBuffer* midi, std::int64_t blockStart, int numSamples) noexcept
{
    auto sent = 0;

    while (sent < queued && queue[(size_t) sent].time < blockStart + numSamples)
    {
        const auto& event = queue[(size_t) sent];
        const auto offset = (int) std::clamp<std::int64_t> (event.time - blockStart, 0, numSamples - 1);

        if (midi != nullptr)
        {
            if (event.on)
                midi->addEvent (juce::MidiMessage::noteOn (event.channel, event.note, (juce::uint8) event.velocity), offset);
            else
                midi->addEvent (juce::MidiMessage::noteOff (event.channel, event.note), offset);
        }

        ++sent;
    }

    if (sent > 0)
    {
        std::copy (queue.begin() + sent, queue.begin() + queued, queue.begin());
        queued -= sent;
    }
}

// History -----------------------------------------------------------------------------------

void LegacyGateW2bDsp::pushHistory (float levelDb, float gainDb) noexcept
{
    columnLevel = std::max (columnLevel, levelDb);
    columnGain = std::min (columnGain, gainDb);

    const auto columnSamples = std::max (1, (int) (historySeconds * rate / historyColumns.load (std::memory_order_relaxed)));

    if (++columnFill < columnSamples)
        return;

    const auto write = historyWrite.load (std::memory_order_relaxed);

    if (write - historyRead.load (std::memory_order_acquire) < historyCapacity)
    {
        history[(size_t) (write & (historyCapacity - 1))] = { columnLevel, columnGain };
        historyWrite.store (write + 1, std::memory_order_release);
    }

    columnFill = 0;
    columnLevel = -200.0f;
    columnGain = 0.0f;
}

int LegacyGateW2bDsp::popHistory (HistoryColumn* destination, int maxColumns) noexcept
{
    auto read = historyRead.load (std::memory_order_relaxed);
    const auto available = historyWrite.load (std::memory_order_acquire) - read;
    const auto count = std::min (available, maxColumns);

    for (int i = 0; i < count; ++i)
        destination[i] = history[(size_t) ((read + i) & (historyCapacity - 1))];

    historyRead.store (read + count, std::memory_order_release);
    return count;
}

// Process -----------------------------------------------------------------------------------

void LegacyGateW2bDsp::setIncomingNotes (const IncomingNote* notes, int count) noexcept
{
    numIncomingNotes = std::clamp (count, 0, (int) maxIncomingNotes);

    for (int i = 0; i < numIncomingNotes; ++i)
        incomingNotes[(size_t) i] = notes[i];
}

void LegacyGateW2bDsp::process (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int numSamples, juce::MidiBuffer* midi) noexcept
{
    const auto blockStart = now;
    const auto settings = current;

    const auto minimumFade = std::max (1, (int) std::min (32.0, std::round (0.0005 * rate)));
    const auto attackSamples = std::max ((double) minimumFade, settings.attackMs * 0.001 * rate);
    const auto releaseSamples = std::max ((double) minimumFade, settings.releaseMs * 0.001 * rate);
    const auto holdSamples = (int) std::lround (settings.holdMs * 0.001 * rate);
    const auto openDb = settings.thresholdDb;
    const auto closeDb = settings.thresholdDb + settings.hysteresisDb;
    const auto usesSidechain = settings.detector >= sideLR && settings.detector <= sideR;
    const auto channelMode = settings.detector % 3;

    // Phase 4 Wave 1: each is skipped entirely at its default.
    const auto midSide = settings.detector == mid || settings.detector == side;
    const auto hostSyncing = settings.detector == hostSync;

    // Phase 4 Wave 2, v1.7.1. The MIDI trigger and Host sync bypass the detector, so Transient
    // has no effect with them. Ratio is Gate mode's, on the Level detector.
    const auto transienting = settings.transient && ! settings.midiIn && ! hostSyncing;
    const auto expanding = settings.ratio < infiniteRatio && ! settings.duck && ! settings.upward && ! transienting;
    const auto upSlope = settings.upRatio - 1.0;
    const auto transientAttack = 1.0 - std::exp (-1.0 / (0.001 * rate));
    const auto transientRelease = 1.0 - std::exp (-1.0 / (0.020 * rate));
    const auto transientAverageStep = 1.0 - std::exp (-1.0 / (0.050 * rate));
    const auto deBleeding = settings.deBleed > 0.0 && ! hostSyncing;
    const auto expanderSlope = settings.ratio - 1.0;
    const auto expanderFloorDb = settings.rangeDb <= rangeFloorDb ? curveFloorDb : settings.rangeDb;
    const auto expanderAttack = 1.0 - std::exp (-1.0 / attackSamples);
    const auto expanderRelease = 1.0 - std::exp (-1.0 / releaseSamples);
    auto nextNote = 0;

    if (! settings.midiIn)
        heldNotes = 0;
    const auto numChannels = std::min (numMain, 2);

    // Lookahead changes fade out, swap and fade back in, as a deliberate setting.
    if (settings.lookahead != activeLookahead)
    {
        switchGain.setTarget (0.0);

        if (switchGain.isSettled())
        {
            // A sounding note ends here, so the change can't leave one stuck.
            if (noteSounding || velocityPending)
            {
                schedule (std::max (now, lastOnTime + 1), false, 0);
                noteSounding = false;
                velocityPending = false;
            }

            activeLookahead = settings.lookahead;
            clearRings();
            holdOffSamples = activeLookahead ? lookaheadSamples : 0;
        }
    }
    else if (holdOffSamples <= 0)
    {
        switchGain.setTarget (1.0);
    }

    auto mainPeak = 0.0, sidechainPeak = 0.0, reductionPeak = 0.0;
    const auto delay = activeLookahead ? lookaheadSamples : 0;

    for (int i = 0; i < numSamples; ++i)
    {
        if (! logHighpass.isSettled() || ! logLowpass.isSettled())
        {
            logHighpass.next();
            logLowpass.next();
            updateFilters();
        }

        if (! rms.isSettled())
            rmsCoefficient = fancy::dsp::timeCoefficient (rms.next(), rate);

        const auto highBlend = highpassBlend.next();
        const auto lowBlend = lowpassBlend.next();
        const auto preOpenSamples = (int) std::lround (preOpen.next());
        const auto curveMoving = ! curveRange.isSettled() || ! duckBlend.isSettled();
        if (! curveRange.isSettled())
        {
            curveRange.next();
            updateCurveRangeDb();
        }

        duckBlend.next();
        const auto upAmount = upBlend.next();

        double in[2], key[2];

        for (int ch = 0; ch < 2; ++ch)
        {
            in[ch] = sanitise (main[std::min (ch, numMain - 1)][i]);
            key[ch] = numSidechain > 0 ? sanitise (sidechain[std::min (ch, numSidechain - 1)][i]) : 0.0;

            mainPeak = std::max (mainPeak, std::abs (in[ch]));
            sidechainPeak = std::max (sidechainPeak, std::abs (key[ch]));
        }

        const auto* source = usesSidechain ? key : in;
        double detectorIn[2] { channelMode == 2 ? source[1] : source[0], channelMode == 1 ? source[0] : source[1] };

        if (midSide)
            detectorIn[0] = detectorIn[1] = settings.detector == mid ? 0.5 * (in[0] + in[1]) : 0.5 * (in[0] - in[1]);

        double filtered[2], level[2];

        // Phase 4 Wave 2: Host sync's pulse goes straight in, past the filters (they're for audio).
        // One sample at a time, so the grid lands to the sample whatever the block size.
        if (hostSyncing)
        {
            float pulse = 0.0f;
            trigger.process (&pulse, 1);
            detectorIn[0] = detectorIn[1] = (double) pulse;
        }

        for (int ch = 0; ch < 2; ++ch)
        {
            filtered[ch] = hostSyncing ? detectorIn[ch] : detectors[ch].filter (detectorIn[ch], highpassCoefficients, lowpassCoefficients, highBlend, lowBlend);
            level[ch] = detectors[ch].levelDb (filtered[ch], rmsCoefficient);
        }

        const auto loudest = std::max (level[0], level[1]);
        const auto numMachines = settings.link ? 1 : 2;

        // De-bleed: the threshold rises by how much louder the sound outside the detector's band
        // is than inside it, scaled by De-bleed.
        double raise[2] { 0.0, 0.0 };

        if (deBleeding)
        {
            double outside[2];

            for (int ch = 0; ch < 2; ++ch)
                outside[ch] = bleedLevels[ch].levelDb (detectorIn[ch] - filtered[ch], rmsCoefficient);

            for (int ch = 0; ch < 2; ++ch)
                raise[ch] = settings.deBleed * std::max (0.0, settings.link ? std::max (outside[0], outside[1]) - loudest : outside[ch] - level[ch]);
        }

        // The MIDI trigger: a held note opens the gate, its release closes it.
        if (settings.midiIn)
        {
            while (nextNote < numIncomingNotes && incomingNotes[(size_t) nextNote].offset <= i)
            {
                heldNotes = incomingNotes[(size_t) nextNote].on ? heldNotes + 1 : std::max (0, heldNotes - 1);
                ++nextNote;
            }
        }

        for (int m = 0; m < numMachines; ++m)
        {
            auto heard = settings.link ? loudest : level[m];

            if (settings.midiIn)
                heard = heldNotes > 0 ? 200.0 : -400.0;

            Machine::Event event;

            if (transienting)
            {
                // Transient: a hit is a rise of Rise dB over the level's own 50 ms average, with
                // the level over Threshold as a floor. It re-arms once the rise has passed.
                const auto x = settings.link ? std::max (std::abs (filtered[0]), std::abs (filtered[1])) : std::abs (filtered[m]);
                auto& follower = transientLevel[m];
                follower += (x - follower) * (x > follower ? transientAttack : transientRelease);
                transientAverage[m] += (x - transientAverage[m]) * transientAverageStep;

                const auto followerDb = 20.0 * std::log10 (std::max (follower, 1.0e-15));
                const auto riseDb = followerDb - 20.0 * std::log10 (std::max (transientAverage[m], 1.0e-15));

                if (riseDb < 0.5 * settings.riseDb)
                    transientArmed[m] = true;

                const auto hit = transientArmed[m] && riseDb >= settings.riseDb && followerDb >= openDb + raise[m];

                if (hit)
                    transientArmed[m] = false;

                event = machines[m].stepTransient (hit, 1.0 / attackSamples, 1.0 / releaseSamples, holdSamples);
            }
            else
            {
                // Upward has no hysteresis: it boosts while the level is over Threshold.
                const auto closeAt = settings.upward ? openDb : closeDb;
                event = machines[m].step (heard, openDb + raise[m], closeAt + raise[m],
                                          1.0 / attackSamples, 1.0 / releaseSamples, holdSamples);
            }

            // MIDI follows the linked state, or the left channel's when unlinked.
            if (m == 0 && settings.midi && event != Machine::none)
                midiEvent (event, now);
        }

        if (velocityPending)
        {
            velocityPeak = std::max ({ velocityPeak, std::abs (filtered[0]), std::abs (filtered[1]) });

            if (now + 1 >= velocityDue)
            {
                const auto onTime = std::max (velocityHeard, now + 1);
                schedule (onTime, true, velocityFor (peakDb (velocityPeak)));
                lastOnTime = onTime;
                velocityPending = false;
                noteSounding = true;
            }
        }

        // The gain curve, written into the rings with the detector's timing.
        const auto writeAt = (int) (now & ringMask);

        for (int ch = 0; ch < 2; ++ch)
        {
            auto& machine = machines[settings.link ? 0 : ch];

            if (machine.position != cachedPosition[ch] || curveMoving)
            {
                cachedPosition[ch] = machine.position;
                cachedGain[ch] = gainFor (machine.position);
            }

            // Ratio below inf: an expander, the level falling (ratio - 1) dB for each dB under
            // Threshold, never deeper than Range, on Attack and Release (Gate mode only).
            if (expanding)
            {
                auto heard = settings.link ? loudest : level[ch];

                if (settings.midiIn)
                    heard = heldNotes > 0 ? 200.0 : -400.0;

                const auto target = std::max (expanderFloorDb, std::min (0.0, (heard - (openDb + raise[settings.link ? 0 : ch])) * expanderSlope));
                auto& current = expanderDb[ch];
                current += (target - current) * (target > current ? expanderAttack : expanderRelease);
                cachedGain[ch] = std::pow (10.0, current / 20.0);
                cachedPosition[ch] = -1.0;
            }

            // Upward: above Threshold the level is raised by (level - Threshold) x (Up ratio - 1)
            // dB, at most Boost, on Attack and Release, and held for Hold once it drops under.
            // With Transient, each hit is lifted by Boost along the machine's curve.
            if (upAmount > 0.0)
            {
                const auto index = settings.link ? 0 : ch;
                double upDb;

                if (transienting)
                {
                    upDb = settings.boostDb * (0.5 - 0.5 * std::cos (std::numbers::pi * machine.position));
                }
                else
                {
                    auto heard = settings.link ? loudest : level[ch];

                    if (settings.midiIn)
                        heard = heldNotes > 0 ? 200.0 : -400.0;

                    const auto over = heard - (openDb + raise[index]);
                    double want;

                    if (over >= 0.0)
                    {
                        want = std::min (settings.boostDb, over * upSlope);
                        heldBoostDb[ch] = want;
                        boostHoldLeft[ch] = holdSamples;
                    }
                    else if (boostHoldLeft[ch] > 0)
                    {
                        --boostHoldLeft[ch];
                        want = heldBoostDb[ch];
                    }
                    else
                    {
                        want = 0.0;
                    }

                    auto& boost = boostDb[ch];
                    boost += (want - boost) * (want > boost ? expanderAttack : expanderRelease);
                    upDb = boost;
                }

                const auto upGain = std::pow (10.0, upDb / 20.0);
                cachedGain[ch] = upAmount >= 1.0 ? upGain : cachedGain[ch] + (upGain - cachedGain[ch]) * upAmount;
                cachedPosition[ch] = -1.0;
            }

            gainRing[ch][(size_t) writeAt] = (float) cachedGain[ch];
            audioRing[ch][(size_t) writeAt] = (float) in[ch];
            filteredRing[ch][(size_t) writeAt] = (float) filtered[ch];
        }

        const auto openAmount = 0.5 - 0.5 * std::cos (std::numbers::pi * machines[0].position);
        openRing[(size_t) writeAt] = (float) openAmount;

        // The audio from `delay` ago, with the gain from Pre-open after it.
        const auto audioAt = (int) ((now - delay) & ringMask);
        const auto gainAt = (int) ((now - (activeLookahead ? std::max (0, delay - preOpenSamples) : 0)) & ringMask);

        const auto wetGain = wet.next();
        const auto dryGain = dry.next();
        const auto noiseGain = noise.next();
        const auto outputGain = output.next();
        const auto bypassAmount = bypass.next();
        const auto fade = switchGain.next();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto delayed = (double) audioRing[ch][(size_t) audioAt];
            const auto gain = (double) gainRing[ch][(size_t) gainAt];

            double processed;

            if (settings.preview)
            {
                processed = (double) filteredRing[ch][(size_t) audioAt] * outputGain;
            }
            else
            {
                const auto noiseSample = noiseGain > 0.0 ? (double) whiteNoise (noiseState[ch]) * (double) openRing[(size_t) gainAt] * noiseGain : 0.0;
                processed = (delayed * gain * wetGain + delayed * dryGain + noiseSample) * outputGain;
            }

            const auto mixed = bypassAmount >= 1.0 ? delayed : processed + (delayed - processed) * bypassAmount;
            main[ch][i] = (float) (mixed * fade);

            reductionPeak = std::max (reductionPeak, gain > 0.0 ? -20.0 * std::log10 (gain) : 120.0);
        }

        pushHistory ((float) loudest, (float) (cachedGain[0] > 0.0 ? 20.0 * std::log10 (cachedGain[0]) : -120.0));

        if (holdOffSamples > 0)
            --holdOffSamples;

        ++now;
    }

    // MIDI off: let the sounding note go.
    if (! settings.midi && (noteSounding || velocityPending))
    {
        schedule (std::max (now - 1, lastOnTime + 1), false, 0);
        noteSounding = false;
        velocityPending = false;
    }

    flushEvents (midi, blockStart, numSamples);

    // Lights and meters.
    const auto& shown = machines[0];
    light.store (shown.state == Machine::hold ? lightHold
                                              : (shown.state == Machine::opening || shown.state == Machine::open ? lightOpen : lightClosed),
                 std::memory_order_relaxed);

    auto stored = reduction.load (std::memory_order_relaxed);
    const auto newPeak = (float) std::min (reductionPeak, 120.0);

    while (newPeak > stored && ! reduction.compare_exchange_weak (stored, newPeak, std::memory_order_relaxed))
    {
    }

    // A sidechain source with nothing on it, while the main input plays.
    const auto blockSeconds = (double) numSamples / rate;

    if (usesSidechain && peakDb (sidechainPeak) < sidechainSilentDb && peakDb (mainPeak) > mainActiveDb)
        silentSidechainSeconds += blockSeconds;
    else if (! usesSidechain || peakDb (sidechainPeak) >= sidechainSilentDb)
        silentSidechainSeconds = 0.0;

    sidechainMissing.store (silentSidechainSeconds >= sidechainWarningSeconds, std::memory_order_relaxed);
}
