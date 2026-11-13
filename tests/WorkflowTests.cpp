// Phase 2.1 item W: undo and redo, A/B, and level-matched Compare, for all nine plugins.

#include <catch2/catch_test_macros.hpp>

#include <fancy/ui/ShellEditor.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <random>
#include <thread>
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

#if FANCY_HAS_DS
 #include "DsProcessor.h"
#endif
#if FANCY_HAS_DSMINI
 #include "DsMiniProcessor.h"
#endif
#if FANCY_HAS_SATURE
 #include "SatureProcessor.h"
#endif
#if FANCY_HAS_ROOM
 #include "RoomProcessor.h"
#endif

namespace
{
constexpr double rate = 48000.0;
constexpr int blockSize = 512;

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
   #if FANCY_HAS_LIMIT
    { LimitProcessor p; callback (p); }
   #endif
   #if FANCY_HAS_DS
    { DsProcessor p; callback (p); }
   #endif
   #if FANCY_HAS_DSMINI
    { DsMiniProcessor p; callback (p); }
   #endif
   #if FANCY_HAS_SATURE
    { SatureProcessor p; callback (p); }
   #endif
   #if FANCY_HAS_ROOM
    { RoomProcessor p; callback (p); }
   #endif
}

std::vector<juce::AudioProcessorParameter*> editable (fancy::FancyProcessor& processor)
{
    std::vector<juce::AudioProcessorParameter*> list;

    for (auto* parameter : processor.getParameters())
        if (parameter != processor.getBypassParameter())
            list.push_back (parameter);

    return list;
}

// A parameter with a continuous range, so any two values differ.
juce::AudioProcessorParameter& firstContinuous (fancy::FancyProcessor& processor)
{
    for (auto* parameter : editable (processor))
        if (! parameter->isDiscrete() && ! parameter->isBoolean())
            return *parameter;

    return *editable (processor).front();
}

// What the editor does when a person moves a control.
void userSet (juce::AudioProcessorParameter& parameter, float value)
{
    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost (value);
    parameter.endChangeGesture();
}

// A structural edit outside the parameters, for the plugins that have state there.
void structuralEdit (fancy::FancyProcessor& processor, std::mt19937& random)
{
    juce::ignoreUnused (processor, random);

   #if FANCY_HAS_FIR
    if (auto* fir = dynamic_cast<FirProcessor*> (&processor))
    {
        std::uniform_real_distribution<double> db (-24.0, 12.0);
        fir->setPoints (0, { { 100.0, db (random) }, { 1000.0, db (random) }, { 8000.0, db (random) } });
    }
   #endif

   #if FANCY_HAS_VERB
    if (auto* verb = dynamic_cast<VerbProcessor*> (&processor))
    {
        const auto type = verb::allModuleTypes[(size_t) std::uniform_int_distribution<int> (1, (int) verb::allModuleTypes.size() - 1) (random)];

        if (verb->addModule (type, 0) == 0)
        {
            // Full: take the first module out instead.
            const auto chain = verb->getChain();

            if (chain.getNumChildren() > 0)
                verb->removeModule ((int) chain.getChild (0)[verb::ids::uid]);
        }
    }
   #endif
}

bool hasStructure (fancy::FancyProcessor& processor)
{
    juce::ignoreUnused (processor);

   #if FANCY_HAS_FIR
    if (dynamic_cast<FirProcessor*> (&processor) != nullptr)
        return true;
   #endif

   #if FANCY_HAS_VERB
    if (dynamic_cast<VerbProcessor*> (&processor) != nullptr)
        return true;
   #endif

    return false;
}

juce::String describeDifference (fancy::FancyProcessor& processor, const fancy::Snapshot& a, const fancy::Snapshot& b)
{
    for (size_t i = 0; i < a.values.size() && i < b.values.size(); ++i)
        if (! (std::isnan (a.values[i]) && std::isnan (b.values[i])) && a.values[i] != b.values[i])
            return processor.getParameters()[(int) i]->getName (64) + ": " + juce::String (a.values[i], 9) + " vs " + juce::String (b.values[i], 9);

    if (! a.extra.isEquivalentTo (b.extra))
        return "state outside the parameters";

    return {};
}

