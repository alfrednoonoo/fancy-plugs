#include "IrBuilder.h"

#include <fancy/dsp/MatchedFilters.h>

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>

namespace verb
{

namespace
{

constexpr int zeroCrossings = 64;
constexpr int tableResolution = 512;
constexpr double kaiserBeta = 12.0;
constexpr double passbandFactor = 0.94;
constexpr double pi = juce::MathConstants<double>::pi;
constexpr double sixtyDbInNepers = 6.907755278982137;
constexpr int maxEchoesShown = 256;

// LL and RR, then the true-stereo cross paths RL (right in, left out) and LR (Phase 3 Q2.8).
// The cross paths stay empty unless a true-stereo source is added, so every module below works
// on exactly the samples it did before.
using Channels = std::array<std::vector<double>, 4>;

constexpr size_t crossToLeft = 2, crossToRight = 3;

bool isTrueStereo (const Channels& ir) noexcept
{
    return ! ir[crossToLeft].empty();
}

void makeTrueStereo (Channels& ir)
{
    if (! isTrueStereo (ir))
    {
        ir[crossToLeft].assign (ir[0].size(), 0.0);
        ir[crossToRight].assign (ir[0].size(), 0.0);
    }
}

double besselI0 (double x) noexcept
{
    auto sum = 1.0, term = 1.0;

    for (int k = 1; k < 64; ++k)
    {
        term *= (x * 0.5 / k) * (x * 0.5 / k);
        sum += term;

        if (term < sum * 1.0e-18)
            break;
    }

    return sum;
}

// sinc × Kaiser, from 0 to zeroCrossings, with a guard point for the interpolation.
const std::vector<double>& kernelTable()
{
    static const std::vector<double> table = []
    {
        std::vector<double> values ((size_t) (zeroCrossings * tableResolution + 2), 0.0);
        const auto denominator = besselI0 (kaiserBeta);

        for (int i = 0; i <= zeroCrossings * tableResolution; ++i)
        {
            const auto x = (double) i / tableResolution;
            const auto position = x / zeroCrossings;
            const auto window = besselI0 (kaiserBeta * std::sqrt (std::max (0.0, 1.0 - position * position))) / denominator;
            const auto sinc = x == 0.0 ? 1.0 : std::sin (pi * x) / (pi * x);
            values[(size_t) i] = sinc * window;
        }

        return values;
    }();

    return table;
}

size_t lengthOf (const Channels& ir) noexcept
{
    return ir[0].size();
}

void growTo (Channels& ir, size_t length)
{
    const auto paths = isTrueStereo (ir) ? ir.size() : 2;

    for (size_t ch = 0; ch < paths; ++ch)
        if (ir[ch].size() < length)
            ir[ch].resize (length, 0.0);
}

std::vector<double> convertChannel (const juce::AudioBuffer<float>& source, int channel, double ratio)
{
    const auto* samples = source.getReadPointer (channel);
    std::vector<double> input ((size_t) source.getNumSamples());

    for (size_t i = 0; i < input.size(); ++i)
        input[i] = samples[i];

    if (std::abs (ratio - 1.0) < 1.0e-9)
        return input;

    return resample (input.data(), (int) input.size(), ratio);
}

// True stereo (Phase 3 Q2.8): a four-channel file is LL, LR, RL, RR; a pair is the left
// input's file (LL, LR) and the right input's (RL, RR), each mono or stereo.
void addTrueStereoFile (Channels& ir, const ModuleSpec& module, double sampleRate, double gain)
{
    const auto& first = module.file->audio;
    const auto* second = module.pairFile != nullptr ? &module.pairFile->audio : nullptr;
    const auto firstRatio = sampleRate / module.file->sampleRate;
    const auto secondRatio = second != nullptr ? sampleRate / module.pairFile->sampleRate : firstRatio;
    const auto maxLength = (size_t) (maxIrSeconds * sampleRate);

    std::array<std::vector<double>, 4> paths; // LL, LR, RL, RR

    paths[0] = convertChannel (first, 0, firstRatio);
    paths[1] = convertChannel (first, std::min (1, first.getNumChannels() - 1), firstRatio);

    if (second != nullptr)
    {
        paths[2] = convertChannel (*second, 0, secondRatio);
        paths[3] = convertChannel (*second, std::min (1, second->getNumChannels() - 1), secondRatio);
    }
    else
    {
        paths[2] = convertChannel (first, 2, firstRatio);
        paths[3] = convertChannel (first, 3, firstRatio);
    }

    size_t length = 0;

    for (const auto& path : paths)
        length = std::max (length, std::min (maxLength, path.size()));

    for (auto& path : paths)
        path.resize (std::max (path.size(), length), 0.0);

    if (module.mono)
    {
        // Mix to mono sums every path to one direct IR: what the stereo output gives summed to mono.
        growTo (ir, length);

        for (size_t i = 0; i < length; ++i)
        {
            const auto sum = (paths[0][i] + paths[1][i] + paths[2][i] + paths[3][i]) * 0.5 * gain;
            ir[0][i] += sum;
            ir[1][i] += sum;
        }

        return;
    }

    if (length == 0)
        return;

    // Grow first: cross paths made on an empty IR would be empty, which reads as "not true stereo".
    growTo (ir, length);
    makeTrueStereo (ir);

    for (size_t i = 0; i < length; ++i)
    {
        ir[0][i] += paths[0][i] * gain;              // LL
        ir[crossToRight][i] += paths[1][i] * gain;   // LR
        ir[crossToLeft][i] += paths[2][i] * gain;    // RL
        ir[1][i] += paths[3][i] * gain;              // RR
    }
}

void addFile (Channels& ir, const ModuleSpec& module, double sampleRate)
{
    if (module.file == nullptr || module.file->audio.getNumSamples() == 0)
        return;

    const auto& source = module.file->audio;
    const auto ratio = sampleRate / module.file->sampleRate;
    const auto sourceChannels = std::min (source.getNumChannels(), 2);
    const auto maxLength = (size_t) (maxIrSeconds * sampleRate);

    auto gain = std::pow (10.0, gainModeTrimDb (module.gainMode) / 20.0);

    if (gainModeUsesSrc (module.gainMode))
        gain /= ratio; // more samples per second would otherwise make the convolution louder

    const auto pairReady = module.pairFile != nullptr && module.pairFile->audio.getNumSamples() > 0;

    if (module.trueStereo && (pairReady || source.getNumChannels() >= 4))
    {
        addTrueStereoFile (ir, module, sampleRate, gain);
        return;
    }

    Channels converted;

    for (int ch = 0; ch < 2; ++ch)
    {
        const auto sourceChannel = std::min (ch, sourceChannels - 1);
        const auto* samples = source.getReadPointer (sourceChannel);

        std::vector<double> input ((size_t) source.getNumSamples());

        for (size_t i = 0; i < input.size(); ++i)
            input[i] = samples[i];

        if (std::abs (ratio - 1.0) < 1.0e-9)
            converted[(size_t) ch] = std::move (input);
        else if (ch == 0 || sourceChannels > 1)
            converted[(size_t) ch] = resample (input.data(), (int) input.size(), ratio);
        else
            converted[1] = converted[0];
    }

    if (module.mono)
    {
        for (size_t i = 0; i < converted[0].size(); ++i)
            converted[0][i] = converted[1][i] = (converted[0][i] + converted[1][i]) * 0.5;
    }

    const auto length = std::min (converted[0].size(), maxLength);
    growTo (ir, length);

    for (size_t ch = 0; ch < 2; ++ch)
        for (size_t i = 0; i < length; ++i)
            ir[ch][i] += converted[ch][i] * gain;
}

struct Biquad
{
    double b0, b1, b2, a1, a2;
    double s1 = 0.0, s2 = 0.0;

