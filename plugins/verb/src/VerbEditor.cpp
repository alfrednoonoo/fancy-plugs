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
      storageDropdown (parameter ("irStorage")),
      duckSourceDropdown (parameter ("duckSource")),
      duckKnob (parameter ("duck"), "Ducking", colours::uiTeal, Knob::tightStyle),
      gateThresholdKnob (parameter ("gateThr"), "Threshold", colours::uiTeal, Knob::tightStyle),
      gateHoldKnob (parameter ("gateHold"), "Hold", colours::uiTeal, Knob::tightStyle),
      gateDecayKnob (parameter ("gateDecay"), "Decay", colours::uiTeal, Knob::tightStyle),
      gateCheckbox (parameter ("gate"), "Gate"),
      extraWidthKnob (parameter ("extraWidth"), "Extra width", colours::uiTeal, Knob::tightStyle),
      freezeToggle (parameter ("freeze"), "Freeze", colours::uiPurple),
      shimmerKnob (parameter ("shimmer"), "Shimmer", colours::uiTeal, Knob::tightStyle),
      intervalDropdown (parameter ("shimmerPitch"))
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

    // Phase 4 Wave 2 (reference/phase4/64__verb_w2_attack_width_mockup.png): Extra width under
    // Equal loudness.
    styleLabel (extraWidthNote, "Multiplies Width (the header's W knob). Mono stays the same; each side rises up to 3 dB. "
                                "Check in mono; 120 to 150 % is the useful range.",
                colours::textSecondary, juce::Justification::topLeft, 11.0f);
    extraWidthNote.setMinimumHorizontalScale (1.0f);
    extraWidthKnob.setBounds (20, 253, 80, 92);
    extraWidthNote.setBounds (112, 273, 200, 64);
    extraWidthKnob.setTooltip ("Widens the reverb past the W knob's 100 %: it multiplies Width, raising only the side, "
                               "so the mono sum stays the same.");

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &latencyDropdown, &fftDropdown, &storageDropdown, &folderButton, &folderPath, &lateLabel, &loudnessToggle, &loudnessTrim,
             &extraWidthKnob, &extraWidthNote })
        drawer.addAndMakeVisible (item);

    // Phase 4 Wave 2: Ducking and the Gate, live on the wet signal after the convolver, in the
    // drawer's free space (Decision 1 A).
    styleLabel (afterCaption, "AFTER THE REVERB", colours::textSecondary, juce::Justification::centredLeft, 10.0f);
    styleLabel (sourceLabel, "Source", colours::textPrimary, juce::Justification::centredLeft, 13.0f);
    styleLabel (sourceNote, "Sidechain when routed (pins 3/4)", colours::textSecondary, juce::Justification::centredLeft, 11.0f);
    styleLabel (duckNote, "Turns the reverb down while the source plays, up to 24 dB at 100 %.", colours::textSecondary,
                juce::Justification::topLeft, 11.0f);
    duckNote.setMinimumHorizontalScale (1.0f);

    afterCaption.setBounds (348, 50, 200, 14);
    sourceLabel.setBounds (348, 70, 56, 24);
    duckSourceDropdown.setBounds (404, 70, 110, 24);
    sourceNote.setBounds (524, 70, 200, 24);
    duckKnob.setBounds (340, 102, 76, 92);
    duckNote.setBounds (428, 126, 280, 32);
    gateCheckbox.setBounds (348, 236, 76, 20);
    gateThresholdKnob.setBounds (428, 206, 76, 92);
    gateHoldKnob.setBounds (508, 206, 76, 92);
    gateDecayKnob.setBounds (588, 206, 76, 92);

    duckSourceDropdown.setTooltip ("What Ducking and the Gate listen to: this track's input, or the sidechain (pins 3/4 in REAPER). "
                                   "With nothing routed to the sidechain, Sidechain acts as Main.");
    duckKnob.setTooltip ("Turns the reverb down while the source plays and lets it bloom in the gaps.");
    gateCheckbox.setTooltip ("Stops the reverb when the source stops: a gated reverb.");
    gateThresholdKnob.setTooltip ("The source level under which the gate starts to close.");
    gateHoldKnob.setTooltip ("How long the gate stays open after the source falls under Threshold.");
    gateDecayKnob.setTooltip ("How quickly the reverb fades out once the gate closes.");

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &afterCaption, &sourceLabel, &duckSourceDropdown, &sourceNote, &duckKnob, &duckNote,
             &gateCheckbox, &gateThresholdKnob, &gateHoldKnob, &gateDecayKnob })
        drawer.addAndMakeVisible (item);

    // Phase 4 Wave 3 (reference/phase4/72__verb_w3_mockup.png, A): Shimmer's row under the Gate,
    // in the drawer's new height over the chain strip.
    styleLabel (intervalLabel, "Interval", colours::textPrimary, juce::Justification::centredLeft, 13.0f);
    styleLabel (shimmerNote, "Feeds the reverb back into itself an interval higher, so the tail rises in octaves. "
                             "It always dies away; Freeze holds it.",
                colours::textSecondary, juce::Justification::topLeft, 11.0f);
    shimmerNote.setMinimumHorizontalScale (1.0f);

    shimmerKnob.setBounds (340, 310, 76, 92);
    intervalLabel.setBounds (428, 332, 60, 24);
    intervalDropdown.setBounds (490, 330, 150, 24);
    shimmerNote.setBounds (428, 362, 290, 32);

    shimmerKnob.setTooltip ("Shimmer: feeds the reverb's output back into it through a pitch shifter, so each pass comes back an interval higher. "
                            "0 % is off.");
    intervalDropdown.setTooltip ("How far each pass of Shimmer is shifted. Octave up is the classic shimmer.");

    for (juce::Component* item : std::initializer_list<juce::Component*> { &shimmerKnob, &intervalLabel, &intervalDropdown, &shimmerNote })
        drawer.addAndMakeVisible (item);

    // Phase 4 Wave 3: Freeze at the IR display's top right, under its info line.
    freezeToggle.setTooltip ("Freeze: holds the reverb as it is, for as long as it's on. Turned off, it dies away at the IR's own decay.");
    getBody().addAndMakeVisible (freezeToggle);

    getBody().addChildComponent (drawer);

    bodyResized();
    onTick (0.0f);
}

