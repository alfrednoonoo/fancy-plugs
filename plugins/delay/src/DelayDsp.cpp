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

double tanhOf (double value, bool fast) noexcept
{
    return fast ? DelayDsp::fastTanh (value) : std::tanh (value);
}

double safetyCurve (double value, bool fast) noexcept
{
    const auto size = std::abs (value);

    if (size <= safetyKnee)
        return value;

    const auto room = 1.0 - safetyKnee;
    return std::copysign (safetyKnee + room * tanhOf ((size - safetyKnee) / room, fast), value);
}

double shape (double value, double saturation, bool fast) noexcept
{
    const auto safe = safetyCurve (value, fast);
    return saturation > 0.0 ? safe + (tanhOf (value, fast) - safe) * saturation : safe;
}

// Phase 3 Q2.3 ---------------------------------------------------------------------------

constexpr double characterMs = 30.0;
constexpr double tapeBumpHz = 90.0, tapeBumpQ = 0.7, tapeBumpDb = 2.0;
constexpr double tapeLossTopHz = 14000.0, tapeLossBottomHz = 5000.0;
constexpr double bbdStages = 4096.0, bbdMinHz = 2500.0, bbdMaxHz = 15000.0;
constexpr double bbdNoise = 1.0e-4; // -80 dBFS at Amount 100 %
constexpr double diffuserMs[] { 7.3, 11.9, 17.1, 23.7 };
constexpr double maxDiffusion = 0.65;
constexpr double duckRangeDb = 6.0; // Amount is reached 6 dB over the threshold

// Phase 4 Wave 3 ---------------------------------------------------------------------------

// The reverse windows' fades, and the longest window before it shortens (docs/delay.md, Phase 4
// Wave 3, section 3).
constexpr double reverseFadeMs = 5.0, reverseLongestMs = 5000.0, reverseShortestMs = 50.0;
constexpr double maxPitchSemitones = 12.0;

// Longer than the shifter's farthest read (51 ms, and its 20 ms look back).
constexpr double shifterWarmMs = 75.0;

// Phase 4 Wave 2 ---------------------------------------------------------------------------

// The triangle's corners are rounded over this share of the cycle: its slope turns over as a
// half sine there, so the pitch moves from +Depth to -Depth without a jump.
constexpr double triangleCorner = 0.1;

// The rounded triangle's peak, in cycles of travel at slope 1; dividing by it gives -1 to 1.
constexpr double trianglePeak = 0.25 - 0.5 * triangleCorner + triangleCorner / std::numbers::pi;

// The triangle's position amplitude over the sine's for the same peak pitch change.
constexpr double triangleScale = 2.0 * std::numbers::pi * trianglePeak;

// A triangle from -1 to 1 with rounded corners, in phase with sin (2 pi cycle): 0 at 0, the peak
// at a quarter, the trough at three quarters.
double roundedTriangle (double cycle) noexcept
{
    auto s = cycle - 0.25;   // the distance from the peak, -0.5 to 0.5
    s -= std::floor (s + 0.5);
    s = std::abs (s);

    constexpr double half = 0.5 * triangleCorner;
    constexpr double bend = triangleCorner / std::numbers::pi;
    double position;

    if (s < half)
        position = trianglePeak - bend * (1.0 - std::cos (std::numbers::pi * s / triangleCorner));
    else if (s > 0.5 - half)
        position = -trianglePeak + bend * (1.0 - std::cos (std::numbers::pi * (0.5 - s) / triangleCorner));
    else
        position = trianglePeak - bend - (s - half);

    return position / trianglePeak;
}

