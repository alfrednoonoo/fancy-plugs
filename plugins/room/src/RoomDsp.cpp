#include "RoomDsp.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
// docs/research/room-model.py: line lengths in ms at Size 100 %. Eco uses every other line.
constexpr std::array<double, RoomDsp::maxLines> lineMsTable { 40.0, 43.1, 46.7, 49.9, 53.3, 57.6, 61.2, 65.9,
                                                              70.3, 74.6, 79.7, 84.1, 89.9, 95.3, 101.6, 108.7 };

// The input diffusers: four all-passes per channel, ms at Size 100 %.
constexpr double diffusionMs[2][4] { { 4.77, 3.59, 12.73, 9.31 }, { 4.91, 3.37, 13.17, 9.67 } };

constexpr double longestReflectionMs = 80.0;
constexpr double modMaxMs = 0.6;              // Mod 100 %
constexpr double freezeRampSeconds = 0.020;   // decision 2
constexpr double smoothingMs = 30.0;
constexpr double predelayGlideMs = 50.0;
constexpr double bypassMs = 10.0;
constexpr double ringOutSeconds = 2.0;        // decision 4
constexpr double ringFadeSeconds = 0.020;
constexpr double quietLevel = 3.1622776601683795e-5; // -90 dBFS
constexpr double quietSeconds = 0.1;
constexpr double duckAttackMs = 10.0, duckReleaseMs = 300.0; // decision 7
constexpr double duckPeakReleaseMs = 50.0;                    // rides over the waveform's zero crossings
constexpr double duckDepthDb = 24.0, duckFullDb = -20.0, duckNoneDb = -50.0;
constexpr double lowestCutHz = 10.0;
constexpr double twoPi = 2.0 * std::numbers::pi;

// Phase 4 Wave 1.
constexpr double autoGainSize = 0.55, autoGainDecay = 1.8; // where Auto gain trims 0 dB
constexpr double autoTrimMs = 50.0, gateOpenMs = 1.0;

// Phase 4 Wave 2. Below attackEnergyMs the taps sit too close together to add as uncorrelated
// copies, so their scaling moves from keeping the amplitude (at 0) to keeping the energy.
constexpr double attackEnergyMs = 10.0;

// Phase 4 Wave 3: Shimmer's loop (docs/room.md, section 3).
constexpr double shimmerHighpassHz = 200.0;
constexpr double shimmerRmsMs = 50.0, shimmerAttackMs = 10.0, shimmerReleaseMs = 100.0;

double frac (double x) noexcept
{
    return x - std::floor (x);
}

int powerOfTwoAbove (double samples) noexcept
{
    auto size = 1;

    while ((double) size < samples)
        size <<= 1;

    return size;
}

double smoothingCoefficient (double milliseconds, double rate) noexcept
{
    return 1.0 - std::exp (-1.0 / (milliseconds * 0.001 * rate));
}
} // namespace

// er_table in the model: 16 taps per channel, 4 to 80 ms at Size 100 %, alternating signs.
RoomDsp::Tap RoomDsp::reflection (int channel, int index) noexcept
{
    const auto j = frac (index * 0.618034 + channel * 0.31);
    const auto ms = 4.0 + 76.0 * std::pow ((index + j) / erTaps, 1.4);
    const auto polarity = (index * 7 + channel * 3) % 3 != 0 ? 1.0 : -1.0;
    return { ms, 0.9 * (1.0 - 0.75 * ms / 80.0) * polarity * 0.35 };
}

// Phase 4 Wave 2 (docs/room.md, item 20): a rising raised cosine, scaled so the 16 copies keep
// the energy of one (docs/research/room-w2-model.py).
double RoomDsp::attackTapWeight (int index) noexcept
{
    auto squares = 0.0;

    for (int k = 0; k < attackTaps; ++k)
    {
        const auto w = 0.5 - 0.5 * std::cos (std::numbers::pi * (k + 1) / attackTaps);
        squares += w * w;
    }

    const auto k = std::clamp (index, 0, attackTaps - 1);
    return (0.5 - 0.5 * std::cos (std::numbers::pi * (k + 1) / attackTaps)) / std::sqrt (squares);
}

RoomDsp::RoomDsp()
{
    for (int ch = 0; ch < 2; ++ch)
        for (int k = 0; k < erTaps; ++k)
            reflections[(size_t) ch][(size_t) k] = reflection (ch, k);

    attackWeightSum = 0.0;

    for (int k = 0; k < attackTaps; ++k)
    {
        attackWeight[(size_t) k] = attackTapWeight (k);
        attackWeightSum += attackWeight[(size_t) k];
    }
}

double RoomDsp::longestReflectionSeconds (double sizeValue) noexcept
{
    auto longest = 0.0;

    for (int ch = 0; ch < 2; ++ch)
        for (int k = 0; k < erTaps; ++k)
            longest = std::max (longest, reflection (ch, k).ms);

    return longest * sizeScale (std::clamp (sizeValue, 0.0, 1.0)) * 0.001;
}

// A line of length d seconds keeps g = 10^(-3 d / T) per pass, so an impulse in it gives
// 1 / (1 - g^2) of energy; the tail's energy is the average over the lines.
double RoomDsp::autoGainDb (double sizeValue, double decaySeconds) noexcept
{
    const auto energy = [] (double sizeNow, double decayNow)
    {
        const auto scale = sizeScale (std::clamp (sizeNow, 0.0, 1.0)) * 0.001;
        const auto t = std::clamp (decayNow, 0.1, 20.0);
        auto sum = 0.0;

        for (const auto ms : lineMsTable)
            sum += 1.0 / (1.0 - std::pow (10.0, -6.0 * ms * scale / t));

        return sum;
    };

    return -10.0 * std::log10 (energy (sizeValue, decaySeconds) / energy (autoGainSize, autoGainDecay));
}

