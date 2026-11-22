#include "DsEditor.h"

#include <fancy/plugin/ParamFormat.h>

#include <algorithm>
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
const juce::Colour lowYellow = colours::accentYellow;   // the Low band (user, 22 November 2026)

constexpr OutMeter::Geometry outGeometry { 14.5f, 16.0f, 316.0f, 26.0f, 4.9f, 324.0f, false, true };
constexpr GainReductionMeter::Geometry gainGeometry { 13.0f, 20.0f, 246.0f, true, "GR" };

ThresholdMeter::Style thresholdStyle (juce::Colour accent = dsGreen)
{
    ThresholdMeter::Style style;
    style.wellX = 12.0f;
    style.wellTop = 20.0f;
    style.wellHeight = 246.0f;
    style.lowestDb = -60.0f;
    style.accent = accent;
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

// BandTabs --------------------------------------------------------------------

void DsEditor::BandTabs::setState (bool lowSelected, bool dualOn)
{
    if (lowSelected != low || dualOn != dual)
    {
        low = lowSelected;
        dual = dualOn;
        repaint();
    }
}

juce::Rectangle<float> DsEditor::BandTabs::powerArea() const
{
    return { (float) getWidth() - 22.0f, 0.0f, 22.0f, (float) getHeight() };
}

void DsEditor::BandTabs::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (bounds, 4.0f);

    const auto half = bounds.reduced (2.0f).withWidth ((bounds.getWidth() - 6.0f) * 0.5f);
    const juce::Rectangle<float> tabs[] { half, half.withX (half.getRight() + 2.0f) };

    for (int i = 0; i < 2; ++i)
    {
        const auto selected = (i == 1) == low;
        const auto accent = i == 0 ? dsGreen : lowYellow;

        if (selected)
        {
            g.setColour (accent);
            g.fillRoundedRectangle (tabs[i], 3.0f);
        }

        g.setColour (selected ? colours::bgWell : (i == 1 && ! dual ? colours::textSecondary : colours::textPrimary));
        g.setFont (font (selected ? Weight::semiBold : Weight::regular, labelHeight));
        g.drawText (i == 0 ? "High" : "Low", tabs[i].withTrimmedRight (i == 1 ? 14.0f : 0.0f), juce::Justification::centred, false);
    }

    // The power switch on Low: lit (yellow, or dark on the selected tab) while Dual band is on.
    const auto area = powerArea().reduced (5.0f, 6.0f);
    const auto size = juce::jmin (area.getWidth(), area.getHeight());
    const auto icon = area.withSizeKeepingCentre (size, size);
    juce::Path power;
    power.addCentredArc (icon.getCentreX(), icon.getCentreY() + 0.5f, size * 0.42f, size * 0.42f, 0.0f, 0.7f, juce::MathConstants<float>::twoPi - 0.7f, true);
    power.startNewSubPath (icon.getCentreX(), icon.getY());
    power.lineTo (icon.getCentreX(), icon.getCentreY());

    g.setColour (dual ? (low ? colours::bgWell : lowYellow) : colours::textSecondary.withAlpha (0.7f));
    g.strokePath (power, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void DsEditor::BandTabs::mouseDown (const juce::MouseEvent& event)
{
    if (powerArea().contains (event.position))
    {
        if (onPower != nullptr)
            onPower();

        return;
    }

    if (onSelect != nullptr)
        onSelect (event.position.x >= (float) getWidth() * 0.5f);
}

// Display ---------------------------------------------------------------------

DsEditor::Display::Display (DsProcessor& processor)
    : dsProcessor (processor)
{
    setTooltip ("The sibilance level (grey) and the reduction (green) over the last 3 s. Drag up or down for Threshold. "
                "Below, the band: drag the blue handle for Freq, or the band's edges for Width.");
}

void DsEditor::Display::setLowSelected (bool shouldBeLow)
{
    if (lowSelected != shouldBeLow)
    {
        lowSelected = shouldBeLow;
        repaint();
    }
}

void DsEditor::Display::resized()
{
    history.setWidth (getWidth(), dsProcessor.getDsp());
}

float DsEditor::Display::yForDb (float db) const noexcept
{
    return fancy::ui::DeEsserHistory::yForDb (db, historyHeight(), historyFloorDb);
}

float DsEditor::Display::dbForY (float y) const noexcept
{
    return fancy::ui::DeEsserHistory::dbForY (y, historyHeight(), historyFloorDb);
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
    if (history.isFrozen())
        return;

    history.update (dsProcessor.getDsp());
    repaint();
}

void DsEditor::Display::showColumns (std::vector<DsDsp::HistoryColumn> newColumns)
{
    history.show (std::move (newColumns));
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
    {
        DeEsserHistory::Style style { dsGreen, juce::String::fromUTF8 ("sibilance level (grey) \xc2\xb7 reduction (green) \xc2\xb7 threshold"),
                                      false, historyFloorDb, historyReductionDb };

        // Phase 4 Wave 2: Low's reduction in yellow, and the selected band's level and threshold.
        if (targets.dual)
        {
            style.caption = juce::String::fromUTF8 ("sibilance level (grey) \xc2\xb7 reduction: High (green), Low (yellow) \xc2\xb7 threshold");
            style.showLowReduction = true;
            style.lowAccent = lowYellow;
            style.lowLevel = lowSelected;
        }

        history.paint (g, bounds.getWidth(), top, (float) (targets.dual && lowSelected ? targets.lowThresholdDb : targets.thresholdDb), style);
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

        // Phase 4 Wave 2: the Low band, in the bar's lower half, in yellow, with its own handle.
        if (targets.dual)
        {
            const auto lowHalf = std::pow (2.0, 0.5 * targets.lowWidthOctaves);
            const auto lowY = barY + barHeight * 0.5f + 1.0f, lowHeight = 8.0f;

            g.setColour (lowYellow.withAlpha (lowSelected ? 0.9f : 0.6f));
            g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (xForHz (targets.lowFreqHz / lowHalf), lowY,
                                                                               xForHz (targets.lowFreqHz * lowHalf), lowY + lowHeight), 3.0f);

            const auto handle = juce::Rectangle<float> (6.0f, 16.0f).withCentre ({ xForHz (targets.lowFreqHz), lowY + lowHeight * 0.5f });
            g.setColour (colours::bgWell);
            g.fillRoundedRectangle (handle.expanded (1.5f), 3.0f);
            g.setColour (colours::uiBlue);
            g.fillRoundedRectangle (handle, 2.0f);
        }

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

    // Phase 4 Wave 2: the Low band's bar, in the lower half, comes first where the two overlap.
    if (targets.dual && position.y >= historyHeight() + 8.0f + 8.0f)
    {
        const auto lowHalf = std::pow (2.0, 0.5 * targets.lowWidthOctaves);
        const auto low = xForHz (targets.lowFreqHz / lowHalf), high = xForHz (targets.lowFreqHz * lowHalf);

        if (std::abs (position.x - xForHz (targets.lowFreqHz)) <= 6.0f)
            return Drag::lowBandFreq;

        if (std::abs (position.x - low) <= 6.0f)
            return Drag::lowBandLowEdge;

        if (std::abs (position.x - high) <= 6.0f)
            return Drag::lowBandHighEdge;

        if (position.x > low && position.x < high)
            return Drag::lowBandFreq;
    }

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
    const char* id = "width";

    switch (drag)
    {
        case Drag::threshold:       id = lowSelected && dsProcessor.readTargets().dual ? "lowThreshold" : "threshold"; break;
        case Drag::freq:            id = "freq"; break;
        case Drag::lowBandFreq:     id = "lowFreq"; break;
        case Drag::lowBandLowEdge:
        case Drag::lowBandHighEdge: id = "lowWidth"; break;
        default:                    break;
    }

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

    // A click on a band's bar selects its tab.
    if (dragging != Drag::threshold && onSelectBand != nullptr)
        onSelectBand (isLowBandDrag (dragging));

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
        case Drag::lowBandFreq:     value = (float) hzForX (position.x); break;
        case Drag::lowBandLowEdge:  value = (float) (2.0 * std::log2 (targets.lowFreqHz / std::max (1.0, hzForX (position.x)))); break;
        case Drag::lowBandHighEdge: value = (float) (2.0 * std::log2 (hzForX (position.x) / targets.lowFreqHz)); break;
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
      listenCheckbox (getFancyProcessor(), "Listen", dsGreen),
      linkCheckbox (parameter ("link"), "Link channels", dsGreen),
      editLink (dsGreen),
      slopeDropdown (parameter ("slope")),
      ceilingKnob (parameter ("ceiling"), "Ceiling", dsGreen, Knob::tightStyle),
      lowThresholdMeter (parameter ("lowThreshold"), thresholdStyle (lowYellow)),
      lowFreqKnob (parameter ("lowFreq"), "Freq", colours::uiBlue, Knob::tightStyle),
      lowWidthKnob (parameter ("lowWidth"), "Width", lowYellow, Knob::tightStyle),
      lowRangeKnob (parameter ("lowRange"), "Range", lowYellow, Knob::tightStyle),
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
    thresholdMeter.setBounds (8, columnY, 66, 298);
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

    for (auto* checkbox : { &relativeCheckbox, &linkCheckbox })
        checkbox->setLabelHeight (labelHeight);

    listenCheckbox.setLabelHeight (labelHeight);

    relativeCheckbox.setTooltip ("Compares the sibilance band with the whole voice, so quiet and loud passages are treated alike. "
                                 "Threshold is then in dB below the voice.");
    listenCheckbox.setTooltip ("Plays only what's being taken away, so you can hear whether it's catching \"s\" sounds or real consonants. "
                               "Turns off when you close this window, and is never saved.");
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

    // Phase 4 Wave 2: the band tabs between Detector and Mode, and Low's controls in High's places.
    setUpLabel (bandLabel, "Band", juce::Justification::centredRight);
    bandLabel.setBounds (232, rowY, 40, rowHeight);
    bandTabs.setBounds (278, rowY + 1, 128, 24);
    bandTabs.setTooltip ("High and Low: two sibilance bands, for \"s\" and for \"sh\". Click a name to see its settings; "
                         "the switch on Low turns the second band on.");
    bandTabs.onSelect = [this] (bool low) { selectBand (low); };
    bandTabs.onPower = [this]
    {
        const auto on = ! dsProcessor().readTargets().dual;
        dsProcessor().setParameter ("dual", on ? 1.0f : 0.0f);
        selectBand (on);
    };
    display.onSelectBand = [this] (bool low) { selectBand (low); };

    lowThresholdMeter.setBounds (thresholdMeter.getBounds());
    lowThresholdMeter.setTooltip ("Low's threshold: how loud the Low band must get before it's turned down.");
    lowFreqKnob.setBounds (freqKnob.getBounds());
    lowWidthKnob.setBounds (widthKnob.getBounds());
    lowRangeKnob.setBounds (rangeKnob.getBounds());
    lowFreqKnob.setTooltip ("The centre of the Low band, where \"sh\" and \"ch\" sit (1 to 8 kHz).");
    lowWidthKnob.setTooltip ("How wide the Low band is, in octaves.");
    lowRangeKnob.setTooltip ("The most the Low band turns down. 0 dB does nothing.");

    body.addAndMakeVisible (bandLabel);
    body.addAndMakeVisible (bandTabs);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &lowThresholdMeter, &lowFreqKnob, &lowWidthKnob, &lowRangeKnob })
        body.addChildComponent (item);

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

    // Phase 4 Wave 1: Filter slope and Ceiling.
    setUpLabel (slopeLabel, "Filter slope", juce::Justification::centredLeft, 14.0f);
    setUpLabel (ceilingLabel, "Ceiling", juce::Justification::centredLeft, 14.0f);
    slopeLabel.setBounds (24, 140, 170, 24);
    slopeDropdown.setBounds (200, 140, 170, 24);
    ceilingLabel.setBounds (24, 180, 170, 24);
    ceilingKnob.setBounds (200, 170, 66, 80);
    slopeDropdown.setTooltip ("How sharply the detector picks out the band. Steeper ignores more of the voice around it.");
    ceilingKnob.setTooltip ("Holds the output's true peak under this level. Off at the bottom. Adds a little latency while on.");

    for (juce::Component* item : std::initializer_list<juce::Component*> { &lookaheadLabel, &lookaheadDropdown, &phaseLabel, &phaseDropdown,
                                                                           &slopeLabel, &slopeDropdown, &ceilingLabel, &ceilingKnob })
        drawer.addAndMakeVisible (item);

    // Learn, in the footer after Link channels.
    learnButton.setBounds (leftX + 384, footerY - 2, 76, 24);
    learnButton.onClick = [this]
    {
        learnedLevels.clear();
        learnedLowLevels.clear();
        learnSeconds = 0.0f;
        learnButton.setText ("Listening...");
    };
    setUpLabel (learnNote, "2 s of playback", juce::Justification::centredLeft, 11.0f);
    learnNote.setBounds (leftX + 466, footerY - 2, 100, 24);
    body.addAndMakeVisible (learnButton);
    body.addAndMakeVisible (learnNote);

    display.setColumnListener ([this] (const DsDsp::HistoryColumn* columns, int count)
    {
        if (learnSeconds < 0.0f)
            return;

        for (int i = 0; i < count; ++i)
        {
            if (columns[i].levelDb > -150.0f)
                learnedLevels.push_back (columns[i].levelDb);

            if (columns[i].lowLevelDb > -150.0f)
                learnedLowLevels.push_back (columns[i].lowLevelDb);
        }
    });

    body.addChildComponent (drawer);
    editLink.onClick = [this] { drawer.setVisible (! drawer.isVisible()); };

    updateLocks();
}

