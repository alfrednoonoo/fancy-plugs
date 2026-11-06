#include "CompDsp.h"
#include "CompProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <fancy/Decibels.h>

#include <algorithm>
#include <chrono>
#include <complex>
#include <cstdio>
#include <limits>
#include <numbers>
#include <vector>

namespace
{
constexpr double pi = std::numbers::pi;
constexpr double rate = 48000.0;

struct Audio
{
    std::vector<float> left, right;

    explicit Audio (int numSamples, float value = 0.0f)
        : left ((size_t) numSamples, value), right ((size_t) numSamples, value) {}

    int size() const { return (int) left.size(); }

    void process (CompDsp& dsp, Audio* key = nullptr, int blockSize = 256)
    {
        for (int start = 0; start < size(); start += blockSize)
        {
            float* main[] { left.data() + start, right.data() + start };
            const float* keyChannels[2] {};

            if (key != nullptr)
            {
                keyChannels[0] = key->left.data() + start;
                keyChannels[1] = key->right.data() + start;
            }

            dsp.process (main, 2, keyChannels, key != nullptr ? 2 : 0, std::min (blockSize, size() - start));
        }
    }
};

Audio sine (double hz, double amplitude, double sampleRate, int numSamples)
{
    Audio signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * hz * i / sampleRate));

    return signal;
}

double levelAt (const std::vector<float>& samples, int from, double hz, double sampleRate)
{
    const auto count = (int) samples.size() - from;
    std::complex<double> sum = 0.0;
    double windowSum = 0.0;

    for (int i = 0; i < count; ++i)
    {
        const auto window = 0.5 - 0.5 * std::cos (2.0 * pi * i / count);
        const auto phase = 2.0 * pi * hz * i / sampleRate;

        sum += (double) samples[(size_t) (from + i)] * window * std::complex<double> (std::cos (phase), -std::sin (phase));
        windowSum += window;
    }

    return fancy::gainToDb (2.0 * std::abs (sum) / windowSum, -200.0);
}

double peakOf (const std::vector<float>& samples, int from = 0, int to = -1)
{
    double peak = 0.0;

    for (size_t i = (size_t) from; i < (to < 0 ? samples.size() : (size_t) to); ++i)
        peak = std::max (peak, (double) std::abs (samples[i]));

    return peak;
}

// Highest inter-sample level, found by 8x windowed-sinc interpolation.
double truePeakOf (const std::vector<float>& samples, int from)
{
    constexpr int halfLength = 32;
    constexpr int oversampling = 8;

    double peak = 0.0;

    for (int i = from + halfLength; i < (int) samples.size() - halfLength; ++i)
    {
        for (int phase = 0; phase < oversampling; ++phase)
        {
            const auto fraction = (double) phase / oversampling;
            double sum = 0.0;

            for (int tap = -halfLength + 1; tap <= halfLength; ++tap)
            {
                const auto x = (double) tap - fraction;
                const auto sinc = std::abs (x) < 1.0e-9 ? 1.0 : std::sin (pi * x) / (pi * x);
                const auto window = 0.5 + 0.5 * std::cos (pi * x / halfLength);

                sum += (double) samples[(size_t) (i + tap)] * sinc * window;
            }

            peak = std::max (peak, std::abs (sum));
        }
    }

    return peak;
}

CompDsp::Targets peakTargets()
{
    CompDsp::Targets targets;
    targets.rmsMs = 0.0;
    targets.kneeDb = 0.0;
    targets.thresholdDb = -20.0;
    targets.ratio = 4.0;
    targets.attackMs = 0.0;
    targets.releaseMs = 50.0;
    return targets;
}

void prepare (CompDsp& dsp, const CompDsp::Targets& targets, double sampleRate = rate)
{
    dsp.prepare (sampleRate);
    dsp.snapToTargets (targets);
}

void setParameter (CompProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getApvts().getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

std::vector<float> render (CompProcessor& processor, int numSamples)
{
    juce::AudioBuffer<float> buffer (4, numSamples);
    buffer.clear();
    juce::MidiBuffer midi;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto sample = (float) (0.5 * std::sin (2.0 * pi * 300.0 * i / rate));
        buffer.setSample (0, i, sample);
        buffer.setSample (1, i, sample);
    }

    processor.processBlock (buffer, midi);

    return { buffer.getReadPointer (0), buffer.getReadPointer (0) + numSamples };
}

