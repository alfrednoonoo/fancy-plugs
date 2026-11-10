#include "DsEditor.h"

#include <fancy/plugin/ParamFormat.h>

#include <cmath>

using namespace fancy::ui;

namespace
{
// design/phase3-mockup-src/ds.html at 100 %.
constexpr int baseWidth = 760;
constexpr int bodyHeight = 392;
constexpr int baseHeight = ShellEditor::headerHeight + bodyHeight;
constexpr int rightColumnX = 660;

constexpr int leftX = 13, contentRight = 650;
constexpr int rowY = 10, rowHeight = 26;
constexpr int columnY = 44;
constexpr int displayX = 119, displayWidth = contentRight - displayX, displayHeight = 160;
constexpr int cardY = 212, cardHeight = 127, cardGap = 8;
constexpr int detectionWidth = 219, reductionX = displayX + detectionWidth + cardGap, reductionWidth = contentRight - reductionX;
constexpr int footerY = 362;
constexpr int faderTop = 26, faderHeight = 331;

constexpr float labelHeight = 12.0f;
constexpr float historyFloorDb = -60.0f, historyReductionDb = 30.0f;
constexpr double rulerLowHz = 1000.0, rulerHighHz = 20000.0;

const juce::Colour dsGreen = colours::accentGreen;

constexpr OutMeter::Geometry outGeometry { 14.5f, 16.0f, 316.0f, 26.0f, 4.9f, 324.0f, false, true };
constexpr GainReductionMeter::Geometry gainGeometry { 13.0f, 20.0f, 246.0f, true, "GR" };

ThresholdMeter::Style thresholdStyle()
{
    ThresholdMeter::Style style;
    style.wellX = 12.0f;
    style.wellTop = 20.0f;
    style.wellHeight = 246.0f;
    style.lowestDb = -60.0f;
    style.accent = dsGreen;
    style.labelHeight = labelHeight;
    style.secondaryLabel = true;
    style.scale = true;
    style.gateReadouts = true;
    style.scaleStepDb = 12.0f;
    style.scaleTextHeight = 9.0f;
    style.readoutTextHeight = 12.0f;
    return style;
}

void setValue (juce::RangedAudioParameter& parameter, float value)
{
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
}

void setUpLabel (juce::Label& label, const juce::String& text, juce::Justification justification, float height = labelHeight)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (font (Weight::regular, height));
    label.setColour (juce::Label::textColourId, colours::textSecondary);
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

const char* const DsEditor::widthHint = "Width only applies to Band. Shelf turns down everything above Freq.";

// Panels ----------------------------------------------------------------------

void DsEditor::Panels::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgBody);

    g.setColour (colours::bgSection);
    g.fillRect (rightColumnX, 0, baseWidth - rightColumnX, bodyHeight);
    g.fillRoundedRectangle ((float) displayX, (float) cardY, (float) detectionWidth, (float) cardHeight, 6.0f);
    g.fillRoundedRectangle ((float) reductionX, (float) cardY, (float) reductionWidth, (float) cardHeight, 6.0f);

    paintCardTitle (g, "DETECTION", displayX, cardY);
    paintCardTitle (g, "REDUCTION", reductionX, cardY);
}

// Display ---------------------------------------------------------------------

DsEditor::Display::Display (DsProcessor& processor)
    : dsProcessor (processor)
{
    setTooltip ("The sibilance level (grey) and the reduction (green) over the last 3 s. Drag up or down for Threshold. "
                "Below, the band: drag the blue handle for Freq, or the band's edges for Width.");
}

void DsEditor::Display::resized()
{
    columns.assign ((size_t) juce::jmax (1, getWidth()), { -200.0f, 0.0f });
    incoming.resize (columns.size());
    writeIndex = 0;
    dsProcessor.getDsp().setHistoryColumns (getWidth());
}

float DsEditor::Display::yForDb (float db) const noexcept
{
    return 4.0f + juce::jlimit (0.0f, 1.0f, db / historyFloorDb) * (historyHeight() - 8.0f);
}

float DsEditor::Display::dbForY (float y) const noexcept
{
    return juce::jlimit (historyFloorDb, 0.0f, (y - 4.0f) / (historyHeight() - 8.0f) * historyFloorDb);
}

float DsEditor::Display::xForHz (double hz) const noexcept
{
    return 14.0f + (float) (getWidth() - 28) * (float) (std::log10 (hz / rulerLowHz) / std::log10 (rulerHighHz / rulerLowHz));
}

