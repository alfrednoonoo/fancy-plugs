#include "FirEditor.h"

#include <fancy/plugin/ParamFormat.h>

using namespace fancy::ui;
using namespace fir;

namespace
{
// Phase 4 Wave 1: Resonance's Tilt, a small bipolar knob, 66 tall with its label and value.
constexpr fancy::ui::Knob::Style tiltKnobStyle { 32.0f, 27.0f, 3.0f, 9.0f, 7.0f, 13.0f, 3.0f, 2.0f, 16.0f, 11.0f, false, true };

// design/phase2-mockup-src/fir.html at 100 %. The shared header is 16 px taller than the
// mockup's, so the window is too. Phase 4 Wave 2 (mockup 59): 70 px wider for the Mid | Side
// switch; the graph and the controls card take the width, and what's right of them moves over.
constexpr int grow = 70;
constexpr int baseWidth = 760 + grow;
constexpr int bodyHeight = 392;
constexpr int baseHeight = ShellEditor::headerHeight + bodyHeight;
constexpr int rightColumnX = 660 + grow;

constexpr int graphX = 13, graphY = 43, graphWidth = 610 + grow, graphHeight = 196;
constexpr int cardY = 248, cardHeight = 111;
constexpr int leftCardX = 13, leftCardWidth = 178;
constexpr int controlsCardX = 199, controlsCardWidth = 450 + grow;
constexpr int footerY = 368;

constexpr float labelHeight = 12.0f;

constexpr OutMeter::Geometry outGeometry { 15.0f, 16.0f, 316.0f, 26.0f, 4.9f, 324.0f, false, true };

void setValue (juce::RangedAudioParameter& parameter, float value)
{
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
}

void styleLabel (juce::Label& label, const juce::String& text, juce::Colour colour, juce::Justification justification, float height = labelHeight)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (font (Weight::regular, height));
    label.setColour (juce::Label::textColourId, colour);
    label.setJustificationType (justification);
    label.setBorderSize ({});
    label.setInterceptsMouseClicks (false, false);
}
} // namespace

// Panels ----------------------------------------------------------------------

void FirEditor::Panels::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgBody);

    g.setColour (colours::bgSection);
    g.fillRect (rightColumnX, 0, baseWidth - rightColumnX, bodyHeight);
    g.fillRoundedRectangle ((float) leftCardX, (float) cardY, (float) leftCardWidth, (float) cardHeight, 6.0f);
    g.fillRoundedRectangle ((float) controlsCardX, (float) cardY, (float) controlsCardWidth, (float) cardHeight, 6.0f);
}

// LearnCard ---------------------------------------------------------------------

FirEditor::LearnCard::LearnCard (FirProcessor& processor)
    : firProcessor (processor),
      sourceSwitch (*processor.getApvts().getParameter ("source"), modeColour (subtract), colours::textPrimary),
      adaptKnob (*processor.getApvts().getParameter ("adaptSpeed"), {}, modeColour (subtract), Knob::compactStyle)
{
    // Phase 2.1 A: the switch takes the title's row, which the card is too narrow to share.
    sourceSwitch.setTextHeight (11.0f);
    sourceSwitch.setBounds (8, 4, 162, 20);
    sourceSwitch.setTooltip ("Noise profile: learned once, tracked while playing (Adaptive), or the larger of the two (Both).");
    addAndMakeVisible (sourceSwitch);

    adaptKnob.setBounds (6, 34, 66, 76);
    adaptKnob.setTooltip ("Adapt speed: how quickly the estimate follows a change in the noise.");
    addChildComponent (adaptKnob);

    bothLearnButton.setBounds (78, 82, 92, 22);
    bothLearnButton.onClick = [this] { firProcessor.startLearning(); };
    addChildComponent (bothLearnButton);

    clearButton.onClick = [this]
    {
        auto& owner = firProcessor;
        const auto clear = [&owner] { owner.clearProfile(); };

        if (auto* editor = findParentComponentOfClass<ShellEditor>())
            editor->confirmThen ("Clear the noise profile?", "The learned profile will be removed. You can learn it again from a section with only noise.",
                                 "Clear", true, clear);
        else
            clear();
    };
    relearnButton.onClick = [this] { firProcessor.startLearning(); };

    addChildComponent (clearButton);
    addChildComponent (relearnButton);
}

juce::Rectangle<int> FirEditor::LearnCard::learnButtonBounds() const
{
    return { 12, 26, getWidth() - 24, 28 };
}

