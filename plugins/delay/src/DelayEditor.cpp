#include "DelayEditor.h"

#include <fancy/plugin/ParamFormat.h>

using namespace fancy::ui;

namespace
{
// reference/phase2/11__delay_mockup.png at 100 %. The shared header is 16 px taller than the
// mockup's, so the window is too.
constexpr int baseWidth = 760;
constexpr int bodyHeight = 375;
constexpr int baseHeight = ShellEditor::headerHeight + bodyHeight;
constexpr int rightColumnX = 656;

constexpr int mapX = 13, mapY = 10, mapWidth = 610, mapHeight = 150;
constexpr int tabRowY = 170, tabRowHeight = 22;
constexpr int cardY = 198, cardHeight = 145;
constexpr int timeCardX = 13, timeCardWidth = 186;
constexpr int controlsCardX = 207, controlsCardWidth = 439;
constexpr int footerY = 351;
constexpr int faderTop = 26, faderHeight = 321;

// Between Knob::smallStyle and mediumStyle. The window fits the mockup's labels at 12 px, not
// the 14 px of the larger Figma frames.
constexpr float labelHeight = 12.0f;
constexpr Knob::Style lengthStyle { 60.0f, 52.0f, 4.0f, 14.0f, 14.0f, 17.0f, 6.0f, 3.0f, 23.0f, labelHeight, false, true };
constexpr Knob::Style tapKnobStyle = Knob::tightStyle;

constexpr OutMeter::Geometry meterGeometry { 14.5f, 15.0f, 306.0f, 25.0f, 4.75f, 314.0f, false, true };

const juce::Identifier selectedTapProperty ("selectedTap");

void setValue (juce::RangedAudioParameter& parameter, float value)
{
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
}

void styleLabel (juce::Label& label, const juce::String& text, juce::Colour colour, juce::Justification justification)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (font (Weight::regular, 14.0f));
    label.setColour (juce::Label::textColourId, colour);
    label.setJustificationType (justification);
    label.setBorderSize ({});
    label.setInterceptsMouseClicks (false, false);
}

juce::String feelSuffix (int feel)
{
    return feel == delay::dotted ? "dotted" : (feel == delay::triplet ? "triplet" : juce::String {});
}

// The note value text for a tap, such as "1/8 dotted" or "1/4 ×3".
juce::String noteText (const delay::NoteLength& length, juce::RangedAudioParameter& noteParameter)
{
    auto text = noteParameter.getAllValueStrings()[length.note];

    if (length.feel != delay::straight)
        text << " " << feelSuffix (length.feel);

    if (length.multiplier > 1)
        text << " " << juce::String::charToString (0x00d7) << length.multiplier;

    return text;
}
} // namespace

// Panels ----------------------------------------------------------------------

void DelayEditor::Panels::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgBody);

    g.setColour (colours::bgSection);
    g.fillRect (rightColumnX, 0, baseWidth - rightColumnX, bodyHeight);
    g.fillRoundedRectangle ((float) timeCardX, (float) cardY, (float) timeCardWidth, (float) cardHeight, 6.0f);
    g.fillRoundedRectangle ((float) controlsCardX, (float) cardY, (float) controlsCardWidth, (float) cardHeight, 6.0f);

    g.setColour (colours::textSecondary);
    g.setFont (font (Weight::semiBold, 12.0f).withExtraKerningFactor (0.06f));
    g.drawText ("TIME", timeCardX + 12, cardY + 10, 60, 24, juce::Justification::centredLeft, false);
}

// FeelSwitch ------------------------------------------------------------------

