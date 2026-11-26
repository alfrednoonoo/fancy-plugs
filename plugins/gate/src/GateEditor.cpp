#include "GateEditor.h"

#include <algorithm>

#include <fancy/plugin/ParamFormat.h>

#include <cmath>
#include <iterator>

using namespace fancy::ui;

namespace
{
// design/phase2_0_1-mockup-src/gate.html at 100 %. The shared header is 16 px taller than the
// mockup's, so the window is too. Phase 4 Wave 2, v1.7.1 (mockups 53 and 54): 50 px wider for
// Upward's third Mode choice and 24 px taller for the Range card's title row, so 790 x 465.
constexpr int baseWidth = 790;
constexpr int growY = 24;

// Phase 4 Wave 3 (mockup 75 A): the BANDS card goes where the envelope card was, and the window
// grows 172 px taller (790 x 637); the cards and the footer move down, and the meters and faders
// lengthen.
constexpr int bandsGrow = 172;
constexpr int bandsCardY = 118, bandsCardHeight = 164;
constexpr int tallY = growY + bandsGrow;

// Phase 4 Wave 2: Host sync's small Shape knob, 66 tall with its label and value (Comp's).
constexpr fancy::ui::Knob::Style syncKnobStyle { 32.0f, 27.0f, 3.0f, 9.0f, 7.0f, 13.0f, 3.0f, 2.0f, 16.0f, 11.0f, false, true };
constexpr int bodyHeight = 368 + tallY;
constexpr int baseHeight = ShellEditor::headerHeight + bodyHeight;
constexpr int rightColumnX = 640;

constexpr int centreX = 120, centreWidth = 506;
constexpr int historyY = 40, historyHeight = 70;
constexpr int envelopeY = 118 + bandsGrow, cardHeight = 96;
constexpr int lowerY = 222 + bandsGrow, lowerHeight = cardHeight + growY, lowerKnobY = lowerY + growY;
constexpr int levelWidth = 251, filterX = 379, filterWidth = 247;
constexpr int footerY = 336 + tallY;
constexpr int faderTop = 26, faderHeight = 331 + tallY;

constexpr float labelHeight = 10.5f;   // the mockup's captions and checkboxes
constexpr float historyFloorDb = -90.0f;

const juce::Colour gateYellow = colours::accentYellow;
const juce::Colour noiseGrey { 0xff909090 };

// Phase 4 Wave 3: the bands' colours, as Limit's and MultiComp's, and their small knobs.
const juce::Colour bandColours[] { colours::uiRed, colours::uiPurple, colours::uiBlue };
constexpr fancy::ui::Knob::Style bandKnobStyle = syncKnobStyle;

// The wells grow with the window, over the same dB range.
constexpr OutMeter::Geometry outGeometry { 14.5f, 16.0f, 316.0f + tallY, 26.0f, 4.9f * (290.0f + tallY) / 290.0f, 324.0f + tallY, false, true };
constexpr GainReductionMeter::Geometry gainGeometry { 16.0f, 14.0f, 232.0f + tallY, true, "Gain" };

ThresholdMeter::Style thresholdStyle()
{
    ThresholdMeter::Style style;
    style.wellX = 8.0f;
    style.wellTop = 14.0f;
    style.wellHeight = 232.0f + tallY;
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

namespace
{
const char* const deBleedTip = "Raises Threshold while the sound outside Lowpass and Highpass is louder than inside, so bleed can't open the gate.";
const char* const bandsFilterNote = "Off while Bands is 2 or 3: each band's detector already hears only its own band.";
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
    g.fillRoundedRectangle ((float) centreX, (float) lowerY, (float) levelWidth, (float) lowerHeight, 6.0f);
    g.fillRoundedRectangle ((float) filterX, (float) lowerY, (float) filterWidth, (float) lowerHeight, 6.0f);
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

    if (onColumns != nullptr && count > 0)
        onColumns (incoming.data(), count);

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
        g.drawText (sidechainWarningText, juce::Rectangle<float> (12.0f, 5.0f, 356.0f, 18.0f),
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

void GateEditor::Light::setUpward (bool isUpward)
{
    if (isUpward != upward)
    {
        upward = isUpward;
        repaint();
    }
}

void GateEditor::Light::paint (juce::Graphics& g)
{
    const auto lit = state != GateDsp::lightClosed;
    const auto colour = state == GateDsp::lightOpen ? gateYellow : state == GateDsp::lightHold ? colours::accentOrange : colours::grey6;
    const auto text = state == GateDsp::lightOpen ? (upward ? "boost" : "open") : state == GateDsp::lightHold ? "hold" : "closed";

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

// BandStrip (Phase 4 Wave 3) --------------------------------------------------------

GateEditor::BandStrip::BandStrip (GateProcessor& processor)
    : gateProcessor (processor)
{
    setTooltip ("Each band's gain reduction, falling from the top in its colour. Drag a blue line to move a crossover, "
                "or click it to type a frequency.");
}

void GateEditor::BandStrip::setAttenuations (const std::array<float, GateDsp::maxBands>& attenuationsDb)
{
    if (attenuationsDb != attenuations)
    {
        attenuations = attenuationsDb;
        repaint();
    }
}

float GateEditor::BandStrip::xForHz (double hz) const noexcept
{
    return (float) getWidth() * (float) std::log10 (juce::jlimit (20.0, 20000.0, hz) / 20.0) / 3.0f;
}

double GateEditor::BandStrip::hzForX (float x) const noexcept
{
    return 20.0 * std::pow (1000.0, juce::jlimit (0.0, 1.0, (double) x / (double) juce::jmax (1, getWidth())));
}

int GateEditor::BandStrip::crossoverAt (juce::Point<float> position) const
{
    const auto targets = gateProcessor.readTargets();
    const auto hz = GateDsp::sanitisedCrossovers (targets, targets.bands);

    for (int i = 0; i < targets.bands - 1; ++i)
        if (std::abs (position.x - xForHz (hz[(size_t) i])) <= 5.0f && position.y <= stripHeight)
            return i;

    return -1;
}

void GateEditor::BandStrip::paint (juce::Graphics& g)
{
    const auto width = (float) getWidth();
    const auto h = stripHeight;
    const auto targets = gateProcessor.readTargets();
    const auto count = targets.bands;
    const auto hz = GateDsp::sanitisedCrossovers (targets, count);

    g.setColour (colours::bgWell);
    g.fillRect (0.0f, 0.0f, width, h);

    std::array<float, GateDsp::maxBands + 1> edges {};
    edges[0] = 0.0f;

    for (int i = 0; i < count - 1; ++i)
        edges[(size_t) i + 1] = xForHz (hz[(size_t) i]);

    edges[(size_t) count] = width;

    for (int i = 0; i < count; ++i)
    {
        const auto x0 = edges[(size_t) i], x1 = edges[(size_t) i + 1];
        const auto colour = bandColours[i];
        const auto drop = h * juce::jlimit (0.0f, scaleDb, attenuations[(size_t) i]) / scaleDb;

        g.setColour (colour.withAlpha (0.10f));
        g.fillRect (x0, 0.0f, x1 - x0, h);
        g.setColour (colour.withAlpha (0.75f));
        g.fillRect (x0 + 1.0f, 0.0f, juce::jmax (0.0f, x1 - x0 - 2.0f), drop);

        const auto centre = juce::Point<float> ((x0 + x1) * 0.5f, h - 13.0f);
        g.setColour (colours::bgWell);
        g.fillEllipse (juce::Rectangle<float> (15.0f, 15.0f).withCentre (centre));
        g.setColour (colour);
        g.drawEllipse (juce::Rectangle<float> (15.0f, 15.0f).withCentre (centre), 1.5f);
        g.setColour (colours::textPrimary);
        g.setFont (font (Weight::semiBold, 9.5f));
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (20.0f, 14.0f).withCentre (centre), juce::Justification::centred, false);
    }

    for (int i = 0; i < count - 1; ++i)
    {
        const auto x = edges[(size_t) i + 1];
        g.setColour (colours::uiBlue);
        g.fillRect (x - 0.75f, 0.0f, 1.5f, h);
        g.fillRoundedRectangle (x - 3.0f, h - 8.0f, 6.0f, 8.0f, 1.5f);
    }

    g.setColour (colours::textSecondary);
    g.setFont (font (Weight::regular, 9.0f));

    const std::pair<double, const char*> labels[] { { 50.0, "50" }, { 100.0, "100" }, { 200.0, "200" }, { 500.0, "500" },
                                                     { 1000.0, "1k" }, { 2000.0, "2k" }, { 5000.0, "5k" }, { 10000.0, "10k" } };

    for (const auto& [labelHz, text] : labels)
        g.drawText (text, juce::Rectangle<float> (xForHz (labelHz) - 20.0f, h + 1.0f, 40.0f, 12.0f), juce::Justification::centred, false);
}

void GateEditor::BandStrip::mouseDown (const juce::MouseEvent& event)
{
    dragging = event.mods.isPopupMenu() ? -1 : crossoverAt (event.position);
    dragged = false;
}

void GateEditor::BandStrip::mouseDrag (const juce::MouseEvent& event)
{
    if (dragging < 0 || (! dragged && event.getDistanceFromDragStart() < 3))
        return;

    dragged = true;

    // Between its neighbours, 1/3 octave from each, as the stage keeps them.
    const auto targets = gateProcessor.readTargets();
    const auto hz = GateDsp::sanitisedCrossovers (targets, targets.bands);
    constexpr auto gap = 1.2599210498948732;
    const auto low = dragging > 0 ? hz[(size_t) dragging - 1] * gap : 20.0;
    const auto high = dragging < targets.bands - 2 ? hz[(size_t) dragging + 1] / gap : 20000.0;

    gateProcessor.setParameter (GateProcessor::crossoverId (dragging), (float) juce::jlimit (low, high, hzForX (event.position.x)));
    repaint();
}

void GateEditor::BandStrip::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0 && ! dragged)
        askForCrossover (dragging);

    dragging = -1;
    dragged = false;
}

void GateEditor::BandStrip::askForCrossover (int index)
{
    const auto targets = gateProcessor.readTargets();
    const auto hz = GateDsp::sanitisedCrossovers (targets, juce::jmax (2, targets.bands));

    DialogSpec spec;
    spec.title = "Crossover " + juce::String (index + 1);
    spec.body = "Between bands " + juce::String (index + 1) + " and " + juce::String (index + 2) + ", from 20 Hz to 20 kHz: 2500 or 2.5k.";
    spec.action = "Set";
    spec.fields.push_back ({ "Frequency (Hz)", fancy::formatHz ((float) hz[(size_t) index]) });
    spec.validate = [] (const juce::StringArray& values)
    {
        const auto value = fancy::parseHz (values[0]);

        if (values[0].trim().isEmpty() || ! std::isfinite (value) || value < 20.0f || value > 20000.0f)
            return juce::String ("Enter a frequency from 20 Hz to 20 kHz.");

        return juce::String {};
    };

    showDialog (*this, spec, [safe = juce::Component::SafePointer<BandStrip> (this), index] (bool accepted, const juce::StringArray& values)
    {
        if (safe == nullptr || ! accepted)
            return;

        fancy::FancyProcessor::UserEdit edit (safe->gateProcessor, "Crossover");
        safe->gateProcessor.setParameter (GateProcessor::crossoverId (index), fancy::parseHz (values[0]));
        safe->repaint();
    });
}

// BandsCard (Phase 4 Wave 3) ------------------------------------------------------

GateEditor::BandsCard::BandsCard (GateProcessor& processor)
    : strip (processor),
      gateProcessor (processor),
      bandsSegments (*processor.getApvts().getParameter ("bands"), gateYellow, colours::bgWell),
      crossoverDropdown (*processor.getApvts().getParameter ("crossoverType"))
{
    // mockup 75 A, at the card's own coordinates.
    styleLabel (crossoverLabel, "Crossover", colours::textSecondary, juce::Justification::centredLeft, 12.0f);
    crossoverLabel.setBounds (273, 5, 62, 24);

    bandsSegments.setBounds (66, 8, 90, 20);
    bandsSegments.setTextHeight (12.0f);
    bandsSegments.setTooltip ("How many bands to split the signal into. Each band opens on its own sound, so cymbals can't hold "
                              "a tom's gate open. 1 gates the whole signal.");

    // Limit's and MultiComp's width for the same text at 12 px.
    crossoverDropdown.setBounds (337, 7, 157, 22);
    crossoverDropdown.setTextHeight (12.0f);
    crossoverDropdown.setTooltip ("The crossovers' slope. Both sum back flat.");

    strip.setBounds (12, 34, 482, (int) BandStrip::stripHeight + 13);

    for (int band = 0; band < GateDsp::maxBands; ++band)
    {
        const auto centre = cellX (band);
        const auto name = juce::String (band + 1);

        auto threshold = std::make_unique<Knob> (*processor.getApvts().getParameter (GateProcessor::bandThresholdId (band)), "Threshold",
                                                 bandColours[band], bandKnobStyle, true);
        threshold->setBounds (juce::roundToInt (centre + 58.0f) - 32, 94, 64, 66);
        threshold->setShowDefaultMark (true);
        threshold->setTooltip ("Band " + name + " opens this far above or below the Threshold fader, which still moves every band.");
        addChildComponent (*threshold);
        thresholdKnobs[(size_t) band] = std::move (threshold);

        auto range = std::make_unique<Knob> (*processor.getApvts().getParameter (GateProcessor::bandDepthId (band)), "Range",
                                             bandColours[band], bandKnobStyle);
        range->setBounds (juce::roundToInt (centre + 124.0f) - 32, 94, 64, 66);
        range->setTooltip ("How much of the Range knob band " + name + " uses, shown as its own Range: at 50 %, -40 dB is -20 dB. "
                           "Lower it to keep some of this band's bleed.");
        addChildComponent (*range);
        rangeKnobs[(size_t) band] = std::move (range);
    }

    for (juce::Component* item : std::initializer_list<juce::Component*> { &crossoverLabel, &bandsSegments, &crossoverDropdown, &strip })
        addAndMakeVisible (item);

    refreshBands();
}

float GateEditor::BandsCard::cellX (int band) noexcept
{
    return 12.0f + 482.0f / 3.0f * (float) band;
}

void GateEditor::BandsCard::refreshBands()
{
    const auto targets = gateProcessor.readTargets();
    const auto count = targets.bands;

    // Each Range knob reads the band's own Range in dB (decision 2 A); -inf counts as -120 dB.
    const auto rangeDb = targets.rangeDb <= GateDsp::rangeFloorDb ? GateDsp::curveFloorDb : targets.rangeDb;

    for (int band = 0; band < GateDsp::maxBands; ++band)
    {
        const auto visible = count > 1 && band < count;
        thresholdKnobs[(size_t) band]->setVisible (visible);
        rangeKnobs[(size_t) band]->setVisible (visible);

        const auto depth = targets.bandDepth[(size_t) band];
        const auto bandDb = rangeDb * depth;
        const auto text = depth >= 1.0 && targets.rangeDb <= GateDsp::rangeFloorDb ? juce::String ("-inf")
                                                                                    : (bandDb > -0.5 ? juce::String ("0") : juce::String (juce::roundToInt (bandDb))) + " dB";
        rangeKnobs[(size_t) band]->setValueTextOverride (text);

        // Upward boosts rather than cuts, so a band's Range has no part in it.
        rangeKnobs[(size_t) band]->setAlpha (targets.upward ? 0.4f : 1.0f);
    }

    if (count != bands)
    {
        bands = count;
        repaint();
    }
}

void GateEditor::BandsCard::update (float seconds)
{
    refreshBands();

    if (frozen)
        return;

    auto& dsp = gateProcessor.getDsp();
    auto changed = false;

    for (int band = 0; band < GateDsp::maxBands; ++band)
    {
        auto taken = juce::jmin (dsp.takeBandReduction (band), 80.0f);

        if (band >= bands)
            taken = 0.0f;

        const auto next = juce::jmax (taken, shown[(size_t) band] - 60.0f * seconds, 0.0f);
        const auto open = band < bands && dsp.isBandOpen (band);

        if (std::abs (next - shown[(size_t) band]) > 0.01f || open != opened[(size_t) band])
        {
            shown[(size_t) band] = next;
            opened[(size_t) band] = open;
            changed = true;
        }
    }

    strip.setAttenuations (shown);

    if (changed)
        repaint();
}

void GateEditor::BandsCard::showValues (const std::array<float, GateDsp::maxBands>& attenuationsDb, const std::array<bool, GateDsp::maxBands>& open)
{
    frozen = true;
    shown = attenuationsDb;
    opened = open;
    refreshBands();
    strip.setAttenuations (shown);
    repaint();
}

void GateEditor::BandsCard::paint (juce::Graphics& g)
{
    g.setColour (colours::bgSection);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    g.setColour (colours::textSecondary);
    g.setFont (font (Weight::semiBold, 12.0f).withExtraKerningFactor (0.06f));
    g.drawText ("BANDS", 12, 6, 120, 24, juce::Justification::centredLeft, false);

    if (bands < 2)
    {
        g.setFont (font (Weight::regular, 11.0f));
        g.drawFittedText ("One band gates the whole signal with the cards below. Choose 2 or 3 to gate each band on its own "
                          "sound, with its own Threshold and Range.",
                          12, 100, 482, 34, juce::Justification::topLeft, 2, 1.0f);
        return;
    }

    for (int band = 0; band < bands; ++band)
    {
        const auto x = cellX (band);
        const auto chip = juce::Rectangle<float> (x + 2.0f, 108.0f, 18.0f, 18.0f);

        g.setColour (bandColours[band]);
        g.drawRoundedRectangle (chip.reduced (0.75f), 3.0f, 1.5f);
        g.setColour (colours::textPrimary);
        g.setFont (font (Weight::semiBold, 11.0f));
        g.drawText (juce::String (band + 1), chip, juce::Justification::centred, false);

        const auto open = opened[(size_t) band];
        g.setColour (open ? gateYellow : colours::textSecondary);
        g.setFont (font (Weight::regular, 10.0f));
        g.drawText (open ? "open" : "shut", juce::Rectangle<float> (x - 2.0f, 130.0f, 26.0f, 12.0f), juce::Justification::centred, false);
    }
}

// CeilingDropdown -------------------------------------------------------------

void GateEditor::CeilingDropdown::paint (juce::Graphics& g)
{
    paintDropdownBox (g, getLocalBounds().toFloat(), getValueText(), 12.0f, isMenuOpen());
}

void GateEditor::CeilingDropdown::mouseDown (const juce::MouseEvent& event)
{
    if (! event.mods.isPopupMenu() && (event.mods.isCtrlDown() || event.mods.isCommandDown()))
        openValueEditor();
    else
        ParamClickControl::mouseDown (event);
}

void GateEditor::CeilingDropdown::clicked()
{
    static constexpr float values[] { (float) fancy::dsp::OutputCeiling::offDb, 0.0f, -0.1f, -0.3f, -0.5f, -1.0f, -2.0f, -3.0f, -6.0f, -12.0f };
    static constexpr int typeId = 100;

    const auto current = param.convertFrom0to1 (param.getValue());

    Menu menu;

    for (int i = 0; i < (int) std::size (values); ++i)
        menu.addItem (i + 1, param.getText (param.convertTo0to1 (values[i]), 0), true, std::abs (values[i] - current) < 0.05f);

    menu.addSeparator();
    menu.addItem (typeId, "Type a value...");

    menu.show (*this, [safeThis = juce::Component::SafePointer<CeilingDropdown> (this)] (int result)
    {
        if (safeThis == nullptr || result <= 0)
            return;

        if (result == typeId)
            safeThis->openValueEditor();
        else
            safeThis->setDenormalised (values[result - 1]);
    });
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
    paintDropdownBox (g, getLocalBounds().toFloat(), text, 10.0f, isMenuOpen());
}

void GateEditor::MidiDropdown::mouseUp (const juce::MouseEvent& event)
{
    if (event.mouseWasClicked() && isEnabled())
        showMenu();
}

void GateEditor::MidiDropdown::showMenu()
{
    const auto targets = gateProcessor.readTargets();
    const auto dot = juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 "));

    // Ids: notes 1-128, channels 201-216, windows 301-309.
    Menu notes, channels, windows;
    notes.setScrollingList (true);
    channels.setScrollingList (true);

    for (int note = 0; note < 128; ++note)
        notes.addItem (1 + note, GateProcessor::noteName (note) + dot + juce::String (note), true, note == targets.note);

    for (int channel = 1; channel <= 16; ++channel)
        channels.addItem (200 + channel, "Channel " + juce::String (channel), true, channel == targets.channel);

    for (int step = 0; step < 9; ++step)
    {
        const auto ms = 1.0 + 0.5 * step;
        windows.addItem (301 + step, fancy::formatFixed ((float) ms, 1) + " ms", true, std::abs (ms - targets.velocityWindowMs) < 0.01);
    }

    Menu menu;
    menu.addSubMenu ("Note: " + GateProcessor::noteName (targets.note) + dot + juce::String (targets.note), std::move (notes));
    menu.addSubMenu ("Channel: " + juce::String (targets.channel), std::move (channels));
    menu.addSubMenu ("Velocity window: " + fancy::formatFixed ((float) targets.velocityWindowMs, 1) + " ms", std::move (windows));

    menu.show (*this, [safe = juce::Component::SafePointer<MidiDropdown> (this)] (int result)
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
      bandsCard (processor),
      preOpenKnob (parameter ("preopen"), "Pre-open", gateYellow, Knob::tightStyle),
      attackKnob (parameter ("attack"), "Attack", gateYellow, Knob::tightStyle),
      holdKnob (parameter ("hold"), "Hold", gateYellow, Knob::tightStyle),
      releaseKnob (parameter ("release"), "Release", gateYellow, Knob::tightStyle),
      hysteresisKnob (parameter ("hysteresis"), "Hysteresis", gateYellow, Knob::tightStyle, true),
      rangeKnob (parameter ("range"), "Range", gateYellow, Knob::tightStyle),
      rmsKnob (parameter ("rms"), "RMS size", gateYellow, Knob::tightStyle),
      lowpassKnob (parameter ("lowpass"), "Lowpass", colours::uiBlue, Knob::tightStyle),
      highpassKnob (parameter ("highpass"), "Highpass", colours::uiBlue, Knob::tightStyle),
      previewCheckbox (getFancyProcessor(), "Preview filter", gateYellow),
      linkCheckbox (parameter ("link"), "Link channels", gateYellow),
      midiCheckbox (parameter ("midi"), "MIDI on open/close", gateYellow),
      velocityCheckbox (parameter ("velocity"), "Velocity from level", gateYellow),
      midiDropdown (processor),
      outMeter (outGeometry),
      wetFader (parameter ("wet"), colours::uiTeal),
      dryFader (parameter ("dry"), colours::uiTeal),
      noiseFader (parameter ("noise"), noiseGrey),
      editLink (gateYellow),
      midiInCheckbox (parameter ("midiIn"), "MIDI trigger", gateYellow),
      ratioKnob (parameter ("ratio"), "Ratio", gateYellow, Knob::smallStyle),
      deBleedKnob (parameter ("deBleed"), "De-bleed", colours::uiBlue, Knob::smallStyle),
      syncRateDropdown (parameter ("syncRate")),
      syncShapeKnob (parameter ("syncShape"), "Shape", gateYellow, syncKnobStyle),
      detModeSegments (parameter ("detMode"), gateYellow, colours::bgHeader),
      riseKnob (parameter ("rise"), "Rise", gateYellow, Knob::tightStyle),
      boostKnob (parameter ("boost"), "Boost", gateYellow, Knob::tightStyle),
      upRatioKnob (parameter ("upRatio"), "Up ratio", gateYellow, Knob::smallStyle),
      ceilingDropdown (parameter ("ceiling"))
{
    auto& body = getBody();

    // Phase 2.1 W.1: room in the header for undo, redo, A/B and Compare.
    setShortHeaderLabels (true);
    setPresetDropdownWidth (150);

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
    addLatencyNote (lookaheadCheckbox);
    latencyLabel.setBounds (294, 9, 100, 24);
    modeLabel.setBounds (440, 9, 40, 24);
    modeSegments.setBounds (492, 9, 134, 24);   // Gate | Duck | Upward (mockup 54)
    modeSegments.setTextHeight (labelHeight);

    // Left column.
    thresholdMeter.setBounds (8, historyY, 58, 276 + tallY);
    gainMeter.setBounds (68, historyY, 40, 269 + tallY);
    light.setBounds (60, historyY + 271 + tallY, 56, 16);
    light.setInterceptsMouseClicks (false, false);

    // Centre.
    history.setBounds (centreX, historyY, centreWidth, historyHeight);
    bandsCard.setBounds (centreX, bandsCardY, centreWidth, bandsCardHeight); // Phase 4 Wave 3

    cardKnobs ({ &preOpenKnob, &attackKnob, &holdKnob, &releaseKnob, &hysteresisKnob }, centreX, centreWidth, envelopeY);
    cardKnobs ({ &rangeKnob, &rmsKnob }, centreX, levelWidth, lowerKnobY);
    cardKnobs ({ &lowpassKnob, &highpassKnob }, filterX, filterWidth, lowerKnobY);

    // Phase 4 Wave 2, v1.7.1: the Range card's title row (mockup 53), and the knobs that take
    // Range's and RMS size's places in Upward and Transient (shown by updateLocks).
    styleLabel (detModeLabel, "Detector", colours::textSecondary, juce::Justification::centredRight, labelHeight);
    detModeLabel.setBounds (196, lowerY + 2, 60, 18);
    detModeSegments.setBounds (262, lowerY + 2, 106, 18);
    detModeSegments.setTextHeight (labelHeight);
    detModeSegments.setTooltip ("Level opens on the level, as a gate does. Transient opens on a sudden rise in level: a drum hit, "
                                "not a bass note, pad or held chord swelling in, however loud. It's for slow-onset bleed; for cymbals use Lowpass.");
    riseKnob.setBounds (rmsKnob.getBounds());
    riseKnob.setTooltip ("How far the level must jump above its own recent average to count as a hit. Threshold is then a floor.");
    boostKnob.setBounds (rangeKnob.getBounds());
    boostKnob.setTooltip ("The most Upward raises the level by, above Threshold.");

    for (juce::Component* item : std::initializer_list<juce::Component*> { &detModeLabel, &detModeSegments })
        body.addAndMakeVisible (item);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &riseKnob, &boostKnob })
        body.addChildComponent (item);

    for (auto* knob : { &attackKnob, &holdKnob, &releaseKnob, &rmsKnob })
        knob->setShowDefaultMark (true);

    detectorDropdown.addSection (0, "Main input");
    detectorDropdown.addSection (3, "Sidechain");
    detectorDropdown.addSection (6, "Mid/side");
    detectorDropdown.addSection (8, "Host sync"); // Phase 4 Wave 2

    hysteresisKnob.setTooltip ("How far below Threshold the level must fall before the gate closes. "
                               "It stops chatter on signals that hover around the threshold.");
    rangeKnob.setTooltip ("How far the closed gate turns the signal down. -inf is silence.");
    holdKnob.setTooltip ("How long the gate stays open after the level falls below the close threshold.");

    // Footer.
    previewCheckbox.setBounds (14, footerY, 98, 20);
    linkCheckbox.setBounds (116, footerY, 98, 20);
    midiCheckbox.setBounds (218, footerY, 128, 20);
    midiDropdown.setBounds (346, footerY - 2, 78, 24);
    velocityCheckbox.setBounds (482, footerY, 140, 20);

    for (auto* checkbox : { &linkCheckbox, &midiCheckbox, &velocityCheckbox })
        checkbox->setLabelHeight (labelHeight);

    previewCheckbox.setLabelHeight (labelHeight);

    previewCheckbox.setTooltip ("Listen to what the detector hears, through Lowpass and Highpass. Turns off when you close this window, and is never saved.");
    linkCheckbox.setTooltip ("Both channels open and close together, on the louder one.");
    midiCheckbox.setTooltip ("Sends a note when the gate opens, and its note-off when it closes. "
                             "In REAPER, route this track's MIDI to an instrument track.");

    // Phase 4 Wave 1: the EDIT link in the top row, and the drawer over the knob cards, so the
    // history, meters and faders stay in view.
    // Wide enough for the whole word (it was clipped to "EDI" at 50 px).
    latencyLabel.setBounds (294, 9, 86, 24);
    editLink.setBounds (382, 9, 60, 24);
    modeLabel.setBounds (450, 9, 36, 24);
    editLink.onClick = [this] { drawer.setVisible (! drawer.isVisible()); };
    body.addAndMakeVisible (editLink);

    drawer.setBounds (centreX, envelopeY - 4, centreWidth, lowerY + lowerHeight - envelopeY + 4);

    midiInCheckbox.setBounds (18, 50, 140, 20);
    midiInCheckbox.setLabelHeight (12.0f);
    midiInCheckbox.setTooltip ("Play the gate from MIDI: a note-on opens it, its note-off closes it. The detector is ignored.");
    styleLabel (midiInNote, "Note-on opens, note-off closes. The detector is ignored.", colours::textSecondary, juce::Justification::topLeft, 11.0f);
    midiInNote.setBounds (40, 72, 190, 30);

    learnButton.setBounds (18, 118 + growY, 84, 26);
    learnButton.onClick = [this]
    {
        learnedLevels.clear();
        learnSeconds = 0.0f;
        learnButton.setText ("Listening...");
    };
    styleLabel (learnNote, "Listens for 3 s and sets Threshold", colours::textSecondary, juce::Justification::centredLeft, 11.0f);
    learnNote.setBounds (110, 118 + growY, 130, 26);

    // The drawer's right half moves with the 50 px the window gains (mockup 54).
    ratioKnob.setBounds (296, 40, 90, 99);
    ratioKnob.setTooltip ("Below inf:1 the gate becomes an expander: the level falls this many dB for each dB under Threshold, never deeper than Range.");
    deBleedKnob.setBounds (396, 40, 90, 99);
    deBleedKnob.setTooltip (deBleedTip);
    styleLabel (drawerNote, "De-bleed needs Lowpass and Highpass set: out-of-band thumps can't open the gate.",
                colours::textSecondary, juce::Justification::topLeft, 11.0f);
    drawerNote.setBounds (300, 146 + growY, 190, 44);

    // Phase 4 Wave 2, v1.7.1: Up ratio in Ratio's place in Upward, and Ceiling under Learn.
    upRatioKnob.setBounds (ratioKnob.getBounds());
    upRatioKnob.setTooltip ("In Upward, how many dB the level rises for each dB over Threshold, never more than Boost.");
    styleLabel (ceilingLabel, "Ceiling", colours::textSecondary, juce::Justification::centredLeft, 13.0f);
    ceilingLabel.setBounds (18, 189, 80, 24);
    ceilingDropdown.setBounds (108, 189, 120, 24);
    ceilingDropdown.setTooltip ("Holds the output's true peak under this level, so Upward's boost can't clip. Off adds no latency; on adds a little. "
                                "Ctrl-click to type a value.");
    addLatencyNote (ceilingDropdown);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &midiInCheckbox, &midiInNote, &learnButton, &learnNote, &ratioKnob, &deBleedKnob, &drawerNote,
                                                                           &ceilingLabel, &ceilingDropdown })
        drawer.addAndMakeVisible (item);