void process (fancy::FancyProcessor& processor, juce::AudioBuffer<float>& buffer)
{
    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);
}

// Paul Kellet's pink noise filter.
struct Pink
{
    std::mt19937 random { 7 };
    std::uniform_real_distribution<float> white { -1.0f, 1.0f };
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;

    float next()
    {
        const auto w = white (random);
        b0 = 0.99886f * b0 + w * 0.0555179f;
        b1 = 0.99332f * b1 + w * 0.0750759f;
        b2 = 0.96900f * b2 + w * 0.1538520f;
        b3 = 0.86650f * b3 + w * 0.3104856f;
        b4 = 0.55000f * b4 + w * 0.5329522f;
        b5 = -0.7616f * b5 - w * 0.0168980f;
        const auto out = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
        b6 = w * 0.115926f;
        return out * 0.05f;
    }
};
} // namespace

// W.2 ------------------------------------------------------------------------------------------

TEST_CASE ("Undo: 50 random edits then 50 undos return to the start; redo replays to the end", "[workflow][undo]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    forEachPlugin ([] (fancy::FancyProcessor& processor)
    {
        INFO (processor.getName());

        auto& workflow = processor.getWorkflow();
        workflow.rebase();

        const auto start = workflow.capture();
        const auto parameters = editable (processor);

        std::mt19937 random (1234);
        std::uniform_int_distribution<int> pick (0, (int) parameters.size() - 1);
        std::uniform_real_distribution<float> unit (0.0f, 1.0f);

        for (int edit = 0; edit < 50; ++edit)
        {
            if (hasStructure (processor) && edit % 5 == 4)
                structuralEdit (processor, random);
            else
                userSet (*parameters[(size_t) pick (random)], unit (random));

            workflow.commit();
        }

        const auto end = workflow.capture();
        const auto steps = workflow.getNumUndoSteps();

        // An edit that lands on the value already there isn't a step.
        CHECK (steps >= 35);
        CHECK (steps <= 50);

        for (int i = 0; i < steps; ++i)
            REQUIRE (workflow.undo());

        CHECK_FALSE (workflow.canUndo());
        INFO (describeDifference (processor, start, workflow.capture()));
        CHECK (workflow.capture().sameAs (start));

        for (int i = 0; i < steps; ++i)
            REQUIRE (workflow.redo());

        CHECK_FALSE (workflow.canRedo());
        INFO (describeDifference (processor, end, workflow.capture()));
        CHECK (workflow.capture().sameAs (end));
    });
}

TEST_CASE ("Undo: the host's automation, state loads and its own undo add no steps", "[workflow][undo]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    forEachPlugin ([] (fancy::FancyProcessor& processor)
    {
        INFO (processor.getName());

        auto& workflow = processor.getWorkflow();
        workflow.rebase();

        const auto parameters = editable (processor);
        auto& edited = firstContinuous (processor);
        auto& automated = parameters.back() != &edited ? *parameters.back() : *parameters.front();

        userSet (edited, edited.getValue() > 0.5f ? 0.1f : 0.9f);
        workflow.commit();
        REQUIRE (workflow.getNumUndoSteps() == 1);

        // Automation playback: from the message thread and from an audio thread, with the
        // person's edit pending, and with nothing pending.
        for (int i = 0; i < 20; ++i)
        {
            automated.setValueNotifyingHost ((float) i / 20.0f);
            workflow.commit();
        }

        std::thread audio ([&automated]
        {
            for (int i = 0; i < 200; ++i)
                automated.setValueNotifyingHost ((float) (i % 10) / 10.0f);
        });
        audio.join();
        workflow.commit();

        edited.beginChangeGesture();
        automated.setValueNotifyingHost (0.33f);
        edited.setValueNotifyingHost (edited.getValue() > 0.5f ? 0.0f : 1.0f);
        edited.endChangeGesture();
        workflow.commit();

        CHECK (workflow.getNumUndoSteps() == 2);

        // The step holds the person's edit only: undoing it leaves the automated value alone.
        const auto automatedValue = automated.getValue();
        REQUIRE (workflow.undo());
        CHECK (automated.getValue() == automatedValue);

        // A state load, as a host's project load or its own undo does it.
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        userSet (edited, 0.2f);
        processor.setStateInformation (state.getData(), (int) state.getSize());
        workflow.commit();

        CHECK (workflow.getNumUndoSteps() == 1);
        CHECK (workflow.getNumRedoSteps() == 1);
    });
}

