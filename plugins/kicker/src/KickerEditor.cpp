#include "KickerEditor.h"

using namespace fancy::ui;

namespace
{
// Native Figma units: the Kicker frame without its 22 px host title bar. Sub mode and the
// Boost | Sub switch follow reference/phase3/21__kicker_sub_mockup.png; Boost's knobs share
// Sub's positions (docs/kicker-sub.md, Decision 3).
constexpr int baseWidth = 392;
constexpr int baseHeight = 472;
constexpr int leftPanelWidth = 264;
constexpr int bodyHeight = baseHeight - ShellEditor::headerHeight;

constexpr InOutMeter::Geometry meterGeometry { 364.0f, 76.3f, 4.829f, 372.0f };

// The large knob, a little smaller than Figma's 116 px so the mode switch and the octave row
// fit above and below it, as the mockup draws.
constexpr Knob::Style mainStyle { 104.0f, 93.0f, 5.0f, 17.0f, 30.0f, 19.0f, 1.0f, 1.0f, 23.0f, 16.0f, true, true };

const char* const boostCaption = "Boost: lifts its own low end";

void setValue (juce::RangedAudioParameter& parameter, float value)
{
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
}
} // namespace

KickerEditor::KickerEditor (KickerProcessor& processor)
    : ShellEditor (processor, baseWidth, baseHeight, HeaderStyle::compact),
      panels (boostCaption, leftPanelWidth, 381),
      modeSegments (parameter ("mode"), colours::uiRed, colours::bgWell),
      freqKnob (parameter ("freq"), "Freq", colours::uiRed, mainStyle),
      wetKnob (parameter ("wet"), "Wet", colours::uiRed, Knob::mediumStyle),
      cutoffKnob (parameter ("subCutoff"), "Cutoff", colours::uiRed, mainStyle),
      subKnob (parameter ("subLevel"), "Sub", colours::uiRed, Knob::mediumStyle),
      octaveSegments (parameter ("subOctave"), colours::grey5, colours::textPrimary),
      dryKnob (parameter ("dry"), "Dry", colours::uiRed, Knob::mediumStyle),
      editLink (colours::uiRed),
      meter (meterGeometry),
      qKnob (parameter ("q"), "Filter Q", colours::uiRed, Knob::smallStyle),
      drawerOutputKnob (parameter ("output"), "Output", colours::uiTeal, Knob::smallStyle),
      sensitivityKnob (parameter ("subSensitivity"), "Sensitivity", colours::uiRed, Knob::smallStyle),
      subLowpassKnob (parameter ("subLowpass"), "Sub low-pass", colours::uiRed, Knob::smallStyle),
      monoCheckbox (parameter ("monoBelow"), "Mono below Freq"),
      softClipCheckbox (parameter ("softClip"), "Soft clip"),
      subMonoCheckbox (parameter ("subMono"), "Mono sub")
{
    auto& body = getBody();

    panels.setInterceptsMouseClicks (false, false);
    panels.setBounds (body.getLocalBounds());
    body.addAndMakeVisible (panels);

    modeSegments.setBounds (82, 15, 100, 28);
    modeSegments.setTooltip ("Boost lifts the kick's own low end. Sub creates new bass an octave or two below it.");

    freqKnob.setBounds (66, 52, 132, 148);
    freqKnob.setShowDefaultMark (true);
    cutoffKnob.setBounds (66, 52, 132, 148);
    cutoffKnob.setShowDefaultMark (true);
    cutoffKnob.setTooltip ("The highest frequency Sub listens to. Set it just above the kick's body.");

    octaveSegments.setBounds (78, 205, 112, 24);
    octaveSegments.setTooltip (juce::String::fromUTF8 ("How far below the kick the sub plays. \xe2\x88\x92" "2 oct suits toms and pitched-up 808s: "
                                                       "two octaves below a 60 Hz kick is 15 Hz, which is filtered out."));

    wetKnob.setBounds (34, 236, 90, 123);
    subKnob.setBounds (34, 236, 90, 123);
    subKnob.setTooltip ("The level of the sub it creates.");
    dryKnob.setBounds (140, 236, 90, 123);

    editLink.setBounds (16, 377, 64, 24);
    meter.setBounds (leftPanelWidth, 0, baseWidth - leftPanelWidth, bodyHeight);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &modeSegments, &dryKnob, &editLink, &meter })
        body.addAndMakeVisible (item);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &freqKnob, &wetKnob, &cutoffKnob, &subKnob, &octaveSegments })
        body.addChildComponent (item);

    drawer.setBounds (0, 0, leftPanelWidth, bodyHeight);
    qKnob.setBounds (32, 60, 90, 99);
    drawerOutputKnob.setBounds (142, 60, 90, 99);
    monoCheckbox.setBounds (32, 192, 200, 20);
    softClipCheckbox.setBounds (32, 224, 200, 20);

    sensitivityKnob.setBounds (8, 60, 84, 99);
    sensitivityKnob.setTooltip ("Below this level Sub stays silent, so bleed and noise don't make sub.");
    subLowpassKnob.setBounds (90, 60, 84, 99);
    subLowpassKnob.setTooltip ("Smooths the sub: nothing it adds goes much above this.");
    subMonoCheckbox.setBounds (32, 192, 200, 20);
    subMonoCheckbox.setTooltip ("The same sub in both channels, made from L+R. Off: each channel makes its own.");

    drawer.addAndMakeVisible (drawerOutputKnob);
    drawer.addAndMakeVisible (softClipCheckbox);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &qKnob, &monoCheckbox, &sensitivityKnob, &subLowpassKnob, &subMonoCheckbox })
        drawer.addChildComponent (item);

    body.addChildComponent (drawer);
    editLink.onClick = [this] { drawer.setVisible (true); };

    updateMode();
}