double softCurve (double u) noexcept
{
    return u / std::sqrt (1.0 + u * u);
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

double DelayDsp::fastTanh (double x) noexcept
{
    // A 7/6 Padé form, held to ±1 where it would pass it.
    const auto x2 = x * x;
    const auto y = x * (135135.0 + x2 * (17325.0 + x2 * (378.0 + x2))) / (135135.0 + x2 * (62370.0 + x2 * (3150.0 + x2 * 28.0)));
    return std::clamp (y, -1.0, 1.0);
}

double DelayDsp::tapMs (const TapSpec& spec, double bpm, double swing) noexcept
{
    if (! spec.noteMode)
        return std::clamp (spec.lengthMs, 0.0, delay::maxDelayMs);

    auto ms = delay::noteMs (spec.noteLength, bpm);

    // Phase 3 Q2.3: on the eighth-note grid, a tap on an odd eighth moves later.
    if (swing > 0.5)
    {
        const auto eighth = delay::beatMs (bpm) * 0.5;
        const auto steps = ms / eighth;
        const auto whole = std::round (steps);

        if (std::abs (steps - whole) < 1.0e-6 && std::fmod (whole, 2.0) == 1.0)
            ms = std::min (delay::maxDelayMs, ms + (2.0 * swing - 1.0) * eighth);
    }

    return ms;
}

void DelayDsp::prepare (double sampleRate)
{
    freezeMix.setTime (30.0, sampleRate);
    freezeMix.snapTo (0.0);

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

        // Phase 3 Q2.3.
        for (auto* smoother : { &tap.modDepth, &tap.diffusion })
            smoother->setTime (parameterMs, rate);

        for (auto* smoother : { &tap.tapeMix, &tap.bbdMix })
            smoother->setTime (characterMs, rate);

        // Phase 4 Wave 3.
        tap.shifter.prepare (rate);
        tap.pitch.setTime (parameterMs, rate);
        tap.pitchMix.setTime (crossfadeMs, rate);
        tap.reverseMix.setTime (crossfadeMs, rate);
        tap.pitch.snapTo (0.0);
        tap.pitchMix.snapTo (0.0);
        tap.reverseMix.snapTo (0.0);

        // Phase 4 Wave 2.
        tap.shapeMix.setTime (characterMs, rate);
        tap.stereoOffset.setTime (parameterMs, rate);
        tap.shapeMix.snapTo (0.0);
        tap.stereoOffset.snapTo (0.25);

        for (int ch = 0; ch < 2; ++ch)
        {
            for (int k = 0; k < 4; ++k)
            {
                tap.diffuserLength[ch][k] = std::max (1, (int) std::lround (diffuserMs[k] * (ch == 0 ? 1.0 : 1.07) * 0.001 * rate));
                tap.diffusers[ch][k].assign ((size_t) tap.diffuserLength[ch][k], 0.0f);
            }
        }

        tap.lastCharacterAmount = tap.lastCutoff = -1.0;
    }

    pitchLead = std::max (1, (int) std::lround (taps[0].shifter.getAverageDelaySamples()));
    reverseFade = std::max (1, (int) std::lround (reverseFadeMs * 0.001 * rate));
    shifterWarm = std::max (1, (int) std::lround (shifterWarmMs * 0.001 * rate));

    duckAmount.setTime (parameterMs, rate);
    duckAttack = 1.0 - std::exp (-1.0 / (0.001 * rate));

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

        // Phase 3 Q2.3.
        for (auto& state : tap.tone)
            state = {};

        for (auto& channel : tap.diffusers)
            for (auto& line : channel)
                std::fill (line.begin(), line.end(), 0.0f);

        for (auto& channel : tap.diffuserIndex)
            std::fill (std::begin (channel), std::end (channel), 0);

        tap.modPhase = tap.walk = tap.walkTarget = 0.0;
        tap.modSin = 0.0;
        tap.modCos = 1.0;
        tap.walkCountdown = 0;
        tap.noiseState = 1;

        // Phase 4 Wave 3.
        tap.shifter.reset();
        tap.shifterRunning = false;
        tap.warmed = 0;

        for (int ch = 0; ch < 2; ++ch)
        {
            tap.reverseLowpass[ch].reset();
            tap.reverseHighpass[ch].reset();
        }

        restartReverse (tap);
    }

    duckEnvelope = 0.0;
    duckGain = 1.0;
    duckStep = 0.0;
    duckCountdown = 0;
}

