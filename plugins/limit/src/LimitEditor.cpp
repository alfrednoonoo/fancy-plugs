#include "LimitEditor.h"

#include <fancy/plugin/ParamFormat.h>

#include <cmath>

using namespace fancy::ui;

namespace
{
// Phase 4 Wave 3 (mockup 74 A): the BANDS card goes where the LIMITER card was, and the window
// grows 172 px taller; the LIMITER card and the footer move down, and the meters lengthen.
constexpr int bandsGrow = 172;

// design/phase3-mockup-src/limit.html at 100 %, with the right column widened to 120 px for
// Out's scale (docs/limit.md, decision 3).
constexpr int baseWidth = 760;
constexpr int bodyHeight = 392 + bandsGrow;
constexpr int baseHeight = ShellEditor::headerHeight + bodyHeight;
constexpr int rightColumnX = 640;

constexpr int leftX = 13, contentRight = 630;
constexpr int rowY = 10, rowHeight = 26;
constexpr int columnY = 44;
constexpr int displayX = 119, displayWidth = contentRight - displayX, displayHeight = 160;
constexpr int bandsCardY = 212, bandsCardHeight = 164;
constexpr int cardY = 212 + bandsGrow, cardHeight = 127;
constexpr int footerY = 362 + bandsGrow;

constexpr float labelHeight = 12.0f;
constexpr float displayRangeDb = 24.0f;

const juce::Colour limitRed = colours::uiRed;

// The meter's top, bottom and 0 dB line are shared by Out and the LUFS bar, so they line up.
constexpr float meterTop = 25.0f, meterBottom = 330.0f + (float) bandsGrow, meterZeroY = 40.0f;
constexpr float meterPixelsPerDb = (meterBottom - meterZeroY) / 54.0f;
constexpr OutMeter::Geometry outGeometry { 32.5f, meterTop, meterBottom, meterZeroY, meterPixelsPerDb, 338.0f + (float) bandsGrow, true, true };
constexpr LoudnessBar::Geometry loudnessGeometry { 14.0f, meterTop, meterBottom, meterZeroY, meterPixelsPerDb, 338.0f + (float) bandsGrow };

constexpr GainReductionMeter::Geometry gainGeometry { 13.0f, 20.0f, 246.0f + (float) bandsGrow, true, "GR" };

// Phase 4 Wave 3: the bands' colours, as Figma's EQ graph and MultiComp's bands.
const juce::Colour bandColours[] { colours::uiRed, colours::uiPurple, colours::uiBlue, colours::uiTealBright };
constexpr Knob::Style bandKnobStyle { 32.0f, 27.0f, 3.0f, 9.0f, 7.0f, 13.0f, 3.0f, 2.0f, 16.0f, 11.0f, false, true };

ThresholdMeter::Style thresholdStyle()
{
    ThresholdMeter::Style style;
    style.wellX = 12.0f;
    style.wellTop = 20.0f;
    style.wellHeight = 246.0f + (float) bandsGrow;
    style.lowestDb = -30.0f;   // the parameter's range, so the handle and the levels agree
    style.accent = limitRed;
    style.labelHeight = labelHeight;
    style.secondaryLabel = true;
    style.scale = true;
    style.gateReadouts = true;
    style.scaleStepDb = 6.0f;
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

juce::String lufsText (double value, int decimals = 1)
{
    return value <= -100.0 ? juce::String ("-inf") : fancy::formatFixed ((float) value, decimals);
}
} // namespace

// Panels ----------------------------------------------------------------------

void LimitEditor::Panels::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgBody);

    g.setColour (colours::bgSection);
    g.fillRect (rightColumnX, 0, baseWidth - rightColumnX, bodyHeight);
    g.fillRoundedRectangle ((float) displayX, (float) cardY, (float) displayWidth, (float) cardHeight, 6.0f);

    g.setColour (colours::textSecondary);
    g.setFont (font (Weight::semiBold, 12.0f).withExtraKerningFactor (0.06f));
    g.drawText ("LIMITER", displayX + 12, cardY + 6, 120, 24, juce::Justification::centredLeft, false);
}

// Display ---------------------------------------------------------------------

LimitEditor::Display::Display (LimitProcessor& processor)
    : limitProcessor (processor)
{
    setTooltip ("Input (grey), output (white) and gain reduction (red) over the last 4 s, with the ceiling dashed. "
                "Click the read-outs to reset the integrated loudness.");
}

void LimitEditor::Display::resized()
{
    const auto historyWidth = juce::jmax (1, getWidth() - panelWidth);
    columns.assign ((size_t) historyWidth, { -200.0f, -200.0f, 0.0f });
    incoming.resize ((size_t) historyWidth);
    writeIndex = 0;
    limitProcessor.getDsp().setHistoryColumns (historyWidth);
}

juce::Rectangle<float> LimitEditor::Display::panelBounds() const
{
    return getLocalBounds().toFloat().removeFromRight ((float) panelWidth);
}

float LimitEditor::Display::yForDb (float db) const
{
    const auto top = 10.0f, height = (float) getHeight() - 20.0f;
    return top + juce::jlimit (0.0f, 1.0f, -db / displayRangeDb) * height;
}

