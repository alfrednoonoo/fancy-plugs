// Phase 4 Wave 2 item 46 for Fancy EQ: the band list and groups (docs/eq.md, "Phase 4 Wave 2:
// band list and groups", section 5). Test 2's numbers are docs/research/eq-groups-prepush.py's.

#include "EqEditor.h"
#include "EqGroups.h"
#include "EqProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <random>
#include <vector>

namespace
{
using eq::groups::Kind;

float value (EqProcessor& processor, const juce::String& id)
{
    auto& parameter = *processor.getApvts().getParameter (id);
    return parameter.convertFrom0to1 (parameter.getValue());
}

// An edit as the editor's controls make one: inside a change gesture.
void edit (EqProcessor& processor, const juce::String& id, float plain)
{
    auto& parameter = *processor.getApvts().getParameter (id);
    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (plain));
    parameter.endChangeGesture();
}

// What a host does: no gesture.
void automate (EqProcessor& processor, const juce::String& id, float plain)
{
    auto& parameter = *processor.getApvts().getParameter (id);
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (plain));
}

std::vector<double> moved (Kind kind, std::vector<double> starts, size_t edited, double now, double low, double high)
{
    const auto move = eq::groups::limitMove (kind, eq::groups::moveFor (kind, starts[edited], now), starts, low, high);

    for (auto& start : starts)
        start = eq::groups::apply (kind, start, move);

    return starts;
}

// Bands 2 and 3 (a bell at 200 Hz and one at 2 kHz) in group A, with the editor open.
struct Grouped
{
    Grouped()
    {
        processor.setParameter ("b2freq", 200.0f);
        processor.setParameter ("b2gain", -5.1f);
        processor.setParameter ("b2q", 2.0f);
        processor.setParameter ("b3freq", 2000.0f);
        processor.setParameter ("b3gain", 5.4f);
        processor.setParameter ("b3q", 1.0f);
        processor.setParameter ("b2group", 1.0f);
        processor.setParameter ("b3group", 1.0f);
        editor.reset (processor.createEditorAndMakeActive());
        processor.getWorkflow().commit();
    }

    ~Grouped()
    {
        processor.editorBeingDeleted (editor.get());
    }

    EqProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
};
} // namespace

// 1. State: a v1.7.0 state loads with no groups and Bands showing; groups never touch the audio.

TEST_CASE ("EQ: a state saved before groups loads with every band in no group, on Bands", "[eq][w2][groups]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    EqProcessor saved;
    saved.setParameter ("b2gain", 4.0f);

    juce::MemoryBlock state;
    saved.getStateInformation (state);

    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    REQUIRE (xml != nullptr);

    auto removed = 0;

    for (auto* child = xml->getFirstChildElement(); child != nullptr;)
    {
        auto* next = child->getNextElement();
        const auto id = child->getStringAttribute ("id");

        if (child->hasTagName ("PARAM") && (id.endsWith ("group") || id == "bandView"))
        {
            xml->removeChildElement (child, true);
            ++removed;
        }

        child = next;
    }

    CHECK (removed == eq::maxBands + 1);

    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary (*xml, oldState);

    // Into an instance whose new parameters have been moved.
    EqProcessor loaded;
    loaded.setParameter ("b2group", 3.0f);
    loaded.setParameter ("bandView", 1.0f);
    loaded.setStateInformation (oldState.getData(), (int) oldState.getSize());

    for (int band = 0; band < eq::maxBands; ++band)
        CHECK (value (loaded, EqProcessor::id (band, "group")) == 0.0f);

    CHECK (value (loaded, "bandView") == 0.0f);
    CHECK (value (loaded, "b2gain") == Catch::Approx (4.0f));
}

TEST_CASE ("EQ: groups and the band view don't change the sound", "[eq][w2][groups]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto render = [] (bool grouped)
    {
        EqProcessor processor;
        processor.setParameter ("b2gain", -5.0f);
        processor.setParameter ("b3gain", 6.0f);

        if (grouped)
        {
            processor.setParameter ("b2group", 1.0f);
            processor.setParameter ("b3group", 1.0f);
            processor.setParameter ("bandView", 1.0f);
        }

        processor.prepareToPlay (48000.0, 256);
        std::mt19937 random (46);
        std::uniform_real_distribution<float> noise (-0.5f, 0.5f);
        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer midi;
        std::vector<float> output;

        for (int block = 0; block < 40; ++block)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i)
                    buffer.setSample (ch, i, noise (random));

            processor.processBlock (buffer, midi);

            for (int i = 0; i < 256; ++i)
                output.push_back (buffer.getSample (0, i));
        }

        processor.releaseResources();
        return output;
    };

    CHECK (render (true) == render (false));
}

// 2. The group arithmetic alone (eq-groups-prepush.py's numbers) ---------------------------------