void DelayEditor::FeelSwitch::paintSegment (juce::Graphics& g, int index, juce::Rectangle<float> area, juce::Colour ink)
{
    const auto centre = area.getCentre().translated (index == delay::straight ? 0.0f : -2.0f, 0.0f);

    // An eighth note: head, stem and flag.
    juce::Path note;
    note.addEllipse (centre.x - 3.5f, centre.y + 2.0f, 4.5f, 3.5f);
    note.addRectangle (centre.x + 0.1f, centre.y - 5.5f, 0.9f, 9.0f);
    note.startNewSubPath (centre.x + 1.0f, centre.y - 5.5f);
    note.quadraticTo (centre.x + 1.5f, centre.y - 3.0f, centre.x + 3.5f, centre.y - 2.0f);
    note.lineTo (centre.x + 1.0f, centre.y - 3.2f);
    note.closeSubPath();

    g.setColour (ink);
    g.fillPath (note);

    if (index == delay::dotted)
    {
        g.fillEllipse (centre.x + 3.0f, centre.y + 3.0f, 2.2f, 2.2f);
    }
    else if (index == delay::triplet)
    {
        g.setFont (font (Weight::regular, 10.0f));
        g.drawText ("3", juce::Rectangle<float> (centre.x + 2.5f, centre.y - 8.0f, 8.0f, 10.0f), juce::Justification::centredLeft, false);
    }
}

// TapEditor -------------------------------------------------------------------

namespace
{
juce::RangedAudioParameter& tapParameter (DelayProcessor& processor, int tap, const char* suffix)
{
    return *processor.getApvts().getParameter (DelayProcessor::id (tap, suffix));
}
} // namespace

DelayEditor::TapEditor::TapEditor (DelayProcessor& processor, int tapIndex, DelayEditor& owner)
    : delayProcessor (processor),
      tap (tapIndex),
      modeSwitch (tapParameter (processor, tapIndex, "mode"), colours::grey6, colours::textPrimary),
      msKnob (tapParameter (processor, tapIndex, "ms"), "Length", tapColour (tapIndex), lengthStyle),
      noteKnob (tapParameter (processor, tapIndex, "note"), "Length", tapColour (tapIndex), lengthStyle),
      noteDropdown (tapParameter (processor, tapIndex, "note")),
      feelSwitch (tapParameter (processor, tapIndex, "feel"), colours::grey6, colours::textPrimary),
      feedbackKnob (tapParameter (processor, tapIndex, "fb"), "Feedback", tapColour (tapIndex), tapKnobStyle),
      lowpassKnob (tapParameter (processor, tapIndex, "lp"), "Lowpass", colours::uiBlue, tapKnobStyle),
      highpassKnob (tapParameter (processor, tapIndex, "hp"), "Highpass", colours::uiBlue, tapKnobStyle),
      resolutionKnob (tapParameter (processor, tapIndex, "bits"), "Resolution", tapColour (tapIndex), tapKnobStyle),
      widthKnob (tapParameter (processor, tapIndex, "width"), "Width", tapColour (tapIndex), tapKnobStyle),
      panKnob (tapParameter (processor, tapIndex, "pan"), "Pan", tapColour (tapIndex), tapKnobStyle, true),
      volumeFader (tapParameter (processor, tapIndex, "vol"), tapColour (tapIndex)),
      soloButton (tapParameter (processor, tapIndex, "solo"), "S", tapColour (tapIndex)),
      powerIcon (tapParameter (processor, tapIndex, "on"), tapColour (tapIndex)),
      closeButton (IconButton::Glyph::cross),
      loopCheckbox (tapParameter (processor, tapIndex, "inloop"), "Filters inside feedback loop"),
      pingCheckbox (tapParameter (processor, tapIndex, "ping"), "Ping-pong"),
      saturateCheckbox (tapParameter (processor, tapIndex, "sat"), "Saturate feedback")
{
    setInterceptsMouseClicks (false, true);

    styleLabel (resultLabel, {}, colours::textSecondary, juce::Justification::centredLeft);
    styleLabel (volumeLabel, "Volume", colours::textSecondary, juce::Justification::centred);

    modeSwitch.setBounds (timeCardX + 94, cardY + 10, 80, 25);
    msKnob.setBounds (timeCardX + 4, cardY + 34, 86, 109);
    noteKnob.setBounds (timeCardX + 4, cardY + 34, 86, 109);
    noteDropdown.setBounds (timeCardX + 92, cardY + 50, 84, 24);
    feelSwitch.setBounds (timeCardX + 92, cardY + 82, 84, 24);
    resultLabel.setBounds (timeCardX + 94, cardY + 112, 88, 20);

    constexpr int knobWidth = 70, knobHeight = 99, knobTop = cardY + 9, firstCentre = 247, spacing = 64;

    const auto placeKnob = [&] (Knob& knob, int index)
    {
        knob.setBounds (firstCentre + index * spacing - knobWidth / 2, knobTop, knobWidth, knobHeight);
    };

    placeKnob (feedbackKnob, 0);
    placeKnob (lowpassKnob, 1);
    placeKnob (highpassKnob, 2);
    placeKnob (resolutionKnob, 3);
    placeKnob (widthKnob, 4);
    placeKnob (panKnob, 5);

    volumeLabel.setFont (font (Weight::regular, labelHeight));
    volumeLabel.setBounds (590, cardY + 7, 52, 17);
    volumeFader.setBounds (594, cardY + 26, 44, 112);

    soloButton.setBounds (563, tabRowY, 23, tabRowHeight);
    powerIcon.setBounds (597, tabRowY + 5, 12, 13);
    closeButton.setBounds (630, tabRowY + 7, 8, 8);

    loopCheckbox.setBounds (15, footerY, 178, 20);
    pingCheckbox.setBounds (205, footerY, 86, 20);
    saturateCheckbox.setBounds (294, footerY, 130, 20);

    for (auto* checkbox : { &loopCheckbox, &pingCheckbox, &saturateCheckbox })
        checkbox->setLabelHeight (labelHeight);

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &modeSwitch, &msKnob, &noteKnob, &noteDropdown, &feelSwitch, &resultLabel,
             &feedbackKnob, &lowpassKnob, &highpassKnob, &resolutionKnob, &widthKnob, &panKnob, &volumeLabel, &volumeFader,
             &soloButton, &powerIcon, &closeButton, &loopCheckbox, &pingCheckbox, &saturateCheckbox })
        addAndMakeVisible (item);

    closeButton.onClick = [&owner] { owner.removeSelectedTap(); };

    feedbackKnob.setTooltip ("Up to 100 %, or 110 % with Saturate feedback on.");
    panKnob.setTooltip ("With Ping-pong on, Pan only picks the side the repeats start on.");
}

