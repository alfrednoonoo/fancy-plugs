#include "VerbEditor.h"
#include "LibraryPicker.h"

#include <cmath>

using namespace fancy::ui;
using verb::ModuleType;

namespace
{
// Positions from the Reaverb1 frame in design/figma-spec.json, with the header removed.
constexpr int displayHeight = 200, rackWidth = 734, stripHeight = 72, columnWidth = 132;

void styleLabel (juce::Label& label, const juce::String& text, juce::Colour colour, juce::Justification justification, float size = 14.0f)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (font (Weight::regular, size));
    label.setColour (juce::Label::textColourId, colour);
    label.setJustificationType (justification);
    label.setBorderSize ({});
}

// A stand-in for Figma's reverb file, so the screenshot runs through the real file path.
juce::File writePreviewFile()
{
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("Users/me/Music/VS8F Reverb IR/VS8F-1/100-Reverb.wav");

    file.getParentDirectory().createDirectory();
    file.deleteFile();

    constexpr double rate = 48000.0;
    constexpr int length = (int) (4.5 * rate);

    juce::AudioBuffer<float> audio (2, length);
    juce::Random random (42);

    for (int i = 0; i < length; ++i)
    {
        const auto remaining = juce::jmax (0.0f, 1.0f - (float) i / (float) (3.0 * rate));
        const auto envelope = remaining * remaining * (0.6f + 0.4f * (float) (i % 4800) / 4800.0f);
        audio.setSample (0, i, envelope * (random.nextFloat() * 2.0f - 1.0f));
        audio.setSample (1, i, envelope * (random.nextFloat() * 2.0f - 1.0f));
    }

    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);

    juce::WavAudioFormat format;
    const auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions {}
                                                            .withSampleRate (rate)
                                                            .withNumChannels (2)
                                                            .withBitsPerSample (24));

    if (writer != nullptr)
        writer->writeFromAudioSampleBuffer (audio, 0, length);

    return file;
}
} // namespace

