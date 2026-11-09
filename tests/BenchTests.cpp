// FancyBench's analysis and test files (Phase 3 item Q1, docs/phase3-Q1.md §6), on synthetic
// signals whose answers are known.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Analysis.h"
#include "TestFile.h"

#if FANCY_HAS_COMP
 #include "CompProcessor.h"
#endif

#if FANCY_HAS_EQ
 #include "EqFilters.h"
#endif

#include <cmath>
#include <complex>
#include <cstdio>
#include <numbers>

namespace
{

using namespace bench;

constexpr double rate = 48000.0;
constexpr double pi = std::numbers::pi;

Signal delayed (const Signal& input, int samples)
{
    Signal output (input.size() + (size_t) samples, 0.0f);

    for (size_t i = 0; i < input.size(); ++i)
        output[i + (size_t) samples] = input[i];

    return output;
}

// RBJ cookbook peaking EQ, run in double.
struct Peaking
{
    double b0, b1, b2, a1, a2;

    Peaking (double hz, double gainDb, double q)
    {
        const auto a = std::pow (10.0, gainDb / 40.0);
        const auto w = 2.0 * pi * hz / rate;
        const auto alpha = std::sin (w) / (2.0 * q);
        const auto a0 = 1.0 + alpha / a;
        b0 = (1.0 + alpha * a) / a0;
        b1 = -2.0 * std::cos (w) / a0;
        b2 = (1.0 - alpha * a) / a0;
        a1 = -2.0 * std::cos (w) / a0;
        a2 = (1.0 - alpha / a) / a0;
    }

    std::complex<double> responseAt (double hz) const
    {
        const auto z1 = std::polar (1.0, -2.0 * pi * hz / rate);
        const auto z2 = z1 * z1;
        return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
    }

    Signal process (const Signal& input) const
    {
        Signal output (input.size());
        double z1 = 0.0, z2 = 0.0;

        for (size_t i = 0; i < input.size(); ++i)
        {
            const auto x = (double) input[i];
            const auto y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            output[i] = (float) y;
        }

        return output;
    }
};

// THD of a memoryless curve on a sine, from its Fourier series: no window and no FFT.
double seriesThdDb (double (*curve) (double, double), double drive, double amplitude)
{
    constexpr int points = 8192;
    double harmonics[11] {};

    for (int k = 1; k <= 10; ++k)
    {
        double sum = 0.0;

        for (int m = 0; m < points; ++m)
        {
            const auto theta = 2.0 * pi * m / points;
            sum += curve (amplitude * std::sin (theta), drive) * std::sin (k * theta);
        }

        harmonics[k] = 2.0 * sum / points;
    }

    double power = 0.0;

    for (int k = 2; k <= 10; ++k)
        power += harmonics[k] * harmonics[k];

    return 20.0 * std::log10 (std::sqrt (power) / std::abs (harmonics[1]));
}

double tanhCurve (double x, double drive) { return std::tanh (drive * x); }

} // namespace

TEST_CASE ("Bench: latency is the cross-correlation peak", "[bench]")
{
    const auto input = whiteNoise ((int) rate, -20.0, 1);

    for (const auto samples : { 0, 1, 64, 1234 })
        CHECK (measureDelay (input, delayed (input, samples), -64, 4096) == samples);

    // A symmetric linear-phase band-pass of 31 taps delays its peak by 15 samples.
    Signal taps (31);

    for (int n = 0; n < 31; ++n)
    {
        const auto t = n - 15;
        const auto sinc = [] (double x, double cutoff) { return x == 0.0 ? 2.0 * cutoff : std::sin (2.0 * pi * cutoff * x) / (pi * x); };
        const auto window = 0.5 - 0.5 * std::cos (2.0 * pi * n / 30.0);
        taps[(size_t) n] = (float) (window * (sinc (t, 0.25) - sinc (t, 0.05)));
    }

    Signal filtered (input.size() + taps.size(), 0.0f);

    for (size_t i = 0; i < input.size(); ++i)
        for (size_t k = 0; k < taps.size(); ++k)
            filtered[i + k] += input[i] * taps[k];

    CHECK (measureDelay (input, delayed (filtered, 100), -64, 4096) == 115);

    // A silent output is reported as such.
    CHECK (measureDelay (input, Signal (input.size(), 0.0f), -64, 4096) == -65);
}

