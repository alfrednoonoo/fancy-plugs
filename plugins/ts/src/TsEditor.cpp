#include "TsEditor.h"

#include <fancy/plugin/ParamFormat.h>

#include <cmath>

using namespace fancy::ui;

namespace
{
// reference/phase4/50__transient_mockup.png (design/phase4-mockup-src/transient.html) at 100 %.
constexpr int baseWidth = 760;
constexpr int bodyHeight = 392;
constexpr int baseHeight = ShellEditor::headerHeight + bodyHeight;
constexpr int rightColumnX = 656;
constexpr int leftX = 13, contentRight = 646;
constexpr int rowY = 10, rowHeight = 26;
constexpr int displayY = 44, displayHeight = 150;
constexpr int cardY = 202, cardHeight = 137;
constexpr int attackCardX = leftX, attackCardWidth = 180;
constexpr int sustainCardX = attackCardX + attackCardWidth + 8, sustainCardWidth = 250;
constexpr int detectCardX = sustainCardX + sustainCardWidth + 8, detectCardWidth = contentRight - detectCardX;
constexpr int footerY = 362;
constexpr int faderTop = 26, faderHeight = 331;
constexpr float labelHeight = 12.0f;
constexpr float gainRangeDb = 24.0f;   // the gain trace's full height, either way

const juce::Colour orange = colours::accentOrange;

constexpr OutMeter::Geometry outGeometry { 14.5f, 16.0f, 316.0f, 26.0f, 4.9f, 324.0f, false, true };

void setValue (juce::RangedAudioParameter& parameter, float value)
{
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
}

void setUpLabel (juce::Label& label, const juce::String& text, juce::Justification justification, float height = labelHeight,
                 juce::Colour colour = colours::textSecondary)
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
        knob->setBounds (juce::roundToInt (centre) - knobWidth / 2, y, knobWidth, knobHeight);
    }
}

void paintCardTitle (juce::Graphics& g, const char* text, int x, int y)
{
    g.setColour (colours::textSecondary);
    g.setFont (font (Weight::semiBold, 12.0f).withExtraKerningFactor (0.06f));
    g.drawText (text, x + 12, y + 6, 140, 24, juce::Justification::centredLeft, false);
}
} // namespace

// Panels ----------------------------------------------------------------------

void TsEditor::Panels::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgBody);
    g.setColour (colours::bgSection);
    g.fillRect (rightColumnX, 0, baseWidth - rightColumnX, bodyHeight);

    for (const auto& [x, width] : { std::pair { attackCardX, attackCardWidth }, std::pair { sustainCardX, sustainCardWidth },
                                    std::pair { detectCardX, detectCardWidth } })
        g.fillRoundedRectangle ((float) x, (float) cardY, (float) width, (float) cardHeight, 6.0f);

    paintCardTitle (g, "ATTACK", attackCardX, cardY);
    paintCardTitle (g, "SUSTAIN", sustainCardX, cardY);
    paintCardTitle (g, "DETECT", detectCardX, cardY);
}

// History ---------------------------------------------------------------------

TsEditor::History::History (TsDsp& dsp)
    : tsDsp (dsp)
{
    setTooltip ("The input (grey), the output (orange) and the gain Fancy TS applies (white, 0 dB dashed) over the last 3 s. "
                "Each detected hit is marked with a dot.");
}

void TsEditor::History::resized()
{
    const auto width = juce::jmax (1, getWidth());
    columns.assign ((size_t) width, { -200.0f, -200.0f, 0.0f, false });
    incoming.resize ((size_t) width);
    writeIndex = 0;
    tsDsp.setHistoryColumns (width);
}

void TsEditor::History::update()
{
    if (frozen)
        return;

    const auto count = tsDsp.popHistory (incoming.data(), (int) incoming.size());

    for (int i = 0; i < count && ! columns.empty(); ++i)
    {
        columns[(size_t) writeIndex] = incoming[(size_t) i];
        writeIndex = (writeIndex + 1) % (int) columns.size();
    }

    if (count > 0)
        repaint();
}

