// Phase 2.1 item S: menus, tooltips, typed values and dialogs drawn by fancy-core.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <fancy/plugin/ParamHelpers.h>
#include <fancy/ui/ShellEditor.h>

#include <memory>
#include <vector>

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

using Catch::Approx;
using namespace fancy::ui;

namespace
{

// A minimal pop-up host, for menus outside a plugin editor.
class TestHost final : public juce::Component, public PopupHost
{
public:
    TestHost() { setSize (400, 400); }

    juce::Component& getPopupParent() override { return *this; }
    juce::Colour getAccentColour() override { return colours::accentYellow; }

    void adoptPopup (std::unique_ptr<juce::Component> popup) override
    {
        addAndMakeVisible (*popup);
        popups.push_back (std::move (popup));
    }

    void disposePopup (juce::Component* popup) override { popup->setVisible (false); }

    std::vector<std::unique_ptr<juce::Component>> popups;
};

// What an opener left open on the editor.
bool somethingOpen (ShellEditor& editor)
{
    auto& parent = editor.getPopupParent();

    if (MenuLayer::findOpen (parent) != nullptr || DialogLayer::findOpen (parent) != nullptr)
        return true;

    if (editor.getTooltips().isVisible() && editor.getTooltips().getShownText().isNotEmpty())
        return true;

    // A typed-value field, on the layout root.
    for (auto* child : parent.getChildren())
        if (auto* field = dynamic_cast<juce::TextEditor*> (child))
            if (field->isVisible())
                return true;

    return false;
}

template <typename Processor, typename Editor, typename Setup>
void checkEveryPopup (Setup setup)
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    Processor processor;
    auto* base = processor.createEditorAndMakeActive();
    auto* editor = dynamic_cast<Editor*> (base);
    REQUIRE (editor != nullptr);

    setup (*editor);

    const auto openers = editor->getPopupOpeners();
    CHECK (openers.size() >= 5);

    for (const auto& opener : openers)
    {
        INFO (processor.getName() << ": " << opener.name);

        editor->closePopups();
        opener.open();

        CHECK (somethingOpen (*editor));

        // Nothing went to a JUCE menu in a window of its own.
        CHECK_FALSE (juce::PopupMenu::dismissAllActiveMenus());

        if (auto* menu = MenuLayer::findOpen (editor->getPopupParent()))
        {
            CHECK (menu->getParentComponent() == &editor->getPopupParent());
            CHECK (menu->getAccent() == editor->getAccentColour());
            CHECK (menu->getNumPanels() >= 1);

            // Every panel fits inside the plugin window.
            for (int i = 0; i < menu->getNumPanels(); ++i)
                CHECK (editor->getPopupParent().getLocalBounds().contains (menu->getPanel (i).getBounds()));
        }

        if (auto* dialog = DialogLayer::findOpen (editor->getPopupParent()))
            CHECK (dialog->getParentComponent() == &editor->getPopupParent());
    }

    editor->closePopups();
    processor.editorBeingDeleted (base);
    delete base;
}

const auto plainPreview = [] (auto& editor) { editor.showPreview (false); };

} // namespace

TEST_CASE ("No JUCE menus, alert windows or tooltip windows are left in the plugins", "[popups]")
{
    const juce::File root (FANCY_SOURCE_DIR);
    REQUIRE (root.getChildFile ("fancy-core").isDirectory());

    const juce::StringArray banned { "juce::PopupMenu", "AlertWindow", "NativeMessageBox", "TooltipWindow" };

    for (const auto* folder : { "fancy-core", "plugins" })
    {
        for (const auto& file : root.getChildFile (folder).findChildFiles (juce::File::findFiles, true, "*.cpp;*.h"))
        {
            const auto text = file.loadFileAsString();

            for (const auto& word : banned)
            {
                INFO (file.getFullPathName() << " uses " << word);
                CHECK_FALSE (text.contains (word));
            }
        }
    }
}

