// Renders each plugin's editor offscreen to PNG at 100 % and 200 %, plus a gallery of the
// fancy-core controls that the finished plugins don't exercise yet. With a second directory,
// it also renders every pop-up each plugin lists (Phase 2.1 item S) into it. With a third, it
// renders each editor, and its preset menu, at the host scales 1.25, 1.5 and 2.0 (item H.3), and
// each window at its smallest and largest size (Phase 3 Q2.1).
// Usage: fancy-screens <output-directory> [<popups-directory> [<host-scale-directory>]]

#include <fancy/ui/Controls.h>
#include <fancy/ui/ShellEditor.h>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <iostream>
#include <memory>

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
#if FANCY_HAS_ROOM
 #include "RoomEditor.h"
 #include "RoomProcessor.h"
#endif

namespace
{

bool writeSnapshot (juce::Component& component, float scale, const juce::File& file)
{
    const auto image = component.createComponentSnapshot (component.getLocalBounds(), true, scale);

    file.deleteFile();
    juce::FileOutputStream stream (file);

    if (! stream.openedOk())
        return false;

    juce::PNGImageFormat png;
    return png.writeImageToStream (image, stream);
}

bool writeBothScales (juce::Component& component, const juce::String& name, const juce::File& outputDir)
{
    return writeSnapshot (component, 1.0f, outputDir.getChildFile (name + "@1x.png"))
        && writeSnapshot (component, 2.0f, outputDir.getChildFile (name + "@2x.png"));
}

#if FANCY_HAS_KICKER
// Kicker's Sub mode (docs/kicker-sub.md), beside its mockup.
bool renderKickerSub (const juce::File& outputDir)
{
    KickerProcessor processor;
    const std::unique_ptr<juce::AudioProcessorEditor> base (processor.createEditorAndMakeActive());
    auto* editor = dynamic_cast<KickerEditor*> (base.get());

    if (editor == nullptr)
        return false;

    editor->showPreview (false, true);
    const auto mainOk = writeBothScales (*editor, "kicker-sub", outputDir);

    editor->showPreview (true, true);
    return writeBothScales (*editor, "kicker-sub-edit", outputDir) && mainOk;
}
#endif

template <typename Processor, typename Editor>
bool renderPlugin (const juce::String& slug, const juce::File& outputDir)
{
    Processor processor;
    const std::unique_ptr<juce::AudioProcessorEditor> base (processor.createEditorAndMakeActive());
    auto* editor = dynamic_cast<Editor*> (base.get());

    if (editor == nullptr)
        return false;

    editor->showPreview (false);
    const auto mainOk = writeBothScales (*editor, slug, outputDir);

    editor->showPreview (true);
    return writeBothScales (*editor, slug + "-edit", outputDir) && mainOk;
}

// The main view with the listen or preview switch on: the filled checkbox and the title strip's
// LISTENING tag (§0.3 of docs/BUILD_PROMPT_PHASE3_NEW_PLUGINS.md).
template <typename Processor, typename Editor>
bool renderListening (const juce::String& slug, const juce::File& outputDir)
{
    Processor processor;
    const std::unique_ptr<juce::AudioProcessorEditor> base (processor.createEditorAndMakeActive());
    auto* editor = dynamic_cast<Editor*> (base.get());

    if (editor == nullptr)
        return false;

    processor.setListening (true);
    editor->showPreview (false);
    return writeBothScales (*editor, slug + "-listening", outputDir);
}

#if FANCY_HAS_EQ
bool renderEq (const juce::File& outputDir)
{
    EqProcessor processor;
    const std::unique_ptr<juce::AudioProcessorEditor> base (processor.createEditorAndMakeActive());
    auto* editor = dynamic_cast<EqEditor*> (base.get());

    if (editor == nullptr)
        return false;

    editor->showPreview (false);
    auto ok = writeBothScales (*editor, "eq", outputDir);

    editor->showPreview (true);
    ok = writeBothScales (*editor, "eq-edit", outputDir) && ok;

    editor->showCompactPreview();
    ok = writeBothScales (*editor, "eq-compact", outputDir) && ok;

    // Phase 2.1 D: a dynamic band, in the normal layout and in the Compact-mode card.
    editor->showDynamicPreview (false);
    ok = writeBothScales (*editor, "eq-dynamic", outputDir) && ok;

    editor->showDynamicPreview (true);
    return writeBothScales (*editor, "eq-dynamic-compact", outputDir) && ok;
}
#endif

#if FANCY_HAS_VERB
bool renderVerb (const juce::File& outputDir)
{
    VerbProcessor processor;
    const std::unique_ptr<juce::AudioProcessorEditor> base (processor.createEditorAndMakeActive());
    auto* editor = dynamic_cast<VerbEditor*> (base.get());

    if (editor == nullptr)
        return false;

    editor->showEmptyPreview();
    auto ok = writeBothScales (*editor, "verb-empty", outputDir);

    editor->showPreview (VerbEditor::Preview::rack);
    ok = writeBothScales (*editor, "verb", outputDir) && ok;

    editor->showPreview (VerbEditor::Preview::addPanel);
    ok = writeBothScales (*editor, "verb-add", outputDir) && ok;

    editor->showPreview (VerbEditor::Preview::setup);
    ok = writeBothScales (*editor, "verb-edit", outputDir) && ok;

    editor->showPreview (VerbEditor::Preview::scrolled);
    return writeBothScales (*editor, "verb-scrolled", outputDir) && ok;
}
#endif

#if FANCY_HAS_FIR
bool renderFir (const juce::File& outputDir)
{
    FirProcessor processor;
    const std::unique_ptr<juce::AudioProcessorEditor> base (processor.createEditorAndMakeActive());
    auto* editor = dynamic_cast<FirEditor*> (base.get());

    if (editor == nullptr)
        return false;

    editor->showPreview (FirEditor::Preview::subtractLearning);
    auto ok = writeBothScales (*editor, "fir", outputDir);

    editor->showPreview (FirEditor::Preview::eqCurve);
    ok = writeBothScales (*editor, "fir-eq", outputDir) && ok;

    editor->showPreview (FirEditor::Preview::drawer);
    ok = writeBothScales (*editor, "fir-edit", outputDir) && ok;

    // Phase 2.1 A: Adaptive, as panel 5 of the mockup.
    editor->showPreview (FirEditor::Preview::subtractAdaptive);
    return writeBothScales (*editor, "fir-adaptive", outputDir) && ok;
}
#endif

// Opens each pop-up the editor lists, and saves the window with it open.
template <typename Processor, typename Editor, typename Setup>
bool renderPopups (const juce::String& slug, const juce::File& outputDir, Setup setup)
{
    Processor processor;
    const std::unique_ptr<juce::AudioProcessorEditor> base (processor.createEditorAndMakeActive());
    auto* editor = dynamic_cast<Editor*> (base.get());

    if (editor == nullptr)
        return false;

    setup (*editor);

    auto ok = true;

    for (const auto& opener : editor->getPopupOpeners())
    {
        editor->closePopups();
        opener.open();

        // Shows the hover state on the row after the ticked one, as the mockup does.
        if (auto* menu = fancy::ui::MenuLayer::findOpen (editor->getPopupParent()))
        {
            auto& panel = menu->getPanel (0);
            const auto& items = panel.getMenu().getItems();

            if (panel.getHighlightedRow() < 0)
            {
                for (int row = 0; row < (int) items.size(); ++row)
                    if (items[(size_t) row].ticked)
                        panel.setHighlightedRow (row);

                panel.moveHighlight (1);
            }
        }

        if (! writeBothScales (*editor, slug + "-" + opener.name, outputDir))
        {
            std::cerr << "cannot render " << slug << "-" << opener.name << "\n";
            ok = false;
        }
    }

    editor->closePopups();
    return ok;
}

// The host's content scale on top of the plugin's own size, as a VST3 host on a high-DPI
// screen sets it.
template <typename Processor, typename Editor, typename Setup>
bool renderHostScales (const juce::String& slug, const juce::File& outputDir, Setup setup)
{
    Processor processor;
    const std::unique_ptr<juce::AudioProcessorEditor> base (processor.createEditorAndMakeActive());
    auto* editor = dynamic_cast<Editor*> (base.get());

    if (editor == nullptr)
        return false;

    setup (*editor);

    auto ok = true;

    for (const auto scale : { 1.25f, 1.5f, 2.0f })
    {
        const auto suffix = "@host" + juce::String (juce::roundToInt (scale * 100.0f));

        editor->setScaleFactor (scale);
        editor->closePopups();
        ok = writeSnapshot (*editor, scale, outputDir.getChildFile (slug + suffix + ".png")) && ok;

        editor->showPresetMenu();
        ok = writeSnapshot (*editor, scale, outputDir.getChildFile (slug + "-preset-menu" + suffix + ".png")) && ok;
    }

    // Phase 3 Q2.1: the window at its smallest and largest, as dragging its edge leaves it.
    editor->setScaleFactor (1.0f);
    editor->closePopups();

    if (auto* constrainer = editor->getConstrainer())
    {
        editor->setSize (constrainer->getMinimumWidth(), constrainer->getMinimumHeight());
        ok = writeSnapshot (*editor, 1.0f, outputDir.getChildFile (slug + "@min.png")) && ok;

        editor->setSize (constrainer->getMaximumWidth(), constrainer->getMaximumHeight());
        ok = writeSnapshot (*editor, 1.0f, outputDir.getChildFile (slug + "@max.png")) && ok;

        editor->showPresetMenu();
        ok = writeSnapshot (*editor, 1.0f, outputDir.getChildFile (slug + "-preset-menu@max.png")) && ok;
    }

    editor->closePopups();
    return ok;
}

bool renderAllHostScales (const juce::File& outputDir)
{
    auto ok = true;
    const auto plain = [] (auto& editor) { editor.showPreview (false); };
    juce::ignoreUnused (plain);

   #if FANCY_HAS_KICKER
    ok = renderHostScales<KickerProcessor, KickerEditor> ("kicker", outputDir, plain) && ok;
   #endif

   #if FANCY_HAS_EXCITER
    ok = renderHostScales<ExciterProcessor, ExciterEditor> ("exciter", outputDir, plain) && ok;
   #endif

   #if FANCY_HAS_COMP
    ok = renderHostScales<CompProcessor, CompEditor> ("comp", outputDir, plain) && ok;
   #endif

   #if FANCY_HAS_EQ
    ok = renderHostScales<EqProcessor, EqEditor> ("eq", outputDir, plain) && ok;
   #endif

   #if FANCY_HAS_VERB
    ok = renderHostScales<VerbProcessor, VerbEditor> ("verb", outputDir, [] (VerbEditor& editor) { editor.showPreview (VerbEditor::Preview::rack); }) && ok;
   #endif

   #if FANCY_HAS_DELAY
    ok = renderHostScales<DelayProcessor, DelayEditor> ("delay", outputDir, plain) && ok;
   #endif

   #if FANCY_HAS_MULTICOMP
    ok = renderHostScales<MultiCompProcessor, MultiCompEditor> ("multicomp", outputDir, plain) && ok;
   #endif

   #if FANCY_HAS_FIR
    ok = renderHostScales<FirProcessor, FirEditor> ("fir", outputDir, [] (FirEditor& editor) { editor.showPreview (FirEditor::Preview::eqCurve); }) && ok;
   #endif

   #if FANCY_HAS_GATE
    ok = renderHostScales<GateProcessor, GateEditor> ("gate", outputDir, plain) && ok;
   #endif

   #if FANCY_HAS_LIMIT
    ok = renderHostScales<LimitProcessor, LimitEditor> ("limit", outputDir, plain) && ok;
   #endif

   #if FANCY_HAS_DS
    ok = renderHostScales<DsProcessor, DsEditor> ("ds", outputDir, plain) && ok;
   #endif
   #if FANCY_HAS_DSMINI
    ok = renderHostScales<DsMiniProcessor, DsMiniEditor> ("dsmini", outputDir, plain) && ok;
   #endif
   #if FANCY_HAS_SATURE
    ok = renderHostScales<SatureProcessor, SatureEditor> ("sature", outputDir, plain) && ok;
   #endif
   #if FANCY_HAS_ROOM
    ok = renderHostScales<RoomProcessor, RoomEditor> ("room", outputDir, plain) && ok;
   #endif

    return ok;
}

bool renderAllPopups (const juce::File& outputDir)
{
    auto ok = true;
    const auto plain = [] (auto& editor) { editor.showPreview (false); };
    juce::ignoreUnused (plain);

   #if FANCY_HAS_KICKER
    ok = renderPopups<KickerProcessor, KickerEditor> ("kicker", outputDir, plain) && ok;
   #endif

   #if FANCY_HAS_EXCITER
    ok = renderPopups<ExciterProcessor, ExciterEditor> ("exciter", outputDir, plain) && ok;
   #endif

   #if FANCY_HAS_COMP
    ok = renderPopups<CompProcessor, CompEditor> ("comp", outputDir, plain) && ok;
   #endif

   #if FANCY_HAS_EQ
    ok = renderPopups<EqProcessor, EqEditor> ("eq", outputDir, plain) && ok;
   #endif

   #if FANCY_HAS_VERB
    ok = renderPopups<VerbProcessor, VerbEditor> ("verb", outputDir, [] (VerbEditor& editor) { editor.showPreview (VerbEditor::Preview::rack); }) && ok;
   #endif

   #if FANCY_HAS_DELAY
    ok = renderPopups<DelayProcessor, DelayEditor> ("delay", outputDir, plain) && ok;
   #endif

   #if FANCY_HAS_MULTICOMP
    ok = renderPopups<MultiCompProcessor, MultiCompEditor> ("multicomp", outputDir, plain) && ok;
   #endif

   #if FANCY_HAS_FIR
    ok = renderPopups<FirProcessor, FirEditor> ("fir", outputDir, [] (FirEditor& editor) { editor.showPreview (FirEditor::Preview::eqCurve); }) && ok;
   #endif

   #if FANCY_HAS_GATE
    ok = renderPopups<GateProcessor, GateEditor> ("gate", outputDir, plain) && ok;
   #endif

   #if FANCY_HAS_LIMIT
    ok = renderPopups<LimitProcessor, LimitEditor> ("limit", outputDir, plain) && ok;
   #endif

   #if FANCY_HAS_DS
    ok = renderPopups<DsProcessor, DsEditor> ("ds", outputDir, plain) && ok;
   #endif
   #if FANCY_HAS_DSMINI
    ok = renderPopups<DsMiniProcessor, DsMiniEditor> ("dsmini", outputDir, plain) && ok;
   #endif
   #if FANCY_HAS_SATURE
    ok = renderPopups<SatureProcessor, SatureEditor> ("sature", outputDir, plain) && ok;
   #endif
   #if FANCY_HAS_ROOM
    ok = renderPopups<RoomProcessor, RoomEditor> ("room", outputDir, plain) && ok;
   #endif

    return ok;
}

class Gallery final : public juce::Component
{
public:
    Gallery()
    {
        using namespace fancy::ui;

        for (juce::Component* item : std::initializer_list<juce::Component*> { &wetFader, &dryFader, &onBox, &offBox, &dropdown, &steppedDropdown, &tealKnob, &blueKnob, &smallKnob })
            addAndMakeVisible (item);

        wetFader.setBounds (16, 16, 44, 300);
        dryFader.setBounds (68, 16, 44, 300);
        tealKnob.setBounds (128, 16, 90, 123);
        blueKnob.setBounds (228, 16, 90, 123);
        smallKnob.setBounds (328, 16, 62, 99);
        onBox.setBounds (136, 170, 100, 20);
        offBox.setBounds (246, 170, 100, 20);
        dropdown.setBounds (136, 204, 180, 24);
        steppedDropdown.setStepArrows (true);
        steppedDropdown.setBounds (136, 240, 238, 26);

        setSize (406, 332);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (fancy::ui::colours::bgBody);
    }

private:
    juce::AudioParameterFloat wet { juce::ParameterID { "wet", 1 }, "Wet", -60.0f, 12.0f, -12.2f };
    juce::AudioParameterFloat dry { juce::ParameterID { "dry", 1 }, "Dry", -60.0f, 12.0f, -5.3f };
    juce::AudioParameterFloat ratio { juce::ParameterID { "ratio", 1 }, "Ratio", 1.0f, 20.0f, 4.0f };
    juce::AudioParameterFloat gain { juce::ParameterID { "gain", 1 }, "Gain", -18.0f, 18.0f, 6.0f };
    juce::AudioParameterFloat width { juce::ParameterID { "width", 1 }, "Width", 0.0f, 100.0f, 100.0f };
    juce::AudioParameterBool on { juce::ParameterID { "on", 1 }, "Weird", true };
    juce::AudioParameterBool off { juce::ParameterID { "off", 1 }, "Classic", false };
    juce::AudioParameterChoice detector { juce::ParameterID { "detector", 1 }, "Detector", { "Main input L+R", "Main L", "Main R" }, 0 };

