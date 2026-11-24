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

void setUpCaption (juce::Label& label, const juce::String& text, float height)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (font (Weight::regular, height));
    label.setColour (juce::Label::textColourId, colours::textSecondary);
    label.setJustificationType (juce::Justification::topLeft);
    label.setBorderSize ({});
    label.setInterceptsMouseClicks (false, false);
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
      listenCheckbox (processor, "Listen to added harmonics", colours::uiRed),
      characterKnob (parameter ("character"), "Character", colours::uiRed, Knob::smallStyle),
      drawerOutputKnob (parameter ("output"), "Output", colours::uiTeal, Knob::smallStyle),
      oversamplingDropdown (parameter ("oversampling")),
      ceilingKnob (parameter ("ceiling"), "Ceiling", colours::uiTeal, Knob::smallStyle),
      autoGainCheckbox (parameter ("autoGain"), "Auto gain"),
      freq2Knob (parameter ("freq2"), "Freq 2", colours::uiRed, Knob::smallStyle),
      harmonics2Knob (parameter ("harmonics2"), "Harmonics 2", colours::uiRed, Knob::smallStyle),
      punchKnob (parameter ("punch"), "Punch", colours::uiRed, Knob::smallStyle),
      highCutKnob (parameter ("highCut"), "High cut", colours::uiRed, Knob::smallStyle),
      widthKnob (parameter ("width"), "Width", colours::uiRed, Knob::smallStyle)
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

    // Phase 4 Wave 1.
    listenCheckbox.setBounds (24, 309, 224, 20);
    listenCheckbox.setTooltip ("Plays only what Exciter adds. Turns off when you close this window, and is never saved.");
    thdLabel.setBounds (leftPanelWidth, bodyHeight - 17, baseWidth - leftPanelWidth, 14);
    thdLabel.setFont (font (Weight::regular, 11.0f));
    thdLabel.setColour (juce::Label::textColourId, colours::textSecondary);
    thdLabel.setJustificationType (juce::Justification::centred);
    thdLabel.setBorderSize ({});
    thdLabel.setTooltip ("The harmonics these settings add to a tone an octave above Freq.");
    body.addAndMakeVisible (listenCheckbox);
    body.addAndMakeVisible (thdLabel);

    oversamplingLabel.setText ("Oversampling", juce::dontSendNotification);
    oversamplingLabel.setFont (font (Weight::regular, 14.0f));
    oversamplingLabel.setColour (juce::Label::textColourId, colours::textPrimary);
    oversamplingLabel.setBorderSize ({});

    drawer.setBounds (0, 0, leftPanelWidth, bodyHeight);
    characterKnob.setBounds (32, 56, 90, 99);
    drawerOutputKnob.setBounds (142, 56, 90, 99);
    oversamplingLabel.setBounds (32, 186, 100, 24);
    oversamplingDropdown.setBounds (140, 186, 92, 24);

    ceilingKnob.setBounds (32, 222, 90, 99);
    ceilingKnob.setTooltip ("Holds the output's true peak under this level, so Clip Boost can't pass it. Off at the bottom.");
    autoGainCheckbox.setBounds (142, 262, 110, 20);
    autoGainCheckbox.setTooltip ("Turns the output down by as much as the harmonics add, so you hear the colour, not the level.");

    mainItems = { &characterKnob, &drawerOutputKnob, &oversamplingLabel, &oversamplingDropdown, &ceilingKnob, &autoGainCheckbox };

    // Phase 4 Wave 2, items 40 to 43 (mockup 62; drawer coordinates are the mockup's window
    // coordinates less 73). The page switch takes the drawer title's place. Main keeps today's
    // drawer as it is: item 44's Oversampling filter row was dropped (Decision 4 B).
    drawerTabs.setBounds (22, 13, 150, 22);
    drawerTabs.onChange = [this] (int page) { showDrawerPage (page); };
    drawer.addAndMakeVisible (drawerTabs);

    setUpCaption (band2Title, "BAND 2", 10.0f);
    setUpCaption (band2Note, "Adds harmonics from Freq 2 up to Freq: warmth and body under today's band.", 11.0f);
    setUpCaption (shapeTitle, "SHAPE", 10.0f);
    setUpCaption (shapeNote, "Punch lifts the harmonics on attacks; High cut and Width act on the added harmonics only.", 11.0f);

    band2Title.setBounds (24, 53, 200, 12);
    freq2Knob.setBounds (27, 63, 90, 99);
    harmonics2Knob.setBounds (132, 63, 96, 99);
    band2Note.setBounds (24, 169, 230, 32);
    shapeTitle.setBounds (24, 211, 200, 12);
    punchKnob.setBounds (20, 221, 72, 99);
    highCutKnob.setBounds (102, 221, 72, 99);
    widthKnob.setBounds (184, 221, 72, 99);
    shapeNote.setBounds (24, 320, 230, 48); // 7 px under the mockup: fancy-core's knob face is 42 px, not 36

    freq2Knob.setTooltip ("Where Band 2 starts. It runs up to Freq, so above Freq it does nothing.");
    harmonics2Knob.setTooltip ("How much Band 2 adds: harmonics from Freq 2 up to Freq, for warmth and body. 0 turns it off.");
    punchKnob.setTooltip ("Lifts the harmonics on attacks, by up to 6 dB, and lowers them in sustains, by up to 6 dB.");
    highCutKnob.setTooltip ("A low-pass on the added harmonics only, to tame fizz. The dry signal is never filtered. Off at the top.");
    widthKnob.setTooltip ("Below 100 % the added harmonics narrow towards the centre; above it they spread wider. Mono keeps them as they are.");

    harmonicsItems = { &band2Title, &freq2Knob, &harmonics2Knob, &band2Note, &shapeTitle, &punchKnob, &highCutKnob, &widthKnob, &shapeNote };

    for (auto* item : mainItems)
        drawer.addAndMakeVisible (item);

    for (auto* item : harmonicsItems)
        drawer.addChildComponent (item);

    showDrawerPage (mainPage);

    body.addChildComponent (drawer);
    editLink.onClick = [this] { drawer.setVisible (true); };
    addLatencyNote (oversamplingDropdown);
}

