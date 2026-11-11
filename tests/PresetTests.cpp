// Phase 3 Q2.1: the preset browser (categories, user folders, favourites, search, the pill's
// arrows), the title strip and free resizing, for all nine plugins.

#include <catch2/catch_test_macros.hpp>

#include <fancy/ui/ShellEditor.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <typeinfo>

#if FANCY_HAS_KICKER
 #include "KickerEditor.h"
 #include "KickerProcessor.h"
#endif

#if FANCY_HAS_EXCITER
 #include "ExciterEditor.h"
 #include "ExciterProcessor.h"
#endif

#if FANCY_HAS_COMP
 #include "CompEditor.h"
 #include "CompProcessor.h"
#endif

#if FANCY_HAS_EQ
 #include "EqEditor.h"
 #include "EqProcessor.h"
#endif

#if FANCY_HAS_VERB
 #include "VerbEditor.h"
 #include "VerbProcessor.h"
#endif

#if FANCY_HAS_DELAY
 #include "DelayEditor.h"
 #include "DelayProcessor.h"
#endif

#if FANCY_HAS_MULTICOMP
 #include "MultiCompEditor.h"
 #include "MultiCompProcessor.h"
#endif

#if FANCY_HAS_FIR
 #include "FirEditor.h"
 #include "FirProcessor.h"
#endif

#if FANCY_HAS_GATE
 #include "GateEditor.h"
 #include "GateProcessor.h"
#endif

#if FANCY_HAS_LIMIT
 #include "LimitEditor.h"
 #include "LimitProcessor.h"
#endif

#if FANCY_HAS_DS
 #include "DsEditor.h"
 #include "DsProcessor.h"
#endif
#if FANCY_HAS_DSMINI
 #include "DsMiniEditor.h"
 #include "DsMiniProcessor.h"
#endif
#if FANCY_HAS_SATURE
 #include "SatureEditor.h"
 #include "SatureProcessor.h"
#endif

using fancy::PresetManager;
using fancy::ui::ShellEditor;