void DelayDsp::setTapTargets (Tap& tap, const TapSpec& spec, bool anySolo, double bpm, double swing, bool smoothTime, bool isMono) noexcept
{
    // Phase 3 Q2.3.
    tap.modDepth.setTarget (spec.used ? std::max (0.0, spec.modDepthCents) : 0.0);
    tap.modRate = std::clamp (spec.modRateHz, 0.05, 20.0);
    tap.modRandom = std::clamp (spec.modRandom, 0.0, 1.0);
    tap.characterAmount = std::clamp (spec.characterAmount, 0.0, 1.0);
    tap.tapeMix.setTarget (spec.character == tape ? 1.0 : 0.0);
    tap.bbdMix.setTarget (spec.character == bbd ? 1.0 : 0.0);
    tap.diffusion.setTarget (std::clamp (spec.diffusion, 0.0, 1.0));

    // Phase 4 Wave 2. 90 degrees is a quarter cycle, exactly.
    tap.shapeMix.setTarget (spec.modShape == triangle ? 1.0 : 0.0);
    tap.stereoOffset.setTarget (std::clamp (spec.modStereoDegrees, 0.0, 180.0) / 360.0);

    // Phase 4 Wave 3. A reversed tap is shifted by its reverse head's speed, not the shifter.
    const auto semitones = std::clamp (spec.pitchSemitones, -maxPitchSemitones, maxPitchSemitones);
    tap.pitch.setTarget (semitones);
    tap.pitchWanted = spec.used && semitones != 0.0 && ! spec.reverse;
    tap.reverseMix.setTarget (spec.used && spec.reverse ? 1.0 : 0.0);
    tap.noteMode = spec.noteMode;

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
    tap.targetDelay = std::clamp ((int) std::lround (tapMs (spec, bpm, swing) * 0.001 * rate), 1, maxDelaySamples);

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
    // A tap starts from a silent ring, so its shifter needs no warming.
    tap.pitchMix.setTarget (tap.pitchWanted ? 1.0 : 0.0);
    tap.warmed = shifterWarm;

    for (auto* smoother : { &tap.feedback, &tap.width, &tap.volume, &tap.panLeft, &tap.panRight, &tap.inGate, &tap.outGate,
                            &tap.lowpassMix, &tap.highpassMix, &tap.crushMix, &tap.loopMix, &tap.saturateMix, &tap.pingMix,
                            &tap.startLeft, &tap.lowpassOctaves, &tap.highpassOctaves, &tap.bits,
                            &tap.modDepth, &tap.tapeMix, &tap.bbdMix, &tap.diffusion, &tap.shapeMix, &tap.stereoOffset,
                            &tap.pitch, &tap.pitchMix, &tap.reverseMix })
        smoother->snapToTarget();

    tap.headA = tap.headB = tap.targetDelay;
    tap.glidePosition = (double) tap.targetDelay;
    tap.fadeLeft = 0;

    tap.lowpassCoefficients.set (std::exp2 (tap.lowpassOctaves.getCurrent()), filterQ, rate);
    tap.highpassCoefficients.set (std::exp2 (tap.highpassOctaves.getCurrent()), filterQ, rate);
    tap.crushScale = std::exp2 (tap.bits.getCurrent() - 1.0);

    // Phase 4 Wave 3: the reverse windows start from when the tap starts running.
    tap.ratio = std::exp2 (tap.pitch.getCurrent() / 12.0);
    tap.ratioFor = tap.pitch.getCurrent();
    tap.shifter.reset();
    tap.shifterRunning = false;
    restartReverse (tap);
}

// Phase 4 Wave 3 ----------------------------------------------------------------------------

int DelayDsp::reverseWindowFor (const Tap& tap) const noexcept
{
    // One tap time, up to 5 s; above that, what's left of the ring's 10 s, at least 50 ms. The
    // head reaches (1 + ratio) windows back at its far end, so a raised pitch shortens a long window.
    const auto longest = (int) std::lround (reverseLongestMs * 0.001 * rate);
    const auto shortest = (int) std::lround (reverseShortestMs * 0.001 * rate);
    const auto window = tap.targetDelay <= longest ? tap.targetDelay : std::max (shortest, maxDelaySamples - tap.targetDelay);
    const auto reach = (int) ((double) (maxDelaySamples - 4) / (1.0 + std::max (1.0, tap.ratio)));

    return std::clamp (window, 2, reach);
}

void DelayDsp::restartReverse (Tap& tap) noexcept
{
    tap.reverseWindow = reverseWindowFor (tap);
    tap.reverseK = 0;
    tap.reverseDelay = 0.0;
    tap.sinceJump = 1 << 30;
}

void DelayDsp::followHostGrid (Tap& tap) noexcept
{
    // Decision 2 A: while the host plays, a Note-mode tap's windows start on its own note grid,
    // counted from the host's position. Off the grid by more than a sample, the head jumps there
    // and fades in again.
    if (! hostPlaying || ! tap.noteMode || hostBpm <= 0.0)
        return;

    const auto window = (double) tap.reverseWindow;
    const auto position = hostPpq * 60.0 / hostBpm * rate;
    const auto phase = position - std::floor (position / window) * window;
    const auto wanted = (int) std::lround (phase) % tap.reverseWindow;

    auto apart = std::abs (wanted - tap.reverseK);
    apart = std::min (apart, tap.reverseWindow - apart);

    if (apart <= 1)
        return;

    tap.reverseK = wanted;
    tap.reverseDelay = (double) wanted * (1.0 + tap.ratio);
    tap.sinceJump = 0;
}

