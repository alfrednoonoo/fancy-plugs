// Loads each built .vst3 as a host does, through JUCE's VST3 hosting, and runs what real hosts
// are known to do to a plugin (Phase 2.1 item H.7.2). Where each scenario comes from is in
// docs/host-compat.md. Prints one line per check and exits with 1 if any failed.
// Usage: fancy-hosttest <folder with .vst3 bundles>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

namespace
{

using Instance = juce::AudioPluginInstance;

int failures = 0;

void report (const juce::String& plugin, const juce::String& check, bool passed, const juce::String& detail = {})
{
    std::cout << (passed ? "PASS  " : "FAIL  ") << plugin << ": " << check;

    if (detail.isNotEmpty())
        std::cout << " (" << detail << ")";

    std::cout << std::endl;

    if (! passed)
        ++failures;
}

void pump (int milliseconds)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds);
}

struct Loaded
{
    juce::PluginDescription description;
    juce::VST3PluginFormat* format = nullptr;

    std::unique_ptr<Instance> create (double rate = 48000.0, int block = 512) const
    {
        juce::String error;
        auto instance = format->createInstanceFromDescription (description, rate, block, error);

        if (instance == nullptr)
            std::cout << "cannot create " << description.name << ": " << error << std::endl;

        return instance;
    }
};

// A signal with something in every band, and a sidechain-friendly level.
void fill (juce::AudioBuffer<float>& buffer, int firstChannel, int numChannels, std::mt19937& random, double& phase, double rate, float level = 0.1f)
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

int channelsFor (const Instance& instance)
{
    return juce::jmax (1, instance.getTotalNumInputChannels(), instance.getTotalNumOutputChannels());
}

struct Result
{
    bool finite = true;
    float peak = 0.0f;
    juce::AudioBuffer<float> output;
};

// Processes a fixed signal: block sizes from the callback, parameter changes per block from the
// other one, input from a fixed seed.
Result run (Instance& instance, double rate, int numSamples, const std::function<int()>& nextBlock,
            const std::function<void (int position)>& beforeBlock = {}, float level = 0.1f, bool keepOutput = false)
{
    Result result;
    std::mt19937 random (7);
    double phase = 0.0;

    const auto channels = channelsFor (instance);
    const auto mainChannels = juce::jmax (1, instance.getMainBusNumInputChannels());

    juce::AudioBuffer<float> input (channels, numSamples);
    input.clear();
    fill (input, 0, mainChannels, random, phase, rate, level);

    if (keepOutput)
        result.output.setSize (channels, numSamples);

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> block (channels, 8192);

    for (int position = 0; position < numSamples;)
    {
        const auto size = juce::jmin (nextBlock(), numSamples - position);

        if (beforeBlock != nullptr)
            beforeBlock (position);

        juce::AudioBuffer<float> view (block.getArrayOfWritePointers(), channels, 0, size);

        for (int channel = 0; channel < channels; ++channel)
            view.copyFrom (channel, 0, input, channel, position, size);

        midi.clear();
        instance.processBlock (view, midi);

        for (int channel = 0; channel < channels; ++channel)
        {
            for (int i = 0; i < size; ++i)
            {
                const auto sample = view.getSample (channel, i);
                result.finite = result.finite && std::isfinite (sample);

                if (channel < juce::jmax (1, instance.getMainBusNumOutputChannels()))
                    result.peak = juce::jmax (result.peak, std::abs (sample));
            }

            if (keepOutput)
                result.output.copyFrom (channel, position, view, channel, 0, size);
        }

        position += size;
    }

    return result;
}

std::function<int()> fixedBlocks (int size) { return [size] { return size; }; }

std::function<int()> randomBlocks (unsigned seed)
{
    auto random = std::make_shared<std::mt19937> (seed);
    return [random]
    {
        const auto kind = (*random)() % 10;

        if (kind == 0) return 0;
        if (kind < 4) return (int) (1 + (*random)() % 16);
        if (kind < 8) return (int) (1 + (*random)() % 1024);
        return (int) (1 + (*random)() % 8192);
    };
}

// Scenarios ------------------------------------------------------------------------------

