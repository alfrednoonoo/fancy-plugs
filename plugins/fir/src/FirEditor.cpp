#include "FirEditor.h"

#include <fancy/plugin/ParamFormat.h>

using namespace fancy::ui;
using namespace fir;

namespace
{
// design/phase2-mockup-src/fir.html at 100 %. The shared header is 16 px taller than the
// mockup's, so the window is too.
constexpr int baseWidth = 760;
constexpr int bodyHeight = 392;
constexpr int baseHeight = ShellEditor::headerHeight + bodyHeight;
constexpr int rightColumnX = 660;

constexpr int graphX = 13, graphY = 43, graphWidth = 610, graphHeight = 196;
constexpr int cardY = 248, cardHeight = 111;
constexpr int leftCardX = 13, leftCardWidth = 178;
constexpr int controlsCardX = 199, controlsCardWidth = 450;
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
    : firProcessor (processor)
{
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

    const auto showButtons = hasProfile && ! learning;

    clearButton.setBounds (12, 58, 70, 24);
    relearnButton.setBounds (90, 58, getWidth() - 102, 24);
    clearButton.setVisible (showButtons);
    relearnButton.setVisible (showButtons);

    repaint();
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

    g.setColour (colours::textSecondary);
    g.setFont (font (Weight::semiBold, 11.0f).withExtraKerningFactor (0.06f));
    g.drawText ("NOISE PROFILE", 12, 6, getWidth() - 24, 16, juce::Justification::centredLeft, false);

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
    if (! (hasProfile && ! learning) && ! learning && learnButtonBounds().contains (event.getPosition()))
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
      fftDropdown (parameter ("fft")),
      reduceCheckbox (parameter ("reduce"), "Reduce artifacts"),
      analysisCheckbox (parameter ("showAnalysis"), "Show analysis"),
      lowLatencyCheckbox (parameter ("lowLatency"), "Low latency"),
      sidechainCheckbox (parameter ("sidechain"), "Sidechain detector"),
      listenNoiseCheckbox (parameter ("listen"), "Listen to removed noise"),
      listenReducedCheckbox (parameter ("listen"), "Listen to gain-reduced part"),
      editLink (colours::uiTeal),
      outMeter (outGeometry),
      gainFader (parameter ("gain"), colours::uiTeal),
      learnLengthKnob (parameter ("learnLength"), "Learn length", modeColour (subtract), Knob::smallStyle),
      drawerOutputKnob (parameter ("output"), "Output", colours::uiTeal, Knob::smallStyle)
{
    auto& body = getBody();

    panels.setInterceptsMouseClicks (false, false);
    panels.setBounds (0, 0, baseWidth, bodyHeight);
    body.addAndMakeVisible (panels);

    styleLabel (latencyLabel, {}, colours::textSecondary, juce::Justification::centredRight);
    latencyLabel.setBounds (380, 13, 264, 24);
    getRoot().addAndMakeVisible (latencyLabel);

    styleLabel (modeLabel, "Mode", colours::textSecondary, juce::Justification::centredLeft);
    styleLabel (editLabel, "Edit", colours::textSecondary, juce::Justification::centredRight);

    modeLabel.setBounds (13, 10, 36, 25);
    modeSwitch.setBounds (50, 10, 308, 25);
    modeSwitch.setTextHeight (labelHeight);
    modeSwitch.setFitToText (true);
    editLabel.setBounds (484, 10, 40, 25);
    editDropdown.setBounds (530, 10, 119, 25);
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

    resetButton.onClick = [this] { firProcessor().resetCurve (juce::jmin (firProcessor().getMode(), numCurves - 1)); };
    flatButton.onClick = [this] { firProcessor().resetCurve (eq); };
    tiltButton.onClick = [this] { firProcessor().setPoints (eq, { { 20.0, -3.0 }, { 20000.0, 3.0 } }); };
    demudButton.onClick = [this] { firProcessor().setPoints (eq, { { 100.0, 0.0 }, { 250.0, -4.0 }, { 600.0, 0.0 } }); };

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

    styleLabel (eqHint, "The curve is the EQ. Click it to add a point.", colours::textSecondary, juce::Justification::centredLeft);
    eqHint.setBounds (controlsCardX + 14, cardY + 40, 250, 20);

    styleLabel (fftLabel, "FFT size", colours::textSecondary, juce::Justification::centredLeft);
    fftLabel.setBounds (471, cardY + 12, 100, 16);
    fftDropdown.setBounds (471, cardY + 30, 100, 24);
    fftDropdown.setTextHeight (labelHeight);
    reduceCheckbox.setBounds (471, cardY + 64, 160, 20);
    reduceCheckbox.setLabelHeight (labelHeight);

    analysisCheckbox.setBounds (15, footerY, 106, 20);
    lowLatencyCheckbox.setBounds (127, footerY, 92, 20);
    sidechainCheckbox.setBounds (225, footerY, 134, 20);
    listenNoiseCheckbox.setBounds (365, footerY, 180, 20);
    listenReducedCheckbox.setBounds (365, footerY, 200, 20);
    editLink.setBounds (586, footerY - 2, 64, 24);

    for (auto* checkbox : { &analysisCheckbox, &lowLatencyCheckbox, &sidechainCheckbox, &listenNoiseCheckbox, &listenReducedCheckbox })
        checkbox->setLabelHeight (labelHeight);

    lowLatencyCheckbox.setTooltip ("Halves the latency, at the same frequency resolution, with a little more smearing.");
    ratioKnob.setTooltip ("Below 1:1 the level above the threshold is raised: upward expansion.");

    outMeter.setBounds (668, 10, 46, 350);
    styleLabel (gainLabel, "Gain", colours::textSecondary, juce::Justification::centred, 14.0f);
    gainLabel.setBounds (709, 10, 33, 17);
    gainFader.setBounds (709, 26, 33, 331);

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &modeLabel, &modeSwitch, &editLabel, &editDropdown, &graph,
             &fftLabel, &fftDropdown, &reduceCheckbox,
             &analysisCheckbox, &lowLatencyCheckbox, &editLink, &outMeter, &gainLabel, &gainFader })
        body.addAndMakeVisible (item);

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &learnCard, &curveCaption, &resetButton, &flatButton, &tiltButton, &demudButton, &convolveText,
             &floorKnob, &gateAttackKnob, &gateReleaseKnob, &ratioKnob, &compAttackKnob, &compReleaseKnob,
             &reductionKnob, &offsetKnob, &smoothingKnob, &subReleaseKnob, &eqHint,
             &sidechainCheckbox, &listenNoiseCheckbox, &listenReducedCheckbox })
        body.addChildComponent (item);

    drawer.setBounds (0, cardY - 8, rightColumnX, bodyHeight - cardY + 8);
    learnLengthKnob.setBounds (40, 50, 90, 99);
    drawerOutputKnob.setBounds (150, 50, 90, 99);
    drawer.addAndMakeVisible (learnLengthKnob);
    drawer.addAndMakeVisible (drawerOutputKnob);

    body.addChildComponent (drawer);
    editLink.onClick = [this] { drawer.setVisible (! drawer.isVisible()); };

    setPresetDropdownWidth (170);
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

    const auto dynamics = mode == gate || mode == compressor || mode == subtract;

    sidechainCheckbox.setVisible (dynamics);
    listenNoiseCheckbox.setVisible (mode == subtract);
    listenReducedCheckbox.setVisible (mode == gate || mode == compressor);
    lowLatencyCheckbox.setAlpha (mode == convolve ? 0.4f : 1.0f);
}