TEST_CASE ("Bench: a sweep measures a known bell's response", "[bench]")
{
    const Peaking bell (1000.0, 6.0, 1.0);
    const auto sweep = logSweep (10.0, 0.48 * rate, 3.0, rate, -20.0);
    const auto frequencies = logFrequencies (20.0, 20000.0, 24);

    Signal input (sweep.signal);
    input.resize (input.size() + (size_t) rate, 0.0f);

    for (const auto latency : { 0, 100 })
    {
        const auto output = delayed (bell.process (input), latency);
        const auto points = sweepResponse (sweep, output, latency, frequencies);

        REQUIRE (points.size() == frequencies.size());

        auto worstDb = 0.0, worstDegrees = 0.0;

        for (const auto& point : points)
        {
            const auto expected = bell.responseAt (point.hz);
            worstDb = std::max (worstDb, std::abs (point.db - 20.0 * std::log10 (std::abs (expected))));
            worstDegrees = std::max (worstDegrees, std::abs (point.degrees - std::arg (expected) * 180.0 / pi));
        }

        INFO ("latency " << latency << ": worst " << worstDb << " dB, " << worstDegrees << " degrees");
        CHECK (worstDb < 0.05);
        CHECK (worstDegrees < 1.0);
    }
}

TEST_CASE ("Bench: THD matches a curve's Fourier series", "[bench]")
{
    const auto length = analysisLength (rate);
    const auto hz = binCentred (1000.0, rate);
    const auto amplitude = 0.5;

    for (const auto drive : { 1.0, 3.0 })
    {
        auto signal = sine (length, hz, rate, 20.0 * std::log10 (amplitude));

        for (auto& x : signal)
            x = (float) tanhCurve (x, drive);

        const auto measured = distortion (signal, hz, rate);
        const auto expected = seriesThdDb (tanhCurve, drive, amplitude);

        INFO ("drive " << drive << ": measured " << measured.thdDb << " dB, series " << expected << " dB");
        CHECK (std::abs (measured.thdDb - expected) < 0.1);
        CHECK (measured.thdnDb >= measured.thdDb - 0.01);
    }

    const auto clean = distortion (sine (length, hz, rate, -6.0), hz, rate);
    INFO ("clean sine: " << clean.thdDb << " dB");
    CHECK (clean.thdDb < -140.0);
    CHECK (clean.fundamentalDbfs == Catch::Approx (-6.0).margin (0.01));
}

TEST_CASE ("Bench: aliasing finds a folded harmonic at its level", "[bench]")
{
    const auto length = analysisLength (rate);
    const auto hz = binCentred (15000.0, rate);
    const auto amplitude = 0.5, c = 0.1;

    REQUIRE (hz == Catch::Approx (15000.0));

    auto signal = sine (length, hz, rate, 20.0 * std::log10 (amplitude));

    for (auto& x : signal)
        x = (float) (x + c * x * x * x);

    // x + c x³: the third harmonic, at 45 kHz, folds to 48 - 45 = 3 kHz.
    const auto third = c * amplitude * amplitude * amplitude / 4.0;
    const auto fundamental = amplitude + 3.0 * third;
    const auto expected = 20.0 * std::log10 (third / fundamental);

    const auto result = aliasing (signal, hz, rate);

    INFO ("worst at " << result.worstHz << " Hz, " << result.worstRelativeDb << " dB against " << expected);
    CHECK (result.count == 1);
    CHECK (std::abs (result.worstHz - 3000.0) < 1.0);
    CHECK (std::abs (result.worstRelativeDb - expected) < 0.5);
    CHECK (std::abs (result.worstDbfs - 20.0 * std::log10 (third)) < 0.5);

    const auto clean = aliasing (sine (length, hz, rate, 0.0), hz, rate);
    CHECK (clean.count == 0);
}

TEST_CASE ("Bench: null test", "[bench]")
{
    const auto input = whiteNoise ((int) rate, -20.0, 3);

    const auto same = nullTest (input, input, 0, 0);
    CHECK (same.rmsDbfs == floorDb);
    CHECK (same.relativeDb == floorDb);

    CHECK (nullTest (input, delayed (input, 37), 37, 0).rmsDbfs == floorDb);

    Signal quieter (input);
    const auto gain = std::pow (10.0, -0.1 / 20.0);

    for (auto& x : quieter)
        x = (float) (x * gain);

    const auto result = nullTest (input, quieter, 0, 0);
    INFO ("0.1 dB difference: " << result.relativeDb << " dB");
    CHECK (std::abs (result.relativeDb - 20.0 * std::log10 (1.0 - gain)) < 0.1);
    CHECK (std::abs (result.relativeDb - -38.8) < 0.1);
}