double RoomDsp::loopEnergy (double sizeValue, double decaySeconds, int numLines) noexcept
{
    const auto lines = numLines < 16 ? 8 : 16;
    const auto scale = sizeScale (std::clamp (sizeValue, 0.0, 1.0)) * 0.001;
    const auto t = std::clamp (decaySeconds, 0.1, 20.0);
    auto sum = 0.0;

    for (int i = 0; i < lines; ++i)
    {
        const auto ms = lineMsTable[(size_t) (lines == 16 ? i : 2 * i)];
        const auto g2 = std::pow (10.0, -6.0 * ms * scale / t);
        sum += g2 / (1.0 - g2);
    }

    const auto a = std::max (1.0e-6, sum / lines);
    return 2.0 * a / lines * (1.14 + 0.44 * a / (a + 2.25));
}

double RoomDsp::tailSeconds (double predelayMs, double sizeValue, double decaySeconds) noexcept
{
    return std::clamp (predelayMs, 0.0, maxPredelayMs) * 0.001 + longestReflectionSeconds (sizeValue) + 1.5 * decaySeconds;
}

void RoomDsp::prepare (double newSampleRate)
{
    sampleRate = std::clamp (newSampleRate, 8000.0, maxRate);
    const auto perMs = sampleRate * 0.001;

    // Sized for the longest settings at this rate, so nothing allocates while playing.
    // Phase 4 Wave 2: Attack's taps reach Pre-delay + 500 ms back. A longer buffer reads the same
    // samples, so the output doesn't change.
    const auto historySize = powerOfTwoAbove ((maxPredelayMs + std::max (longestReflectionMs, maxAttackMs)) * perMs + 4.0);
    history.assign ((size_t) historySize * 2, 0.0f);
    historyMask = historySize - 1;

    const auto allpassSize = powerOfTwoAbove (14.0 * perMs + 4.0);
    allpassMask = allpassSize - 1;

    for (auto& channel : allpass)
        for (auto& buffer : channel)
            buffer.assign ((size_t) allpassSize, 0.0f);

    const auto lineSize = powerOfTwoAbove ((lineMsTable.back() + modMaxMs) * perMs + 8.0);

    for (auto& network : networks)
    {
        for (auto& line : network.lines)
            line.assign ((size_t) lineSize, 0.0f);

        network.mask = lineSize - 1;
    }

    for (auto* smoother : { &size, &decay, &diffusion, &mod, &width, &damping, &logLowCut, &logHighCut,
                            &erGain, &ducking, &wet, &dry, &output })
        smoother->setTime (smoothingMs, sampleRate);

    predelay.setTime (predelayGlideMs, sampleRate);
    bypass.setTime (bypassMs, sampleRate);

    // Phase 4 Wave 1.
    for (auto* smoother : { &modRate, &eqLogHz[0], &eqLogHz[1], &eqLogHz[2], &eqGain[0], &eqGain[1], &eqGain[2], &eqQ })
        smoother->setTime (smoothingMs, sampleRate);

    autoTrim.setTime (autoTrimMs, sampleRate);

    // Phase 4 Wave 2.
    lowDecay.setTime (smoothingMs, sampleRate);
    attack.setTime (smoothingMs, sampleRate);

    // Phase 4 Wave 3.
    shimmerAmount.setTime (smoothingMs, sampleRate);
    loopHighpass.set (shimmerHighpassHz, 0.70710678118654752, sampleRate);
    shifter.prepare (sampleRate);
    shifter.setMode (fancy::dsp::PitchShifter::Mode::plain);
    shimmerRms = 1.0 - std::exp (-1.0 / (shimmerRmsMs * 0.001 * sampleRate));
    shimmerAttack = 1.0 - std::exp (-1.0 / (shimmerAttackMs * 0.001 * sampleRate));
    shimmerRelease = 1.0 - std::exp (-1.0 / (shimmerReleaseMs * 0.001 * sampleRate));

    gateOpenStep = 1.0 / std::max (1.0, gateOpenMs * 0.001 * sampleRate);

    freezeRampSamples = std::max (1, (int) (freezeRampSeconds * sampleRate));
    freezeStep = 1.0 / freezeRampSamples;
    duckAttack = smoothingCoefficient (duckAttackMs, sampleRate);
    duckRelease = smoothingCoefficient (duckReleaseMs, sampleRate);
    duckPeakRelease = smoothingCoefficient (duckPeakReleaseMs, sampleRate);

    prepared = true;
    snapToTargets (targets);
}

void RoomDsp::configure (Network& network, int quality)
{
    network.quality = quality;
    network.numLines = quality == eco ? 8 : 16;
    network.cubic = quality == high;

    for (int i = 0; i < network.numLines; ++i)
    {
        const auto index = (size_t) i;
        network.lineMs[index] = lineMsTable[network.numLines == 16 ? index : 2 * index];
        network.rate[index] = twoPi * (0.31 + 0.97 * frac (i * 0.618034 + 0.17)) / sampleRate;
        network.phase[index] = twoPi * frac (i * 0.4142 + 0.29);
        network.sign[index] = (i / 2) % 2 == 0 ? 1.0 : -1.0;
    }

    network.modOffset.fill (0.0);
    network.inGain = 1.0 / std::sqrt (network.numLines / 2.0);
    network.outGain = network.inGain;
    network.hScale = 1.0 / std::sqrt ((double) network.numLines);
}

