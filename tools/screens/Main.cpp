// Renders each plugin's editor offscreen to PNG at 100 % and 200 %, plus a gallery of the
// fancy-core controls that the finished plugins don't exercise yet.
// Usage: fancy-screens <output-directory>

#include <fancy/ui/Controls.h>

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

class Gallery final : public juce::Component
{
public:
    Gallery()
    {
        using namespace fancy::ui;

        for (juce::Component* item : std::initializer_list<juce::Component*> { &wetFader, &dryFader, &onBox, &offBox, &dropdown, &tealKnob, &blueKnob, &smallKnob })
            addAndMakeVisible (item);

        wetFader.setBounds (16, 16, 44, 300);
        dryFader.setBounds (68, 16, 44, 300);
        tealKnob.setBounds (128, 16, 90, 123);
        blueKnob.setBounds (228, 16, 90, 123);
        smallKnob.setBounds (328, 16, 62, 99);
        onBox.setBounds (136, 170, 100, 20);
        offBox.setBounds (246, 170, 100, 20);
        dropdown.setBounds (136, 204, 180, 24);

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
};

} // namespace

int main (int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: fancy-screens <output-directory>\n";
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
   #endif

   #if FANCY_HAS_EXCITER
    ok = renderPlugin<ExciterProcessor, ExciterEditor> ("exciter", outputDir) && ok;
   #endif

    Gallery gallery;
    ok = writeBothScales (gallery, "gallery", outputDir) && ok;

    return ok ? 0 : 1;
}