TEST_CASE ("Bench: clicks counts steps, not smooth ramps", "[bench]")
{
    // A cosine, so every step lands on a peak. Steps every 50 ms from 125 ms: 18 in a second.
    const auto length = (int) rate;
    const auto period = (int) (0.05 * rate), first = (int) (0.125 * rate), ramp = (int) (0.020 * rate);
    const auto base = sine (length, 1000.0, rate, -12.0, pi / 2.0);

    const auto render = [&] (bool ramped)
    {
        Signal output (base);

        for (int n = 0; n < length; ++n)
        {
            auto gain = 1.0;

            if (n >= first)
            {
                const auto step = (n - first) / period;
                const auto into = (n - first) % period;
                const auto from = step % 2 == 0 ? 1.0 : 0.5, to = step % 2 == 0 ? 0.5 : 1.0;
                gain = ramped && into < ramp ? from + (to - from) * (0.5 - 0.5 * std::cos (pi * into / ramp)) : to;
            }

            output[(size_t) n] = (float) (base[(size_t) n] * gain);
        }

        return output;
    };

    const auto steps = clicks (render (false), 1000.0, rate, -60.0);
    const auto ramps = clicks (render (true), 1000.0, rate, -60.0);
    const auto steady = clicks (base, 1000.0, rate, -60.0);

    INFO ("steps: " << steps.count << " at " << steps.peakDbfs << " dBFS; ramps: " << ramps.count << " at " << ramps.peakDbfs
                    << " dBFS; steady: " << steady.peakDbfs << " dBFS");
    CHECK (steps.count == 18);
    CHECK (ramps.count == 0);
    CHECK (steady.count == 0);
}

TEST_CASE ("Bench: test files", "[bench]")
{
    TestSpec spec;

    CHECK (parseTestFile ("{ not json", spec).failed());
    CHECK (parseTestFile ("[]", spec).failed());
    CHECK (parseTestFile (R"({ "plugins": [], "tests": { "cpu": {} } })", spec).failed());
    CHECK (parseTestFile (R"({ "plugins": [ { "label": "x" } ], "tests": { "cpu": {} } })", spec).failed());
    CHECK (parseTestFile (R"({ "plugins": [ { "path": "a.vst3" } ], "tests": { "speed": {} } })", spec).failed());
    CHECK (parseTestFile (R"({ "plugins": [ { "path": "a.vst3" } ], "tests": { "clicks": { "param": "x" } } })", spec).failed());
    CHECK (parseTestFile (R"({ "plugins": [ { "path": "a.vst3" } ], "blockSizes": [ 0 ], "tests": { "cpu": {} } })", spec).failed());
    CHECK (parseTestFile (R"({ "plugins": [ { "path": "a.vst3", "disabled": true } ], "tests": { "cpu": {} } })", spec).failed());

    const auto good = parseTestFile (R"({
        "name": "Example",
        "sampleRate": 44100,
        "blockSizes": [ 64, 256 ],
        "plugins": [
            { "label": "Ours", "path": "%FANCY_VST3%/Fancy Comp.vst3", "params": { "Ratio": "4" } },
            { "label": "Paid", "path": "C:/x.vst3", "disabled": true },
            { "path": "C:/Some Plugin.vst3", "todo": "check the names" }
        ],
        "tests": { "cpu": { "seconds": 5 }, "latency": {}, "clicks": { "param": "Threshold", "from": "-40", "to": "0" } }
    })", spec);

    INFO (good.getErrorMessage());
    REQUIRE (good.wasOk());
    CHECK (spec.name == "Example");
    CHECK (spec.sampleRate == 44100.0);
    CHECK ((spec.blockSizes == std::vector<int> { 64, 256 }));
    REQUIRE (spec.plugins.size() == 2);
    CHECK (spec.plugins[1].label == "Some Plugin");
    CHECK (spec.plugins[1].todo == "check the names");
    CHECK (spec.has ("cpu"));
    CHECK (! spec.has ("thd"));
    CHECK ((double) spec.options ("cpu")["seconds"] == 5.0);

    // FANCY_VST3 defaults to the install folder.
    if (juce::SystemStats::getEnvironmentVariable ("FANCY_VST3", {}).isEmpty())
        CHECK (expandPath ("%FANCY_VST3%/Fancy Comp.vst3")
               == juce::SystemStats::getEnvironmentVariable ("USERPROFILE", {}) + "\\VST3-Dev\\Fancy Comp.vst3");

    CHECK (expandPath ("%NO_SUCH_VARIABLE_FOR_BENCH%/x") == "%NO_SUCH_VARIABLE_FOR_BENCH%\\x");
}

