// Phase 4 for Fancy Kicker: Clip oversampling (Wave 2 item 45) and the sidechain Duck
// (docs/kicker.md, "Phase 4 Wave 2: an oversampled clipper" and "Phase 4: sidechain Duck", their
// sections 4 and 6). Numbers modelled first in docs/research/kicker-w2-prepush.py, on the same
// signals.

#include "KickerDsp.h"
#include "KickerDucker.h"
#include "KickerProcessor.h"
#include "LegacyKickerW1Dsp.h"

#include <fancy/plugin/ParamHelpers.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = 3.14159265358979323846;

int samples (double seconds) { return (int) std::lround (seconds * rate); }

void set (KickerProcessor& processor, const char* id, float value)
{
    auto& parameter = *processor.getApvts().getParameter (id);
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
}

// Main input (stereo, the same in both channels) and an optional sidechain (stereo, the same in
// both), through the processor at this block size. Returns the left output.
std::vector<float> render (KickerProcessor& processor, const std::vector<float>& input, const std::vector<float>* key, int block,
                           bool withSidechainBus)
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (juce::AudioChannelSet::stereo());
    layout.inputBuses.add (withSidechainBus ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::disabled());
    layout.outputBuses.add (juce::AudioChannelSet::stereo());
    REQUIRE (processor.setBusesLayout (layout));

    processor.setRateAndBufferSizeDetails (rate, block);
    processor.prepareToPlay (rate, block);

    const auto channels = juce::jmax (processor.getTotalNumInputChannels(), processor.getTotalNumOutputChannels());
    juce::AudioBuffer<float> buffer (channels, block);
    juce::MidiBuffer midi;
    std::vector<float> output;
    output.reserve (input.size());

    for (int start = 0; start < (int) input.size(); start += block)
    {
        const auto count = juce::jmin (block, (int) input.size() - start);
        buffer.setSize (channels, count, false, false, true);
        buffer.clear();

        for (int i = 0; i < count; ++i)
        {
            buffer.setSample (0, i, input[(size_t) (start + i)]);
            buffer.setSample (1, i, input[(size_t) (start + i)]);

            if (withSidechainBus && key != nullptr)
            {
                buffer.setSample (2, i, (*key)[(size_t) (start + i)]);
                buffer.setSample (3, i, (*key)[(size_t) (start + i)]);
            }
        }

        midi.clear();
        processor.processBlock (buffer, midi);

        for (int i = 0; i < count; ++i)
            output.push_back (buffer.getSample (0, i));
    }

    processor.releaseResources();
    return output;
}

// A kick every half second: a body falling from 150 to 50 Hz, with a short click.
std::vector<float> kicks (int numSamples, double level)
{
    std::vector<float> signal ((size_t) numSamples);
    auto phase = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto t = std::fmod ((double) i / rate, 0.5);

        if (t < 0.5 / rate)
            phase = 0.0;

        phase += 2.0 * pi * (50.0 + 100.0 * std::exp (-t / 0.02)) / rate;
        const auto click = t < 0.002 ? 0.4 * std::sin (2.0 * pi * 3000.0 * t) : 0.0;
        signal[(size_t) i] = (float) (level * std::exp (-t / 0.2) * std::sin (phase) + click);
    }

    return signal;
}
} // namespace

// Duck and Clip oversampling, test 1 each: the defaults render as v1.7.0 -----------------------