void FirEditor::LearnCard::update()
{
    auto& shared = firProcessor.getShared();
    const auto rate = firProcessor.getSampleRate() > 0.0 ? firProcessor.getSampleRate() : 48000.0;

    learning = firProcessor.isLearning();
    hasProfile = shared.hasProfile.load();
    progress = firProcessor.getLearnProgress();
    totalSeconds = (float) ((double) shared.learnTarget.load() / rate);
    seconds = progress * totalSeconds;
    capturedSeconds = shared.profileSeconds.load();

    if (! frozen)
    {
        source = juce::jlimit (0, 2, juce::roundToInt (firProcessor.getApvts().getRawParameterValue ("source")->load()));
        tracking = shared.hasAdaptive.load();
        floorDb = shared.adaptiveFloorDb.load() + firProcessor.getApvts().getRawParameterValue ("offset")->load();
        held = shared.heldFraction.load() > 0.02f;
    }

    const auto learned = source == fir::learnedSource;
    const auto showButtons = learned && hasProfile && ! learning;

    clearButton.setBounds (12, 58, 70, 24);
    relearnButton.setBounds (90, 58, getWidth() - 102, 24);
    clearButton.setVisible (showButtons);
    relearnButton.setVisible (showButtons);

    adaptKnob.setVisible (! learned);
    bothLearnButton.setVisible (source == fir::bothSources);
    bothLearnButton.setText (learning ? "Learning " + fancy::formatFixed (seconds, 1) + " s" : hasProfile ? juce::String ("Re-learn") : juce::String ("Learn"));

    repaint();
}

void FirEditor::LearnCard::showTracking (float shownFloorDb, bool isHeld)
{
    frozen = true;
    source = fir::adaptiveSource;
    tracking = true;
    floorDb = shownFloorDb;
    held = isHeld;
    learning = false;

    clearButton.setVisible (false);
    relearnButton.setVisible (false);
    adaptKnob.setVisible (true);
    bothLearnButton.setVisible (false);
    repaint();
}

void FirEditor::LearnCard::paintTracking (juce::Graphics& g, int x, bool compact)
{
    const auto width = getWidth() - x - 6;
    const auto approx = juce::String (juce::CharPointer_UTF8 (" \xe2\x89\x88 "));
    const auto minus = juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92"));

    g.setColour (tracking ? juce::Colour (0xff0dfc90) : colours::textSecondary);
    g.fillEllipse ((float) x, 37.0f, 7.0f, 7.0f);

    g.setFont (font (Weight::regular, 12.0f));
    g.setColour (colours::textPrimary);
    g.drawText (tracking ? (compact && held ? "Holding" : "Tracking") : juce::String ("Starting") + juce::String::charToString (0x2026),
                x + 11, 32, width - 11, 16, juce::Justification::centredLeft, false);

    g.setFont (font (Weight::regular, 10.5f));
    g.setColour (colours::textSecondary);

    if (tracking)
    {
        const auto rounded = juce::roundToInt (floorDb);
        // "Floor", not "Noise floor": the card is too narrow for the dB after the longer label.
        g.drawText (juce::String ("Floor") + approx + (rounded < 0 ? minus : juce::String()) + juce::String (std::abs (rounded)) + " dB",
                    x, 50, width, 14, juce::Justification::centredLeft, false);
    }

    if (held && ! compact)
        g.drawFittedText ("Speech/music held, not learned", x, 66, width, 28, juce::Justification::topLeft, 2, 0.9f);
}

void FirEditor::LearnCard::showLearning (float shownSeconds, float total)
{
    learning = true;
    hasProfile = false;
    seconds = shownSeconds;
    totalSeconds = total;
    progress = shownSeconds / total;

    clearButton.setVisible (false);
    relearnButton.setVisible (false);
    repaint();
}

void FirEditor::LearnCard::paint (juce::Graphics& g)
{
    const auto red = modeColour (subtract);

    // Adaptive and Both: Adapt speed, and the tracker's status (Phase 2.1 A).
    if (source != fir::learnedSource)
    {
        g.setColour (colours::textSecondary);
        g.setFont (font (Weight::regular, 10.5f));
        g.drawText ("Adapt speed", 6, 24, 66, 12, juce::Justification::centred, false);

        paintTracking (g, 78, source == fir::bothSources);
        return;
    }

    g.setFont (font (Weight::regular, labelHeight));

    if (hasProfile && ! learning)
    {
        g.setColour (colours::textPrimary);
        g.drawText ("Profile: " + fancy::formatFixed (capturedSeconds, 1) + " s captured", 12, 28, getWidth() - 24, 22,
                    juce::Justification::centredLeft, false);

        g.setColour (colours::textSecondary);
        g.drawFittedText ("Clear it, or learn again from a section with only noise.", 12, 84, getWidth() - 24, 24, juce::Justification::topLeft, 2);
        return;
    }

    const auto button = learnButtonBounds().toFloat();

    g.setColour (red);
    g.fillRoundedRectangle (button, 5.0f);

    g.setColour (colours::textPrimary);
    g.fillEllipse (button.getX() + 10.0f, button.getCentreY() - 4.0f, 8.0f, 8.0f);

    g.setFont (font (Weight::semiBold, 13.0f));

    const auto text = learning ? "Learning" + juce::String::charToString (0x2026) + " " + fancy::formatFixed (seconds, 1) + " / " + fancy::formatFixed (totalSeconds, 1) + " s"
                               : juce::String ("Learn");

    g.drawText (text, button.withTrimmedLeft (26.0f), juce::Justification::centredLeft, false);

    const auto bar = juce::Rectangle<float> (button.getX(), button.getBottom() + 8.0f, button.getWidth(), 4.0f);

    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (bar, 2.0f);
    g.setColour (red);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * progress), 2.0f);

    g.setFont (font (Weight::regular, labelHeight));
    g.setColour (colours::textSecondary);
    g.drawFittedText ("Play a section with only noise.\nLearning stops by itself.", 12, 68, getWidth() - 20, 34, juce::Justification::topLeft, 2, 0.85f);
}

