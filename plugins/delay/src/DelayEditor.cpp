#include "DelayEditor.h"

#include <fancy/plugin/ParamFormat.h>

using namespace fancy::ui;

namespace
{
// reference/phase2/11__delay_mockup.png at 100 %. The shared header is 16 px taller than the
// mockup's, so the window is too. Phase 4 Wave 2 (reference/phase4/57__delay_w2_mockup.png) grows
// it 60 px wider for the Motion page's Stereo knob: the tap graph and the knob card widen, and
// everything right of them moves across. v1.7.2 (reference/phase4/67__delay_freeze_mockup.png, A) grows
// it 24 px taller: the tap graph has a label strip for Freeze, so everything under it moves down.
// Wave 3 (reference/phase4/69__delay_w3_mockup.png) grows it 64 px wider for the
// Tone page's Pitch knob: the tap graph and the knob card widen, everything right of
// them moves across, and the Motion page's knobs spread to the Tone page's 64 px.
constexpr int grownWidth = 64;
constexpr int baseWidth = 820 + grownWidth;
constexpr int bodyHeight = 399;
constexpr int baseHeight = ShellEditor::headerHeight + bodyHeight;
constexpr int rightColumnX = 716 + grownWidth;

constexpr int mapX = 13, mapY = 10, mapWidth = 670 + grownWidth, mapHeight = 174;
constexpr int tabRowY = 194, tabRowHeight = 22;
constexpr int cardY = 222, cardHeight = 145;
constexpr int timeCardX = 13, timeCardWidth = 186;
constexpr int controlsCardX = 207, controlsCardWidth = 499 + grownWidth;
constexpr int footerY = 375;
constexpr int faderTop = 26, faderHeight = 345;

// Between Knob::smallStyle and mediumStyle. The window fits the mockup's labels at 12 px, not
// the 14 px of the larger Figma frames.
constexpr float labelHeight = 12.0f;
constexpr Knob::Style lengthStyle { 60.0f, 52.0f, 4.0f, 14.0f, 14.0f, 17.0f, 6.0f, 3.0f, 23.0f, labelHeight, false, true };
constexpr Knob::Style tapKnobStyle = Knob::tightStyle;

// Phase 3 Q2.3: a smaller labelled knob, for Amount under the Character switch.
constexpr Knob::Style amountStyle { 36.0f, 30.0f, 3.0f, 10.0f, 8.0f, 14.0f, 4.0f, 2.0f, 18.0f, 11.0f, false, true };

constexpr OutMeter::Geometry meterGeometry { 14.5f, 15.0f, 330.0f, 25.0f, 5.156f, 338.0f, false, true };

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

// WaveSwitch ------------------------------------------------------------------

void DelayEditor::WaveSwitch::paintSegment (juce::Graphics& g, int index, juce::Rectangle<float> area, juce::Colour ink)
{
    // An 18 x 10 symbol, as drawn in reference/phase4/57__delay_w2_mockup.png.
    const auto box = juce::Rectangle<float> (18.0f, 10.0f).withCentre (area.getCentre());
    const auto x = [&box] (float u) { return box.getX() + u; };
    const auto y = [&box] (float v) { return box.getY() + v; };

    juce::Path wave;
    wave.startNewSubPath (x (1.0f), y (5.0f));

    if (index == 0)
    {
        wave.cubicTo (x (3.5f), y (-1.5f), x (5.5f), y (-1.5f), x (9.0f), y (5.0f));
        wave.cubicTo (x (12.5f), y (11.5f), x (14.5f), y (11.5f), x (17.0f), y (5.0f));
    }
    else
    {
        // Rounded corners, as the triangle's are.
        wave.lineTo (x (4.6f), y (1.0f));
        wave.quadraticTo (x (5.0f), y (0.6f), x (5.4f), y (1.0f));
        wave.lineTo (x (12.6f), y (9.0f));
        wave.quadraticTo (x (13.0f), y (9.4f), x (13.4f), y (9.0f));
        wave.lineTo (x (17.0f), y (5.0f));
    }

    g.setColour (ink);
    g.strokePath (wave, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// DirectionSwitch -------------------------------------------------------------

void DelayEditor::DirectionSwitch::paintSegment (juce::Graphics& g, int index, juce::Rectangle<float> area, juce::Colour ink)
{
    // A 7 x 8 triangle, pointing right for Forward and left for Reverse.
    const auto c = area.getCentre();
    juce::Path triangle;

    if (index == 0)
        triangle.addTriangle (c.x - 3.0f, c.y - 4.0f, c.x - 3.0f, c.y + 4.0f, c.x + 4.0f, c.y);
    else
        triangle.addTriangle (c.x + 3.0f, c.y - 4.0f, c.x + 3.0f, c.y + 4.0f, c.x - 4.0f, c.y);

    g.setColour (ink);
    g.fillPath (triangle);
}

juce::String DelayEditor::DirectionSwitch::getTooltip()
{
    const auto reverse = getMouseXYRelative().x >= getWidth() / 2;

    return reverse ? "Reverse: plays each window of one tap time backwards, so notes swell in"
                   : "Forward: the repeats play as they came in";
}

juce::String DelayEditor::WaveSwitch::getTooltip()
{
    const auto triangle = getMouseXYRelative().x >= getWidth() / 2;

    return triangle ? "Triangle: a steady detune that flips each half cycle"
                    : "Sine: the pitch sweeps smoothly up and down";
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
      saturateCheckbox (tapParameter (processor, tapIndex, "sat"), "Saturate feedback"),
      characterSegments (tapParameter (processor, tapIndex, "char"), tapColour (tapIndex), colours::bgWell),
      amountKnob (tapParameter (processor, tapIndex, "charAmt"), "Amount", tapColour (tapIndex), amountStyle),
      modRateKnob (tapParameter (processor, tapIndex, "modRate"), "Rate", tapColour (tapIndex), tapKnobStyle),
      modDepthKnob (tapParameter (processor, tapIndex, "modDepth"), "Depth", tapColour (tapIndex), tapKnobStyle),
      randomKnob (tapParameter (processor, tapIndex, "modRand"), "Random", tapColour (tapIndex), tapKnobStyle),
      diffusionKnob (tapParameter (processor, tapIndex, "diff"), "Diffusion", tapColour (tapIndex), tapKnobStyle),
      shapeSwitch (tapParameter (processor, tapIndex, "modShape"), tapColour (tapIndex), colours::bgWell),
      stereoKnob (tapParameter (processor, tapIndex, "modStereo"), "Stereo", tapColour (tapIndex), tapKnobStyle),
      pitchKnob (tapParameter (processor, tapIndex, "pitch"), "Pitch", tapColour (tapIndex), tapKnobStyle, true),
      directionSwitch (tapParameter (processor, tapIndex, "reverse"), tapColour (tapIndex), colours::bgWell)
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

    // Phase 4 Wave 3 (reference/phase4/69__delay_w3_mockup.png): Pitch in the seventh place, and
    // the direction switch under Feedback, as the wave switch sits under Rate.
    placeKnob (pitchKnob, 6);
    directionSwitch.setBounds (firstCentre - 25, cardY + 113, 50, 17);

    volumeLabel.setFont (font (Weight::regular, labelHeight));
    volumeLabel.setBounds (650 + grownWidth, cardY + 7, 52, 17);
    volumeFader.setBounds (654 + grownWidth, cardY + 26, 44, 112);

    soloButton.setBounds (623 + grownWidth, tabRowY, 23, tabRowHeight);
    powerIcon.setBounds (657 + grownWidth, tabRowY + 5, 12, 13);
    closeButton.setBounds (690 + grownWidth, tabRowY + 7, 8, 8);

    loopCheckbox.setBounds (15, footerY, 178, 20);
    pingCheckbox.setBounds (205, footerY, 86, 20);
    saturateCheckbox.setBounds (294, footerY, 130, 20);

    for (auto* checkbox : { &loopCheckbox, &pingCheckbox, &saturateCheckbox })
        checkbox->setLabelHeight (labelHeight);

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &modeSwitch, &msKnob, &noteKnob, &noteDropdown, &feelSwitch, &resultLabel,
             &feedbackKnob, &lowpassKnob, &highpassKnob, &resolutionKnob, &widthKnob, &panKnob, &pitchKnob, &directionSwitch,
             &volumeLabel, &volumeFader, &soloButton, &powerIcon, &closeButton, &loopCheckbox, &pingCheckbox, &saturateCheckbox })
        addAndMakeVisible (item);

    closeButton.onClick = [&owner] { owner.removeSelectedTap(); };

    // "1/8." or "1/8d" also picks dotted, and "1/8t" triplet.
    noteKnob.onTypedText = [this] (const juce::String& typed)
    {
        const auto text = typed.toLowerCase().trim();
        const auto feel = text.endsWithChar ('.') || text.endsWithChar ('d') ? 1 : text.endsWithChar ('t') ? 2 : -1;

        if (feel < 0)
            return false;

        auto& note = tapParameter (delayProcessor, tap, "note");
        auto& feelParameter = tapParameter (delayProcessor, tap, "feel");

        for (auto& [parameter, value] : { std::pair<juce::RangedAudioParameter*, float> { &note, note.getValueForText (text.trimCharactersAtEnd (".dt")) },
                                          std::pair<juce::RangedAudioParameter*, float> { &feelParameter, feelParameter.convertTo0to1 ((float) feel) } })
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (value);
            parameter->endChangeGesture();
        }

        return true;
    };