namespace
{

// A preset folder of its own for each test, removed afterwards.
struct TempPresetRoot
{
    TempPresetRoot()
        : root (juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("fancy-preset-tests-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64())))
    {
        root.createDirectory();
        PresetManager::setUserRootForTests (root);
    }

    ~TempPresetRoot()
    {
        PresetManager::setUserRootForTests ({});
        root.deleteRecursively();
    }

    juce::File root;
};

void forEachProcessor (const std::function<void (fancy::FancyProcessor&)>& check)
{
#if FANCY_HAS_KICKER
    { KickerProcessor p; check (p); }
#endif
#if FANCY_HAS_EXCITER
    { ExciterProcessor p; check (p); }
#endif
#if FANCY_HAS_COMP
    { CompProcessor p; check (p); }
#endif
#if FANCY_HAS_EQ
    { EqProcessor p; check (p); }
#endif
#if FANCY_HAS_VERB
    { VerbProcessor p; check (p); }
#endif
#if FANCY_HAS_DELAY
    { DelayProcessor p; check (p); }
#endif
#if FANCY_HAS_MULTICOMP
    { MultiCompProcessor p; check (p); }
#endif
#if FANCY_HAS_FIR
    { FirProcessor p; check (p); }
#endif
#if FANCY_HAS_GATE
    { GateProcessor p; check (p); }
#endif
#if FANCY_HAS_LIMIT
    { LimitProcessor p; check (p); }
#endif
#if FANCY_HAS_DS
    { DsProcessor p; check (p); }
#endif
#if FANCY_HAS_DSMINI
    { DsMiniProcessor p; check (p); }
#endif
#if FANCY_HAS_SATURE
    { SatureProcessor p; check (p); }
#endif
}

void forEachEditor (const std::function<void (fancy::FancyProcessor&, ShellEditor&)>& check)
{
    forEachProcessor ([&check] (fancy::FancyProcessor& processor)
    {
        auto* base = processor.createEditorAndMakeActive();
        auto* editor = dynamic_cast<ShellEditor*> (base);
        REQUIRE (editor != nullptr);

        check (processor, *editor);

        editor->closePopups();
        processor.editorBeingDeleted (base);
        delete base;
    });
}

// Each parameter's value as it means it: a switch set to 0.13 is off, as a state saves it.
std::vector<float> parameterValues (fancy::FancyProcessor& processor)
{
    std::vector<float> values;

    for (auto* parameter : processor.getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            values.push_back (ranged->convertFrom0to1 (ranged->getValue()));
        else
            values.push_back (parameter->getValue());
    }

    return values;
}

PresetManager::Entry userEntry (const juce::String& path)
{
    PresetManager::Entry entry;
    entry.isUser = true;
    entry.path = path;
    entry.name = path.fromLastOccurrenceOf ("/", false, false);
    return entry;
}

// Every visible component of the layout lies inside the window.
void checkInside (juce::Component& root, juce::Component& component)
{
    for (auto* child : component.getChildren())
    {
        if (! child->isVisible() || child->getWidth() <= 0 || child->getHeight() <= 0)
            continue;

        const auto area = root.getLocalArea (child, child->getLocalBounds());
        INFO (child->getName() << " " << typeid (*child).name() << " at " << area.toString() << " in " << root.getLocalBounds().toString());
        CHECK (root.getLocalBounds().expanded (1).contains (area));
    }
}

} // namespace

TEST_CASE ("Presets: every factory preset but the first has a category", "[presets]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    forEachProcessor ([] (fancy::FancyProcessor& processor)
    {
        INFO (processor.getName());
        const auto entries = processor.getPresets().getEntries();
        REQUIRE (entries.size() >= 2);

        CHECK (entries.front().category.isEmpty());

        for (size_t i = 1; i < entries.size(); ++i)
        {
            INFO (entries[i].name);
            CHECK_FALSE (entries[i].isUser);
            CHECK (entries[i].category.isNotEmpty());
        }

        CHECK_FALSE (processor.getPresets().getFactoryCategories().isEmpty());
    });
}

TEST_CASE ("Presets: search matches the starts of words in the name and category", "[presets]")
{
    PresetManager::Entry entry;
    entry.name = "Parallel Crush";
    entry.category = "Drums";

    CHECK (PresetManager::matchesSearch (entry, ""));
    CHECK (PresetManager::matchesSearch (entry, "drum"));
    CHECK (PresetManager::matchesSearch (entry, "crush par"));
    CHECK (PresetManager::matchesSearch (entry, "PARALLEL"));
    CHECK_FALSE (PresetManager::matchesSearch (entry, "rush"));
    CHECK_FALSE (PresetManager::matchesSearch (entry, "drum vocal"));

    entry.name = "1/8 dotted";
    entry.category = "Tempo";
    CHECK (PresetManager::matchesSearch (entry, "8 dot"));
}

TEST_CASE ("Presets: a user preset with a category round-trips bit-identical", "[presets]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const TempPresetRoot temp;

    forEachProcessor ([&temp] (fancy::FancyProcessor& processor)
    {
        INFO (processor.getName());
        auto& presets = processor.getPresets();

        // Settings away from every default.
        for (auto* parameter : processor.getParameters())
            if (parameter != processor.getBypassParameter())
                parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, parameter->getDefaultValue() * 0.7f + 0.13f));

        const auto saved = parameterValues (processor);

        REQUIRE (presets.saveUser ("Round trip", "Drums"));
        CHECK (temp.root.getChildFile (processor.getName()).getChildFile ("Presets/Drums/Round trip.fancypreset").existsAsFile());
        CHECK (presets.getCurrentCategory() == "Drums");
        CHECK (presets.getUserCategories().contains ("Drums"));

        const auto entries = presets.getFiltered ({ PresetManager::Filter::Kind::userCategory, "Drums", {} });
        REQUIRE (entries.size() == 1);
        CHECK (entries.front().path == "Drums/Round trip");
        CHECK (presets.isCurrent (entries.front()));

        presets.load (0);
        CHECK_FALSE (presets.isUserPresetLoaded());

        presets.load (entries.front());
        CHECK (presets.isUserPresetLoaded());
        CHECK (presets.getCurrentName() == "Round trip");
        CHECK_FALSE (presets.isModified());

        const auto loaded = parameterValues (processor);
        REQUIRE (loaded.size() == saved.size());

        for (size_t i = 0; i < saved.size(); ++i)
            if (processor.getParameters()[(int) i] != processor.getBypassParameter())
                CHECK (loaded[i] == saved[i]);

        // The same name may sit in another category, but not twice in one.
        CHECK (presets.userPresetExists ("Round trip", "Drums"));
        CHECK_FALSE (presets.userPresetExists ("Round trip", {}));
        REQUIRE (presets.saveUser ("Round trip"));
        CHECK (presets.getUserCategories().contains (PresetManager::userCategory));
    });
}