void LimitEditor::Display::update()
{
    if (frozen)
        return;

    auto& dsp = limitProcessor.getDsp();
    const auto count = dsp.popHistory (incoming.data(), (int) incoming.size());

    for (int i = 0; i < count && ! columns.empty(); ++i)
    {
        columns[(size_t) writeIndex] = incoming[(size_t) i];
        writeIndex = (writeIndex + 1) % (int) columns.size();
    }

    readings.shortTerm = dsp.loudness.getShortTerm();
    readings.integrated = dsp.loudness.getIntegrated();
    readings.truePeak = dsp.truePeakMeter.getPeakDb();
    readings.range = dsp.loudness.getRange();
    readings.overs = dsp.getOvers();
    ceilingDb = (float) limitProcessor.readTargets().ceilingDb;

    repaint();
}

void LimitEditor::Display::showColumns (std::vector<LimitDsp::HistoryColumn> newColumns, const Readings& newReadings)
{
    frozen = true;
    columns = std::move (newColumns);
    writeIndex = 0;
    readings = newReadings;
    ceilingDb = (float) limitProcessor.readTargets().ceilingDb;
    repaint();
}

void LimitEditor::Display::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto panel = panelBounds();
    const auto historyWidth = (float) columns.size();

    g.setColour (colours::bgSection);
    g.fillRoundedRectangle (bounds, 6.0f);

    // History: oldest on the left.
    if (! columns.empty())
    {
        juce::Path input, output, reductionPath, clipPath;
        auto anyClipped = false;
        const auto bottom = bounds.getBottom();
        const auto count = (int) columns.size();

        input.startNewSubPath (0.0f, bottom);
        reductionPath.startNewSubPath (0.0f, 0.0f);
        clipPath.startNewSubPath (0.0f, 0.0f);

        for (int i = 0; i < count; ++i)
        {
            const auto& column = columns[(size_t) ((writeIndex + i) % count)];
            const auto x = (float) i;

            input.lineTo (x, yForDb (column.inputDb));
            reductionPath.lineTo (x, juce::jlimit (0.0f, bottom, column.reductionDb * ((float) getHeight() - 20.0f) / displayRangeDb));
            clipPath.lineTo (x, juce::jlimit (0.0f, bottom, column.clipDb * ((float) getHeight() - 20.0f) / displayRangeDb));
            anyClipped = anyClipped || column.clipDb > 0.0f;

            if (i == 0)
                output.startNewSubPath (x, yForDb (column.outputDb));
            else
                output.lineTo (x, yForDb (column.outputDb));
        }

        input.lineTo (historyWidth, bottom);
        input.closeSubPath();
        reductionPath.lineTo (historyWidth, 0.0f);
        reductionPath.closeSubPath();
        clipPath.lineTo (historyWidth, 0.0f);
        clipPath.closeSubPath();

        g.saveState();
        g.reduceClipRegion (juce::Rectangle<float> (0.0f, 0.0f, historyWidth, bounds.getHeight()).toNearestInt());

        g.setColour (colours::textSecondary.withAlpha (0.28f));
        g.fillPath (input);

        g.setColour (juce::Colour (0xffededed).withAlpha (0.8f));
        g.strokePath (output, juce::PathStrokeType (1.2f));

        g.setColour (limitRed.withAlpha (0.75f));
        g.fillPath (reductionPath);

        // Phase 4 Wave 2: what the clipper took, in orange over the limiter's red.
        const auto clipping = anyClipped || limitProcessor.readTargets().clip;

        if (anyClipped)
        {
            g.setColour (colours::accentOrange);
            g.fillPath (clipPath);
        }

        // The ceiling, dashed.
        const auto y = yForDb (ceilingDb);
        g.setColour (limitRed);

        for (auto x = 0.0f; x < historyWidth; x += 7.0f)
            g.fillRect (x, y - 0.5f, 4.0f, 1.0f);

        g.setColour (colours::textSecondary);
        g.setFont (font (Weight::regular, 10.0f));
        g.drawText (juce::String::fromUTF8 (clipping ? "input (grey) \xc2\xb7 output (white) \xc2\xb7 reduction (red) \xc2\xb7 clipped (orange) \xc2\xb7 ceiling"
                                                     : "input (grey) \xc2\xb7 output (white) \xc2\xb7 reduction (red) \xc2\xb7 ceiling"),
                    juce::Rectangle<float> (8.0f, bounds.getBottom() - 22.0f, historyWidth - 16.0f, 16.0f), juce::Justification::centredLeft, false);

        g.restoreState();
    }

    // The read-outs.
    {
        juce::Path shape;
        shape.addRoundedRectangle (panel.getX(), panel.getY(), panel.getWidth(), panel.getHeight(), 6.0f, 6.0f, false, true, false, true);
        g.setColour (colours::bgWell);
        g.fillPath (shape);
    }

    const std::pair<const char*, juce::String> rows[] {
        { "Short-term", lufsText (readings.shortTerm) },
        { "Integrated", lufsText (readings.integrated) },
        { "True peak", lufsText (readings.truePeak) },
        { "Range", fancy::formatFixed ((float) readings.range, 1) },
    };
    const char* units[] { "LUFS", "LUFS", "dBTP", "LU" };

    auto y = panel.getY() + 14.0f;

    for (int i = 0; i < 4; ++i)
    {
        g.setColour (colours::textSecondary);
        g.setFont (font (Weight::regular, 10.0f));
        g.drawText (rows[i].first, juce::Rectangle<float> (panel.getX() + 12.0f, y, panel.getWidth() - 16.0f, 14.0f), juce::Justification::centredLeft, false);

        const auto valueFont = font (Weight::regular, 15.0f);
        const auto valueWidth = juce::GlyphArrangement::getStringWidth (valueFont, rows[i].second);

        g.setColour (colours::textPrimary);
        g.setFont (valueFont);
        g.drawText (rows[i].second, juce::Rectangle<float> (panel.getX() + 12.0f, y + 13.0f, valueWidth + 2.0f, 18.0f), juce::Justification::centredLeft, false);

        g.setColour (colours::textSecondary);
        g.setFont (font (Weight::regular, 10.0f));
        g.drawText (units[i], juce::Rectangle<float> (panel.getX() + 16.0f + valueWidth, y + 15.0f, 40.0f, 16.0f), juce::Justification::centredLeft, false);

        // Phase 4 Wave 1: the overs since the last reset, beside the held true peak.
        if (i == 2)
        {
            g.setColour (readings.overs > 0 ? limitRed : colours::textSecondary);
            g.drawText (juce::String (readings.overs) + (readings.overs == 1 ? " over" : " overs"),
                        juce::Rectangle<float> (panel.getRight() - 58.0f, y, 50.0f, 14.0f), juce::Justification::centredRight, false);
        }

        y += 34.0f;
    }
}

