// Phase 2.1 item H: what hosts do to a plugin, tested in-process. H.1 is in HostRenderTests.cpp;
// the same scenarios against the built .vst3 files are in tools/hosttest.

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <fancy/ui/ShellEditor.h>

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <new>
#include <random>
#include <typeinfo>
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

// Allocation counting ---------------------------------------------------------------------
// The plugins' code is compiled into this program, so replacing operator new here sees every
// allocation they make. It only counts on a thread that asks it to, around processBlock.

namespace
{
thread_local bool countingAllocations = false;
std::atomic<int> allocationsWhileCounting { 0 };

void* allocate (std::size_t size)
{
    if (countingAllocations)
        allocationsWhileCounting.fetch_add (1, std::memory_order_relaxed);

    if (auto* memory = std::malloc (size == 0 ? 1 : size))
        return memory;

    throw std::bad_alloc();
}
} // namespace

void* operator new (std::size_t size) { return allocate (size); }
void* operator new[] (std::size_t size) { return allocate (size); }
void* operator new (std::size_t size, const std::nothrow_t&) noexcept { try { return allocate (size); } catch (...) { return nullptr; } }
void* operator new[] (std::size_t size, const std::nothrow_t&) noexcept { try { return allocate (size); } catch (...) { return nullptr; } }
void operator delete (void* memory) noexcept { std::free (memory); }
void operator delete[] (void* memory) noexcept { std::free (memory); }
void operator delete (void* memory, std::size_t) noexcept { std::free (memory); }
void operator delete[] (void* memory, std::size_t) noexcept { std::free (memory); }
void operator delete (void* memory, const std::nothrow_t&) noexcept { std::free (memory); }
void operator delete[] (void* memory, const std::nothrow_t&) noexcept { std::free (memory); }

namespace
{

constexpr double rate = 48000.0;
constexpr int blockSize = 512;

void set (fancy::FancyProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.getApvts().getParameter (id);
    REQUIRE (parameter != nullptr);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

// Noise with a sine on top, at about -20 dBFS, so detectors, filters and gates all work.
void fill (juce::AudioBuffer<float>& buffer, int firstChannel, int numChannels, std::mt19937& random, double& phase, float level = 0.1f)
{
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto sine = (float) std::sin (phase);
        phase += 2.0 * juce::MathConstants<double>::pi * 220.0 / rate;

        for (int channel = firstChannel; channel < firstChannel + numChannels; ++channel)
            buffer.setSample (channel, i, level * (0.5f * noise (random) + sine));
    }
}

bool allFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getSample (channel, i)))
                return false;

    return true;
}

// Runs seconds of signal through the processor as it's laid out now, sidechain silent unless asked.
struct Run
{
    bool finite = true;
    float outputPeak = 0.0f;
};

Run process (fancy::FancyProcessor& processor, double seconds, bool feedSidechain = false)
{
    Run result;
    std::mt19937 random (5);
    double phase = 0.0, sidechainPhase = 0.0;
    juce::MidiBuffer midi;

    const auto mainChannels = processor.getMainBusNumInputChannels();
    const auto totalChannels = juce::jmax (processor.getTotalNumInputChannels(), processor.getTotalNumOutputChannels());

    juce::AudioBuffer<float> buffer (totalChannels, blockSize);

    for (int done = 0; done < (int) (seconds * rate); done += blockSize)
    {
        buffer.clear();
        fill (buffer, 0, mainChannels, random, phase);

        if (feedSidechain && totalChannels > mainChannels)
            fill (buffer, mainChannels, totalChannels - mainChannels, random, sidechainPhase, 0.3f);

        midi.clear();
        processor.processBlock (buffer, midi);

        result.finite = result.finite && allFinite (buffer);

        for (int channel = 0; channel < processor.getMainBusNumOutputChannels(); ++channel)
            result.outputPeak = juce::jmax (result.outputPeak, buffer.getMagnitude (channel, 0, blockSize));
    }

    return result;
}

bool setLayout (fancy::FancyProcessor& processor, juce::AudioChannelSet main, juce::AudioChannelSet sidechain)
{
    auto layout = processor.getBusesLayout();
    layout.inputBuses.getReference (0) = main;
    layout.outputBuses.getReference (0) = main;

    if (layout.inputBuses.size() > 1)
        layout.inputBuses.getReference (1) = sidechain;

    return processor.setBusesLayout (layout);
}