TEST_CASE ("Presets: damaged and foreign files are left out and never applied", "[presets]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const TempPresetRoot temp;

#if FANCY_HAS_KICKER
    KickerProcessor processor;
    auto& presets = processor.getPresets();
    const auto folder = presets.getUserDirectory();
    REQUIRE (folder.createDirectory());

    REQUIRE (presets.saveUser ("Good"));
    const auto good = folder.getChildFile ("Good.fancypreset").loadFileAsString();

    folder.getChildFile ("Empty.fancypreset").create();
    folder.getChildFile ("Truncated.fancypreset").replaceWithText (good.substring (0, good.length() / 2));
    folder.getChildFile ("Text.fancypreset").replaceWithText ("This is not a preset.");
    folder.getChildFile ("Wrong root.fancypreset").replaceWithText ("<?xml version=\"1.0\"?>\n<Something stateVersion=\"3\"/>");
    folder.getChildFile ("Other plugin.fancypreset").replaceWithText ("<?xml version=\"1.0\"?>\n<FancyComp stateVersion=\"3\"><PARAM id=\"wet\" value=\"0.1\"/></FancyComp>");

    {
        juce::MemoryBlock noise;
        juce::Random random (42);

        for (int i = 0; i < 4096; ++i)
        {
            const auto byte = (char) random.nextInt (256);
            noise.append (&byte, 1);
        }

        folder.getChildFile ("Random bytes.fancypreset").replaceWithData (noise.getData(), noise.getSize());
    }

    CHECK (presets.getUserNames() == juce::StringArray ("Good"));

    auto& wet = *processor.getApvts().getParameter ("wet");
    wet.setValueNotifyingHost (0.25f);
    const auto before = parameterValues (processor);

    for (const auto* name : { "Empty", "Truncated", "Text", "Wrong root", "Other plugin", "Random bytes", "Missing" })
    {
        INFO (name);
        presets.load (userEntry (name));
        CHECK (parameterValues (processor) == before);
        CHECK (presets.getCurrentName() == "Good");
    }

    // A preset folder that's a file, or missing, lists nothing and doesn't crash.
    folder.deleteRecursively();
    CHECK (presets.getUserNames().isEmpty());
    folder.getParentDirectory().createDirectory();
    folder.replaceWithText ("not a folder");
    CHECK (presets.getUserNames().isEmpty());
    CHECK_FALSE (presets.saveUser ("Anything"));
#endif
}

TEST_CASE ("Presets: favourites persist, follow renames and survive a damaged file", "[presets]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const TempPresetRoot temp;

#if FANCY_HAS_KICKER
    PresetManager::Entry tight;
    tight.name = "Tight Kick";
    tight.factoryIndex = 1;

    {
        KickerProcessor processor;
        auto& presets = processor.getPresets();

        CHECK (presets.countFavourites() == 0);
        presets.setFavourite (tight, true);
        CHECK (presets.isFavourite (tight));

        REQUIRE (presets.saveUser ("Mine", "808s"));
        presets.setFavourite (userEntry ("808s/Mine"), true);
        CHECK (presets.countFavourites() == 2);

        const auto favourites = presets.getFiltered ({ PresetManager::Filter::Kind::favourites, {}, {} });
        CHECK (favourites.size() == 2);

        // Renaming carries the star; deleting drops it.
        REQUIRE (presets.renameUser ("Mine 2"));
        CHECK (presets.isFavourite (userEntry ("808s/Mine 2")));
        CHECK_FALSE (presets.isFavourite (userEntry ("808s/Mine")));
    }

    {
        // Another instance, as in another project, reads the same favourites.
        KickerProcessor processor;
        auto& presets = processor.getPresets();
        CHECK (presets.isFavourite (tight));
        CHECK (presets.isFavourite (userEntry ("808s/Mine 2")));

        presets.load (userEntry ("808s/Mine 2"));
        REQUIRE (presets.deleteUser());
        CHECK (presets.countFavourites() == 1);

        presets.getFavouritesFile().replaceWithText ("<favourites><item key=");
    }

    {
        KickerProcessor processor;
        auto& presets = processor.getPresets();
        CHECK (presets.countFavourites() == 0);
        CHECK (presets.getFiltered ({ PresetManager::Filter::Kind::favourites, {}, {} }).empty());

        presets.setFavourite (tight, true);
        CHECK (presets.isFavourite (tight));
    }
#endif
}

