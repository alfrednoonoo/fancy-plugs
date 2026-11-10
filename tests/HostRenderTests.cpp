// Phase 2.1 item H.1: an offline render, with random blocks, sounds exactly like playback.
//
// Each plugin, in each mode, renders the same 30 s of noise and sine bursts with automation
// three ways: real time in 128-sample blocks, offline in 128-sample blocks, and offline in
// random blocks of 0 to 8192 samples. Every automation point starts a new block in all three,
// so the parameters change at the same sample.
//
// - Offline with random blocks against offline with 128: the block size must change nothing.
// - Real time against offline: a plugin with no worker thread must be identical. One with a
//   worker (a design, a build or an impulse) is compared outside a window after each change,
//   with the real-time render waiting for the worker before each block, as a machine that
//   keeps up would. Inside the window the output only has to be finite. Verb's real-time
//   render is paced at real time, because its convolver plays silence in a stage whose
//   background job is late, which only happens when rendering faster than real time.
//
// A rebuilt engine lands a block later in real time than offline. For FIR's nonlinear,
// frame-based modes that shifts the new engine's frame grid, so after an FFT-size change the
// two renders keep differing. That difference is printed, and not required to be zero.

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <fancy/plugin/FancyProcessor.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <random>
#include <vector>

#if FANCY_HAS_KICKER
 #include "KickerProcessor.h"
#endif

#if FANCY_HAS_EXCITER
 #include "ExciterProcessor.h"
#endif

#if FANCY_HAS_COMP
 #include "CompProcessor.h"
#endif

#if FANCY_HAS_EQ
 #include "EqProcessor.h"
#endif

#if FANCY_HAS_VERB
 #include "VerbProcessor.h"
#endif

#if FANCY_HAS_DELAY
 #include "DelayProcessor.h"
#endif

#if FANCY_HAS_MULTICOMP
 #include "MultiCompProcessor.h"
#endif

#if FANCY_HAS_FIR
 #include "FirProcessor.h"
#endif

#if FANCY_HAS_GATE
 #include "GateProcessor.h"
#endif

#if FANCY_HAS_LIMIT
 #include "LimitProcessor.h"
#endif

