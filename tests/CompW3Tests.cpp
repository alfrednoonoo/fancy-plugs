// Phase 4 Wave 3 for Fancy Comp: History on the main page (docs/comp.md, "Phase 4 Wave 3: History
// on the main page", section 5; mockup 77 C1). Editor only: Comp's legacy, automation and render
// tests (tests 4 and 5) run as they are, and the history's own tests are CompW1Tests'.

#include "CompEditor.h"
#include "CompProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <memory>
#include <vector>

namespace
{
struct OpenEditor
{
    juce::ScopedJuceInitialiser_GUI gui;
    CompProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> base;
    CompEditor* editor = nullptr;

    OpenEditor()
    {
        processor.prepareToPlay (48000.0, 512);
        base.reset (processor.createEditorAndMakeActive());
        editor = dynamic_cast<CompEditor*> (base.get());
    }

    ~OpenEditor()
    {
        if (base != nullptr)
            processor.editorBeingDeleted (base.get());
    }
};
} // namespace

TEST_CASE ("comp w3: the editor opens 200 px taller, at 934 x 727", "[comp][w3]")
{
    OpenEditor open;
    REQUIRE (open.editor != nullptr);

    CHECK (open.editor->getWidth() == 934);
    CHECK (open.editor->getHeight() == 727 + fancy::ui::ShellEditor::titleHeight);

    if (auto* constrainer = open.editor->getConstrainer())
        CHECK (constrainer->getMinimumHeight() >= 727);
}

TEST_CASE ("comp w3: the main page's knobs are small, and nothing on it overlaps", "[comp][w3]")
{
    OpenEditor open;
    REQUIRE (open.editor != nullptr);

    auto& body = open.editor->getBodyForTests();
    const auto drawerOpen = GENERATE (false, true);
    open.editor->setDrawerOpen (drawerOpen);

    std::vector<juce::Component*> shown;
    auto knobs = 0;

    for (auto* child : body.getChildren())
    {
        if (! child->isVisible() || child->getBounds() == body.getLocalBounds())
            continue; // the background panels

        // Every main-page knob is the shared small knob: 90 x 99 with its label and value.
        if (dynamic_cast<fancy::ui::Knob*> (child) != nullptr)
        {
            ++knobs;
            CHECK (child->getWidth() == 90);
            CHECK (child->getHeight() == 99);
        }

        INFO (child->getName() << " at " << child->getBounds().toString());
        CHECK (body.getLocalBounds().contains (child->getBounds()));
        shown.push_back (child);
    }

    CHECK (knobs == 8);

    // With the drawer open it covers the knob cards (they're under it by design); nothing else may
    // meet it, and nothing else may meet anything.
    const auto* drawer = [&]() -> juce::Component*
    {
        for (auto* child : shown)
            if (child->getBounds() == open.editor->getDrawerBounds())
                return child;

        return nullptr;
    }();

    CHECK ((drawer != nullptr) == drawerOpen);

    for (size_t a = 0; a < shown.size(); ++a)
    {
        for (size_t b = a + 1; b < shown.size(); ++b)
        {
            const auto* first = shown[a];
            const auto* second = shown[b];

            if (first == drawer || second == drawer)
            {
                const auto* other = first == drawer ? second : first;
                const auto underDrawer = dynamic_cast<const fancy::ui::Knob*> (other) != nullptr
                                      || dynamic_cast<const fancy::ui::ParamCheckbox*> (other) != nullptr;

                if (underDrawer && other->getY() >= drawer->getY())
                    continue;
            }

            INFO (first->getBounds().toString() << " and " << second->getBounds().toString());
            CHECK_FALSE (first->getBounds().intersects (second->getBounds()));
        }
    }
}

TEST_CASE ("comp w3: the history graph is 500 x 276, and 500 x 152 above the open drawer", "[comp][w3]")
{
    OpenEditor open;
    REQUIRE (open.editor != nullptr);

    open.editor->setDrawerOpen (false);
    CHECK (open.editor->getHistoryGraphBounds() == juce::Rectangle<int> (224, 86, 500, 276));

    open.editor->setDrawerOpen (true);
    CHECK (open.editor->getHistoryGraphBounds() == juce::Rectangle<int> (224, 86, 500, 152));
    CHECK (open.editor->getDrawerBounds() == juce::Rectangle<int> (212, 250, 524, 374));
    CHECK (open.editor->getHistoryGraphBounds().getBottom() < open.editor->getDrawerBounds().getY());

    CHECK (DrawerTabs::numTabs == 4);   // Detector, Character, Upward, Options

    open.editor->setDrawerOpen (false);
    CHECK (open.editor->getHistoryGraphBounds().getHeight() == 276);
}