double stepAfter (CompDsp::Targets targets, double attackMs)
{
    targets.attackMs = attackMs;

    CompDsp dsp;
    prepare (dsp, targets);

    Audio signal (24000, 0.03f);
    std::fill (signal.left.begin() + 2400, signal.left.begin() + 2880, 1.0f);
    std::fill (signal.right.begin() + 2400, signal.right.begin() + 2880, 1.0f);
    signal.process (dsp);

    return dsp.takeGainReduction (0);
}
} // namespace

TEST_CASE ("static curve", "[comp]")
{
    const auto reduction = [] (double level, double knee, bool weird = false)
    {
        return CompDsp::computeReduction (level, -20.0, 0.75, knee, weird);
    };

    CHECK (reduction (-30.0, 0.0) == 0.0);
    CHECK (reduction (-8.0, 0.0) == Catch::Approx (9.0));

    SECTION ("the soft knee is centred on the threshold and continuous")
    {
        CHECK (reduction (-23.0, 6.0) == 0.0);
        CHECK (reduction (-17.0, 6.0) == Catch::Approx (2.25));
        CHECK (reduction (-20.0, 6.0) == Catch::Approx (0.5625));
        CHECK (reduction (-17.0001, 6.0) == Catch::Approx (reduction (-16.9999, 6.0)).margin (0.001));
    }

    SECTION ("the weird knee starts at the threshold")
    {
        CHECK (reduction (-20.0, 6.0, true) == 0.0);
        CHECK (reduction (-14.0001, 6.0, true) == Catch::Approx (reduction (-13.9999, 6.0, true)).margin (0.001));
        CHECK (reduction (-8.0, 6.0, true) == Catch::Approx (6.75));
    }
}

TEST_CASE ("compression matches the static curve", "[comp]")
{
    const auto oversampling = GENERATE ((int) CompDsp::off, (int) CompDsp::x2, (int) CompDsp::x4);

    auto targets = peakTargets();
    targets.oversampling = oversampling;

    CompDsp dsp;
    prepare (dsp, targets);

    Audio signal (24000, 0.398f); // -8 dBFS, 12 dB over the threshold
    signal.process (dsp);

    REQUIRE (fancy::gainToDb (std::abs (signal.left.back())) == Catch::Approx (-17.0).margin (0.05));
}

TEST_CASE ("infinite ratio holds the threshold", "[comp]")
{
    auto targets = peakTargets();
    targets.ratio = CompDsp::infiniteRatio;

    CompDsp dsp;
    prepare (dsp, targets);

    Audio signal (24000, 1.0f);
    signal.process (dsp);

    REQUIRE (fancy::gainToDb (std::abs (signal.left.back())) == Catch::Approx (-20.0).margin (0.05));
}

TEST_CASE ("attack time follows the convention", "[comp]")
{
    const auto attack90 = GENERATE (false, true);

    auto targets = peakTargets();
    targets.attackMs = 100.0;
    targets.attack90 = attack90;
    targets.thresholdDb = -60.0;
    targets.ratio = CompDsp::infiniteRatio;

    CompDsp dsp;
    prepare (dsp, targets);

    Audio signal (48000, 1.0f);
    signal.process (dsp);

    // At 100 ms the gain reduction is 63 % of its 60 dB target, or 90 % with the option on.
    const auto reductionAt100Ms = -fancy::gainToDb (std::abs (signal.left[4799]));
    const auto expected = attack90 ? 54.0 : 37.9;

    REQUIRE (reductionAt100Ms == Catch::Approx (expected).margin (0.5));
}

TEST_CASE ("maximum reduction ignores attack unless Classic is on", "[comp]")
{
    auto targets = peakTargets();
    targets.thresholdDb = -20.0;
    targets.ratio = 4.0;

    SECTION ("branching holds when sustained")
    {
        const auto settledReduction = [&] (double attackMs)
        {
            auto t = targets;
            t.attackMs = attackMs;

            CompDsp dsp;
            prepare (dsp, t);

            Audio signal (48000, 1.0f);
            signal.process (dsp);

            return dsp.takeGainReduction (0);
        };

        REQUIRE (settledReduction (1.0) == Catch::Approx (settledReduction (50.0)).margin (0.05));
    }

    SECTION ("a smoothed detector gives a short burst less reduction at longer attacks")
    {
        targets.classic = true;
        REQUIRE (stepAfter (targets, 1.0) > stepAfter (targets, 100.0) + 3.0);
    }
}