    drawer.addChildComponent (upRatioKnob);

    body.addChildComponent (drawer);

    // Phase 4 Wave 2: Host sync's group under Ratio and De-bleed, shown by updateSyncGroup.
    styleLabel (syncCaption, "HOST SYNC", colours::textSecondary, juce::Justification::topLeft, 10.0f);
    styleLabel (syncRateLabel, "Rate", colours::textPrimary, juce::Justification::centredLeft, 12.0f);
    syncCaption.setBounds (300, 146 + growY, 190, 12);
    syncRateLabel.setBounds (300, 166 + growY, 32, 24);
    syncRateDropdown.setBounds (332, 166 + growY, 88, 24);
    syncRateDropdown.setTextHeight (12.0f);
    syncShapeKnob.setBounds (428, 138 + growY, 60, 66);
    syncRateDropdown.setTooltip ("The grid Host sync opens the gate on, at the host's tempo.");
    syncShapeKnob.setTooltip ("How long each opening lasts, as a share of the step, whatever Threshold is.");

    for (juce::Component* item : std::initializer_list<juce::Component*> { &syncCaption, &syncRateLabel, &syncRateDropdown, &syncShapeKnob })
        drawer.addChildComponent (item);

    history.onColumns = [this] (const GateDsp::HistoryColumn* columns, int count) { learnFrom (columns, count); };