    double run (double x) noexcept
    {
        const auto y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }
};

Biquad butterworthSection (double hz, double sampleRate, double q, bool highpass) noexcept
{
    const auto w = 2.0 * pi * hz / sampleRate;
    const auto alpha = std::sin (w) / (2.0 * q);
    const auto c = std::cos (w);
    const auto a0 = 1.0 + alpha;

    if (highpass)
        return { (1.0 + c) * 0.5 / a0, -(1.0 + c) / a0, (1.0 + c) * 0.5 / a0, -2.0 * c / a0, (1.0 - alpha) / a0 };

    return { (1.0 - c) * 0.5 / a0, (1.0 - c) / a0, (1.0 - c) * 0.5 / a0, -2.0 * c / a0, (1.0 - alpha) / a0 };
}

// 24 dB/oct. Lowpass at the top of its range and Highpass at the bottom are off.
void applyLpHp (Channels& ir, const ModuleSpec& module, double sampleRate)
{
    const auto& settings = settingsOf (ModuleType::lpHp);
    const auto lowpass = (double) module.values[0];
    const auto highpass = (double) module.values[1];
    const auto useLowpass = module.values[0] < settings[0].high && lowpass < sampleRate * 0.49;
    const auto useHighpass = module.values[1] > settings[1].low && highpass < sampleRate * 0.49;

    for (auto& channel : ir)
    {
        if (channel.empty())
            continue;

        const auto runSections = [&channel, sampleRate] (double hz, bool high)
        {
            auto first = butterworthSection (hz, sampleRate, 0.5411961001461969, high);
            auto second = butterworthSection (hz, sampleRate, 1.3065629648763766, high);

            for (auto& sample : channel)
                sample = second.run (first.run (sample));
        };

        if (useHighpass)
            runSections (highpass, true);

        if (useLowpass)
            runSections (lowpass, false);
    }
}

// Phase 4 Wave 1: low shelf, bell and high shelf, with Fancy EQ's matched filters. A band at
// 0 dB isn't run at all, so an EQ module at its defaults leaves the IR bit-identical.
void applyEq (Channels& ir, const ModuleSpec& module, double sampleRate)
{
    namespace matched = fancy::dsp::matched;

    const auto band = [sampleRate] (matched::BandType type, float hz, float gainDb, double q)
    {
        matched::BandSpec spec;
        spec.type = type;
        spec.frequencyHz = std::min ((double) hz, sampleRate * 0.45);
        spec.gainDb = gainDb;
        spec.q = q;
        spec.order = 2;
        spec.on = true;
        return matched::design (spec, sampleRate);
    };

    std::vector<matched::BandFilter> filters;

    if (module.values[1] != 0.0f)
        filters.push_back (band (matched::BandType::lowShelf, module.values[0], module.values[1], matched::butterworthQ));

    if (module.values[3] != 0.0f)
        filters.push_back (band (matched::BandType::band, module.values[2], module.values[3], (double) module.values[4]));

    if (module.values[6] != 0.0f)
        filters.push_back (band (matched::BandType::highShelf, module.values[5], module.values[6], matched::butterworthQ));

    for (auto& channel : ir)
    {
        if (channel.empty())
            continue;

        for (const auto& filter : filters)
        {
            for (int k = 0; k < filter.numSections; ++k)
            {
                const auto& c = filter.sections[(size_t) k];
                double s1 = 0.0, s2 = 0.0;

                for (auto& sample : channel)
                {
                    const auto x = sample;
                    const auto y = c.b0 * x + s1;
                    s1 = c.b1 * x - c.a1 * y + s2;
                    s2 = c.b2 * x - c.a2 * y;
                    sample = y;
                }
            }
        }
    }
}

void addEchoes (Channels& ir, const ModuleSpec& module, double sampleRate, std::vector<double>& echoSeconds)
{
    const auto startMs = (double) module.values[0];
    const auto lengthMs = (double) module.values[1];
    const auto spacingMs = (double) module.values[2];
    const auto errorMs = (double) module.values[3];
    const auto exponent = 1.0 + 4.0 * (double) module.values[4];
    const auto startDb = (double) module.values[5];
    const auto endDb = (double) module.values[6];

    const auto count = 1 + (int) std::floor (lengthMs / spacingMs + 1.0e-9);
    const auto maxLength = (size_t) (maxIrSeconds * sampleRate);

    std::array<juce::Random, 2> random { juce::Random ((juce::int64) module.seed), juce::Random ((juce::int64) module.seed + 1) };

    for (int echo = 0; echo < count; ++echo)
    {
        const auto position = count > 1 ? (double) echo / (double) (count - 1) : 0.0;
        const auto gain = std::pow (10.0, (startDb + (endDb - startDb) * position) / 20.0);
        const auto ms = startMs + echo * spacingMs;

        for (size_t ch = 0; ch < 2; ++ch)
        {
            const auto u = (double) random[ch].nextFloat() * 2.0 - 1.0;
            const auto offset = errorMs * std::pow (std::abs (u), exponent) * (u < 0.0 ? -1.0 : 1.0);
            const auto sample = (juce::int64) std::llround (std::max (0.0, ms + offset) * 0.001 * sampleRate);

            if (sample < (juce::int64) maxLength)
            {
                growTo (ir, (size_t) sample + 1);
                ir[ch][(size_t) sample] += gain;
            }
        }

        if ((int) echoSeconds.size() < maxEchoesShown)
            echoSeconds.push_back (ms * 0.001);
    }
}

double peakOf (const Channels& ir) noexcept
{
    auto peak = 0.0;

    for (const auto& channel : ir)
        for (const auto sample : channel)
            peak = std::max (peak, std::abs (sample));

    return peak;
}

void applyNormalize (Channels& ir, const ModuleSpec& module)
{
    const auto peak = peakOf (ir);

    if (peak <= 0.0)
        return;

    const auto gain = std::pow (10.0, (double) module.values[0] / 20.0) / peak;

    for (auto& channel : ir)
        for (auto& sample : channel)
            sample *= gain;
}

void cutTo (Channels& ir, size_t start, size_t end)
{
    end = std::min (end, lengthOf (ir));
    start = std::min (start, end);

    for (auto& channel : ir)
    {
        if (channel.empty())
            continue;

        channel.erase (channel.begin() + (std::ptrdiff_t) end, channel.end());
        channel.erase (channel.begin(), channel.begin() + (std::ptrdiff_t) start);
    }
}

void applyTrim (Channels& ir, const ModuleSpec& module, double sampleRate)
{
    const auto startMs = (double) module.values[0];
    const auto endCutMs = (double) module.values[1];
    const auto stretch = (double) module.values[2] * 0.01;
    const auto fadeInMs = (double) module.values[3];
    const auto fadeOutMs = (double) module.values[4];
    const auto gain = std::pow (10.0, (double) module.values[5] / 20.0);

    const auto toSamples = [sampleRate] (double ms) { return (size_t) std::llround (ms * 0.001 * sampleRate); };
    const auto length = lengthOf (ir);
    const auto endCut = toSamples (endCutMs);

    cutTo (ir, toSamples (startMs), length > endCut ? length - endCut : 0);

    const auto kept = lengthOf (ir);
    const auto fadeIn = std::min (toSamples (fadeInMs), kept);
    const auto fadeOut = std::min (toSamples (fadeOutMs), kept);

    for (auto& channel : ir)
    {
        if (channel.empty())
            continue;

        for (size_t i = 0; i < fadeIn; ++i)
            channel[i] *= (double) i / (double) fadeIn;

        for (size_t i = 0; i < fadeOut; ++i)
            channel[kept - 1 - i] *= (double) i / (double) fadeOut;

        for (auto& sample : channel)
            sample *= gain;
    }

    if (std::abs (stretch - 1.0) > 1.0e-6 && kept > 0)
    {
        const auto maxLength = (size_t) (maxIrSeconds * sampleRate);

        for (auto& channel : ir)
        {
            if (channel.empty())
                continue;

            channel = resample (channel.data(), (int) channel.size(), stretch);

            if (channel.size() > maxLength)
                channel.resize (maxLength);
        }
    }
}

void applyReverse (Channels& ir)
{
    for (auto& channel : ir)
        std::reverse (channel.begin(), channel.end());
}

// Phase 3 Q2.8: the Reverb generator with any of its new settings away from their defaults.
// Bands, early reflections and true stereo come from their own seeded noise, and the original
// loop below stays as it was for the defaults.
constexpr double speedOfSound = 343.0;
constexpr double earOffsetMetres = 0.09;
constexpr double lowSplitHz = 250.0, highSplitHz = 4000.0;
constexpr double earlyLevel = 3.0;

struct Box
{
    double length, width, height;
    int order;
    double absorption;
};

Box boxFor (EarlyType type, double size) noexcept
{
    if (type == EarlyType::room)
        return { size, 0.75 * size, 0.45 * size, 3, 0.80 };

    return { size, 0.6 * size, 0.5 * size, 2, 0.85 };
}

double imageOf (int n, double length, double position) noexcept
{
    return n * length + ((n % 2) == 0 ? position : length - position);
}

// Fourth-order Butterworth: two sections.
struct Butterworth4
{
    Biquad first, second;

