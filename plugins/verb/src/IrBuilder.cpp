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

using Channels = std::array<std::vector<double>, 2>;

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
    for (auto& channel : ir)
        if (channel.size() < length)
            channel.resize (length, 0.0);
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
        gain *= ratio;

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
        for (size_t i = 0; i < fade; ++i)
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
            const auto sample = (ir[0][i] + ir[1][i]) * 0.5;
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
    preview.numChannels = length == 0 ? 0 : (ir[0] == ir[1] ? 1 : 2);

    built.audio.setSize (2, (int) length);

    for (int ch = 0; ch < 2; ++ch)
    {
        auto* out = built.audio.getWritePointer (ch);

        for (size_t i = 0; i < length; ++i)
            out[i] = (float) ir[(size_t) ch][i];
    }

    return built;
}

} // namespace verb
