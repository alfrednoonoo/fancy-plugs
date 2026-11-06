#include "CompEditor.h"

using namespace fancy::ui;

namespace
{
// Figma Reacomp frame without its host title bar, widened by the GR panel.
constexpr int baseWidth = 934;
constexpr int baseHeight = 527;
constexpr int bodyHeight = baseHeight - ShellEditor::headerHeight;

constexpr int gridLeft = 212;
constexpr int gridWidth = 524;
constexpr int rowTop = 50;
constexpr int rowHeight = 374;

void setValue (juce::RangedAudioParameter& parameter, float value)
{
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
}

void styleLabel (juce::Label& label, const juce::String& text, juce::Colour colour, juce::Justification justification)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (font (Weight::regular, 14.0f));
    label.setColour (juce::Label::textColourId, colour);
    label.setJustificationType (justification);
    label.setBorderSize ({});
}
} // namespace

void CompEditor::Panels::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgSection);

    g.setColour (colours::bgBody);
    g.fillRect (0, rowTop, 108, rowHeight);
    g.fillRect (112, rowTop, 96, rowHeight);
    g.fillRect (gridLeft, rowTop, gridWidth, 199);
    g.fillRect (gridLeft, 253, 260, 171);
    g.fillRect (476, 253, 260, 171);
}