void LimitEditor::Display::mouseUp (const juce::MouseEvent& event)
{
    if (event.mouseWasClicked() && panelBounds().contains (event.position))
    {
        limitProcessor.resetLoudness();
        frozen = false;
    }
}

// TargetDropdown --------------------------------------------------------------

LimitEditor::TargetDropdown::TargetDropdown (LimitProcessor& processor)
    : limitProcessor (processor)
{
    setTooltip ("The loudness you're aiming for. It moves the red marker on the LUFS meter; it doesn't change the sound.");
    refresh();
}

void LimitEditor::TargetDropdown::refresh()
{
    auto& parameter = *limitProcessor.getApvts().getParameter ("target");
    const auto index = juce::roundToInt (parameter.convertFrom0to1 (parameter.getValue()));

    const auto newText = index == LimitProcessor::custom
                           ? juce::String::fromUTF8 ("Custom \xc2\xb7 ") + fancy::formatFixed (limitProcessor.getTargetLufs(), 1) + " LUFS"
                           : parameter.getCurrentValueAsText();

    if (newText != text)
    {
        text = newText;
        repaint();
    }
}

void LimitEditor::TargetDropdown::paint (juce::Graphics& g)
{
    paintDropdownBox (g, getLocalBounds().toFloat(), text, labelHeight, isMenuOpen());
}

void LimitEditor::TargetDropdown::mouseUp (const juce::MouseEvent& event)
{
    if (event.mouseWasClicked() && isEnabled())
        showMenu();
}

void LimitEditor::TargetDropdown::showMenu()
{
    auto& parameter = *limitProcessor.getApvts().getParameter ("target");
    const auto names = parameter.getAllValueStrings();
    const auto selected = juce::roundToInt (parameter.convertFrom0to1 (parameter.getValue()));

    Menu menu;

    for (int i = 0; i < names.size(); ++i)
        menu.addItem (i + 1, names[i], true, i == selected);

    menu.show (*this, [safe = juce::Component::SafePointer<TargetDropdown> (this)] (int result)
    {
        if (safe == nullptr || result == 0)
            return;

        if (result - 1 == LimitProcessor::custom)
        {
            safe->askForCustom();
            return;
        }

        safe->limitProcessor.setParameter ("target", (float) (result - 1));
        safe->refresh();
    });
}

void LimitEditor::TargetDropdown::askForCustom()
{
    DialogSpec spec;
    spec.title = "Custom target";
    spec.body = "Integrated loudness to aim for, from -36 to -5 LUFS.";
    spec.action = "Set";
    spec.fields.push_back ({ "Target (LUFS)", fancy::formatFixed (limitProcessor.getTargetLufs(), 1) });
    spec.validate = [] (const juce::StringArray& values)
    {
        const auto text = values[0].upToFirstOccurrenceOf ("LUFS", false, true).trim();
        const auto value = fancy::parseNumber (text);

        if (text.isEmpty() || ! std::isfinite (value) || value < -36.0f || value > -5.0f)
            return juce::String ("Enter a value from -36 to -5.");

        return juce::String {};
    };

    showDialog (*this, spec, [safe = juce::Component::SafePointer<TargetDropdown> (this)] (bool accepted, const juce::StringArray& values)
    {
        if (safe == nullptr || ! accepted)
            return;

        const auto value = fancy::parseNumber (values[0].upToFirstOccurrenceOf ("LUFS", false, true).trim());
        fancy::FancyProcessor::UserEdit edit (safe->limitProcessor, "Custom target");
        safe->limitProcessor.setParameter ("customTarget", value);
        safe->limitProcessor.setParameter ("target", (float) LimitProcessor::custom);
        safe->refresh();
    });
}

// LimitEditor -----------------------------------------------------------------