TEST_CASE ("Kicker with Duck 0 and Clip oversampling Off renders as v1.7.0", "[kicker][w2]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    struct Setting { const char* name; float mode, octave, softClip; bool sidechainBus, hostSync; };
    const auto setting = GENERATE (Setting { "Boost", 0.0f, 0.0f, 0.0f, false, false },
                                   Setting { "Boost, Soft clip, sidechain connected", 0.0f, 0.0f, 1.0f, true, false },
                                   Setting { "Sub, one octave, sidechain connected", 1.0f, 0.0f, 0.0f, true, false },
                                   Setting { "Sub, two octaves, Soft clip, Host sync chosen", 1.0f, 1.0f, 1.0f, false, true });
    INFO (setting.name);

    const auto input = kicks (samples (1.5), 1.2);
    std::vector<float> key ((size_t) input.size());

    for (size_t i = 0; i < key.size(); ++i)
        key[i] = (float) (0.8 * std::sin (2.0 * pi * 50.0 * (double) i / rate));

    KickerProcessor processor;
    set (processor, "mode", setting.mode);
    set (processor, "subOctave", setting.octave);
    set (processor, "softClip", setting.softClip);
    set (processor, "wet", -6.0f);
    set (processor, "duckFrom", setting.hostSync ? 1.0f : 0.0f);
    const auto output = render (processor, input, &key, 256, setting.sidechainBus);

    // v1.7.0's engine with the targets v1.7.0's processor read from those settings (its
    // readTargets, from the stored values).
    const auto value = [&] (const char* id) { return processor.getApvts().getRawParameterValue (id)->load(); };
    LegacyKickerW1Dsp::Targets targets;
    targets.frequencyHz = (double) value ("freq");
    targets.q = (double) value ("q");
    targets.wetGain = fancy::levelToGain (value ("wet"), -60.0f);
    targets.dryGain = fancy::levelToGain (value ("dry"), -60.0f);
    targets.outputGain = fancy::dbToGain ((double) value ("output"));
    targets.monoWet = value ("monoBelow") >= 0.5f;
    targets.softClip = value ("softClip") >= 0.5f;
    targets.sub = juce::roundToInt (value ("mode")) == 1;
    targets.subCutoffHz = (double) value ("subCutoff");
    targets.subDivisions = juce::roundToInt (value ("subOctave")) == 1 ? 4 : 2;
    targets.subGain = fancy::levelToGain (value ("subLevel"), -60.0f);
    targets.subSensitivityGain = fancy::dbToGain ((double) value ("subSensitivity"));
    targets.subLowpassHz = (double) value ("subLowpass");
    targets.subMono = value ("subMono") >= 0.5f;
    targets.subLengthMs = value ("subLen") >= 1999.5f ? 0.0 : (double) value ("subLen");
    targets.roomCut = value ("roomCut") >= 0.5f;
    targets.roomHz = (double) value ("roomFreq");
    targets.subPolarity = value ("subPolarity") >= 0.5f;
    REQUIRE (targets.softClip == (setting.softClip > 0.5f));
    REQUIRE (targets.sub == (setting.mode > 0.5f));

    LegacyKickerW1Dsp legacy;
    legacy.prepare (rate);
    legacy.snapToTargets (targets);

    auto left = input, right = input;

    for (int start = 0; start < (int) input.size(); start += 256)
    {
        float* channels[] { left.data() + start, right.data() + start };
        legacy.setTargets (targets);
        legacy.process (channels, 2, juce::jmin (256, (int) input.size() - start));
    }

    REQUIRE (output.size() == left.size());
    auto differences = 0;

    for (size_t i = 0; i < output.size(); ++i)
        if (output[i] != left[i])
            ++differences;

    CHECK (differences == 0);
}

// Clip oversampling, test 2: 4x and 2x against an ideal reference -------------------------------