TEST_CASE ("Undo: steps are gestures, kept to 100, and redo clears on a new edit", "[workflow][undo]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    forEachPlugin ([] (fancy::FancyProcessor& processor)
    {
        INFO (processor.getName());

        auto& workflow = processor.getWorkflow();
        workflow.rebase();

        auto& parameter = firstContinuous (processor);

        // One drag, many values: one step, named after the parameter.
        const auto from = parameter.getValue();
        const auto to = from > 0.5f ? 0.1f : 0.9f;
        parameter.beginChangeGesture();

        for (int i = 0; i <= 20; ++i)
        {
            parameter.setValueNotifyingHost (from + (to - from) * (float) i / 20.0f);
            workflow.commit(); // ignored while the gesture is open
        }

        parameter.endChangeGesture();
        workflow.commit();

        CHECK (workflow.getNumUndoSteps() == 1);
        CHECK (workflow.getUndoName() == parameter.getName (64));

        // A gesture that changes nothing isn't a step.
        userSet (parameter, parameter.getValue());
        workflow.commit();
        CHECK (workflow.getNumUndoSteps() == 1);

        // 101 edits keep the last 100.
        for (int i = 0; i < 101; ++i)
        {
            userSet (parameter, i % 2 == 0 ? 0.0f : 1.0f);
            workflow.commit();
        }

        CHECK (workflow.getNumUndoSteps() == fancy::Workflow::maxSteps);

        REQUIRE (workflow.undo());
        CHECK (workflow.canRedo());

        userSet (parameter, 0.5f);
        workflow.commit();
        CHECK_FALSE (workflow.canRedo());

        // A preset load is one step, however many parameters it sets.
        if (processor.getPresets().getFactoryNames().size() > 1)
        {
            const auto before = workflow.getNumUndoSteps();
            const auto settings = workflow.capture();

            processor.getPresets().load (processor.getPresets().getFactoryNames().size() - 1);
            workflow.commit();

            // The history is full, so the count stays at 100; the newest step is the load.
            CHECK (workflow.getNumUndoSteps() == juce::jmin (before + 1, fancy::Workflow::maxSteps));
            CHECK (workflow.getUndoName() == "Load preset");
            REQUIRE (workflow.undo());
            INFO (describeDifference (processor, settings, workflow.capture()));
            CHECK (workflow.capture().sameAs (settings));
        }
    });
}

