#include "LimitEditor.h"

#include <fancy/plugin/ParamFormat.h>

#include <cmath>

using namespace fancy::ui;

namespace
{
// design/phase3-mockup-src/limit.html at 100 %, with the right column widened to 120 px for
// Out's scale (docs/limit.md, decision 3).
constexpr int baseWidth = 760;
constexpr int bodyHeight = 392;
constexpr int baseHeight = ShellEditor::headerHeight + bodyHeight;
constexpr int rightColumnX = 640;

constexpr int leftX = 13, contentRight = 630;
constexpr int rowY = 10, rowHeight = 26;
constexpr int columnY = 44;
constexpr int displayX = 119, displayWidth = contentRight - displayX, displayHeight = 160;
constexpr int cardY = 212, cardHeight = 127;
constexpr int footerY = 362;

constexpr float labelHeight = 12.0f;
constexpr float displayRangeDb = 24.0f;

const juce::Colour limitRed = colours::uiRed;

// The meter's top, bottom and 0 dB line are shared by Out and the LUFS bar, so they line up.
constexpr float meterTop = 25.0f, meterBottom = 330.0f, meterZeroY = 40.0f;
constexpr float meterPixelsPerDb = (meterBottom - meterZeroY) / 54.0f;
constexpr OutMeter::Geometry outGeometry { 32.5f, meterTop, meterBottom, meterZeroY, meterPixelsPerDb, 338.0f, true, true };
constexpr LoudnessBar::Geometry loudnessGeometry { 14.0f, meterTop, meterBottom, meterZeroY, meterPixelsPerDb, 338.0f };

constexpr GainReductionMeter::Geometry gainGeometry { 13.0f, 20.0f, 246.0f, true, "GR" };

ThresholdMeter::Style thresholdStyle()
{
    ThresholdMeter::Style style;
    style.wellX = 12.0f;
    style.wellTop = 20.0f;
    style.wellHeight = 246.0f;
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
        juce::Path input, output, reductionPath;
        const auto bottom = bounds.getBottom();
        const auto count = (int) columns.size();

        input.startNewSubPath (0.0f, bottom);
        reductionPath.startNewSubPath (0.0f, 0.0f);

        for (int i = 0; i < count; ++i)
        {
            const auto& column = columns[(size_t) ((writeIndex + i) % count)];
            const auto x = (float) i;

            input.lineTo (x, yForDb (column.inputDb));
            reductionPath.lineTo (x, juce::jlimit (0.0f, bottom, column.reductionDb * ((float) getHeight() - 20.0f) / displayRangeDb));

            if (i == 0)
                output.startNewSubPath (x, yForDb (column.outputDb));
            else
                output.lineTo (x, yForDb (column.outputDb));
        }

        input.lineTo (historyWidth, bottom);
        input.closeSubPath();
        reductionPath.lineTo (historyWidth, 0.0f);
        reductionPath.closeSubPath();

        g.saveState();
        g.reduceClipRegion (juce::Rectangle<float> (0.0f, 0.0f, historyWidth, bounds.getHeight()).toNearestInt());

        g.setColour (colours::textSecondary.withAlpha (0.28f));
        g.fillPath (input);

        g.setColour (juce::Colour (0xffededed).withAlpha (0.8f));
        g.strokePath (output, juce::PathStrokeType (1.2f));

        g.setColour (limitRed.withAlpha (0.75f));
        g.fillPath (reductionPath);

        // The ceiling, dashed.
        const auto y = yForDb (ceilingDb);
        g.setColour (limitRed);

        for (auto x = 0.0f; x < historyWidth; x += 7.0f)
            g.fillRect (x, y - 0.5f, 4.0f, 1.0f);

        g.setColour (colours::textSecondary);
        g.setFont (font (Weight::regular, 10.0f));
        g.drawText (juce::String::fromUTF8 ("input (grey) \xc2\xb7 output (white) \xc2\xb7 reduction (red) \xc2\xb7 ceiling"),
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
      ballisticsDropdown (parameter ("ballistics"))
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
    thresholdMeter.setBounds (8, columnY, 66, 282);
    thresholdMeter.setTooltip ("Threshold: the signal is pushed up by this much, then held at Ceiling.");
    gainMeter.setBounds (78, columnY, 34, 290);
    gainMeter.setBarColour (limitRed);

    // The display and the Limiter card.
    display.setBounds (displayX, columnY, displayWidth, displayHeight);

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

    // Right column.
    outMeter.setBounds (rightColumnX + 2, 10, 74, 372);
    loudnessBar.setBounds (rightColumnX + 76, 10, 40, 372);
    loudnessBar.setTooltip ("Short-term loudness (3 s). The red line is the Target.");

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &styleLabel, &styleSegments, &targetLabel, &targetDropdown,
             &thresholdMeter, &gainMeter, &display,
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
    resetButton.onClick = [&processor] { processor.resetLoudness(); };

    body.addChildComponent (drawer);
    editLink.onClick = [this] { drawer.setVisible (! drawer.isVisible()); };
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

    thresholdMeter.update (processor.inputMeter, false, seconds);
    gainMeter.update (reduction, reduction, seconds);
    outMeter.update (processor.outputMeter, seconds);

    const auto rms = juce::roundToInt (processor.getApvts().getRawParameterValue ("ballistics")->load()) == 1;
    loudnessBar.setLevel ((float) (rms ? dsp.loudness.getRmsDb() : dsp.loudness.getShortTerm()));
    loudnessBar.setTarget (processor.getTargetLufs());

    targetDropdown.refresh();
    display.update();
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

    thresholdMeter.show (-4.0f, -5.0f, false);
    gainMeter.show (3.1f, 3.1f);
    outMeter.show (-1.0f, -1.0f);
    loudnessBar.setLevel (-13.8f);
    loudnessBar.setTarget (limitProcessor().getTargetLufs());
    targetDropdown.refresh();

    drawer.setVisible (drawerOpen);
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
    return openers;
}