void RoomDsp::clear (Network& network)
{
    for (int i = 0; i < network.numLines; ++i)
        std::fill (network.lines[(size_t) i].begin(), network.lines[(size_t) i].end(), 0.0f);

    network.lp.fill (0.0);
    network.v.fill (0.0);
    network.shelfState.fill (0.0);
    network.ringing = false;
    network.ringFade = 1.0;
    network.ringSamples = 0;
    network.quietSamples = 0;
}

void RoomDsp::reset()
{
    std::fill (history.begin(), history.end(), 0.0f);

    for (auto& channel : allpass)
        for (auto& buffer : channel)
            std::fill (buffer.begin(), buffer.end(), 0.0f);

    for (auto& network : networks)
        network.running = false;

    auto& network = networks[(size_t) activeNetwork];
    configure (network, std::clamp (targets.quality, (int) eco, (int) high));
    clear (network);
    network.running = true;

    for (auto* filter : { &lowCut, &highCut })
        for (auto& channel : *filter)
            channel.reset();

    sampleCount = 0;
    knotStart = 0;
    freezeAmount = 0.0;
    duckEnvelope = 0.0;
    duckPeak = 0.0;

    // Phase 4 Wave 1: the sines start from the rate as it is, and the EQ and gate start clean.
    modRateNow = modRate.getCurrent();

    for (auto& band : eqBands)
        for (int ch = 0; ch < 2; ++ch)
            band.s1[ch] = band.s2[ch] = 0.0;

    eqLastQ = -1.0;
    updateEq();
    gateGain = 1.0;
    gateHoldCount = 0;

    updateLengths (size.isSettled(), predelay.isSettled());
    lastSize = size.getCurrent();
    lastPredelay = predelay.getCurrent();

    // Phase 4 Wave 2: the shelves start clean (configure and clear), switched on as Low decay is.
    shelfActive = ! (lowDecay.getCurrent() == 1.0 && lowDecay.isSettled());
    updateGains (network);
    lastDecay = decay.getCurrent();
    lastDamping = damping.getCurrent();
    gainsForScale = lastSize;
    lastLowDecay = lowDecay.getCurrent();
    updateModulation (network, 0);
    updateFilters();

    // Phase 4 Wave 3: the loop starts empty, running if Shimmer is up.
    resetShimmer();
    shimmerRunning = ! (shimmerAmount.getCurrent() == 0.0 && shimmerAmount.isSettled());
}

void RoomDsp::resetShimmer() noexcept
{
    for (auto* filter : { &loopLowCut, &loopHighCut })
        for (auto& channel : *filter)
            channel.reset();

    for (auto& filter : loopHighpassFilters)
        filter.reset();

    shifter.reset();
    shimmerPending.fill (0.0);
    shimmerInput = shimmerPeak = shimmerFeedback = 0.0;
    shimmerGuard = 1.0;
    shimmerHoldLeft = 0;
    loopSize = loopDecay = -1.0;
    loopLines = 0;
}

void RoomDsp::setTargets (const Targets& newTargets)
{
    targets = newTargets;

    predelay.setTarget (std::clamp (targets.predelayMs, 0.0, maxPredelayMs));

    // Decision 2: Size, Decay and Damping hold while frozen, and glide on once Freeze is off.
    if (! targets.freeze)
    {
        size.setTarget (std::clamp (targets.size, 0.0, 1.0));
        decay.setTarget (std::clamp (targets.decay, 0.1, 20.0));
        damping.setTarget (std::clamp (targets.damping, 0.0, 1.0));
        lowDecay.setTarget (std::clamp (targets.lowDecay, 0.25, 4.0));
    }

    attack.setTarget (std::clamp (targets.attackMs, 0.0, maxAttackMs));

    const auto nyquistLimit = 0.45 * sampleRate;
    diffusion.setTarget (std::clamp (targets.diffusion, 0.0, 1.0));
    mod.setTarget (std::clamp (targets.mod, 0.0, 1.0));
    width.setTarget (std::clamp (targets.width, -1.0, 2.0));
    logLowCut.setTarget (std::log (std::clamp (targets.lowCutHz, lowestCutHz, nyquistLimit)));
    logHighCut.setTarget (std::log (std::clamp (targets.highCutHz, lowestCutHz, nyquistLimit)));
    erGain.setTarget (std::max (0.0, targets.erGain));
    ducking.setTarget (std::clamp (targets.ducking, 0.0, 1.0));
    wet.setTarget (targets.wetGain);
    dry.setTarget (targets.dryGain);
    output.setTarget (targets.outputGain);
    bypass.setTarget (targets.bypass ? 1.0 : 0.0);

    // Phase 4 Wave 1.
    modRate.setTarget (std::clamp (targets.modRate, 0.25, 4.0));
    eqLogHz[0].setTarget (std::log (std::clamp (targets.eqLowHz, 20.0, nyquistLimit)));
    eqLogHz[1].setTarget (std::log (std::clamp (targets.eqMidHz, 20.0, nyquistLimit)));
    eqLogHz[2].setTarget (std::log (std::clamp (targets.eqHighHz, 20.0, nyquistLimit)));
    eqGain[0].setTarget (std::clamp (targets.eqLowDb, -12.0, 12.0));
    eqGain[1].setTarget (std::clamp (targets.eqMidDb, -12.0, 12.0));
    eqGain[2].setTarget (std::clamp (targets.eqHighDb, -12.0, 12.0));
    eqQ.setTarget (std::clamp (targets.eqMidQ, 0.3, 8.0));
    autoTrim.setTarget (targets.autoGain ? std::pow (10.0, autoGainDb (targets.size, targets.decay) / 20.0) : 1.0);

    // Phase 4 Wave 3.
    shimmerAmount.setTarget (std::clamp (targets.shimmer, 0.0, 1.0));

    const auto quality = std::clamp (targets.quality, (int) eco, (int) high);

    if (prepared && quality != networks[(size_t) activeNetwork].quality)
        switchQuality (quality);
}

