#include "IrBuilder.h"

#include <juce_core/juce_core.h>

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

void addReverb (Channels& ir, const ModuleSpec& module, double sampleRate)
{
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
            default: break;
        }
    }

    auto& preview = built.preview;
    preview.sampleRate = sampleRate;
    preview.fullSeconds = (double) lengthOf (ir) / sampleRate;
    makePreview (ir, preview);

    const auto start = (size_t) (chain.trimStart * sampleRate);
    const auto end = chain.trimEnd > 0.0 ? (size_t) (chain.trimEnd * sampleRate) : lengthOf (ir);
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

    return built;
}

} // namespace verb