void VerbEditor::bodyResized()
{
    display.setBounds (0, 0, rackWidth, displayHeight);
    rack.setBounds (0, displayHeight, rackWidth, ModuleCard::height);
    strip.setBounds (0, displayHeight + ModuleCard::height, rackWidth, stripHeight);

    // Phase 4 Wave 3 (mockup 72, A): the drawer covers the chain strip too, for Shimmer's row.
    drawer.setBounds (rack.getBounds().withHeight (ModuleCard::height + stripHeight));
    drawer.toFront (false);

    freezeToggle.setBounds (rackWidth - 86, 35, 70, 22);
    freezeToggle.toFront (false);

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

    // Phase 4 Wave 3: the display tints while frozen.
    display.setFrozen (parameter ("freeze").getValue() >= 0.5f);

    // Phase 4 Wave 2: the Gate's knobs dim while it's off.
    {
        const auto gateOn = parameter ("gate").getValue() >= 0.5f;

        for (auto* knob : { &gateThresholdKnob, &gateHoldKnob, &gateDecayKnob })
            knob->setAlpha (gateOn ? 1.0f : 0.4f);
    }

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

    drawer.setVisible (preview == Preview::setup || preview == Preview::shimmer);

    // Phase 4 Wave 3: Freeze on, and Shimmer at 50 % (reference/phase4/72__verb_w3_mockup.png).
    setValue ("freeze", preview == Preview::frozen ? 1.0f : 0.0f);
    setValue ("shimmer", preview == Preview::shimmer ? 50.0f : 0.0f);
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
    {
        // The picker shot before this one leaves it open.
        if (auto* picker = LibraryPicker::findOpen (getPopupParent()))
            picker->close();

        rack.showDropPreview ("Bright Room.wav");
    }
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