    // Right column.
    outMeter.setBounds (rightColumnX + 6, 10, 46, 350 + tallY);

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
             &thresholdMeter, &gainMeter, &light, &history, &bandsCard,
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

    // The drawer was added before the cards and knobs, so it went under them (seen in the
    // gate-drawer screenshot of run 44980548512): it belongs on top.
    drawer.toFront (false);

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

    // Phase 4 Wave 2, v1.7.1. Transient has no effect with the MIDI trigger or Host sync, which
    // bypass the detector; Hysteresis has no part in Transient or Upward.
    const auto detectorBypassed = targets.midiIn || targets.detector == GateDsp::hostSync;
    const auto transient = targets.transient && ! detectorBypassed;

    detModeSegments.setAlpha (detectorBypassed ? 0.4f : 1.0f);
    rmsKnob.setVisible (! targets.transient);
    riseKnob.setVisible (targets.transient);
    riseKnob.setAlpha (detectorBypassed ? 0.4f : 1.0f);
    rangeKnob.setVisible (! targets.upward);
    boostKnob.setVisible (targets.upward);
    ratioKnob.setVisible (! targets.upward);
    upRatioKnob.setVisible (targets.upward);
    upRatioKnob.setAlpha (transient ? 0.4f : 1.0f);   // with Transient, each hit is lifted by Boost
    ratioKnob.setAlpha (transient ? 0.4f : 1.0f);     // Ratio is the Level detector's expander
    hysteresisKnob.setAlpha (transient || targets.upward ? 0.4f : 1.0f);
    light.setUpward (targets.upward);

