#include "KickerEditor.h"

using namespace fancy::ui;

namespace
{
// Native Figma units: the Kicker frame without its 22 px host title bar.
constexpr int baseWidth = 392;
constexpr int baseHeight = 472;
constexpr int leftPanelWidth = 264;
constexpr int bodyHeight = baseHeight - ShellEditor::headerHeight;

constexpr InOutMeter::Geometry meterGeometry { 364.0f, 76.3f, 4.829f, 372.0f };
} // namespace

KickerEditor::KickerEditor (KickerProcessor& processor)
    : ShellEditor (processor, baseWidth, baseHeight, HeaderStyle::compact),
      panels ("Fancy Kicker", leftPanelWidth, 381),
      freqKnob (parameter ("freq"), "Freq", colours::uiRed, Knob::largeStyle),
      wetKnob (parameter ("wet"), "Wet", colours::uiRed, Knob::mediumStyle),
      dryKnob (parameter ("dry"), "Dry", colours::uiRed, Knob::mediumStyle),
      editLink (colours::uiRed),
      meter (meterGeometry),
      qKnob (parameter ("q"), "Filter Q", colours::uiRed, Knob::smallStyle),
      drawerOutputKnob (parameter ("output"), "Output", colours::uiTeal, Knob::smallStyle),
      monoCheckbox (parameter ("monoBelow"), "Mono below Freq"),
      softClipCheckbox (parameter ("softClip"), "Soft clip")
{
    auto& body = getBody();

    panels.setInterceptsMouseClicks (false, false);
    panels.setBounds (body.getLocalBounds());
    body.addAndMakeVisible (panels);

    freqKnob.setBounds (66, 28, 132, 172);
    freqKnob.setShowDefaultMark (true);
    wetKnob.setBounds (34, 216, 90, 123);
    dryKnob.setBounds (140, 216, 90, 123);
    editLink.setBounds (16, 377, 64, 24);
    meter.setBounds (leftPanelWidth, 0, baseWidth - leftPanelWidth, bodyHeight);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &freqKnob, &wetKnob, &dryKnob, &editLink, &meter })
        body.addAndMakeVisible (item);

    drawer.setBounds (0, 0, leftPanelWidth, bodyHeight);
    qKnob.setBounds (32, 60, 90, 99);
    drawerOutputKnob.setBounds (142, 60, 90, 99);
    monoCheckbox.setBounds (32, 192, 200, 20);
    softClipCheckbox.setBounds (32, 224, 200, 20);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &qKnob, &drawerOutputKnob, &monoCheckbox, &softClipCheckbox })
        drawer.addAndMakeVisible (item);

    body.addChildComponent (drawer);
    editLink.onClick = [this] { drawer.setVisible (true); };
}

void KickerEditor::onTick (float seconds)
{
    auto& processor = getFancyProcessor();
    meter.update (processor.inputMeter, processor.outputMeter, seconds);
}

void KickerEditor::showPreview (bool drawerOpen)
{
    stopUpdates();
    meter.show (-5.2f, -6.0f, -5.4f, -6.1f);
    drawer.setVisible (drawerOpen);
}