// Every processor, for the checks that apply to all nine.
template <typename Callback>
void forEachPlugin (Callback&& callback)
{
   #if FANCY_HAS_KICKER
    { KickerProcessor p; callback (p); }
   #endif
   #if FANCY_HAS_EXCITER
    { ExciterProcessor p; callback (p); }
   #endif
   #if FANCY_HAS_COMP
    { CompProcessor p; callback (p); }
   #endif
   #if FANCY_HAS_EQ
    { EqProcessor p; callback (p); }
   #endif
   #if FANCY_HAS_VERB
    { VerbProcessor p; callback (p); }
   #endif
   #if FANCY_HAS_DELAY
    { DelayProcessor p; callback (p); }
   #endif
   #if FANCY_HAS_MULTICOMP
    { MultiCompProcessor p; callback (p); }
   #endif
   #if FANCY_HAS_FIR
    { FirProcessor p; callback (p); }
   #endif
   #if FANCY_HAS_GATE
    { GateProcessor p; callback (p); }
   #endif
}

} // namespace

// H.2 -------------------------------------------------------------------------------------

TEST_CASE ("Editors leave the keyboard to the host", "[host][keyboard]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    forEachPlugin ([] (fancy::FancyProcessor& processor)
    {
        INFO (processor.getName());

        auto* editor = processor.createEditorAndMakeActive();
        REQUIRE (editor != nullptr);
        editor->setVisible (true);

        // No control takes the focus when it's clicked, apart from fields being typed in.
        std::function<void (juce::Component&)> walk = [&walk] (juce::Component& component)
        {
            if (dynamic_cast<juce::TextEditor*> (&component) == nullptr)
            {
                INFO (typeid (component).name());
                CHECK_FALSE (component.getWantsKeyboardFocus());
            }

            for (auto* child : component.getChildren())
                walk (*child);
        };

        walk (*editor);

        // With nothing focused, the keys reach the editor, and it passes every one on. Its own
        // shortcuts, Ctrl+S and undo's (W.2), are tested in WorkflowTests.cpp.
        for (const auto& key : { juce::KeyPress (juce::KeyPress::spaceKey), juce::KeyPress (juce::KeyPress::returnKey),
                                 juce::KeyPress (juce::KeyPress::escapeKey), juce::KeyPress ('a'), juce::KeyPress ('s'),
                                 juce::KeyPress ('z'), juce::KeyPress ('1'), juce::KeyPress (juce::KeyPress::leftKey),
                                 juce::KeyPress (juce::KeyPress::upKey), juce::KeyPress ('z', juce::ModifierKeys::altModifier, 0) })
        {
            INFO (key.getTextDescription());
            CHECK_FALSE (editor->keyPressed (key));
        }

        processor.editorBeingDeleted (editor);
        delete editor;
    });
}

// H.4 and H.9 -----------------------------------------------------------------------------

TEST_CASE ("Every plugin works on a mono main bus with no sidechain", "[host][buses]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    forEachPlugin ([] (fancy::FancyProcessor& processor)
    {
        INFO (processor.getName());
        REQUIRE (setLayout (processor, juce::AudioChannelSet::mono(), juce::AudioChannelSet::disabled()));

        processor.prepareToPlay (rate, blockSize);
        processor.waitForBackgroundWork (20000);
        const auto run = process (processor, 2.0);
        processor.releaseResources();

        CHECK (run.finite);
    });
}

namespace
{
// A sidechain plugin with its sidechain chosen, through each bus layout and back.
void checkSidechain (fancy::FancyProcessor& processor, const std::function<void (bool)>& chooseSidechain, bool usesWatch)
{
    INFO (processor.getName());
    const juce::AudioChannelSet layouts[] { juce::AudioChannelSet::disabled(), juce::AudioChannelSet::mono(),
                                            juce::AudioChannelSet::stereo(), juce::AudioChannelSet::disabled() };

    chooseSidechain (true);

    for (const auto& sidechain : layouts)
    {
        INFO ("sidechain " << sidechain.getDescription());

        // Hosts switch buses while the plugin is inactive, between two blocks of playback.
        processor.releaseResources();
        REQUIRE (setLayout (processor, juce::AudioChannelSet::stereo(), sidechain));
        processor.setRateAndBufferSizeDetails (rate, blockSize);
        processor.prepareToPlay (rate, blockSize);
        processor.sidechainWatch.reset();
        processor.waitForBackgroundWork (20000);

        // Nothing on the sidechain for 3 s: the warning comes on.
        const auto silent = process (processor, 3.0, false);
        CHECK (silent.finite);

        if (usesWatch)
            CHECK (processor.sidechainWatch.isMissing());

        // A signal arrives, where the bus exists: the warning goes.
        if (! sidechain.isDisabled())
        {
            const auto fed = process (processor, 0.5, true);
            CHECK (fed.finite);
            CHECK (fed.outputPeak > 0.0f);

            if (usesWatch)
                CHECK_FALSE (processor.sidechainWatch.isMissing());
        }
    }

    // With the main input as the source there's nothing to warn about.
    chooseSidechain (false);
    processor.sidechainWatch.reset();
    process (processor, 3.0, false);

    if (usesWatch)
        CHECK_FALSE (processor.sidechainWatch.isMissing());

    processor.releaseResources();
}
} // namespace

