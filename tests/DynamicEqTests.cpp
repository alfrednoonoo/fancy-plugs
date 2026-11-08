// Phase 2.1 item D: dynamic bands in Fancy EQ. Design: docs/phase2_1-D.md.

#include "EqDsp.h"
#include "EqProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <fancy/Decibels.h>

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <functional>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = 3.14159265358979323846;
constexpr int blockSize = 256;

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples), right ((size_t) numSamples) {}
    int size() const { return (int) left.size(); }
};

// A sine whose RMS level is levelDb, so a full-scale sine is -3 dB.
Stereo sine (double hz, double levelDb, int numSamples, double startPhase = 0.0)
{
    Stereo signal (numSamples);
    const auto amplitude = std::sqrt (2.0) * std::pow (10.0, levelDb / 20.0);

    for (int i = 0; i < numSamples; ++i)
        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) (amplitude * std::sin (startPhase + 2.0 * pi * hz * i / rate));

    return signal;
}

eq::BandSpec band (eq::BandType type, double hz, double gainDb = 0.0, double q = 1.0)
{
    eq::BandSpec spec;
    spec.type = type;
    spec.frequencyHz = hz;
    spec.gainDb = gainDb;
    spec.q = q;
    spec.on = true;
    return spec;
}

eq::DynamicSpec dynamics (double thresholdDb = -24.0, double rangeDb = -6.0, double attackMs = 5.0, double releaseMs = 80.0)
{
    eq::DynamicSpec spec;
    spec.on = true;
    spec.thresholdDb = thresholdDb;
    spec.rangeDb = rangeDb;
    spec.attackMs = attackMs;
    spec.releaseMs = releaseMs;
    return spec;
}

EqDsp::Targets targets (const eq::BandSpec& spec, const eq::DynamicSpec& dynamic = {})
{
    EqDsp::Targets result;
    result.bands[0] = spec;
    result.dynamics[0] = dynamic;
    return result;
}

// Renders in blocks, calling `each` after every block with the samples done so far.
Stereo render (const EqDsp::Targets& settings, const Stereo& input, const Stereo* sidechain = nullptr,
               std::function<void (EqDsp&, int)> each = {}, int stepLength = EqDsp::stepSamples)
{
    EqDsp dsp;
    dsp.setStepLength (stepLength);
    dsp.prepare (rate);
    dsp.setSynchronousDesign (true);
    dsp.snapToTargets (settings);

    auto output = input;

    for (int start = 0; start < output.size(); start += blockSize)
    {
        const auto n = std::min (blockSize, output.size() - start);
        float* channels[] { output.left.data() + start, output.right.data() + start };

        if (sidechain != nullptr)
        {
            const float* side[] { sidechain->left.data() + start, sidechain->right.data() + start };
            dsp.process (channels, 2, n, side, 2);
        }
        else
        {
            dsp.process (channels, 2, n);
        }

        if (each)
            each (dsp, start + n);
    }

    return output;
}

// The level of a tone, by correlation with a Hann window, over [from, end).
double levelAt (const std::vector<float>& samples, int from, double hz)
{
    const auto count = (int) samples.size() - from;
    std::complex<double> sum = 0.0;
    double windowSum = 0.0;

    for (int i = 0; i < count; ++i)
    {
        const auto window = 0.5 - 0.5 * std::cos (2.0 * pi * i / count);
        const auto phase = 2.0 * pi * hz * (from + i) / rate;
        sum += (double) samples[(size_t) (from + i)] * window * std::complex<double> (std::cos (phase), -std::sin (phase));
        windowSum += window;
    }

    return fancy::gainToDb (2.0 * std::abs (sum) / windowSum, -300.0);
}

// The change the band settles on, read from the DSP at the end of a render.
double finalChange (const EqDsp::Targets& settings, const Stereo& input, const Stereo* sidechain = nullptr)
{
    double change = 0.0;
    render (settings, input, sidechain, [&change] (EqDsp& dsp, int) { change = dsp.getDynamicChange (0); });
    return change;
}