namespace
{
// A plain radix-2 FFT in double precision, for the ideal reference.
void fft (std::vector<std::complex<double>>& data, bool inverse)
{
    const auto n = data.size();

    for (size_t i = 1, j = 0; i < n; ++i)
    {
        auto bit = n >> 1;

        for (; (j & bit) != 0; bit >>= 1)
            j ^= bit;

        j ^= bit;

        if (i < j)
            std::swap (data[i], data[j]);
    }

    for (size_t length = 2; length <= n; length <<= 1)
    {
        const auto angle = (inverse ? 2.0 : -2.0) * pi / (double) length;
        const std::complex<double> step (std::cos (angle), std::sin (angle));

        for (size_t i = 0; i < n; i += length)
        {
            std::complex<double> w (1.0, 0.0);

            for (size_t k = 0; k < length / 2; ++k)
            {
                const auto a = data[i + k], b = data[i + k + length / 2] * w;
                data[i + k] = a + b;
                data[i + k + length / 2] = a - b;
                w *= step;
            }
        }
    }

    if (inverse)
        for (auto& value : data)
            value /= (double) n;
}

// Everything above this frequency removed, in the FFT (circular, as the model's).
std::vector<double> lowpass (const std::vector<double>& x, double hz)
{
    std::vector<std::complex<double>> spectrum (x.begin(), x.end());
    fft (spectrum, false);
    const auto n = spectrum.size();

    for (size_t k = 0; k < n; ++k)
    {
        const auto bin = k <= n / 2 ? k : n - k;

        if ((double) bin * rate / (double) n > hz)
            spectrum[k] = 0.0;
    }

    fft (spectrum, true);
    std::vector<double> result (n);

    for (size_t k = 0; k < n; ++k)
        result[k] = spectrum[k].real();

    return result;
}

// The soft clip run at 16x with ideal (FFT) resampling.
std::vector<double> idealClip (const std::vector<double>& x)
{
    const auto n = x.size(), big = n * 16;
    std::vector<std::complex<double>> spectrum (x.begin(), x.end());
    fft (spectrum, false);

    std::vector<std::complex<double>> wide (big);

    for (size_t k = 0; k < n / 2; ++k)
    {
        wide[k] = spectrum[k];

        if (k > 0)
            wide[big - k] = spectrum[n - k];
    }

    wide[n / 2] = 0.5 * spectrum[n / 2];
    wide[big - n / 2] = 0.5 * spectrum[n / 2];
    fft (wide, true);

    for (auto& value : wide)
        value = KickerDsp::softClip (value.real() * 16.0);

    fft (wide, false);
    std::vector<std::complex<double>> narrow (n);

    for (size_t k = 0; k < n / 2; ++k)
    {
        narrow[k] = wide[k];

        if (k > 0)
            narrow[n - k] = wide[big - k];
    }

    narrow[n / 2] = wide[n / 2] + wide[big - n / 2];
    fft (narrow, true);

    std::vector<double> result (n);

    for (size_t k = 0; k < n; ++k)
        result[k] = narrow[k].real() / 16.0;

    return result;
}

// kicker-w2-prepush.py's clipper signal: a 55 Hz kick with a 3 ms noise click from an LCG,
// 1.6 times over, faded out, 2000 samples of silence first, in 32768 samples, its click
// low-passed at 18 kHz.
std::vector<double> clipSignal()
{
    const auto n = samples (0.25);
    std::vector<double> x (32768, 0.0);
    std::uint32_t state = 45;
    const auto fade = samples (0.02);

    for (int i = 0; i < n; ++i)
    {
        state = 1664525u * state + 1013904223u;
        const auto noise = (double) state / 4294967296.0 * 2.0 - 1.0;
        const auto t = (double) i / rate;
        auto value = std::sin (2.0 * pi * 55.0 * t) * std::exp (-t / 0.12)
                   + (t < 0.003 ? 1.2 * std::sqrt (3.0) * noise * std::exp (-t / 0.0015) : 0.0);
        value *= 1.6;

        if (i >= n - fade)
            value *= 0.5 + 0.5 * std::cos (pi * (double) (i - (n - fade)) / (double) fade);

        x[(size_t) (2000 + i)] = value;
    }

    return lowpass (x, 18000.0);
}

double clipErrorDb (const std::vector<double>& x, const std::vector<double>& reference, float factor, int& latency)
{
    KickerProcessor processor;
    set (processor, "wet", -60.0f); // -inf: the output is the dry signal, soft clipped
    set (processor, "softClip", 1.0f);
    set (processor, "clipOs", factor);

    const std::vector<float> input (x.begin(), x.end());
    const auto output = render (processor, input, nullptr, 512, false);

    processor.setRateAndBufferSizeDetails (rate, 512);
    processor.prepareToPlay (rate, 512);
    latency = processor.getLatencySamples();
    processor.releaseResources();

    std::vector<double> aligned (x.size(), 0.0);

    for (size_t i = 0; i + (size_t) latency < output.size(); ++i)
        aligned[i] = (double) output[i + (size_t) latency];

    const auto y = lowpass (aligned, 20000.0);
    auto error = 0.0, signal = 0.0;

    for (size_t i = 0; i < y.size(); ++i)
    {
        error += (y[i] - reference[i]) * (y[i] - reference[i]);
        signal += reference[i] * reference[i];
    }

    return 10.0 * std::log10 (error / signal);
}
} // namespace