void FirEditor::LearnCard::mouseUp (const juce::MouseEvent& event)
{
    if (source == fir::learnedSource && ! (hasProfile && ! learning) && ! learning && learnButtonBounds().contains (event.getPosition()))
        firProcessor.startLearning();
}

// FirEditor ----------------------------------------------------------------------

FirEditor::FirEditor (FirProcessor& processor)
    : ShellEditor (processor, baseWidth, baseHeight, HeaderStyle::full),
      modeSwitch (parameter ("mode"), modeColour (eq), colours::textPrimary),
      editDropdown (parameter ("edit")),
      graph (processor),
      learnCard (processor),
      floorKnob (parameter ("gateFloor"), "Floor", modeColour (gate), Knob::tightStyle),
      gateAttackKnob (parameter ("gateAttack"), "Attack", modeColour (gate), Knob::tightStyle),
      gateReleaseKnob (parameter ("gateRelease"), "Release", modeColour (gate), Knob::tightStyle),
      ratioKnob (parameter ("compRatio"), "Ratio", modeColour (compressor), Knob::tightStyle),
      compAttackKnob (parameter ("compAttack"), "Attack", modeColour (compressor), Knob::tightStyle),
      compReleaseKnob (parameter ("compRelease"), "Release", modeColour (compressor), Knob::tightStyle),
      reductionKnob (parameter ("reduction"), "Reduction", modeColour (subtract), Knob::tightStyle),
      offsetKnob (parameter ("offset"), "Offset", modeColour (subtract), Knob::tightStyle, true),
      smoothingKnob (parameter ("smoothing"), "Smoothing", modeColour (subtract), Knob::tightStyle),
      subReleaseKnob (parameter ("subRelease"), "Release", modeColour (subtract), Knob::tightStyle),
      resLowKnob (parameter ("resLow"), {}, modeColour (resonance), Knob::compactStyle),
      resHighKnob (parameter ("resHigh"), {}, modeColour (resonance), Knob::compactStyle),
      resDepthKnob (parameter ("resDepth"), "Depth", modeColour (resonance), Knob::tightStyle),
      resSharpnessKnob (parameter ("resSharpness"), "Sharpness", modeColour (resonance), Knob::tightStyle),
      resTiltKnob (parameter ("resTilt"), "Tilt", modeColour (resonance), tiltKnobStyle, true),
      resAttackKnob (parameter ("resAttack"), "Attack", modeColour (resonance), Knob::tightStyle),
      resReleaseKnob (parameter ("resRelease"), "Release", modeColour (resonance), Knob::tightStyle),
      fftDropdown (parameter ("fft")),
      reduceCheckbox (parameter ("reduce"), "Reduce artifacts"),
      analysisCheckbox (parameter ("showAnalysis"), "Show analysis"),
      lowLatencyCheckbox (parameter ("lowLatency"), "Low latency"),
      sidechainCheckbox (parameter ("sidechain"), "Sidechain detector"),
      listenNoiseCheckbox (getFancyProcessor(), "Listen to removed noise"),
      listenReducedCheckbox (getFancyProcessor(), "Listen to gain-reduced part"),
      listenResonanceCheckbox (getFancyProcessor(), "Listen to removed resonances"),
      editLink (colours::uiTeal),
      outMeter (outGeometry),
      gainFader (parameter ("gain"), colours::uiTeal),
      learnLengthKnob (parameter ("learnLength"), "Learn length", modeColour (subtract), Knob::smallStyle),
      drawerOutputKnob (parameter ("output"), "Output", colours::uiTeal, Knob::smallStyle),
      midSideCheckbox (parameter ("midSide"), "Mid/Side")
{
    // The title strip keeps one colour while the accent follows the selection.
    setTitleColour (modeColour (eq));

    auto& body = getBody();

    panels.setInterceptsMouseClicks (false, false);
    panels.setBounds (0, 0, baseWidth, bodyHeight);
    body.addAndMakeVisible (panels);

    // Phase 2.1 W.1: room in the header for undo, redo, A/B and Compare.
    setShortHeaderLabels (true);
    setPresetDropdownWidth (120);

    styleLabel (latencyLabel, {}, colours::textSecondary, juce::Justification::centredRight);
    latencyLabel.setBounds (getHeaderFreeLeft(), 13, getHeaderFreeRight() - getHeaderFreeLeft(), 24);
    getRoot().addAndMakeVisible (latencyLabel);

    styleLabel (modeLabel, "Mode", colours::textSecondary, juce::Justification::centredLeft);
    styleLabel (editLabel, "Edit", colours::textSecondary, juce::Justification::centredRight);

    modeLabel.setBounds (13, 10, 36, 25);
    modeSwitch.setBounds (50, 10, 400, 25);
    modeSwitch.setTextHeight (labelHeight);
    modeSwitch.setFitToText (true);
    editLabel.setBounds (484 + grow, 10, 40, 25);

    // Phase 4 Wave 2: Mid | Side, right after the Mode switch, while Mid/Side is on.
    midSideSwitch.setBounds (456, 10, 66, 25);
    midSideSwitch.setColours (colours::uiTeal, colours::uiTeal);
    midSideSwitch.setTextHeight (labelHeight);
    midSideSwitch.onChange = [this] (int index)
    {
        firProcessor().setViewSide (index == 1);
        graph.update();
    };
    midSideSwitch.setTooltip ("Which domain's curve you see and edit. Each has its own.");
    getBody().addChildComponent (midSideSwitch);

    // Phase 4 Wave 1: Learn between the Mode switch and Edit, in Gate and Compressor modes.
    curveLearnButton.setBounds (455 + grow, 10, 48, 25);
    curveLearnButton.onClick = [this]
    {
        if (graph.isLearningCurve())
            graph.cancelLearn();
        else
            graph.startLearn (graph.thresholdCurve());
    };
    getBody().addChildComponent (curveLearnButton);
    editDropdown.setBounds (530 + grow, 10, 119, 25);
    editDropdown.setTextHeight (labelHeight);

    graph.setBounds (graphX, graphY, graphWidth, graphHeight);

    learnCard.setBounds (leftCardX, cardY, leftCardWidth, cardHeight);

    styleLabel (curveCaption, {}, colours::textSecondary, juce::Justification::topLeft);
    curveCaption.setBounds (leftCardX + 12, cardY + 8, leftCardWidth - 24, 16);
    resetButton.setBounds (leftCardX + 12, cardY + 30, leftCardWidth - 24, 24);
    flatButton.setBounds (leftCardX + 12, cardY + 62, 42, 24);
    tiltButton.setBounds (leftCardX + 60, cardY + 62, 40, 24);
    demudButton.setBounds (leftCardX + 106, cardY + 62, 60, 24);

    styleLabel (convolveText, "Convolves the left input with the right one, which acts as the impulse response. Route the sound to channel 1 and the response to channel 2.",
                colours::textSecondary, juce::Justification::topLeft);
    convolveText.setBounds (leftCardX + 12, cardY + 10, leftCardWidth - 24, cardHeight - 20);
    convolveText.setMinimumHorizontalScale (1.0f);

    resetButton.onClick = [this] { firProcessor().resetCurve (graph.thresholdCurve()); };

    // Phase 4 Wave 2: the ratio curve back to flat at the Ratio knob's value.
    resetRatioButton.setBounds (leftCardX + 12, cardY + 64, leftCardWidth - 24, 24);
    resetRatioButton.onClick = [this] { firProcessor().resetCurve (firProcessor().shownCurve (fir::ratioCurve)); };

    styleLabel (ratioHint, "Ratio moves the whole ratio curve.", colours::textSecondary, juce::Justification::topLeft, 11.0f);
    ratioHint.setBounds (410, cardY + 22, 124, 30);
    ratioHint.setMinimumHorizontalScale (1.0f);
    flatButton.onClick = [this] { firProcessor().resetCurve (firProcessor().shownCurve (eq)); };
    tiltButton.onClick = [this] { firProcessor().setPoints (firProcessor().shownCurve (eq), { { 20.0, -3.0 }, { 20000.0, 3.0 } }); };
    demudButton.onClick = [this] { firProcessor().setPoints (firProcessor().shownCurve (eq), { { 100.0, 0.0 }, { 250.0, -4.0 }, { 600.0, 0.0 } }); };

    constexpr int knobWidth = 70, knobHeight = 99, knobTop = cardY + 8, firstCentre = 239, spacing = 64;

    const auto placeKnob = [&] (Knob& knob, int index)
    {
        knob.setBounds (firstCentre + index * spacing - knobWidth / 2, knobTop, knobWidth, knobHeight);
    };

    placeKnob (floorKnob, 0);
    placeKnob (gateAttackKnob, 1);
    placeKnob (gateReleaseKnob, 2);
    placeKnob (ratioKnob, 0);
    placeKnob (compAttackKnob, 1);
    placeKnob (compReleaseKnob, 2);
    placeKnob (reductionKnob, 0);
    placeKnob (offsetKnob, 1);
    placeKnob (smoothingKnob, 2);
    placeKnob (subReleaseKnob, 3);
    placeKnob (resDepthKnob, 0);
    placeKnob (resSharpnessKnob, 1);
    placeKnob (resAttackKnob, 2);
    placeKnob (resReleaseKnob, 3);

    // Phase 3 Q2.7: RANGE, with Low and High as compact knobs under their own labels.
    styleLabel (rangeCaption, "RANGE", colours::textSecondary, juce::Justification::topLeft);
    rangeCaption.setBounds (leftCardX + 12, cardY + 8, 80, 16);

    // Phase 4 Wave 2 (reference/phase4/42__fir_w2_mockup.png): back to a flat depth curve.
    resetDepthButton.setBounds (leftCardX + 60, cardY + 5, 88, 20);
    resetDepthButton.onClick = [this] { firProcessor().resetCurve (firProcessor().shownCurve (fir::depthCurve)); };
    styleLabel (rangeLowLabel, "Low", colours::textPrimary, juce::Justification::centred);
    styleLabel (rangeHighLabel, "High", colours::textPrimary, juce::Justification::centred);

    constexpr int rangeLowCentre = leftCardX + 45, rangeHighCentre = leftCardX + 125;

    rangeLowLabel.setBounds (rangeLowCentre - 33, cardY + 24, 66, 16);
    rangeHighLabel.setBounds (rangeHighCentre - 33, cardY + 24, 66, 16);
    resLowKnob.setBounds (rangeLowCentre - 33, cardY + 39, 66, 72);
    resHighKnob.setBounds (rangeHighCentre - 33, cardY + 39, 66, 72);
    resLowKnob.setTooltip ("Resonance works between Low and High, fading out over half an octave outside.");
    resHighKnob.setTooltip (resLowKnob.getTooltip());
    resSharpnessKnob.setTooltip ("Higher finds only narrower peaks; lower also takes broader bumps.");
    resDepthKnob.setTooltip ("The most a resonance is turned down. A peak never goes below 4 dB over the spectrum around it.");

    styleLabel (eqHint, "The curve is the EQ. Click it to add a point.", colours::textSecondary, juce::Justification::centredLeft);
    eqHint.setBounds (controlsCardX + 14, cardY + 40, 250, 20);

    styleLabel (fftLabel, "FFT size", colours::textSecondary, juce::Justification::centredLeft);
    fftLabel.setBounds (471 + grow, cardY + 12, 100, 16);
    fftDropdown.setBounds (471 + grow, cardY + 30, 100, 24);
    fftDropdown.setTextHeight (labelHeight);
    reduceCheckbox.setBounds (471 + grow, cardY + 64, 160, 20);

    // Phase 4 Wave 1: Tilt beside FFT size.
    resTiltKnob.setBounds (584 + grow, cardY, 60, 66);
    resTiltKnob.setTooltip ("Leans Depth towards the highs (positive) or the lows (negative): at +100 %, twice the depth at 16 kHz "
                            "and half at 62 Hz.");
    reduceCheckbox.setLabelHeight (labelHeight);

    analysisCheckbox.setBounds (15, footerY, 106, 20);
    lowLatencyCheckbox.setBounds (127, footerY, 92, 20);
    sidechainCheckbox.setBounds (225, footerY, 134, 20);
    listenNoiseCheckbox.setBounds (365, footerY, 180, 20);
    listenReducedCheckbox.setBounds (365, footerY, 200, 20);
    listenResonanceCheckbox.setBounds (365, footerY, 210, 20);
    editLink.setBounds (586 + grow, footerY - 2, 64, 24);

    for (auto* checkbox : { &analysisCheckbox, &lowLatencyCheckbox, &sidechainCheckbox })
        checkbox->setLabelHeight (labelHeight);

    for (auto* checkbox : { &listenNoiseCheckbox, &listenReducedCheckbox, &listenResonanceCheckbox })
    {
        checkbox->setLabelHeight (labelHeight);
        checkbox->setTooltip ("Plays only what's being taken away. Turns off when you close this window, and is never saved.");
    }

    lowLatencyCheckbox.setTooltip ("Halves the latency, at the same frequency resolution, with a little more smearing.");
    ratioKnob.setTooltip ("Below 1:1 the level above the threshold is raised: upward expansion.");

    outMeter.setBounds (668 + grow, 10, 46, 350);
    styleLabel (gainLabel, "Gain", colours::textSecondary, juce::Justification::centred, 14.0f);
    gainLabel.setBounds (709 + grow, 10, 33, 17);
    gainFader.setBounds (709 + grow, 26, 33, 331);

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &modeLabel, &modeSwitch, &editLabel, &editDropdown, &graph,
             &fftLabel, &fftDropdown, &reduceCheckbox,
             &analysisCheckbox, &lowLatencyCheckbox, &editLink, &outMeter, &gainLabel, &gainFader })
        body.addAndMakeVisible (item);

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &learnCard, &curveCaption, &resetButton, &flatButton, &tiltButton, &demudButton, &convolveText,
             &floorKnob, &gateAttackKnob, &gateReleaseKnob, &ratioKnob, &compAttackKnob, &compReleaseKnob,
             &reductionKnob, &offsetKnob, &smoothingKnob, &subReleaseKnob, &eqHint,
             &rangeCaption, &rangeLowLabel, &rangeHighLabel, &resLowKnob, &resHighKnob,
             &resDepthKnob, &resSharpnessKnob, &resAttackKnob, &resReleaseKnob, &resTiltKnob, &resetDepthButton,
             &sidechainCheckbox, &listenNoiseCheckbox, &listenReducedCheckbox, &listenResonanceCheckbox,
             &resetRatioButton, &ratioHint })
        body.addChildComponent (item);

    drawer.setBounds (0, cardY - 8, rightColumnX, bodyHeight - cardY + 8);
    learnLengthKnob.setBounds (40, 50, 90, 99);
    drawerOutputKnob.setBounds (150, 50, 90, 99);
    drawer.addAndMakeVisible (learnLengthKnob);
    drawer.addAndMakeVisible (drawerOutputKnob);

    // Phase 4 Wave 2 (item 28, mockup 59): Mid/Side beside Learn length and Output.
    midSideCheckbox.setBounds (270, 62, 120, 22);
    midSideCheckbox.setTooltip ("Works on mid and side instead of left and right. Builds a new engine, so the sound crossfades.");
    drawer.addAndMakeVisible (midSideCheckbox);

    styleLabel (midSideText, "Every mode works on mid and side instead of left and right, each with its own curve, Learn and gains. "
                             "The Mid | Side switch beside the Mode row picks which one you edit. Convolve L/R stays left and right.",
                colours::textSecondary, juce::Justification::topLeft, 11.5f);
    midSideText.setBounds (296, 88, 400, 48);
    midSideText.setMinimumHorizontalScale (1.0f);
    drawer.addAndMakeVisible (midSideText);

    body.addChildComponent (drawer);
    editLink.onClick = [this] { drawer.setVisible (! drawer.isVisible()); };
    addLatencyNote (fftDropdown);
    addLatencyNote (lowLatencyCheckbox);

    updateMode();
}

