#include "ChorusEditor.h"

#include <fancy/plugin/ParamFormat.h>

#include <cmath>

using namespace fancy::ui;

namespace
{
// reference/phase4/51__chorus_mockup.png (design/phase4-mockup-src/chorus.html) at 100 %.
constexpr int baseWidth = 760;
constexpr int bodyHeight = 392;
constexpr int baseHeight = ShellEditor::headerHeight + bodyHeight;
constexpr int rightColumnX = 656;
constexpr int leftX = 13, contentRight = 646;
constexpr int rowY = 10, rowHeight = 26;
constexpr int displayY = 44, displayHeight = 150;
constexpr int cardY = 202, cardHeight = 137;
constexpr int motionCardX = leftX, motionCardWidth = 244;
constexpr int voicesCardX = motionCardX + motionCardWidth + 8, voicesCardWidth = 186;
constexpr int toneCardX = voicesCardX + voicesCardWidth + 8, toneCardWidth = contentRight - toneCardX;
constexpr int footerY = 362;
constexpr int faderTop = 26, faderHeight = 331;
constexpr float labelHeight = 12.0f;

// The voice map: the delay traces on the left, the stereo field on the right.
constexpr int traceLeft = 40, stereoWidth = 150;

const juce::Colour pink = colours::accentPink;

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

// Knobs spread evenly over a card; never wider than their share, so neighbours don't overlap.
void cardKnobs (std::initializer_list<Knob*> knobs, int x, int width, int y)
{
    constexpr int knobHeight = 92;
    const auto spacing = (float) width / (float) knobs.size();
    const auto knobWidth = juce::jmin (76, (int) spacing);
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

// The delay one voice has at a moment, as the DSP computes it (for the screenshot's preview).
float previewDelayMs (const ChorusDsp::Targets& t, int voice, double seconds)
{
    const auto phase = t.rateHz * seconds + t.spread * (double) voice / (double) t.voices;
    auto lfo = ChorusDsp::lfoValue (t.shape, phase, (std::uint32_t) (101 + voice));

    if (t.mode == ChorusDsp::ensemble)
        lfo += 0.07 * ChorusDsp::lfoValue (ChorusDsp::sine, 6.7 * phase + 0.37 * (double) voice, 0);

    return (float) (ChorusDsp::centreDelayMs (t.mode, t.delayMs) + ChorusDsp::sweepMs (t.mode, t.delayMs, t.depth) * lfo);
}
} // namespace

// Panels ----------------------------------------------------------------------

void ChorusEditor::Panels::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgBody);
    g.setColour (colours::bgSection);
    g.fillRect (rightColumnX, 0, baseWidth - rightColumnX, bodyHeight);

    for (const auto& [x, width] : { std::pair { motionCardX, motionCardWidth }, std::pair { voicesCardX, voicesCardWidth },
                                    std::pair { toneCardX, toneCardWidth } })
        g.fillRoundedRectangle ((float) x, (float) cardY, (float) width, (float) cardHeight, 6.0f);

    paintCardTitle (g, "MOTION", motionCardX, cardY);
    paintCardTitle (g, "VOICES", voicesCardX, cardY);
    paintCardTitle (g, "TONE", toneCardX, cardY);
}

// VoiceMap --------------------------------------------------------------------

ChorusEditor::VoiceMap::VoiceMap (ChorusProcessor& processor)
    : chorus (processor)
{
    setTooltip ("Each voice's delay over the last 2 s, brightest first, and on the right where each voice sits between left and right.");
}

int ChorusEditor::VoiceMap::traceWidth() const noexcept
{
    return juce::jmax (1, getWidth() - stereoWidth - traceLeft - 20);
}

void ChorusEditor::VoiceMap::resized()
{
    const auto width = traceWidth();
    ChorusDsp::HistoryColumn empty {};
    empty.voices = 0;
    columns.assign ((size_t) width, empty);
    incoming.resize ((size_t) width);
    writeIndex = 0;
    chorus.getDsp().setHistoryColumns (width);
}

void ChorusEditor::VoiceMap::update()
{
    if (frozen)
        return;

    const auto count = chorus.getDsp().popHistory (incoming.data(), (int) incoming.size());

    for (int i = 0; i < count && ! columns.empty(); ++i)
    {
        columns[(size_t) writeIndex] = incoming[(size_t) i];
        writeIndex = (writeIndex + 1) % (int) columns.size();
    }

    if (count > 0)
        repaint();
}