    // Phase 3 Q2.3 (reference/phase3/27__delay_q23_mockup.png): Tone | Motion in the tap row,
    // and the Motion page in the knob card's place.
    pageSwitch.setBounds (400, tabRowY, 130, tabRowHeight);
    pageSwitch.onChange = [this] (bool motion) { showPage (motion); };

    styleLabel (characterLabel, "Character", colours::textPrimary, juce::Justification::centredLeft);
    characterLabel.setFont (font (Weight::regular, labelHeight));
    characterLabel.setBounds (controlsCardX + 14, cardY + 8, 120, 16);
    characterSegments.setBounds (controlsCardX + 12, cardY + 28, 124, 22);
    characterSegments.setTextHeight (11.0f);
    amountKnob.setBounds (controlsCardX + 10, cardY + 58, 70, 78);

    {
        constexpr int knobWidth = 66, knobHeight = 99, knobTop = cardY + 9, firstX = controlsCardX + 144;
        int index = 0;

        // Phase 4 Wave 3 (reference/phase4/69__delay_w3_mockup.png): Rate, Depth, Random, Stereo
        // (Wave 2) and Diffusion 64 px apart, the Tone page's spacing, in the width the window
        // gained. The wave switch stays under Rate.
        for (auto* knob : { &modRateKnob, &modDepthKnob, &randomKnob, &stereoKnob, &diffusionKnob })
            knob->setBounds (firstX + (index++) * 64, knobTop, knobWidth, knobHeight);

        shapeSwitch.setBounds (364, cardY + 113, 50, 17);
    }

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &pageSwitch, &characterLabel, &characterSegments, &amountKnob, &modRateKnob, &modDepthKnob, &randomKnob, &diffusionKnob,
             &shapeSwitch, &stereoKnob })
        addChildComponent (item);

    pageSwitch.setVisible (true);
    showPage (false);

    modDepthKnob.setTooltip ("The peak pitch change of the wow and flutter. 0 is none.");
    randomKnob.setTooltip ("How much of the motion is a slow random wander rather than a steady wobble.");
    diffusionKnob.setTooltip ("Smears each repeat into the next, inside the feedback loop.");
    pitchKnob.setTooltip ("Shifts the repeats, again on each pass through the feedback loop: +12 st climbs in octaves, "
                          "a small amount drifts. 0 is off.");

    stereoKnob.setTooltip (juce::String::fromUTF8 ("How far ahead the right channel's wobble runs. 0\xc2\xb0 moves both sides together; "
                                                   "180\xc2\xb0 moves them in opposition, the widest. 90\xc2\xb0 is the default."));

    feedbackKnob.setTooltip ("Up to 100 %, or 110 % with Saturate feedback on.");
    panKnob.setTooltip ("With Ping-pong on, Pan only picks the side the repeats start on.");
}