FirProcessor& FirEditor::firProcessor() noexcept
{
    return dynamic_cast<FirProcessor&> (getFancyProcessor());
}

void FirEditor::updateMode()
{
    const auto mode = firProcessor().getMode();

    if (mode == shownMode)
        return;

    shownMode = mode;

    modeSwitch.setSelectedColours (modeColour (mode), mode == gate ? colours::bgWell : colours::textPrimary);

    const auto curveMode = mode == eq || mode == gate || mode == compressor;

    learnCard.setVisible (mode == subtract);
    curveCaption.setVisible (curveMode);
    resetRatioButton.setVisible (mode == compressor);
    ratioHint.setVisible (mode == compressor);
    resetButton.setVisible (curveMode);
    flatButton.setVisible (mode == eq);
    tiltButton.setVisible (mode == eq);
    demudButton.setVisible (mode == eq);
    convolveText.setVisible (mode == convolve);
    eqHint.setVisible (mode == eq);

    curveCaption.setText (mode == eq ? "CURVE" : "THRESHOLD CURVE", juce::dontSendNotification);

    for (auto* knob : { &floorKnob, &gateAttackKnob, &gateReleaseKnob })
        knob->setVisible (mode == gate);

    for (auto* knob : { &ratioKnob, &compAttackKnob, &compReleaseKnob })
        knob->setVisible (mode == compressor);

    for (auto* knob : { &reductionKnob, &offsetKnob, &smoothingKnob, &subReleaseKnob })
        knob->setVisible (mode == subtract);

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &rangeCaption, &rangeLowLabel, &rangeHighLabel, &resLowKnob, &resHighKnob,
             &resDepthKnob, &resSharpnessKnob, &resAttackKnob, &resReleaseKnob, &listenResonanceCheckbox, &resTiltKnob, &resetDepthButton })
        item->setVisible (mode == resonance);

    const auto dynamics = mode == gate || mode == compressor || mode == subtract;

    // Resonance listens to its own input, so the Sidechain detector is greyed there (Q2.7, Decision 4).
    sidechainCheckbox.setVisible (dynamics || mode == resonance);
    sidechainCheckbox.setEnabled (mode != resonance);
    sidechainCheckbox.setAlpha (mode == resonance ? 0.4f : 1.0f);
    sidechainCheckbox.setTooltip (mode == resonance ? "Resonance listens to its own input" : juce::String());
    listenNoiseCheckbox.setVisible (mode == subtract);
    listenReducedCheckbox.setVisible (mode == gate || mode == compressor);
    lowLatencyCheckbox.setAlpha (mode == convolve ? 0.4f : 1.0f);

    // Phase 4 Wave 1: Learn only where there's a threshold curve; Edit moves over for it.
    const auto learns = mode == gate || mode == compressor;
    graph.cancelLearn();
    curveLearnButton.setVisible (learns);
    editLabel.setBounds ((learns ? 504 : 484) + grow, 10, learns ? 26 : 40, 25);
}