TEST_CASE ("release curves", "[comp]")
{
    const auto reductionAfterRelease = [] (bool linear, bool automatic)
    {
        auto targets = peakTargets();
        targets.thresholdDb = -40.0;
        targets.ratio = CompDsp::infiniteRatio;
        targets.releaseMs = 100.0;
        targets.linearRelease = linear;
        targets.autoRelease = automatic;

        CompDsp dsp;
        prepare (dsp, targets);

        Audio signal (48000, 0.001f); // -60 dBFS, below the threshold
        std::fill (signal.left.begin(), signal.left.begin() + 4800, 1.0f);
        std::fill (signal.right.begin(), signal.right.begin() + 4800, 1.0f);
        signal.process (dsp);

        // Reduction 100 ms after the signal drops, read from the quiet signal itself.
        return -fancy::gainToDb (std::abs (signal.left[9600]) / 0.001);
    };

    // 40 dB of reduction. Exponential falls to 37 % in one time constant.
    CHECK (reductionAfterRelease (false, false) == Catch::Approx (14.7).margin (0.5));

    // Linear falls 8.7 dB per time constant.
    CHECK (reductionAfterRelease (true, false) == Catch::Approx (31.3).margin (0.5));

    // The 100 ms burst only charged the slow envelope to 63 %, and it then decays to 9.3 dB.
    // The fast stage trails it by a factor of 1 / (1 - 0.1), which gives 10.3 dB.
    CHECK (reductionAfterRelease (false, true) == Catch::Approx (10.33).margin (0.1));
}

static double worstAliasDb (int oversampling)
{
    constexpr double sampleRate = 44100.0;

    auto targets = peakTargets();
    targets.attackMs = 0.1;
    targets.releaseMs = 1.0;
    targets.oversampling = oversampling;

    CompDsp dsp;
    prepare (dsp, targets, sampleRate);

    auto signal = sine (15000.0, 0.5, sampleRate, 88200);
    signal.process (dsp);

    // 15 kHz has no harmonics below Nyquist, so every harmonic that lands in the audio band
    // is an alias. Gain ripple on a sine creates mainly odd harmonics, so all of them count.
    double worst = -200.0;

    for (int harmonic = 2; harmonic <= 40; ++harmonic)
    {
        const auto wrapped = std::fmod ((double) harmonic * 15000.0, sampleRate);
        const auto alias = std::min (wrapped, sampleRate - wrapped);

        if (std::abs (alias - 15000.0) > 1.0 && alias > 1.0)
            worst = std::max (worst, levelAt (signal.left, 44100, alias, sampleRate));
    }

    return worst;
}

TEST_CASE ("aliasing is below -90 dBFS at 4x", "[comp]")
{
    REQUIRE (worstAliasDb (CompDsp::x4) < -90.0);
}

TEST_CASE ("limiter holds the ceiling at extreme levels", "[comp]")
{
    const auto oversampling = GENERATE ((int) CompDsp::off, (int) CompDsp::x2, (int) CompDsp::x4);
    const auto gainDb = GENERATE (0.0, 24.0, 100.0, 200.0);

    CompDsp::Targets targets;
    targets.thresholdDb = -60.0;
    targets.ratio = 1.0;
    targets.oversampling = oversampling;
    targets.limitOutput = true;
    targets.outputGain = fancy::dbToGain (std::min (gainDb, 12.0));

    CompDsp dsp;
    prepare (dsp, targets);

    // A tone with strong inter-sample peaks, at the requested level above full scale.
    Audio signal (24000);
    const auto amplitude = (float) std::min (fancy::dbToGain (gainDb), 1.0e7);

    for (int i = 0; i < signal.size(); ++i)
        signal.left[(size_t) i] = signal.right[(size_t) i] = amplitude * (float) std::sin (0.5 * pi * i + 0.785);

    signal.process (dsp);

    CHECK (peakOf (signal.left) <= 1.0);
    CHECK (truePeakOf (signal.left, 4800) < fancy::dbToGain (0.2));
}