TEST_CASE ("EQ groups: Freq and Q by a ratio, Gain by an offset, stopping at the first member's limit", "[eq][w2][groups]")
{
    auto result = moved (Kind::ratio, { 200.0, 2000.0 }, 0, 400.0, 20.0, 20000.0);
    CHECK (result[0] == Catch::Approx (400.0));
    CHECK (result[1] == Catch::Approx (4000.0));

    result = moved (Kind::ratio, { 200.0, 2000.0 }, 0, 4000.0, 20.0, 20000.0);
    CHECK (result[0] == Catch::Approx (2000.0));
    CHECK (result[1] == Catch::Approx (20000.0));

    result = moved (Kind::ratio, { 200.0, 2000.0 }, 1, 100.0, 20.0, 20000.0);
    CHECK (result[0] == Catch::Approx (20.0));
    CHECK (result[1] == Catch::Approx (200.0));

    result = moved (Kind::offset, { -5.1, 5.4 }, 0, -2.1, -24.0, 24.0);
    CHECK (result[0] == Catch::Approx (-2.1));
    CHECK (result[1] == Catch::Approx (8.4));

    result = moved (Kind::offset, { -5.1, 5.4 }, 0, 20.0, -24.0, 24.0);
    CHECK (result[0] == Catch::Approx (13.5));
    CHECK (result[1] == Catch::Approx (24.0));

    result = moved (Kind::ratio, { 2.0, 1.0 }, 0, 4.0, 0.1, 30.0);
    CHECK (result[0] == Catch::Approx (4.0));
    CHECK (result[1] == Catch::Approx (2.0));

    CHECK (eq::groups::letter (0) == 0);
    CHECK (eq::groups::letter (1) == 'A');
    CHECK (eq::groups::letter (4) == 'D');
}

// 3. Through the editor: a grouped edit moves the group, as one undo step -----------------------

TEST_CASE ("EQ groups: an edit in the window moves the whole group, and undo restores it in one step", "[eq][w2][groups]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    Grouped grouped;
    auto& processor = grouped.processor;

    edit (processor, "b2freq", 400.0f);
    CHECK (value (processor, "b2freq") == Catch::Approx (400.0f).epsilon (0.002));
    CHECK (value (processor, "b3freq") == Catch::Approx (4000.0f).epsilon (0.002));

    edit (processor, "b2gain", -2.1f);
    CHECK (value (processor, "b3gain") == Catch::Approx (8.4f).margin (0.06));

    edit (processor, "b3q", 2.0f);
    CHECK (value (processor, "b2q") == Catch::Approx (4.0f).epsilon (0.01));

    // Past the top of the range: the group stops where band 3 reaches 24 kHz, so band 2 stops at
    // 2.4 kHz rather than going on to the 10 kHz asked for.
    processor.getWorkflow().commit();
    edit (processor, "b2freq", 10000.0f);
    CHECK (value (processor, "b3freq") == Catch::Approx (24000.0f).epsilon (0.002));
    CHECK (value (processor, "b2freq") == Catch::Approx (2400.0f).epsilon (0.002));

    // One step: both bands go back together.
    processor.getWorkflow().commit();
    REQUIRE (processor.getWorkflow().undo());
    CHECK (value (processor, "b2freq") == Catch::Approx (400.0f).epsilon (0.002));
    CHECK (value (processor, "b3freq") == Catch::Approx (4000.0f).epsilon (0.002));
}

TEST_CASE ("EQ groups: a band without gain follows Freq but keeps no gain, and other groups stay put", "[eq][w2][groups]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    Grouped grouped;
    auto& processor = grouped.processor;

    // Band 1, a high-pass at 80 Hz, joins group A; band 4 goes to group B.
    processor.setParameter ("b1type", 5.0f);
    processor.setParameter ("b1group", 1.0f);
    processor.setParameter ("b4group", 2.0f);
    const auto band1Gain = value (processor, "b1gain");
    const auto band4Freq = value (processor, "b4freq");

    edit (processor, "b2freq", 400.0f);
    CHECK (value (processor, "b1freq") == Catch::Approx (160.0f).epsilon (0.002));
    CHECK (value (processor, "b4freq") == band4Freq);

    edit (processor, "b2gain", 0.0f);
    CHECK (value (processor, "b1gain") == band1Gain);
    CHECK (value (processor, "b3gain") == Catch::Approx (10.5f).margin (0.06));
}

// 4. Automation moves only its own band (Decision 1 A) --------------------------------------------

TEST_CASE ("EQ groups: host automation moves only the band it's written to", "[eq][w2][groups]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    Grouped grouped;
    auto& processor = grouped.processor;

    automate (processor, "b2freq", 800.0f);
    CHECK (value (processor, "b2freq") == Catch::Approx (800.0f).epsilon (0.002));
    CHECK (value (processor, "b3freq") == Catch::Approx (2000.0f).epsilon (0.002));

    automate (processor, "b3gain", 0.0f);
    CHECK (value (processor, "b2gain") == Catch::Approx (-5.1f).margin (0.06));
}

// 5. The band view is saved with the project (Decision 4 A) --------------------------------------

TEST_CASE ("EQ: the band view and the groups survive a save and reload", "[eq][w2][groups]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    EqProcessor saved;
    saved.setParameter ("bandView", 1.0f);
    saved.setParameter ("b3group", 4.0f);

    juce::MemoryBlock state;
    saved.getStateInformation (state);

    EqProcessor loaded;
    loaded.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (value (loaded, "bandView") == 1.0f);
    CHECK (value (loaded, "b3group") == 4.0f);

    // A band added later starts in no group, whatever its slot held.
    loaded.removeBand (2);
    CHECK (loaded.addBand (500.0f, 0.0f) == 2);
    CHECK (value (loaded, "b3group") == 0.0f);
}