void ExciterEditor::onTick (float seconds)
{
    auto& processor = getFancyProcessor();
    meter.update (processor.inputMeter, processor.outputMeter, seconds);

    const auto thd = dynamic_cast<ExciterProcessor&> (processor).getThdPercent();
    thdLabel.setText ("THD " + juce::String (thd, thd < 10.0f ? 1 : 0) + " %", juce::dontSendNotification);
}

void ExciterEditor::showPreview (bool drawerOpen)
{
    stopUpdates();

    setValue (parameter ("clipBoost"), 0.2f);
    setValue (parameter ("harmonics"), 42.0f);

    meter.show (-5.2f, -6.0f, -5.4f, -6.1f);
    drawer.setVisible (drawerOpen);
    showDrawerPage (mainPage);
}

void ExciterEditor::showDrawerPage (int page)
{
    drawerTabs.setSelected (page);

    for (auto* item : mainItems)
        item->setVisible (page == mainPage);

    for (auto* item : harmonicsItems)
        item->setVisible (page == harmonicsPage);
}

void ExciterEditor::showHarmonicsPreview()
{
    showPreview (true);

    setValue (parameter ("freq2"), 400.0f);
    setValue (parameter ("harmonics2"), 30.0f);
    setValue (parameter ("punch"), 50.0f);
    setValue (parameter ("highCut"), 12000.0f);
    setValue (parameter ("width"), 120.0f);

    showDrawerPage (harmonicsPage);
}

juce::Colour ExciterEditor::getAccentColour()
{
    return colours::uiRed;
}

std::vector<ShellEditor::PopupOpener> ExciterEditor::getPopupOpeners()
{
    auto openers = ShellEditor::getPopupOpeners();
    openers.push_back ({ "dropdown-oversampling", [this] { drawer.setVisible (true); showDrawerPage (mainPage); oversamplingDropdown.click(); } });
    openers.push_back ({ "value-freq", [this] { freqKnob.openValueEditor(); } });
    return openers;
}