juce::String DelayEditor::TapEditor::detailText() const
{
    const auto spec = delayProcessor.readTap (tap);
    const auto ms = DelayDsp::tapMs (spec, delayProcessor.getBpm());

    auto text = juce::String (juce::roundToInt (ms)) + " ms";

    if (spec.noteMode)
        text << " " << juce::String::charToString (0x00b7) << " " << noteText (spec.noteLength, tapParameter (delayProcessor, tap, "note"));

    return text;
}

void DelayEditor::TapEditor::update (int titleX)
{
    const auto spec = delayProcessor.readTap (tap);
    const auto ms = DelayDsp::tapMs (spec, delayProcessor.getBpm());

    msKnob.setVisible (! spec.noteMode);
    noteKnob.setVisible (spec.noteMode);
    noteDropdown.setVisible (spec.noteMode);
    feelSwitch.setVisible (spec.noteMode);
    resultLabel.setVisible (spec.noteMode);

    auto suffix = feelSuffix (spec.noteLength.feel);

    if (spec.noteLength.multiplier > 1)
        suffix << " " << juce::String::charToString (0x00d7) << spec.noteLength.multiplier;

    noteKnob.setValueSuffix (suffix, tapColour (tap));
    resultLabel.setText ("= " + fancy::formatFixed ((float) ms, 1) + " ms", juce::dontSendNotification);

    titleLeft = titleX;
    repaint (0, tabRowY, 560, tabRowHeight);
}

void DelayEditor::TapEditor::paint (juce::Graphics& g)
{
    const juce::Rectangle<int> row (titleLeft, tabRowY, 556 - titleLeft, tabRowHeight);

    const auto title = "TAP " + juce::String (tap + 1);

    g.setFont (font (Weight::semiBold, 14.0f).withExtraKerningFactor (0.04f));
    g.setColour (tapColour (tap));
    g.drawText (title, row, juce::Justification::centredLeft, true);

    g.setFont (font (Weight::regular, 13.0f));
    g.setColour (colours::textSecondary);
    g.drawText (detailText(), row.withTrimmedLeft (46), juce::Justification::centredLeft, true);

    g.setColour (colours::bgSection);
    g.fillRoundedRectangle (592.0f, (float) tabRowY, 23.0f, (float) tabRowHeight, 3.0f);
    g.fillRoundedRectangle (622.0f, (float) tabRowY, 24.0f, (float) tabRowHeight, 3.0f);
}