TEST_CASE ("Menus move by keyboard, skip disabled rows and open submenus", "[popups]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    TestHost host;
    juce::Component target;
    target.setBounds (10, 10, 120, 24);
    host.addAndMakeVisible (target);

    Menu sub;
    sub.addItem (10, "Ten").addItem (11, "Eleven");

    Menu menu;
    menu.addSection ("Group").addItem (1, "One").addItem (2, "Two", false).addItem (3, "Three").addSeparator().addSubMenu ("Sub", sub);

    auto chosen = -1;
    menu.show (target, [&chosen] (int id) { chosen = id; });

    auto* layer = MenuLayer::findOpen (host);
    REQUIRE (layer != nullptr);
    CHECK (layer->getAccent() == colours::accentYellow);

    // The panel opens 4 px under its target, at least as wide.
    const auto& panel = layer->getPanel (0);
    CHECK (panel.getY() == target.getBottom() + 4);
    CHECK (panel.getX() == target.getX());
    CHECK (panel.getWidth() >= target.getWidth());

    layer->keyPressed (juce::KeyPress (juce::KeyPress::downKey));
    CHECK (layer->getHighlightedId() == 1);

    layer->keyPressed (juce::KeyPress (juce::KeyPress::downKey));
    CHECK (layer->getHighlightedId() == 3); // Two is disabled

    layer->keyPressed (juce::KeyPress (juce::KeyPress::downKey));
    layer->keyPressed (juce::KeyPress (juce::KeyPress::rightKey));
    REQUIRE (layer->getNumPanels() == 2);
    CHECK (layer->getHighlightedId() == 10);

    // The submenu opens beside its row, overlapping the panel by 2 px.
    CHECK (layer->getPanel (1).getX() == layer->getPanel (0).getRight() - 2);

    layer->keyPressed (juce::KeyPress (juce::KeyPress::leftKey));
    CHECK (layer->getNumPanels() == 1);

    layer->keyPressed (juce::KeyPress (juce::KeyPress::upKey));
    CHECK (layer->getHighlightedId() == 3);

    layer->keyPressed (juce::KeyPress (juce::KeyPress::returnKey));
    CHECK (chosen == 3);
    CHECK (MenuLayer::findOpen (host) == nullptr);

    // Esc closes with no choice.
    chosen = -1;
    menu.show (target, [&chosen] (int id) { chosen = id; });
    layer = MenuLayer::findOpen (host);
    REQUIRE (layer != nullptr);
    layer->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
    CHECK (chosen == 0);
}

TEST_CASE ("Long value lists scroll after 12 rows with the current value in view", "[popups]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    TestHost host;
    host.setSize (400, 800);
    juce::Component target;
    target.setBounds (10, 10, 120, 24);
    host.addAndMakeVisible (target);

    Menu menu;
    menu.setScrollingList (true);

    for (int i = 0; i < 128; ++i)
        menu.addItem (1 + i, juce::String (i), true, i == 69);

    menu.show (target, [] (int) {});

    auto* layer = MenuLayer::findOpen (host);
    REQUIRE (layer != nullptr);

    auto& panel = layer->getPanel (0);
    CHECK (panel.isScrolling());
    CHECK ((float) panel.getHeight() == Approx (12.0f * 26.0f + 10.0f).margin (1.0f));

    // The ticked row is inside the visible part.
    const auto top = panel.getRowTop (69);
    CHECK (top >= 0.0f);
    CHECK (top + 26.0f <= (float) panel.getHeight());

    layer->finish (0);
}

TEST_CASE ("Dialogs: Esc cancels, Enter acts, and a check can keep them open", "[popups]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    TestHost host;

    auto result = 0;
    juce::String typed;

    DialogSpec spec;
    spec.title = "Rename preset";
    spec.action = "Rename";
    spec.fields.push_back ({ "Name", "Old" });
    spec.validate = [] (const juce::StringArray& values) { return values[0].isEmpty() ? juce::String ("Type a name.") : juce::String(); };

    showDialog (host, spec, [&] (bool accepted, const juce::StringArray& values)
    {
        result = accepted ? 1 : -1;
        typed = values[0];
    });

    auto* dialog = DialogLayer::findOpen (host);
    REQUIRE (dialog != nullptr);
    REQUIRE (dialog->getField (0) != nullptr);

    dialog->getField (0)->setText ({});
    dialog->keyPressed (juce::KeyPress (juce::KeyPress::returnKey));
    CHECK (result == 0);
    CHECK (dialog->getError().isNotEmpty());

    dialog->getField (0)->setText ("New");
    dialog->keyPressed (juce::KeyPress (juce::KeyPress::returnKey));
    CHECK (result == 1);
    CHECK (typed == "New");
    CHECK (DialogLayer::findOpen (host) == nullptr);

    DialogSpec confirm;
    confirm.title = "Delete user preset?";
    confirm.action = "Delete";
    confirm.destructive = true;
    showDialog (host, confirm, [&] (bool accepted, const juce::StringArray&) { result = accepted ? 2 : -2; });

    dialog = DialogLayer::findOpen (host);
    REQUIRE (dialog != nullptr);
    dialog->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
    CHECK (result == -2);
}