DsProcessor& DsEditor::dsProcessor() noexcept
{
    return dynamic_cast<DsProcessor&> (getFancyProcessor());
}

void DsEditor::selectBand (bool low)
{
    lowSelected = low;

    for (juce::Component* high : std::initializer_list<juce::Component*> { &thresholdMeter, &freqKnob, &widthKnob, &rangeKnob })
        high->setVisible (! low);

    for (juce::Component* lowControl : std::initializer_list<juce::Component*> { &lowThresholdMeter, &lowFreqKnob, &lowWidthKnob, &lowRangeKnob })
        lowControl->setVisible (low);

    // Low is always a band (docs/ds.md, Phase 4 Wave 2).
    shapeSegments.setAlpha (low ? 0.4f : 1.0f);
    shapeSegments.setInterceptsMouseClicks (! low, ! low);

    gainMeter.setBarColour (low ? lowYellow : dsGreen);
    display.setLowSelected (low);
    updateLocks();
}

void DsEditor::updateLocks()
{
    const auto targets = dsProcessor().readTargets();

    bandTabs.setState (lowSelected, targets.dual);

    // With Dual band off, Low's controls still set it, dimmed, for when it's switched on.
    for (juce::Component* lowControl : std::initializer_list<juce::Component*> { &lowThresholdMeter, &lowFreqKnob, &lowWidthKnob, &lowRangeKnob })
        lowControl->setAlpha (targets.dual ? 1.0f : 0.5f);

    lowThresholdMeter.setReadoutNote (targets.relative ? "relative" : juce::String {});

    widthKnob.setLocked (targets.shelf, widthHint);
    widthKnob.setAlpha (targets.shelf ? 0.5f : 1.0f);
    thresholdMeter.setReadoutNote (targets.relative ? "relative" : juce::String {});
}

