// Renders each plugin's editor offscreen to PNG at 100 % and 200 %.
// Usage: fancy-screens <output-directory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <iostream>
#include <memory>

#if FANCY_HAS_KICKER
 #include "KickerProcessor.h"
#endif

namespace
{

bool writeSnapshot (juce::Component& editor, float scale, const juce::File& file)
{
    const auto image = editor.createComponentSnapshot (editor.getLocalBounds(), true, scale);

    file.deleteFile();
    juce::FileOutputStream stream (file);

    if (! stream.openedOk())
        return false;

    juce::PNGImageFormat png;
    return png.writeImageToStream (image, stream);
}

template <typename Processor>
bool renderPlugin (const juce::String& slug, const juce::File& outputDir)
{
    Processor processor;
    const std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());

    if (editor == nullptr)
        return false;

    return writeSnapshot (*editor, 1.0f, outputDir.getChildFile (slug + "@1x.png"))
        && writeSnapshot (*editor, 2.0f, outputDir.getChildFile (slug + "@2x.png"));
}

} // namespace

int main (int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: fancy-screens <output-directory>\n";
        return 2;
    }

    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto outputDir = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);

    if (! outputDir.createDirectory())
    {
        std::cerr << "cannot create " << argv[1] << "\n";
        return 1;
    }

    bool ok = true;

   #if FANCY_HAS_KICKER
    ok = renderPlugin<KickerProcessor> ("kicker", outputDir) && ok;
   #endif

    return ok ? 0 : 1;
}
