// §0.3 of docs/BUILD_PROMPT_PHASE3_NEW_PLUGINS.md: listen and preview switches are temporary.
// Each plugin with one is checked the same way: it isn't a host parameter, it's never saved,
// it's off after a project or preset loads, undo and A/B leave it alone, closing the editor turns
// it off, and an offline render only hears it while the window is open and it's on.

#include <catch2/catch_test_macros.hpp>

#include <fancy/ui/ShellEditor.h>

#include <memory>
#include <random>

#if FANCY_HAS_DS
 #include "DsEditor.h"
 #include "DsProcessor.h"
#endif

namespace
{
constexpr double rate = 48000.0;
constexpr int blockSize = 256, numBlocks = 96;

// Half a second of loud noise, the same on both channels, rendered offline.
juce::AudioBuffer<float> render (fancy::FancyProcessor& processor)
{
    processor.setNonRealtime (true);
    processor.prepareToPlay (rate, blockSize);

    const auto channels = juce::jmax (processor.getTotalNumInputChannels(), processor.getTotalNumOutputChannels());
    const auto outputs = juce::jmin (2, processor.getMainBusNumOutputChannels());

    juce::AudioBuffer<float> output (outputs, blockSize * numBlocks);
    juce::AudioBuffer<float> buffer (channels, blockSize);
    juce::MidiBuffer midi;

    std::mt19937 random (3);
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);

    for (int block = 0; block < numBlocks; ++block)
    {
        buffer.clear();

        for (int i = 0; i < blockSize; ++i)
        {
            const auto sample = 1.4f * noise (random);

            for (int channel = 0; channel < outputs; ++channel)
                buffer.setSample (channel, i, sample);
        }

        processor.processBlock (buffer, midi);
        midi.clear();

        for (int channel = 0; channel < outputs; ++channel)
            output.copyFrom (channel, block * blockSize, buffer, channel, 0, blockSize);
    }

    processor.releaseResources();
    return output;
}

bool identical (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    if (a.getNumChannels() != b.getNumChannels() || a.getNumSamples() != b.getNumSamples())
        return false;

    for (int channel = 0; channel < a.getNumChannels(); ++channel)
        for (int i = 0; i < a.getNumSamples(); ++i)
            if (a.getSample (channel, i) != b.getSample (channel, i))
                return false;

    return true;
}

juce::String stateText (fancy::FancyProcessor& processor)
{
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    const auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    return xml != nullptr ? xml->toString() : juce::String {};
}

// `oldId` is the parameter the switch used to be, or would have been. `audible` asks for proof
// that the switch is heard while the window is open, for plugins whose switch changes noise at
// their defaults.
template <typename Processor>
void checkListenSwitch (const char* oldId, bool audible)
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto quotedId = juce::String ("\"") + oldId + "\"";

    SECTION ("it isn't a host parameter")
    {
        Processor processor;
        CHECK (processor.getApvts().getParameter (oldId) == nullptr);

        for (auto* parameter : processor.getParameters())
        {
            const auto name = parameter->getName (100);
            INFO (name);
            CHECK_FALSE (name.containsIgnoreCase ("listen"));
            CHECK_FALSE (name.containsIgnoreCase ("preview"));
        }
    }

    SECTION ("it counts only while the editor is open, and closing it turns the switch off")
    {
        Processor processor;
        processor.setListening (true);
        CHECK_FALSE (processor.isListening());

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        REQUIRE (editor != nullptr);
        CHECK (processor.isListening());

        editor.reset();
        CHECK_FALSE (processor.isListeningSwitchOn());
        CHECK_FALSE (processor.isListening());
    }

    SECTION ("undo and A/B leave it alone")
    {
        Processor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        auto* shell = dynamic_cast<fancy::ui::ShellEditor*> (editor.get());
        REQUIRE (shell != nullptr);

        auto& workflow = processor.getWorkflow();
        workflow.commit();
        const auto couldUndo = workflow.canUndo();

        processor.setListening (true);
        workflow.commit();
        CHECK_FALSE (workflow.hasPendingEdits());
        CHECK (workflow.canUndo() == couldUndo);

        shell->selectSlot (1);
        CHECK (processor.isListeningSwitchOn());
        shell->selectSlot (0);
        CHECK (processor.isListeningSwitchOn());
    }

    SECTION ("saved with it on, it reloads off, and the state doesn't mention it")
    {
        Processor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        processor.setListening (true);

        juce::MemoryBlock state;
        processor.getStateInformation (state);
        CHECK_FALSE (stateText (processor).contains (quotedId));

        Processor reloaded;
        std::unique_ptr<juce::AudioProcessorEditor> reloadedEditor (reloaded.createEditorAndMakeActive());
        reloaded.setStateInformation (state.getData(), (int) state.getSize());
        CHECK_FALSE (reloaded.isListening());

        // The same instance, reloading its own project with the window open.
        processor.setStateInformation (state.getData(), (int) state.getSize());
        CHECK_FALSE (processor.isListening());
    }

    SECTION ("a preset loads with it off")
    {
        Processor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        processor.setListening (true);
        processor.getPresets().load (0);
        CHECK_FALSE (processor.isListening());
    }

    SECTION ("a state saved with the old parameter on opens with it off, and drops it")
    {
        Processor processor;
        juce::MemoryBlock state;
        processor.getStateInformation (state);

        auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
        REQUIRE (xml != nullptr);
        auto* old = xml->createNewChildElement ("PARAM");
        old->setAttribute ("id", oldId);
        old->setAttribute ("value", 1.0);

        juce::MemoryBlock oldState;
        juce::AudioProcessor::copyXmlToBinary (*xml, oldState);

        Processor loaded;
        std::unique_ptr<juce::AudioProcessorEditor> editor (loaded.createEditorAndMakeActive());
        loaded.setStateInformation (oldState.getData(), (int) oldState.getSize());
        CHECK_FALSE (loaded.isListening());
        CHECK_FALSE (stateText (loaded).contains (quotedId));
    }

    SECTION ("an offline render only hears it while the window is open and it's on")
    {
        Processor off;
        const auto reference = render (off);

        // On, with no window: as if it were off, bit for bit.
        Processor on;
        on.setListening (true);
        CHECK (identical (reference, render (on)));

        Processor shown;
        std::unique_ptr<juce::AudioProcessorEditor> editor (shown.createEditorAndMakeActive());
        shown.setListening (true);
        CHECK (shown.isListening());

        if (audible)
            CHECK_FALSE (identical (reference, render (shown)));
    }
}
} // namespace

#if FANCY_HAS_DS
TEST_CASE ("DS: Listen is temporary (§0.3)", "[listen][ds]")
{
    checkListenSwitch<DsProcessor> ("listen", true);
}
#endif