TEST_CASE ("Undo: Ctrl+Z, Ctrl+Shift+Z and Ctrl+Y belong to the plugin; other keys go to the host", "[workflow][keyboard]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    forEachPlugin ([] (fancy::FancyProcessor& processor)
    {
        INFO (processor.getName());

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        REQUIRE (editor != nullptr);

        auto& workflow = processor.getWorkflow();
        auto& parameter = firstContinuous (processor);
        const auto before = parameter.getValue();

        userSet (parameter, before > 0.5f ? 0.0f : 1.0f);
        workflow.commit();
        const auto after = parameter.getValue();

        CHECK (editor->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
        CHECK (parameter.getValue() == before);
        CHECK (editor->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
        CHECK (parameter.getValue() == after);
        CHECK (editor->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
        CHECK (editor->keyPressed (juce::KeyPress ('y', juce::ModifierKeys::ctrlModifier, 0)));
        CHECK (parameter.getValue() == after);

        for (const auto& key : { juce::KeyPress ('z'), juce::KeyPress ('y'), juce::KeyPress (juce::KeyPress::spaceKey),
                                 juce::KeyPress ('z', juce::ModifierKeys::altModifier, 0) })
            CHECK_FALSE (editor->keyPressed (key));

        processor.editorBeingDeleted (editor.get());
    });
}

// W.3 ------------------------------------------------------------------------------------------

TEST_CASE ("A/B: switching, copying, undo, and both slots saved with the project", "[workflow][ab]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    forEachPlugin ([] (fancy::FancyProcessor& processor)
    {
        INFO (processor.getName());

        auto& workflow = processor.getWorkflow();
        workflow.rebase();

        const auto parameters = editable (processor);
        std::mt19937 random (5);
        std::uniform_real_distribution<float> unit (0.0f, 1.0f);

        const auto randomise = [&]
        {
            for (int i = 0; i < 12; ++i)
                userSet (*parameters[(size_t) std::uniform_int_distribution<int> (0, (int) parameters.size() - 1) (random)], unit (random));

            if (hasStructure (processor))
                structuralEdit (processor, random);

            workflow.commit();
        };

        // B starts as a copy of A.
        randomise();
        const auto a = workflow.capture();
        CHECK (workflow.getActiveSlot() == 0);

        workflow.selectSlot (1);
        CHECK (workflow.getActiveSlot() == 1);
        CHECK (workflow.capture().sameAs (a));

        randomise();
        const auto b = workflow.capture();
        REQUIRE_FALSE (b.sameAs (a));

        // A -> B -> A restores each exactly.
        workflow.selectSlot (0);
        INFO (describeDifference (processor, a, workflow.capture()));
        CHECK (workflow.capture().sameAs (a));
        CHECK (workflow.getOtherSlot().sameAs (b));

        workflow.selectSlot (1);
        CHECK (workflow.capture().sameAs (b));
        workflow.selectSlot (0);
        CHECK (workflow.capture().sameAs (a));

        // A switch is one undo step, and undoing it switches back.
        const auto steps = workflow.getNumUndoSteps();
        workflow.selectSlot (1);
        CHECK (workflow.getNumUndoSteps() == steps + 1);
        REQUIRE (workflow.undo());
        CHECK (workflow.getActiveSlot() == 0);
        CHECK (workflow.capture().sameAs (a));
        REQUIRE (workflow.redo());
        CHECK (workflow.getActiveSlot() == 1);
        CHECK (workflow.capture().sameAs (b));

        // Copy B to A, and undo it.
        workflow.copyToOtherSlot();
        CHECK (workflow.getOtherSlot().sameAs (b));
        REQUIRE (workflow.undo());
        CHECK (workflow.getOtherSlot().sameAs (a));

        // Saved and loaded: the active slot, and the other one, as the host restores them.
        juce::MemoryBlock state;
        processor.getStateInformation (state);

        workflow.selectSlot (0);
        randomise();
        processor.setStateInformation (state.getData(), (int) state.getSize());

        CHECK (workflow.getActiveSlot() == 1);
        CHECK (workflow.getNumUndoSteps() > 0); // the history stays; the load isn't a step

        // A parameter goes through its plain value in the saved state, as the host's own does.
        const auto other = workflow.getOtherSlot();

        for (size_t i = 0; i < other.values.size(); ++i)
        {
            if (std::isnan (other.values[i]))
                continue;

            // Compared as plain values: a switch keeps the raw 0-1 value it was given (0.74 is
            // on), and the saved state, like the host's own, keeps its plain value (on).
            auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (processor.getParameters()[(int) i]);
            REQUIRE (parameter != nullptr);

            const auto loaded = parameter->convertFrom0to1 (other.values[i]);
            const auto saved = parameter->convertFrom0to1 (a.values[i]);
            INFO (parameter->getName (64) << ": loaded " << loaded << ", saved " << saved);
            CHECK (std::abs (loaded - saved) <= 1.0e-5f * juce::jmax (1.0f, std::abs (saved)));
        }

        CHECK (other.extra.isEquivalentTo (a.extra));
    });
}

TEST_CASE ("A/B: a version 1 state loads with B as a copy of A, and renders bit-identically", "[workflow][ab][compat]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    forEachPlugin ([] (fancy::FancyProcessor& original)
    {
        INFO (original.getName());

        // Settings away from the defaults, then the state as the current release saved it.
        std::mt19937 random (11);
        std::uniform_real_distribution<float> unit (0.1f, 0.9f);
        const auto parameters = editable (original);

        for (size_t i = 0; i < parameters.size(); i += 3)
            parameters[i]->setValueNotifyingHost (unit (random));

        juce::MemoryBlock saved;
        original.getStateInformation (saved);

        auto tree = juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize()));
        tree.removeChild (tree.getChildWithName (fancy::Workflow::slotsType), nullptr);
        tree.setProperty ("stateVersion", 1, nullptr);

        juce::MemoryBlock versionOne;
        juce::AudioProcessor::copyXmlToBinary (*tree.createXml(), versionOne);

        // Two fresh instances: one loads the version 1 state, the other the version 2 state.
        const auto render = [] (fancy::FancyProcessor& processor)
        {
            processor.setRateAndBufferSizeDetails (rate, blockSize);
            processor.prepareToPlay (rate, blockSize);
            processor.waitForBackgroundWork (20000);

            juce::AudioBuffer<float> all (juce::jmax (2, processor.getTotalNumInputChannels()), (int) rate);
            std::mt19937 noise (3);
            std::uniform_real_distribution<float> sample (-0.3f, 0.3f);

            for (int ch = 0; ch < processor.getMainBusNumInputChannels(); ++ch)
                for (int i = 0; i < all.getNumSamples(); ++i)
                    all.setSample (ch, i, sample (noise));

            for (int start = 0; start < all.getNumSamples(); start += blockSize)
            {
                juce::AudioBuffer<float> block (all.getArrayOfWritePointers(), all.getNumChannels(), start, juce::jmin (blockSize, all.getNumSamples() - start));
                process (processor, block);
                processor.waitForBackgroundWork (20000);
            }

            processor.releaseResources();
            return all;
        };

        const auto check = [&] (fancy::FancyProcessor& fresh, fancy::FancyProcessor& current)
        {
            fresh.setStateInformation (versionOne.getData(), (int) versionOne.getSize());
            current.setStateInformation (saved.getData(), (int) saved.getSize());

            CHECK (fresh.getWorkflow().getActiveSlot() == 0);
            CHECK (fresh.getWorkflow().getOtherSlot().sameAs (fresh.getWorkflow().capture()));
            CHECK (fresh.getWorkflow().getNumUndoSteps() == 0);
            CHECK (fresh.getWorkflow().capture().sameAs (current.getWorkflow().capture()));

            const auto expected = render (current);
            const auto actual = render (fresh);

            for (int ch = 0; ch < fresh.getMainBusNumOutputChannels(); ++ch)
                for (int i = 0; i < expected.getNumSamples(); ++i)
                    if (expected.getSample (ch, i) != actual.getSample (ch, i))
                        FAIL ("sample " << i << " on channel " << ch << " differs");
        };

        // A fresh instance of the same plugin.
       #define FANCY_CHECK_FRESH(Type) if (dynamic_cast<Type*> (&original) != nullptr) { Type fresh, current; check (fresh, current); return; }
       #if FANCY_HAS_KICKER
        FANCY_CHECK_FRESH (KickerProcessor)
       #endif
       #if FANCY_HAS_EXCITER
        FANCY_CHECK_FRESH (ExciterProcessor)
       #endif
       #if FANCY_HAS_COMP
        FANCY_CHECK_FRESH (CompProcessor)
       #endif
       #if FANCY_HAS_EQ
        FANCY_CHECK_FRESH (EqProcessor)
       #endif
       #if FANCY_HAS_VERB
        FANCY_CHECK_FRESH (VerbProcessor)
       #endif
       #if FANCY_HAS_DELAY
        FANCY_CHECK_FRESH (DelayProcessor)
       #endif
       #if FANCY_HAS_MULTICOMP
        FANCY_CHECK_FRESH (MultiCompProcessor)
       #endif
       #if FANCY_HAS_FIR
        FANCY_CHECK_FRESH (FirProcessor)
       #endif
       #if FANCY_HAS_GATE
        FANCY_CHECK_FRESH (GateProcessor)
       #endif
       #if FANCY_HAS_LIMIT
        FANCY_CHECK_FRESH (LimitProcessor)
       #endif
       #if FANCY_HAS_DS
        FANCY_CHECK_FRESH (DsProcessor)
       #endif
       #if FANCY_HAS_DSMINI
        FANCY_CHECK_FRESH (DsMiniProcessor)
       #endif
       #if FANCY_HAS_SATURE
        FANCY_CHECK_FRESH (SatureProcessor)
       #endif
       #if FANCY_HAS_ROOM
        FANCY_CHECK_FRESH (RoomProcessor)
       #endif
       #undef FANCY_CHECK_FRESH
    });
}

