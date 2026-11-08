#include "GateEditor.h"

#include <fancy/plugin/ParamFormat.h>

#include <cmath>

using namespace fancy::ui;

namespace
{
// design/phase2_0_1-mockup-src/gate.html at 100 %. The shared header is 16 px taller than the
// mockup's, so the window is too.
constexpr int baseWidth = 740;
constexpr int bodyHeight = 368;
constexpr int baseHeight = ShellEditor::headerHeight + bodyHeight;
constexpr int rightColumnX = 590;

constexpr int centreX = 120, centreWidth = 456;
constexpr int historyY = 40, historyHeight = 70;
constexpr int envelopeY = 118, cardHeight = 96;
constexpr int lowerY = 222, levelWidth = 226, filterX = 354, filterWidth = 222;
constexpr int footerY = 336;
constexpr int faderTop = 26, faderHeight = 331;

constexpr float labelHeight = 10.5f;   // the mockup's captions and checkboxes
constexpr float historyFloorDb = -90.0f;

const juce::Colour gateYellow = colours::accentYellow;
const juce::Colour noiseGrey { 0xff909090 };

constexpr OutMeter::Geometry outGeometry { 14.5f, 16.0f, 316.0f, 26.0f, 4.9f, 324.0f, false, true };
constexpr GainReductionMeter::Geometry gainGeometry { 16.0f, 14.0f, 232.0f, true, "Gain" };

ThresholdMeter::Style thresholdStyle()
{
    ThresholdMeter::Style style;
    style.wellX = 8.0f;
    style.wellTop = 14.0f;
    style.wellHeight = 232.0f;
    style.lowestDb = -90.0f;
    style.accent = gateYellow;
    style.labelHeight = labelHeight;
    style.secondaryLabel = true;
    style.scale = true;
    style.gateReadouts = true;
    style.scaleStepDb = 18.0f;
    style.scaleTextHeight = 8.0f;
    style.readoutTextHeight = 10.5f;
    return style;
}

void setValue (juce::RangedAudioParameter& parameter, float value)
{
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
}

void styleLabel (juce::Label& label, const juce::String& text, juce::Colour colour, juce::Justification justification, float height = 14.0f)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (font (Weight::regular, height));
    label.setColour (juce::Label::textColourId, colour);
    label.setJustificationType (justification);
    label.setBorderSize ({});
    label.setInterceptsMouseClicks (false, false);
}

void cardKnobs (std::initializer_list<Knob*> knobs, int x, int width, int y)
{
    constexpr int knobWidth = 76, knobHeight = 92;

    const auto spacing = (float) width / (float) knobs.size();
    auto index = 0;

    for (auto* knob : knobs)
    {
        const auto centre = (float) x + spacing * ((float) index++ + 0.5f);
        knob->setBounds (juce::roundToInt (centre) - knobWidth / 2, y + 2, knobWidth, knobHeight);
    }
}
} // namespace

const char* const GateEditor::preOpenHint = "Pre-open needs Lookahead. Tick 'Lookahead (+50 ms latency)' above to use it.";

juce::String GateEditor::velocityTooltip (const GateDsp::Targets& targets)
{
    const auto late = GateDsp::noteLatenessMs (targets);

    if (late <= 0.0)
        return "Velocity is the peak level over the velocity window after the gate opens, from -60 dBFS (1) to 0 dBFS (127).";

    const auto amount = fancy::formatFixed ((float) late, late < 10.0 ? 1 : 0);

    if (targets.lookahead)
        return "Notes are sent " + amount + " ms late to measure how hard each hit is. Lower Pre-open for on-time notes.";

    return "Notes are sent " + amount + " ms late to measure how hard each hit is. Turn on Lookahead for on-time notes.";
}

// Panels ----------------------------------------------------------------------