namespace
{

constexpr double sampleRate = 48000.0;
constexpr int seconds = 30;
constexpr int length = (int) sampleRate * seconds;
constexpr int maxBlock = 8192;
constexpr int changeWindow = (int) sampleRate; // after each change, for plugins with a worker

using Settings = std::vector<std::pair<const char*, float>>;

struct Point
{
    int sample;
    const char* id;
    float value;
};

// A change that isn't a parameter, such as swapping the impulse, at a sample.
struct Event
{
    int sample;
    std::function<void (fancy::FancyProcessor&)> run;
};

struct Plan
{
    Settings settings;
    std::vector<Point> automation;
    std::vector<Event> events;
    bool hasWorker = false;
    int samples = length;
    int window = changeWindow;
    bool paceRealTime = false;
    bool eventWindowToEnd = false;
};

void set (fancy::FancyProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.getApvts().getParameter (id);
    REQUIRE (parameter != nullptr);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

// Pink-ish noise at about -20 dBFS, with a sine burst every half second.
juce::AudioBuffer<float> testSignal()
{
    juce::AudioBuffer<float> signal (2, length);
    std::mt19937 random (1234);
    std::normal_distribution<float> white (0.0f, 1.0f);

    double b0 = 0.0, b1 = 0.0, b2 = 0.0;

    for (int i = 0; i < length; ++i)
    {
        const auto w = (double) white (random);
        b0 = 0.99765 * b0 + w * 0.0990460;
        b1 = 0.96300 * b1 + w * 0.2965164;
        b2 = 0.57000 * b2 + w * 1.0526913;
        const auto pink = (b0 + b1 + b2 + w * 0.1848) * 0.02;

        const auto inBurst = (i % 24000) < 6000;
        const auto burst = inBurst ? 0.25 * std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate) : 0.0;

        signal.setSample (0, i, (float) (pink + burst));
        signal.setSample (1, i, (float) (pink * 0.9 - burst));
    }

    return signal;
}

const juce::AudioBuffer<float>& input()
{
    static const auto signal = testSignal();
    return signal;
}

enum class Run { realTime, offline, offlineRandom };

template <typename Processor>
juce::AudioBuffer<float> render (const Plan& plan, Run run, const std::function<void (Processor&)>& setup)
{
    Processor processor;
    setup (processor);

    for (const auto& [id, value] : plan.settings)
        set (processor, id, value);

    const auto offline = run != Run::realTime;
    processor.setNonRealtime (offline);
    processor.prepareToPlay (sampleRate, maxBlock);

    if (plan.hasWorker && ! offline)
        REQUIRE (processor.waitForBackgroundWork (60000));

    juce::AudioBuffer<float> output (2, plan.samples);

    for (int channel = 0; channel < 2; ++channel)
        output.copyFrom (channel, 0, input(), channel, 0, plan.samples);

    juce::MidiBuffer midi;
    std::mt19937 random (99);
    const auto started = juce::Time::getMillisecondCounterHiRes();

    // Blocks of 0 to 8192 samples: some empty, many tiny, some large.
    const auto randomBlock = [&random]
    {
        const auto kind = random() % 10;

        if (kind == 0) return 0;
        if (kind < 4) return (int) (1 + random() % 16);
        if (kind < 8) return (int) (1 + random() % 1024);
        return (int) (1 + random() % maxBlock);
    };

    std::vector<int> changes;

    for (const auto& point : plan.automation)
        changes.push_back (point.sample);

    for (const auto& event : plan.events)
        changes.push_back (event.sample);

    std::sort (changes.begin(), changes.end());

    for (int position = 0; position < plan.samples;)
    {
        for (const auto& point : plan.automation)
            if (point.sample == position)
                set (processor, point.id, point.value);

        for (const auto& event : plan.events)
            if (event.sample == position)
                event.run (processor);

        auto next = plan.samples;

        for (const auto change : changes)
        {
            if (change > position)
            {
                next = std::min (change, plan.samples);
                break;
            }
        }

        const auto wanted = run == Run::offlineRandom ? randomBlock() : 128;
        const auto size = std::min (wanted, next - position);

        // A fast machine: the worker has caught up before the block plays.
        if (plan.hasWorker && ! offline)
            REQUIRE (processor.waitForBackgroundWork (60000));

        juce::AudioBuffer<float> block (output.getArrayOfWritePointers(), 2, position, size);
        midi.clear();
        processor.processBlock (block, midi);
        position += size;

        // Real time, for a plugin whose background work has to keep up with playback.
        if (plan.paceRealTime && ! offline)
            while (juce::Time::getMillisecondCounterHiRes() < started + 1000.0 * position / sampleRate)
                juce::Thread::sleep (1);
    }

    processor.releaseResources();
    return output;
}

struct Difference
{
    double worstDb = -400.0;
    bool identical = true;
    bool finite = true;
};

// The largest difference, in dB below the reference's peak. Samples inside a window after a
// change are only checked for being finite.
using Windows = std::vector<std::pair<int, int>>;

Difference compare (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, const Windows& windows, int from = 0)
{
    Difference result;
    auto peak = 1.0e-9;
    const auto length = b.getNumSamples();

    for (int channel = 0; channel < 2; ++channel)
        peak = std::max (peak, (double) b.getMagnitude (channel, 0, length));

    auto worst = 0.0;

    for (int channel = 0; channel < 2; ++channel)
    {
        const auto* x = a.getReadPointer (channel);
        const auto* y = b.getReadPointer (channel);

        for (int i = from; i < length; ++i)
        {
            if (! std::isfinite (x[i]) || ! std::isfinite (y[i]))
            {
                result.finite = false;
                continue;
            }

            const auto inWindow = std::any_of (windows.begin(), windows.end(), [i] (const auto& window) { return i >= window.first && i < window.second; });

            if (inWindow)
                continue;

            if (x[i] != y[i])
            {
                result.identical = false;
                worst = std::max (worst, (double) std::abs (x[i] - y[i]));
            }
        }
    }

    result.worstDb = worst > 0.0 ? 20.0 * std::log10 (worst / peak) : -400.0;
    return result;
}

template <typename Processor>
void checkRenders (const char* name, const Plan& plan, const std::function<void (Processor&)>& setup = [] (Processor&) {})
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto realTime = render<Processor> (plan, Run::realTime, setup);
    const auto offline = render<Processor> (plan, Run::offline, setup);
    const auto offlineRandom = render<Processor> (plan, Run::offlineRandom, setup);