TEST_CASE ("A/B: switching mid-playback makes no step beyond the old and new sound's own", "[workflow][ab][click]")
{
    // A sine plays through A (the defaults); B is the last factory preset. The largest
    // sample-to-sample step in the 50 ms after the switch is held against the largest in
    // steady playback of A and of B.
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    forEachPlugin ([] (fancy::FancyProcessor& processor)
    {
        auto& workflow = processor.getWorkflow();
        auto& presets = processor.getPresets();

        if (presets.getFactoryNames().size() < 2)
            return;

        workflow.rebase();
        workflow.selectSlot (1);
        presets.load (presets.getFactoryNames().size() - 1);
        workflow.commit();
        workflow.selectSlot (0);

        processor.setRateAndBufferSizeDetails (rate, blockSize);
        processor.prepareToPlay (rate, blockSize);
        processor.waitForBackgroundWork (20000);

        const auto channels = juce::jmax (2, processor.getTotalNumInputChannels());
        double phase = 0.0;

        const auto run = [&] (double seconds)
        {
            juce::AudioBuffer<float> out (2, (int) (seconds * rate));
            juce::AudioBuffer<float> block (channels, blockSize);

            for (int start = 0; start < out.getNumSamples(); start += blockSize)
            {
                const auto n = juce::jmin (blockSize, out.getNumSamples() - start);
                block.setSize (channels, n, false, false, true);
                block.clear();

                for (int i = 0; i < n; ++i)
                {
                    const auto value = (float) (0.25 * std::sin (phase));
                    phase += 2.0 * juce::MathConstants<double>::pi * 220.0 / rate;

                    for (int ch = 0; ch < processor.getMainBusNumInputChannels(); ++ch)
                        block.setSample (ch, i, value);
                }

                process (processor, block);
                processor.waitForBackgroundWork (20000);

                for (int ch = 0; ch < 2; ++ch)
                    out.copyFrom (ch, start, block, juce::jmin (ch, processor.getMainBusNumOutputChannels() - 1), 0, n);
            }

            return out;
        };

        const auto largestStep = [] (const juce::AudioBuffer<float>& buffer, int from, int to)
        {
            auto largest = 0.0f;

            for (int ch = 0; ch < 2; ++ch)
                for (int i = juce::jmax (1, from); i < juce::jmin (to, buffer.getNumSamples()); ++i)
                    largest = juce::jmax (largest, std::abs (buffer.getSample (ch, i) - buffer.getSample (ch, i - 1)));

            return largest;
        };

        const auto steadyA = run (2.0);
        workflow.selectSlot (1);
        const auto afterSwitch = run (0.05);
        const auto steadyB = run (2.0);

        const auto stepA = largestStep (steadyA, (int) rate, steadyA.getNumSamples());
        const auto stepB = largestStep (steadyB, (int) rate, steadyB.getNumSamples());
        const auto stepSwitch = largestStep (afterSwitch, 0, afterSwitch.getNumSamples());

        std::cout << "A/B switch: " << processor.getName() << " to " << presets.getFactoryNames()[presets.getFactoryNames().size() - 1]
                  << ": largest step " << stepSwitch << " after the switch, " << stepA << " in A, " << stepB << " in B" << std::endl;

        // Parameter smoothing glides to the other settings (docs/phase2_1-W.md, question 2).
        CHECK (stepSwitch <= 1.5f * juce::jmax (stepA, stepB) + 1.0e-4f);

        processor.releaseResources();
    });
}