TEST_CASE ("Presets: the arrows step through the browser's list, wrap, and each step is one undo step", "[presets]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const TempPresetRoot temp;

    forEachProcessor ([] (fancy::FancyProcessor& processor)
    {
        INFO (processor.getName());
        auto& presets = processor.getPresets();
        auto& workflow = processor.getWorkflow();
        workflow.commit();

        const auto entries = presets.getEntries();
        const PresetManager::Filter all;

        // From the first preset, so plugins whose first isn't called Default start there too.
        presets.load (0);
        workflow.commit();

        for (size_t i = 1; i <= entries.size(); ++i)
        {
            const auto steps = workflow.getNumUndoSteps();
            REQUIRE (presets.step (1, all));
            workflow.commit();

            const auto& expected = entries[i % entries.size()];
            INFO (expected.name);
            CHECK (presets.isCurrent (expected));
            CHECK (workflow.getNumUndoSteps() <= juce::jmin (steps + 1, fancy::Workflow::maxSteps));
        }

        // Back past the first wraps to the last.
        presets.load (0);
        REQUIRE (presets.step (-1, all));
        CHECK (presets.isCurrent (entries.back()));

        // Within a category, only its presets.
        const auto category = presets.getFactoryCategories()[0];
        const PresetManager::Filter one { PresetManager::Filter::Kind::factoryCategory, category, {} };
        const auto members = presets.getFiltered (one);
        REQUIRE_FALSE (members.empty());

        for (size_t i = 0; i < members.size() * 2; ++i)
        {
            REQUIRE (presets.step (1, one));
            CHECK (presets.getFiltered (one).size() == members.size());
            CHECK (std::any_of (members.begin(), members.end(), [&] (const PresetManager::Entry& entry) { return presets.isCurrent (entry); }));
        }

        // An empty list does nothing.
        CHECK_FALSE (presets.step (1, { PresetManager::Filter::Kind::all, {}, "zzzz no such preset" }));
    });
}

TEST_CASE ("Preset browser: opens inside every window, filters, loads and closes", "[presets][popups]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const TempPresetRoot temp;

    forEachEditor ([] (fancy::FancyProcessor& processor, ShellEditor& editor)
    {
        INFO (processor.getName());
        auto& presets = processor.getPresets();
        REQUIRE (presets.saveUser ("Browser test", "Mine"));

        editor.getBrowserFilter() = {};
        editor.showPresetMenu();
        auto* browser = editor.getPresetBrowser();
        REQUIRE (browser != nullptr);

        CHECK (editor.getPopupParent().getLocalBounds().contains (browser->getPanelBounds()));
        CHECK (browser->getPanelBounds().getWidth() <= 420);
        CHECK (browser->getNumEntries() == (int) presets.getEntries().size());
        CHECK (browser->getFooterLabels().contains ("Rename" + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6"))));

        browser->setCategory (PresetManager::Filter::Kind::userCategory, "Mine");
        CHECK (browser->getNumEntries() == 1);

        browser->setCategory (PresetManager::Filter::Kind::all);
        browser->setSearch ("zzz");
        CHECK (browser->getNumEntries() == 0);
        CHECK (browser->getEmptyText().contains ("zzz"));

        browser->setSearch ({});
        browser->chooseEntry (0, false);
        CHECK (presets.isCurrent (browser->getEntry (0)));
        CHECK (editor.getPresetBrowser() != nullptr);

        // Esc closes it.
        browser->getSearchField().keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
        CHECK (editor.getPresetBrowser() == nullptr);

        // The arrows on the pill.
        editor.stepPreset (1);
        CHECK (presets.isCurrent (presets.getEntries()[1]));
    });
}

TEST_CASE ("Free resizing: every window resizes from its minimum to 200 % and keeps its layout inside", "[presets][resize]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    forEachEditor ([] (fancy::FancyProcessor& processor, ShellEditor& editor)
    {
        INFO (processor.getName());

        auto* constrainer = editor.getConstrainer();
        REQUIRE (constrainer != nullptr);
        CHECK (editor.isResizable());

        const auto minimum = juce::Rectangle<int> (constrainer->getMinimumWidth(), constrainer->getMinimumHeight());
        const auto maximum = juce::Rectangle<int> (constrainer->getMaximumWidth(), constrainer->getMaximumHeight());

        // The title strip is part of the window.
        CHECK (editor.getHeight() >= ShellEditor::titleHeight + ShellEditor::headerHeight);

        for (const auto size : { minimum, maximum })
        {
            INFO (size.toString());
            editor.setSize (size.getWidth(), size.getHeight());
            CHECK (editor.getWidth() == size.getWidth());
            CHECK (editor.getHeight() == size.getHeight());

            auto& root = editor.getPopupParent();
            const auto shown = editor.getLocalArea (&root, root.getLocalBounds());
            CHECK (editor.getLocalBounds().expanded (1).contains (shown));
            checkInside (root, root);
        }

        // A fixed layout's maximum is twice its minimum; a window dragged between them scales.
        if (constrainer->getFixedAspectRatio() > 0.0)
        {
            CHECK (maximum.getWidth() == 2 * minimum.getWidth());
            CHECK (maximum.getHeight() == 2 * minimum.getHeight());

            editor.setSize (juce::roundToInt (minimum.getWidth() * 1.5), juce::roundToInt (minimum.getHeight() * 1.5));
            CHECK (std::abs (editor.getUiScale() - 1.5f) < 0.01f);
            CHECK (std::abs ((float) (double) processor.getApvts().state.getProperty ("uiScale") - 1.5f) < 0.011f);
        }
    });
}