    Windows windows;

    if (plan.hasWorker)
    {
        for (const auto& point : plan.automation)
            windows.emplace_back (point.sample, point.sample + plan.window);

        for (const auto& event : plan.events)
            windows.emplace_back (event.sample, plan.eventWindowToEnd ? plan.samples : event.sample + plan.window);

        windows.emplace_back (0, plan.window);
    }

    const auto describe = [] (const Difference& difference)
    {
        return difference.identical ? juce::String ("bit-identical") : juce::String (difference.worstDb, 1) + " dB";
    };

    const auto blocks = compare (offlineRandom, offline, {});
    const auto modes = compare (realTime, offline, windows);

    std::cout << "offline render: " << name << ": random blocks " << describe (blocks) << ", real time " << describe (modes);

    if (plan.hasWorker)
        std::cout << " outside " << juce::String ((double) plan.window / sampleRate, 1) << " s after each change";

    if (plan.eventWindowToEnd && ! plan.events.empty())
        std::cout << "; after the change, real time differs by " << describe (compare (realTime, offline, {}, plan.events.front().sample));

    std::cout << std::endl;

    INFO (name);
    CHECK (blocks.finite);
    CHECK (modes.finite);
    CHECK (blocks.worstDb <= -120.0);
    CHECK (modes.worstDb <= -120.0);
}

// Automation that alternates between two values, every 5 s from 2.5 s.
std::vector<Point> alternate (const char* id, float a, float b, int offset = 0)
{
    std::vector<Point> points;

    for (int i = 0; i < 5; ++i)
        points.push_back ({ (int) (sampleRate * (2.5 + 5.0 * i)) + offset, id, i % 2 == 0 ? a : b });

    return points;
}

std::vector<Point> join (std::initializer_list<std::vector<Point>> lists)
{
    std::vector<Point> all;

    for (const auto& list : lists)
        all.insert (all.end(), list.begin(), list.end());

    return all;
}

} // namespace

#if FANCY_HAS_KICKER
TEST_CASE ("Kicker: an offline render matches playback", "[host][offline]")
{
    checkRenders<KickerProcessor> ("Kicker", { {}, join ({ alternate ("freq", 45.0f, 90.0f), alternate ("wet", -20.0f, -6.0f, 777) }), {} });
}
#endif

#if FANCY_HAS_EXCITER
TEST_CASE ("Exciter: an offline render matches playback", "[host][offline]")
{
    const auto oversampling = GENERATE (0.0f, 1.0f, 2.0f);
    checkRenders<ExciterProcessor> (("Exciter, oversampling " + juce::String (oversampling)).toRawUTF8(),
                                    { { { "oversampling", oversampling } },
                                      join ({ alternate ("freq", 2000.0f, 6000.0f), alternate ("harmonics", 20.0f, 80.0f, 501) }), {} });
}
#endif

#if FANCY_HAS_COMP
TEST_CASE ("Comp: an offline render matches playback", "[host][offline]")
{
    const auto oversampling = GENERATE (0.0f, 1.0f, 2.0f);
    checkRenders<CompProcessor> (("Comp, oversampling " + juce::String (oversampling)).toRawUTF8(),
                                 { { { "oversampling", oversampling } },
                                   join ({ alternate ("threshold", -30.0f, -10.0f), alternate ("ratio", 2.0f, 8.0f, 333), alternate ("attack", 1.0f, 30.0f, 999) }), {} });
}
#endif