double DsEditor::Display::hzForX (float x) const noexcept
{
    return rulerLowHz * std::pow (rulerHighHz / rulerLowHz, (double) ((x - 14.0f) / (float) (getWidth() - 28)));
}

void DsEditor::Display::update()
{
    if (frozen)
        return;

    const auto count = dsProcessor.getDsp().popHistory (incoming.data(), (int) incoming.size());

    for (int i = 0; i < count && ! columns.empty(); ++i)
    {
        columns[(size_t) writeIndex] = incoming[(size_t) i];
        writeIndex = (writeIndex + 1) % (int) columns.size();
    }

    repaint();
}

void DsEditor::Display::showColumns (std::vector<DsDsp::HistoryColumn> newColumns)
{
    frozen = true;
    columns = std::move (newColumns);
    writeIndex = 0;
    repaint();
}

void DsEditor::Display::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto targets = dsProcessor.readTargets();
    const auto top = historyHeight();

    g.setColour (colours::bgSection);
    g.fillRoundedRectangle (bounds, 6.0f);

    // History: oldest on the left.
    if (! columns.empty())
    {
        juce::Path level, reduction;
        const auto count = (int) columns.size();
        const auto pixelsPerDb = (top - 8.0f) / historyReductionDb;

        level.startNewSubPath (0.0f, top);
        reduction.startNewSubPath (0.0f, 0.0f);

        for (int i = 0; i < count; ++i)
        {
            const auto& column = columns[(size_t) ((writeIndex + i) % count)];
            level.lineTo ((float) i, yForDb (column.levelDb));
            reduction.lineTo ((float) i, 4.0f + juce::jlimit (0.0f, top, column.reductionDb * pixelsPerDb));
        }

        level.lineTo ((float) count, top);
        level.closeSubPath();
        reduction.lineTo ((float) count, 0.0f);
        reduction.closeSubPath();

        g.saveState();
        g.reduceClipRegion (juce::Rectangle<float> (0.0f, 0.0f, bounds.getWidth(), top).toNearestInt());

        g.setColour (colours::textSecondary.withAlpha (0.3f));
        g.fillPath (level);

        g.setColour (dsGreen.withAlpha (0.8f));
        g.fillRect (0.0f, yForDb ((float) targets.thresholdDb) - 0.5f, bounds.getWidth(), 1.0f);

        g.setColour (dsGreen.withAlpha (0.55f));
        g.fillPath (reduction);

        g.setColour (colours::textSecondary);
        g.setFont (font (Weight::regular, 10.0f));
        g.drawText (juce::String::fromUTF8 ("sibilance level (grey) \xc2\xb7 reduction (green) \xc2\xb7 threshold"),
                    juce::Rectangle<float> (8.0f, top - 20.0f, bounds.getWidth() - 16.0f, 16.0f), juce::Justification::centredLeft, false);
        g.restoreState();
    }

    // The ruler.
    {
        const auto ruler = bounds.withTop (top);
        juce::Path shape;
        shape.addRoundedRectangle (ruler.getX(), ruler.getY(), ruler.getWidth(), ruler.getHeight(), 6.0f, 6.0f, false, false, true, true);
        g.setColour (colours::bgWell);
        g.fillPath (shape);

        const auto barY = top + 8.0f, barHeight = 14.0f;
        const auto half = std::pow (2.0, 0.5 * targets.widthOctaves);
        const auto low = targets.shelf ? targets.freqHz : targets.freqHz / half;
        const auto high = targets.shelf ? rulerHighHz : std::min (targets.freqHz * half, rulerHighHz);

        g.setColour (dsGreen.withAlpha (0.35f));
        g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (xForHz (low), barY, xForHz (high), barY + barHeight), 3.0f);

        g.setColour (colours::uiBlue);
        g.fillRoundedRectangle (juce::Rectangle<float> (6.0f, 20.0f).withCentre ({ xForHz (targets.freqHz), barY + barHeight * 0.5f }), 2.0f);

        g.setFont (font (Weight::regular, 10.0f));

        for (const auto hz : { 1000.0, 2000.0, 3000.0, 5000.0, 8000.0, 12000.0, 20000.0 })
        {
            const auto x = xForHz (hz);
            g.setColour (colours::grey6);
            g.fillRect (x - 0.5f, top + 27.0f, 1.0f, 4.0f);
            g.setColour (colours::textSecondary);
            g.drawText (juce::String (juce::roundToInt (hz / 1000.0)) + "k", juce::Rectangle<float> (x - 15.0f, top + 29.0f, 30.0f, 12.0f),
                        juce::Justification::centred, false);
        }
    }
}