void ChorusEditor::VoiceMap::showColumns (std::vector<ChorusDsp::HistoryColumn> newColumns)
{
    frozen = true;
    columns = std::move (newColumns);
    writeIndex = 0;
    repaint();
}

void ChorusEditor::VoiceMap::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto targets = chorus.readTargets();
    const auto height = (float) getHeight(), width = (float) getWidth();
    const auto right = (float) (traceLeft + traceWidth());
    const auto longest = ChorusDsp::centreDelayMs (targets.mode, targets.delayMs) + ChorusDsp::sweepMs (targets.mode, targets.delayMs, targets.depth);
    const auto topMs = longest > 20.0 ? 40.0f : 20.0f;
    const auto plotTop = 22.0f, plotBottom = height - 22.0f;
    const auto yFor = [=] (float ms) { return plotBottom - (plotBottom - plotTop) * juce::jlimit (0.0f, 1.0f, ms / topMs); };

    // Grid: 0, half and the top.
    g.setFont (font (Weight::regular, 9.0f));

    for (const auto ms : { 0.0f, topMs * 0.5f, topMs })
    {
        const auto y = yFor (ms);
        g.setColour (ms == 0.0f ? colours::grey5 : colours::bgBody);
        g.fillRect ((float) traceLeft, y, right - (float) traceLeft, 1.0f);
        g.setColour (colours::textSecondary);
        g.drawText (juce::String (juce::roundToInt (ms)) + " ms", juce::Rectangle<float> (0.0f, y - 6.0f, (float) traceLeft - 6.0f, 12.0f),
                    juce::Justification::centredRight, false);
    }

    // The traces, the first voice brightest.
    const auto count = (int) columns.size();

    for (int v = ChorusDsp::maxVoices - 1; v >= 0; --v)
    {
        juce::Path path;
        auto drawing = false;

        for (int i = 0; i < count; ++i)
        {
            const auto& column = columns[(size_t) ((writeIndex + i) % count)];

            if (v >= column.voices)
            {
                drawing = false;
                continue;
            }

            const auto x = (float) traceLeft + (float) i;
            const auto y = yFor (column.delayMs[(size_t) v]);

            if (drawing)
                path.lineTo (x, y);
            else
                path.startNewSubPath (x, y);

            drawing = true;
        }

        const auto voices = juce::jmax (1, targets.voices);
        g.setColour (pink.withAlpha (1.0f - 0.5f * (float) v / (float) juce::jmax (1, voices - 1)));
        g.strokePath (path, juce::PathStrokeType (1.8f));
    }

    // The stereo field.
    const auto divider = right + 20.0f;
    g.setColour (colours::bgBody);
    g.fillRect (divider, 14.0f, 1.0f, height - 28.0f);

    const auto fieldLeft = divider + 14.0f, fieldRight = width - 12.0f;
    const auto centreY = height * 0.5f, centreX = (fieldLeft + fieldRight) * 0.5f, half = (fieldRight - fieldLeft) * 0.5f - 8.0f;

    g.setColour (colours::grey6);
    g.fillRect (fieldLeft, centreY, fieldRight - fieldLeft, 1.0f);

    g.setColour (colours::textSecondary);
    g.drawText ("L", juce::Rectangle<float> (fieldLeft, centreY + 8.0f, 12.0f, 12.0f), juce::Justification::centredLeft, false);
    g.drawText ("R", juce::Rectangle<float> (fieldRight - 12.0f, centreY + 8.0f, 12.0f, 12.0f), juce::Justification::centredRight, false);

    g.setFont (font (Weight::regular, 10.0f));
    g.drawText ("voices in stereo", juce::Rectangle<float> (fieldLeft, 10.0f, fieldRight - fieldLeft, 14.0f), juce::Justification::centredLeft, false);

    const auto voices = juce::jlimit (1, ChorusDsp::maxVoices, targets.voices);

    for (int v = voices - 1; v >= 0; --v)
    {
        const auto x = centreX + (float) (ChorusDsp::voicePosition (v, voices) * targets.width) * half;
        g.setColour (pink.withAlpha (1.0f - 0.5f * (float) v / (float) juce::jmax (1, voices - 1)));
        g.fillEllipse (x - 6.0f, centreY - 6.0f, 12.0f, 12.0f);
    }

    // The caption.
    const auto rate = targets.sync ? ChorusProcessor::noteName (juce::roundToInt (chorus.read ("syncRate"))) + " (synced)"
                                   : fancy::formatFixed ((float) targets.rateHz, 2) + " Hz";
    const auto caption = juce::String::fromUTF8 ("each voice's delay, last 2 s \xc2\xb7 ") + juce::String (voices)
                       + (voices == 1 ? " voice" : " voices") + juce::String::fromUTF8 (" \xc2\xb7 ") + rate;

    g.setColour (colours::textSecondary);
    g.drawText (caption, juce::Rectangle<float> (8.0f, 4.0f, right, 14.0f), juce::Justification::centredLeft, false);
}