    // Phase 4 Wave 3 (decision 3 A): with bands, each band's detector is already its band, so the
    // detector's Lowpass, Highpass and De-bleed are out.
    const auto multiband = targets.bands > 1;

    for (auto* knob : { &lowpassKnob, &highpassKnob, &deBleedKnob })
        knob->setAlpha (multiband ? 0.4f : 1.0f);

    lowpassKnob.setTooltip (multiband ? juce::String (bandsFilterNote) : juce::String {});
    highpassKnob.setTooltip (multiband ? juce::String (bandsFilterNote) : juce::String {});
    deBleedKnob.setTooltip (multiband ? bandsFilterNote : deBleedTip);
}

void GateEditor::updateSyncGroup()
{
    const auto targets = gateProcessor().readTargets();
    const auto sync = targets.detector == GateDsp::hostSync;

    drawerNote.setVisible (! sync);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &syncCaption, &syncRateLabel, &syncRateDropdown, &syncShapeKnob })
    {
        item->setVisible (sync);
        item->setAlpha (targets.midiIn ? 0.4f : 1.0f); // MIDI trigger wins (Decision 2 A)
    }

    if (sync)
        syncCaption.setText (gateProcessor().getDsp().syncHasHostTempo() ? "HOST SYNC" : "NO HOST TEMPO: 120 BPM", juce::dontSendNotification);
}