DsEditor::Display::Drag DsEditor::Display::dragAt (juce::Point<float> position) const
{
    if (position.y < historyHeight())
        return Drag::threshold;

    const auto targets = dsProcessor.readTargets();
    const auto half = std::pow (2.0, 0.5 * targets.widthOctaves);

    if (std::abs (position.x - xForHz (targets.freqHz)) <= 6.0f || targets.shelf)
        return Drag::freq;

    if (std::abs (position.x - xForHz (targets.freqHz / half)) <= 6.0f)
        return Drag::lowEdge;

    if (std::abs (position.x - xForHz (targets.freqHz * half)) <= 6.0f)
        return Drag::highEdge;

    return Drag::freq;
}

juce::RangedAudioParameter& DsEditor::Display::parameterFor (Drag drag) const
{
    auto& apvts = dsProcessor.getApvts();
    const auto* id = drag == Drag::threshold ? "threshold" : (drag == Drag::freq ? "freq" : "width");
    return *apvts.getParameter (id);
}

void DsEditor::Display::mouseMove (const juce::MouseEvent& event)
{
    const auto drag = dragAt (event.position);
    setMouseCursor (drag == Drag::threshold ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::LeftRightResizeCursor);
}

void DsEditor::Display::mouseDown (const juce::MouseEvent& event)
{
    dragging = dragAt (event.position);
    parameterFor (dragging).beginChangeGesture();
    dragTo (event.position);
}

void DsEditor::Display::mouseDrag (const juce::MouseEvent& event)
{
    dragTo (event.position);
}

void DsEditor::Display::mouseUp (const juce::MouseEvent&)
{
    if (dragging != Drag::none)
        parameterFor (dragging).endChangeGesture();

    dragging = Drag::none;
}

void DsEditor::Display::dragTo (juce::Point<float> position)
{
    auto& parameter = parameterFor (dragging);
    const auto targets = dsProcessor.readTargets();
    float value = 0.0f;

    switch (dragging)
    {
        case Drag::threshold: value = dbForY (position.y); break;
        case Drag::freq:      value = (float) hzForX (position.x); break;
        case Drag::lowEdge:   value = (float) (2.0 * std::log2 (targets.freqHz / std::max (1.0, hzForX (position.x)))); break;
        case Drag::highEdge:  value = (float) (2.0 * std::log2 (hzForX (position.x) / targets.freqHz)); break;
        case Drag::none:      return;
    }

    parameter.setValueNotifyingHost (parameter.convertTo0to1 (parameter.getNormalisableRange().snapToLegalValue (value)));
    repaint();
}

// DsEditor --------------------------------------------------------------------