void TsEditor::History::showColumns (std::vector<TsDsp::HistoryColumn> newColumns)
{
    frozen = true;
    columns = std::move (newColumns);
    writeIndex = 0;
    repaint();
}

void TsEditor::History::setWarning (bool shouldShow)
{
    if (warning != shouldShow)
    {
        warning = shouldShow;
        repaint();
    }
}

void TsEditor::History::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto width = (float) getWidth(), height = (float) getHeight();
    const auto centre = 22.0f + (height - 44.0f) * 0.5f + 10.0f;
    const auto halfHeight = (height - 44.0f) * 0.5f;
    const auto gainY = 22.0f;

    // Levels, mirrored about the centre line: 0 dBFS at the edges, -60 at the centre.
    const auto levelSpan = [halfHeight] (float db) { return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f) * halfHeight; };
    const auto gainOffset = [] (float db) { return -juce::jlimit (-1.0f, 1.0f, db / gainRangeDb) * 16.0f; };

    g.setColour (colours::bgBody);
    g.fillRect (0.0f, centre, width, 1.0f);

    const auto count = (int) columns.size();

    if (count > 0)
    {
        juce::Path input, outputTop, outputBottom, gainPath;
        input.startNewSubPath (0.0f, centre);

        for (int i = 0; i < count; ++i)
        {
            const auto& column = columns[(size_t) ((writeIndex + i) % count)];
            input.lineTo ((float) i, centre - levelSpan (column.inputDb));
        }

        for (int i = count - 1; i >= 0; --i)
        {
            const auto& column = columns[(size_t) ((writeIndex + i) % count)];
            input.lineTo ((float) i, centre + levelSpan (column.inputDb));
        }

        input.closeSubPath();
        g.setColour (colours::grey6);
        g.fillPath (input);

        for (int i = 0; i < count; ++i)
        {
            const auto& column = columns[(size_t) ((writeIndex + i) % count)];
            const auto x = (float) i;
            const auto span = levelSpan (column.outputDb);

            if (i == 0)
            {
                outputTop.startNewSubPath (x, centre - span);
                outputBottom.startNewSubPath (x, centre + span);
                gainPath.startNewSubPath (x, gainY + gainOffset (column.gainDb));
            }
            else
            {
                outputTop.lineTo (x, centre - span);
                outputBottom.lineTo (x, centre + span);
                gainPath.lineTo (x, gainY + gainOffset (column.gainDb));
            }
        }

        g.setColour (orange);
        g.strokePath (outputTop, juce::PathStrokeType (1.4f));
        g.strokePath (outputBottom, juce::PathStrokeType (1.4f));

        // The 0 dB line, dashed, and the gain applied.
        g.setColour (colours::grey5);

        for (auto x = 0.0f; x < width; x += 6.0f)
            g.fillRect (x, gainY - 0.5f, 3.0f, 1.0f);

        g.setColour (colours::textPrimary);
        g.strokePath (gainPath, juce::PathStrokeType (1.2f));

        // The hits.
        g.setColour (orange);

        for (int i = 0; i < count; ++i)
            if (columns[(size_t) ((writeIndex + i) % count)].hit)
                g.fillEllipse ((float) i - 2.5f, height - 26.5f, 5.0f, 5.0f);
    }

    g.setColour (colours::textSecondary);
    g.setFont (font (Weight::regular, 10.0f));
    g.drawText (juce::String::fromUTF8 ("input (grey) \xc2\xb7 output (orange) \xc2\xb7 gain (white, 0 dB dashed) \xc2\xb7 hits marked"),
                juce::Rectangle<float> (8.0f, height - 18.0f, width - 16.0f, 14.0f), juce::Justification::centredLeft, false);

    if (warning)
    {
        g.setColour (colours::uiRed);
        g.setFont (font (Weight::medium, 12.0f));
        g.drawText ("No sidechain signal: check track routing (REAPER: pins 3/4)", juce::Rectangle<float> (12.0f, 30.0f, width - 24.0f, 18.0f),
                    juce::Justification::centredLeft, false);
    }
}

// TsEditor --------------------------------------------------------------------