void offlineBlocks (const Loaded& plugin)
{
    const auto render = [&] (const std::function<int()>& blocks)
    {
        auto instance = plugin.create();
        instance->setNonRealtime (true);
        instance->prepareToPlay (48000.0, 8192);

        auto& parameters = instance->getParameters();

        // A parameter moves at fixed points, at a block boundary in both renders.
        auto* moving = parameters.isEmpty() ? nullptr : parameters[0];
        const auto result = run (*instance, 48000.0, 48000 * 10, [&blocks] { return blocks(); },
                                 [moving] (int position)
                                 {
                                     if (moving != nullptr && position % 96000 == 0)
                                         moving->setValue ((position / 96000) % 2 == 0 ? 0.3f : 0.7f);
                                 },
                                 0.1f, true);
        instance->releaseResources();
        return result;
    };

    // Blocks that always land on the 2 s points where the parameter moves.
    const auto aligned = [] (std::function<int()> source)
    {
        auto position = std::make_shared<int> (0);
        return [source, position]
        {
            auto size = source();
            const auto next = (*position / 96000 + 1) * 96000;
            size = juce::jmin (size, next - *position);
            *position += size;
            return size;
        };
    };

    const auto steady = render (aligned (fixedBlocks (128)));
    const auto random = render (aligned (randomBlocks (3)));

    auto worst = 0.0f;

    for (int channel = 0; channel < steady.output.getNumChannels(); ++channel)
        for (int i = 0; i < steady.output.getNumSamples(); ++i)
            worst = juce::jmax (worst, std::abs (steady.output.getSample (channel, i) - random.output.getSample (channel, i)));

    report (plugin.description.name, "offline, random blocks of 0-8192 against 128", random.finite && worst <= 1.0e-6f,
            worst == 0.0f ? "bit-identical" : juce::String (20.0 * std::log10 (juce::jmax (1.0e-30f, worst)), 1) + " dBFS");
}

void sampleRates (const Loaded& plugin)
{
    auto instance = plugin.create (44100.0);
    auto ok = true;

    for (const auto rate : { 44100.0, 48000.0, 96000.0, 44100.0 })
    {
        instance->releaseResources();
        instance->prepareToPlay (rate, 512);
        const auto result = run (*instance, rate, (int) rate, fixedBlocks (512));
        ok = ok && result.finite;
    }

    instance->releaseResources();
    report (plugin.description.name, "44.1 -> 48 -> 96 -> 44.1 kHz without reloading", ok);
}

void reactivate (const Loaded& plugin)
{
    auto instance = plugin.create();
    instance->prepareToPlay (48000.0, 512);
    const auto before = run (*instance, 48000.0, 24000, fixedBlocks (512));

    instance->releaseResources();
    instance->prepareToPlay (48000.0, 512);

    // A minute of silence, then signal again.
    const auto silence = run (*instance, 48000.0, 48000 * 60, fixedBlocks (512), {}, 0.0f);
    const auto after = run (*instance, 48000.0, 48000, fixedBlocks (512));
    instance->releaseResources();

    report (plugin.description.name, "deactivate, reactivate, and 60 s of silence", before.finite && silence.finite && after.finite);
}

void stateRestore (const Loaded& plugin)
{
    auto source = plugin.create();
    std::mt19937 random (11);
    std::uniform_real_distribution<float> unit (0.0f, 1.0f);

    for (auto* parameter : source->getParameters())
        if (parameter != source->getBypassParameter())
            parameter->setValue (unit (random));

    source->prepareToPlay (48000.0, 512);
    run (*source, 48000.0, 4800, fixedBlocks (512));

    juce::MemoryBlock state;
    source->getStateInformation (state);

    const auto matches = [&] (Instance& target)
    {
        const auto& a = source->getParameters();
        const auto& b = target.getParameters();

        if (a.size() != b.size())
            return false;

        for (int i = 0; i < a.size(); ++i)
            if (std::abs (a[i]->getValue() - b[i]->getValue()) > 1.0e-5f)
                return false;

        return true;
    };

    // JUCE sets the component state, then hands it to the controller: the VST3 split.
    auto beforePrepare = plugin.create();
    beforePrepare->setStateInformation (state.getData(), (int) state.getSize());
    pump (50);
    report (plugin.description.name, "state restored before the first prepareToPlay", matches (*beforePrepare));

    auto afterPrepare = plugin.create();
    afterPrepare->prepareToPlay (48000.0, 512);
    run (*afterPrepare, 48000.0, 4800, fixedBlocks (512));
    afterPrepare->setStateInformation (state.getData(), (int) state.getSize());
    pump (50);
    const auto result = run (*afterPrepare, 48000.0, 4800, fixedBlocks (512));
    report (plugin.description.name, "state restored while playing", matches (*afterPrepare) && result.finite);

    source->releaseResources();
    afterPrepare->releaseResources();
}