TEST_CASE ("pre-comp latency is reported and wet and dry stay aligned", "[comp]")
{
    const auto oversampling = GENERATE ((int) CompDsp::off, (int) CompDsp::x2, (int) CompDsp::x4);
    const auto limit = GENERATE (false, true);

    CompDsp::Targets targets;
    targets.preCompMs = 5.0;
    targets.oversampling = oversampling;
    targets.limitOutput = limit;
    targets.wetGain = 0.5;
    targets.dryGain = 0.5;

    CompDsp dsp;
    prepare (dsp, targets);

    const auto latency = dsp.getLatencySamples();
    REQUIRE (latency >= 240);

    // With no compression, equal wet and dry sum to the input if and only if they align.
    // JUCE's fractional delay leaves the oversampled wet path within a small fraction of a
    // sample. This margin is about 0.03 samples at 5 kHz, where a whole sample would be 0.16.
    const auto input = sine (5000.0, 0.25, rate, 9600);
    auto output = input;
    output.process (dsp);

    const auto margin = oversampling == CompDsp::off ? 1.0e-5 : 6.0e-3;

    for (int i = 4800; i < input.size(); ++i)
        REQUIRE (output.left[(size_t) i] == Catch::Approx (input.left[(size_t) (i - latency)]).margin (margin));
}

TEST_CASE ("pre-comp catches a transient", "[comp]")
{
    auto targets = peakTargets();
    targets.attackMs = 1.0;
    targets.preCompMs = 5.0;

    CompDsp dsp;
    prepare (dsp, targets);

    Audio signal (9600);
    std::fill (signal.left.begin() + 4800, signal.left.end(), 1.0f);
    std::fill (signal.right.begin() + 4800, signal.right.end(), 1.0f);
    signal.process (dsp);

    // The delayed step arrives with most of the 15 dB of reduction already applied.
    const auto arrival = 4800 + dsp.getLatencySamples();
    REQUIRE (signal.left[(size_t) arrival] < 0.35f);
}

TEST_CASE ("sidechain drives the detector", "[comp]")
{
    auto targets = peakTargets();
    targets.detector = CompDsp::sideLR;

    CompDsp dsp;
    prepare (dsp, targets);

    Audio signal (24000, 0.05f);
    Audio key (24000, 1.0f);
    signal.process (dsp, &key);

    // 0.05 is below the threshold, but the key is 20 dB over it, so 15 dB is taken off.
    REQUIRE (std::abs (signal.left.back()) == Catch::Approx (0.05 * fancy::dbToGain (-15.0)).margin (0.001));
}

TEST_CASE ("detector filter is heard through preview and removes content", "[comp]")
{
    auto targets = peakTargets();
    targets.highpassHz = 2000.0;
    targets.previewFilter = true;

    CompDsp dsp;
    prepare (dsp, targets);

    auto signal = sine (100.0, 0.5, rate, 24000);
    signal.process (dsp);

    REQUIRE (peakOf (signal.left, 12000) < 0.01);
}

TEST_CASE ("detector modes choose the right signal", "[comp]")
{
    const auto reductionFor = [] (int detector)
    {
        auto targets = peakTargets();
        targets.detector = detector;
        targets.linkChannels = false;

        CompDsp dsp;
        prepare (dsp, targets);

        Audio signal (24000);
        std::fill (signal.left.begin(), signal.left.end(), 1.0f);
        std::fill (signal.right.begin(), signal.right.end(), -1.0f);
        signal.process (dsp);

        return dsp.takeGainReduction (0);
    };

    // Left is the negative of right, so Mid is silent and Side is at full level.
    CHECK (reductionFor (CompDsp::mid) == 0.0f);
    CHECK (reductionFor (CompDsp::side) == Catch::Approx (15.0f).margin (0.05f));
    CHECK (reductionFor (CompDsp::mainR) == Catch::Approx (15.0f).margin (0.05f));
    CHECK (reductionFor (CompDsp::sideL) == 0.0f);
}

TEST_CASE ("link channels shares the reduction", "[comp]")
{
    const auto quietChannelLevel = [] (bool link)
    {
        auto targets = peakTargets();
        targets.linkChannels = link;

        CompDsp dsp;
        prepare (dsp, targets);

        Audio signal (24000);
        std::fill (signal.left.begin(), signal.left.end(), 1.0f);
        std::fill (signal.right.begin(), signal.right.end(), 0.05f);
        signal.process (dsp);

        return (double) std::abs (signal.right.back());
    };

    CHECK (quietChannelLevel (false) == Catch::Approx (0.05).margin (1.0e-4));
    CHECK (quietChannelLevel (true) == Catch::Approx (0.05 * fancy::dbToGain (-15.0)).margin (1.0e-3));
}