TsEditor::TsEditor (TsProcessor& processor)
    : ShellEditor (processor, baseWidth, baseHeight, HeaderStyle::full),
      contourSegments (parameter ("contour"), orange, colours::bgWell),
      detectorDropdown (parameter ("detector")),
      history (processor.getDsp()),
      attackKnob (parameter ("attack"), "Attack", orange, Knob::tightStyle, true),
      speedKnob (parameter ("speed"), "Speed", orange, Knob::tightStyle),
      sustainKnob (parameter ("sustain"), "Sustain", orange, Knob::tightStyle, true),
      lengthKnob (parameter ("length"), "Length", orange, Knob::tightStyle),
      pumpKnob (parameter ("pump"), "Pump", orange, Knob::tightStyle),
      sensitivityKnob (parameter ("sensitivity"), "Sensitivity", orange, Knob::tightStyle),
      highpassKnob (parameter ("highpass"), "Highpass", colours::uiBlue, Knob::tightStyle),
      clipCheckbox (parameter ("clip"), "Clip output", orange),
      linkCheckbox (parameter ("link"), "Link channels", orange),
      listenCheckbox (getFancyProcessor(), "Listen to change", orange),
      editLink (orange),
      outMeter (outGeometry),
      wetFader (parameter ("wet"), colours::uiTeal),
      dryFader (parameter ("dry"), colours::uiTeal),
      lookaheadDropdown (parameter ("lookahead")),
      oversamplingDropdown (parameter ("oversampling")),
      channelsDropdown (parameter ("channels")),
      lowpassKnob (parameter ("lowpass"), "Lowpass", colours::uiBlue, Knob::smallStyle),
      deadbandKnob (parameter ("deadband"), "Dead band", orange, Knob::smallStyle),
      outputKnob (parameter ("output"), "Output", colours::uiTeal, Knob::smallStyle, true)
{
    auto& body = getBody();

    // Phase 2.1 W.1: room in the header for undo, redo, A/B and Compare.
    setShortHeaderLabels (true);
    setPresetDropdownWidth (120);

    panels.setInterceptsMouseClicks (false, false);
    panels.setBounds (0, 0, baseWidth, bodyHeight);
    body.addAndMakeVisible (panels);

    // The top row.
    setUpLabel (contourLabel, "Contour", juce::Justification::centredLeft);
    contourLabel.setBounds (leftX + 2, rowY, 52, rowHeight);
    contourSegments.setBounds (70, rowY + 1, 190, 24);
    contourSegments.setTextHeight (labelHeight);
    contourSegments.setFitToText (true);
    contourSegments.setTooltip ("The shape of Sustain's work over Length after each hit. Sharp lets go quickly and then gently, "
                                "Medium evenly, Smooth holds on and then lets go.");

    setUpLabel (detectorLabel, "Detector", juce::Justification::centredRight);
    detectorLabel.setBounds (400, rowY, 64, rowHeight);
    detectorDropdown.setBounds (470, rowY + 1, 140, 24);
    detectorDropdown.setTextHeight (labelHeight);
    detectorDropdown.addSection (0, "Main input");
    detectorDropdown.addSection (1, "Sidechain");
    detectorDropdown.addSection (2, "Mid/side");
    detectorDropdown.setTooltip ("What finds the hits: this track, the sidechain (pins 3/4 in REAPER), or the mid or side of this track.");

    history.setBounds (leftX, displayY, contentRight - leftX, displayHeight);

    // Cards.
    cardKnobs ({ &attackKnob, &speedKnob }, attackCardX, attackCardWidth, cardY + 30);
    cardKnobs ({ &sustainKnob, &lengthKnob, &pumpKnob }, sustainCardX, sustainCardWidth, cardY + 30);
    cardKnobs ({ &sensitivityKnob, &highpassKnob }, detectCardX, detectCardWidth, cardY + 30);

    attackKnob.setTooltip ("Turns the start of each hit up (right) or down (left), the same at any level.");
    speedKnob.setTooltip ("How long the start of a hit counts as the attack. Short for clicks, longer for plucks.");
    sustainKnob.setTooltip ("Turns the ring after each hit up (right) or down (left).");
    lengthKnob.setTooltip ("How long after each hit Sustain works. It never reaches the noise between hits.");
    pumpKnob.setTooltip ("Dips the level just after each attack, for a pumping, breathing feel.");
    sensitivityKnob.setTooltip ("How small a jump in level counts as a hit. Higher catches softer hits.");
    highpassKnob.setTooltip ("Keeps low notes from triggering the detector, so a bass line doesn't count as hits. Off at the bottom.");

    for (auto* knob : { &attackKnob, &sustainKnob })
        knob->setShowDefaultMark (true);

    // Footer.
    clipCheckbox.setBounds (leftX, footerY, 100, 20);
    linkCheckbox.setBounds (leftX + 112, footerY, 110, 20);
    listenCheckbox.setBounds (leftX + 234, footerY, 140, 20);
    editLink.setBounds (contentRight - 64, footerY - 2, 64, 24);

    for (auto* checkbox : { &clipCheckbox, &linkCheckbox })
        checkbox->setLabelHeight (labelHeight);

    listenCheckbox.setLabelHeight (labelHeight);

    clipCheckbox.setTooltip ("A soft clipper on the output, so a raised attack can't clip harshly. With Oversampling on it adds "
                             "latency while it's ticked.");
    linkCheckbox.setTooltip ("Both channels shaped together, from the louder one. Off shapes each channel on its own.");
    listenCheckbox.setTooltip ("Plays only what Fancy TS changes. Turns off when you close this window, and is never saved.");

    // Right column, as Sature's.
    outMeter.setBounds (rightColumnX - 6, 10, 46, 350);
    setUpLabel (wetLabel, "Wet", juce::Justification::centred, 14.0f, colours::textPrimary);
    setUpLabel (dryLabel, "Dry", juce::Justification::centred, 14.0f, colours::textPrimary);
    wetLabel.setBounds (690, 10, 33, 17);
    dryLabel.setBounds (723, 10, 33, 17);
    wetFader.setBounds (690, faderTop, 33, faderHeight);
    dryFader.setBounds (723, faderTop, 33, faderHeight);
    wetFader.setTooltip ("The shaped signal's level.");
    dryFader.setTooltip ("The untouched signal's level, mixed in for parallel shaping. It stays lined up with the shaped one.");

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &contourLabel, &contourSegments, &detectorLabel, &detectorDropdown, &history,
             &attackKnob, &speedKnob, &sustainKnob, &lengthKnob, &pumpKnob, &sensitivityKnob, &highpassKnob,
             &clipCheckbox, &linkCheckbox, &listenCheckbox, &editLink,
             &outMeter, &wetLabel, &dryLabel, &wetFader, &dryFader })
        body.addAndMakeVisible (item);

    // EDIT drawer.
    drawer.setBounds (0, displayY - 4, rightColumnX, bodyHeight - displayY + 4);

    const std::pair<juce::Label*, const char*> rows[] { { &lookaheadLabel, "Lookahead" }, { &oversamplingLabel, "Oversampling" }, { &channelsLabel, "Channels" } };
    juce::Component* controls[] { &lookaheadDropdown, &oversamplingDropdown, &channelsDropdown };

    for (int i = 0; i < 3; ++i)
    {
        setUpLabel (*rows[i].first, rows[i].second, juce::Justification::centredLeft, 14.0f, colours::textPrimary);
        rows[i].first->setBounds (24, 60 + 40 * i, 150, 24);
        controls[i]->setBounds (170, 60 + 40 * i, 150, 24);
        drawer.addAndMakeVisible (rows[i].first);
        drawer.addAndMakeVisible (controls[i]);
    }

    setUpLabel (latencyNote, {}, juce::Justification::centredLeft, labelHeight);
    latencyNote.setBounds (330, 60, 80, 24);

    lowpassKnob.setBounds (400, 96, 76, 99);
    deadbandKnob.setBounds (480, 96, 76, 99);
    outputKnob.setBounds (560, 96, 76, 99);

    setUpLabel (drawerNote, "Lookahead (Off, 1, 2 or 5 ms) lets Attack catch the very first sample of a hit; it adds that much latency. "
                            "Oversampling is for Clip output. Channels: Stereo, Mid only, Side only. "
                            "Dead band ignores hits that would change the level by less than this.",
                juce::Justification::topLeft, 11.0f);
    drawerNote.setBounds (24, 210, 560, 48);

    lookaheadDropdown.setTooltip ("Delays the audio so Attack can act on a hit's very first sample. Adds that much latency.");
    oversamplingDropdown.setTooltip ("Runs Clip output at a higher rate so it doesn't alias. Adds latency only while Clip output is on.");
    channelsDropdown.setTooltip ("Shape both channels, or only the mid (centre) or the side (width).");
    lowpassKnob.setTooltip ("Keeps highs from triggering the detector. Off at the top.");
    deadbandKnob.setTooltip ("Hits that would change the level by less than this are left alone, so small wobbles aren't shaped.");
    addLatencyNote (lookaheadDropdown);
    addLatencyNote (oversamplingDropdown);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &latencyNote, &lowpassKnob, &deadbandKnob, &outputKnob, &drawerNote })
        drawer.addAndMakeVisible (item);

    body.addChildComponent (drawer);
    editLink.onClick = [this] { drawer.setVisible (! drawer.isVisible()); };

    updateLocks();
}