    fancy::ui::Fader wetFader { wet, fancy::ui::colours::uiTeal }, dryFader { dry, fancy::ui::colours::uiTeal };
    fancy::ui::Knob tealKnob { ratio, "Ratio", fancy::ui::colours::uiTeal, fancy::ui::Knob::mediumStyle };
    fancy::ui::Knob blueKnob { gain, "Bipolar", fancy::ui::colours::uiBlue, fancy::ui::Knob::mediumStyle, true };
    fancy::ui::Knob smallKnob { width, "Width", fancy::ui::colours::uiTeal, fancy::ui::Knob::smallStyle };
    fancy::ui::ParamCheckbox onBox { on, "Weird" }, offBox { off, "Classic" };
    fancy::ui::ParamDropdown dropdown { detector };

    // The step-arrow variant (Fancy DS-Mini's profile).
    juce::AudioParameterChoice profile { juce::ParameterID { "profile", 1 }, "Profile", { "Female voice", "Male voice", "Sibilance" }, 0 };
    fancy::ui::ParamDropdown steppedDropdown { profile };
};

} // namespace

int main (int argc, char** argv)
{
    if (argc < 2 || argc > 4)
    {
        std::cerr << "usage: fancy-screens <output-directory> [<popups-directory>]\n";
        return 2;
    }

    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const juce::SharedResourcePointer<fancy::ui::Typefaces> typefaces;

    const auto outputDir = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);

    if (! outputDir.createDirectory())
    {
        std::cerr << "cannot create " << argv[1] << "\n";
        return 1;
    }

    bool ok = true;

   #if FANCY_HAS_KICKER
    ok = renderPlugin<KickerProcessor, KickerEditor> ("kicker", outputDir) && ok;
    ok = renderKickerSub (outputDir) && ok;
   #endif

   #if FANCY_HAS_EXCITER
    ok = renderPlugin<ExciterProcessor, ExciterEditor> ("exciter", outputDir) && ok;
   #endif

   #if FANCY_HAS_COMP
    ok = renderPlugin<CompProcessor, CompEditor> ("comp", outputDir) && ok;
    ok = renderListening<CompProcessor, CompEditor> ("comp", outputDir) && ok;
   #endif

   #if FANCY_HAS_EQ
    ok = renderEq (outputDir) && ok;
   #endif

   #if FANCY_HAS_VERB
    ok = renderVerb (outputDir) && ok;
   #endif

   #if FANCY_HAS_DELAY
    ok = renderPlugin<DelayProcessor, DelayEditor> ("delay", outputDir) && ok;
   #endif

   #if FANCY_HAS_MULTICOMP
    ok = renderPlugin<MultiCompProcessor, MultiCompEditor> ("multicomp", outputDir) && ok;
   #endif

   #if FANCY_HAS_FIR
    ok = renderFir (outputDir) && ok;
   #endif

   #if FANCY_HAS_GATE
    ok = renderPlugin<GateProcessor, GateEditor> ("gate", outputDir) && ok;
    ok = renderListening<GateProcessor, GateEditor> ("gate", outputDir) && ok;
   #endif

   #if FANCY_HAS_LIMIT
    ok = renderPlugin<LimitProcessor, LimitEditor> ("limit", outputDir) && ok;
   #endif

   #if FANCY_HAS_DS
    ok = renderPlugin<DsProcessor, DsEditor> ("ds", outputDir) && ok;
    ok = renderListening<DsProcessor, DsEditor> ("ds", outputDir) && ok;
   #endif
   #if FANCY_HAS_DSMINI
    ok = renderPlugin<DsMiniProcessor, DsMiniEditor> ("dsmini", outputDir) && ok;
    ok = renderListening<DsMiniProcessor, DsMiniEditor> ("dsmini", outputDir) && ok;
   #endif
   #if FANCY_HAS_SATURE
    ok = renderPlugin<SatureProcessor, SatureEditor> ("sature", outputDir) && ok;
    ok = renderListening<SatureProcessor, SatureEditor> ("sature", outputDir) && ok;
   #endif
   #if FANCY_HAS_ROOM
    ok = renderPlugin<RoomProcessor, RoomEditor> ("room", outputDir) && ok;
   #endif

    Gallery gallery;
    ok = writeBothScales (gallery, "gallery", outputDir) && ok;

    if (argc >= 3)
    {
        const auto popupsDir = juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]);

        if (! popupsDir.createDirectory())
        {
            std::cerr << "cannot create " << argv[2] << "\n";
            return 1;
        }

        ok = renderAllPopups (popupsDir) && ok;
    }

    if (argc == 4)
    {
        const auto hostScaleDir = juce::File::getCurrentWorkingDirectory().getChildFile (argv[3]);

        if (! hostScaleDir.createDirectory())
            return 1;

        ok = renderAllHostScales (hostScaleDir) && ok;
    }

    return ok ? 0 : 1;
}