TEST_CASE ("Kicker's Clip oversampling cuts the clipper's aliasing on a hard kick", "[kicker][w2]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto x = clipSignal();
    const auto reference = lowpass (idealClip (x), 20000.0);

    int offLatency = -1, x2Latency = -1, x4Latency = -1;
    const auto off = clipErrorDb (x, reference, 0.0f, offLatency);
    const auto x2 = clipErrorDb (x, reference, 1.0f, x2Latency);
    const auto x4 = clipErrorDb (x, reference, 2.0f, x4Latency);

    std::printf ("kicker clip oversampling: error below 20 kHz Off %.1f, 2x %.1f, 4x %.1f dB; latency %d, %d, %d\n",
                 off, x2, x4, offLatency, x2Latency, x4Latency);

    CHECK (offLatency == 0);
    CHECK (x2 <= off - 15.0);
    CHECK (x4 <= off - 30.0);
}

// Clip oversampling, test 3: the latency, and the dry path matched --------------------------------

TEST_CASE ("Kicker's Clip oversampling reports its latency and delays the dry path to match", "[kicker][w2]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto factor = GENERATE (1.0f, 2.0f);
    INFO ("factor " << factor);

    // A 1 kHz tone under the clipper's knee: with 4x or 2x on, the output is the input delayed.
    std::vector<float> input ((size_t) samples (0.5));

    for (size_t i = 0; i < input.size(); ++i)
        input[i] = (float) (0.3 * std::sin (2.0 * pi * 1000.0 * (double) i / rate));

    for (const auto bypassed : { false, true })
    {
        INFO ("bypass " << bypassed);
        KickerProcessor processor;
        set (processor, "wet", -60.0f);
        set (processor, "softClip", 1.0f);
        set (processor, "clipOs", factor);
        set (processor, "bypass", bypassed ? 1.0f : 0.0f);

        processor.setRateAndBufferSizeDetails (rate, 333);
        processor.prepareToPlay (rate, 333);
        const auto latency = processor.getLatencySamples();
        processor.releaseResources();
        REQUIRE (latency > 0);

        const auto output = render (processor, input, nullptr, 333, false);
        auto worst = 0.0;

        for (int i = samples (0.05); i < (int) input.size(); ++i)
            worst = std::max (worst, std::abs ((double) output[(size_t) i] - (double) input[(size_t) (i - latency)]));

        CHECK (worst < 1.0e-3);
    }

    // Back to Off: the latency drops to 0 once the switch has faded through.
    KickerProcessor processor;
    set (processor, "clipOs", factor);
    processor.setRateAndBufferSizeDetails (rate, 256);
    processor.prepareToPlay (rate, 256);
    CHECK (processor.getLatencySamples() > 0);

    set (processor, "clipOs", 0.0f);
    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;

    for (int block = 0; block < 40; ++block)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
    }

    CHECK (processor.getLatencySamples() == 0);
    processor.releaseResources();
}

// Duck, test 2: the model's mix --------------------------------------------------------------