// W.4 ------------------------------------------------------------------------------------------

#if FANCY_HAS_EQ
TEST_CASE ("Compare: a +6 dB bell on pink noise is level-matched within 0.5 LU, with no click", "[workflow][compare]")
{
    EqProcessor processor;

    REQUIRE (processor.addBand (1000.0f, 6.0f) >= 0);

    processor.setRateAndBufferSizeDetails (rate, blockSize);
    processor.prepareToPlay (rate, blockSize);
    processor.compare.setMeasuring (true);

    Pink pink;
    juce::AudioBuffer<float> block (juce::jmax (2, processor.getTotalNumInputChannels()), blockSize);
    fancy::dsp::ShortTermLoudness meter;
    meter.prepare (rate);

    auto largestStep = 0.0f, previous = 0.0f;

    const auto run = [&] (double seconds, bool measure)
    {
        for (int done = 0; done < (int) (seconds * rate); done += blockSize)
        {
            block.clear();

            for (int i = 0; i < blockSize; ++i)
            {
                const auto value = pink.next();
                block.setSample (0, i, value);
                block.setSample (1, i, value);
            }

            process (processor, block);

            for (int i = 0; i < blockSize; ++i)
            {
                const float frame[] { block.getSample (0, i), block.getSample (1, i) };

                if (measure)
                    meter.add (frame, 2);

                largestStep = juce::jmax (largestStep, std::abs (frame[0] - previous));
                previous = frame[0];
            }
        }
    };

    run (4.0, false);

    const auto processed = processor.compare.getWetLoudness();
    const auto dry = processor.compare.getDryLoudness();
    INFO ("processed " << processed << " LUFS, dry " << dry << " LUFS, gain " << processor.compare.getGainDb() << " dB");
    CHECK (processed - dry > 1.0);
    CHECK (std::abs (processor.compare.getGainDb() - (float) (processed - dry)) < 0.01f);

    const auto steadyStep = largestStep;
    largestStep = 0.0f;

    // On: the gain is frozen, and the output is the level-matched input.
    const auto gain = processor.compare.getGainDb();
    processor.compare.setOn (true);
    run (0.5, false);
    const auto switchStep = largestStep;
    run (3.5, true);

    CHECK (processor.compare.getGainDb() == gain);
    CHECK (std::abs (meter.getLoudness() - processed) < 0.5);

    // The crossfade adds no step bigger than the noise's own.
    CHECK (switchStep < 1.5f * steadyStep);

    largestStep = 0.0f;
    processor.compare.setOn (false);
    run (0.5, false);
    CHECK (largestStep < 1.5f * steadyStep);

    processor.releaseResources();
}
#endif