void KickerEditor::updateMode()
{
    const auto sub = juce::roundToInt (parameter ("mode").convertFrom0to1 (parameter ("mode").getValue())) == 1;
    const auto twoOctaves = juce::roundToInt (parameter ("subOctave").convertFrom0to1 (parameter ("subOctave").getValue())) == 1;

    panels.setCaption (sub ? (twoOctaves ? "Sub: adds two octaves below" : "Sub: adds an octave below") : boostCaption);

    const auto mode = sub ? 1 : 0;

    if (mode == shownMode)
        return;

    shownMode = mode;

    freqKnob.setVisible (! sub);
    wetKnob.setVisible (! sub);
    cutoffKnob.setVisible (sub);
    subKnob.setVisible (sub);
    octaveSegments.setVisible (sub);

    qKnob.setVisible (! sub);
    monoCheckbox.setVisible (! sub);
    sensitivityKnob.setVisible (sub);
    subLowpassKnob.setVisible (sub);
    subMonoCheckbox.setVisible (sub);

    // Sub's drawer has three knobs in a row; Boost's has two.
    drawerOutputKnob.setBounds (sub ? juce::Rectangle<int> (172, 60, 84, 99) : juce::Rectangle<int> (142, 60, 90, 99));
    softClipCheckbox.setBounds (32, 224, 200, 20);
}

void KickerEditor::onTick (float seconds)
{
    auto& processor = getFancyProcessor();
    updateMode();
    meter.update (processor.inputMeter, processor.outputMeter, seconds);
}

void KickerEditor::showPreview (bool drawerOpen, bool subMode)
{
    stopUpdates();
    setValue (parameter ("mode"), subMode ? 1.0f : 0.0f);
    updateMode();

    if (subMode)
        meter.show (-9.2f, -9.4f, -5.8f, -6.0f);
    else
        meter.show (-5.2f, -6.0f, -5.4f, -6.1f);

    drawer.setVisible (drawerOpen);
}

juce::Colour KickerEditor::getAccentColour()
{
    return colours::uiRed;
}

std::vector<ShellEditor::PopupOpener> KickerEditor::getPopupOpeners()
{
    auto openers = ShellEditor::getPopupOpeners();
    openers.push_back ({ "value-freq", [this] { updateMode(); freqKnob.openValueEditor(); } });
    return openers;
}