bool identical (const Stereo& a, const Stereo& b)
{
    return a.left == b.left && a.right == b.right;
}

// Everything outside +-1/3 octave of the tone, relative to the whole, in dB (THD+N).
double thdPlusNoise (const std::vector<float>& samples, double hz)
{
    constexpr int order = 17;
    constexpr int size = 1 << order;

    juce::dsp::FFT fft (order);
    std::vector<float> data ((size_t) size * 2, 0.0f);

    const auto from = std::max (0, (int) samples.size() - size);

    for (int i = 0; i < size && from + i < (int) samples.size(); ++i)
        data[(size_t) i] = samples[(size_t) (from + i)] * (float) (0.5 - 0.5 * std::cos (2.0 * pi * i / size));

    fft.performFrequencyOnlyForwardTransform (data.data());

    const auto low = hz / std::pow (2.0, 1.0 / 3.0), high = hz * std::pow (2.0, 1.0 / 3.0);
    double inside = 0.0, outside = 0.0;

    for (int bin = 1; bin < size / 2; ++bin)
    {
        const auto binHz = bin * rate / size;
        const auto power = (double) data[(size_t) bin] * data[(size_t) bin];
        (binHz >= low && binHz <= high ? inside : outside) += power;
    }

    return 10.0 * std::log10 ((outside + 1.0e-300) / (inside + outside + 1.0e-300));
}
} // namespace

TEST_CASE ("Dynamic EQ: 20 dB under Threshold, a dynamic band is bit-identical to a static one", "[eq][dynamic]")
{
    const auto input = sine (1000.0, -44.0, (int) rate);
    const auto spec = band (eq::BandType::band, 1000.0, 3.0);

    const auto dynamic = render (targets (spec, dynamics()), input);
    const auto staticBand = render (targets (spec), input);

    CHECK (identical (dynamic, staticBand));
}

TEST_CASE ("Dynamic EQ: 12 dB over Threshold, a -6 dB Range gives -6 dB, at the Attack time", "[eq][dynamic]")
{
    const auto input = sine (1000.0, -12.0, (int) (rate * 1.5));
    const auto spec = band (eq::BandType::band, 1000.0);

    // Steady state, from the DSP and from the output itself.
    const auto settings = targets (spec, dynamics (-24.0, -6.0, 50.0));
    const auto output = render (settings, input);
    const auto measured = levelAt (output.left, (int) rate, 1000.0) - levelAt (input.left, (int) rate, 1000.0);

    CHECK (std::abs (finalChange (settings, input) - (-6.0)) < 0.2);
    CHECK (std::abs (measured - (-6.0)) < 0.2);

    // 63 % of the change within 1.2 x Attack.
    std::vector<double> changes;
    EqDsp dsp;
    dsp.prepare (rate);
    dsp.snapToTargets (settings);
    auto copy = input;

    for (int i = 0; i < copy.size(); ++i)
    {
        float* channels[] { copy.left.data() + i, copy.right.data() + i };
        dsp.process (channels, 2, 1);
        changes.push_back (dsp.getDynamicChange (0));
    }

    const auto reached = std::find_if (changes.begin(), changes.end(), [] (double change) { return change <= -6.0 * 0.632; });
    REQUIRE (reached != changes.end());

    const auto milliseconds = 1000.0 * (double) std::distance (changes.begin(), reached) / rate;
    INFO ("63 % after " << milliseconds << " ms, Attack 50 ms");
    CHECK (milliseconds <= 1.2 * 50.0);
}

TEST_CASE ("Dynamic EQ: a tone two octaves away doesn't move the band", "[eq][dynamic]")
{
    const auto spec = band (eq::BandType::band, 1000.0);

    for (const auto hz : { 250.0, 4000.0 })
    {
        INFO (hz << " Hz");
        CHECK (std::abs (finalChange (targets (spec, dynamics()), sine (hz, -12.0, (int) rate))) < 0.1);
    }
}