LimitEditor::LimitEditor (LimitProcessor& processor)
    : ShellEditor (processor, baseWidth, baseHeight, HeaderStyle::full),
      styleSegments (parameter ("style"), limitRed, colours::bgWell),
      targetDropdown (processor),
      thresholdMeter (parameter ("threshold"), thresholdStyle()),
      gainMeter (gainGeometry),
      display (processor),
      bandsCard (processor),
      ceilingKnob (parameter ("ceiling"), "Ceiling", limitRed, Knob::tightStyle),
      releaseKnob (parameter ("release"), "Release", limitRed, Knob::tightStyle),
      lookaheadKnob (parameter ("lookahead"), "Lookahead", limitRed, Knob::tightStyle),
      linkKnob (parameter ("link"), "Link", limitRed, Knob::tightStyle),
      truePeakCheckbox (parameter ("truePeak"), "True peak", limitRed),
      autoReleaseCheckbox (parameter ("autoRelease"), "Auto release", limitRed),
      listenCheckbox (parameter ("listen"), "Listen to removed", limitRed),
      editLink (limitRed),
      outMeter (outGeometry),
      loudnessBar (loudnessGeometry),
      oversamplingDropdown (parameter ("oversampling")),
      phaseDropdown (parameter ("osPhase")),
      ditherDropdown (parameter ("dither")),
      ballisticsDropdown (parameter ("ballistics")),
      releaseCurveDropdown (parameter ("releaseCurve")),
      dcCheckbox (parameter ("dcFilter"), "5 Hz high-pass", limitRed),
      clipCheckbox (parameter ("clip"), "Clip before limiting", limitRed),
      clipKnob (parameter ("clipAmount"), "Clip", limitRed, Knob::tightStyle)
{
    auto& body = getBody();

    // Phase 2.1 W.1: room in the header for undo, redo, A/B and Compare.
    setShortHeaderLabels (true);
    setPresetDropdownWidth (150);

    panels.setInterceptsMouseClicks (false, false);
    panels.setBounds (0, 0, baseWidth, bodyHeight);
    body.addAndMakeVisible (panels);

    // The row under the header.
    setUpLabel (styleLabel, "Style", juce::Justification::centredLeft);
    setUpLabel (targetLabel, "Target", juce::Justification::centredRight);

    styleLabel.setBounds (leftX, rowY, 36, rowHeight);
    styleSegments.setBounds (52, rowY + 1, 172, 24);
    styleSegments.setTextHeight (labelHeight);
    styleSegments.setTooltip ("Clean: smoothest. Punchy: reduces later, so transients keep their front. Dense: faster release, more consistent loudness.");

    targetLabel.setBounds (410, rowY, 52, rowHeight);
    targetDropdown.setBounds (470, rowY + 1, contentRight - 470, 24);

    // Left column.
    thresholdMeter.setBounds (8, columnY, 66, 282 + bandsGrow);
    thresholdMeter.setTooltip ("Threshold: the signal is pushed up by this much, then held at Ceiling.");
    gainMeter.setBounds (78, columnY, 34, 290 + bandsGrow);
    gainMeter.setBarColour (limitRed);

    // The display and the Limiter card.
    display.setBounds (displayX, columnY, displayWidth, displayHeight);
    bandsCard.setBounds (displayX, bandsCardY, displayWidth, bandsCardHeight); // Phase 4 Wave 3

    Knob* knobs[] { &ceilingKnob, &releaseKnob, &lookaheadKnob, &linkKnob };
    constexpr int knobWidth = 76, knobHeight = 92;
    const auto spacing = (float) displayWidth / 4.0f;

    for (int i = 0; i < 4; ++i)
    {
        const auto centre = (float) displayX + spacing * ((float) i + 0.5f);
        knobs[i]->setBounds (juce::roundToInt (centre) - knobWidth / 2, cardY + 30, knobWidth, knobHeight);
    }

    ceilingKnob.setTooltip ("The highest level the output reaches. With True peak on, it's in dBTP.");
    releaseKnob.setTooltip ("How fast the gain recovers after a peak. With Auto release on, sustained limiting recovers more slowly.");
    lookaheadKnob.setTooltip ("How early the limiter starts to turn down before a peak. 0 ms is instant and can distort. "
                              "The latency stays at 5 ms whatever this is set to.");
    linkKnob.setTooltip ("How much the two channels share their gain reduction. 100 % keeps the stereo image still.");

    for (auto* knob : { &releaseKnob, &lookaheadKnob })
        knob->setShowDefaultMark (true);

    // Footer.
    truePeakCheckbox.setBounds (leftX, footerY, 100, 20);
    autoReleaseCheckbox.setBounds (leftX + 112, footerY, 116, 20);
    listenCheckbox.setBounds (leftX + 240, footerY, 160, 20);
    editLink.setBounds (contentRight - 64, footerY - 2, 64, 24);

    for (auto* checkbox : { &truePeakCheckbox, &autoReleaseCheckbox, &listenCheckbox })
        checkbox->setLabelHeight (labelHeight);

    truePeakCheckbox.setTooltip ("Holds the peaks between samples under Ceiling too, so the output stays under it after conversion or encoding.");
    autoReleaseCheckbox.setTooltip ("Recovers fast from short peaks and slowly from sustained limiting, to avoid pumping and bass distortion.");
    listenCheckbox.setTooltip ("Plays only what the limiter takes away.");

    // Phase 4 Wave 1: Gain lock remembers Threshold as it's switched on (Decision 1 A).
    gainLockCheckbox.setBounds (leftX + 412, footerY, 100, 20);
    gainLockCheckbox.onChange = [this] (bool on)
    {
        auto& processor = limitProcessor();

        if (on)
            processor.setParameter ("gainLockRef", processor.getApvts().getRawParameterValue ("threshold")->load());

        processor.setParameter ("gainLock", on ? 1.0f : 0.0f);
    };
    body.addAndMakeVisible (gainLockCheckbox);

    autoTargetButton.setBounds (290, rowY + 1, 112, 24);
    autoTargetButton.setTooltip ("Listens for 10 s of playback, then moves Threshold once so the integrated loudness meets the Target.");
    autoTargetButton.onClick = [this]
    {
        if (autoTargetButton.progress >= 0.0f)
        {
            autoTargetButton.progress = -1.0f;
        }
        else if (! limitProcessor().readTargets().gainLock)
        {
            limitProcessor().resetLoudness();
            autoTargetSeconds = 0.0f;
            autoTargetButton.progress = 0.0f;
        }

        autoTargetButton.repaint();
    };
    body.addAndMakeVisible (autoTargetButton);

    // Right column.
    outMeter.setBounds (rightColumnX + 2, 10, 74, 372 + bandsGrow);
    loudnessBar.setBounds (rightColumnX + 76, 10, 40, 372 + bandsGrow);
    loudnessBar.setTooltip ("Short-term loudness (3 s). The red line is the Target.");

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &styleLabel, &styleSegments, &targetLabel, &targetDropdown,
             &thresholdMeter, &gainMeter, &display, &bandsCard,
             &ceilingKnob, &releaseKnob, &lookaheadKnob, &linkKnob,
             &truePeakCheckbox, &autoReleaseCheckbox, &listenCheckbox, &editLink,
             &outMeter, &loudnessBar })
        body.addAndMakeVisible (item);

    // EDIT drawer, over the left part of the body.
    drawer.setBounds (0, columnY - 4, rightColumnX, bodyHeight - columnY + 4);

    struct Row
    {
        juce::Label& label;
        const char* text;
        juce::Component& control;
    };

    const Row rows[] {
        { oversamplingLabel, "Oversampling", oversamplingDropdown },
        { phaseLabel, "Oversampling filter", phaseDropdown },
        { ditherLabel, "Dither", ditherDropdown },
        { ballisticsLabel, "Meter ballistics", ballisticsDropdown },
        { resetLabel, "Loudness", resetButton },
        { releaseCurveLabel, "Release curve", releaseCurveDropdown },
        { dcLabel, "DC filter", dcCheckbox },
    };

    auto y = 60;

    for (const auto& row : rows)
    {
        setUpLabel (row.label, row.text, juce::Justification::centredLeft, 14.0f);
        row.label.setBounds (24, y, 170, 24);
        row.control.setBounds (200, y, 170, 24);
        drawer.addAndMakeVisible (row.label);
        drawer.addAndMakeVisible (row.control);
        y += 40;
    }

    oversamplingDropdown.setTooltip ("Runs the limiter at a higher rate. Off can alias at high reduction.");
    phaseDropdown.setTooltip ("Minimum phase has the least latency. Linear phase keeps the phase of the highs.");
    addLatencyNote (oversamplingDropdown);
    addLatencyNote (phaseDropdown);
    ditherDropdown.setTooltip ("Adds low-level noise before a bounce to a lower bit depth. Shaped moves it to the highs. "
                               "Keep Ceiling at -0.1 or lower with dither on.");
    ballisticsDropdown.setTooltip ("What the LUFS meter shows: short-term loudness (EBU) or plain RMS.");
    releaseCurveDropdown.setTooltip ("Exponential recovers fast and then gently. Linear recovers at a constant rate in dB.");
    dcCheckbox.setTooltip ("Removes DC offset before the limiter, so it can't use up headroom.");
    resetButton.onClick = [&processor] { processor.resetLoudness(); };

    // Phase 4 Wave 2: Clip before limiting, beside the DC filter and Release curve rows.
    clipCheckbox.setBounds (400, 300, 160, 24);
    clipKnob.setBounds (556, 244, 76, 92); // its value clear of the drawer's bottom edge
    clipCheckbox.setTooltip ("A soft clipper before the limiter: it shaves the fastest peaks, so the limiter has less to do. "
                             "For dense, drum-heavy material.");
    clipKnob.setTooltip ("How hard the clipper works: its ceiling sits this much under Ceiling + 12 dB. 2 to 4 dB is the usual range.");
    drawer.addAndMakeVisible (clipCheckbox);
    drawer.addAndMakeVisible (clipKnob);

    body.addChildComponent (drawer);
    editLink.onClick = [this] { drawer.setVisible (! drawer.isVisible()); };
}