DsEditor::DsEditor (DsProcessor& processor)
    : ShellEditor (processor, baseWidth, baseHeight, HeaderStyle::full),
      detectorDropdown (parameter ("detector")),
      modeSegments (parameter ("mode"), dsGreen, colours::bgWell),
      thresholdMeter (parameter ("threshold"), thresholdStyle()),
      gainMeter (gainGeometry),
      display (processor),
      shapeSegments (parameter ("shape"), colours::grey5, colours::textPrimary),
      freqKnob (parameter ("freq"), "Freq", colours::uiBlue, Knob::tightStyle),
      widthKnob (parameter ("width"), "Width", dsGreen, Knob::tightStyle),
      rangeKnob (parameter ("range"), "Range", dsGreen, Knob::tightStyle),
      attackKnob (parameter ("attack"), "Attack", dsGreen, Knob::tightStyle),
      releaseKnob (parameter ("release"), "Release", dsGreen, Knob::tightStyle),
      relativeCheckbox (parameter ("relative"), "Relative threshold", dsGreen),
      listenCheckbox (parameter ("listen"), "Listen", dsGreen),
      linkCheckbox (parameter ("link"), "Link channels", dsGreen),
      editLink (dsGreen),
      outMeter (outGeometry),
      gainFader (parameter ("output"), colours::uiTeal),
      lookaheadDropdown (parameter ("lookahead")),
      phaseDropdown (parameter ("splitPhase"))
{
    auto& body = getBody();

    // Phase 2.1 W.1: room in the header for undo, redo, A/B and Compare.
    setShortHeaderLabels (true);
    setPresetDropdownWidth (150);

    panels.setInterceptsMouseClicks (false, false);
    panels.setBounds (0, 0, baseWidth, bodyHeight);
    body.addAndMakeVisible (panels);

    // The row under the header.
    setUpLabel (detectorLabel, "Detector", juce::Justification::centredLeft);
    setUpLabel (modeLabel, "Mode", juce::Justification::centredRight);

    detectorLabel.setBounds (leftX, rowY, 56, rowHeight);
    detectorDropdown.setBounds (72, rowY + 1, 140, 24);
    detectorDropdown.setTextHeight (labelHeight);
    detectorDropdown.addSection (0, "Main input");
    detectorDropdown.addSection (5, "Sidechain");
    detectorDropdown.setTooltip ("What the de-esser listens to. Mid and Side catch sibilance in the centre or at the sides.");

    modeLabel.setBounds (510, rowY, 44, rowHeight);
    modeSegments.setBounds (562, rowY + 1, contentRight - 562, 24);
    modeSegments.setTextHeight (labelHeight);
    modeSegments.setTooltip ("Split turns down only the sibilance band. Wide turns down the whole signal.");

    // Left column.
    thresholdMeter.setBounds (8, columnY, 66, 282);
    thresholdMeter.setTooltip ("Threshold: how loud the sibilance band must get before it's turned down.");
    gainMeter.setBounds (78, columnY, 34, 290);
    gainMeter.setBarColour (dsGreen);

    display.setBounds (displayX, columnY, displayWidth, displayHeight);

    // Cards.
    shapeSegments.setBounds (displayX + detectionWidth - 112, cardY + 8, 100, 20);
    shapeSegments.setTextHeight (11.0f);
    shapeSegments.setTooltip ("Band listens to a band around Freq. Shelf listens to everything above it.");

    cardKnobs ({ &freqKnob, &widthKnob }, displayX, detectionWidth, cardY + 30);
    cardKnobs ({ &rangeKnob, &attackKnob, &releaseKnob }, reductionX, reductionWidth, cardY + 30);

    freqKnob.setTooltip ("The centre of the sibilance band, or where the shelf starts.");
    widthKnob.setTooltip ("How wide the sibilance band is, in octaves.");
    rangeKnob.setTooltip ("The most it turns down. 0 dB does nothing.");
    attackKnob.setTooltip ("How fast it turns down when an \"s\" starts.");
    releaseKnob.setTooltip ("How fast it comes back after an \"s\".");

    widthKnob.onLockedClick = [this] { showTip (widthKnob, widthHint); shapeSegments.repaint(); };

    for (auto* knob : { &attackKnob, &releaseKnob })
        knob->setShowDefaultMark (true);

    // Footer.
    relativeCheckbox.setBounds (leftX, footerY, 150, 20);
    listenCheckbox.setBounds (leftX + 162, footerY, 74, 20);
    linkCheckbox.setBounds (leftX + 248, footerY, 120, 20);
    editLink.setBounds (contentRight - 64, footerY - 2, 64, 24);

    for (auto* checkbox : { &relativeCheckbox, &listenCheckbox, &linkCheckbox })
        checkbox->setLabelHeight (labelHeight);

    relativeCheckbox.setTooltip ("Compares the sibilance band with the whole voice, so quiet and loud passages are treated alike. "
                                 "Threshold is then in dB below the voice.");
    listenCheckbox.setTooltip ("Plays only what's being taken away, so you can hear whether it's catching \"s\" sounds or real consonants.");
    linkCheckbox.setTooltip ("Both channels are turned down together, on the louder one.");

    // Right column.
    outMeter.setBounds (rightColumnX + 6, 10, 46, 350);
    setUpLabel (gainLabel, "Gain", juce::Justification::centred, 14.0f);
    gainLabel.setColour (juce::Label::textColourId, colours::textPrimary);
    gainLabel.setBounds (rightColumnX + 52, 10, 40, 17);
    gainFader.setBounds (rightColumnX + 55, faderTop, 33, faderHeight);
    gainFader.setTooltip ("The output level. The same control as the knob in the header.");

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &detectorLabel, &detectorDropdown, &modeLabel, &modeSegments,
             &thresholdMeter, &gainMeter, &display, &shapeSegments,
             &freqKnob, &widthKnob, &rangeKnob, &attackKnob, &releaseKnob,
             &relativeCheckbox, &listenCheckbox, &linkCheckbox, &editLink,
             &outMeter, &gainLabel, &gainFader })
        body.addAndMakeVisible (item);

    // EDIT drawer.
    drawer.setBounds (0, columnY - 4, rightColumnX, bodyHeight - columnY + 4);

    setUpLabel (lookaheadLabel, "Lookahead", juce::Justification::centredLeft, 14.0f);
    setUpLabel (phaseLabel, "Split filter", juce::Justification::centredLeft, 14.0f);
    lookaheadLabel.setBounds (24, 60, 170, 24);
    lookaheadDropdown.setBounds (200, 60, 170, 24);
    phaseLabel.setBounds (24, 100, 170, 24);
    phaseDropdown.setBounds (200, 100, 170, 24);

    lookaheadDropdown.setTooltip ("Starts turning down before an \"s\" arrives, so the fastest onsets are caught.");
    phaseDropdown.setTooltip ("Minimum phase leaves the sound untouched when nothing is turned down. "
                              "Linear phase splits with steeper, phase-true filters.");
    addLatencyNote (lookaheadDropdown);
    addLatencyNote (phaseDropdown);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &lookaheadLabel, &lookaheadDropdown, &phaseLabel, &phaseDropdown })
        drawer.addAndMakeVisible (item);

    body.addChildComponent (drawer);
    editLink.onClick = [this] { drawer.setVisible (! drawer.isVisible()); };

    updateLocks();
}

