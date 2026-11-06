#include "KickerEditor.h"

using namespace fancy::ui;

namespace
{
constexpr int baseWidth = 265;
constexpr int baseHeight = 315;
constexpr int leftPanelWidth = 178;
constexpr int bodyHeight = baseHeight - ShellEditor::headerHeight;
} // namespace

void KickerEditor::Panels::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.setColour (colours::bgBody);
    g.fillRect (bounds.removeFromLeft (leftPanelWidth));
    g.setColour (colours::bgSection);
    g.fillRect (bounds);

    g.setColour (colours::textSecondary);
    g.setFont (font (Weight::regular, 11.0f));
    g.drawText ("Fancy Kicker", 70, 255, 92, 16, juce::Justification::centredRight, false);
}

KickerEditor::KickerEditor (KickerProcessor& processor)
    : ShellEditor (processor, baseWidth, baseHeight, HeaderStyle::compact),
      freqKnob (parameter ("freq"), "Freq", colours::uiRed, Knob::largeStyle),
      wetKnob (parameter ("wet"), "Wet", colours::uiRed, Knob::mediumStyle),
      dryKnob (parameter ("dry"), "Dry", colours::uiRed, Knob::mediumStyle),
      editLink (colours::uiRed),
      qKnob (parameter ("q"), "Filter Q", colours::uiRed, Knob::smallStyle),
      drawerOutputKnob (parameter ("output"), "Output", colours::uiTeal, Knob::smallStyle),
      monoCheckbox (parameter ("monoBelow"), "Mono below Freq"),
      softClipCheckbox (parameter ("softClip"), "Soft clip")
{
    auto& body = getBody();

    panels.setInterceptsMouseClicks (false, false);
    panels.setBounds (body.getLocalBounds());
    body.addAndMakeVisible (panels);

    freqKnob.setBounds (29, 18, 120, 117);
    wetKnob.setBounds (17, 144, 72, 86);
    dryKnob.setBounds (89, 144, 72, 86);
    editLink.setBounds (17, 255, 46, 16);
    meter.setBounds (leftPanelWidth, 0, baseWidth - leftPanelWidth, bodyHeight);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &freqKnob, &wetKnob, &dryKnob, &editLink, &meter })
        body.addAndMakeVisible (item);

    drawer.setBounds (0, 0, leftPanelWidth, bodyHeight);
    qKnob.setBounds (10, 42, 76, 80);
    drawerOutputKnob.setBounds (92, 42, 76, 80);
    monoCheckbox.setBounds (20, 142, 150, 20);
    softClipCheckbox.setBounds (20, 170, 150, 20);

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