void GateEditor::Panels::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgBody);

    g.setColour (colours::bgSection);
    g.fillRect (rightColumnX, 0, baseWidth - rightColumnX, bodyHeight);

    g.fillRoundedRectangle ((float) centreX, (float) historyY, (float) centreWidth, (float) historyHeight, 6.0f);
    g.fillRoundedRectangle ((float) centreX, (float) envelopeY, (float) centreWidth, (float) cardHeight, 6.0f);
    g.fillRoundedRectangle ((float) centreX, (float) lowerY, (float) levelWidth, (float) cardHeight, 6.0f);
    g.fillRoundedRectangle ((float) filterX, (float) lowerY, (float) filterWidth, (float) cardHeight, 6.0f);
}

// History ---------------------------------------------------------------------

GateEditor::History::History (GateDsp& dsp, juce::RangedAudioParameter& threshold, juce::RangedAudioParameter& hysteresis)
    : gateDsp (dsp), thresholdParam (threshold), hysteresisParam (hysteresis)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTooltip ("Input level (grey) and the gate's gain (yellow) over the last 3 s. Click or drag to set Threshold.");
}

void GateEditor::History::resized()
{
    columns.assign ((size_t) juce::jmax (1, getWidth()), { historyFloorDb, 0.0f });
    incoming.resize (columns.size());
    writeIndex = 0;
    gateDsp.setHistoryColumns (getWidth());
}

void GateEditor::History::update()
{
    if (columns.empty())
        return;

    const auto count = gateDsp.popHistory (incoming.data(), (int) incoming.size());

    for (int i = 0; i < count; ++i)
    {
        columns[(size_t) writeIndex] = incoming[(size_t) i];
        writeIndex = (writeIndex + 1) % (int) columns.size();
    }

    repaint();
}

void GateEditor::History::showColumns (std::vector<GateDsp::HistoryColumn> newColumns)
{
    newColumns.resize (columns.size(), { historyFloorDb, 0.0f });
    columns = std::move (newColumns);
    writeIndex = 0;
    repaint();
}

void GateEditor::History::setWarning (bool shouldShow)
{
    if (shouldShow != warning)
    {
        warning = shouldShow;
        repaint();
    }
}

float GateEditor::History::yForDb (float db) const
{
    const auto height = (float) getHeight() - 12.0f;
    return 6.0f + height * juce::jlimit (0.0f, 1.0f, db / historyFloorDb);
}

float GateEditor::History::dbForY (float y) const
{
    const auto height = (float) getHeight() - 12.0f;
    return juce::jlimit (historyFloorDb, 0.0f, (y - 6.0f) / height * historyFloorDb);
}