VerbEditor::VerbEditor (VerbProcessor& processorToUse)
    : ShellEditor (processorToUse, baseWidth, baseHeight, HeaderStyle::full),
      processor (processorToUse),
      rack (processorToUse),
      strip (processorToUse, rack),
      predelayKnob (parameter ("predelay"), "Pre-delay", colours::uiTeal, Knob::smallStyle),
      centerKnob (parameter ("center"), {}, colours::uiTeal, Knob::headerStyle, true),
      widthKnob (parameter ("width"), {}, colours::uiTeal, Knob::headerStyle),
      wetFader (parameter ("wet"), colours::uiTeal),
      dryFader (parameter ("dry"), colours::uiTeal),
      latencyDropdown (parameter ("latency")),
      fftDropdown (parameter ("maxFft")),
      storageDropdown (parameter ("irStorage"))
{
    // Phase 2.1 W.1: room in the header for undo, redo, A/B and Compare.
    setPresetDropdownWidth (230);

    styleLabel (centerLabel, {}, colours::uiTeal, juce::Justification::centred, 12.0f);
    styleLabel (widthLabel, {}, colours::uiTeal, juce::Justification::centred, 12.0f);
    styleLabel (wetLabel, "Wet", colours::textSecondary, juce::Justification::centred);
    styleLabel (dryLabel, "Dry", colours::textSecondary, juce::Justification::centred);

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &columnPanel, &display, &rack, &strip, &predelayKnob, &centerLabel, &widthLabel, &centerKnob, &widthKnob,
             &wetLabel, &dryLabel, &wetFader, &dryFader, &setupButton })
        getBody().addAndMakeVisible (item);

    rack.onLayoutChanged = [this] { strip.repaint(); };

    display.onTrimChanged = [this] (double start, double end) { processor.setTrim (start, end); };

    // Phase 4 Wave 1: Export IR from the display's right-click menu, and the rack's messages.
    display.onContextMenu = [this] (juce::Point<float> position) { showDisplayMenu (position); };
    rack.onMessage = [this] (const juce::String& text) { showTip (rack, text); };

    setupButton.onClick = [this] { drawer.setVisible (! drawer.isVisible()); };

    const std::pair<juce::Label*, const char*> labels[] {
        { &latencyLabel, "Latency" }, { &fftLabel, "Max FFT" }, { &storageLabel, "IR storage" }, { &folderLabel, "IR folder" }
    };

    constexpr int labelX = 24, controlX = 124, rowHeight = 24, firstRow = 52, rowStep = 34;

    const auto row = [] (int index) { return firstRow + index * rowStep; };

    for (int i = 0; i < 4; ++i)
    {
        styleLabel (*labels[i].first, labels[i].second, colours::textPrimary, juce::Justification::centredLeft);
        labels[i].first->setBounds (labelX, row (i), 96, rowHeight);
        drawer.addAndMakeVisible (labels[i].first);
    }

    latencyDropdown.setBounds (controlX, row (0), 170, rowHeight);
    fftDropdown.setBounds (controlX, row (1), 170, rowHeight);
    storageDropdown.setBounds (controlX, row (2), 170, rowHeight);
    folderButton.setBounds (controlX, row (3), 90, rowHeight);

    styleLabel (folderPath, {}, colours::textSecondary, juce::Justification::centredLeft, 12.0f);
    folderPath.setBounds (controlX + 100, row (3), rackWidth - controlX - 124, rowHeight);

    styleLabel (lateLabel, {}, colours::textSecondary, juce::Justification::centredLeft, 12.0f);
    lateLabel.setBounds (labelX, row (4), rackWidth - 48, rowHeight);

    folderButton.onClick = [this] { chooseIrFolder(); };
    addLatencyNote (latencyDropdown);

    // Phase 4 Wave 1 (docs/verb.md section 1): Equal loudness, with the applied trim beside it.
    loudnessToggle.setBounds (labelX, row (5), 150, rowHeight);
    loudnessToggle.onChange = [this] (bool on) { processor.setEqualLoudness (on); };
    styleLabel (loudnessTrim, {}, colours::textSecondary, juce::Justification::centredLeft, 12.0f);
    loudnessTrim.setBounds (labelX + 160, row (5), 200, rowHeight);
    loudnessTrim.setTooltip ("The gain Equal loudness applied, so this IR plays at the same loudness as any other.");

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &latencyDropdown, &fftDropdown, &storageDropdown, &folderButton, &folderPath, &lateLabel, &loudnessToggle, &loudnessTrim })
        drawer.addAndMakeVisible (item);

    getBody().addChildComponent (drawer);

    bodyResized();
    onTick (0.0f);
}

void VerbEditor::bodyResized()
{
    display.setBounds (0, 0, rackWidth, displayHeight);
    rack.setBounds (0, displayHeight, rackWidth, ModuleCard::height);
    strip.setBounds (0, displayHeight + ModuleCard::height, rackWidth, stripHeight);

    drawer.setBounds (rack.getBounds());
    drawer.toFront (false);

    const auto column = rackWidth;

    columnPanel.setBounds (column, 0, columnWidth, baseHeight - headerHeight);

    predelayKnob.setBounds (column + 33, 24, 66, 99);
    centerLabel.setBounds (column + 18, 147, 52, 14);
    widthLabel.setBounds (column + 66, 147, 50, 14);
    centerKnob.setBounds (column + 29, 167, 28, 28);
    widthKnob.setBounds (column + 78, 167, 28, 28);
    wetLabel.setBounds (column + 21, 219, 44, 17);
    dryLabel.setBounds (column + 67, 219, 44, 17);
    wetFader.setBounds (column + 21, 244, 44, 308);
    dryFader.setBounds (column + 67, 244, 44, 308);
    setupButton.setBounds (column + 25, 580, 82, 24);
}