// BandStrip (Phase 4 Wave 3) --------------------------------------------------------

LimitEditor::BandStrip::BandStrip (LimitProcessor& processor)
    : limitProcessor (processor)
{
    setTooltip ("Each band's gain reduction, falling from the top in its colour. Drag a blue line to move a crossover, "
                "or click it to type a frequency.");
}

void LimitEditor::BandStrip::setReductions (const std::array<float, LimitDsp::maxBands>& reductionsDb)
{
    if (reductionsDb != reductions)
    {
        reductions = reductionsDb;
        repaint();
    }
}

float LimitEditor::BandStrip::xForHz (double hz) const noexcept
{
    return (float) getWidth() * (float) std::log10 (juce::jlimit (20.0, 20000.0, hz) / 20.0) / 3.0f;
}

double LimitEditor::BandStrip::hzForX (float x) const noexcept
{
    return 20.0 * std::pow (1000.0, juce::jlimit (0.0, 1.0, (double) x / (double) juce::jmax (1, getWidth())));
}

int LimitEditor::BandStrip::crossoverAt (juce::Point<float> position) const
{
    const auto targets = limitProcessor.readTargets();
    const auto hz = LimitDsp::sanitisedCrossovers (targets, targets.bands);

    for (int i = 0; i < targets.bands - 1; ++i)
        if (std::abs (position.x - xForHz (hz[(size_t) i])) <= 5.0f && position.y <= stripHeight)
            return i;

    return -1;
}