// ChorusEditor ----------------------------------------------------------------

ChorusEditor::ChorusEditor (ChorusProcessor& processor)
    : ShellEditor (processor, baseWidth, baseHeight, HeaderStyle::full),
      modeSegments (parameter ("mode"), pink, colours::textPrimary),
      voiceMap (processor),
      rateKnob (parameter ("rate"), "Rate", pink, Knob::tightStyle),
      syncRateKnob (parameter ("syncRate"), "Rate", pink, Knob::tightStyle),
      depthKnob (parameter ("depth"), "Depth", pink, Knob::tightStyle),
      delayKnob (parameter ("delay"), "Delay", pink, Knob::tightStyle),
      feedbackKnob (parameter ("feedback"), "Feedback", pink, Knob::tightStyle, true),
      voicesKnob (parameter ("voices"), "Voices", pink, Knob::tightStyle),
      spreadKnob (parameter ("spread"), "Spread", pink, Knob::tightStyle),
      widthKnob (parameter ("width"), "Width", pink, Knob::tightStyle),
      lowCutKnob (parameter ("lowCut"), "Low cut", colours::uiBlue, Knob::tightStyle),
      highCutKnob (parameter ("highCut"), "High cut", colours::uiBlue, Knob::tightStyle),
      ageKnob (parameter ("age"), "Age", pink, Knob::tightStyle),
      syncCheckbox (parameter ("sync"), "Tempo sync", pink),
      bassMonoCheckbox (parameter ("bassMono"), "Keep bass mono", pink),
      listenCheckbox (getFancyProcessor(), "Listen to wet", pink),
      editLink (pink),
      outMeter (outGeometry),
      wetFader (parameter ("wet"), colours::uiTeal),
      dryFader (parameter ("dry"), colours::uiTeal),
      shapeDropdown (parameter ("lfoShape")),
      inputDropdown (parameter ("input")),
      lfoPhaseKnob (parameter ("lfoPhase"), "LFO phase", pink, Knob::smallStyle),
      duckingKnob (parameter ("ducking"), "Ducking", pink, Knob::smallStyle),
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
    setUpLabel (modeLabel, "Mode", juce::Justification::centredLeft);
    modeLabel.setBounds (leftX + 2, rowY, 40, rowHeight);
    modeSegments.setBounds (60, rowY + 1, 310, 24);
    modeSegments.setTextHeight (labelHeight);
    modeSegments.setFitToText (true);
    modeSegments.setTooltip ("Classic: one LFO, the voices offset evenly. Ensemble: a second, faster LFO, so the voices shimmer. "
                             "Dimension: half the depth, left and right moving opposite ways. Doubler: smooth random drift, "
                             "like a second take. Flange: short delays for jet sweeps with Feedback.");

    voiceMap.setBounds (leftX, displayY, contentRight - leftX, displayHeight);

    // Cards.
    cardKnobs ({ &rateKnob, &depthKnob, &delayKnob, &feedbackKnob }, motionCardX, motionCardWidth, cardY + 30);
    syncRateKnob.setBounds (rateKnob.getBounds());
    cardKnobs ({ &voicesKnob, &spreadKnob, &widthKnob }, voicesCardX, voicesCardWidth, cardY + 30);
    cardKnobs ({ &lowCutKnob, &highCutKnob, &ageKnob }, toneCardX, toneCardWidth, cardY + 30);

    rateKnob.setTooltip ("How fast the voices move. Slow for width, fast for vibrato.");
    syncRateKnob.setTooltip ("One LFO cycle as a note length at the host's tempo.");
    depthKnob.setTooltip ("How far each voice's delay moves, so how much its pitch wobbles.");
    delayKnob.setTooltip ("Each voice's average delay. In Flange mode the same knob covers 0.3 to 10 ms.");
    feedbackKnob.setTooltip ("Sends the voices back into themselves: positive and negative give different combs. "
                             "Strong in Flange mode; turn Output down at high settings.");
    voicesKnob.setTooltip ("How many copies of the sound, 1 to 6.");
    spreadKnob.setTooltip ("How far apart the voices' LFOs are. 0 moves them together; more makes them move in turn.");
    widthKnob.setTooltip ("How far apart the voices sit between left and right. 0 is mono.");
    lowCutKnob.setTooltip ("Takes lows out of the chorus, so the bass stays clean. Off at the bottom. Keep bass mono uses this frequency.");
    highCutKnob.setTooltip ("Takes highs out of the chorus, for a softer, darker sound. Off at the top.");
    ageKnob.setTooltip ("An old bucket-brigade chorus's character: darker, a little saturated, with faint hiss.");

    feedbackKnob.setShowDefaultMark (true);

    // Footer.
    syncCheckbox.setBounds (leftX, footerY, 110, 20);
    bassMonoCheckbox.setBounds (leftX + 120, footerY, 130, 20);
    listenCheckbox.setBounds (leftX + 262, footerY, 130, 20);
    editLink.setBounds (contentRight - 64, footerY - 2, 64, 24);

    for (auto* checkbox : { &syncCheckbox, &bassMonoCheckbox })
        checkbox->setLabelHeight (labelHeight);

    listenCheckbox.setLabelHeight (labelHeight);

    syncCheckbox.setTooltip ("Locks the LFO to the host's tempo, so the motion lands on the beat. Rate becomes a note length.");
    bassMonoCheckbox.setTooltip ("Keeps the chorus's lows in the centre, below the Low cut frequency, so the low end stays solid in mono.");
    listenCheckbox.setTooltip ("Plays only the chorus voices, without the dry signal. Turns off when you close this window, and is never saved.");

    // Right column, as TS's.
    outMeter.setBounds (rightColumnX - 6, 10, 46, 350);
    setUpLabel (wetLabel, "Wet", juce::Justification::centred, 14.0f, colours::textPrimary);
    setUpLabel (dryLabel, "Dry", juce::Justification::centred, 14.0f, colours::textPrimary);
    wetLabel.setBounds (690, 10, 33, 17);
    dryLabel.setBounds (723, 10, 33, 17);
    wetFader.setBounds (690, faderTop, 33, faderHeight);
    dryFader.setBounds (723, faderTop, 33, faderHeight);
    wetFader.setTooltip ("The chorus voices' level.");
    dryFader.setTooltip ("The untouched signal's level.");

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &modeLabel, &modeSegments, &voiceMap,
             &rateKnob, &depthKnob, &delayKnob, &feedbackKnob, &voicesKnob, &spreadKnob, &widthKnob,
             &lowCutKnob, &highCutKnob, &ageKnob,
             &syncCheckbox, &bassMonoCheckbox, &listenCheckbox, &editLink,
             &outMeter, &wetLabel, &dryLabel, &wetFader, &dryFader })
        body.addAndMakeVisible (item);

    body.addChildComponent (syncRateKnob);

    // EDIT drawer (decisions 1 and 2 A: no polarity dropdown, no oversampling).
    drawer.setBounds (0, displayY - 4, rightColumnX, bodyHeight - displayY + 4);

    const std::pair<juce::Label*, const char*> rows[] { { &shapeLabel, "LFO shape" }, { &inputLabel, "Input" } };
    juce::Component* controls[] { &shapeDropdown, &inputDropdown };

    for (int i = 0; i < 2; ++i)
    {
        setUpLabel (*rows[i].first, rows[i].second, juce::Justification::centredLeft, 14.0f, colours::textPrimary);
        rows[i].first->setBounds (24, 60 + 40 * i, 150, 24);
        controls[i]->setBounds (170, 60 + 40 * i, 150, 24);
        drawer.addAndMakeVisible (rows[i].first);
        drawer.addAndMakeVisible (controls[i]);
    }

    lfoPhaseKnob.setBounds (400, 56, 76, 99);
    duckingKnob.setBounds (480, 56, 76, 99);
    outputKnob.setBounds (560, 56, 76, 99);

    setUpLabel (drawerNote, juce::String::fromUTF8 ("LFO shape: Sine, Triangle, Smooth random (Doubler always drifts at random). "
                                                    "Input: Stereo, or Mono (sums the input, for a mono source made wide). "
                                                    "LFO phase: the right channel's LFO offset, 0 to 180\xc2\xb0. "
                                                    "Ducking turns the chorus down while the input is loud. Bass mono's frequency is the Low cut knob's."),
                juce::Justification::topLeft, 11.0f);
    drawerNote.setBounds (24, 170, 560, 64);

    shapeDropdown.setTooltip ("The shape of the voices' motion. Smooth random wanders instead of repeating.");
    inputDropdown.setTooltip ("Stereo keeps the input's stereo; Mono sums it first, for a mono source made wide.");
    lfoPhaseKnob.setTooltip (juce::String::fromUTF8 ("How far the right channel's motion is behind the left's. 0\xc2\xb0 moves them together; "
                                                     "more makes the chorus wider."));
    duckingKnob.setTooltip ("Turns the chorus down by up to 24 dB while the input is loud, so it fills the gaps instead.");

    for (juce::Component* item : std::initializer_list<juce::Component*> { &lfoPhaseKnob, &duckingKnob, &outputKnob, &drawerNote })
        drawer.addAndMakeVisible (item);

    body.addChildComponent (drawer);
    editLink.onClick = [this] { drawer.setVisible (! drawer.isVisible()); };

    updateLocks();
}