void FirEditor::onTick (float seconds)
{
    auto& processor = firProcessor();

    updateMode();

    // Phase 4 Wave 2: Mid | Side while Mid/Side is on (never in Convolve L/R).
    midSideSwitch.setVisible (processor.isMidSide());
    midSideSwitch.setSelected (processor.getShared().viewSide.load() ? 1 : 0);

    graph.update();
    learnCard.update();

    // Phase 4 Wave 1: Learn's progress on its button.
    curveLearnButton.setText (graph.isLearningCurve() ? juce::String (juce::roundToInt (graph.getCurveLearnProgress() * 100.0f)) + "%" : "Learn");
    outMeter.update (processor.outputMeter, seconds);

    const auto latency = processor.getLatency();
    const auto rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;

    // Milliseconds only: the header's free space is narrower since the workflow controls (W.1).
    latencyLabel.setText ("Latency " + fancy::formatFixed ((float) (1000.0 * latency / rate), 1) + " ms",
                          juce::dontSendNotification);
}

void FirEditor::showPreview (bool drawerOpen)
{
    showPreview (drawerOpen ? Preview::drawer : Preview::subtractLearning);
}

void FirEditor::showPreview (Preview preview)
{
    stopUpdates();

    setValue (parameter ("fft"), 4.0f);

    // Phase 4 Wave 2 (mockup 59): Mid/Side in the drawer's panel and the side's ratio curve; off
    // everywhere else, so the other screenshots stay as they were.
    const auto sidePreview = preview == Preview::compressorRatioSide;
    firProcessor().setListening (false);
    setValue (parameter ("midSide"), preview == Preview::drawer || sidePreview ? 1.0f : 0.0f);
    firProcessor().setViewSide (sidePreview);
    graph.setEditsRatio (sidePreview);
    midSideSwitch.setVisible (firProcessor().isMidSide());
    midSideSwitch.setSelected (sidePreview ? 1 : 0);

    if (preview == Preview::compressorThreshold || sidePreview)
    {
        // The mockup's harsh band: the threshold dips at 3 kHz, and the ratio is 4:1 from 2 to
        // 5 kHz and 1:1 elsewhere, as the Dynamic de-harsh preset has it.
        setValue (parameter ("mode"), (float) compressor);
        setValue (parameter ("compRatio"), 4.0f);
        setValue (parameter ("compAttack"), 20.0f);
        setValue (parameter ("compRelease"), 200.0f);

        auto& owner = firProcessor();
        owner.setPoints (owner.shownCurve (compressor), { { 1200.0, -30.0 }, { 3000.0, -36.0 }, { 8000.0, -30.0 } });
        owner.setPoints (owner.shownCurve (fir::ratioCurve), { { 1800.0, -2.0 }, { 2200.0, 0.0 }, { 3000.0, 0.0 }, { 5000.0, 0.0 }, { 6000.0, -2.0 } });

        updateMode();
        midSideSwitch.setVisible (owner.isMidSide());
        graph.update();
        graph.showHarshPreview();
        graph.selectPoint (sidePreview ? 2 : -1);
        outMeter.show (-10.4f, -9.8f);
        latencyLabel.setText ("Latency 42.7 ms", juce::dontSendNotification);
        drawer.setVisible (false);
        return;
    }

    if (preview == Preview::eqCurve)
    {
        setValue (parameter ("mode"), (float) eq);
        firProcessor().setPoints (eq, { { 80.0, 3.0 }, { 250.0, -4.0 }, { 3000.0, 2.0 }, { 12000.0, 4.0 } });
    }
    else if (preview == Preview::resonance || preview == Preview::resonanceRange)
    {
        // Phase 3 Q2.7, mockup 28: panel 1 at the defaults, panel 2 the "Harsh Vocal" preset
        // with Listen on.
        setValue (parameter ("mode"), (float) resonance);

        if (preview == Preview::resonanceRange)
        {
            setValue (parameter ("resDepth"), 8.0f);
            setValue (parameter ("resSharpness"), 60.0f);
            setValue (parameter ("resAttack"), 2.0f);
            setValue (parameter ("resRelease"), 60.0f);
            setValue (parameter ("resLow"), 1500.0f);
            setValue (parameter ("resHigh"), 10000.0f);
            firProcessor().setListening (true);
        }

        updateMode();
        graph.update();
        graph.showPreviewSpectra (true);
        outMeter.show (-10.4f, -9.8f);
        latencyLabel.setText ("Latency 42.7 ms", juce::dontSendNotification);
        drawer.setVisible (false);
        return;
    }
    else
    {
        setValue (parameter ("mode"), (float) subtract);
    }

    // A made-up noise profile matching the mockup's red line, in the same units the engine
    // learns: the level the graph shows, converted back to power per bin.
    auto& shared = firProcessor().getShared();

    const auto scale = [] (int fftSize)
    {
        auto sum = 0.0, energy = 0.0;

        for (int n = 0; n < fftSize; ++n)
        {
            const auto window = std::sqrt (0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * n / fftSize));
            sum += window;
            energy += window * window;
        }

        return energy * (2.0 / sum) * (2.0 / sum);
    } (2048);

    for (int i = 0; i < profilePoints; ++i)
    {
        const auto hz = juce::jmax (22.0, profileHz (i));
        const auto octave = std::log10 (hz);
        const auto noise = -62.0 - 6.0 * (octave - 1.5) + (std::abs (hz - 60.0) < 6.0 ? 20.0 : 0.0) + (std::abs (hz - 120.0) < 8.0 ? 12.0 : 0.0) + 5.0;

        shared.profile[(size_t) i].store ((float) (std::pow (10.0, noise / 10.0) / scale));
    }

    // The mockup shows learning in progress, with the profile already drawn.
    shared.hasProfile.store (true);
    shared.profileVersion.fetch_add (1);

    // Phase 2.1 A, panel 5: Adaptive, the tracked profile a little under the learned one.
    if (preview == Preview::subtractAdaptive)
    {
        setValue (parameter ("source"), (float) fir::adaptiveSource);

        for (int i = 0; i < profilePoints; ++i)
        {
            const auto hz = juce::jmax (22.0, profileHz (i));
            const auto noise = -66.0 - 7.0 * (std::log10 (hz) - 1.5) + 2.0 * std::sin (hz / 900.0);
            shared.adaptive[(size_t) i].store ((float) (std::pow (10.0, noise / 10.0) / scale));
        }

        shared.hasAdaptive.store (true);
    }

    updateMode();
    graph.update();
    graph.showPreviewSpectra();

    if (preview == Preview::subtractAdaptive)
        learnCard.showTracking (-52.0f, true);
    else
        learnCard.showLearning (1.6f, 2.0f);

    outMeter.show (-10.4f, -9.8f);

    latencyLabel.setText ("Latency 42.7 ms", juce::dontSendNotification);

    drawer.setVisible (preview == Preview::drawer);
}