CompEditor::CompEditor (CompProcessor& processor)
    : ShellEditor (processor, baseWidth, baseHeight, HeaderStyle::full),
      detectorDropdown (parameter ("detector")),
      editLink (colours::uiTeal),
      thresholdMeter (parameter ("threshold")),
      ratioKnob (parameter ("ratio"), "Ratio", colours::uiTeal, Knob::mediumStyle),
      kneeKnob (parameter ("knee"), "Knee", colours::uiTeal, Knob::mediumStyle),
      attackKnob (parameter ("attack"), "Attack", colours::uiTeal, Knob::mediumStyle),
      releaseKnob (parameter ("release"), "Release", colours::uiTeal, Knob::mediumStyle),
      weirdCheckbox (parameter ("weird"), "Weird", colours::uiTealBright),
      classicCheckbox (parameter ("classic"), "Classic", colours::uiTealBright),
      autoCheckbox (parameter ("auto"), "Auto", colours::uiTealBright),
      preCompKnob (parameter ("precomp"), "Pre-comp", colours::accentGreen, Knob::mediumStyle),
      rmsKnob (parameter ("rms"), "RMS size", colours::accentGreen, Knob::mediumStyle),
      lowpassKnob (parameter ("lowpass"), "Lowpass", colours::uiBlue, Knob::mediumStyle),
      highpassKnob (parameter ("highpass"), "Highpass", colours::uiBlue, Knob::mediumStyle),
      linkCheckbox (parameter ("link"), "Link channels"),
      makeupCheckbox (parameter ("makeup"), "Auto make-up"),
      previewCheckbox (parameter ("preview"), "Preview filter"),
      limitCheckbox (parameter ("limit"), "Limit output"),
      wetFader (parameter ("wet"), colours::uiTeal),
      dryFader (parameter ("dry"), colours::uiTeal),
      oversamplingDropdown (parameter ("oversampling")),
      phaseDropdown (parameter ("osPhase")),
      releaseCurveDropdown (parameter ("releaseCurve")),
      feedbackCheckbox (parameter ("feedback"), "Feedback detection"),
      attack90Checkbox (parameter ("attack90"), "Attack = 90 % rise"),
      linkAmountKnob (parameter ("linkAmount"), "Link amount", colours::uiTeal, Knob::smallStyle),
      drawerOutputKnob (parameter ("output"), "Output", colours::uiTeal, Knob::smallStyle)
{
    auto& body = getBody();

    panels.setInterceptsMouseClicks (false, false);
    panels.setBounds (body.getLocalBounds());
    body.addAndMakeVisible (panels);

    styleLabel (detectorLabel, "Detector input", colours::textPrimary, juce::Justification::centredLeft);
    styleLabel (wetLabel, "Wet", colours::textSecondary, juce::Justification::centred);
    styleLabel (dryLabel, "Dry", colours::textSecondary, juce::Justification::centred);

    detectorLabel.setBounds (24, 13, 100, 24);
    detectorDropdown.setBounds (129, 13, 131, 24);
    editLink.setBounds (656, 13, 64, 24);

    thresholdMeter.setBounds (0, 74, 108, 326);
    gainReductionMeter.setBounds (112, 74, 96, 326);

    ratioKnob.setBounds (258, 75, 90, 123);
    kneeKnob.setBounds (372, 75, 90, 123);
    attackKnob.setBounds (486, 75, 90, 123);
    releaseKnob.setBounds (600, 75, 90, 123);
    weirdCheckbox.setBounds (388, 205, 70, 20);
    classicCheckbox.setBounds (497, 205, 76, 20);
    autoCheckbox.setBounds (619, 205, 60, 20);

    attackKnob.setShowDefaultMark (true);
    releaseKnob.setShowDefaultMark (true);
    rmsKnob.setShowDefaultMark (true);

    preCompKnob.setBounds (244, 277, 90, 123);
    rmsKnob.setBounds (350, 277, 90, 123);
    lowpassKnob.setBounds (508, 277, 90, 123);
    highpassKnob.setBounds (614, 277, 90, 123);

    linkCheckbox.setBounds (72, 440, 130, 20);
    makeupCheckbox.setBounds (232, 440, 130, 20);
    previewCheckbox.setBounds (392, 440, 130, 20);
    limitCheckbox.setBounds (552, 440, 130, 20);

    outMeter.setBounds (760, 24, 60, 426);
    wetLabel.setBounds (820, 24, 44, 17);
    dryLabel.setBounds (866, 24, 44, 17);
    wetFader.setBounds (820, 49, 44, 401);
    dryFader.setBounds (866, 49, 44, 401);

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &detectorLabel, &detectorDropdown, &editLink, &thresholdMeter, &gainReductionMeter,
             &ratioKnob, &kneeKnob, &attackKnob, &releaseKnob, &weirdCheckbox, &classicCheckbox, &autoCheckbox,
             &preCompKnob, &rmsKnob, &lowpassKnob, &highpassKnob,
             &linkCheckbox, &makeupCheckbox, &previewCheckbox, &limitCheckbox,
             &outMeter, &wetLabel, &dryLabel, &wetFader, &dryFader })
        body.addAndMakeVisible (item);

    styleLabel (oversamplingLabel, "Oversampling", colours::textPrimary, juce::Justification::centredLeft);
    styleLabel (phaseLabel, "Oversampling filter", colours::textPrimary, juce::Justification::centredLeft);
    styleLabel (releaseCurveLabel, "Release curve", colours::textPrimary, juce::Justification::centredLeft);

    drawer.setBounds (gridLeft, rowTop, gridWidth, rowHeight);

    oversamplingLabel.setBounds (24, 60, 130, 24);
    oversamplingDropdown.setBounds (160, 60, 110, 24);
    phaseLabel.setBounds (24, 96, 130, 24);
    phaseDropdown.setBounds (160, 96, 110, 24);
    releaseCurveLabel.setBounds (24, 132, 130, 24);
    releaseCurveDropdown.setBounds (160, 132, 110, 24);
    feedbackCheckbox.setBounds (24, 180, 200, 20);
    attack90Checkbox.setBounds (24, 212, 200, 20);
    linkAmountKnob.setBounds (306, 60, 90, 99);
    drawerOutputKnob.setBounds (412, 60, 90, 99);

    for (juce::Component* item : std::initializer_list<juce::Component*> {
             &oversamplingLabel, &oversamplingDropdown, &phaseLabel, &phaseDropdown, &releaseCurveLabel, &releaseCurveDropdown,
             &feedbackCheckbox, &attack90Checkbox, &linkAmountKnob, &drawerOutputKnob })
        drawer.addAndMakeVisible (item);

    body.addChildComponent (drawer);
    editLink.onClick = [this] { drawer.setVisible (! drawer.isVisible()); };

    for (auto* knob : { &ratioKnob, &kneeKnob, &attackKnob, &releaseKnob })
        knob->setValueColour (colours::uiTealBright);

    attackKnob.setTooltip ("Time constant: the time to reach about 63 % of the target. Change this in EDIT.");
}

void CompEditor::onTick (float seconds)
{
    auto& processor = dynamic_cast<CompProcessor&> (getFancyProcessor());

    const auto left = processor.takeGainReduction (0);
    const auto right = processor.takeGainReduction (1);

    thresholdMeter.update (processor.inputMeter, juce::jmax (left, right) > 0.5f, seconds);
    gainReductionMeter.update (left, right, seconds);
    outMeter.update (processor.outputMeter, seconds);
}

void CompEditor::showPreview (bool drawerOpen)
{
    stopUpdates();

    setValue (parameter ("threshold"), -4.4f);
    setValue (parameter ("highpass"), 93.0f);
    setValue (parameter ("wet"), -11.7f);
    setValue (parameter ("dry"), -4.2f);

    for (const auto* id : { "weird", "classic", "auto", "makeup", "preview", "limit" })
        setValue (parameter (id), 1.0f);

    setValue (parameter ("link"), 0.0f);

    thresholdMeter.show (-9.3f, -10.7f, true);
    gainReductionMeter.show (2.8f, 2.6f);
    outMeter.show (-4.2f, -5.8f);

    drawer.setVisible (drawerOpen);
}