    double run (double x) noexcept { return second.run (first.run (x)); }
};

Butterworth4 butterworth4 (double hz, double sampleRate, bool highpass) noexcept
{
    const auto limited = std::min (hz, 0.45 * sampleRate);
    return { butterworthSection (limited, sampleRate, 0.5411961001461969, highpass),
             butterworthSection (limited, sampleRate, 1.3065629648763766, highpass) };
}

void addReverbExtended (Channels& ir, const ModuleSpec& module, double sampleRate)
{
    const auto lengthSeconds = (double) module.values[0];
    const auto decaySeconds = (double) module.values[1];
    const auto density = (double) module.values[2] * 0.01;
    const auto damping = (double) module.values[3] * 0.01;
    const auto spread = (double) module.values[4] * 0.01;
    const auto lowMultiplier = juce::jlimit (0.25, 4.0, (double) module.values[5]);
    const auto highMultiplier = juce::jlimit (0.25, 4.0, (double) module.values[6]);
    const auto size = juce::jlimit (2.0, 60.0, (double) module.values[7]);
    const auto buildUpSeconds = (double) module.values[8] / 1000.0;
    const auto early = (EarlyType) juce::jlimit (0, 3, juce::roundToInt (module.values[9]));
    const auto trueStereo = module.values[10] >= 0.5f;

    const auto length = std::max ((size_t) 1, (size_t) (lengthSeconds * sampleRate));
    const auto fillSamples = buildUpSeconds * sampleRate;
    const auto startProbability = 0.02 + 0.98 * density * density;
    const auto common = std::sqrt (std::max (0.0, 1.0 - spread * spread));
    const auto banded = lowMultiplier != 1.0 || highMultiplier != 1.0;
    const auto ramps = early == EarlyType::room || early == EarlyType::hall;
    const auto paths = trueStereo ? 4 : 2; // LL, RR, then RL, LR
    const auto bands = banded ? 3 : 1;

    juce::Random mask ((juce::int64) module.seed);
    juce::Random random ((juce::int64) module.seed * 31 + 7919);
    const auto noise = [&random] { return (double) random.nextFloat() * 2.0 - 1.0; };

    // Which input side and which output side each path joins.
    constexpr int sourceOf[] { 0, 1, 1, 0 };
    constexpr int earOf[] { 0, 1, 0, 1 };

    Channels wet;

    for (int p = 0; p < paths; ++p)
        wet[(size_t) p].assign (length, 0.0);

    // Early reflections, as impulses ahead of the damping filter.
    if (ramps)
    {
        for (int p = 0; p < paths; ++p)
        {
            for (const auto& reflection : earlyPattern (early, size, sourceOf[p], earOf[p]))
            {
                const auto index = (size_t) std::llround (reflection.seconds * sampleRate);

                if (index < length)
                    wet[(size_t) p][index] += earlyLevel * reflection.gain;
            }
        }
    }

    const std::array<double, 3> multipliers { lowMultiplier, 1.0, highMultiplier };
    std::array<std::array<Butterworth4, 2>, 4> bandFilters {}; // per path: [low or high part, mid's second filter]

    for (int p = 0; p < paths; ++p)
    {
        bandFilters[(size_t) p][0] = butterworth4 (lowSplitHz, sampleRate, false);
        bandFilters[(size_t) p][1] = butterworth4 (highSplitHz, sampleRate, true);
    }

    // The mid band is a high pass at 250 Hz into a low pass at 4 kHz.
    std::array<std::array<Butterworth4, 2>, 4> midFilters {};

    for (int p = 0; p < paths; ++p)
    {
        midFilters[(size_t) p][0] = butterworth4 (lowSplitHz, sampleRate, true);
        midFilters[(size_t) p][1] = butterworth4 (highSplitHz, sampleRate, false);
    }

    std::array<double, 4> lowpass {};

    for (size_t i = 0; i < length; ++i)
    {
        const auto t = (double) i / sampleRate;
        const auto fill = fillSamples > 0.0 ? std::min (1.0, (double) i / fillSamples) : 1.0;
        const auto probability = early == EarlyType::plate ? 1.0 : startProbability + (1.0 - startProbability) * fill;
        const auto keep = (double) mask.nextFloat() < probability;
        const auto amplitude = keep ? (ramps ? fill : 1.0) / std::sqrt (probability) : 0.0;
        const auto crossGain = 0.5 + 0.35 * fill;

        const auto cutoff = damping > 0.0 ? std::max (200.0, 20000.0 * std::exp (-t * damping * 4.0 / decaySeconds)) : 0.0;
        const auto coefficient = cutoff > 0.0 ? 1.0 - std::exp (-2.0 * pi * cutoff / sampleRate) : 1.0;

        std::array<double, 4> sum {};

        for (int b = 0; b < bands; ++b)
        {
            const auto envelope = std::exp (-sixtyDbInNepers * t / (decaySeconds * multipliers[(size_t) (banded ? b : 1)]));
            const auto shared = noise();

            for (int p = 0; p < paths; ++p)
            {
                auto x = (common * shared + spread * noise()) * amplitude;

                if (banded)
                {
                    auto& split = bandFilters[(size_t) p];
                    auto& mid = midFilters[(size_t) p];
                    x = b == 0 ? split[0].run (x) : b == 2 ? split[1].run (x) : mid[1].run (mid[0].run (x));
                }

                sum[(size_t) p] += x * envelope * (p >= 2 ? crossGain : 1.0);
            }
        }

        for (int p = 0; p < paths; ++p)
        {
            auto& state = lowpass[(size_t) p];
            state += (sum[(size_t) p] + wet[(size_t) p][i] - state) * coefficient;
            wet[(size_t) p][i] = state;
        }
    }

    const auto fade = std::min (length, (size_t) (0.05 * sampleRate));

    for (int p = 0; p < paths; ++p)
        for (size_t i = 0; i < fade; ++i)
            wet[(size_t) p][length - 1 - i] *= (double) i / (double) fade;

    // Unit energy over the direct paths, as the original; the cross paths keep their level to them.
    auto energy = 0.0;

    for (int p = 0; p < 2; ++p)
        for (const auto sample : wet[(size_t) p])
            energy += sample * sample;

    const auto gain = energy > 0.0 ? std::sqrt (2.0 / energy) : 0.0;

    growTo (ir, length);

    if (trueStereo)
        makeTrueStereo (ir);

    for (int p = 0; p < paths; ++p)
        for (size_t i = 0; i < length; ++i)
            ir[(size_t) p][i] += wet[(size_t) p][i] * gain;
}

bool usesNewReverbSettings (const ModuleSpec& module) noexcept
{
    const auto& settings = settingsOf (ModuleType::reverb);

    for (size_t i = 5; i < settings.size(); ++i)
        if (module.values[i] != settings[i].defaultValue)
            return true;

    return false;
}

void addReverb (Channels& ir, const ModuleSpec& module, double sampleRate)
{
    if (usesNewReverbSettings (module))
    {
        addReverbExtended (ir, module, sampleRate);
        return;
    }

    const auto lengthSeconds = (double) module.values[0];
    const auto decaySeconds = (double) module.values[1];
    const auto density = (double) module.values[2] * 0.01;
    const auto damping = (double) module.values[3] * 0.01;
    const auto spread = (double) module.values[4] * 0.01;

    const auto length = std::max ((size_t) 1, (size_t) (lengthSeconds * sampleRate));
    const auto fillSamples = 0.1 * sampleRate;
    const auto startProbability = 0.02 + 0.98 * density * density;
    const auto common = std::sqrt (std::max (0.0, 1.0 - spread * spread));

    juce::Random random ((juce::int64) module.seed);
    const auto noise = [&random] { return (double) random.nextFloat() * 2.0 - 1.0; };

    Channels wet;
    wet[0].resize (length);
    wet[1].resize (length);

    std::array<double, 2> lowpass {};

    for (size_t i = 0; i < length; ++i)
    {
        const auto t = (double) i / sampleRate;
        const auto fill = std::min (1.0, (double) i / fillSamples);
        const auto probability = startProbability + (1.0 - startProbability) * fill;
        const auto keep = (double) random.nextFloat() < probability;
        const auto scale = keep ? std::exp (-sixtyDbInNepers * t / decaySeconds) / std::sqrt (probability) : 0.0;

        const auto shared = noise();
        const auto left = noise();
        const auto right = noise();

        const auto cutoff = damping > 0.0 ? std::max (200.0, 20000.0 * std::exp (-t * damping * 4.0 / decaySeconds)) : 0.0;
        const auto coefficient = cutoff > 0.0 ? 1.0 - std::exp (-2.0 * pi * cutoff / sampleRate) : 1.0;

        lowpass[0] += ((common * shared + spread * left) * scale - lowpass[0]) * coefficient;
        lowpass[1] += ((common * shared + spread * right) * scale - lowpass[1]) * coefficient;

        wet[0][i] = lowpass[0];
        wet[1][i] = lowpass[1];
    }

    // A short fade, so a Length under the decay doesn't end on a step.
    const auto fade = std::min (length, (size_t) (0.05 * sampleRate));

    for (auto& channel : wet)
        for (size_t i = 0; i < fade && ! channel.empty(); ++i)
            channel[length - 1 - i] *= (double) i / (double) fade;

    // Unit energy, so a long tail comes out about as loud as a short one.
    auto energy = 0.0;

    for (const auto& channel : wet)
        for (const auto sample : channel)
            energy += sample * sample;

    const auto gain = energy > 0.0 ? std::sqrt (2.0 / energy) : 0.0;

    growTo (ir, length);

    for (size_t ch = 0; ch < 2; ++ch)
        for (size_t i = 0; i < length; ++i)
            ir[ch][i] += wet[ch][i] * gain;
}

void makePreview (const Channels& ir, IrPreview& preview)
{
    const auto length = lengthOf (ir);

    preview.columns.assign (length > 0 ? (size_t) previewColumns : 0, {});

    for (size_t column = 0; column < preview.columns.size(); ++column)
    {
        const auto begin = column * length / preview.columns.size();
        const auto end = std::max (begin + 1, (column + 1) * length / preview.columns.size());

        auto low = 0.0, high = 0.0, energy = 0.0;

        for (size_t i = begin; i < std::min (end, length); ++i)
        {
            const auto sample = isTrueStereo (ir) ? (ir[0][i] + ir[1][i] + ir[crossToLeft][i] + ir[crossToRight][i]) * 0.5
                                                  : (ir[0][i] + ir[1][i]) * 0.5;
            low = std::min (low, sample);
            high = std::max (high, sample);
            energy += sample * sample;
        }

        preview.columns[column] = { (float) low, (float) high, (float) std::sqrt (energy / (double) (end - begin)) };
    }
}

} // namespace

std::vector<Reflection> earlyPattern (EarlyType type, double sizeMetres, int sourceSide, int earSide)
{
    std::vector<Reflection> reflections;

    if (type != EarlyType::room && type != EarlyType::hall)
        return reflections;

    const auto box = boxFor (type, sizeMetres);
    const double source[] { 0.3 * box.length, (sourceSide == 0 ? 0.4 : 0.6) * box.width, 0.5 * box.height };
    const double ear[] { 0.7 * box.length, 0.5 * box.width + (earSide == 0 ? -earOffsetMetres : earOffsetMetres), 0.4 * box.height };

    const auto distance = [&ear] (double x, double y, double z)
    {
        return std::sqrt ((x - ear[0]) * (x - ear[0]) + (y - ear[1]) * (y - ear[1]) + (z - ear[2]) * (z - ear[2]));
    };

    const auto direct = distance (source[0], source[1], source[2]);

    for (int nx = -box.order; nx <= box.order; ++nx)
    {
        for (int ny = -box.order; ny <= box.order; ++ny)
        {
            for (int nz = -box.order; nz <= box.order; ++nz)
            {
                const auto bounces = std::abs (nx) + std::abs (ny) + std::abs (nz);

                if (bounces == 0 || bounces > box.order)
                    continue;

                const auto d = distance (imageOf (nx, box.length, source[0]), imageOf (ny, box.width, source[1]), imageOf (nz, box.height, source[2]));
                reflections.push_back ({ (d - direct) / speedOfSound, std::pow (box.absorption, bounces) * direct / d });
            }
        }
    }

    std::sort (reflections.begin(), reflections.end(), [] (const auto& a, const auto& b) { return a.seconds < b.seconds; });
    return reflections;
}

std::vector<double> resample (const double* source, int numSamples, double ratio)
{
    const auto& table = kernelTable();

    const auto cutoff = std::min (1.0, ratio) * passbandFactor;
    const auto halfWidth = (double) zeroCrossings / cutoff;
    const auto length = (size_t) std::ceil ((double) numSamples * ratio);

    std::vector<double> output (length, 0.0);

    for (size_t i = 0; i < length; ++i)
    {
        const auto position = (double) i / ratio;
        const auto first = std::max (0, (int) std::ceil (position - halfWidth));
        const auto last = std::min (numSamples - 1, (int) std::floor (position + halfWidth));

        auto sum = 0.0;

        for (int j = first; j <= last; ++j)
        {
            const auto x = std::abs (position - (double) j) * cutoff * tableResolution;
            const auto index = std::min ((size_t) x, table.size() - 2);
            const auto fraction = x - (double) index;

            sum += source[j] * (table[index] + (table[index + 1] - table[index]) * fraction);
        }

        output[i] = sum * cutoff;
    }

    return output;
}

std::vector<double> decorrelate (const std::vector<double>& signal, int seed)
{
    constexpr int order = 11, size = 1 << order, hop = size / 4;

    const auto length = (int) signal.size();
    std::vector<double> output ((size_t) length, 0.0);

    if (length == 0)
        return output;

    juce::dsp::FFT fft (order);
    juce::Random random ((juce::int64) seed);

    std::vector<float> window ((size_t) size), frame ((size_t) size * 2);

    for (int n = 0; n < size; ++n)
        window[(size_t) n] = (float) std::sqrt (0.5 - 0.5 * std::cos (2.0 * pi * n / size));

    for (int start = hop - size; start < length; start += hop)
    {
        std::fill (frame.begin(), frame.end(), 0.0f);

        for (int n = 0; n < size; ++n)
            if (const auto index = start + n; index >= 0 && index < length)
                frame[(size_t) n] = (float) signal[(size_t) index] * window[(size_t) n];

        fft.performRealOnlyForwardTransform (frame.data(), true);

        for (int k = 1; k < size / 2; ++k)
        {
            const auto magnitude = std::hypot ((double) frame[(size_t) (2 * k)], (double) frame[(size_t) (2 * k + 1)]);
            const auto phase = random.nextDouble() * 2.0 * pi;
            frame[(size_t) (2 * k)] = (float) (magnitude * std::cos (phase));
            frame[(size_t) (2 * k + 1)] = (float) (magnitude * std::sin (phase));
        }

        fft.performRealOnlyInverseTransform (frame.data());

        for (int n = 0; n < size; ++n)
            if (const auto index = start + n; index >= 0 && index < length)
                output[(size_t) index] += (double) frame[(size_t) n] * window[(size_t) n];
    }

    // The same total energy as the input, whatever the transform's scaling.
    auto energyIn = 0.0, energyOut = 0.0;

    for (size_t i = 0; i < output.size(); ++i)
    {
        energyIn += signal[i] * signal[i];
        energyOut += output[i] * output[i];
    }

    if (energyOut > 0.0)
        for (auto& sample : output)
            sample *= std::sqrt (energyIn / energyOut);

    return output;
}

namespace
{
// Splits the finished IR for a Modulate module: early part, tail as it is, tail decorrelated.
void splitForModulation (const Channels& ir, const ModuleSpec& modulate, double sampleRate, BuiltIr& built)
{
    const auto length = lengthOf (ir);
    const auto start = (size_t) ((double) modulate.values[0] * 0.001 * sampleRate);
    const auto fade = (size_t) (0.01 * sampleRate);

    if (length == 0 || start + fade >= length)
        return;

    const auto paths = isTrueStereo (ir) ? 4 : 2;

    built.modulation.on = true;
    built.modulation.rateHz = (double) modulate.values[1];
    built.modulation.depth = (double) modulate.values[2] * 0.01;
    built.modulation.spread = (double) modulate.values[3] * 0.01;
    built.modulation.seed = modulate.seed;

    built.tailA.setSize (paths, (int) length);
    built.tailB.setSize (paths, (int) length);

    for (int p = 0; p < paths; ++p)
    {
        const auto& source = ir[(size_t) p];
        const auto weight = [start, fade] (size_t i) { return i < start ? 0.0 : i < start + fade ? (double) (i - start) / (double) fade : 1.0; };

        std::vector<double> tail (length, 0.0);

        for (size_t i = start; i < length; ++i)
            tail[i] = source[i];

        auto other = decorrelate (tail, modulate.seed * 13 + p);

        auto energyA = 0.0, energyB = 0.0;

        for (size_t i = 0; i < length; ++i)
        {
            other[i] *= weight (i);
            energyA += source[i] * weight (i) * source[i] * weight (i);
            energyB += other[i] * other[i];
        }

        const auto match = energyB > 0.0 ? std::sqrt (energyA / energyB) : 0.0;

        auto* early = built.audio.getWritePointer (p);
        auto* a = built.tailA.getWritePointer (p);
        auto* b = built.tailB.getWritePointer (p);

        for (size_t i = 0; i < length; ++i)
        {
            const auto w = weight (i);
            early[i] = (float) (source[i] * (1.0 - w));
            a[i] = (float) (source[i] * w);
            b[i] = (float) (other[i] * match);
        }
    }
}
double weightedEnergyOf (const double* samples, size_t length, double sampleRate)
{
    if (length == 0)
        return 0.0;

    // Frames about 1/6 s long, so the lowest bins sit near 6 Hz at any rate.
    const auto order = juce::jlimit (8, 16, (int) std::ceil (std::log2 (sampleRate / 6.0)));
    const auto size = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> frame ((size_t) size * 2);

    const auto binHz = sampleRate / (double) size;
    const auto top = std::min (20000.0, sampleRate * 0.49);
    double weightSum = 0.0;
    std::vector<std::pair<int, double>> weights;

    for (int k = 1; k < size / 2; ++k)
    {
        const auto hz = binHz * (double) k;

        if (hz >= 20.0 && hz <= top)
        {
            weights.emplace_back (k, 1.0 / hz);
            weightSum += 1.0 / hz;
        }
    }

    if (weightSum <= 0.0)
        return 0.0;

    double energy = 0.0;

    for (size_t from = 0; from < length; from += (size_t) size)
    {
        std::fill (frame.begin(), frame.end(), 0.0f);
        const auto count = std::min ((size_t) size, length - from);

        for (size_t i = 0; i < count; ++i)
            frame[i] = (float) samples[from + i];

        fft.performRealOnlyForwardTransform (frame.data(), true);

        double sum = 0.0;

        for (const auto& [k, weight] : weights)
        {
            const auto re = (double) frame[(size_t) (2 * k)];
            const auto im = (double) frame[(size_t) (2 * k + 1)];
            sum += (re * re + im * im) * weight;
        }

        energy += sum / weightSum;
    }

    return energy;
}

// Scales every channel to the reference loudness and returns the trim in dB.
double applyEqualLoudness (Channels& ir, size_t start, size_t end, double sampleRate)
{
    end = std::min (end, lengthOf (ir));

    if (start >= end)
        return 0.0;

    const auto energyOf = [&] (size_t ch)
    {
        return ir[ch].size() < end ? 0.0 : weightedEnergyOf (ir[ch].data() + start, end - start, sampleRate);
    };

    // What each output gets from two uncorrelated inputs, averaged over the two outputs.
    const auto energy = isTrueStereo (ir) ? 0.5 * (energyOf (0) + energyOf (1) + energyOf (crossToLeft) + energyOf (crossToRight))
                                          : 0.5 * (energyOf (0) + energyOf (1));

    if (! (energy > 1.0e-20))
        return 0.0;

    const auto trimDb = juce::jlimit (-maxLoudnessTrimDb, maxLoudnessTrimDb,
                                      10.0 * std::log10 (referenceLoudnessEnergy (sampleRate) / energy));
    const auto gain = std::pow (10.0, trimDb / 20.0);

    for (auto& channel : ir)
        for (auto& sample : channel)
            sample *= gain;

    return trimDb;
}
} // namespace

double pinkEnergy (const std::vector<double>& samples, double sampleRate)
{
    return weightedEnergyOf (samples.data(), samples.size(), sampleRate);
}

double referenceLoudnessEnergy (double sampleRate) noexcept
{
    // Where a library IR at the default gain mode (-18 dB) lands: the library's IRs measure
    // about 0.46 at 0 dB (CI run 44942993322), so those play with about 0 dB of trim. Scaled with
    // the rate, as a resampled IR's energy is.
    return 0.46 * std::pow (10.0, -18.0 / 10.0) * sampleRate / 48000.0;
}

juce::AudioBuffer<float> exportableIr (const BuiltIr& built)
{
    const auto& audio = built.audio;
    const auto channels = audio.getNumChannels();
    const auto length = audio.getNumSamples();
    juce::AudioBuffer<float> out (channels >= 4 ? 4 : 2, length);

    if (channels == 0)
        return out;

    // The build holds LL, RR, RL, LR; a four-channel file is LL, LR, RL, RR.
    const int order[4] = { 0, 3, 2, 1 };

    for (int ch = 0; ch < out.getNumChannels(); ++ch)
    {
        const auto from = channels >= 4 ? order[ch] : juce::jmin (ch, channels - 1);
        auto* dest = out.getWritePointer (ch);

        for (int i = 0; i < length; ++i)
        {
            auto value = audio.getSample (from, i);

            if (built.modulation.on && from < built.tailA.getNumChannels() && i < built.tailA.getNumSamples())
                value += built.tailA.getSample (from, i);

            dest[i] = value;
        }
    }

    return out;
}

// Phase 4 Wave 2: the Decay module ------------------------------------------------------

namespace
{
constexpr int decayFftOrder = 13, decayFftSize = 1 << decayFftOrder, decayHop = decayFftSize / 4;
constexpr double minimumRtSeconds = 0.05;   // shorter is a filter's ring, not a decay
constexpr double floorMarginDb = 10.0, floorFadeSeconds = 0.1, floorWindowSeconds = 0.2, envelopeSeconds = 0.01;

// 0 below the crossover, 1 above, over half an octave (a raised cosine in log frequency).
double crossFade (double hz, double crossHz) noexcept
{
    if (hz <= 0.0)
        return 0.0;

    const auto x = std::log2 (hz / crossHz) / 0.5 + 0.5;
    return 0.5 - 0.5 * std::cos (pi * std::clamp (x, 0.0, 1.0));
}

// Splits one channel into its three bands: Hann frames at a quarter hop (they sum to 2), each
// spectrum times the band's mask, back, overlap-added. The masks sum to 1, so the bands do too.
class BandSplitter
{
public:
    BandSplitter (double lowCrossHz, double highCrossHz, double sampleRate)
        : fft (decayFftOrder)
    {
        window.resize ((size_t) decayFftSize);

        for (int n = 0; n < decayFftSize; ++n)
            window[(size_t) n] = (float) (0.5 - 0.5 * std::cos (2.0 * pi * n / decayFftSize));

        for (auto& mask : masks)
            mask.resize ((size_t) decayFftSize / 2 + 1);

        for (int k = 0; k <= decayFftSize / 2; ++k)
        {
            const auto hz = (double) k * sampleRate / decayFftSize;
            const auto low = crossFade (hz, lowCrossHz), high = crossFade (hz, highCrossHz);
            masks[0][(size_t) k] = (float) (1.0 - low);
            masks[1][(size_t) k] = (float) (low * (1.0 - high));
            masks[2][(size_t) k] = (float) (low * high);
        }

        // Whatever the transform's own scaling: an impulse through forward and back.
        std::vector<float> probe ((size_t) decayFftSize * 2, 0.0f);
        probe[0] = 1.0f;
        fft.performRealOnlyForwardTransform (probe.data(), true);
        mirror (probe);
        fft.performRealOnlyInverseTransform (probe.data());
        scale = probe[0] != 0.0f ? 0.5 / (double) probe[0] : 0.0;   // 0.5: the Hann frames sum to 2
    }

