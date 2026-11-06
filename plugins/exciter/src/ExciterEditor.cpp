#include "ExciterEditor.h"

using namespace fancy::ui;

namespace
{
// Native Figma units: the Exciter frame without its 22 px host title bar.
constexpr int baseWidth = 392;
constexpr int baseHeight = 432;
constexpr int leftPanelWidth = 264;
constexpr int bodyHeight = baseHeight - ShellEditor::headerHeight;

constexpr InOutMeter::Geometry meterGeometry { 324.0f, 70.67f, 4.308f, 332.0f };

void setValue (juce::RangedAudioParameter& parameter, float value)
{
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
}
} // namespace

ExciterEditor::ExciterEditor (ExciterProcessor& processor)
    : ShellEditor (processor, baseWidth, baseHeight, HeaderStyle::compact),
      panels ("Fancy Exciter", leftPanelWidth, 341),
      freqKnob (parameter ("freq"), "Freq", colours::uiRed, Knob::mediumStyle),
      clipBoostKnob (parameter ("clipBoost"), "Clip Boost", colours::uiRed, Knob::mediumStyle),
      harmonicsKnob (parameter ("harmonics"), "Harmonics", colours::uiRed, Knob::mediumStyle),
      mixBackKnob (parameter ("mixBack"), "Mix Back", colours::uiRed, Knob::mediumStyle),
      editLink (colours::uiRed),
      meter (meterGeometry),
      characterKnob (parameter ("character"), "Character", colours::uiRed, Knob::smallStyle),
      drawerOutputKnob (parameter ("output"), "Output", colours::uiTeal, Knob::smallStyle),
      oversamplingDropdown (parameter ("oversampling"))
{
    auto& body = getBody();

    panels.setInterceptsMouseClicks (false, false);
    panels.setBounds (body.getLocalBounds());
    body.addAndMakeVisible (panels);

    freqKnob.setBounds (33, 40, 90, 123);
    clipBoostKnob.setBounds (139, 40, 90, 123);
    harmonicsKnob.setBounds (33, 179, 90, 123);
    mixBackKnob.setBounds (139, 179, 90, 123);
    editLink.setBounds (16, 337, 64, 24);
    meter.setBounds (leftPanelWidth, 0, baseWidth - leftPanelWidth, bodyHeight);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &freqKnob, &clipBoostKnob, &harmonicsKnob, &mixBackKnob, &editLink, &meter })
        body.addAndMakeVisible (item);

    oversamplingLabel.setText ("Oversampling", juce::dontSendNotification);
    oversamplingLabel.setFont (font (Weight::regular, 14.0f));
    oversamplingLabel.setColour (juce::Label::textColourId, colours::textPrimary);
    oversamplingLabel.setBorderSize ({});

    drawer.setBounds (0, 0, leftPanelWidth, bodyHeight);
    characterKnob.setBounds (32, 56, 90, 99);
    drawerOutputKnob.setBounds (142, 56, 90, 99);
    oversamplingLabel.setBounds (32, 186, 100, 24);
    oversamplingDropdown.setBounds (140, 186, 92, 24);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &characterKnob, &drawerOutputKnob, &oversamplingLabel, &oversamplingDropdown })
        drawer.addAndMakeVisible (item);

    body.addChildComponent (drawer);
    editLink.onClick = [this] { drawer.setVisible (true); };
}

void ExciterEditor::onTick (float seconds)
{
    auto& processor = getFancyProcessor();
    meter.update (processor.inputMeter, processor.outputMeter, seconds);
}

void ExciterEditor::showPreview (bool drawerOpen)
{
    stopUpdates();

    setValue (parameter ("clipBoost"), 0.2f);
    setValue (parameter ("harmonics"), 42.0f);

    meter.show (-5.2f, -6.0f, -5.4f, -6.1f);
    drawer.setVisible (drawerOpen);
}