// DelayEditor -----------------------------------------------------------------

DelayEditor::DelayEditor (DelayProcessor& processor)
    : ShellEditor (processor, baseWidth, baseHeight, HeaderStyle::full),
      tapMap (processor),
      smoothCheckbox (parameter ("smooth"), "Smooth time changes"),
      editLink (colours::uiTeal),
      outMeter (meterGeometry),
      wetFader (parameter ("wet"), colours::uiTeal),
      dryFader (parameter ("dry"), colours::uiTeal),
      drawerOutputKnob (parameter ("output"), "Output", colours::uiTeal, Knob::smallStyle)
{
    auto& body = getBody();

    panels.setInterceptsMouseClicks (false, false);
    panels.setBounds (0, 0, baseWidth, bodyHeight);
    body.addAndMakeVisible (panels);

    tapMap.setBounds (mapX, mapY, mapWidth, mapHeight);
    tapMap.onTapSelected = [this] (int tap) { selectTap (tap); };
    body.addAndMakeVisible (tapMap);

    tabs.setBounds (mapX, tabRowY, 480, tabRowHeight);
    tabs.onSelect = [this] (int tap) { selectTap (tap); };
    tabs.onAdd = [this] { addTap(); };
    body.addAndMakeVisible (tabs);

    for (int tap = 0; tap < delay::maxTaps; ++tap)
    {
        tapEditors.push_back (std::make_unique<TapEditor> (processor, tap, *this));
        tapEditors.back()->setBounds (0, 0, rightColumnX, bodyHeight);
        body.addChildComponent (*tapEditors.back());

        multiplierDropdowns.push_back (std::make_unique<ParamDropdown> (parameter (DelayProcessor::id (tap, "mult"))));
    }

    tabs.toFront (false);

    smoothCheckbox.setBounds (431, footerY, 151, 20);
    smoothCheckbox.setLabelHeight (labelHeight);
    editLink.setBounds (582, footerY - 2, 64, 24);
    outMeter.setBounds (650, 10, 46, 340);

    styleLabel (wetLabel, "Wet", colours::textSecondary, juce::Justification::centred);
    styleLabel (dryLabel, "Dry", colours::textSecondary, juce::Justification::centred);

    wetLabel.setBounds (690, 10, 33, 17);
    dryLabel.setBounds (723, 10, 33, 17);
    wetFader.setBounds (690, faderTop, 33, faderHeight);
    dryFader.setBounds (723, faderTop, 33, faderHeight);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &smoothCheckbox, &editLink, &outMeter, &wetLabel, &dryLabel, &wetFader, &dryFader })
        body.addAndMakeVisible (item);

    styleLabel (tempoLabel, {}, colours::textSecondary, juce::Justification::centredRight);
    tempoLabel.setBounds (520, 13, 124, 24);
    getRoot().addAndMakeVisible (tempoLabel);

    drawer.setBounds (0, tabRowY - 2, rightColumnX, bodyHeight - tabRowY + 2);

    styleLabel (multiplierLabel, "Note multiplier", colours::textPrimary, juce::Justification::centredLeft);
    multiplierLabel.setBounds (24, 60, 110, 24);
    drawer.addAndMakeVisible (multiplierLabel);

    for (auto& dropdown : multiplierDropdowns)
    {
        dropdown->setBounds (140, 60, 80, 24);
        drawer.addChildComponent (*dropdown);
    }

    drawerOutputKnob.setBounds (300, 56, 90, 99);
    drawer.addAndMakeVisible (drawerOutputKnob);

    body.addChildComponent (drawer);
    editLink.onClick = [this] { drawer.setVisible (! drawer.isVisible()); };

    selectedTap = juce::jlimit (0, delay::maxTaps - 1, (int) processor.getApvts().state.getProperty (selectedTapProperty, 0));
    tapMap.setSelectedTap (selectedTap);

    updateSelection();
}