TEST_CASE ("Dynamic EQ: a level sweeping across Threshold adds no zipper noise", "[eq][dynamic]")
{
    // A 1 kHz tone rising from -48 to -6 dB and back over 2.7 s, through a band with a fast
    // Attack and Release, compared with the same band redesigned on every sample.
    const auto numSamples = 1 << 17;
    Stereo input (numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto position = (double) i / numSamples;
        const auto levelDb = -48.0 + 42.0 * std::sin (pi * position);
        const auto value = std::sqrt (2.0) * std::pow (10.0, levelDb / 20.0) * std::sin (2.0 * pi * 1000.0 * i / rate);
        input.left[(size_t) i] = input.right[(size_t) i] = (float) value;
    }

    const auto settings = targets (band (eq::BandType::band, 1000.0, 0.0, 2.0), dynamics (-30.0, -12.0, 1.0, 20.0));

    const auto every16 = render (settings, input);
    const auto everySample = render (settings, input, nullptr, {}, 1);

    const auto zipper = thdPlusNoise (every16.left, 1000.0);
    const auto reference = thdPlusNoise (everySample.left, 1000.0);

    INFO ("THD+N " << zipper << " dB, redesigned every sample " << reference << " dB");
    CHECK (zipper <= reference + 3.0);
}

TEST_CASE ("Dynamic EQ: Below mode acts on quiet signals only", "[eq][dynamic]")
{
    const auto spec = band (eq::BandType::band, 1000.0, 2.0);
    auto settings = dynamics();
    settings.below = true;

    // 20 dB under: the full Range.
    CHECK (std::abs (finalChange (targets (spec, settings), sine (1000.0, -44.0, (int) rate)) - (-6.0)) < 0.2);

    // 12 dB over: once the level is known it lets go to exactly 0 and sounds as the static band.
    // (A render starts from silence, which is quiet, so Below acts for the first milliseconds.)
    const auto loud = sine (1000.0, -12.0, (int) rate);
    const auto dynamicOut = render (targets (spec, settings), loud);
    const auto staticOut = render (targets (spec), loud);

    CHECK (finalChange (targets (spec, settings), sine (1000.0, -12.0, (int) rate * 3)) == 0.0); // after Release has run its course
    CHECK (std::abs (levelAt (dynamicOut.left, (int) (rate / 2), 1000.0) - levelAt (staticOut.left, (int) (rate / 2), 1000.0)) < 0.001);
}

TEST_CASE ("Dynamic EQ: Sidechain makes the band listen to the sidechain only", "[eq][dynamic]")
{
    const auto spec = band (eq::BandType::band, 1000.0);
    auto settings = dynamics();
    settings.sidechain = true;

    // The main input has nothing near 1 kHz; the sidechain does.
    const auto main = sine (4000.0, -12.0, (int) rate);
    const auto side = sine (1000.0, -12.0, (int) rate);

    CHECK (std::abs (finalChange (targets (spec, settings), main, &side) - (-6.0)) < 0.2);

    // The other way round: a loud main input at 1 kHz with a quiet sidechain doesn't move it.
    const auto loudMain = sine (1000.0, -12.0, (int) rate);
    const auto quietSide = sine (1000.0, -60.0, (int) rate);
    CHECK (std::abs (finalChange (targets (spec, settings), loudMain, &quietSide)) < 0.1);

    // Without Sidechain ticked, the same main input moves it.
    CHECK (std::abs (finalChange (targets (spec, dynamics()), loudMain, &quietSide) - (-6.0)) < 0.2);
}