void LimitEditor::BandStrip::paint (juce::Graphics& g)
{
    const auto width = (float) getWidth();
    const auto h = stripHeight;
    const auto targets = limitProcessor.readTargets();
    const auto count = targets.bands;
    const auto hz = LimitDsp::sanitisedCrossovers (targets, count);

    g.setColour (colours::bgWell);
    g.fillRect (0.0f, 0.0f, width, h);

    std::array<float, LimitDsp::maxBands + 1> edges {};
    edges[0] = 0.0f;

    for (int i = 0; i < count - 1; ++i)
        edges[(size_t) i + 1] = xForHz (hz[(size_t) i]);

    edges[(size_t) count] = width;

    for (int i = 0; i < count; ++i)
    {
        const auto x0 = edges[(size_t) i], x1 = edges[(size_t) i + 1];
        const auto colour = bandColours[i];
        const auto drop = h * juce::jlimit (0.0f, scaleDb, reductions[(size_t) i]) / scaleDb;

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

void LimitEditor::BandStrip::mouseDown (const juce::MouseEvent& event)
{
    dragging = event.mods.isPopupMenu() ? -1 : crossoverAt (event.position);
    dragged = false;
}

void LimitEditor::BandStrip::mouseDrag (const juce::MouseEvent& event)
{
    if (dragging < 0 || (! dragged && event.getDistanceFromDragStart() < 3))
        return;

    dragged = true;

    // Between its neighbours, 1/3 octave from each, as the stage keeps them.
    const auto targets = limitProcessor.readTargets();
    const auto hz = LimitDsp::sanitisedCrossovers (targets, targets.bands);
    constexpr auto gap = 1.2599210498948732;
    const auto low = dragging > 0 ? hz[(size_t) dragging - 1] * gap : 20.0;
    const auto high = dragging < targets.bands - 2 ? hz[(size_t) dragging + 1] / gap : 20000.0;

    limitProcessor.setParameter (LimitProcessor::crossoverId (dragging), (float) juce::jlimit (low, high, hzForX (event.position.x)));
    repaint();
}

void LimitEditor::BandStrip::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0 && ! dragged)
        askForCrossover (dragging);

    dragging = -1;
    dragged = false;
}

void LimitEditor::BandStrip::askForCrossover (int index)
{
    const auto targets = limitProcessor.readTargets();
    const auto hz = LimitDsp::sanitisedCrossovers (targets, juce::jmax (2, targets.bands));

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

        fancy::FancyProcessor::UserEdit edit (safe->limitProcessor, "Crossover");
        safe->limitProcessor.setParameter (LimitProcessor::crossoverId (index), fancy::parseHz (values[0]));
        safe->repaint();
    });
}

// BandsCard (Phase 4 Wave 3) ------------------------------------------------------

LimitEditor::BandsCard::BandsCard (LimitProcessor& processor)
    : strip (processor),
      limitProcessor (processor),
      bandsSegments (*processor.getApvts().getParameter ("bands"), limitRed, colours::bgWell),
      crossoverDropdown (*processor.getApvts().getParameter ("crossoverType"))
{
    setUpLabel (crossoverLabel, "Crossover", juce::Justification::centredLeft);
    crossoverLabel.setBounds (277, 5, 62, 24);

    bandsSegments.setBounds (67, 8, 116, 20);
    bandsSegments.setTextHeight (labelHeight);
    bandsSegments.setTooltip ("How many bands to split the signal into. Each is limited on its own first, so a loud kick no longer "
                              "turns down the rest. 1 is the plain limiter.");
    addLatencyNote (bandsSegments);

    // MultiComp's width for the same text at 12 px, so "Min phase" isn't cut off; the right edge is
    // the mockup's, the label moves 14 px left.
    crossoverDropdown.setBounds (341, 7, 157, 22);
    crossoverDropdown.setTextHeight (labelHeight);
    crossoverDropdown.setTooltip ("The crossovers' slope. Both sum back flat.");

    strip.setBounds (12, 34, 486, (int) BandStrip::stripHeight + 13);

    for (int band = 0; band < LimitDsp::maxBands; ++band)
    {
        auto& parameter = *processor.getApvts().getParameter (LimitProcessor::bandReleaseId (band));
        auto knob = std::make_unique<Knob> (parameter, "Release", bandColours[band], bandKnobStyle);
        knob->setBounds (juce::roundToInt (cellX (band) + 58.0f) - 35, 94, 70, 66);
        knob->setShowDefaultMark (true);
        knob->setTooltip ("How fast band " + juce::String (band + 1) + " recovers. With Auto release on, sustained limiting "
                          "recovers more slowly from here.");
        addChildComponent (*knob);
        releaseKnobs[(size_t) band] = std::move (knob);
    }

    for (juce::Component* item : std::initializer_list<juce::Component*> { &crossoverLabel, &bandsSegments, &crossoverDropdown, &strip })
        addAndMakeVisible (item);

    refreshBands();
}