void GateEditor::onTick (float seconds)
{
    auto& processor = gateProcessor();
    auto& dsp = processor.getDsp();

    updateLocks();
    updateSyncGroup(); // Phase 4 Wave 2

    const auto reduction = juce::jmin (dsp.takeReduction(), 80.0f);

    thresholdMeter.update (processor.inputMeter, false, seconds);
    gainMeter.update (reduction, reduction, seconds);
    light.setState (dsp.getLight());
    history.setWarning (dsp.isSidechainMissing());
    history.update();
    bandsCard.update (seconds); // Phase 4 Wave 3
    outMeter.update (processor.outputMeter, seconds);

    if (learnSeconds >= 0.0f)
    {
        learnSeconds += seconds;

        if (learnSeconds >= 3.0f)
            finishLearning();
    }
}

void GateEditor::learnFrom (const GateDsp::HistoryColumn* columns, int count)
{
    if (learnSeconds < 0.0f)
        return;

    for (int i = 0; i < count; ++i)
        if (columns[i].levelDb > -200.0f)
            learnedLevels.push_back (columns[i].levelDb);
}

void GateEditor::finishLearning()
{
    learnSeconds = -1.0f;
    learnButton.setText ("Learn");

    if (learnedLevels.size() < 8)
        return;

    std::sort (learnedLevels.begin(), learnedLevels.end());

    const auto at = [this] (double fraction) { return learnedLevels[(size_t) juce::jlimit (0, (int) learnedLevels.size() - 1, (int) (fraction * (double) learnedLevels.size()))]; };
    const auto floorDb = at (0.10), peakDb = at (0.95);

    if (peakDb - floorDb < 3.0f)
        return;

    auto& threshold = parameter ("threshold");
    const auto learned = floorDb + (peakDb - floorDb) / 3.0f;

    threshold.beginChangeGesture();
    threshold.setValueNotifyingHost (threshold.convertTo0to1 (learned));
    threshold.endChangeGesture();
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

void GateEditor::showDrawerPreview()
{
    showPreview (false);
    setValue (parameter ("detector"), (float) GateDsp::hostSync);
    drawer.setVisible (true);
    updateLocks();
    updateSyncGroup();
    syncCaption.setText ("HOST SYNC", juce::dontSendNotification);
}

void GateEditor::showModePreview (bool upward)
{
    showPreview (false);
    setValue (parameter (upward ? "mode" : "detMode"), upward ? 2.0f : 1.0f);
    updateLocks();
}

void GateEditor::showUpwardDrawerPreview()
{
    showPreview (false);
    setValue (parameter ("mode"), 2.0f);
    setValue (parameter ("ceiling"), -1.0f);
    drawer.setVisible (true);
    updateLocks();
    updateSyncGroup();
}

void GateEditor::showBandsPreview()
{
    // mockup 75's panel 1: a drum overhead, the lows open, the mids shut on bleed, the highs part-way.
    showPreview (false);
    setValue (parameter ("bands"), 2.0f);
    setValue (parameter ("crossover1"), 200.0f);
    setValue (parameter ("crossover2"), 3000.0f);
    setValue (parameter ("bandThreshold1"), 0.0f);
    setValue (parameter ("bandThreshold2"), 6.0f);
    setValue (parameter ("bandThreshold3"), 3.0f);
    setValue (parameter ("bandDepth3"), 50.0f);
    updateLocks();
    bandsCard.showValues ({ 0.0f, 40.0f, 12.0f }, { true, false, true });
}

juce::Colour GateEditor::getAccentColour()
{
    return gateYellow;
}

std::vector<ShellEditor::PopupOpener> GateEditor::getPopupOpeners()
{
    auto openers = ShellEditor::getPopupOpeners();
    openers.push_back ({ "dropdown-detector", [this] { detectorDropdown.click(); } });
    openers.push_back ({ "midi-menu", [this]
                         {
                             midiDropdown.showMenu();

                             if (auto* menu = MenuLayer::findOpen (getRoot()))
                             {
                                 auto& panel = menu->getPanel (0);
                                 panel.setHighlightedRow (0);
                                 menu->openSubMenu (panel, 0);
                             }
                         } });
    openers.push_back ({ "tooltip-preopen", [this] { showTip (preOpenKnob, preOpenKnob.getTooltip()); } });
    openers.push_back ({ "value-release", [this] { releaseKnob.openValueEditor(); } });
    return openers;
}