TEST_CASE ("Dynamic EQ: a low shelf hears below its Freq, and a dynamic notch cuts only when loud", "[eq][dynamic]")
{
    const auto shelf = band (eq::BandType::lowShelf, 200.0);

    CHECK (std::abs (finalChange (targets (shelf, dynamics()), sine (60.0, -12.0, (int) rate)) - (-6.0)) < 0.3);
    CHECK (std::abs (finalChange (targets (shelf, dynamics()), sine (3000.0, -12.0, (int) rate))) < 0.1);

    const auto notch = band (eq::BandType::notch, 1000.0, 0.0, 4.0);
    auto settings = dynamics (-24.0, -12.0);

    // Quiet: it passes (a 0 dB bell), so 1 kHz comes through at its own level.
    const auto quiet = sine (1000.0, -44.0, (int) rate);
    const auto quietOut = render (targets (notch, settings), quiet);
    CHECK (std::abs (levelAt (quietOut.left, (int) (rate / 2), 1000.0) - levelAt (quiet.left, (int) (rate / 2), 1000.0)) < 0.01);

    // Loud: it cuts by the Range.
    const auto loud = sine (1000.0, -12.0, (int) rate);
    const auto loudOut = render (targets (notch, settings), loud);
    CHECK (std::abs (levelAt (loudOut.left, (int) (rate / 2), 1000.0) - levelAt (loud.left, (int) (rate / 2), 1000.0) - (-12.0)) < 0.3);

    // A positive Range doesn't make a notch boost.
    settings.rangeDb = 12.0;
    CHECK (finalChange (targets (notch, settings), loud) == 0.0);
}

TEST_CASE ("Dynamic EQ: in Linear phase, dynamic bands run as their static settings", "[eq][dynamic]")
{
    auto dynamic = targets (band (eq::BandType::band, 1000.0, 4.0), dynamics());
    auto staticBand = targets (band (eq::BandType::band, 1000.0, 4.0));
    dynamic.linearPhase = staticBand.linearPhase = true;

    const auto input = sine (1000.0, -6.0, (int) rate);
    const auto a = render (dynamic, input);
    const auto b = render (staticBand, input);

    CHECK (identical (a, b));
    CHECK (finalChange (dynamic, input) == 0.0);
}

TEST_CASE ("Dynamic EQ: older states load with every band static, even into an edited instance", "[eq][dynamic][compat]")
{
    EqProcessor original;
    original.setParameter ("b2gain", 4.0f);
    original.setParameter ("b2freq", 700.0f);

    juce::MemoryBlock saved;
    original.getStateInformation (saved);

    // As version 2 saved it: none of the dynamic parameters.
    auto tree = juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize()));

    for (int i = tree.getNumChildren(); --i >= 0;)
    {
        const auto id = tree.getChild (i).getProperty ("id").toString();

        for (const auto* suffix : { "dyn", "thresh", "range", "attack", "release", "dynMode", "dynSc" })
            if (id.startsWithChar ('b') && id.endsWith (suffix) && juce::CharacterFunctions::isDigit (id[1]))
                tree.removeChild (i, nullptr);
    }

    tree.setProperty ("stateVersion", 2, nullptr);
    juce::MemoryBlock older;
    juce::AudioProcessor::copyXmlToBinary (*tree.createXml(), older);

    // An instance with dynamic bands switched on, which the older state must switch off.
    EqProcessor edited;
    edited.setParameter ("b2dyn", 1.0f);
    edited.setParameter ("b2range", 12.0f);
    edited.setStateInformation (older.getData(), (int) older.getSize());

    for (int b = 0; b < eq::maxBands; ++b)
        CHECK_FALSE (edited.readDynamics (b).on);

    CHECK (edited.readDynamics (1).rangeDb == -6.0);
    CHECK (edited.readBand (1).gainDb == 4.0);

    // It renders exactly as the current state does.
    EqProcessor current;
    current.setStateInformation (saved.getData(), (int) saved.getSize());

    const auto renderProcessor = [] (EqProcessor& processor)
    {
        processor.setRateAndBufferSizeDetails (rate, blockSize);
        processor.prepareToPlay (rate, blockSize);

        juce::AudioBuffer<float> all (4, (int) rate);
        all.clear();
        std::mt19937 noise (3);
        std::uniform_real_distribution<float> sample (-0.3f, 0.3f);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < all.getNumSamples(); ++i)
                all.setSample (ch, i, sample (noise));

        juce::MidiBuffer midi;

        for (int start = 0; start < all.getNumSamples(); start += blockSize)
        {
            juce::AudioBuffer<float> block (all.getArrayOfWritePointers(), processor.getTotalNumInputChannels(), start, blockSize);
            processor.processBlock (block, midi);
        }

        processor.releaseResources();
        return all;
    };

    const auto expected = renderProcessor (current);
    const auto actual = renderProcessor (edited);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < expected.getNumSamples(); ++i)
            if (expected.getSample (ch, i) != actual.getSample (ch, i))
                FAIL ("sample " << i << " on channel " << ch << " differs");
}