// Esc closes the add panel (Phase 3 Q2.8); every other key goes on as before.
bool VerbEditor::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && rack.isAddPanelOpen())
    {
        rack.hideAddPanel();
        return true;
    }

    return ShellEditor::keyPressed (key);
}

bool VerbEditor::listsParameter (const juce::AudioProcessorParameter& candidate)
{
    return candidate.isAutomatable();
}

void VerbEditor::updateDisplay()
{
    const auto chain = processor.getChain();
    const auto preview = processor.getPreview();
    display.setPreview (preview, chain[verb::ids::trimStart], chain[verb::ids::trimEnd]);
    rack.refresh();

    const auto trim = preview.loudnessTrimDb;
    loudnessTrim.setText (processor.getEqualLoudness() ? juce::String (trim > 0.0 ? "+" : "") + juce::String (trim, 1) + " dB" : juce::String(),
                          juce::dontSendNotification);
}

void VerbEditor::showDisplayMenu (juce::Point<float> position)
{
    const auto chain = processor.getChain();
    const auto trimmed = (double) chain[verb::ids::trimStart] > 0.0 || (double) chain[verb::ids::trimEnd] > 0.0;

    fancy::ui::Menu menu;
    menu.addItem (1, "Export IR...", processor.getPreview().trimmedSeconds > 0.0);
    menu.addItem (2, "Reset trim", trimmed);

    menu.showAt (display, position, [safeThis = juce::Component::SafePointer<VerbEditor> (this)] (int chosen)
    {
        if (safeThis == nullptr)
            return;

        if (chosen == 1)
            safeThis->rack.exportIr();
        else if (chosen == 2)
            safeThis->processor.setTrim (0.0, 0.0);
    });
}

void VerbEditor::onTick (float)
{
    centerLabel.setText (parameter ("center").getCurrentValueAsText(), juce::dontSendNotification);
    widthLabel.setText (parameter ("width").getCurrentValueAsText(), juce::dontSendNotification);

    if (previewFrozen)
        return;

    if (const auto revision = processor.getChainRevision(); revision != seenChainRevision)
    {
        seenChainRevision = revision;
        rack.rebuild();
    }

    if (const auto revision = processor.getBuildRevision(); revision != seenBuildRevision)
    {
        seenBuildRevision = revision;
        updateDisplay();
    }

    strip.repaint();

    folderPath.setText (processor.getIrFolder().getFullPathName(), juce::dontSendNotification);
    loudnessToggle.setOn (processor.getEqualLoudness());
    lateLabel.setText ("Late blocks since reset: " + juce::String (processor.getLateBlocks()), juce::dontSendNotification);
}

void VerbEditor::chooseIrFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose your IR folder", processor.getIrFolder());

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& result)
                          {
                              if (result.getResult().isDirectory())
                                  processor.setIrFolder (result.getResult());
                          });
}

void VerbEditor::setValue (const char* id, float value)
{
    auto& target = parameter (id);
    target.setValueNotifyingHost (target.convertTo0to1 (value));
}

void VerbEditor::showEmptyPreview()
{
    stopUpdates();
    previewFrozen = true;

    setValue ("predelay", 3.0f);
    setValue ("wet", -12.2f);
    setValue ("dry", -5.3f);

    processor.waitForBuild (20000);
    rack.rebuild();
    updateDisplay();
    onTick (0.0f);
}