void RoomDsp::snapToTargets (const Targets& newTargets)
{
    setTargets (newTargets);

    size.setTarget (std::clamp (targets.size, 0.0, 1.0));
    decay.setTarget (std::clamp (targets.decay, 0.1, 20.0));
    damping.setTarget (std::clamp (targets.damping, 0.0, 1.0));
    lowDecay.setTarget (std::clamp (targets.lowDecay, 0.25, 4.0));

    for (auto* smoother : { &predelay, &size, &decay, &diffusion, &mod, &width, &damping, &logLowCut, &logHighCut,
                            &erGain, &ducking, &wet, &dry, &output, &bypass,
                            &modRate, &eqLogHz[0], &eqLogHz[1], &eqLogHz[2], &eqGain[0], &eqGain[1], &eqGain[2], &eqQ, &autoTrim,
                            &lowDecay, &attack, &shimmerAmount })
        smoother->snapToTarget();

    if (prepared)
        reset();
}

// Decision 4: the old network stops taking input and rings out, and the new one starts empty.
// A network still ringing out from an earlier change is dropped.
void RoomDsp::switchQuality (int quality)
{
    auto& old = networks[(size_t) activeNetwork];
    old.ringing = true;
    old.ringSamples = 0;
    old.quietSamples = 0;
    old.ringFade = 1.0;

    activeNetwork = 1 - activeNetwork;
    auto& next = networks[(size_t) activeNetwork];
    configure (next, quality);
    clear (next);
    next.running = true;

    const auto scale = sizeScale (size.getCurrent());
    const auto perMs = sampleRate * 0.001;

    for (int i = 0; i < next.numLines; ++i)
    {
        const auto exact = next.lineMs[(size_t) i] * scale * perMs;
        next.length[(size_t) i] = size.isSettled() ? std::round (exact) : exact;
    }

    updateGains (next);
    updateModulation (next, knotStart);
}

int RoomDsp::getNumRunningNetworks() const noexcept
{
    return (networks[0].running ? 1 : 0) + (networks[1].running ? 1 : 0);
}

int RoomDsp::getActiveQuality() const noexcept
{
    return networks[(size_t) activeNetwork].quality;
}

// Lengths follow Size and Pre-delay per sample while they move, and are whole samples, as the
// model's, once they settle.
void RoomDsp::updateLengths (bool sizeRounded, bool predelayRounded) noexcept
{
    const auto scale = sizeScale (size.getCurrent());
    const auto perMs = sampleRate * 0.001;
    const auto shape = [] (double exact, bool rounded) { return rounded ? std::round (exact) : exact; };

    predelaySamples = shape (predelay.getCurrent() * perMs, predelayRounded);

    for (auto& network : networks)
        if (network.running)
            for (int i = 0; i < network.numLines; ++i)
                network.length[(size_t) i] = shape (network.lineMs[(size_t) i] * scale * perMs, sizeRounded);

    for (size_t ch = 0; ch < 2; ++ch)
    {
        for (size_t k = 0; k < 4; ++k)
            allpassLength[ch][k] = std::max (1.0, shape (diffusionMs[ch][k] * scale * perMs, sizeRounded));

        for (size_t k = 0; k < (size_t) erTaps; ++k)
            reflectionOffset[ch][k] = predelaySamples + shape (reflections[ch][k].ms * scale * perMs, sizeRounded);
    }
}

void RoomDsp::updateGains (Network& network) noexcept
{
    const auto t = decay.getCurrent();
    const auto highs = t * (1.0 - 0.7 * damping.getCurrent());

    for (int i = 0; i < network.numLines; ++i)
    {
        const auto d = network.length[(size_t) i];
        const auto g = std::pow (10.0, -3.0 * d / (sampleRate * t));
        const auto gh = std::pow (10.0, -3.0 * d / (sampleRate * highs));
        const auto ratio = gh / g;
        network.g[(size_t) i] = g;
        network.bco[(size_t) i] = (1.0 - ratio) / (1.0 + ratio);
    }

    // Phase 4 Wave 2: Low decay's shelf, a bilinear first-order low shelf at 250 Hz with the extra
    // gain per pass at DC that makes the lows decay for Low decay x Decay, and 1 at Nyquist.
    if (! shelfActive)
        return;

    const auto multiplier = lowDecay.getCurrent();
    const auto wc = std::tan (std::numbers::pi * std::min (lowShelfHz, 0.45 * sampleRate) / sampleRate);

    for (int i = 0; i < network.numLines; ++i)
    {
        const auto d = network.length[(size_t) i];
        const auto glow = std::pow (10.0, -3.0 * d / (sampleRate * t * multiplier)) / network.g[(size_t) i];
        const auto a0 = 1.0 + wc;
        network.shelfB0[(size_t) i] = (1.0 + glow * wc) / a0;
        network.shelfB1[(size_t) i] = (glow * wc - 1.0) / a0;
        network.shelfA1[(size_t) i] = (wc - 1.0) / a0;
    }
}

// The exact sine at each 16th sample of the absolute count, linear in between (MOD_STEP).
void RoomDsp::updateModulation (Network& network, int64_t knot) noexcept
{
    for (int i = 0; i < network.numLines; ++i)
    {
        const auto index = (size_t) i;
        const auto rate = network.rate[index] * modRateNow;
        network.m0[index] = std::sin (network.phase[index] + rate * (double) knot + network.modOffset[index]);
        network.m1[index] = std::sin (network.phase[index] + rate * (double) (knot + modStep) + network.modOffset[index]);
    }
}