TEST_CASE ("auto make-up compensates half of the reduction at full scale", "[comp]")
{
    auto targets = peakTargets();
    targets.autoMakeup = true;

    CompDsp dsp;
    prepare (dsp, targets);

    Audio signal (24000, 1.0f);
    signal.process (dsp);

    // 15 dB of reduction, 7.5 dB of make-up.
    REQUIRE (fancy::gainToDb (std::abs (signal.left.back())) == Catch::Approx (-7.5).margin (0.05));
}

TEST_CASE ("changing latency while playing is safe", "[comp]")
{
    auto targets = peakTargets();
    targets.thresholdDb = 0.0;

    CompDsp dsp;
    prepare (dsp, targets);

    auto signal = sine (1000.0, 0.25, rate, 48000);
    constexpr int blockSize = 128;

    for (int start = 0, block = 0; start < signal.size(); start += blockSize, ++block)
    {
        if (block == 40) targets.oversampling = CompDsp::x4;
        if (block == 130) targets.preCompMs = 3.0;
        if (block == 220) targets.limitOutput = true;

        dsp.setTargets (targets);

        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (channels, 2, nullptr, 0, std::min (blockSize, signal.size() - start));
    }

    double largestStep = 0.0;

    for (size_t i = 1; i < signal.left.size(); ++i)
    {
        REQUIRE (std::isfinite (signal.left[i]));
        largestStep = std::max (largestStep, (double) std::abs (signal.left[i] - signal.left[i - 1]));
    }

    CHECK (dsp.getLatencySamples() > 144);

    // A clean 0.25 peak at 1 kHz moves about 0.033 per sample.
    REQUIRE (largestStep < 0.05);
}

TEST_CASE ("rapid pre-comp changes wait until the value settles", "[comp]")
{
    CompDsp::Targets targets;
    targets.preCompMs = 0.0;

    CompDsp dsp;
    prepare (dsp, targets);

    float samples[2][128] {};
    float* channels[] { samples[0], samples[1] };

    for (int block = 0; block < 100; ++block)
    {
        targets.preCompMs = (double) block * 0.1;
        dsp.setTargets (targets);
        dsp.process (channels, 2, nullptr, 0, 128);

        REQUIRE (dsp.getLatencySamples() == 0);
    }

    for (int block = 0; block < 100; ++block)
        dsp.process (channels, 2, nullptr, 0, 128);

    CHECK (dsp.getLatencySamples() == (int) std::lround (9.9 * 48.0));
}

TEST_CASE ("bypass passes the delayed input exactly", "[comp]")
{
    auto targets = peakTargets();
    targets.preCompMs = 2.0;
    targets.bypass = true;

    CompDsp dsp;
    prepare (dsp, targets);

    const auto input = sine (440.0, 0.5, rate, 4800);
    auto output = input;
    output.process (dsp);

    const auto latency = dsp.getLatencySamples();

    for (int i = latency; i < input.size(); ++i)
        REQUIRE (output.left[(size_t) i] == input.left[(size_t) (i - latency)]);
}

TEST_CASE ("bad input never poisons the compressor", "[comp]")
{
    CompDsp dsp;
    prepare (dsp, peakTargets());

    Audio bad (64, std::numeric_limits<float>::quiet_NaN());
    bad.process (dsp);

    auto signal = sine (440.0, 0.5, rate, 4800);
    signal.process (dsp);

    for (const auto sample : signal.left)
        REQUIRE (std::isfinite (sample));
}

TEST_CASE ("comp block size doesn't change the output", "[comp]")
{
    const auto targets = peakTargets();

    CompDsp whole, single;
    prepare (whole, targets);
    prepare (single, targets);

    auto a = sine (440.0, 0.5, rate, 8192);
    auto b = a;

    a.process (whole, nullptr, 8192);
    b.process (single, nullptr, 1);

    for (size_t i = 0; i < a.left.size(); ++i)
        REQUIRE (a.left[i] == Catch::Approx (b.left[i]).margin (1.0e-6));
}

TEST_CASE ("comp works at every supported sample rate and in mono", "[comp]")
{
    const auto sampleRate = GENERATE (44100.0, 96000.0, 192000.0, 384000.0);

    auto targets = peakTargets();
    targets.oversampling = CompDsp::x4;
    targets.preCompMs = 250.0;
    targets.limitOutput = true;

    CompDsp dsp;
    prepare (dsp, targets, sampleRate);

    auto signal = sine (440.0, 0.5, sampleRate, 8192);
    float* channels[] { signal.left.data() };
    dsp.process (channels, 1, nullptr, 0, signal.size());

    for (const auto sample : signal.left)
        REQUIRE (std::isfinite (sample));
}