void DsEditor::onTick (float seconds)
{
    auto& processor = dsProcessor();
    auto& dsp = processor.getDsp();

    updateLocks();

    const auto highReduction = juce::jmin (dsp.takeReduction(), 60.0f);
    const auto lowReduction = juce::jmin (dsp.takeLowReduction(), 60.0f);
    const auto reduction = lowSelected ? lowReduction : highReduction;

    thresholdMeter.update (dsp.levelMeter, false, seconds);
    lowThresholdMeter.update (dsp.lowLevelMeter, false, seconds);
    gainMeter.update (reduction, reduction, seconds);
    outMeter.update (processor.outputMeter, seconds);
    display.update();

    if (learnSeconds >= 0.0f)
    {
        learnSeconds += seconds;

        if (learnSeconds >= 2.0f)
            finishLearning();
    }
}

void DsEditor::finishLearning()
{
    learnSeconds = -1.0f;
    learnButton.setText ("Learn");

    if (learnedLevels.size() < 8)
        return;

    const auto setFromPercentile = [this] (std::vector<float>& levels, const char* parameterId)
    {
        std::sort (levels.begin(), levels.end());
        const auto learned = levels[(size_t) juce::jlimit (0, (int) levels.size() - 1, (int) (0.9 * (double) levels.size()))];

        auto& threshold = parameter (parameterId);
        threshold.beginChangeGesture();
        threshold.setValueNotifyingHost (threshold.convertTo0to1 (learned));
        threshold.endChangeGesture();
    };

    setFromPercentile (learnedLevels, "threshold");

    // Phase 4 Wave 2 (Decision 4 A): with two bands, Low's threshold from its own level too.
    if (dsProcessor().readTargets().dual && learnedLowLevels.size() >= 8)
        setFromPercentile (learnedLowLevels, "lowThreshold");
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
    setValue (parameter ("dual"), 1.0f);
    setValue (parameter ("lowFreq"), 3500.0f);
    setValue (parameter ("lowWidth"), 1.0f);
    setValue (parameter ("lowThreshold"), -20.0f);
    setValue (parameter ("lowRange"), -9.0f);

    selectBand (false);
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

        // Phase 4 Wave 2: "sh" sounds half way between, turned down by the Low band.
        const auto u = (x + 90) % 120;
        const auto lowLevelDb = u < 18 ? -14.0f + 3.0f * random.nextFloat() : -40.0f - 6.0f * random.nextFloat();

        columns.push_back ({ levelDb, juce::jmax (0.0f, levelDb + 28.0f) * 0.55f, lowLevelDb, juce::jmax (0.0f, lowLevelDb + 20.0f) * 0.6f });
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