juce::Colour FirEditor::getAccentColour()
{
    return modeColour (firProcessor().getMode());
}

std::vector<ShellEditor::PopupOpener> FirEditor::getPopupOpeners()
{
    auto openers = ShellEditor::getPopupOpeners();
    openers.push_back ({ "dropdown-edit", [this] { editDropdown.click(); } });
    openers.push_back ({ "dropdown-fft", [this] { fftDropdown.click(); } });
    openers.push_back ({ "sidechain-notice", [this] { forceSidechainNotice (true); } });
    openers.push_back ({ "point-prompt", [this]
                         {
                             const auto mode = firProcessor().getMode();
                             const auto curve = mode == resonance ? fir::depthCurve : juce::jmin (mode, (int) compressor);

                             if (! firProcessor().getPoints (curve).empty())
                                 graph.typePoint (0);
                         } });
    openers.push_back ({ "axis-prompt", [this] { graph.typeAxis (true); } });
    openers.push_back ({ "clear-profile", [this] { confirmThen ("Clear the noise profile?", "The learned profile will be removed. You can learn it again from a section with only noise.", "Clear", true, [] {}); } });
    openers.push_back ({ "tooltip-low-latency", [this] { showTip (lowLatencyCheckbox, lowLatencyCheckbox.getTooltip()); } });
    openers.push_back ({ "tooltip-sidechain-resonance", [this]
                         {
                             setValue (parameter ("mode"), (float) resonance);
                             updateMode();
                             showTip (sidechainCheckbox, sidechainCheckbox.getTooltip());
                         } });
    return openers;
}

juce::Rectangle<int> FirEditor::getSidechainNoticeBounds()
{
    // Over the top of the graph, clear of the mode switch it would otherwise cover.
    const auto area = getRoot().getLocalArea (&graph, graph.getLocalBounds());
    return { area.getCentreX() - 190, area.getY() + 8, 380, 22 };
}