#if FANCY_HAS_EQ
TEST_CASE ("EQ: an offline render matches playback", "[host][offline]")
{
    const auto linear = GENERATE (0.0f, 1.0f);
    checkRenders<EqProcessor> (linear > 0.5f ? "EQ, linear phase" : "EQ, minimum phase",
                               { { { "phase", linear } },
                                 join ({ alternate ("b2gain", -6.0f, 6.0f), alternate ("b2freq", 300.0f, 1500.0f, 1234) }), {}, linear > 0.5f });
}

// Phase 2.1 D: dynamic bands, Above and Below, with their thresholds and gains automated.
TEST_CASE ("EQ: an offline render with dynamic bands matches playback", "[host][offline]")
{
    checkRenders<EqProcessor> ("EQ, dynamic bands",
                               { { { "b2dyn", 1.0f }, { "b2thresh", -40.0f }, { "b2range", -9.0f }, { "b2attack", 2.0f },
                                   { "b3dyn", 1.0f }, { "b3dynMode", 1.0f }, { "b3thresh", -30.0f }, { "b3range", 6.0f } },
                                 join ({ alternate ("b2thresh", -45.0f, -25.0f), alternate ("b2gain", -3.0f, 3.0f, 1234),
                                         alternate ("b3release", 20.0f, 300.0f, 777) }), {} });
}
#endif

#if FANCY_HAS_DELAY
TEST_CASE ("Delay: an offline render matches playback", "[host][offline]")
{
    const auto noteMode = GENERATE (0.0f, 1.0f);
    checkRenders<DelayProcessor> (noteMode > 0.5f ? "Delay, note taps" : "Delay, ms taps",
                                  { { { "t1mode", noteMode }, { "t1fb", 40.0f } },
                                    join ({ alternate ("t1fb", 20.0f, 70.0f), alternate ("t1lp", 3000.0f, 12000.0f, 555), alternate ("t1ms", 300.0f, 450.0f, 2048) }), {} });
}
#endif

#if FANCY_HAS_MULTICOMP
// Phase 2.1 U: upward bands, with Depth and Time automated.
TEST_CASE ("MultiComp: an offline render with upward compression matches playback", "[host][offline]")
{
    checkRenders<MultiCompProcessor> ("MultiComp, upward",
                                      { { { "b1up", 1.0f }, { "b2up", 1.0f }, { "b2uthr", -30.0f }, { "b2uratio", 4.0f }, { "b1thr", -20.0f } },
                                        join ({ alternate ("depth", 40.0f, 100.0f), alternate ("time", 50.0f, 300.0f, 777),
                                                alternate ("b2lift", 6.0f, 18.0f, 1500) }), {} });
}

TEST_CASE ("MultiComp: an offline render matches playback", "[host][offline]")
{
    const auto crossover = GENERATE (0.0f, 1.0f, 2.0f);
    checkRenders<MultiCompProcessor> (("MultiComp, crossover " + juce::String (crossover)).toRawUTF8(),
                                      { { { "crossover", crossover } },
                                        join ({ alternate ("b1thr", -30.0f, -10.0f), alternate ("b1top", 150.0f, 300.0f, 4321) }), {}, crossover > 1.5f });
}
#endif

#if FANCY_HAS_FIR
// Phase 2.1 A: Subtract, tracking the noise, with Adapt speed automated.
TEST_CASE ("FIR: an offline render in Adaptive matches playback", "[host][offline]")
{
    Plan plan { { { "mode", 3.0f }, { "source", 1.0f } }, join ({ alternate ("adaptSpeed", 1.0f, 4.0f), alternate ("reduction", -24.0f, -12.0f, 777) }), {}, true };
    checkRenders<FirProcessor> ("FIR, Subtract adaptive", plan);
}