ChorusEditor::~ChorusEditor() = default;

ChorusProcessor& ChorusEditor::chorusProcessor() noexcept
{
    return dynamic_cast<ChorusProcessor&> (getFancyProcessor());
}

void ChorusEditor::updateLocks()
{
    const auto targets = chorusProcessor().readTargets();

    // Tempo sync swaps Rate for its note length.
    rateKnob.setVisible (! targets.sync);
    syncRateKnob.setVisible (targets.sync);

    // In Flange mode the Delay knob's 1 to 30 ms covers 0.3 to 10 ms; it shows the delay in use.
    if (targets.mode == ChorusDsp::flange)
    {
        const auto ms = (float) ChorusDsp::flangeDelayMs (targets.delayMs);
        delayKnob.setValueTextOverride (fancy::formatFixed (ms, ms < 1.0f ? 2 : 1) + " ms");
    }
    else
    {
        delayKnob.setValueTextOverride ({});
    }
}

void ChorusEditor::onTick (float seconds)
{
    updateLocks();
    voiceMap.update();
    outMeter.update (chorusProcessor().outputMeter, seconds);
}

void ChorusEditor::showPreview (bool drawerOpen)
{
    stopUpdates();

    // The mockup's values are the defaults.
    const auto targets = chorusProcessor().readTargets();
    std::vector<ChorusDsp::HistoryColumn> columns;
    const auto width = contentRight - leftX - stereoWidth - traceLeft - 20;

    for (int x = 0; x < width; ++x)
    {
        ChorusDsp::HistoryColumn column {};
        column.voices = targets.voices;

        for (int v = 0; v < targets.voices; ++v)
            column.delayMs[(size_t) v] = previewDelayMs (targets, v, ChorusDsp::historySeconds * (double) x / (double) width);

        columns.push_back (column);
    }

    voiceMap.showColumns (std::move (columns));
    outMeter.show (-8.2f, -8.0f);
    updateLocks();

    drawer.setVisible (drawerOpen);
}

juce::Colour ChorusEditor::getAccentColour()
{
    return pink;
}

std::vector<ShellEditor::PopupOpener> ChorusEditor::getPopupOpeners()
{
    auto openers = ShellEditor::getPopupOpeners();
    openers.push_back ({ "dropdown-shape", [this] { drawer.setVisible (true); shapeDropdown.click(); } });
    openers.push_back ({ "tooltip-listen", [this] { showTip (listenCheckbox, listenCheckbox.getTooltip()); } });
    openers.push_back ({ "value-rate", [this] { rateKnob.openValueEditor(); } });
    return openers;
}
