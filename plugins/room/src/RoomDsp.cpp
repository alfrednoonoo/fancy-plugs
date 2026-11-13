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

RoomDsp::RoomDsp()
{
    for (int ch = 0; ch < 2; ++ch)
        for (int k = 0; k < erTaps; ++k)
            reflections[(size_t) ch][(size_t) k] = reflection (ch, k);
}

double RoomDsp::longestReflectionSeconds (double sizeValue) noexcept
{
    auto longest = 0.0;

    for (int ch = 0; ch < 2; ++ch)
        for (int k = 0; k < erTaps; ++k)
            longest = std::max (longest, reflection (ch, k).ms);

    return longest * sizeScale (std::clamp (sizeValue, 0.0, 1.0)) * 0.001;
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
    const auto historySize = powerOfTwoAbove ((maxPredelayMs + longestReflectionMs) * perMs + 4.0);
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

    updateLengths (size.isSettled(), predelay.isSettled());
    lastSize = size.getCurrent();
    lastPredelay = predelay.getCurrent();
    updateGains (network);
    lastDecay = decay.getCurrent();
    lastDamping = damping.getCurrent();
    gainsForScale = lastSize;
    updateModulation (network, 0);
    updateFilters();
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
    }

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

    for (auto* smoother : { &predelay, &size, &decay, &diffusion, &mod, &width, &damping, &logLowCut, &logHighCut,
                            &erGain, &ducking, &wet, &dry, &output, &bypass })
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
}

// The exact sine at each 16th sample of the absolute count, linear in between (MOD_STEP).
void RoomDsp::updateModulation (Network& network, int64_t knot) noexcept
{
    for (int i = 0; i < network.numLines; ++i)
    {
        const auto index = (size_t) i;
        network.m0[index] = std::sin (network.phase[index] + network.rate[index] * (double) knot);
        network.m1[index] = std::sin (network.phase[index] + network.rate[index] * (double) (knot + modStep));
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
}

void RoomDsp::runKnot() noexcept
{
    knotStart = sampleCount;

    const auto gainsChanged = decay.getCurrent() != lastDecay || damping.getCurrent() != lastDamping || size.getCurrent() != gainsForScale;

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

        const auto y = gi * (1.0 - bi) * read + bi * network.lp[index];
        network.lp[index] = y;
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

void RoomDsp::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    if (! prepared || numChannels <= 0)
        return;

    const auto ringOutSamples = (int64_t) (ringOutSeconds * sampleRate);
    const auto ringFadeStep = 1.0 / std::max (1.0, ringFadeSeconds * sampleRate);
    const auto quietLimit = (int) (quietSeconds * sampleRate);
    const auto stereo = numChannels > 1;

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

        freezeAmount = targets.freeze ? std::min (1.0, freezeAmount + freezeStep) : std::max (0.0, freezeAmount - freezeStep);

        if (sampleCount % modStep == 0)
            runKnot();

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
        readHistory (predelaySamples, diffused[0], diffused[1]);

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

        auto wetL = lateL + erNow * eL;
        auto wetR = lateR + erNow * eR;

        wetL = highCut[0].process (lowCut[0].process (wetL));
        wetR = highCut[1].process (lowCut[1].process (wetR));

        // Width: mid/side on the wet signal only (decision 6).
        const auto mid = 0.5 * (wetL + wetR), side = 0.5 * (wetL - wetR);
        wetL = mid + widthNow * side;
        wetR = mid - widthNow * side;

        // Ducking (decision 7): a peak detector on the dry input sets how far down the reverb
        // goes, and the reduction follows it with 10 ms attack and 300 ms release, in dB, so the
        // reverb comes back 300 ms after the playing stops whatever the level was.
        const auto level = std::max (std::abs (inLeft), std::abs (inRight));
        duckPeak = level > duckPeak ? level : duckPeak + (level - duckPeak) * duckPeakRelease;

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

        const auto wetGain = wetNow * duckGain;

        if (stereo)
        {
            const auto outL = (wetL * wetGain + inLeft * dryNow) * outputNow;
            const auto outR = (wetR * wetGain + inRight * dryNow) * outputNow;
            channels[0][i] = (float) (outL + (inLeft - outL) * bypassNow);
            channels[1][i] = (float) (outR + (inRight - outR) * bypassNow);
        }
        else
        {
            const auto out = (0.5 * (wetL + wetR) * wetGain + inLeft * dryNow) * outputNow;
            channels[0][i] = (float) (out + (inLeft - out) * bypassNow);
        }

        ++sampleCount;
    }
}