namespace
{
// kicker-duck-model.py's bass line (55 to 82 Hz in 8th notes) and a kick on every beat.
std::vector<float> bassLine (int numSamples)
{
    const double notes[] { 55.0, 65.4, 73.4, 82.4 };
    std::vector<float> bass ((size_t) numSamples, 0.0f);
    const auto noteLength = samples (0.25);

    for (int k = 0; k * noteLength < numSamples; ++k)
        for (int m = 0; m < noteLength && k * noteLength + m < numSamples; ++m)
            bass[(size_t) (k * noteLength + m)] = (float) (0.35 * std::sin (2.0 * pi * notes[(k / 2) % 4] * m / rate)
                                                           * std::min (1.0, m / (0.005 * rate)));

    return bass;
}

std::vector<float> kickTrack (int numSamples)
{
    std::vector<float> kick ((size_t) numSamples, 0.0f);

    for (int k = 0; k * samples (0.5) < numSamples; ++k)
        for (int m = 0; m < samples (0.3) && k * samples (0.5) + m < numSamples; ++m)
            kick[(size_t) (k * samples (0.5) + m)] += (float) (0.8 * std::sin (2.0 * pi * 50.0 * m / rate) * std::exp (-m / (0.12 * rate)));

    return kick;
}

// The ducked output's level against Duck 0's, in dB, over the samples where `inside` holds.
template <typename Inside>
double ratioDb (const std::vector<float>& ducked, const std::vector<float>& plain, Inside inside)
{
    auto a = 0.0, b = 0.0;

    for (int i = 0; i < (int) ducked.size(); ++i)
    {
        if (inside (i))
        {
            a += (double) ducked[(size_t) i] * (double) ducked[(size_t) i];
            b += (double) plain[(size_t) i] * (double) plain[(size_t) i];
        }
    }

    return 10.0 * std::log10 (a / b);
}
} // namespace

TEST_CASE ("Kicker's Duck clears the low end under each kick and lets the bass back before the next", "[kicker][w2]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto bass = bassLine (samples (4.0));
    const auto kick = kickTrack (samples (4.0));

    KickerProcessor plain, ducked;
    set (ducked, "duck", 6.0f);
    set (ducked, "duckRelease", 120.0f);

    const auto plainOut = render (plain, bass, &kick, 256, true);
    const auto duckedOut = render (ducked, bass, &kick, 256, true);

    const auto under = ratioDb (duckedOut, plainOut, [] (int i) { return std::fmod ((double) i / rate, 0.5) < 0.12; });
    const auto back = ratioDb (duckedOut, plainOut, [] (int i)
                               {
                                   const auto phase = std::fmod ((double) i / rate, 0.5);
                                   return (double) i / rate > 0.5 && phase >= 0.4 && phase < 0.5;
                               });

    std::printf ("kicker duck: under each kick %.2f dB, the last 100 ms before the next %.2f dB\n", under, back);

    CHECK (under <= -5.0);
    CHECK (back >= -2.0);
}

// Duck, test 3: a kick's tail held at -30 dBFS doesn't duck ---------------------------------------

TEST_CASE ("Kicker's Duck ignores a key held at -30 dBFS", "[kicker][w2]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto bass = bassLine (samples (1.0));
    std::vector<float> key ((size_t) bass.size());

    for (size_t i = 0; i < key.size(); ++i)
        key[i] = (float) (std::pow (10.0, -30.0 / 20.0) * std::sin (2.0 * pi * 50.0 * (double) i / rate));

    KickerProcessor plain, ducked;
    set (ducked, "duck", 24.0f);

    const auto plainOut = render (plain, bass, &key, 256, true);
    const auto duckedOut = render (ducked, bass, &key, 256, true);

    auto differences = 0;

    for (size_t i = 0; i < plainOut.size(); ++i)
        if (plainOut[i] != duckedOut[i])
            ++differences;

    CHECK (differences == 0);
}

// Duck, test 4: Host sync at 1/4, 120 BPM, ducks on every beat to the sample ---------------------

TEST_CASE ("Kicker's Duck from Host sync ducks on every beat to the sample at any block size", "[kicker][w2]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto block = GENERATE (64, 333, 4096);
    INFO ("block " << block);

    // A steady 0.5 on the dry path only, so the output is 0.5 times the duck's gain. With no
    // play head, Host sync runs on its own at 120 BPM from the first sample.
    const std::vector<float> input ((size_t) samples (2.0), 0.5f);

    KickerProcessor processor;
    set (processor, "wet", -60.0f);
    set (processor, "duck", 6.0f);
    set (processor, "duckFrom", 1.0f);
    set (processor, "duckRate", (float) KickerProcessor::defaultSyncRate);
    const auto output = render (processor, input, nullptr, block, false);

    // Where the gain starts falling fast (more than 0.01 dB a sample) after it wasn't.
    std::vector<int> onsets;

    for (int i = 2; i < (int) output.size(); ++i)
    {
        const auto step = 20.0 * std::log10 ((double) output[(size_t) i] / (double) output[(size_t) (i - 1)]);
        const auto before = 20.0 * std::log10 ((double) output[(size_t) (i - 1)] / (double) output[(size_t) (i - 2)]);

        if (step < -0.01 && before >= -0.01)
            onsets.push_back (i);
    }

    REQUIRE (onsets.size() == 3);

    for (size_t k = 0; k < onsets.size(); ++k)
        CHECK (onsets[k] == (int) (k + 1) * samples (0.5));

    // And it ducks by Duck at the bottom of each pulse.
    CHECK (20.0 * std::log10 ((double) output[(size_t) (samples (0.5) + samples (0.2))] / 0.5) == Catch::Approx (-6.0).margin (0.05));
}