DsProcessor& DsEditor::dsProcessor() noexcept
{
    return dynamic_cast<DsProcessor&> (getFancyProcessor());
}

void DsEditor::updateLocks()
{
    const auto targets = dsProcessor().readTargets();

    widthKnob.setLocked (targets.shelf, widthHint);
    widthKnob.setAlpha (targets.shelf ? 0.5f : 1.0f);
    thresholdMeter.setReadoutSuffix (targets.relative ? " rel" : juce::String {});
}

void DsEditor::onTick (float seconds)
{
    auto& processor = dsProcessor();
    auto& dsp = processor.getDsp();

    updateLocks();

    const auto reduction = juce::jmin (dsp.takeReduction(), 60.0f);

    thresholdMeter.update (dsp.levelMeter, false, seconds);
    gainMeter.update (reduction, reduction, seconds);
    outMeter.update (processor.outputMeter, seconds);
    display.update();
}

void DsEditor::showPreview (bool drawerOpen)
{
    stopUpdates();

    setValue (parameter ("threshold"), -28.0f);
    setValue (parameter ("freq"), 6200.0f);
    setValue (parameter ("width"), 1.0f);
    setValue (parameter ("range"), -12.0f);
    setValue (parameter ("attack"), 0.5f);
    setValue (parameter ("release"), 60.0f);

    updateLocks();

    // The mockup's history: "s" bursts every 120 px over a quiet voice, turned down above Threshold.
    std::vector<DsDsp::HistoryColumn> columns;
    juce::Random random (4);

    for (int x = 0; x < displayWidth; ++x)
    {
        const auto t = (x + 30) % 120;
        auto levelDb = t < 14 ? -18.0f + 4.0f * random.nextFloat() : -44.0f - 6.0f * random.nextFloat();

        if (t > 60 && t < 66)
            levelDb = -24.0f;

        columns.push_back ({ levelDb, juce::jmax (0.0f, levelDb + 28.0f) * 0.55f });
    }

    display.showColumns (std::move (columns));

    thresholdMeter.show (-22.0f, -24.0f, false);
    gainMeter.show (5.6f, 5.6f);
    outMeter.show (-9.4f, -9.0f);

    drawer.setVisible (drawerOpen);
}

juce::Colour DsEditor::getAccentColour()
{
    return dsGreen;
}

std::vector<ShellEditor::PopupOpener> DsEditor::getPopupOpeners()
{
    auto openers = ShellEditor::getPopupOpeners();
    openers.push_back ({ "dropdown-detector", [this] { detectorDropdown.click(); } });
    openers.push_back ({ "dropdown-lookahead", [this] { drawer.setVisible (true); lookaheadDropdown.click(); } });
    openers.push_back ({ "tooltip-relative", [this] { showTip (relativeCheckbox, relativeCheckbox.getTooltip()); } });
    openers.push_back ({ "value-freq", [this] { freqKnob.openValueEditor(); } });
    return openers;
}