TEST_CASE ("Compare: the output is the input, delayed by the plugin's latency, in every plugin", "[workflow][compare]")
{
    forEachPlugin ([] (fancy::FancyProcessor& processor)
    {
        INFO (processor.getName());

        processor.compare.setOn (true);
        processor.setRateAndBufferSizeDetails (rate, blockSize);
        processor.prepareToPlay (rate, blockSize);
        processor.waitForBackgroundWork (20000);

        const auto latency = processor.getLatencySamples();
        const auto total = latency + 4 * blockSize;
        const auto channels = juce::jmax (2, processor.getTotalNumInputChannels());

        std::vector<float> output;
        juce::AudioBuffer<float> block (channels, blockSize);

        for (int start = 0; start < total; start += blockSize)
        {
            block.clear();

            if (start == 0)
                for (int ch = 0; ch < processor.getMainBusNumInputChannels(); ++ch)
                    block.setSample (ch, 10, 0.5f);

            process (processor, block);

            for (int i = 0; i < blockSize; ++i)
                output.push_back (block.getSample (0, i));
        }

        // Nothing measured yet, so the gain is 0 dB.
        CHECK (processor.compare.getGainDb() == 0.0f);

        for (int i = 0; i < (int) output.size(); ++i)
            CHECK (output[(size_t) i] == (i == 10 + latency ? 0.5f : 0.0f));

        processor.compare.setOn (false);
        processor.releaseResources();
    });
}