void FirEditor::onTick (float seconds)
{
    auto& processor = firProcessor();

    updateMode();
    graph.update();
    learnCard.update();
    outMeter.update (processor.outputMeter, seconds);

    const auto latency = processor.getLatency();
    const auto rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;

    latencyLabel.setText ("Latency " + juce::String (latency) + " smp " + juce::String::charToString (0x00b7) + " "
                              + fancy::formatFixed ((float) (1000.0 * latency / rate), 1) + " ms",
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

    if (preview == Preview::eqCurve)
    {
        setValue (parameter ("mode"), (float) eq);
        firProcessor().setPoints (eq, { { 80.0, 3.0 }, { 250.0, -4.0 }, { 3000.0, 2.0 }, { 12000.0, 4.0 } });
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

    updateMode();
    graph.update();
    graph.showPreviewSpectra();
    learnCard.showLearning (1.6f, 2.0f);

    outMeter.show (-10.4f, -9.8f);

    latencyLabel.setText ("Latency 2048 smp " + juce::String::charToString (0x00b7) + " 42.7 ms", juce::dontSendNotification);

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
    openers.push_back ({ "point-prompt", [this] { if (! firProcessor().getPoints (juce::jmin (firProcessor().getMode(), numCurves - 1)).empty()) graph.typePoint (0); } });
    openers.push_back ({ "axis-prompt", [this] { graph.typeAxis (true); } });
    openers.push_back ({ "clear-profile", [this] { confirmThen ("Clear the noise profile?", "The learned profile will be removed. You can learn it again from a section with only noise.", "Clear", true, [] {}); } });
    openers.push_back ({ "tooltip-low-latency", [this] { showTip (lowLatencyCheckbox, lowLatencyCheckbox.getTooltip()); } });
    return openers;
}