    std::array<std::vector<double>, 3> split (const std::vector<double>& x)
    {
        const auto length = (int) x.size();
        std::array<std::vector<double>, 3> bands;

        for (auto& band : bands)
            band.assign ((size_t) length, 0.0);

        std::vector<float> spectrum ((size_t) decayFftSize * 2), frame ((size_t) decayFftSize * 2);

        for (int start = decayHop - decayFftSize; start < length; start += decayHop)
        {
            std::fill (spectrum.begin(), spectrum.end(), 0.0f);

            for (int n = 0; n < decayFftSize; ++n)
                if (const auto index = start + n; index >= 0 && index < length)
                    spectrum[(size_t) n] = (float) x[(size_t) index] * window[(size_t) n];

            fft.performRealOnlyForwardTransform (spectrum.data(), true);

            for (size_t b = 0; b < 3; ++b)
            {
                std::fill (frame.begin(), frame.end(), 0.0f);

                for (int k = 0; k <= decayFftSize / 2; ++k)
                {
                    frame[(size_t) (2 * k)] = spectrum[(size_t) (2 * k)] * masks[b][(size_t) k];
                    frame[(size_t) (2 * k + 1)] = spectrum[(size_t) (2 * k + 1)] * masks[b][(size_t) k];
                }

                mirror (frame);
                fft.performRealOnlyInverseTransform (frame.data());

                for (int n = 0; n < decayFftSize; ++n)
                    if (const auto index = start + n; index >= 0 && index < length)
                        bands[b][(size_t) index] += (double) frame[(size_t) n] * scale;
            }
        }

        return bands;
    }

private:
    // The negative frequencies as the conjugates of the positive ones, for any FFT engine.
    static void mirror (std::vector<float>& data)
    {
        for (int k = decayFftSize / 2 + 1; k < decayFftSize; ++k)
        {
            data[(size_t) (2 * k)] = data[(size_t) (2 * (decayFftSize - k))];
            data[(size_t) (2 * k + 1)] = -data[(size_t) (2 * (decayFftSize - k) + 1)];
        }
    }