void RoomDsp::updateFilters() noexcept
{
    lastLowLog = logLowCut.getCurrent();
    lastHighLog = logHighCut.getCurrent();

    for (auto& filter : lowCut)
        filter.setButterworthHighPass (std::exp (lastLowLog), sampleRate);

    for (auto& filter : highCut)
        filter.setButterworthLowPass (std::exp (lastHighLog), sampleRate);

    // Phase 4 Wave 3: Shimmer's loop has its own copies, with the same settings.
    for (auto& filter : loopLowCut)
        filter.setButterworthHighPass (std::exp (lastLowLog), sampleRate);

    for (auto& filter : loopHighCut)
        filter.setButterworthLowPass (std::exp (lastHighLog), sampleRate);
}

// Phase 4 Wave 1: a new Mod rate takes effect at a knot, with each line's offset moved so its
// sine carries on from where it was.
void RoomDsp::applyModRate() noexcept
{
    const auto next = modRate.getCurrent();

    if (next == modRateNow)
        return;

    for (auto& network : networks)
        for (int i = 0; i < network.numLines; ++i)
            network.modOffset[(size_t) i] += network.rate[(size_t) i] * (modRateNow - next) * (double) knotStart;

    modRateNow = next;
}

// Phase 4 Wave 1: the reverb EQ's coefficients, when a band's settings have moved. A band whose
// gain has settled at 0 drops out of the path, with its state cleared.
void RoomDsp::updateEq() noexcept
{
    namespace matched = fancy::dsp::matched;

    const matched::BandType types[] { matched::BandType::lowShelf, matched::BandType::band, matched::BandType::highShelf };
    const auto q = eqQ.getCurrent();

    for (size_t b = 0; b < 3; ++b)
    {
        auto& band = eqBands[b];
        const auto gain = eqGain[b].getCurrent();
        const auto hz = std::exp (eqLogHz[b].getCurrent());
        const auto active = ! (gain == 0.0 && eqGain[b].isSettled());

        if (! active)
        {
            if (band.active)
                for (int ch = 0; ch < 2; ++ch)
                    band.s1[ch] = band.s2[ch] = 0.0;

            band.active = false;
            continue;
        }

        if (band.active && hz == eqLastHz[b] && gain == eqLastGain[b] && (b != 1 || q == eqLastQ))
            continue;

        matched::BandSpec spec;
        spec.type = types[b];
        spec.frequencyHz = hz;
        spec.gainDb = gain;
        spec.q = b == 1 ? q : matched::butterworthQ;
        spec.order = 2;
        spec.on = true;

        const auto filter = matched::design (spec, sampleRate);
        band.coefficients = filter.numSections > 0 ? filter.sections[0] : matched::Section {};
        band.active = true;
        eqLastHz[b] = hz;
        eqLastGain[b] = gain;
    }

    eqLastQ = q;
}

void RoomDsp::runKnot() noexcept
{
    knotStart = sampleCount;
    applyModRate();
    updateEq();

    // Phase 4 Wave 2: Low decay's shelves run until it settles back at x1, and leave clean.
    const auto shelfWanted = ! (lowDecay.getCurrent() == 1.0 && lowDecay.isSettled());
    const auto shelfChanged = shelfWanted != shelfActive;

    if (shelfChanged)
    {
        shelfActive = shelfWanted;

        if (! shelfActive)
            for (auto& network : networks)
                network.shelfState.fill (0.0);
    }

    const auto gainsChanged = decay.getCurrent() != lastDecay || damping.getCurrent() != lastDamping || size.getCurrent() != gainsForScale
                              || (shelfActive && (shelfChanged || lowDecay.getCurrent() != lastLowDecay));

    for (auto& network : networks)
    {
        if (! network.running)
            continue;

        updateModulation (network, knotStart);

        if (gainsChanged)
            updateGains (network);
    }

    lastDecay = decay.getCurrent();
    lastDamping = damping.getCurrent();
    gainsForScale = size.getCurrent();
    lastLowDecay = lowDecay.getCurrent();

    if (logLowCut.getCurrent() != lastLowLog || logHighCut.getCurrent() != lastHighLog)
        updateFilters();
}