TEST_CASE ("Dynamic EQ: the sidechain bus is optional, mono or stereo, and the I/O pill counts it", "[eq][dynamic][host]")
{
    EqProcessor processor;

    auto layout = processor.getBusesLayout();
    REQUIRE (layout.inputBuses.size() == 2);
    CHECK (layout.inputBuses[1].isDisabled());

    for (const auto& sidechain : { juce::AudioChannelSet::disabled(), juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo() })
    {
        layout.inputBuses.getReference (1) = sidechain;
        CHECK (processor.setBusesLayout (layout));
    }
}

// CPU, by the method in §0.4 of the Phase 2.1 brief: 8 dynamic bands on noise that keeps them
// moving, against Fancy Comp's defaults in the same run. The target is < 1.5 %.
TEST_CASE ("Dynamic EQ CPU measurement", "[.measure]")
{
    constexpr int measureBlock = 128;
    constexpr double seconds = 10.0;

    const auto run = [] (bool dynamic)
    {
        EqDsp::Targets settings;
        constexpr double centres[] { 100.0, 200.0, 400.0, 800.0, 1600.0, 3200.0, 6400.0, 12800.0 };

        for (int b = 0; b < 8; ++b)
        {
            settings.bands[(size_t) b] = band (eq::BandType::band, centres[b], 2.0, 1.5);

            if (dynamic)
                settings.dynamics[(size_t) b] = dynamics (-30.0, -6.0, 5.0, 80.0);
        }

        std::vector<double> runs;

        for (int attempt = 0; attempt < 5; ++attempt)
        {
            EqDsp dsp;
            dsp.prepare (rate);
            dsp.snapToTargets (settings);

            std::mt19937 random (8);
            std::uniform_real_distribution<float> noise (-0.5f, 0.5f);
            float left[measureBlock], right[measureBlock];
            float* channels[] { left, right };
            std::chrono::duration<double> spent {};
            double swell = 0.0;

            for (int done = 0; done < (int) (seconds * rate); done += measureBlock)
            {
                // Noise whose level swells, so the bands keep crossing Threshold.
                swell += 2.0 * pi * measureBlock / rate;
                const auto level = (float) (0.5 + 0.45 * std::sin (swell));

                for (int i = 0; i < measureBlock; ++i)
                {
                    left[i] = level * noise (random);
                    right[i] = level * noise (random);
                }

                const auto start = std::chrono::steady_clock::now();
                dsp.process (channels, 2, measureBlock);
                spent += std::chrono::steady_clock::now() - start;
            }

            runs.push_back (100.0 * spent.count() / seconds);
        }

        std::sort (runs.begin(), runs.end());
        return runs[2];
    };

    const auto staticBands = run (false);
    const auto dynamicBands = run (true);

    std::printf ("dynamic eq cpu (median of 5, 48 kHz, 128 samples, stereo): 8 static bells %.3f %%, 8 dynamic bells %.3f %% (target < 1.5 %%)\n",
                 staticBands, dynamicBands);
}