void DelayEditor::TapEditor::showPage (bool motion)
{
    pageSwitch.motion = motion;
    pageSwitch.repaint();

    for (juce::Component* item : std::initializer_list<juce::Component*> { &feedbackKnob, &lowpassKnob, &highpassKnob, &resolutionKnob, &widthKnob, &panKnob,
                                                                           &pitchKnob, &directionSwitch })
        item->setVisible (! motion);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &characterLabel, &characterSegments, &amountKnob, &modRateKnob, &modDepthKnob,
                                                                           &randomKnob, &diffusionKnob, &shapeSwitch, &stereoKnob })
        item->setVisible (motion);
}

void DelayEditor::TapEditor::PageSwitch::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (bounds, 5.0f);

    const auto inner = bounds.reduced (2.0f);
    const auto half = inner.getWidth() * 0.5f;

    for (int i = 0; i < 2; ++i)
    {
        const auto area = inner.withX (inner.getX() + (float) i * half).withWidth (half);
        const auto selected = (i == 1) == motion;

        if (selected)
        {
            g.setColour (colours::grey5);
            g.fillRoundedRectangle (area, 3.0f);
        }

        g.setColour (selected ? colours::textPrimary : colours::textSecondary);
        g.setFont (font (Weight::regular, 12.0f));
        g.drawText (i == 0 ? "Tone" : "Motion", area, juce::Justification::centred, false);
    }
}