#if FANCY_HAS_COMP
TEST_CASE ("Comp: sidechain inactive, mono, stereo, and the warning", "[host][sidechain]")
{
    CompProcessor processor;
    checkSidechain (processor, [&] (bool on) { set (processor, "detector", on ? 3.0f : 0.0f); }, true);
}
#endif

#if FANCY_HAS_MULTICOMP
TEST_CASE ("MultiComp: sidechain inactive, mono, stereo, and the warning", "[host][sidechain]")
{
    MultiCompProcessor processor;
    checkSidechain (processor, [&] (bool on) { set (processor, "detector", on ? 3.0f : 0.0f); }, true);
}
#endif

#if FANCY_HAS_FIR
TEST_CASE ("FIR: sidechain inactive, mono, stereo, and the warning", "[host][sidechain]")
{
    FirProcessor processor;
    checkSidechain (processor, [&] (bool on) { set (processor, "mode", 2.0f); set (processor, "sidechain", on ? 1.0f : 0.0f); }, true);
}
#endif

#if FANCY_HAS_EQ
// Phase 2.1 D: a dynamic band with Sidechain ticked.
TEST_CASE ("EQ: sidechain inactive, mono, stereo, and the warning", "[host][sidechain]")
{
    EqProcessor processor;
    set (processor, "b2dyn", 1.0f);

    checkSidechain (processor, [&] (bool on) { set (processor, "b2dynSc", on ? 1.0f : 0.0f); }, true);
}
#endif

#if FANCY_HAS_GATE
TEST_CASE ("Gate: sidechain inactive, mono and stereo", "[host][sidechain]")
{
    GateProcessor processor;
    checkSidechain (processor, [&] (bool on) { set (processor, "detector", on ? 3.0f : 0.0f); }, false);
}

// H.5 -------------------------------------------------------------------------------------

TEST_CASE ("Gate: MIDI out that nobody reads changes nothing in the audio", "[host][midi]")
{
    const auto render = [] (bool midiOut)
    {
        GateProcessor processor;
        set (processor, "threshold", -30.0f);
        set (processor, "midi", midiOut ? 1.0f : 0.0f);
        processor.prepareToPlay (rate, blockSize);

        std::mt19937 random (5);
        double phase = 0.0;
        juce::AudioBuffer<float> buffer (2, blockSize), all (2, blockSize * 200);
        juce::MidiBuffer midi;

        for (int block = 0; block < 200; ++block)
        {
            fill (buffer, 0, 2, random, phase, (block / 20) % 2 == 0 ? 0.3f : 0.001f);
            midi.clear();
            processor.processBlock (buffer, midi);

            for (int channel = 0; channel < 2; ++channel)
                all.copyFrom (channel, block * blockSize, buffer, channel, 0, blockSize);
        }

        return all;
    };

    const auto with = render (true);
    const auto without = render (false);

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < with.getNumSamples(); ++i)
            REQUIRE (with.getSample (channel, i) == without.getSample (channel, i));
}
#endif

// H.6 -------------------------------------------------------------------------------------

namespace
{
struct LatencyCounter final : public juce::AudioProcessorListener
{
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}

    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& details) override
    {
        if (details.latencyChanged)
            ++notices;
    }

    std::atomic<int> notices { 0 };
};