void GateEditor::History::paint (juce::Graphics& g)
{
    const auto width = getWidth();
    const auto height = (float) getHeight();
    const auto size = (int) columns.size();

    g.reduceClipRegion (getLocalBounds());

    // Oldest on the left.
    juce::Path level, gain;
    level.startNewSubPath (0.0f, height);

    for (int x = 0; x < juce::jmin (width, size); ++x)
    {
        const auto& column = columns[(size_t) ((writeIndex + x) % size)];
        const auto levelY = yForDb (column.levelDb);
        const auto gainY = yForDb (column.gainDb);

        level.lineTo ((float) x, levelY);
        level.lineTo ((float) x + 1.0f, levelY);

        if (x == 0)
            gain.startNewSubPath (0.0f, gainY);
        else
            gain.lineTo ((float) x + 0.5f, gainY);
    }

    level.lineTo ((float) width, height);
    level.closeSubPath();

    g.setColour (noiseGrey.withAlpha (0.28f));
    g.fillPath (level);

    const auto openDb = thresholdParam.convertFrom0to1 (thresholdParam.getValue());
    const auto closeDb = openDb + hysteresisParam.convertFrom0to1 (hysteresisParam.getValue());

    g.setColour (gateYellow.withAlpha (0.7f));
    g.fillRect (0.0f, yForDb (openDb) - 0.5f, (float) width, 1.0f);

    g.setColour (gateYellow.withAlpha (0.6f));

    for (auto x = 0.0f; x < (float) width; x += 6.0f)
        g.fillRect (x, yForDb (closeDb) - 0.5f, 3.0f, 1.0f);

    g.setColour (gateYellow);
    g.strokePath (gain, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (warning)
    {
        g.setFont (font (Weight::medium, 12.0f));
        g.setColour (colours::bgSection.withAlpha (0.85f));
        g.fillRoundedRectangle (6.0f, 5.0f, 364.0f, 18.0f, 3.0f);
        g.setColour (colours::accentRed);
        g.drawText ("No sidechain signal: check track routing (REAPER: pins 3/4)", juce::Rectangle<float> (12.0f, 5.0f, 356.0f, 18.0f),
                    juce::Justification::centredLeft, false);
    }
}

void GateEditor::History::setThresholdAt (float y)
{
    thresholdParam.setValueNotifyingHost (thresholdParam.convertTo0to1 (dbForY (y)));
    repaint();
}

void GateEditor::History::mouseDown (const juce::MouseEvent& event)
{
    thresholdParam.beginChangeGesture();
    setThresholdAt (event.position.y);
}

void GateEditor::History::mouseDrag (const juce::MouseEvent& event)
{
    setThresholdAt (event.position.y);
}

void GateEditor::History::mouseUp (const juce::MouseEvent&)
{
    thresholdParam.endChangeGesture();
}

// Light -----------------------------------------------------------------------

void GateEditor::Light::setState (int newState)
{
    if (newState != state)
    {
        state = newState;
        repaint();
    }
}

void GateEditor::Light::paint (juce::Graphics& g)
{
    const auto lit = state != GateDsp::lightClosed;
    const auto colour = state == GateDsp::lightOpen ? gateYellow : state == GateDsp::lightHold ? colours::accentOrange : colours::grey6;
    const auto text = state == GateDsp::lightOpen ? "open" : state == GateDsp::lightHold ? "hold" : "closed";

    g.setFont (font (Weight::regular, labelHeight));

    const auto textWidth = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text);
    const auto left = ((float) getWidth() - 11.0f - textWidth) * 0.5f;
    const auto dot = juce::Rectangle<float> (left, (float) getHeight() * 0.5f - 3.5f, 7.0f, 7.0f);

    if (lit)
    {
        g.setColour (colour.withAlpha (0.35f));
        g.fillEllipse (dot.expanded (2.5f));
    }

    g.setColour (colour);
    g.fillEllipse (dot);

    g.setColour (lit ? colour : colours::textSecondary);
    g.drawText (text, juce::Rectangle<float> (left + 11.0f, 0.0f, textWidth + 2.0f, (float) getHeight()), juce::Justification::centredLeft, false);
}

// MidiDropdown ----------------------------------------------------------------

GateEditor::MidiDropdown::MidiDropdown (GateProcessor& processor)
    : gateProcessor (processor)
{
    setTooltip ("MIDI note, channel and velocity window");
    refresh();
}

void GateEditor::MidiDropdown::refresh()
{
    const auto targets = gateProcessor.readTargets();
    const auto newText = GateProcessor::noteName (targets.note) + juce::String::fromUTF8 (" \xc2\xb7 ch ") + juce::String (targets.channel);

    if (newText != text)
    {
        text = newText;
        repaint();
    }
}

void GateEditor::MidiDropdown::paint (juce::Graphics& g)
{
    paintDropdownBox (g, getLocalBounds().toFloat(), text, 10.0f);
}