TEST_CASE ("Typed values: units are optional, and text without a value changes nothing", "[popups][entry]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto time = fancy::floatParameter ("release", "Release", juce::NormalisableRange<float> (1.0f, 2000.0f, 0.1f), 50.0f,
                                             juce::AudioParameterFloatAttributes {}
                                                 .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 1) + " ms"; })
                                                 .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseMs (text); }));

    const auto level = fancy::levelParameter ("gain", "Gain", -60.0f, 12.0f, 0.0f, -100.0f);

    const auto frequency = fancy::floatParameter ("freq", "Freq", fancy::logRange (20.0f, 20000.0f, 0.1f), 1000.0f,
                                                  juce::AudioParameterFloatAttributes {}
                                                      .withStringFromValueFunction ([] (float value, int) { return fancy::formatHz (value); })
                                                      .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseHz (text); }));

    struct Case
    {
        juce::RangedAudioParameter* parameter;
        const char* text;
        float expected;
    };

    const Case cases[] {
        { time.get(), "120", 120.0f },
        { time.get(), "120ms", 120.0f },
        { time.get(), "120 ms", 120.0f },
        { time.get(), "0.12 s", 120.0f },
        { time.get(), "0.12s", 120.0f },
        { frequency.get(), "2k", 2000.0f },
        { frequency.get(), "2500", 2500.0f },
        { level.get(), "-6", -6.0f },
        { level.get(), "-6 dB", -6.0f },
        { time.get(), "9000 ms", 2000.0f }, // clamps
    };

    for (const auto& item : cases)
    {
        INFO (item.text);
        Knob knob (*item.parameter, "Value", colours::uiTeal, Knob::mediumStyle);
        juce::Component holder;
        holder.setSize (200, 200);
        knob.setBounds (10, 10, 90, 123);
        holder.addAndMakeVisible (knob);

        knob.openValueEditor();
        REQUIRE (knob.getValueEditor() != nullptr);
        knob.getValueEditor()->setText (item.text);
        knob.getValueEditor()->onReturnKey();

        CHECK (item.parameter->convertFrom0to1 (item.parameter->getValue()) == Approx (item.expected).margin (0.05f));
    }

    for (const auto* text : { "abc", "" })
    {
        INFO ("'" << text << "'");
        time->setValueNotifyingHost (time->convertTo0to1 (50.0f));

        Knob knob (*time, "Release", colours::uiTeal, Knob::mediumStyle);
        juce::Component holder;
        holder.setSize (200, 200);
        knob.setBounds (10, 10, 90, 123);
        holder.addAndMakeVisible (knob);

        knob.openValueEditor();
        REQUIRE (knob.getValueEditor() != nullptr);
        knob.getValueEditor()->setText (text);
        knob.getValueEditor()->onReturnKey();

        CHECK (time->convertFrom0to1 (time->getValue()) == Approx (50.0f));
    }
}

#if FANCY_HAS_DELAY
TEST_CASE ("Delay notes accept a dotted 1/8 typed as 1/8.", "[popups][entry]")
{
    DelayProcessor processor;
    auto& note = *processor.getApvts().getParameter (DelayProcessor::id (0, "note"));

    const auto expected = note.getValueForText ("1/8");
    CHECK (note.getValueForText ("1/8.") == Approx (expected));
    CHECK (note.getCurrentValueAsText().isNotEmpty());
}
#endif