void DelayDsp::setTargets (const Targets& targets, bool isMono) noexcept
{
    const auto anySolo = std::any_of (targets.taps.begin(), targets.taps.end(),
                                      [] (const TapSpec& spec) { return spec.used && spec.on && spec.solo; });

    for (size_t i = 0; i < taps.size(); ++i)
        setTapTargets (taps[i], targets.taps[i], anySolo, targets.bpm, targets.swing, targets.smoothTime, isMono);

    wetGain.setTarget (targets.wetGain);
    dryGain.setTarget (targets.dryGain);
    outputGain.setTarget (targets.outputGain);
    bypassMix.setTarget (targets.bypass ? 1.0 : 0.0);

    // Phase 3 Q2.3.
    duckAmount.setTarget (std::clamp (targets.duckAmountDb, 0.0, 24.0));
    duckThresholdDb = targets.duckThresholdDb;
    duckRelease = 1.0 - std::exp (-1.0 / (std::max (1.0, targets.duckReleaseMs) * 0.001 * rate));
    useFastTanh = targets.fastTanh;

    // Phase 4 Wave 1.
    freezeOn = targets.freeze;

    // Phase 4 Wave 3.
    hostPlaying = targets.hostPlaying;
    hostPpq = targets.hostPpq;
    hostBpm = targets.bpm;
}