TEST_CASE ("Bench: display numbers", "[bench]")
{
    double value = 0.0;

    CHECK ((parseDisplayNumber ("4.0:1", value) && value == 4.0));
    CHECK ((parseDisplayNumber ("2.5k", value) && value == 2500.0));
    CHECK ((parseDisplayNumber ("2.50 kHz", value) && value == 2500.0));
    CHECK ((parseDisplayNumber ("-20.0 dB", value) && value == -20.0));
    CHECK ((parseDisplayNumber (juce::String::fromUTF8 ("\xe2\x88\x92" "12 dB"), value) && value == -12.0));
    CHECK ((parseDisplayNumber ("-inf", value) && std::isinf (value) && value < 0.0));
    CHECK ((parseDisplayNumber ("6.0 dB knee", value) && value == 6.0));
    CHECK (! parseDisplayNumber ("Off", value));
}

namespace
{

// A processor whose frequency parameter shows "kHz" but can't parse text, so the bench has to
// search its range; and a choice.
class FakeProcessor final : public juce::AudioProcessor
{
public:
    FakeProcessor()
    {
        addParameter (new juce::AudioParameterFloat (
            juce::ParameterID { "freq", 1 }, "Frequency", juce::NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.2f), 1000.0f,
            juce::AudioParameterFloatAttributes {}
                .withStringFromValueFunction ([] (float hz, int)
                                              { return hz < 1000.0f ? juce::String (hz, 0) + " Hz" : juce::String (hz / 1000.0f, 2) + " kHz"; })
                .withValueFromStringFunction ([] (const juce::String&) { return 20.0f; })));

        addParameter (new juce::AudioParameterChoice (juce::ParameterID { "mode", 1 }, "Mode", juce::StringArray { "Clean", "Warm", "Hot" }, 0));
    }

    const juce::String getName() const override { return "Fake"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
};

} // namespace

TEST_CASE ("Bench: parameters by name, index and normalised value", "[bench]")
{
    FakeProcessor fake;
    juce::String error;

    CHECK (findParameter (fake, "frequency", error) == fake.getParameters()[0]);
    CHECK (findParameter (fake, "#1", error) == fake.getParameters()[1]);
    CHECK (findParameter (fake, "#2", error) == nullptr);
    CHECK (findParameter (fake, "Frequncy", error) == nullptr);
    CHECK (error.contains ("Frequency"));

    ParamSetting setting;

    // The plugin's own parser fails, so the range is searched.
    REQUIRE (resolveValue (*fake.getParameters()[0], "2500", setting).wasOk());
    CHECK (setting.shown == "2.50 kHz");
    CHECK (setting.note == "found by search");

    REQUIRE (resolveValue (*fake.getParameters()[0], "2.5k", setting).wasOk());
    CHECK (setting.shown == "2.50 kHz");

    REQUIRE (resolveValue (*fake.getParameters()[0], "=0", setting).wasOk());
    CHECK (setting.shown == "20 Hz");

    REQUIRE (resolveValue (*fake.getParameters()[1], "warm", setting).wasOk());
    CHECK (setting.shown == "Warm");
    CHECK (resolveValue (*fake.getParameters()[1], "Boiling", setting).failed());

    std::vector<ParamSetting> settings;
    auto params = juce::JSON::parse (R"({ "Mode": "Hot", "#0": "500" })");
    REQUIRE (applyParams (fake, params, settings).wasOk());
    CHECK (settings.size() == 2);
    CHECK (fake.getParameters()[1]->getText (fake.getParameters()[1]->getValue(), 64) == "Hot");
    CHECK (settings[1].shown == "500 Hz");

    CHECK (applyParams (fake, juce::JSON::parse (R"({ "Nope": "1" })"), settings).failed());

   #if FANCY_HAS_COMP
    // Our own text parser takes the value directly.
    CompProcessor comp;
    REQUIRE (resolveValue (*findParameter (comp, "Ratio", error), "4", setting).wasOk());
    CHECK (setting.note.isEmpty());
    REQUIRE (resolveValue (*findParameter (comp, "Threshold", error), "-20", setting).wasOk());
    CHECK (setting.note.isEmpty());
   #endif
}

#if FANCY_HAS_EQ
// The response the CI smoke check compares Fancy EQ's measured bell with: the EQ's own design.
TEST_CASE ("Bench: Fancy EQ bell reference", "[.measure]")
{
    eq::BandSpec spec;
    spec.type = eq::BandType::band;
    spec.frequencyHz = 1000.0;
    spec.gainDb = 6.0;
    spec.q = 1.0;
    spec.order = 2;
    spec.on = true;

    const auto filter = eq::design (spec, rate);
    juce::String line ("bench eq-bell reference:");

    for (const auto hz : logFrequencies (20.0, 20000.0, 24))
        line << " " << juce::String (hz, 4) << ":" << juce::String (20.0 * std::log10 (std::abs (filter.responseAt (2.0 * pi * hz / rate))), 5);

    std::printf ("%s\n", line.toRawUTF8());
}
#endif