void DelayEditor::TapEditor::PageSwitch::mouseDown (const juce::MouseEvent& event)
{
    const auto wantMotion = event.position.x >= (float) getWidth() * 0.5f;

    if (wantMotion != motion && onChange != nullptr)
        onChange (wantMotion);
}

juce::String DelayEditor::TapEditor::detailText() const
{
    const auto spec = delayProcessor.readTap (tap);
    const auto ms = DelayDsp::tapMs (spec, delayProcessor.getBpm(), delayProcessor.getApvts().getRawParameterValue ("swing")->load() * 0.01);

    auto text = juce::String (juce::roundToInt (ms)) + " ms";

    if (spec.noteMode)
        text << " " << juce::String::charToString (0x00b7) << " " << noteText (spec.noteLength, tapParameter (delayProcessor, tap, "note"));

    return text;
}

void DelayEditor::TapEditor::update (int titleX)
{
    const auto spec = delayProcessor.readTap (tap);
    const auto ms = DelayDsp::tapMs (spec, delayProcessor.getBpm(), delayProcessor.getApvts().getRawParameterValue ("swing")->load() * 0.01);

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
    const juce::Rectangle<int> row (titleLeft, tabRowY, 396 - titleLeft, tabRowHeight);

    const auto title = "TAP " + juce::String (tap + 1);

    g.setFont (font (Weight::semiBold, 14.0f).withExtraKerningFactor (0.04f));
    g.setColour (tapColour (tap));
    g.drawText (title, row, juce::Justification::centredLeft, true);

    g.setFont (font (Weight::regular, 13.0f));
    g.setColour (colours::textSecondary);
    g.drawText (detailText(), row.withTrimmedLeft (46), juce::Justification::centredLeft, true);

    g.setColour (colours::bgSection);
    g.fillRoundedRectangle ((float) (652 + grownWidth), (float) tabRowY, 23.0f, (float) tabRowHeight, 3.0f);
    g.fillRoundedRectangle ((float) (682 + grownWidth), (float) tabRowY, 24.0f, (float) tabRowHeight, 3.0f);
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
      drawerOutputKnob (parameter ("output"), "Output", colours::uiTeal, Knob::smallStyle),
      swingKnob (parameter ("swing"), "Swing", colours::uiTeal, Knob::tightStyle),
      duckAmountKnob (parameter ("duckAmt"), "Amount", colours::uiTeal, Knob::tightStyle),
      duckThresholdKnob (parameter ("duckThr"), "Threshold", colours::uiTeal, Knob::tightStyle),
      duckReleaseKnob (parameter ("duckRel"), "Release", colours::uiTeal, Knob::tightStyle),
      freezeToggle (parameter ("freeze"), "Freeze", colours::accentYellow),
      ceilingKnob (parameter ("ceiling"), "Ceiling", colours::uiTeal, Knob::tightStyle)
{
    auto& body = getBody();

    panels.setInterceptsMouseClicks (false, false);
    panels.setBounds (0, 0, baseWidth, bodyHeight);
    body.addAndMakeVisible (panels);

    tapMap.setBounds (mapX, mapY, mapWidth, mapHeight);
    tapMap.onTapSelected = [this] (int tap) { selectTap (tap); };
    body.addAndMakeVisible (tapMap);

    // Phase 4 Wave 1: Freeze, in the tap graph's label strip, after "dry".
    freezeToggle.setBounds (mapX + 94, mapY + 10, 70, 22);
    freezeToggle.setTooltip ("Freeze: the repeats hold for as long as it's on, and new input isn't heard. Automatable.");
    body.addAndMakeVisible (freezeToggle);

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
    editLink.setBounds (642 + grownWidth, footerY - 2, 64, 24);
    outMeter.setBounds (710 + grownWidth, 10, 46, 364);

    styleLabel (wetLabel, "Wet", colours::textSecondary, juce::Justification::centred);
    styleLabel (dryLabel, "Dry", colours::textSecondary, juce::Justification::centred);

    wetLabel.setBounds (750 + grownWidth, 10, 33, 17);
    dryLabel.setBounds (783 + grownWidth, 10, 33, 17);
    wetFader.setBounds (750 + grownWidth, faderTop, 33, faderHeight);
    dryFader.setBounds (783 + grownWidth, faderTop, 33, faderHeight);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &smoothCheckbox, &editLink, &outMeter, &wetLabel, &dryLabel, &wetFader, &dryFader })
        body.addAndMakeVisible (item);

    // Phase 2.1 W.1: room in the header for undo, redo, A/B and Compare.
    setShortHeaderLabels (true);
    setPresetDropdownWidth (120);

    styleLabel (tempoLabel, {}, colours::textSecondary, juce::Justification::centredRight);
    tempoLabel.setBounds (getHeaderFreeLeft(), 13, getHeaderFreeRight() - getHeaderFreeLeft(), 24);
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

    drawerOutputKnob.setBounds (236, 56, 80, 99);

    // Phase 3 Q2.3: Swing and Ducking.
    swingKnob.setBounds (24, 100, 66, 99);
    styleLabel (duckingLabel, "DUCKING", colours::textSecondary, juce::Justification::centredLeft);
    duckingLabel.setFont (font (Weight::semiBold, 11.0f).withExtraKerningFactor (0.06f));
    duckingLabel.setBounds (350, 40, 100, 14);
    duckAmountKnob.setBounds (340, 56, 72, 99);
    duckThresholdKnob.setBounds (412, 56, 72, 99);
    duckReleaseKnob.setBounds (484, 56, 72, 99);
    swingKnob.setTooltip ("For Note-mode taps on an odd eighth note: 50 % is straight, higher pushes them later.");
    duckAmountKnob.setTooltip ("How far the repeats dip while you play above the threshold. 0 dB is off.");

    // Phase 4 Wave 1: Ceiling, after Ducking.
    ceilingKnob.setBounds (560, 56, 72, 99);
    ceilingKnob.setTooltip ("Holds the output's true peak under this level, so feedback at 100 % and over can't clip the bus. Off at the bottom.");

    for (juce::Component* item : std::initializer_list<juce::Component*> { &swingKnob, &duckingLabel, &duckAmountKnob, &duckThresholdKnob, &duckReleaseKnob, &ceilingKnob })
        drawer.addAndMakeVisible (item);
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
    if (delayProcessor().countTaps() <= 1)
        return;

    // A new tap's length follows the tap before it, so those settings don't count as changes.
    const auto changed = differsFromDefault (DelayProcessor::id (selectedTap, ""), { "used", "mode", "ms", "note", "feel", "mult" });

    confirmThen ("Delete tap " + juce::String (selectedTap + 1) + "?", "Its settings will be lost.", "Delete", changed,
                 [this] { removeTapNow(); });
}