void DelayDsp::snapToTargets (const Targets& targets, bool isMono) noexcept
{
    setTargets (targets, isMono);

    for (auto& tap : taps)
        snapTap (tap);

    for (auto* smoother : { &wetGain, &dryGain, &outputGain, &bypassMix, &duckAmount, &freezeMix })
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

float DelayDsp::readAt (const Tap& tap, int channel, int writeAt, double position) const noexcept
{
    const auto whole = (int) position;
    const auto fraction = position - (double) whole;
    const auto head = std::max (2, whole);
    const auto& ring = tap.ring[channel];

    return (float) hermite (ring[(size_t) ((writeAt - head + 1) & ringMask)],
                            ring[(size_t) ((writeAt - head) & ringMask)],
                            ring[(size_t) ((writeAt - head - 1) & ringMask)],
                            ring[(size_t) ((writeAt - head - 2) & ringMask)],
                            fraction);
}

void DelayDsp::updateCharacter (Tap& tap) noexcept
{
    const auto amount = tap.characterAmount;
    const auto clock = bbdStages / ((double) tap.targetDelay / rate);
    const auto bbdHz = std::clamp (0.4 * clock, bbdMinHz, bbdMaxHz);
    const auto cutoff = std::min (bbdMaxHz + (bbdHz - bbdMaxHz) * amount, 0.45 * rate);

    if (cutoff != tap.lastCutoff)
    {
        tap.bbdCoefficients.set (cutoff, filterQ, rate);
        tap.lastCutoff = cutoff;
    }

    if (amount == tap.lastCharacterAmount)
        return;

    tap.lastCharacterAmount = amount;

    // Tape's head bump: a peaking filter, scaled by Amount.
    const auto a = std::pow (10.0, tapeBumpDb * amount / 40.0);
    const auto w = 2.0 * std::numbers::pi * tapeBumpHz / rate;
    const auto alpha = std::sin (w) / (2.0 * tapeBumpQ);
    const auto a0 = 1.0 + alpha / a;

    tap.bumpB0 = (1.0 + alpha * a) / a0;
    tap.bumpB1 = -2.0 * std::cos (w) / a0;
    tap.bumpB2 = (1.0 - alpha * a) / a0;
    tap.bumpA1 = tap.bumpB1;
    tap.bumpA2 = (1.0 - alpha / a) / a0;

    // Tape's high-frequency loss, darker as Amount rises.
    const auto lossHz = std::min (tapeLossTopHz * std::pow (tapeLossBottomHz / tapeLossTopHz, amount), 0.45 * rate);
    tap.lossCoefficient = 1.0 - std::exp (-2.0 * std::numbers::pi * lossHz / rate);
}

double DelayDsp::applyCharacter (Tap& tap, int channel, double value, double tapeMix, double bbdMix) noexcept
{
    auto& state = tap.tone[channel];
    const auto amount = tap.characterAmount;
    auto result = value;

    if (tapeMix > 0.0)
    {
        const auto drive = 1.0 + 2.0 * amount;
        const auto saturated = softCurve (value * drive) / drive;

        const auto bumped = tap.bumpB0 * saturated + state.bump1;
        state.bump1 = tap.bumpB1 * saturated - tap.bumpA1 * bumped + state.bump2;
        state.bump2 = tap.bumpB2 * saturated - tap.bumpA2 * bumped;

        state.loss += (bumped - state.loss) * tap.lossCoefficient;
        result += (state.loss - value) * tapeMix;
    }

    if (bbdMix > 0.0)
    {
        // The compander's trace: clock noise that rides the signal.
        state.envelope += (std::abs (value) - state.envelope) * 0.002;
        tap.noiseState = tap.noiseState * 1664525u + 1013904223u;
        const auto noise = ((double) (tap.noiseState >> 8) / 8388608.0 - 1.0) * bbdNoise * amount * (1.0 + 20.0 * state.envelope);

        const auto input = softCurve (value * 1.2) / 1.2 + noise;
        const auto filtered = state.bbdB.tick (tap.bbdCoefficients, state.bbdA.tick (tap.bbdCoefficients, input).low).low;
        result += (filtered - value) * bbdMix;
    }

    return result;
}

double DelayDsp::diffuse (Tap& tap, int channel, double value, double coefficient) noexcept
{
    for (int k = 0; k < 4; ++k)
    {
        auto& line = tap.diffusers[channel][k];
        const auto index = (size_t) tap.diffuserIndex[channel][k];
        const auto delayed = (double) line[index];
        const auto output = delayed - coefficient * value;

        line[index] = (float) (value + coefficient * output);
        value = output;
    }

    return value;
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

void DelayDsp::processTap (Tap& tap, const float* inLeft, const float* inRight, double* wetLeft, double* wetRight, int numSamples, const double* frozen) noexcept
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

        // Phase 3 Q2.3: modulation, character and diffusion, each skipped at its default.
        const auto depthCents = tap.modDepth.next();
        const auto tapeMix = tap.tapeMix.next();
        const auto bbdMix = tap.bbdMix.next();
        const auto diffusion = tap.diffusion.next();
        const auto shapeMix = tap.shapeMix.next();     // Phase 4 Wave 2
        const auto stereoOffset = tap.stereoOffset.next();
        const auto characterOn = tapeMix > 0.0 || bbdMix > 0.0;

        // Phase 4 Wave 3: Pitch's glide and the two crossfades, each 0 while it's off.
        const auto semitones = tap.pitch.next();

        if (tap.pitchWanted && tap.warmed < shifterWarm)
        {
            ++tap.warmed;
            tap.pitchMix.setTarget (0.0);
        }
        else
        {
            tap.pitchMix.setTarget (tap.pitchWanted ? 1.0 : 0.0);
        }

        const auto pitchMix = tap.pitchMix.next();
        const auto reverseMix = tap.reverseMix.next();

        if (semitones != tap.ratioFor)
        {
            tap.ratio = std::exp2 (semitones / 12.0);
            tap.ratioFor = semitones;
        }

        if (characterOn)
        {
            updateCharacter (tap);
            tap.characterActive = true;
        }
        else if (tap.characterActive)
        {
            for (auto& state : tap.tone)
                state = {};

            tap.characterActive = false;
        }

        double offset[2] { 0.0, 0.0 };

        if (depthCents > 0.0)
        {
            // Wow and flutter: a sine plus a smoothed random walk, scaled so the peak pitch
            // change is Depth. The right channel's sine runs a quarter cycle ahead.
            const auto twoPi = 2.0 * std::numbers::pi;

            if (depthCents != tap.modAmplitudeDepth || tap.modRate != tap.modAmplitudeRate)
            {
                tap.modAmplitude = (std::exp2 (depthCents / 1200.0) - 1.0) / (twoPi * tap.modRate) * rate;
                tap.modAmplitudeDepth = depthCents;
                tap.modAmplitudeRate = tap.modRate;
            }

            if (tap.modRate != tap.modStepRate)
            {
                const auto step = twoPi * tap.modRate / rate;
                tap.modStepSin = std::sin (step);
                tap.modStepCos = std::cos (step);
                tap.modStepRate = tap.modRate;
            }

            const auto amplitude = tap.modAmplitude;

            // Rotate the phasor one step, and pull it back onto the unit circle.
            const auto nextSin = tap.modSin * tap.modStepCos + tap.modCos * tap.modStepSin;
            const auto nextCos = tap.modCos * tap.modStepCos - tap.modSin * tap.modStepSin;
            const auto norm = 1.5 - 0.5 * (nextSin * nextSin + nextCos * nextCos);
            tap.modSin = nextSin * norm;
            tap.modCos = nextCos * norm;

            if (--tap.walkCountdown <= 0)
            {
                tap.noiseState = tap.noiseState * 1664525u + 1013904223u;
                tap.walkTarget = (double) (tap.noiseState >> 8) / 8388608.0 - 1.0;
                tap.walkCountdown = std::max (1, (int) (rate / (tap.modRate * 4.0)));
            }

            tap.walk += (tap.walkTarget - tap.walk) * (tap.modRate * 6.0 / rate);

            // The phasor's angle in cycles, kept for the triangle (Phase 4 Wave 2).
            tap.modPhase += tap.modRate / rate;
            tap.modPhase -= std::floor (tap.modPhase);

            if (shapeMix == 0.0 && stereoOffset == 0.25)
            {
                // Sine at 90 degrees: the modulation as it was before Phase 4 Wave 2, to the bit.
                for (int ch = 0; ch < 2; ++ch)
                {
                    const auto wave = ch == 0 ? tap.modSin : tap.modCos; // the right channel a quarter cycle ahead
                    const auto motion = (1.0 - tap.modRandom) * wave + tap.modRandom * tap.walk;
                    offset[ch] = amplitude * (1.0 + motion);
                }
            }
            else
            {
                // Phase 4 Wave 2: the right channel runs stereoOffset of a cycle ahead, and Shape
                // crossfades the sine into the rounded triangle. The triangle needs triangleScale
                // times the sine's travel for the same peak pitch, and the centre moves up with it
                // so the read never comes ahead of the tap's time.
                if (stereoOffset != tap.stereoFor)
                {
                    const auto angle = 2.0 * std::numbers::pi * stereoOffset;
                    tap.stereoSin = std::sin (angle);
                    tap.stereoCos = std::cos (angle);
                    tap.stereoFor = stereoOffset;
                }

                const double sines[2] { tap.modSin, tap.modSin * tap.stereoCos + tap.modCos * tap.stereoSin };
                const auto centre = 1.0 + shapeMix * (triangleScale - 1.0);

                for (int ch = 0; ch < 2; ++ch)
                {
                    auto shaped = sines[ch];

                    if (shapeMix > 0.0)
                    {
                        const auto triangleValue = roundedTriangle (tap.modPhase + (ch == 0 ? 0.0 : stereoOffset));
                        shaped += shapeMix * (triangleScale * triangleValue - shaped);
                    }

                    const auto motion = (1.0 - tap.modRandom) * shaped + tap.modRandom * tap.walk;
                    offset[ch] = amplitude * (centre + motion);
                }
            }
        }

        // Read heads ------------------------------------------------------------------------

        if (tap.smoothTime && tap.fadeLeft == 0 && tap.headA != tap.targetDelay)
        {
            tap.headB = tap.targetDelay;
            tap.fadeLeft = fadeSamples;
        }

        double read[2];
        const auto modulated = depthCents > 0.0;

        // Phase 4 Wave 3: a pitched tap reads the ring the shifter's average delay sooner, so its
        // repeats still land on the tap time. Read beside today's heads, which stay as they were.
        const auto pitching = pitchMix > 0.0 || tap.pitchWanted;
        double pitchedIn[2] { 0.0, 0.0 };

        const auto readSooner = [&] (int ch, int head)
        {
            return modulated ? (double) readAt (tap, ch, writeAt, std::max (2.0, (double) (head - pitchLead) + offset[ch]))
                             : (double) readHead (tap, ch, writeAt, std::max (2, head - pitchLead));
        };

        if (tap.smoothTime)
        {
            for (int ch = 0; ch < 2; ++ch)
                read[ch] = modulated ? (double) readAt (tap, ch, writeAt, (double) tap.headA + offset[ch])
                                     : (double) readHead (tap, ch, writeAt, tap.headA);

            if (pitching)
                for (int ch = 0; ch < 2; ++ch)
                    pitchedIn[ch] = readSooner (ch, tap.headA);

            if (tap.fadeLeft > 0)
            {
                const auto position = 1.0 - (double) tap.fadeLeft / (double) fadeSamples;

                for (int ch = 0; ch < 2; ++ch)
                    read[ch] += ((modulated ? (double) readAt (tap, ch, writeAt, (double) tap.headB + offset[ch])
                                            : (double) readHead (tap, ch, writeAt, tap.headB)) - read[ch]) * position;

                if (pitching)
                    for (int ch = 0; ch < 2; ++ch)
                        pitchedIn[ch] += (readSooner (ch, tap.headB) - pitchedIn[ch]) * position;

                if (--tap.fadeLeft == 0)
                    tap.headA = tap.headB;
            }
        }
        else
        {
            for (int ch = 0; ch < 2; ++ch)
                read[ch] = modulated ? (double) readAt (tap, ch, writeAt, tap.glidePosition + offset[ch])
                                     : (double) readGlide (tap, ch, writeAt);

            if (pitching)
                for (int ch = 0; ch < 2; ++ch)
                    pitchedIn[ch] = (double) readAt (tap, ch, writeAt, std::max (2.0, tap.glidePosition - (double) pitchLead) + offset[ch]);

            const auto target = (double) tap.targetDelay;

            if (tap.glidePosition != target)
            {
                tap.glidePosition += (target - tap.glidePosition) * glideCoefficient;

                if (std::abs (target - tap.glidePosition) < 0.001)
                    tap.glidePosition = target;
            }
        }

        // What Freeze writes back: the ring as read, which for a pitched tap is the shifter's
        // input, so a frozen loop holds instead of climbing.
        double held[2] { read[0], read[1] };

        if (pitching)
        {
            double shifted[2];
            tap.shifter.process (pitchedIn, shifted, 2, tap.ratio);
            tap.shifterRunning = true;

            for (int ch = 0; ch < 2; ++ch)
            {
                read[ch] += (shifted[ch] - read[ch]) * pitchMix;
                held[ch] += (pitchedIn[ch] - held[ch]) * pitchMix;
            }
        }
        else if (tap.shifterRunning)
        {
            tap.shifter.reset();
            tap.shifterRunning = false;
            tap.warmed = 0;
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

            if (characterOn)
                value = applyCharacter (tap, ch, value, tapeMix, bbdMix);

            const auto looped = read[ch] + (value - read[ch]) * loopMix;

            output[ch] = shaped ? shape (value, saturation, useFastTanh) : value;
            loop[ch] = shaped ? shape (looped, saturation, useFastTanh) : looped;

            const auto blocked = loop[ch] - tap.dcInput[ch] + dcCoefficient * tap.dcOutput[ch];
            tap.dcInput[ch] = loop[ch];
            tap.dcOutput[ch] = blocked;

            if (diffusion > 0.0)
                loop[ch] = diffuse (tap, ch, blocked, diffusion * maxDiffusion) * feedback;
            else
                loop[ch] = blocked * feedback;
        }

        // Phase 4 Wave 1: frozen, the loop writes back exactly what it read, and the input stops.
        const auto hold = frozen != nullptr ? frozen[i] : 0.0;

        if (hold > 0.0)
            for (int ch = 0; ch < 2; ++ch)
                loop[ch] += (held[ch] - loop[ch]) * hold;

        // Phase 4 Wave 3: the reverse head. Inside each window it runs backwards from the write
        // head at the pitch's speed, so its delay grows from 0 by 1 + ratio a sample, and each
        // window fades in and out over 5 ms. It plays the forward loop backwards: Feedback and the
        // loop are as they were (decision 1 A). Its own filters and resolution follow the tap's.
        if (tap.reverseK == 0)
        {
            tap.reverseWindow = reverseWindowFor (tap);
            tap.reverseDelay = 0.0;
        }

        if (reverseMix > 0.0)
        {
            const auto fade = (double) std::min (reverseFade, std::max (1, tap.reverseWindow / 2));
            const auto edge = (double) std::min ({ tap.reverseK, tap.reverseWindow - tap.reverseK, tap.sinceJump });
            const auto envelope = edge >= fade ? 1.0 : 0.5 - 0.5 * std::cos (std::numbers::pi * edge / fade);

            for (int ch = 0; ch < 2; ++ch)
            {
                auto value = (double) readAt (tap, ch, writeAt, std::max (2.0, tap.reverseDelay) + offset[ch]);

                if (lowpassMix > 0.0)
                    value += (tap.reverseLowpass[ch].tick (tap.lowpassCoefficients, value).low - value) * lowpassMix;
                else
                    tap.reverseLowpass[ch].reset();

                if (highpassMix > 0.0)
                    value += (tap.reverseHighpass[ch].tick (tap.highpassCoefficients, value).high - value) * highpassMix;
                else
                    tap.reverseHighpass[ch].reset();

                if (crushMix > 0.0)
                    value += (std::round (value * tap.crushScale) / tap.crushScale - value) * crushMix;

                const auto reversed = (shaped ? shape (value, saturation, useFastTanh) : value) * envelope;
                output[ch] += (reversed - output[ch]) * reverseMix;
            }
        }

        tap.reverseDelay = std::min (tap.reverseDelay + 1.0 + tap.ratio, (double) (maxDelaySamples - 4));
        tap.sinceJump = std::min (tap.sinceJump + 1, 1 << 30);

        if (++tap.reverseK >= tap.reverseWindow)
            tap.reverseK = 0;

        // Back into the ring ----------------------------------------------------------------

        auto left = (double) inLeft[i] * inGate;
        auto right = (double) inRight[i] * inGate;

        if (hold > 0.0)
        {
            left *= 1.0 - hold;
            right *= 1.0 - hold;
        }

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
            writeLeft = safetyCurve (writeLeft, useFastTanh);
            writeRight = safetyCurve (writeRight, useFastTanh);
        }

        if (diffusion > 0.0)
        {
            tap.diffusionActive = true;

            for (int ch = 0; ch < 2; ++ch)
                for (int k = 0; k < 4; ++k)
                    if (++tap.diffuserIndex[ch][k] >= tap.diffuserLength[ch][k])
                        tap.diffuserIndex[ch][k] = 0;
        }
        else if (tap.diffusionActive)
        {
            for (auto& channel : tap.diffusers)
                for (auto& line : channel)
                    std::fill (line.begin(), line.end(), 0.0f);

            tap.diffusionActive = false;
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

    // Phase 4 Wave 3: the host's position is for the block's first sample.
    for (auto& tap : taps)
        if (! tap.asleep)
            followHostGrid (tap);

    for (int start = 0; start < numSamples;)
    {
        const auto count = std::min (chunkSize, numSamples - start);

        float* const left = channels[0] + start;
        float* const right = numChannels > 1 ? channels[1] + start : left;

        std::fill_n (wetLeft, count, 0.0);
        std::fill_n (wetRight, count, 0.0);

        // Phase 4 Wave 1: Freeze's crossfade for this chunk, or none at all while it's off.
        double frozen[chunkSize];
        const auto freezing = freezeOn || ! freezeMix.isSettled() || freezeMix.getCurrent() > 0.0;
        freezeMix.setTarget (freezeOn ? 1.0 : 0.0);

        if (freezing)
            for (int i = 0; i < count; ++i)
                frozen[i] = freezeMix.next();

        for (auto& tap : taps)
            if (! tap.asleep)
                processTap (tap, left, right, wetLeft, wetRight, count, freezing ? frozen : nullptr);

        for (int i = 0; i < count; ++i)
        {
            auto wet = wetGain.next();

            // Phase 3 Q2.3: the wet signal ducks while the input is over the threshold.
            if (const auto duck = duckAmount.next(); duck > 0.0)
            {
                const auto level = std::max (std::abs ((double) left[i]), std::abs ((double) right[i]));
                duckEnvelope += (level - duckEnvelope) * (level > duckEnvelope ? duckAttack : duckRelease);

                if (--duckCountdown <= 0)
                {
                    const auto levelDb = 20.0 * std::log10 (std::max (duckEnvelope, 1.0e-9));
                    const auto over = std::clamp ((levelDb - duckThresholdDb) / duckRangeDb, 0.0, 1.0);
                    duckStep = (std::pow (10.0, -duck * over / 20.0) - duckGain) / (double) duckInterval;
                    duckCountdown = duckInterval;
                }

                duckGain += duckStep;
                wet *= duckGain;
            }
            else
            {
                duckGain = 1.0;
                duckStep = 0.0;
                duckCountdown = 0;
            }
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