TEST_CASE ("FIR: an offline render matches playback, through an FFT-size change", "[host][offline]")
{
    const auto mode = GENERATE (0.0f, 1.0f, 2.0f, 3.0f, 4.0f);
    const std::vector<Event> sizeChange { { length / 2, [] (fancy::FancyProcessor& processor) { set (processor, "fft", 3.0f); } } };

    // EQ and Subtract are linear, so the new engine's frame grid doesn't change their output.
    const auto frameGridMatters = mode == 1.0f || mode == 2.0f || mode == 4.0f;

    Plan plan { { { "mode", mode } }, join ({ alternate ("output", -6.0f, 0.0f), alternate ("gain", -3.0f, 3.0f, 777) }), sizeChange, true };
    plan.eventWindowToEnd = frameGridMatters;

    checkRenders<FirProcessor> (("FIR, mode " + juce::String (mode)).toRawUTF8(), plan);
}
#endif

#if FANCY_HAS_GATE
TEST_CASE ("Gate: an offline render matches playback", "[host][offline]")
{
    const auto lookahead = GENERATE (0.0f, 1.0f);
    checkRenders<GateProcessor> (lookahead > 0.5f ? "Gate, lookahead" : "Gate",
                                 { { { "lookahead", lookahead } },
                                   join ({ alternate ("threshold", -40.0f, -20.0f), alternate ("release", 50.0f, 200.0f, 888) }), {} });
}
#endif

#if FANCY_HAS_LIMIT
TEST_CASE ("Limit: an offline render matches playback", "[host][offline]")
{
    // Dither on too: its noise is seeded, so it repeats.
    const auto dither = GENERATE (0.0f, 2.0f);
    checkRenders<LimitProcessor> (dither > 0.5f ? "Limit, dither" : "Limit",
                                  { { { "dither", dither }, { "threshold", -12.0f } },
                                    join ({ alternate ("threshold", -12.0f, -6.0f), alternate ("release", 30.0f, 300.0f, 888) }), {} });
}
#endif

#if FANCY_HAS_VERB
namespace
{
juce::File writeImpulse (const char* name, int seed, double decaySeconds)
{
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("fancy-host-tests");
    folder.createDirectory();
    const auto file = folder.getChildFile (name);

    const auto samples = (int) (decaySeconds * sampleRate);
    juce::AudioBuffer<float> audio (2, samples);
    std::mt19937 random ((unsigned) seed);
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < samples; ++i)
            audio.setSample (channel, i, 0.5f * noise (random) * std::exp (-5.0f * (float) i / (float) samples));

    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);

    juce::WavAudioFormat wav;
    const auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions {}
                                                         .withSampleRate (sampleRate)
                                                         .withNumChannels (2)
                                                         .withBitsPerSample (24));
    REQUIRE (writer != nullptr);
    REQUIRE (writer->writeFromAudioSampleBuffer (audio, 0, samples));
    return file;
}
} // namespace

TEST_CASE ("Verb: an offline render matches playback, through an impulse swap", "[host][offline]")
{
    const auto lowCpu = GENERATE (0.0f, 1.0f);

    const auto first = writeImpulse ("first.wav", 1, 0.8);
    const auto second = writeImpulse ("second.wav", 2, 1.5);

    const std::vector<Event> swap { { (int) sampleRate * 6, [second] (fancy::FancyProcessor& processor)
                                      {
                                          auto& verb = dynamic_cast<VerbProcessor&> (processor);
                                          const auto uid = (int) verb.getChain().getChild (0).getProperty (verb::ids::uid);
                                          verb.loadFile (uid, second);
                                      } } };

    // 12 s, paced at real time, with a window covering the old impulse's tail after the swap.
    Plan plan { { { "latency", lowCpu }, { "wet", -6.0f } },
                join ({ alternate ("wet", -12.0f, -3.0f), alternate ("predelay", 0.0f, 40.0f, 1500) }), swap, true };
    plan.samples = (int) sampleRate * 12;
    plan.window = (int) (sampleRate * 2.5);
    plan.paceRealTime = true;

    checkRenders<VerbProcessor> (lowCpu > 0.5f ? "Verb, low CPU" : "Verb, zero latency", plan,
                                 [first] (VerbProcessor& verb)
                                 {
                                     const auto uid = verb.addModule (verb::ModuleType::file, 0);
                                     REQUIRE (verb.loadFile (uid, first));
                                 });
}
#endif