void DelayEditor::removeTapNow()
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
        {
            // Phase 3 Q2.3: T or B on a tap with Tape or BBD character; Phase 4 Wave 3: a ring
            // with a backwards triangle on a reversed tap.
            const auto spec = processor.readTap (tap);
            const auto character = spec.character;
            used.push_back ({ tap, tapColour (tap), character == DelayDsp::tape ? "T" : character == DelayDsp::bbd ? "B" : "", spec.reverse });
        }

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

void DelayEditor::showMotionPreview()
{
    showPreview (false);

    const auto tapValue = [this] (int tap, const char* suffix, float value) { setValue (parameter (DelayProcessor::id (tap, suffix)), value); };

    // The values drawn in reference/phase3/27__delay_q23_mockup.png.
    tapValue (0, "char", 1.0f);
    tapValue (0, "charAmt", 60.0f);
    tapValue (0, "modRate", 0.6f);
    tapValue (0, "modDepth", 12.0f);

    // Phase 4 Wave 2: Triangle, as drawn in reference/phase4/57__delay_w2_mockup.png.
    tapValue (0, "modShape", 1.0f);

    updateSelection();
    tapEditors[0]->showMotion (true);
}

void DelayEditor::showReversePreview()
{
    showPreview (false);

    // reference/phase4/69__delay_w3_mockup.png, picture 2.
    setValue (parameter (DelayProcessor::id (0, "reverse")), 1.0f);
    setValue (parameter (DelayProcessor::id (0, "pitch")), 7.0f);

    updateSelection();
    tapEditors[0]->showMotion (false);
}