TsEditor::~TsEditor() = default;

TsProcessor& TsEditor::tsProcessor() noexcept
{
    return dynamic_cast<TsProcessor&> (getFancyProcessor());
}

void TsEditor::updateLocks()
{
    const auto targets = tsProcessor().readTargets();
    const auto latency = tsProcessor().getDsp().latencyFor (targets.lookahead, targets.clip, targets.oversampling);

    latencyNote.setText ("(+" + juce::String (latency) + (latency == 1 ? " sample)" : " samples)"), juce::dontSendNotification);

    // Oversampling only matters while Clip output is on (Decision 2 A).
    oversamplingDropdown.setAlpha (targets.clip ? 1.0f : 0.5f);
}

void TsEditor::onTick (float seconds)
{
    auto& processor = tsProcessor();

    updateLocks();
    history.setWarning (processor.getDsp().isSidechainMissing());
    history.update();
    outMeter.update (processor.outputMeter, seconds);
}

void TsEditor::showPreview (bool drawerOpen)
{
    stopUpdates();

    setValue (parameter ("attack"), 6.0f);
    setValue (parameter ("speed"), 8.0f);
    setValue (parameter ("sustain"), -4.0f);
    setValue (parameter ("length"), 180.0f);
    setValue (parameter ("highpass"), 40.0f);
    setValue (parameter ("clip"), 1.0f);

    // The mockup's history: hits every 152 px, boosted at the start and pulled down in the tail.
    std::vector<TsDsp::HistoryColumn> columns;
    juce::Random random (4);

    for (int x = 0; x < contentRight - leftX; ++x)
    {
        const auto t = (float) ((x + 40) % 152) / 152.0f;
        const auto inputDb = -6.0f - 60.0f * t - 3.0f * random.nextFloat();
        const auto gainDb = t < 0.06f ? 6.0f : (t < 0.5f ? -4.0f : -1.0f);
        columns.push_back ({ inputDb, juce::jmin (-0.5f, inputDb + gainDb), gainDb, (x + 40) % 152 == 0 });
    }

    history.showColumns (std::move (columns));
    history.setWarning (false);
    outMeter.show (-6.4f, -6.2f);
    updateLocks();

    drawer.setVisible (drawerOpen);
}

juce::Colour TsEditor::getAccentColour()
{
    return orange;
}

std::vector<ShellEditor::PopupOpener> TsEditor::getPopupOpeners()
{
    auto openers = ShellEditor::getPopupOpeners();
    openers.push_back ({ "dropdown-detector", [this] { detectorDropdown.click(); } });
    openers.push_back ({ "tooltip-listen", [this] { showTip (listenCheckbox, listenCheckbox.getTooltip()); } });
    openers.push_back ({ "value-attack", [this] { attackKnob.openValueEditor(); } });
    return openers;
}