bool setLayout (Instance& instance, const juce::AudioChannelSet& main, const juce::AudioChannelSet& sidechain)
{
    auto layout = instance.getBusesLayout();
    layout.inputBuses.getReference (0) = main;
    layout.outputBuses.getReference (0) = main;

    if (layout.inputBuses.size() > 1)
        layout.inputBuses.getReference (1) = sidechain;

    return instance.setBusesLayout (layout);
}

void buses (const Loaded& plugin)
{
    auto instance = plugin.create();

    const auto mono = setLayout (*instance, juce::AudioChannelSet::mono(), juce::AudioChannelSet::disabled());
    instance->prepareToPlay (48000.0, 512);
    const auto monoRun = run (*instance, 48000.0, 48000, fixedBlocks (512));
    instance->releaseResources();
    report (plugin.description.name, "mono main bus", mono && monoRun.finite);

    if (instance->getBusCount (true) < 2)
        return;

    auto ok = true;

    for (const auto& sidechain : { juce::AudioChannelSet::disabled(), juce::AudioChannelSet::stereo(),
                                   juce::AudioChannelSet::mono(), juce::AudioChannelSet::disabled() })
    {
        ok = ok && setLayout (*instance, juce::AudioChannelSet::stereo(), sidechain);
        instance->prepareToPlay (48000.0, 512);
        ok = ok && run (*instance, 48000.0, 24000, fixedBlocks (512)).finite;
        instance->releaseResources();
    }

    report (plugin.description.name, "sidechain inactive -> stereo -> mono -> inactive", ok);
}

void hostBypass (const Loaded& plugin)
{
    auto instance = plugin.create();
    auto* bypass = instance->getBypassParameter();

    if (bypass == nullptr)
    {
        report (plugin.description.name, "host bypass", false, "no bypass parameter");
        return;
    }

    instance->prepareToPlay (48000.0, 512);
    bypass->setValue (1.0f);
    pump (20);

    const auto latency = instance->getLatencySamples();
    const auto length = 48000 * 2;
    const auto result = run (*instance, 48000.0, length, fixedBlocks (512), {}, 0.1f, true);

    // The same input as run() made, to compare against.
    std::mt19937 random (7);
    double phase = 0.0;
    juce::AudioBuffer<float> input (channelsFor (*instance), length);
    input.clear();
    fill (input, 0, juce::jmax (1, instance->getMainBusNumInputChannels()), random, phase, 48000.0);

    // After the crossfade, the output is the input delayed by the reported latency.
    auto worst = 0.0f;

    for (int channel = 0; channel < juce::jmax (1, instance->getMainBusNumOutputChannels()); ++channel)
        for (int i = 24000; i < length; ++i)
            worst = juce::jmax (worst, std::abs (result.output.getSample (channel, i) - input.getSample (channel, i - latency)));

    instance->releaseResources();
    report (plugin.description.name, "host bypass is time-aligned", result.finite && worst < 1.0e-5f,
            "latency " + juce::String (latency) + ", worst " + juce::String (worst, 8));
}

struct LatencyCounter final : public juce::AudioProcessorListener
{
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}

    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& details) override
    {
        if (details.latencyChanged)
            ++notices;
    }

    int notices = 0;
};

// The settings that change each plugin's latency, by parameter name, with two values.
struct LatencySetting
{
    const char* plugin;
    const char* parameter;
    float from, to;
};

const LatencySetting latencySettings[] {
    { "Fancy Exciter", "Oversampling", 0.0f, 0.5f },
    { "Fancy Comp", "Oversampling", 0.0f, 0.5f },
    { "Fancy EQ", "Phase", 0.0f, 1.0f },
    { "Fancy MultiComp", "Crossover", 0.0f, 1.0f },
    { "Fancy FIR", "Low latency", 0.0f, 1.0f },
    { "Fancy Gate", "Lookahead", 0.0f, 1.0f },
    { "Fancy Verb", "Latency mode", 0.0f, 1.0f },
};