juce::Colour DelayEditor::getAccentColour()
{
    return colours::accentYellow;
}

std::vector<ShellEditor::PopupOpener> DelayEditor::getPopupOpeners()
{
    auto openers = ShellEditor::getPopupOpeners();
    openers.push_back ({ "dropdown-note", [this] { tapEditors[(size_t) selectedTap]->getNoteDropdown().click(); } });
    openers.push_back ({ "dropdown-multiplier", [this] { drawer.setVisible (true); multiplierDropdowns[(size_t) selectedTap]->click(); } });
    openers.push_back ({ "delete-tap", [this] { confirmThen ("Delete tap " + juce::String (selectedTap + 1) + "?", "Its settings will be lost.", "Delete", true, [] {}); } });
    openers.push_back ({ "tooltip-feedback", [this] { auto& knob = tapEditors[(size_t) selectedTap]->getFeedbackKnob(); showTip (knob, knob.getTooltip()); } });
    openers.push_back ({ "value-note", [this] { tapEditors[(size_t) selectedTap]->getNoteKnob().openValueEditor(); } });
    openers.push_back ({ "tooltip-wave", [this]
                         {
                             // The openers before it leave the drawer and the typed-value box open.
                             auto& editor = *tapEditors[(size_t) selectedTap];

                             if (auto* typed = editor.getNoteKnob().getValueEditor(); typed != nullptr && typed->onEscapeKey != nullptr)
                                 typed->onEscapeKey();

                             drawer.setVisible (false);
                             setValue (parameter (DelayProcessor::id (selectedTap, "modShape")), 1.0f);
                             updateSelection();
                             editor.showMotion (true);
                             showTip (editor.getShapeSwitch(), "Triangle: a steady detune that flips each half cycle");
                         } });
    openers.push_back ({ "tooltip-direction", [this]
                         {
                             // After tooltip-wave, which left the Motion page showing.
                             auto& editor = *tapEditors[(size_t) selectedTap];
                             drawer.setVisible (false);
                             editor.showMotion (false);
                             showTip (editor.getDirectionSwitch(), "Reverse: plays each window of one tap time backwards, so notes swell in");
                         } });
    return openers;
}