// The detector alone: full above -12 dBFS, none below -24 ------------------------------------------

TEST_CASE ("KickerDucker ducks fully above -12 dBFS and not below -24", "[kicker][w2]")
{
    KickerDucker ducker;
    ducker.prepare (rate);
    ducker.setDepth (12.0);
    ducker.setRelease (120.0);

    const auto settle = [&] (double keyDb)
    {
        std::vector<float> key ((size_t) samples (0.5), (float) std::pow (10.0, keyDb / 20.0)), gain (key.size());
        ducker.reset();
        ducker.process (key.data(), gain.data(), (int) key.size());
        return 20.0 * std::log10 ((double) gain.back());
    };

    CHECK (settle (-6.0) == Catch::Approx (-12.0).margin (0.01));
    CHECK (settle (-12.0) == Catch::Approx (-12.0).margin (0.01));
    CHECK (settle (-18.0) == Catch::Approx (-6.0).margin (0.01));
    CHECK (settle (-25.0) == 0.0);
    CHECK (ducker.isIdle() == false); // depth set, even with nothing ducked

    ducker.setDepth (0.0);
    CHECK (ducker.isIdle());
}

// CPU, for docs/measurements/kicker.txt: the processor at its defaults, and with Duck from the
// sidechain, Soft clip and Clip oversampling 4x. Five runs each, the median. Hidden from the
// default run; CI runs it with the other measurements.
TEST_CASE ("kicker duck and clip oversampling CPU measurement", "[.measure]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    constexpr int block = 128;
    constexpr int numBlocks = 7500; // 20 s of audio

    const auto measure = [&] (bool loaded)
    {
        std::vector<double> runs;

        for (int run = 0; run < 5; ++run)
        {
            KickerProcessor processor;

            if (loaded)
            {
                set (processor, "duck", 6.0f);
                set (processor, "softClip", 1.0f);
                set (processor, "clipOs", 2.0f);
            }

            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add (juce::AudioChannelSet::stereo());
            layout.inputBuses.add (juce::AudioChannelSet::stereo());
            layout.outputBuses.add (juce::AudioChannelSet::stereo());
            REQUIRE (processor.setBusesLayout (layout));
            processor.setRateAndBufferSizeDetails (rate, block);
            processor.prepareToPlay (rate, block);

            juce::AudioBuffer<float> buffer (4, block);
            juce::MidiBuffer midi;
            auto phase = 0.0;
            const auto start = std::chrono::steady_clock::now();

            for (int b = 0; b < numBlocks; ++b)
            {
                for (int i = 0; i < block; ++i, phase += 2.0 * pi * 50.0 / rate)
                    for (int ch = 0; ch < 4; ++ch)
                        buffer.setSample (ch, i, (float) (0.5 * std::sin (phase)));

                processor.processBlock (buffer, midi);
            }

            const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
            runs.push_back (100.0 * elapsed.count() / ((double) (numBlocks * block) / rate));
            processor.releaseResources();
        }

        std::sort (runs.begin(), runs.end());
        return runs[2];
    };

    const auto defaults = measure (false);
    const auto loaded = measure (true);
    std::printf ("Kicker processor CPU at 48 kHz, 128 samples, stereo: defaults %.4f %%, Duck + Soft clip + 4x %.4f %% (%.2fx)\n",
                 defaults, loaded, loaded / defaults);
}