float LimitEditor::BandsCard::cellX (int band) noexcept
{
    return 12.0f + 121.5f * (float) band;
}

void LimitEditor::BandsCard::refreshBands()
{
    const auto count = limitProcessor.readTargets().bands;

    for (int band = 0; band < LimitDsp::maxBands; ++band)
        releaseKnobs[(size_t) band]->setVisible (count > 1 && band < count);

    if (count != bands)
    {
        bands = count;
        repaint();
    }
}

void LimitEditor::BandsCard::update (float finalReductionDb, float seconds)
{
    refreshBands();

    if (frozen)
        return;

    auto& dsp = limitProcessor.getDsp();
    auto changed = false;

    for (int band = 0; band < LimitDsp::maxBands; ++band)
    {
        const auto taken = dsp.takeBandReduction (band);
        const auto target = bands > 1 ? (band < bands ? taken : 0.0f) : (band == 0 ? finalReductionDb : 0.0f);
        const auto next = juce::jmax (target, shown[(size_t) band] - 20.0f * seconds, 0.0f);

        if (std::abs (next - shown[(size_t) band]) > 0.01f)
        {
            shown[(size_t) band] = next;
            changed = true;
        }
    }

    strip.setReductions (shown);

    if (changed)
        repaint();
}

void LimitEditor::BandsCard::showValues (const std::array<float, LimitDsp::maxBands>& reductionsDb)
{
    frozen = true;
    shown = reductionsDb;
    refreshBands();
    strip.setReductions (shown);
    repaint();
}

void LimitEditor::BandsCard::paint (juce::Graphics& g)
{
    g.setColour (colours::bgSection);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    g.setColour (colours::textSecondary);
    g.setFont (font (Weight::semiBold, 12.0f).withExtraKerningFactor (0.06f));
    g.drawText ("BANDS", 12, 6, 120, 24, juce::Justification::centredLeft, false);

    if (bands < 2)
        return;

    for (int band = 0; band < bands; ++band)
    {
        const auto x = cellX (band);
        const auto chip = juce::Rectangle<float> (x + 4.0f, 118.0f, 18.0f, 18.0f);

        g.setColour (bandColours[band]);
        g.drawRoundedRectangle (chip.reduced (0.75f), 3.0f, 1.5f);
        g.setColour (colours::textPrimary);
        g.setFont (font (Weight::semiBold, 11.0f));
        g.drawText (juce::String (band + 1), chip, juce::Justification::centred, false);

        g.setColour (colours::textSecondary);
        g.setFont (font (Weight::regular, 10.0f));
        g.drawText ("GR", juce::Rectangle<float> (x + 88.0f, 112.0f, 30.0f, 12.0f), juce::Justification::centred, false);

        const auto value = shown[(size_t) band];
        g.setColour (colours::textPrimary);
        g.setFont (font (Weight::regular, 12.0f));
        g.drawText (value >= 0.05f ? "-" + fancy::formatFixed (value, 1) : juce::String ("0.0"),
                    juce::Rectangle<float> (x + 84.0f, 126.0f, 38.0f, 14.0f), juce::Justification::centred, false);
    }
}

// AutoTargetButton (Phase 4 Wave 1) ----------------------------------------------

void LimitEditor::AutoTargetButton::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto enabled = isEnabled();

    g.setColour (progress >= 0.0f ? colours::bgSection : limitRed.withAlpha (enabled ? 1.0f : 0.4f));
    g.fillRoundedRectangle (bounds, 3.0f);

    if (progress >= 0.0f)
    {
        g.setColour (limitRed);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);

        // The ring, filling as the 10 s go by.
        const auto ring = bounds.withWidth (bounds.getHeight()).reduced (6.0f).translated (4.0f, 0.0f);
        juce::Path arc;
        arc.addCentredArc (ring.getCentreX(), ring.getCentreY(), ring.getWidth() * 0.5f, ring.getHeight() * 0.5f, 0.0f,
                           0.0f, juce::MathConstants<float>::twoPi * juce::jlimit (0.01f, 1.0f, progress), true);
        g.strokePath (arc, juce::PathStrokeType (2.0f));
    }

    g.setFont (font (Weight::semiBold, 12.0f));
    g.setColour (progress >= 0.0f ? colours::textPrimary : colours::bgWell);
    g.drawText (progress >= 0.0f ? "Listening..." : "Auto to target", bounds.withTrimmedLeft (progress >= 0.0f ? 18.0f : 0.0f),
                juce::Justification::centred, false);
}

void LimitEditor::AutoTargetButton::mouseDown (const juce::MouseEvent& event)
{
    if (! event.mods.isPopupMenu() && isEnabled() && onClick != nullptr)
        onClick();
}