TEST_CASE ("Compare: the gain holds below -70 LUFS and stays within 24 dB", "[workflow][compare]")
{
    fancy::dsp::CompareStage stage;
    stage.prepare (rate);
    stage.setMeasuring (true);

    juce::AudioBuffer<float> buffer (2, blockSize);

    // Silence: no update.
    for (int i = 0; i < 400; ++i)
    {
        buffer.clear();
        stage.pushInput (buffer.getArrayOfReadPointers(), 2, blockSize);
        stage.process (buffer.getArrayOfWritePointers(), 2, blockSize, 0);
    }

    CHECK (stage.getGainDb() == 0.0f);

    // A plugin that adds 40 dB: clamped to 24.
    std::mt19937 random (2);
    std::uniform_real_distribution<float> noise (-0.01f, 0.01f);

    for (int i = 0; i < 400; ++i)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < blockSize; ++s)
                buffer.setSample (ch, s, noise (random));

        stage.pushInput (buffer.getArrayOfReadPointers(), 2, blockSize);
        buffer.applyGain (100.0f);
        stage.process (buffer.getArrayOfWritePointers(), 2, blockSize, 0);
    }

    CHECK (stage.getGainDb() == fancy::dsp::CompareStage::maxGainDb);
}

// CPU, by the method in §0.4 of the Phase 2.1 brief: the median of five runs in one build, and
// the ratio to Fancy Comp at its defaults. Compare costs nothing while no editor is open, and
// its cost while measuring or on is printed against that.
TEST_CASE ("Workflow CPU measurement", "[.measure]")
{
    constexpr int measureBlock = 128;
    constexpr double seconds = 10.0;

    enum class State { idle, measuring, on };

    const auto cpu = [] (fancy::FancyProcessor& processor, State state)
    {
        std::vector<double> runs;

        for (int run = 0; run < 5; ++run)
        {
            processor.compare.setMeasuring (state != State::idle);
            processor.compare.setOn (state == State::on);
            processor.setRateAndBufferSizeDetails (rate, measureBlock);
            processor.prepareToPlay (rate, measureBlock);
            processor.waitForBackgroundWork (20000);

            juce::AudioBuffer<float> block (juce::jmax (2, processor.getTotalNumInputChannels()), measureBlock);
            juce::MidiBuffer midi;
            std::mt19937 random (8);
            std::uniform_real_distribution<float> noise (-0.25f, 0.25f);
            std::chrono::duration<double> spent {};

            for (int done = 0; done < (int) (seconds * rate); done += measureBlock)
            {
                block.clear();

                for (int ch = 0; ch < processor.getMainBusNumInputChannels(); ++ch)
                    for (int i = 0; i < measureBlock; ++i)
                        block.setSample (ch, i, noise (random));

                midi.clear();
                const auto start = std::chrono::steady_clock::now();
                processor.processBlock (block, midi);
                spent += std::chrono::steady_clock::now() - start;
            }

            processor.releaseResources();
            runs.push_back (100.0 * spent.count() / seconds);
        }

        processor.compare.setOn (false);
        processor.compare.setMeasuring (false);
        std::sort (runs.begin(), runs.end());
        return runs[2];
    };

    double reference = 0.0;

   #if FANCY_HAS_COMP
    {
        CompProcessor comp;
        reference = cpu (comp, State::idle);
    }
   #endif

    std::printf ("workflow cpu reference: Fancy Comp default %.3f %% (median of 5, 48 kHz, 128 samples, stereo)\n", reference);

    forEachPlugin ([&] (fancy::FancyProcessor& processor)
    {
        const auto idle = cpu (processor, State::idle);
        const auto measuring = cpu (processor, State::measuring);
        const auto on = cpu (processor, State::on);
        const auto ratio = [reference] (double value) { return reference > 0.0 ? value / reference : 0.0; };

        std::printf ("workflow cpu: %-18s default %.3f %% (%.2f x Comp); editor open, Compare measuring %.3f %% (%.2f x); Compare on %.3f %% (%.2f x)\n",
                     processor.getName().toRawUTF8(), idle, ratio (idle), measuring, ratio (measuring), on, ratio (on));
    });
}