#if FANCY_HAS_KICKER
TEST_CASE ("Presets: Default at defaults, a mark once changed, and reload discards it", "[popups][presets]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    KickerProcessor processor;
    auto& presets = processor.getPresets();

    CHECK (presets.getCurrentName() == "Default");
    CHECK_FALSE (presets.isModified());
    CHECK (presets.getFactoryNames().contains ("Default"));

    auto& wet = *processor.getApvts().getParameter ("wet");
    wet.setValueNotifyingHost (wet.getValue() * 0.5f + 0.1f);
    CHECK (presets.isModified());

    // The mark survives a project save and reload.
    juce::MemoryBlock saved;
    processor.getStateInformation (saved);

    KickerProcessor reopened;
    reopened.setStateInformation (saved.getData(), (int) saved.getSize());
    CHECK (reopened.getPresets().isModified());

    presets.reload();
    CHECK_FALSE (presets.isModified());
    CHECK (presets.getCurrentName() == "Default");

    // Edits outside the parameters count too.
    processor.noteEdited();
    CHECK (presets.isModified());
}

TEST_CASE ("Presets: save, rename and delete a user preset", "[popups][presets]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    KickerProcessor processor;
    auto& presets = processor.getPresets();

    const auto name = "S test " + juce::String (juce::Random::getSystemRandom().nextInt (1000000));
    const auto renamed = name + " renamed";

    REQUIRE (presets.saveUser (name));
    CHECK (presets.isUserPresetLoaded());
    CHECK (presets.getCurrentName() == name);
    CHECK_FALSE (presets.isModified());
    CHECK (presets.nameExists (name));
    CHECK (presets.getUserNames().contains (name));

    REQUIRE (presets.renameUser (renamed));
    CHECK (presets.getCurrentName() == renamed);
    CHECK_FALSE (presets.getUserNames().contains (name));
    CHECK (presets.getUserNames().contains (renamed));

    // A factory name can't be taken.
    CHECK_FALSE (presets.renameUser ("Default"));

    REQUIRE (presets.deleteUser());
    CHECK_FALSE (presets.getUserNames().contains (renamed));
    CHECK_FALSE (presets.isUserPresetLoaded());
    CHECK (presets.getCurrentName() == "Default");
}
#endif

#if FANCY_HAS_KICKER
TEST_CASE ("Kicker: every pop-up opens on the editor", "[popups]") { checkEveryPopup<KickerProcessor, KickerEditor> (plainPreview); }
#endif

#if FANCY_HAS_EXCITER
TEST_CASE ("Exciter: every pop-up opens on the editor", "[popups]") { checkEveryPopup<ExciterProcessor, ExciterEditor> (plainPreview); }
#endif

#if FANCY_HAS_COMP
TEST_CASE ("Comp: every pop-up opens on the editor", "[popups]") { checkEveryPopup<CompProcessor, CompEditor> (plainPreview); }
#endif

#if FANCY_HAS_EQ
TEST_CASE ("EQ: every pop-up opens on the editor", "[popups]") { checkEveryPopup<EqProcessor, EqEditor> (plainPreview); }
#endif

#if FANCY_HAS_VERB
TEST_CASE ("Verb: every pop-up opens on the editor", "[popups]")
{
    checkEveryPopup<VerbProcessor, VerbEditor> ([] (VerbEditor& editor) { editor.showPreview (VerbEditor::Preview::rack); });
}
#endif

#if FANCY_HAS_DELAY
TEST_CASE ("Delay: every pop-up opens on the editor", "[popups]") { checkEveryPopup<DelayProcessor, DelayEditor> (plainPreview); }
#endif

#if FANCY_HAS_MULTICOMP
TEST_CASE ("MultiComp: every pop-up opens on the editor", "[popups]") { checkEveryPopup<MultiCompProcessor, MultiCompEditor> (plainPreview); }
#endif

#if FANCY_HAS_FIR
TEST_CASE ("FIR: every pop-up opens on the editor", "[popups]")
{
    checkEveryPopup<FirProcessor, FirEditor> ([] (FirEditor& editor) { editor.showPreview (FirEditor::Preview::eqCurve); });
}
#endif

#if FANCY_HAS_GATE
TEST_CASE ("Gate: every pop-up opens on the editor", "[popups]") { checkEveryPopup<GateProcessor, GateEditor> (plainPreview); }
#endif