// Counts the seconds with signal, and at 10 moves Threshold by target - measured, once, as one
// undo step. Lowering Threshold raises the loudness by about as much.
void LimitEditor::updateAutoTarget (float seconds)
{
    auto& processor = limitProcessor();
    const auto locked = processor.readTargets().gainLock;

    autoTargetButton.setEnabled (! locked);
    autoTargetButton.setAlpha (locked ? 0.4f : 1.0f);

    if (autoTargetButton.progress < 0.0f)
        return;

    if (locked)
    {
        autoTargetButton.progress = -1.0f;
        autoTargetButton.repaint();
        return;
    }

    if (processor.getDsp().loudness.getMomentary() > -70.0)
        autoTargetSeconds += seconds;

    autoTargetButton.progress = autoTargetSeconds / 10.0f;

    if (autoTargetSeconds >= 10.0f)
    {
        const auto measured = processor.getDsp().loudness.getIntegrated();

        if (measured > -70.0)
        {
            const auto threshold = processor.getApvts().getRawParameterValue ("threshold")->load();
            const auto moved = juce::jlimit (-30.0f, 0.0f, threshold - (processor.getTargetLufs() - (float) measured));
            processor.setParameter ("threshold", moved);
        }

        autoTargetButton.progress = -1.0f;
    }

    autoTargetButton.repaint();
}

LimitProcessor& LimitEditor::limitProcessor() noexcept
{
    return dynamic_cast<LimitProcessor&> (getFancyProcessor());
}

void LimitEditor::onTick (float seconds)
{
    auto& processor = limitProcessor();
    auto& dsp = processor.getDsp();

    const auto reduction = juce::jmin (dsp.takeReduction(), 60.0f);

    // Phase 4 Wave 1.
    gainLockCheckbox.setOn (processor.readTargets().gainLock);
    updateAutoTarget (seconds);

    // Phase 4 Wave 2: the Clip knob dims while the clipper is off.
    clipKnob.setAlpha (processor.readTargets().clip ? 1.0f : 0.4f);

    thresholdMeter.update (processor.inputMeter, false, seconds);
    gainMeter.update (reduction, reduction, seconds);
    outMeter.update (processor.outputMeter, seconds);

    const auto rms = juce::roundToInt (processor.getApvts().getRawParameterValue ("ballistics")->load()) == 1;
    loudnessBar.setLevel ((float) (rms ? dsp.loudness.getRmsDb() : dsp.loudness.getShortTerm()));
    loudnessBar.setTarget (processor.getTargetLufs());

    targetDropdown.refresh();
    display.update();
    bandsCard.update (reduction, seconds); // Phase 4 Wave 3
}

void LimitEditor::showPreview (bool drawerOpen)
{
    stopUpdates();

    setValue (parameter ("threshold"), -7.0f);
    setValue (parameter ("ceiling"), -1.0f);
    setValue (parameter ("release"), 80.0f);
    setValue (parameter ("lookahead"), 2.0f);
    setValue (parameter ("link"), 100.0f);

    // The mockup's history: hits every 46 px over a busy bed, held at the ceiling.
    std::vector<LimitDsp::HistoryColumn> columns;
    juce::Random random (3);
    const auto historyWidth = displayWidth - Display::panelWidth;

    for (int x = 0; x < historyWidth; ++x)
    {
        const auto t = x % 46;
        const auto inputDb = t < 3 ? 2.0f + 3.0f * random.nextFloat() : -4.0f - (float) t * 0.12f - 3.0f * random.nextFloat();
        const auto outputDb = juce::jmin (-1.0f, inputDb);
        columns.push_back ({ inputDb, outputDb, juce::jmax (0.0f, inputDb + 1.0f) });
    }

    display.showColumns (std::move (columns), { -13.8, -14.2, -1.0, 5.2 });

    // Phase 4 Wave 3: one band, its strip showing the limiter's reduction.
    setValue (parameter ("bands"), 0.0f);
    bandsCard.showValues ({ 3.1f, 0.0f, 0.0f, 0.0f });

    thresholdMeter.show (-4.0f, -5.0f, false);
    gainMeter.show (3.1f, 3.1f);
    outMeter.show (-1.0f, -1.0f);
    loudnessBar.setLevel (-13.8f);
    loudnessBar.setTarget (limitProcessor().getTargetLufs());
    targetDropdown.refresh();

    drawer.setVisible (drawerOpen);
}

void LimitEditor::showBandsPreview()
{
    showPreview (false);

    // Mockup 74, panel 1: three bands, crossovers at 150 Hz and 3 kHz, the bass band limiting most.
    setValue (parameter ("bands"), 2.0f);
    setValue (parameter (LimitProcessor::crossoverId (0)), 150.0f);
    setValue (parameter (LimitProcessor::crossoverId (1)), 3000.0f);
    bandsCard.showValues ({ 4.0f, 1.5f, 2.5f, 0.0f });
}

juce::Colour LimitEditor::getAccentColour()
{
    return limitRed;
}

std::vector<ShellEditor::PopupOpener> LimitEditor::getPopupOpeners()
{
    auto openers = ShellEditor::getPopupOpeners();
    openers.push_back ({ "target-menu", [this] { targetDropdown.showMenu(); } });
    openers.push_back ({ "target-custom", [this] { targetDropdown.askForCustom(); } });
    openers.push_back ({ "dropdown-oversampling", [this] { drawer.setVisible (true); oversamplingDropdown.click(); } });
    openers.push_back ({ "tooltip-lookahead", [this] { showTip (lookaheadKnob, lookaheadKnob.getTooltip()); } });
    openers.push_back ({ "value-release", [this] { releaseKnob.openValueEditor(); } });
    openers.push_back ({ "crossover-entry", [this]
                         {
                             setValue (parameter ("bands"), 2.0f);
                             bandsCard.strip.askForCrossover (1);
                         } });
    return openers;
}