void GateEditor::MidiDropdown::mouseUp (const juce::MouseEvent& event)
{
    if (! event.mouseWasClicked() || ! isEnabled())
        return;

    const auto targets = gateProcessor.readTargets();

    // Ids: notes 1-128, channels 201-216, windows 301-309.
    juce::PopupMenu notes, channels, windows;

    for (int octave = -1; octave <= 9; ++octave)
    {
        juce::PopupMenu octaveMenu;
        auto containsCurrent = false;

        for (int note = (octave + 1) * 12; note < juce::jmin (128, (octave + 2) * 12); ++note)
        {
            octaveMenu.addItem (1 + note, GateProcessor::noteName (note) + "  (" + juce::String (note) + ")", true, note == targets.note);
            containsCurrent = containsCurrent || note == targets.note;
        }

        notes.addSubMenu ("Octave " + juce::String (octave), octaveMenu, true, nullptr, containsCurrent);
    }

    for (int channel = 1; channel <= 16; ++channel)
        channels.addItem (200 + channel, "Channel " + juce::String (channel), true, channel == targets.channel);

    for (int step = 0; step < 9; ++step)
    {
        const auto ms = 1.0 + 0.5 * step;
        windows.addItem (301 + step, fancy::formatFixed ((float) ms, 1) + " ms", true, std::abs (ms - targets.velocityWindowMs) < 0.01);
    }

    juce::PopupMenu menu;
    menu.addSubMenu ("Note: " + GateProcessor::noteName (targets.note), notes);
    menu.addSubMenu ("Channel: " + juce::String (targets.channel), channels);
    menu.addSubMenu ("Velocity window: " + fancy::formatFixed ((float) targets.velocityWindowMs, 1) + " ms", windows);

    juce::Component::SafePointer<MidiDropdown> safe (this);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int result)
    {
        if (safe == nullptr || result == 0)
            return;

        auto& processor = safe->gateProcessor;

        if (result >= 301)
            processor.setParameter ("velWindow", 1.0f + 0.5f * (float) (result - 301));
        else if (result >= 201)
            processor.setParameter ("midiChannel", (float) (result - 200));
        else
            processor.setParameter ("midiNote", (float) (result - 1));

        safe->refresh();
    });
}

// GateEditor ------------------------------------------------------------------