double RoomDsp::runNetwork (Network& network, const double* diffused, double f, double t, double depth, double& lateRight) noexcept
{
    const auto mask = network.mask;
    const auto w = (int) (sampleCount & (int64_t) mask);
    const auto frozen = f > 0.0;
    auto lateLeft = 0.0;
    lateRight = 0.0;

    // Read the lines, damp them, sum the outputs.
    for (int i = 0; i < network.numLines; ++i)
    {
        const auto index = (size_t) i;
        const auto* b = network.lines[index].data();
        const auto length = network.length[index];
        const auto m = network.m0[index] + t * (network.m1[index] - network.m0[index]);
        const auto modulated = length + depth * m;
        const auto dd = (1.0 - f) * modulated + f * length;
        const auto whole = (int) std::floor (dd);
        const auto fr = dd - (double) whole;

        double read;

        if (network.cubic)
        {
            // 4-point Lagrange, written as a cubic in fr.
            const auto xm1 = (double) b[(w - whole + 1) & mask];
            const auto x0 = (double) b[(w - whole) & mask];
            const auto x1 = (double) b[(w - whole - 1) & mask];
            const auto x2 = (double) b[(w - whole - 2) & mask];
            const auto c3 = (x2 - xm1) / 6.0 + (x0 - x1) * 0.5;
            const auto c2 = (xm1 + x1) * 0.5 - x0;
            const auto c1 = x1 - x2 / 6.0 - x0 * 0.5 - xm1 / 3.0;
            read = ((c3 * fr + c2) * fr + c1) * fr + x0;
        }
        else
        {
            const auto a0 = (double) b[(w - whole) & mask];
            read = a0 + fr * ((double) b[(w - whole - 1) & mask] - a0);
        }

        auto gi = network.g[index];
        auto bi = network.bco[index];

        if (frozen)
        {
            gi += f * (1.0 - gi);
            bi *= 1.0 - f;
        }

        auto y = gi * (1.0 - bi) * read + bi * network.lp[index];
        network.lp[index] = y;

        // Phase 4 Wave 2: Low decay's shelf after the damping. Freeze fades it out with the rest
        // of the loop's loss, so a frozen tail holds.
        if (shelfActive)
        {
            const auto shelved = network.shelfB0[index] * y + network.shelfState[index];
            network.shelfState[index] = network.shelfB1[index] * y - network.shelfA1[index] * shelved;
            y = frozen ? y + (1.0 - f) * (shelved - y) : shelved;
        }

        network.v[index] = y;

        if ((i & 1) != 0)
            lateRight += network.sign[index] * y;
        else
            lateLeft += network.sign[index] * y;
    }

    // Mix: fast Walsh-Hadamard, orthonormal with hScale.
    auto* v = network.v.data();

    for (int h = 1; h < network.numLines; h *= 2)
    {
        for (int start = 0; start < network.numLines; start += 2 * h)
        {
            for (int j = start; j < start + h; ++j)
            {
                const auto a = v[j], bb = v[j + h];
                v[j] = a + bb;
                v[j + h] = a - bb;
            }
        }
    }

    const auto inputGain = network.ringing ? 0.0 : network.inGain * (1.0 - f);

    for (int i = 0; i < network.numLines; ++i)
    {
        const auto index = (size_t) i;
        network.lines[index][(size_t) w] = (float) (v[i] * network.hScale + inputGain * network.sign[index] * diffused[i & 1]);
    }

    lateRight *= network.outGain;
    return lateLeft * network.outGain;
}