void latencyNotices (const Loaded& plugin)
{
    for (const auto& setting : latencySettings)
    {
        if (plugin.description.name != setting.plugin)
            continue;

        auto instance = plugin.create();
        juce::AudioProcessorParameter* parameter = nullptr;

        for (auto* candidate : instance->getParameters())
            if (candidate->getName (64) == setting.parameter)
                parameter = candidate;

        if (parameter == nullptr)
        {
            report (plugin.description.name, juce::String ("latency notice for ") + setting.parameter, false, "no such parameter");
            continue;
        }

        parameter->setValue (setting.from);
        instance->prepareToPlay (48000.0, 512);
        run (*instance, 48000.0, 24000, fixedBlocks (512));
        pump (50);

        LatencyCounter counter;
        instance->addListener (&counter);
        const auto start = instance->getLatencySamples();

        const auto settle = [&]
        {
            for (int i = 0; i < 20; ++i)
            {
                run (*instance, 48000.0, 4800, fixedBlocks (512));
                pump (20);
            }
        };

        parameter->setValue (setting.to);
        settle();
        const auto changed = instance->getLatencySamples();
        const auto first = counter.notices;

        parameter->setValue (setting.from);
        settle();
        const auto back = instance->getLatencySamples();

        instance->removeListener (&counter);
        instance->releaseResources();

        report (plugin.description.name, juce::String ("latency notice for ") + setting.parameter,
                changed != start && back == start && first == 1 && counter.notices == 2,
                juce::String (start) + " -> " + juce::String (changed) + " -> " + juce::String (back)
                    + ", notices " + juce::String (first) + " then " + juce::String (counter.notices));
    }
}

void automationFlood (const Loaded& plugin)
{
    auto instance = plugin.create();
    instance->prepareToPlay (48000.0, 512);

    std::mt19937 random (13);
    std::uniform_real_distribution<float> unit (0.0f, 1.0f);
    auto& parameters = instance->getParameters();

    const auto result = run (*instance, 48000.0, 48000 * 10, fixedBlocks (512), [&] (int)
    {
        for (auto* parameter : parameters)
            if (parameter != instance->getBypassParameter())
                parameter->setValue (unit (random));
    });

    instance->releaseResources();
    report (plugin.description.name, "every parameter changed every block for 10 s", result.finite);
}

void editorCycles (const Loaded& plugin)
{
    auto instance = plugin.create();
    instance->prepareToPlay (48000.0, 512);

    auto ok = true;

    for (int i = 0; i < 50 && ok; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (instance->createEditorIfNeeded());

        if (editor == nullptr)
        {
            ok = false;
            break;
        }

        juce::Component window;
        window.setOpaque (true);
        window.setSize (editor->getWidth(), editor->getHeight());
        window.addAndMakeVisible (*editor);
        window.addToDesktop (0);
        window.setVisible (true);
        pump (10);

        if (i % 10 == 0 && editor->isResizable())
        {
            editor->setSize (editor->getWidth() + 40, editor->getHeight() + 30);
            pump (10);
        }

        window.removeFromDesktop();
        instance->editorBeingDeleted (editor.get());
        editor.reset();
    }

    instance->releaseResources();
    report (plugin.description.name, "editor opened and closed 50 times, and resized", ok);
}

} // namespace

int main (int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: fancy-hosttest <folder with .vst3 bundles>\n";
        return 2;
    }

    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    const auto bundles = folder.findChildFiles (juce::File::findDirectories, false, "*.vst3");

    if (bundles.isEmpty())
    {
        std::cerr << "no .vst3 bundles in " << folder.getFullPathName() << "\n";
        return 2;
    }

    juce::VST3PluginFormat format;

    for (const auto& bundle : bundles)
    {
        juce::OwnedArray<juce::PluginDescription> types;
        format.findAllTypesForFile (types, bundle.getFullPathName());

        if (types.isEmpty())
        {
            report (bundle.getFileName(), "loads", false);
            continue;
        }

        const Loaded plugin { *types[0], &format };
        report (plugin.description.name, "loads", plugin.create() != nullptr);

        offlineBlocks (plugin);
        sampleRates (plugin);
        reactivate (plugin);
        stateRestore (plugin);
        buses (plugin);
        hostBypass (plugin);
        latencyNotices (plugin);
        automationFlood (plugin);
        editorCycles (plugin);
    }

    std::cout << (failures == 0 ? "All host checks passed" : juce::String (failures) + " host checks failed") << std::endl;
    return failures == 0 ? 0 : 1;
}