    juce::dsp::FFT fft;
    std::vector<float> window;
    std::array<std::vector<float>, 3> masks;
    double scale = 1.0;
};

// 10 ms moving average of an energy envelope, in dB.
std::vector<double> envelopeDb (const std::vector<double>& energy, double sampleRate)
{
    const auto window = std::max (1, (int) std::lround (envelopeSeconds * sampleRate));
    const auto n = (int) energy.size();
    std::vector<double> out ((size_t) n);
    auto sum = 0.0;

    for (int i = 0; i < n; ++i)
    {
        sum += energy[(size_t) i];

        if (i >= window)
            sum -= energy[(size_t) (i - window)];

        out[(size_t) i] = 10.0 * std::log10 (std::max (sum / window, 1.0e-30));
    }

    return out;
}

// The gain that turns a band's RT from rt into multiplier x rt, from its envelope's peak on.
std::vector<double> decayGain (const std::vector<double>& energy, double rt, double multiplier, double sampleRate, bool& limited)
{
    const auto n = energy.size();
    std::vector<double> gain (n, 1.0);
    const auto envelope = envelopeDb (energy, sampleRate);
    const auto peak = (size_t) std::distance (envelope.begin(), std::max_element (envelope.begin(), envelope.end()));
    const auto rate = sixtyDbInNepers * (1.0 / (multiplier * rt) - 1.0 / rt);

    for (size_t i = peak; i < n; ++i)
        gain[i] = std::exp (-rate * (double) (i - peak) / sampleRate);

    limited = false;

    if (multiplier > 1.0)
    {
        // The band's floor: the median of its last 200 ms.
        const auto tail = std::min (n, (size_t) (floorWindowSeconds * sampleRate));
        std::vector<double> last (envelope.end() - (std::ptrdiff_t) tail, envelope.end());

        if (! last.empty())
        {
            std::nth_element (last.begin(), last.begin() + (std::ptrdiff_t) (last.size() / 2), last.end());
            const auto floorDb = last[last.size() / 2];

            for (size_t i = peak; i < n; ++i)
            {
                if (envelope[i] < floorDb + floorMarginDb)
                {
                    limited = true;
                    const auto held = gain[i];
                    const auto fade = std::max ((size_t) 1, (size_t) (floorFadeSeconds * sampleRate));

                    for (size_t j = i; j < n; ++j)
                    {
                        const auto k = j - i;
                        gain[j] = k < fade ? held * (0.5 + 0.5 * std::cos (pi * (double) k / (double) fade)) : 0.0;
                    }

                    break;
                }
            }
        }
    }

    return gain;
}
} // namespace

double schroederRt60 (const std::vector<double>& energy, double sampleRate)
{
    const auto n = energy.size();

    if (n < 2)
        return 0.0;

    std::vector<double> decayCurve (n);
    auto sum = 0.0;

    for (size_t i = n; i-- > 0;)
    {
        sum += energy[i];
        decayCurve[i] = sum;
    }

    if (! (decayCurve[0] > 0.0))
        return 0.0;

    const auto measure = [&] (double endDb)
    {
        const auto startLevel = decayCurve[0] * std::pow (10.0, -0.5);
        const auto endLevel = decayCurve[0] * std::pow (10.0, endDb / 10.0);
        auto first = n, last = n;

        for (size_t i = 0; i < n; ++i)
        {
            if (first == n && decayCurve[i] <= startLevel)
                first = i;

            if (decayCurve[i] <= endLevel)
            {
                last = i;
                break;
            }
        }

        if (first >= n || last >= n || last <= first)
            return 0.0;

        return (double) (last - first) / sampleRate * 60.0 / (-endDb - 5.0);
    };

    auto rt = measure (-35.0);

    if (rt <= 0.0)
        rt = measure (-25.0);

    return rt >= minimumRtSeconds ? rt : 0.0;
}

DecayReadout reshapeDecay (std::vector<std::vector<double>*>& channels, const std::array<double, 3>& multipliers,
                           double lowCrossHz, double highCrossHz, double sampleRate)
{
    DecayReadout readout;

    if (channels.empty() || channels.front()->empty())
        return readout;

    const auto length = channels.front()->size();
    BandSplitter splitter (lowCrossHz, std::max (highCrossHz, lowCrossHz * 2.0), sampleRate);

    // 1. Each band's energy, over every channel together.
    std::array<std::vector<double>, 3> energy;

    for (auto& e : energy)
        e.assign (length, 0.0);

    for (auto* channel : channels)
    {
        const auto bands = splitter.split (*channel);

        for (size_t b = 0; b < 3; ++b)
            for (size_t i = 0; i < length; ++i)
                energy[b][i] += bands[b][i] * bands[b][i];
    }

    // 2. Each band's RT, and the gain that reshapes it.
    std::array<std::vector<double>, 3> gains;
    auto anyChange = false;

    for (size_t b = 0; b < 3; ++b)
    {
        const auto rt = schroederRt60 (energy[b], sampleRate);
        readout.measured[b] = rt > 0.0;
        readout.beforeSeconds[b] = readout.afterSeconds[b] = rt;

        if (rt <= 0.0 || multipliers[b] == 1.0)
            continue;

        bool limited = false;
        gains[b] = decayGain (energy[b], rt, multipliers[b], sampleRate, limited);
        readout.limited[b] = limited;

        std::vector<double> after (length);

        for (size_t i = 0; i < length; ++i)
            after[i] = energy[b][i] * gains[b][i] * gains[b][i];

        readout.afterSeconds[b] = schroederRt60 (after, sampleRate);
        anyChange = true;
    }

    // 3. Each channel plus every reshaped band's change, so untouched bands add nothing.
    if (anyChange)
    {
        for (auto* channel : channels)
        {
            const auto bands = splitter.split (*channel);

            for (size_t b = 0; b < 3; ++b)
                if (! gains[b].empty())
                    for (size_t i = 0; i < length; ++i)
                        (*channel)[i] += (gains[b][i] - 1.0) * bands[b][i];
        }
    }

    return readout;
}

BuiltIr buildIr (const ChainSpec& chain, double sampleRate)
{
    BuiltIr built;
    Channels ir;

    for (const auto& module : chain.modules)
    {
        if (! module.on)
            continue;

        switch (module.type)
        {
            case ModuleType::file:      addFile (ir, module, sampleRate); break;
            case ModuleType::lpHp:      applyLpHp (ir, module, sampleRate); break;
            case ModuleType::echo:      addEchoes (ir, module, sampleRate, built.preview.echoSeconds); break;
            case ModuleType::normalize: applyNormalize (ir, module); break;
            case ModuleType::trim:      applyTrim (ir, module, sampleRate); break;
            case ModuleType::reverse:   applyReverse (ir); break;
            case ModuleType::reverb:    addReverb (ir, module, sampleRate); break;
            case ModuleType::eq:        applyEq (ir, module, sampleRate); break;

            case ModuleType::decay:
            {
                // Phase 4 Wave 2: every channel the IR has, so true stereo's cross paths too.
                std::vector<std::vector<double>*> channels;

                for (auto& channel : ir)
                    if (! channel.empty())
                        channels.push_back (&channel);

                auto readout = reshapeDecay (channels, { (double) module.values[0], (double) module.values[1], (double) module.values[2] },
                                             (double) module.values[3], (double) module.values[4], sampleRate);
                readout.uid = module.uid;
                built.preview.decays.push_back (readout);
                break;
            }

            default: break;
        }
    }

    auto& preview = built.preview;
    preview.sampleRate = sampleRate;
    preview.fullSeconds = (double) lengthOf (ir) / sampleRate;

    const auto start = (size_t) (chain.trimStart * sampleRate);
    const auto end = chain.trimEnd > 0.0 ? (size_t) (chain.trimEnd * sampleRate) : lengthOf (ir);

    // Phase 4 Wave 1: Equal loudness scales the whole IR before the display sees it, so the
    // waveform and peak show what plays. Off, nothing here runs.
    if (chain.equalLoudness)
        preview.loudnessTrimDb = applyEqualLoudness (ir, start, end, sampleRate);

    makePreview (ir, preview);
    cutTo (ir, start, end);

    const auto length = lengthOf (ir);
    const auto peak = peakOf (ir);

    preview.trimmedSeconds = (double) length / sampleRate;
    preview.peakDb = peak > 0.0 ? 20.0 * std::log10 (peak) : -200.0;
    const auto trueStereo = isTrueStereo (ir);
    preview.numChannels = length == 0 ? 0 : (trueStereo ? 4 : (ir[0] == ir[1] ? 1 : 2));

    built.audio.setSize (trueStereo ? 4 : 2, (int) length);

    for (int ch = 0; ch < built.audio.getNumChannels(); ++ch)
    {
        auto* out = built.audio.getWritePointer (ch);

        for (size_t i = 0; i < length; ++i)
            out[i] = (float) ir[(size_t) ch][i];
    }

    // Modulate acts on the finished IR, wherever it sits in the chain (Phase 3 Q2.8).
    for (const auto& module : chain.modules)
    {
        if (module.type == ModuleType::modulate && module.on)
        {
            splitForModulation (ir, module, sampleRate, built);
            break;
        }
    }

    return built;
}

} // namespace verb