void RoomDsp::process (float* const* channels, int numChannels, int numSamples,
                       const float* const* sidechain, int numSidechain) noexcept
{
    if (! prepared || numChannels <= 0)
        return;

    const auto ringOutSamples = (int64_t) (ringOutSeconds * sampleRate);
    const auto ringFadeStep = 1.0 / std::max (1.0, ringFadeSeconds * sampleRate);
    const auto quietLimit = (int) (quietSeconds * sampleRate);
    const auto stereo = numChannels > 1;

    // Phase 4 Wave 2: Duck from Sidechain, when there's a sidechain to read.
    const auto keyed = targets.duckSidechain && sidechain != nullptr && numSidechain > 0 && sidechain[0] != nullptr;
    const auto keyedStereo = keyed && numSidechain > 1 && sidechain[1] != nullptr;
    const auto listen = targets.listenDucking;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto inLeft = (double) channels[0][i];
        const auto inRight = stereo ? (double) channels[1][i] : inLeft;
        auto xL = inLeft, xR = inRight;

        if (targets.monoSum)
            xL = xR = 0.5 * (inLeft + inRight);

        // Settings for this sample.
        const auto sizeNow = size.next();
        const auto predelayNow = predelay.next();

        if (sizeNow != lastSize || predelayNow != lastPredelay)
        {
            updateLengths (size.isSettled(), predelay.isSettled());
            lastSize = sizeNow;
            lastPredelay = predelayNow;
        }

        decay.next();
        damping.next();
        logLowCut.next();
        logHighCut.next();

        const auto apg = 0.75 * diffusion.next();
        const auto depth = mod.next() * modMaxMs * sampleRate * 0.001;
        const auto widthNow = width.next();
        const auto erNow = erGain.next();
        const auto duckNow = ducking.next();
        const auto wetNow = wet.next();
        const auto dryNow = dry.next();
        const auto outputNow = output.next();
        const auto bypassNow = bypass.next();

        // Phase 4 Wave 1.
        modRate.next();
        eqQ.next();

        for (size_t b = 0; b < 3; ++b)
        {
            eqLogHz[b].next();
            eqGain[b].next();
        }

        const auto autoTrimNow = autoTrim.next();

        // Phase 4 Wave 2.
        lowDecay.next();
        const auto attackNow = attack.next();

        // Phase 4 Wave 3: Shimmer runs while it isn't settled at 0; stopping empties the loop.
        const auto shimmerNow = shimmerAmount.next();
        const auto shimmerOn = ! (shimmerNow == 0.0 && shimmerAmount.isSettled());

        if (shimmerOn != shimmerRunning)
        {
            resetShimmer();
            shimmerRunning = shimmerOn;
        }

        freezeAmount = targets.freeze ? std::min (1.0, freezeAmount + freezeStep) : std::max (0.0, freezeAmount - freezeStep);

        if (sampleCount % modStep == 0)
        {
            runKnot();

            // Phase 4 Wave 3: the loop gain and the guard's hold follow Size, Decay and Quality
            // at the knots, so block sizes don't change them.
            if (shimmerRunning)
            {
                const auto sizeKnot = size.getCurrent(), decayKnot = decay.getCurrent();
                const auto lines = networks[(size_t) activeNetwork].numLines;

                if (sizeKnot != loopSize || decayKnot != loopDecay || lines != loopLines)
                {
                    loopSize = sizeKnot;
                    loopDecay = decayKnot;
                    loopLines = lines;
                    loopScale = 0.9 / std::sqrt (loopEnergy (sizeKnot, decayKnot, lines));

                    const auto rt60 = std::max (0.05, decayKnot) * sampleRate;
                    shimmerHold = (int) std::min (rt60, 60.0 * sampleRate);
                    shimmerLetGo = std::pow (10.0, -6.0 / rt60); // in power: 60 dB per Decay
                }
            }
        }

        // Phase 4 Wave 3: the guard's reference, the input's 50 ms RMS, held at its peak.
        if (shimmerRunning)
        {
            shimmerInput += (0.5 * (xL * xL + xR * xR) - shimmerInput) * shimmerRms;

            if (shimmerInput >= shimmerPeak)
            {
                shimmerPeak = shimmerInput;
                shimmerHoldLeft = shimmerHold;
            }
            else if (shimmerHoldLeft > 0)
            {
                --shimmerHoldLeft;
            }
            else
            {
                shimmerPeak *= shimmerLetGo;
            }
        }

        const auto t = (double) (sampleCount - knotStart) / modStep;

        // The input's history, for Pre-delay and the reflections.
        const auto hw = (int) (sampleCount & (int64_t) historyMask);
        history[(size_t) (2 * hw)] = (float) xL;
        history[(size_t) (2 * hw + 1)] = (float) xR;

        const auto readHistory = [this, hw] (double offset, double& left, double& right)
        {
            const auto whole = (int) std::floor (offset);
            const auto fr = offset - (double) whole;
            const auto a = (size_t) (2 * ((hw - whole) & historyMask));
            const auto b = (size_t) (2 * ((hw - whole - 1) & historyMask));
            left = (double) history[a] + fr * ((double) history[b] - (double) history[a]);
            right = (double) history[a + 1] + fr * ((double) history[b + 1] - (double) history[a + 1]);
        };

        // Early reflections, from the input delayed by Pre-delay.
        auto eL = 0.0, eR = 0.0;

        for (size_t k = 0; k < (size_t) erTaps; ++k)
        {
            double hL, hR;
            readHistory (reflectionOffset[0][k], hL, hR);
            eL += reflections[0][k].gain * (0.75 * hL + 0.25 * hR);
            readHistory (reflectionOffset[1][k], hL, hR);
            eR += reflections[1][k].gain * (0.25 * hL + 0.75 * hR);
        }

        // Diffusers on the pre-delayed input.
        double diffused[2];

        if (attackNow == 0.0 && attack.isSettled())
        {
            readHistory (predelaySamples, diffused[0], diffused[1]);
        }
        else
        {
            // Phase 4 Wave 2 (item 20): 16 taps spread over Attack, rising, before the diffusers,
            // so the late tail swells in. Close together they add in phase, so their scaling
            // moves from keeping the amplitude at 0 ms to keeping the energy from 10 ms.
            const auto span = attackNow * sampleRate * 0.001;
            const auto blend = std::min (1.0, attackNow / attackEnergyMs);
            const auto scale = 1.0 / (blend + (1.0 - blend) * attackWeightSum);
            diffused[0] = diffused[1] = 0.0;

            for (int k = 0; k < attackTaps; ++k)
            {
                double hL, hR;
                readHistory (predelaySamples + span * attackTapPosition (k), hL, hR);
                const auto w = attackWeight[(size_t) k] * scale;
                diffused[0] += w * hL;
                diffused[1] += w * hR;
            }
        }

        // Phase 4 Wave 3: Shimmer's feedback joins the diffusers' input, after the Attack taps.
        if (shimmerRunning)
        {
            diffused[0] += shimmerPending[0];
            diffused[1] += shimmerPending[1];
        }

        for (size_t ch = 0; ch < 2; ++ch)
        {
            auto s = diffused[ch];

            for (size_t k = 0; k < 4; ++k)
            {
                auto& buffer = allpass[ch][k];
                const auto length = allpassLength[ch][k];
                const auto whole = (int) std::floor (length);
                const auto fr = length - (double) whole;
                const auto pos = (int) (sampleCount & (int64_t) allpassMask);
                const auto a = (double) buffer[(size_t) ((pos - whole) & allpassMask)];
                const auto delayed = a + fr * ((double) buffer[(size_t) ((pos - whole - 1) & allpassMask)] - a);
                const auto y = -apg * s + delayed;
                buffer[(size_t) pos] = (float) (s + apg * y);
                s = y;
            }

            diffused[ch] = s;
        }

        // The late network, and the one ringing out after a Quality change.
        auto lateL = 0.0, lateR = 0.0;

        for (auto& network : networks)
        {
            if (! network.running)
                continue;

            double right;
            const auto left = runNetwork (network, diffused, freezeAmount, t, depth, right);

            if (network.ringing)
            {
                ++network.ringSamples;
                network.quietSamples = std::max (std::abs (left), std::abs (right)) < quietLevel ? network.quietSamples + 1 : 0;

                if (network.ringSamples >= ringOutSamples)
                    network.ringFade = std::max (0.0, network.ringFade - ringFadeStep);

                if (network.quietSamples >= quietLimit || network.ringFade <= 0.0)
                    network.running = false;
            }

            lateL += left * network.ringFade;
            lateR += right * network.ringFade;
        }

        // Phase 4 Wave 3: the late output, through the loop, for the next sample.
        if (shimmerRunning)
        {
            const double cut[2] { loopHighCut[0].process (loopLowCut[0].process (lateL)),
                                  loopHighCut[1].process (loopLowCut[1].process (lateR)) };
            const double in[2] { loopHighpassFilters[0].tick (loopHighpass, cut[0]).high,
                                 loopHighpassFilters[1].tick (loopHighpass, cut[1]).high };
            double shifted[2];
            shifter.process (in, shifted, 2, targets.shimmerRatio);

            const auto gain = shimmerNow * loopScale;
            const auto l = gain * shifted[0], r = gain * shifted[1];
            shimmerFeedback += (0.5 * (l * l + r * r) - shimmerFeedback) * shimmerRms;

            const auto wanted = shimmerFeedback > shimmerPeak && shimmerFeedback > 1.0e-30 ? std::sqrt (shimmerPeak / shimmerFeedback) : 1.0;
            shimmerGuard += (wanted - shimmerGuard) * (wanted < shimmerGuard ? shimmerAttack : shimmerRelease);

            shimmerPending[0] = std::isfinite (l) ? l * shimmerGuard : 0.0;
            shimmerPending[1] = std::isfinite (r) ? r * shimmerGuard : 0.0;
        }

        auto wetL = lateL + erNow * eL;
        auto wetR = lateR + erNow * eR;

        wetL = highCut[0].process (lowCut[0].process (wetL));
        wetR = highCut[1].process (lowCut[1].process (wetR));

        // Phase 4 Wave 1: the reverb EQ, each band only while it's in use.
        for (auto& band : eqBands)
        {
            if (! band.active)
                continue;

            const auto& c = band.coefficients;
            double* values[2] { &wetL, &wetR };

            for (int ch = 0; ch < 2; ++ch)
            {
                const auto x = *values[ch];
                const auto y = c.b0 * x + band.s1[ch];
                band.s1[ch] = c.b1 * x - c.a1 * y + band.s2[ch];
                band.s2[ch] = c.b2 * x - c.a2 * y;
                *values[ch] = y;
            }
        }

        // Width: mid/side on the wet signal only (decision 6).
        const auto mid = 0.5 * (wetL + wetR), side = 0.5 * (wetL - wetR);
        wetL = mid + widthNow * side;
        wetR = mid - widthNow * side;

        // Ducking (decision 7): a peak detector on the dry input sets how far down the reverb
        // goes, and the reduction follows it with 10 ms attack and 300 ms release, in dB, so the
        // reverb comes back 300 ms after the playing stops whatever the level was.
        const auto level = std::max (std::abs (inLeft), std::abs (inRight));

        // Phase 4 Wave 2 (item 21): the same ducker, keyed by the sidechain when chosen.
        const auto key = keyed ? std::max (std::abs ((double) sidechain[0][i]), keyedStereo ? std::abs ((double) sidechain[1][i]) : 0.0)
                               : level;
        duckPeak = key > duckPeak ? key : duckPeak + (key - duckPeak) * duckPeakRelease;

        auto duckGain = 1.0;

        if (duckNow > 0.0 || duckEnvelope > 0.0)
        {
            const auto db = 20.0 * std::log10 (std::max (duckPeak, 1.0e-9));
            const auto wanted = duckDepthDb * duckNow * std::clamp ((db - duckNoneDb) / (duckFullDb - duckNoneDb), 0.0, 1.0);
            duckEnvelope += (wanted - duckEnvelope) * (wanted > duckEnvelope ? duckAttack : duckRelease);

            if (duckEnvelope < 1.0e-6)
                duckEnvelope = 0.0;

            duckGain = std::pow (10.0, -duckEnvelope / 20.0);
        }

        auto wetGain = wetNow * duckGain;

        // Phase 4 Wave 1: the gate reads the input, so the reverb stops when the drum does.
        if (targets.gate || gateGain < 1.0)
        {
            const auto holdSamples = (int) (std::clamp (targets.gateHoldMs, 10.0, 500.0) * 0.001 * sampleRate);
            const auto closeStep = 1.0 / std::max (1.0, std::clamp (targets.gateDecayMs, 5.0, 500.0) * 0.001 * sampleRate);
            const auto open = ! targets.gate || 20.0 * std::log10 (std::max (level, 1.0e-9)) > targets.gateThresholdDb;

            if (open)
            {
                gateHoldCount = holdSamples;
                gateGain = std::min (1.0, gateGain + gateOpenStep);
            }
            else if (gateHoldCount > 0)
            {
                --gateHoldCount;
                gateGain = std::min (1.0, gateGain + gateOpenStep);
            }
            else
            {
                gateGain = std::max (0.0, gateGain - closeStep);
            }

            wetGain *= gateGain;
        }

        if (targets.autoGain || ! autoTrim.isSettled() || autoTrimNow != 1.0)
            wetGain *= autoTrimNow;

        // Phase 4 Wave 2: Listen to ducking plays only what Ducking takes away from the wet
        // signal, with the gate and Auto gain as they are. It's temporary (§0.3).
        auto dryGain = dryNow;

        if (listen)
        {
            wetGain = duckGain > 0.0 ? wetGain * (1.0 - duckGain) / duckGain : 0.0;
            dryGain = 0.0;
        }

        if (stereo)
        {
            const auto outL = (wetL * wetGain + inLeft * dryGain) * outputNow;
            const auto outR = (wetR * wetGain + inRight * dryGain) * outputNow;
            channels[0][i] = (float) (outL + (inLeft - outL) * bypassNow);
            channels[1][i] = (float) (outR + (inRight - outR) * bypassNow);
        }
        else
        {
            const auto out = (0.5 * (wetL + wetR) * wetGain + inLeft * dryGain) * outputNow;
            channels[0][i] = (float) (out + (inLeft - out) * bypassNow);
        }

        ++sampleCount;
    }
}