// Switches a setting while playing, and back: one notice each way.
void checkLatencyNotices (fancy::FancyProcessor& processor, const char* id, float from, float to, const std::vector<std::pair<const char*, float>>& before = {})
{
    INFO (processor.getName() << ": " << id << " " << from << " -> " << to);

    for (const auto& [name, value] : before)
        set (processor, name, value);

    set (processor, id, from);
    processor.prepareToPlay (rate, blockSize);
    processor.waitForBackgroundWork (20000);
    process (processor, 0.5);

    LatencyCounter counter;
    processor.addListener (&counter);

    const auto settle = [&]
    {
        for (int i = 0; i < 20; ++i)
        {
            processor.waitForBackgroundWork (20000);
            process (processor, 0.1);
        }
    };

    const auto start = processor.getLatencySamples();

    set (processor, id, to);
    settle();
    const auto changed = processor.getLatencySamples();
    CHECK (changed != start);
    CHECK (counter.notices == 1);

    set (processor, id, from);
    settle();
    CHECK (processor.getLatencySamples() == start);
    CHECK (counter.notices == 2);

    processor.removeListener (&counter);
    processor.releaseResources();
}
} // namespace

TEST_CASE ("Each latency change reaches the host once", "[host][latency]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

   #if FANCY_HAS_EXCITER
    { ExciterProcessor p; checkLatencyNotices (p, "oversampling", 0.0f, 1.0f); }
   #endif
   #if FANCY_HAS_COMP
    { CompProcessor p; checkLatencyNotices (p, "oversampling", 0.0f, 1.0f); }
   #endif
   #if FANCY_HAS_EQ
    { EqProcessor p; checkLatencyNotices (p, "phase", 0.0f, 1.0f); }
    { EqProcessor p; checkLatencyNotices (p, "firLength", 2.0f, 1.0f, { { "phase", 1.0f } }); }
    { EqProcessor p; checkLatencyNotices (p, "oversampling", 0.0f, 1.0f); }
   #endif
   #if FANCY_HAS_MULTICOMP
    { MultiCompProcessor p; checkLatencyNotices (p, "crossover", 0.0f, 2.0f); }
    { MultiCompProcessor p; checkLatencyNotices (p, "limit", 0.0f, 1.0f); }
   #endif
   #if FANCY_HAS_FIR
    { FirProcessor p; checkLatencyNotices (p, "fft", 5.0f, 3.0f); }
    { FirProcessor p; checkLatencyNotices (p, "lowLatency", 0.0f, 1.0f); }
   #endif
   #if FANCY_HAS_GATE
    { GateProcessor p; checkLatencyNotices (p, "lookahead", 0.0f, 1.0f); }
   #endif
   #if FANCY_HAS_VERB
    { VerbProcessor p; checkLatencyNotices (p, "latency", 0.0f, 1.0f); }
   #endif
}

// §4.1 under an automation flood ----------------------------------------------------------

TEST_CASE ("Every parameter changed every block: no NaN, and no allocation on the audio thread", "[host][flood]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    forEachPlugin ([] (fancy::FancyProcessor& processor)
    {
        INFO (processor.getName());

        processor.prepareToPlay (rate, blockSize);
        processor.waitForBackgroundWork (20000);
        process (processor, 0.5); // warm-up, uncounted

        std::mt19937 random (17);
        std::uniform_real_distribution<float> unit (0.0f, 1.0f);
        double phase = 0.0;

        juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumInputChannels(), processor.getTotalNumOutputChannels()), blockSize);
        juce::MidiBuffer midi;
        midi.ensureSize (4096);

        auto finite = true;
        allocationsWhileCounting = 0;

        // Compare measures, and goes on and off, as it would with the editor open (W.4).
        processor.compare.setMeasuring (true);
        auto blocks = 0;

        for (int done = 0; done < (int) (10.0 * rate); done += blockSize)
        {
            for (auto* parameter : processor.getParameters())
                if (parameter != processor.getBypassParameter())
                    parameter->setValueNotifyingHost (unit (random));

            if (++blocks % 37 == 0)
                processor.compare.setOn (! processor.compare.isOn());

            buffer.clear();
            fill (buffer, 0, processor.getMainBusNumInputChannels(), random, phase);
            midi.clear();

            countingAllocations = true;
            processor.processBlock (buffer, midi);
            countingAllocations = false;

            finite = finite && allFinite (buffer);
        }

        processor.compare.setOn (false);
        processor.compare.setMeasuring (false);
        processor.releaseResources();

        CHECK (finite);
        CHECK (allocationsWhileCounting.load() == 0);
    });
}