void VerbEditor::showPreview (Preview preview)
{
    stopUpdates();
    previewFrozen = true;

    if (processor.getChain().getNumChildren() == 0)
    {
        const auto file = processor.addModule (ModuleType::file, 0);
        processor.loadFile (file, writePreviewFile());

        const auto filter = processor.addModule (ModuleType::lpHp, 1);
        processor.setModuleValue (filter, "highpass", 93.0f);

        const auto echo = processor.addModule (ModuleType::echo, 2);

        const std::pair<const char*, float> echoValues[] {
            { "start", 5.2f }, { "length", 99.0f }, { "spacing", 30.3f }, { "error", 0.0f },
            { "errdst", 0.35f }, { "startVol", -8.4f }, { "endVol", -14.7f }
        };

        for (const auto& [id, value] : echoValues)
            processor.setModuleValue (echo, id, value);

        const auto normalize = processor.addModule (ModuleType::normalize, 3);
        processor.setModuleValue (normalize, "peak", -34.3f);
    }

    closePopups();
    rack.fileDragExit ({});

    // Phase 4 Wave 1's previews start from the library one.
    const auto wave1 = preview == Preview::eqModule || preview == Preview::picker || preview == Preview::drop;

    // Phase 3 Q2.8: a library IR in true stereo, and the Reverb generator with Modulate.
    if (preview == Preview::library || preview == Preview::generators || wave1)
    {
        while (processor.getChain().getNumChildren() > 0)
            processor.removeModule ((int) processor.getChain().getChild (0)[verb::ids::uid]);

        if (preview == Preview::library || wave1)
        {
            const auto file = processor.addModule (ModuleType::file, 0);
            processor.loadLibrary (file, "Halls/Concert Hall");

            const auto filter = processor.addModule (ModuleType::lpHp, 1);
            processor.setModuleValue (filter, "highpass", 93.0f);

            // The mockup's EQ card (reference/phase4/36__verb_w1_mockup.png).
            if (preview == Preview::eqModule)
            {
                const auto eq = processor.addModule (ModuleType::eq, 2);
                processor.setModuleValue (eq, "lowGain", -2.0f);
                processor.setModuleValue (eq, "midGain", 3.0f);
                processor.setModuleValue (eq, "highGain", -1.5f);
            }
        }
        else
        {
            const auto reverb = processor.addModule (ModuleType::reverb, 0);

            const std::pair<const char*, float> reverbValues[] {
                { "lowDecay", 1.4f }, { "highDecay", 0.6f }, { "size", 24.0f }, { "buildUp", 60.0f }, { "early", 2.0f }, { "trueStereo", 1.0f }
            };

            for (const auto& [id, value] : reverbValues)
                processor.setModuleValue (reverb, id, value);

            processor.addModule (ModuleType::modulate, 1);
        }
    }

    processor.waitForBuild (20000);

    rack.rebuild();
    rack.hideAddPanel();
    updateDisplay();

    if (preview == Preview::addPanel)
        rack.showAddPanel ((int) rack.getCards().size()); // at the end, as the Add module box opens it

    if (preview == Preview::scrolled)
        rack.setScroll (rack.getContentWidth());

    drawer.setVisible (preview == Preview::setup);
    onTick (0.0f);
    strip.repaint();

    if (preview == Preview::picker)
    {
        processor.setFavourite ("Halls/Concert Hall", true);

        for (const auto& card : rack.getCards())
        {
            if (card->getType() == ModuleType::file)
            {
                card->showFileMenu();
                break;
            }
        }

        if (auto* picker = LibraryPicker::findOpen (getPopupParent()))
            picker->setSearch ("hall");
    }

    if (preview == Preview::drop)
        rack.showDropPreview ("Bright Room.wav");
}

juce::Colour VerbEditor::getAccentColour()
{
    return colours::uiPurple;
}

std::vector<ShellEditor::PopupOpener> VerbEditor::getPopupOpeners()
{
    auto openers = ShellEditor::getPopupOpeners();
    openers.push_back ({ "dropdown-latency", [this] { drawer.setVisible (true); latencyDropdown.click(); } });
    openers.push_back ({ "dropdown-src-gain", [this]
                         {
                             for (const auto& card : rack.getCards())
                             {
                                 if (card->getType() == verb::ModuleType::file)
                                 {
                                     card->showGainMenu();
                                     return;
                                 }
                             }
                         } });
    openers.push_back ({ "delete-module", [this] { confirmThen ("Delete module?", "Its settings will be lost.", "Delete", true, [] {}); } });
    openers.push_back ({ "value-predelay", [this] { predelayKnob.openValueEditor(); } });
    return openers;
}