GateEditor::GateEditor (GateProcessor& processor)
    : ShellEditor (processor, baseWidth, baseHeight, HeaderStyle::full),
      detectorDropdown (parameter ("detector")),
      lookaheadCheckbox (parameter ("lookahead"), "Lookahead", gateYellow),
      modeSegments (parameter ("mode"), gateYellow, colours::bgHeader),
      thresholdMeter (parameter ("threshold"), thresholdStyle()),
      gainMeter (gainGeometry),
      history (processor.getDsp(), parameter ("threshold"), parameter ("hysteresis")),
      preOpenKnob (parameter ("preopen"), "Pre-open", gateYellow, Knob::tightStyle),
      attackKnob (parameter ("attack"), "Attack", gateYellow, Knob::tightStyle),
      holdKnob (parameter ("hold"), "Hold", gateYellow, Knob::tightStyle),
      releaseKnob (parameter ("release"), "Release", gateYellow, Knob::tightStyle),
      hysteresisKnob (parameter ("hysteresis"), "Hysteresis", gateYellow, Knob::tightStyle, true),
      rangeKnob (parameter ("range"), "Range", gateYellow, Knob::tightStyle),
      rmsKnob (parameter ("rms"), "RMS size", gateYellow, Knob::tightStyle),
      lowpassKnob (parameter ("lowpass"), "Lowpass", colours::uiBlue, Knob::tightStyle),
      highpassKnob (parameter ("highpass"), "Highpass", colours::uiBlue, Knob::tightStyle),
      previewCheckbox (parameter ("preview"), "Preview filter", gateYellow),
      linkCheckbox (parameter ("link"), "Link channels", gateYellow),
      midiCheckbox (parameter ("midi"), "MIDI on open/close", gateYellow),
      velocityCheckbox (parameter ("velocity"), "Velocity from level", gateYellow),
      midiDropdown (processor),
      outMeter (outGeometry),
      wetFader (parameter ("wet"), colours::uiTeal),
      dryFader (parameter ("dry"), colours::uiTeal),
      noiseFader (parameter ("noise"), noiseGrey)
{
    auto& body = getBody();

    setPresetDropdownWidth (170);

    panels.setInterceptsMouseClicks (false, false);
    panels.setBounds (0, 0, baseWidth, bodyHeight);
    body.addAndMakeVisible (panels);

    // The row under the header.
    styleLabel (detectorLabel, "Detector input", colours::textSecondary, juce::Justification::centredLeft, labelHeight);
    styleLabel (latencyLabel, "(+50 ms latency)", colours::textSecondary, juce::Justification::centredLeft, labelHeight);
    styleLabel (modeLabel, "Mode", colours::textSecondary, juce::Justification::centredRight, labelHeight);

    detectorLabel.setBounds (13, 9, 76, 24);
    detectorDropdown.setBounds (90, 9, 106, 24);
    detectorDropdown.setTextHeight (11.0f);
    lookaheadCheckbox.setBounds (210, 11, 84, 20);
    lookaheadCheckbox.setLabelHeight (labelHeight);
    latencyLabel.setBounds (294, 9, 100, 24);
    modeLabel.setBounds (440, 9, 40, 24);
    modeSegments.setBounds (486, 9, 90, 24);
    modeSegments.setTextHeight (labelHeight);

    // Left column.
    thresholdMeter.setBounds (8, historyY, 58, 276);
    gainMeter.setBounds (68, historyY, 40, 269);
    light.setBounds (60, historyY + 271, 56, 16);
    light.setInterceptsMouseClicks (false, false);

    // Centre.
    history.setBounds (centreX, historyY, centreWidth, historyHeight);

    cardKnobs ({ &preOpenKnob, &attackKnob, &holdKnob, &releaseKnob, &hysteresisKnob }, centreX, centreWidth, envelopeY);
    cardKnobs ({ &rangeKnob, &rmsKnob }, centreX, levelWidth, lowerY);
    cardKnobs ({ &lowpassKnob, &highpassKnob }, filterX, filterWidth, lowerY);

    for (auto* knob : { &attackKnob, &holdKnob, &releaseKnob, &rmsKnob })
        knob->setShowDefaultMark (true);

    hysteresisKnob.setTooltip ("How far below Threshold the level must fall before the gate closes. "
                               "It stops chatter on signals that hover around the threshold.");
    rangeKnob.setTooltip ("How far the closed gate turns the signal down. -inf is silence.");
    holdKnob.setTooltip ("How long the gate stays open after the level falls below the close threshold.");

    // Footer.
    previewCheckbox.setBounds (14, footerY, 98, 20);
    linkCheckbox.setBounds (116, footerY, 98, 20);
    midiCheckbox.setBounds (218, footerY, 128, 20);
    midiDropdown.setBounds (346, footerY - 2, 78, 24);
    velocityCheckbox.setBounds (432, footerY, 140, 20);

    for (auto* checkbox : { &previewCheckbox, &linkCheckbox, &midiCheckbox, &velocityCheckbox })
        checkbox->setLabelHeight (labelHeight);

    previewCheckbox.setTooltip ("Listen to what the detector hears, through Lowpass and Highpass.");
    linkCheckbox.setTooltip ("Both channels open and close together, on the louder one.");
    midiCheckbox.setTooltip ("Sends a note when the gate opens, and its note-off when it closes. "
                             "In REAPER, route this track's MIDI to an instrument track.");

    // Right column.
    outMeter.setBounds (rightColumnX + 6, 10, 46, 350);

    styleLabel (wetLabel, "Wet", colours::textSecondary, juce::Justification::centred, labelHeight);
    styleLabel (dryLabel, "Dry", colours::textSecondary, juce::Justification::centred, labelHeight);
    styleLabel (noiseLabel, "Noise", colours::textSecondary, juce::Justification::centred, labelHeight);

    constexpr int faderWidth = 33, faderX = rightColumnX + 50;

    wetLabel.setBounds (faderX, 10, faderWidth, 17);
    dryLabel.setBounds (faderX + faderWidth, 10, faderWidth, 17);
    noiseLabel.setBounds (faderX + 2 * faderWidth, 10, faderWidth, 17);
    wetFader.setBounds (faderX, faderTop, faderWidth, faderHeight);
    dryFader.setBounds (faderX + faderWidth, faderTop, faderWidth, faderHeight);
    noiseFader.setBounds (faderX + 2 * faderWidth, faderTop, faderWidth, faderHeight);

    noiseFader.setTooltip ("White noise, only while the gate is open. In Duck mode, while it ducks.");

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &detectorLabel, &detectorDropdown, &lookaheadCheckbox, &latencyLabel, &modeLabel, &modeSegments,
             &thresholdMeter, &gainMeter, &light, &history,
             &preOpenKnob, &attackKnob, &holdKnob, &releaseKnob, &hysteresisKnob,
             &rangeKnob, &rmsKnob, &lowpassKnob, &highpassKnob,
             &previewCheckbox, &linkCheckbox, &midiCheckbox, &midiDropdown, &velocityCheckbox,
             &outMeter, &wetLabel, &dryLabel, &noiseLabel, &wetFader, &dryFader, &noiseFader })
        body.addAndMakeVisible (item);

    preOpenKnob.setTooltip ("How early the gate starts to open, before the level crosses Threshold.");
    preOpenKnob.onLockedClick = [this]
    {
        showTip (preOpenKnob, preOpenHint);
        lookaheadCheckbox.pulse (gateYellow, 600);
    };

    updateLocks();
}