TEST_CASE ("comp automation with an unchanged value is bit-identical", "[comp]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    CompProcessor plain, automated;

    for (auto* processor : { &plain, &automated })
    {
        setParameter (*processor, "threshold", -20.0f);
        processor->prepareToPlay (rate, 2048);
    }

    for (auto* parameter : automated.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
        const auto before = automated.getApvts().getRawParameterValue (ranged->getParameterID())->load();

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->getValue());
        parameter->endChangeGesture();

        INFO (ranged->getParameterID());
        CHECK (automated.getApvts().getRawParameterValue (ranged->getParameterID())->load() == before);
    }

    REQUIRE (render (plain, 2048) == render (automated, 2048));
}

TEST_CASE ("comp state round-trips", "[comp]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    CompProcessor source;
    setParameter (source, "threshold", -24.0f);
    setParameter (source, "ratio", 8.0f);
    setParameter (source, "detector", 3.0f);
    setParameter (source, "limit", 1.0f);

    juce::MemoryBlock state;
    source.getStateInformation (state);

    CompProcessor destination;
    destination.setStateInformation (state.getData(), (int) state.getSize());

    auto& apvts = destination.getApvts();
    CHECK (apvts.getRawParameterValue ("threshold")->load() == Catch::Approx (-24.0f));
    CHECK (apvts.getRawParameterValue ("ratio")->load() == Catch::Approx (8.0f));
    CHECK (apvts.getRawParameterValue ("detector")->load() == 3.0f);
    CHECK (apvts.getRawParameterValue ("limit")->load() == 1.0f);
}

TEST_CASE ("corrupted comp state is ignored", "[comp]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    CompProcessor processor;

    juce::MemoryBlock state;
    processor.getStateInformation (state);
    processor.setStateInformation (state.getData(), (int) state.getSize() / 2);

    juce::Random random (9876);

    for (size_t i = 0; i < state.getSize(); ++i)
        state[i] = (char) random.nextInt (256);

    processor.setStateInformation (state.getData(), (int) state.getSize());

    processor.prepareToPlay (rate, 512);
    CHECK (render (processor, 512).size() == 512);
}

TEST_CASE ("comp supports sidechain layouts", "[comp]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    CompProcessor processor;

    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses = { juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo() };
    layout.outputBuses = { juce::AudioChannelSet::stereo() };
    CHECK (processor.checkBusesLayoutSupported (layout));

    layout.inputBuses = { juce::AudioChannelSet::mono(), juce::AudioChannelSet::disabled() };
    layout.outputBuses = { juce::AudioChannelSet::mono() };
    CHECK (processor.checkBusesLayoutSupported (layout));

    layout.inputBuses = { juce::AudioChannelSet::stereo(), juce::AudioChannelSet::disabled() };
    layout.outputBuses = { juce::AudioChannelSet::mono() };
    CHECK_FALSE (processor.checkBusesLayoutSupported (layout));
}

// Hidden from the default run. CI runs these on their own and keeps the output.
TEST_CASE ("comp measurements", "[.measure]")
{
    constexpr int blockSize = 128;
    constexpr int numBlocks = 18750; // 50 s of audio

    const char* const names[] { "Off", "2x", "4x" };

    for (int oversampling = 0; oversampling < 3; ++oversampling)
    {
        auto targets = peakTargets();
        targets.oversampling = oversampling;
        targets.rmsMs = 5.0;
        targets.limitOutput = false;

        CompDsp dsp;
        prepare (dsp, targets);

        const auto latency = dsp.getLatencySamples();

        auto signal = sine (440.0, 0.5, rate, blockSize);
        float* channels[] { signal.left.data(), signal.right.data() };

        const auto start = std::chrono::steady_clock::now();

        for (int i = 0; i < numBlocks; ++i)
            dsp.process (channels, 2, nullptr, 0, blockSize);

        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
        const auto percent = 100.0 * elapsed.count() / ((double) (numBlocks * blockSize) / rate);

        targets.limitOutput = true;
        CompDsp limited;
        prepare (limited, targets);

        std::printf ("Comp oversampling %-3s: latency %3d samples, %3d with Limit output; CPU %.4f %% at 48 kHz, 128 samples, stereo (target without oversampling < 1 %%); worst alias %.1f dBFS (15 kHz, attack 0.1 ms, release 1 ms)\n",
                     names[oversampling], latency, limited.getLatencySamples(), percent, worstAliasDb (oversampling));
    }
}