DelayProcessor& DelayEditor::delayProcessor() noexcept
{
    return dynamic_cast<DelayProcessor&> (getFancyProcessor());
}

bool DelayEditor::listsParameter (const juce::AudioProcessorParameter& item)
{
    const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*> (&item);

    if (ranged == nullptr)
        return false;

    const auto parameterId = ranged->getParameterID();
    const auto isTapParameter = parameterId[0] == 't' && juce::CharacterFunctions::isDigit (parameterId[1]);

    if (! isTapParameter)
        return true;

    return ! parameterId.endsWith ("used") && delayProcessor().isTapUsed (parameterId.substring (1).getIntValue() - 1);
}

void DelayEditor::selectTap (int tap)
{
    selectedTap = tap;
    getFancyProcessor().getApvts().state.setProperty (selectedTapProperty, tap, nullptr);
    tapMap.setSelectedTap (tap);
    updateSelection();
}

void DelayEditor::addTap()
{
    const auto added = delayProcessor().addTap (selectedTap);

    if (added >= 0)
        selectTap (added);
}

void DelayEditor::removeSelectedTap()
{
    auto& processor = delayProcessor();

    if (processor.countTaps() <= 1)
        return;

    processor.removeTap (selectedTap);

    for (int offset = 1; offset < delay::maxTaps; ++offset)
    {
        for (const auto candidate : { selectedTap - offset, selectedTap + offset })
        {
            if (candidate >= 0 && candidate < delay::maxTaps && processor.isTapUsed (candidate))
            {
                selectTap (candidate);
                return;
            }
        }
    }
}

void DelayEditor::updateSelection()
{
    auto& processor = delayProcessor();

    std::vector<ColourTabs::Tab> used;

    for (int tap = 0; tap < delay::maxTaps; ++tap)
        if (processor.isTapUsed (tap))
            used.push_back ({ tap, tapColour (tap) });

    if (! used.empty() && ! processor.isTapUsed (selectedTap))
    {
        // A preset or automation removed it.
        selectedTap = used.front().id;
        tapMap.setSelectedTap (selectedTap);
    }

    tabs.showAddButton = (int) used.size() < delay::maxTaps;
    tabs.setTabs (std::move (used), selectedTap);

    for (int tap = 0; tap < delay::maxTaps; ++tap)
    {
        const auto isSelected = tap == selectedTap;

        tapEditors[(size_t) tap]->setVisible (isSelected);
        multiplierDropdowns[(size_t) tap]->setVisible (isSelected);
    }

    tapEditors[(size_t) selectedTap]->update (tabs.getEndX() + 8);
}

void DelayEditor::onTick (float seconds)
{
    auto& processor = delayProcessor();

    updateSelection();
    tapMap.update();
    outMeter.update (processor.outputMeter, seconds);

    tempoLabel.setText (fancy::formatFixed ((float) processor.getBpm(), 1) + " BPM" + (processor.hasHostTempo() ? " (host)" : ""),
                        juce::dontSendNotification);
}

void DelayEditor::showPreview (bool drawerOpen)
{
    stopUpdates();

    const auto tapValue = [this] (int tap, const char* suffix, float value) { setValue (parameter (DelayProcessor::id (tap, suffix)), value); };

    tapValue (0, "fb", 42.0f);
    tapValue (0, "lp", 4500.0f);
    tapValue (0, "hp", 180.0f);
    tapValue (0, "pan", -60.0f);
    tapValue (0, "ping", 1.0f);

    tapValue (1, "used", 1.0f);
    tapValue (1, "note", 2.0f);
    tapValue (1, "feel", 0.0f);
    tapValue (1, "vol", -6.0f);

    tapValue (2, "used", 1.0f);
    tapValue (2, "note", 2.0f);
    tapValue (2, "feel", 1.0f);
    tapValue (2, "vol", -12.0f);

    setValue (parameter ("wet"), -6.0f);

    selectTap (0);
    tapMap.update();
    outMeter.show (-8.0f, -6.1f);

    tempoLabel.setText ("120.0 BPM (host)", juce::dontSendNotification);

    drawer.setVisible (drawerOpen);
}