GateProcessor& GateEditor::gateProcessor() noexcept
{
    return dynamic_cast<GateProcessor&> (getFancyProcessor());
}

void GateEditor::updateLocks()
{
    const auto targets = gateProcessor().readTargets();

    const auto preOpenLocked = ! targets.lookahead;
    preOpenKnob.setLocked (preOpenLocked, preOpenHint);
    preOpenKnob.setAlpha (preOpenLocked ? 0.5f : 1.0f);
    preOpenKnob.setValueTextOverride (preOpenLocked ? "off" : juce::String {});

    // The MIDI settings only matter while MIDI is on. They stay usable, just dimmed.
    midiDropdown.setAlpha (targets.midi ? 1.0f : 0.5f);
    velocityCheckbox.setAlpha (targets.midi ? 1.0f : 0.5f);
    midiDropdown.refresh();

    velocityCheckbox.setTooltip (velocityTooltip (targets));

    thresholdMeter.setCloseDb ((float) (targets.thresholdDb + targets.hysteresisDb));
}

void GateEditor::onTick (float seconds)
{
    auto& processor = gateProcessor();
    auto& dsp = processor.getDsp();

    updateLocks();

    const auto reduction = juce::jmin (dsp.takeReduction(), 80.0f);

    thresholdMeter.update (processor.inputMeter, false, seconds);
    gainMeter.update (reduction, reduction, seconds);
    light.setState (dsp.getLight());
    history.setWarning (dsp.isSidechainMissing());
    history.update();
    outMeter.update (processor.outputMeter, seconds);
}

void GateEditor::showPreview (bool secondView)
{
    stopUpdates();

    setValue (parameter ("threshold"), -38.0f);
    setValue (parameter ("hysteresis"), -6.0f);
    setValue (parameter ("hold"), 40.0f);
    setValue (parameter ("release"), 120.0f);
    setValue (parameter ("range"), -40.0f);
    setValue (parameter ("lowpass"), 12000.0f);
    setValue (parameter ("highpass"), 120.0f);
    setValue (parameter ("lookahead"), secondView ? 1.0f : 0.0f);
    setValue (parameter ("midi"), secondView ? 1.0f : 0.0f);
    setValue (parameter ("velocity"), secondView ? 1.0f : 0.0f);
    setValue (parameter ("detector"), secondView ? 3.0f : 0.0f);

    updateLocks();

    // The mockup's history: hits every 76 px that decay into a noise floor, and the gate
    // opening and closing on them.
    std::vector<GateDsp::HistoryColumn> columns;
    juce::Random random (5);
    auto open = false;
    auto gainDb = -40.0f;

    for (int x = 0; x < centreWidth; ++x)
    {
        const auto t = x % 76;
        const auto levelDb = t < 2 ? -12.0f : juce::jmax (-50.0f + 6.0f * random.nextFloat(), -12.0f - (float) t * 0.55f) - 2.0f * random.nextFloat();

        if (levelDb > -38.0f)
            open = true;
        else if (levelDb < -44.0f)
            open = false;

        gainDb += ((open ? 0.0f : -40.0f) - gainDb) * (open ? 0.9f : 0.06f);
        columns.push_back ({ levelDb, gainDb });
    }

    history.showColumns (std::move (columns));
    history.setWarning (secondView);

    thresholdMeter.show (-26.0f, -28.0f, false);
    gainMeter.show (6.8f, 6.8f);
    light.setState (GateDsp::lightOpen);
    outMeter.show (-8.1f, -8.6f);
}
